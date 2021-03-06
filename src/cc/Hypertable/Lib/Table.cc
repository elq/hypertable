/** -*- c++ -*-
 * Copyright (C) 2008 Doug Judd (Zvents, Inc.)
 *
 * This file is part of Hypertable.
 *
 * Hypertable is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; version 2 of the
 * License, or any later version.
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
#include <cstring>

#include "Common/String.h"
#include "Common/DynamicBuffer.h"
#include "Common/Error.h"
#include "Common/Logger.h"

#include "Hyperspace/HandleCallback.h"
#include "Hyperspace/Session.h"

#include "Table.h"
#include "TableScanner.h"
#include "TableMutator.h"

using namespace Hypertable;
using namespace Hyperspace;


Table::Table(PropertiesPtr &props, ConnectionManagerPtr &conn_manager,
             Hyperspace::SessionPtr &hyperspace, const String &name)
  : m_props(props), m_comm(conn_manager->get_comm()),
    m_conn_manager(conn_manager), m_hyperspace(hyperspace), m_stale(true) {

  HT_TRY("getting table timeout",
    m_timeout_ms = props->get_i32("Hypertable.Request.Timeout"));

  initialize(name.c_str());

  m_range_locator = new RangeLocator(props, m_conn_manager, m_hyperspace,
                                     m_timeout_ms);
}


Table::Table(RangeLocatorPtr &range_locator, ConnectionManagerPtr &conn_manager,
    Hyperspace::SessionPtr &hyperspace, const String &name, uint32_t timeout_ms)
  : m_comm(conn_manager->get_comm()), m_conn_manager(conn_manager),
    m_hyperspace(hyperspace), m_range_locator(range_locator),
    m_timeout_ms(timeout_ms), m_stale(true) {

  initialize(name.c_str());
}


void Table::initialize(const char *name) {
  String tablefile = "/hypertable/tables/"; tablefile += name;
  DynamicBuffer value_buf(0);
  uint64_t handle;
  HandleCallbackPtr null_handle_callback;
  String errmsg;

  // TODO: issue 11
  /**
   * Open table file
   */
  try {
    handle = m_hyperspace->open(tablefile, OPEN_FLAG_READ,
                                null_handle_callback);
  }
  catch (Exception &e) {
    if (e.code() == Error::HYPERSPACE_BAD_PATHNAME)
      HT_THROW2(Error::TABLE_NOT_FOUND, e, "");
    HT_THROW2F(e.code(), e, "Unable to open Hyperspace table file '%s'",
               tablefile.c_str());
  }

  m_table.set_name(name);

  /**
   * Get table_id attribute
   */
  value_buf.clear();
  m_hyperspace->attr_get(handle, "table_id", value_buf);

  m_table.id = atoi((const char *)value_buf.base);

  /**
   * Get schema attribute
   */
  value_buf.clear();
  m_hyperspace->attr_get(handle, "schema", value_buf);

  m_hyperspace->close(handle);

  m_schema = Schema::new_instance((const char *)value_buf.base,
      strlen((const char *)value_buf.base), true);

  if (!m_schema->is_valid()) {
    HT_ERRORF("Schema Parse Error: %s", m_schema->get_error_string());
    HT_THROW_(Error::BAD_SCHEMA);
  }

  m_table.generation = m_schema->get_generation();
  m_stale = false;
}


void Table::refresh() {
  ScopedLock lock(m_mutex);
  HT_ASSERT(m_table.name);
  m_stale = true;
  initialize(m_table.name);
}


void Table::get(TableIdentifierManaged &ident_copy, SchemaPtr &schema_copy) {
  ScopedLock lock(m_mutex);
  ident_copy = m_table;
  schema_copy = m_schema;
}


void
Table::refresh(TableIdentifierManaged &ident_copy, SchemaPtr &schema_copy) {
  ScopedLock lock(m_mutex);
  HT_ASSERT(m_table.name);
  m_stale = true;
  initialize(m_table.name);
  ident_copy = m_table;
  schema_copy = m_schema;
}


Table::~Table() {
}


TableMutator *
Table::create_mutator(uint32_t timeout_ms) {
  return new TableMutator(m_comm, this, m_range_locator,
                          timeout_ms ? timeout_ms : m_timeout_ms);
}


TableScanner *
Table::create_scanner(const ScanSpec &scan_spec, uint32_t timeout_ms,
                      bool retry_table_not_found) {
  return new TableScanner(m_comm, this, m_range_locator, scan_spec,
                          timeout_ms ? timeout_ms : m_timeout_ms,
                          retry_table_not_found);
}
