#
# Copyright (C) 2008 Doug Judd (Zvents, Inc.)
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License
# as published by the Free Software Foundation; either version 2
# of the License, or any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
# 02110-1301, USA.
#

set(SRC_DIR "${HYPERTABLE_SOURCE_DIR}/src/cc/Tools/rsclient")
set(DST_DIR "${HYPERTABLE_BINARY_DIR}/src/cc/Tools/rsclient")

set(TEST_DEPENDENCIES ${DST_DIR}/Test1-data.txt ${DST_DIR}/Test2-data.txt
    ${DST_DIR}/Test4-data.txt)

set(rsclient_SRCS
rsclient.cc
RangeServerCommandInterpreter.cc
TableInfo.cc
)

# rsclient - Range Server client command interpreter
add_executable(rsclient ${rsclient_SRCS})
target_link_libraries(rsclient Hypertable)

# rsTest
add_executable(rsTest tests/rsTest.cc ${TEST_DEPENDENCIES})
target_link_libraries(rsTest HyperCommon)

install(TARGETS rsclient RUNTIME DESTINATION ${VERSION}/bin)

add_custom_command(SOURCE ${SRC_DIR}/tests/Test1-data.txt.gz
                   COMMAND cp ARGS ${SRC_DIR}/tests/Test1-data.txt.gz ${DST_DIR}
                   COMMAND gunzip ARGS -f ${DST_DIR}/Test1-data.txt.gz
                   OUTPUT ${DST_DIR}/Test1-data.txt)
add_custom_command(SOURCE ${SRC_DIR}/tests/Test2-data.txt.gz
                   COMMAND cp ARGS ${SRC_DIR}/tests/Test2-data.txt.gz ${DST_DIR}
                   COMMAND gunzip ARGS -f ${DST_DIR}/Test2-data.txt.gz
                   OUTPUT ${DST_DIR}/Test2-data.txt)
add_custom_command(SOURCE ${SRC_DIR}/tests/Test4-data.txt.gz
                   COMMAND cp ARGS ${SRC_DIR}/tests/Test4-data.txt.gz ${DST_DIR}
                   COMMAND gunzip ARGS -f ${DST_DIR}/Test4-data.txt.gz
                   OUTPUT ${DST_DIR}/Test4-data.txt)

configure_file(${HYPERTABLE_SOURCE_DIR}/conf/hypertable.cfg
               ${DST_DIR}/hypertable.cfg)
configure_file(${SRC_DIR}/tests/Test1.golden ${DST_DIR}/Test1.golden)
configure_file(${SRC_DIR}/tests/Test2.golden ${DST_DIR}/Test2.golden)
configure_file(${SRC_DIR}/tests/Test3.golden ${DST_DIR}/Test3.golden)
configure_file(${SRC_DIR}/tests/Test4.golden ${DST_DIR}/Test4.golden)
configure_file(${SRC_DIR}/tests/Test1.cmd ${DST_DIR}/Test1.cmd)
configure_file(${SRC_DIR}/tests/Test2.cmd ${DST_DIR}/Test2.cmd)
configure_file(${SRC_DIR}/tests/Test3.cmd ${DST_DIR}/Test3.cmd)
configure_file(${SRC_DIR}/tests/Test4.cmd ${DST_DIR}/Test4.cmd)

configure_file(${SRC_DIR}/tests/initialize.hql ${DST_DIR}/initialize.hql)

add_test(RangeServer rsTest)
