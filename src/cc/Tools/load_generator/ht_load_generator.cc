/**
 * Copyright (C) 2009 Sanjit Jhala (Zvents, Inc.)
 *
 * This file is part of Hypertable.
 *
 * Hypertable is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or any later version.
 *
 * Hypertable is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 */
#include "Common/Compat.h"

#include <iostream>
#include <fstream>
#include <cstdio>
#include <cmath>

extern "C" {
#include <stdio.h>
#include <time.h>
}

#include <boost/algorithm/string.hpp>
#include <boost/progress.hpp>
#include <boost/shared_array.hpp>
#include <boost/timer.hpp>
#include <boost/thread/xtime.hpp>

#include "Common/Stopwatch.h"
#include "Common/String.h"
#include "Common/System.h"
#include "Common/Usage.h"
#include "Common/Config.h"

#include "Hypertable/Lib/Client.h"
#include "Hypertable/Lib/DataGenerator.h"
#include "Hypertable/Lib/Config.h"
#include "Hypertable/Lib/Cells.h"

using namespace Hypertable;
using namespace Hypertable::Config;
using namespace std;
using namespace boost;

namespace {

  const char *usage =
    "\n"
    "Usage: ht_generate_load [options] <type> <generator-config>\n\n"
    "Description:\n"
    "  This program is used to generate load on a Hypertable\n"
    "  cluster.  The <type> argument indicates the type of load\n"
    "  to generate ('query' or 'update') and the <generator-config>\n"
    "  argument is the name of the DataGenerator config file.\n\n"
    "Options";

  struct AppPolicy : Config::Policy {
    static void init_options() {
      allow_unregistered_options(true);
      cmdline_desc(usage).add_options()
        ("help,h", "Show this help message and exit")
        ("help-config", "Show help message for config properties")
        ("table", str()->default_value("LoadTest"), "Name of table to query/update")
        ("max-bytes", i64(), "Amount of data to generate, measured by number "
         "of key and value bytes produced")
        ("sample-file", str(),
         "Output file to hold request latencies, one per line")
        ("seed", i32()->default_value(1), "Pseudo-random number generator seed")
        ("stdout", boo()->zero_tokens()->default_value(false),
         "Display generated data to stdout instead of sending load to cluster")
        ("verbose,v", boo()->zero_tokens()->default_value(false),
         "Show more verbose output")
        ("flush", boo()->zero_tokens()->default_value(false), "Flush after each update")
        ("version", "Show version information and exit")
        ;
      alias("max-bytes", "DataGenerator.MaxBytes");
      alias("seed", "DataGenerator.Seed");
      cmdline_hidden_desc().add_options()
        ("type", str(), "Type.")
        ("generator-config", str(), "DataGenerator config file.");
      cmdline_positional_desc().add("type", 1).add("generator-config", 1);
    }
  };
}


typedef Meta::list<AppPolicy, DataGeneratorPolicy, DefaultCommPolicy> Policies;

void generate_update_load(PropertiesPtr &props, String &tablename, bool flush, bool to_stdout, String &sample_fname);
void generate_query_load(PropertiesPtr &props, String &tablename, bool to_stdout, String &sample_fname);
double std_dev(uint64_t nn, double sum, double sq_sum);
void parse_command_line(int argc, char **argv, PropertiesPtr &props);

int main(int argc, char **argv) {
  String table, load_type, generator_config, sample_fname;
  PropertiesPtr generator_props = new Properties();
  bool flush, to_stdout;

  try {
    init_with_policies<Policies>(argc, argv);

    if (!has("generator-config")) {
      std::cout << cmdline_desc() << std::flush;
      _exit(0);
    }

    load_type = get_str("type");
    generator_config = get_str("generator-config");

    table = get_str("table");

    sample_fname = has("sample-file") ? get_str("sample-file") : "";

    flush = get_bool("flush");
    to_stdout = get_bool("stdout");

    // Only try to parse config file if it exists or not default
    if (FileUtils::exists(generator_config))
      generator_props->load(generator_config, cmdline_hidden_desc(), true);
    else
      HT_THROW(Error::FILE_NOT_FOUND, generator_config);

    parse_command_line(argc, argv, generator_props);

    if (load_type == "update")
      generate_update_load(generator_props, table, flush, to_stdout, sample_fname);
    else if (load_type == "query")
      generate_query_load(generator_props, table, to_stdout, sample_fname);
    else {
      std::cout << cmdline_desc() << std::flush;
      _exit(1);
    }
  }
  catch (Exception &e) {
    HT_ERROR_OUT << e << HT_END;
    exit(1);
  }

  fflush(stdout);
  _exit(0); // don't bother with static objects
}


void parse_command_line(int argc, char **argv, PropertiesPtr &props) {
  const char *ptr;
  String key, value;
  props->parse_args(argc, argv, cmdline_desc(), 0, 0, true);
  for (int i=1; i<argc; i++) {
    if (argv[i][0] == '-') {
      ptr = strchr(argv[i], '=');
      if (ptr) {
        key = String(argv[i], ptr-argv[i]);
        trim_if(key, is_any_of("-"));
        if (!props->has(key)) {
          value = String(ptr+1);
          trim_if(value, is_any_of("'\""));
          props->set(key, boost::any_cast<String>(value));
        }
      }
      else {
        key = String(argv[i]);
        trim_if(key, is_any_of("-"));
        if (!props->has(key))
          props->set(key, boost::any_cast<String>(String("true")));
      }
    }
  }
}


void generate_update_load(PropertiesPtr &props, String &tablename, bool flush,
                          bool to_stdout, String &sample_fname)
{
  double cum_latency=0, cum_sq_latency=0, latency=0;
  double min_latency=10000000, max_latency=0;
  uint64_t total_cells=0;
  Cells cells;
  clock_t start_clocks=0, stop_clocks=0;
  double clocks_per_usec = (double)CLOCKS_PER_SEC / 1000000.0;
  bool output_samples = false;
  ofstream sample_file;
  DataGenerator dg(props);

  if (to_stdout) {
    cout << "rowkey\tcolumnkey\tvalue\n";
    for (DataGenerator::iterator iter = dg.begin(); iter != dg.end(); iter++) {
      if (*(*iter).column_qualifier == 0)
        cout << (*iter).row_key << "\t" << (*iter).column_family 
             << "\t" << (const char *)(*iter).value << "\n";
      else
        cout << (*iter).row_key << "\t" << (*iter).column_family << ":" 
             << (*iter).column_qualifier << "\t" << (const char *)(*iter).value << "\n";
    }
    cout << flush;
    return;
  }

  if (sample_fname != "") {
    sample_file.open(sample_fname.c_str());
    output_samples = true;
  }

  Stopwatch stopwatch;

  try {
    ClientPtr hypertable_client_ptr;
    TablePtr table_ptr;
    TableMutatorPtr mutator_ptr;
    String config_file = get_str("config");
    boost::progress_display progress_meter(dg.get_limit());    

    if (config_file != "")
      hypertable_client_ptr = new Hypertable::Client(config_file);
    else
      hypertable_client_ptr = new Hypertable::Client();

    table_ptr = hypertable_client_ptr->open_table(tablename);
    mutator_ptr = table_ptr->create_mutator();

    for (DataGenerator::iterator iter = dg.begin(); iter != dg.end(); iter++) {

      // do update
      cells.clear();
      cells.push_back(*iter);
      if (flush)
        start_clocks = clock();

      mutator_ptr->set_cells(cells);

      if (flush) {
        mutator_ptr->flush();
        stop_clocks = clock();
        if (stop_clocks < start_clocks)
          latency = ((std::numeric_limits<clock_t>::max() - start_clocks) + stop_clocks) / clocks_per_usec;
        else
          latency = (stop_clocks-start_clocks) / clocks_per_usec;
        if (output_samples)
          sample_file << (unsigned long)latency << "\n";
        else {
          cum_latency += latency;
          cum_sq_latency += pow(latency,2);
          if (latency < min_latency)
            min_latency = latency;
          if (latency > max_latency)
            max_latency = latency;
        }
      }

      ++total_cells;
      progress_meter += iter.last_data_size();
    }
  }
  catch (Exception &e) {
    HT_ERROR_OUT << e << HT_END;
    exit(1);
  }

  stopwatch.stop();
    
  printf("\n");
  printf("\n");
  printf("        Elapsed time: %.2f s\n", stopwatch.elapsed());
  printf("Total cells inserted: %llu\n", (Llu) total_cells);
  printf("Throughput (cells/s): %.2f\n", (double)total_cells/stopwatch.elapsed());
  printf("Total bytes inserted: %llu\n", (Llu)dg.get_limit());
  printf("Throughput (bytes/s): %.2f\n", (double)dg.get_limit()/stopwatch.elapsed());

  if (flush && !output_samples) {
    printf("  Latency min (usec): %llu\n", (Llu)min_latency);
    printf("  Latency max (usec): %llu\n", (Llu)max_latency);
    printf("  Latency avg (usec): %llu\n", (Llu)((double)cum_latency/total_cells));
    printf("Latency stddev (usec): %llu\n", (Llu)std_dev(total_cells, cum_latency, cum_sq_latency));
  }
  printf("\n");

  if (output_samples)
    sample_file.close();

}


void generate_query_load(PropertiesPtr &props, String &tablename, bool to_stdout, String &sample_fname)
{
  double cum_latency=0, cum_sq_latency=0, latency=0;
  double min_latency=10000000, max_latency=0;
  uint64_t total_cells=0;
  Cells cells;
  clock_t start_clocks, stop_clocks;
  double clocks_per_usec = (double)CLOCKS_PER_SEC / 1000000.0;
  bool output_samples = false;
  ofstream sample_file;
  DataGenerator dg(props, true);

  if (to_stdout) {
    for (DataGenerator::iterator iter = dg.begin(); iter != dg.end(); iter++) {
      if (*(*iter).column_qualifier == 0)
        cout << (*iter).row_key << "\t" << (*iter).column_family << "\n";
      else
        cout << (*iter).row_key << "\t" << (*iter).column_family << ":" 
             << (*iter).column_qualifier << "\n";
    }
    cout << flush;
    return;
  }

  if (sample_fname != "") {
    sample_file.open(sample_fname.c_str());
    output_samples = true;
  }

  Stopwatch stopwatch;

  try {
    ClientPtr hypertable_client_ptr;
    TablePtr table_ptr;
    ScanSpecBuilder scan_spec;
    Cell cell;
    String config_file = get_str("config");
    boost::progress_display progress_meter(dg.get_limit());    

    if (config_file != "")
      hypertable_client_ptr = new Hypertable::Client(config_file);
    else
      hypertable_client_ptr = new Hypertable::Client();

    table_ptr = hypertable_client_ptr->open_table(tablename);

    for (DataGenerator::iterator iter = dg.begin(); iter != dg.end(); iter++) {

      scan_spec.clear();
      scan_spec.add_column((*iter).column_family);
      scan_spec.add_row((*iter).row_key);

      start_clocks = clock();

      TableScanner *scanner_ptr=table_ptr->create_scanner(scan_spec.get());

      while (scanner_ptr->next(cell))
        ;

      delete scanner_ptr;

      stop_clocks = clock();
      if (stop_clocks < start_clocks)
        latency = ((std::numeric_limits<clock_t>::max() - start_clocks) + stop_clocks) / clocks_per_usec;
      else
        latency = (stop_clocks-start_clocks) / clocks_per_usec;
      if (output_samples)
        sample_file << (unsigned long)latency << "\n";
      else {
        cum_latency += latency;
        cum_sq_latency += pow(latency,2);
        if (latency < min_latency)
          min_latency = latency;
        if (latency > max_latency)
          max_latency = latency;
      }

      ++total_cells;
      progress_meter += iter.last_data_size();
    }
  }
  catch (Exception &e) {
    HT_ERROR_OUT << e << HT_END;
    exit(1);
  }

  stopwatch.stop();
    
  printf("\n");
  printf("\n");
  printf("        Elapsed time: %.2f s\n", stopwatch.elapsed());
  printf("Total cells inserted: %llu\n", (Llu) total_cells);
  printf("Throughput (cells/s): %.2f\n", (double)total_cells/stopwatch.elapsed());
  printf("Total bytes inserted: %llu\n", (Llu)dg.get_limit());
  printf("Throughput (bytes/s): %.2f\n", (double)dg.get_limit()/stopwatch.elapsed());

  if (!output_samples) {
    printf("  Latency min (usec): %llu\n", (Llu)min_latency);
    printf("  Latency max (usec): %llu\n", (Llu)max_latency);
    printf("  Latency avg (usec): %llu\n", (Llu)((double)cum_latency/total_cells));
    printf("Latency stddev (usec): %llu\n", (Llu)std_dev(total_cells, cum_latency, cum_sq_latency));
  }
  printf("\n");

  if (output_samples)
    sample_file.close();
}



/**
 * @param nn Size of set of numbers
 * @param sum Sum of numbers in set
 * @param sq_sum Sum of squares of numbers in set
 * @return std deviation of set
 */
double std_dev(uint64_t nn, double sum, double sq_sum)
{
  double mean = sum/nn;
  double sq_std = sqrt((sq_sum/(double)nn) - pow(mean,2));
  return sq_std;
}
