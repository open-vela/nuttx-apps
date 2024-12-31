# ##############################################################################
# apps/testing/testsuites/CMakeLists.txt
#
# Licensed to the Apache Software Foundation (ASF) under one or more contributor
# license agreements.  See the NOTICE file distributed with this work for
# additional information regarding copyright ownership.  The ASF licenses this
# file to you under the Apache License, Version 2.0 (the "License"); you may not
# use this file except in compliance with the License.  You may obtain a copy of
# the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
# WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.  See the
# License for the specific language governing permissions and limitations under
# the License.
#
# ##############################################################################

if(CONFIG_CM_FS_TEST)
  set(INCDIR
      ${CMAKE_CURRENT_SOURCE_DIR}/kernel/fs/include ${NUTTX_DIR}/../apps/nshlib
      ${NUTTX_DIR}/../apps/testing/cmocka/include)
  file(GLOB FS_CSRCS ${CMAKE_CURRENT_SOURCE_DIR}/kernel/fs/cases/*.c
       ${CMAKE_CURRENT_SOURCE_DIR}/kernel/fs/common/*.c)
  nuttx_add_application(
    NAME
    cmocka_fs_test
    STACKSIZE
    ${CONFIG_TESTING_TESTCASES_STACKSIZE}
    PRIORITY
    ${CONFIG_TESTING_TESTCASES_PRIORITY}
    SRCS
    kernel/fs/cmocka_fs_test.c
    ${FS_CSRCS}
    INCLUDE_DIRECTORIES
    ${INCDIR}
    DEPENDS
    cmocka)
endif()

if(CONFIG_CM_SCHED_TEST)
  set(INCDIR ${CMAKE_CURRENT_SOURCE_DIR}/kernel/sched/include
             ${NUTTX_DIR}/../apps/testing/cmocka/include)
  file(GLOB SCHED_CSRCS ${CMAKE_CURRENT_SOURCE_DIR}/kernel/sched/cases/*.c
       ${CMAKE_CURRENT_SOURCE_DIR}/kernel/sched/common/*.c)
  nuttx_add_application(
    NAME
    cmocka_sched_test
    STACKSIZE
    ${CONFIG_TESTING_TESTCASES_STACKSIZE}
    PRIORITY
    ${CONFIG_TESTING_TESTCASES_PRIORITY}
    SRCS
    kernel/sched/cmocka_sched_test.c
    ${SCHED_CSRCS}
    INCLUDE_DIRECTORIES
    ${INCDIR}
    DEPENDS
    cmocka)
endif()

if(CONFIG_CM_SYSCALL_TEST)
  set(INCDIR
      ${CMAKE_CURRENT_SOURCE_DIR}/kernel/syscall/include
      ${NUTTX_DIR}/../apps/nshlib ${NUTTX_DIR}/../apps/testing/cmocka/include)
  file(GLOB SYSCALL_CSRCS ${CMAKE_CURRENT_SOURCE_DIR}/kernel/syscall/cases/*.c
       ${CMAKE_CURRENT_SOURCE_DIR}/kernel/syscall/common/*.c)
  nuttx_add_application(
    NAME
    cmocka_syscall_test
    STACKSIZE
    ${CONFIG_TESTING_TESTCASES_STACKSIZE}
    PRIORITY
    ${CONFIG_TESTING_TESTCASES_PRIORITY}
    SRCS
    kernel/syscall/cmocka_syscall_test.c
    ${SYSCALL_CSRCS}
    INCLUDE_DIRECTORIES
    ${INCDIR}
    DEPENDS
    cmocka)
endif()

if(CONFIG_CM_MM_TEST)
  set(INCDIR ${CMAKE_CURRENT_SOURCE_DIR}/kernel/mm/include
             ${NUTTX_DIR}/../apps/testing/cmocka/include)
  file(GLOB MM_CSRCS ${CMAKE_CURRENT_SOURCE_DIR}/kernel/mm/cases/*.c
       ${CMAKE_CURRENT_SOURCE_DIR}/kernel/mm/common/*.c)

  nuttx_add_application(
    NAME
    cmocka_mm_test
    STACKSIZE
    ${CONFIG_TESTING_TESTCASES_STACKSIZE}
    PRIORITY
    ${CONFIG_TESTING_TESTCASES_PRIORITY}
    SRCS
    kernel/mm/cmocka_mm_test.c
    ${MM_CSRCS}
    INCLUDE_DIRECTORIES
    ${INCDIR}
    DEPENDS
    cmocka)
  nuttx_add_application(
    NAME
    mem_batch_opt_perf_test
    STACKSIZE
    ${CONFIG_TESTING_TESTCASES_STACKSIZE}
    PRIORITY
    ${CONFIG_TESTING_TESTCASES_PRIORITY}
    SRCS
    kernel/mm/tool/mem_batch_opt_perf_test.c
    INCLUDE_DIRECTORIES
    ${INCDIR}
    DEPENDS
    cmocka)
  nuttx_add_application(
    NAME
    mem_cycle_opt_perf_test
    STACKSIZE
    ${CONFIG_TESTING_TESTCASES_STACKSIZE}
    PRIORITY
    ${CONFIG_TESTING_TESTCASES_PRIORITY}
    SRCS
    kernel/mm/tool/mem_cycle_opt_perf_test.c
    INCLUDE_DIRECTORIES
    ${INCDIR}
    DEPENDS
    cmocka)
endif()

if(CONFIG_CM_KVDB_TEST)
  set(INCDIR
      ${CMAKE_CURRENT_SOURCE_DIR}/kernel/kv/include
      ${NUTTX_DIR}/../frameworks/utils/include
      ${NUTTX_DIR}/../apps/testing/cmocka/include)
  file(GLOB KVDB_CSRCS ${CMAKE_CURRENT_SOURCE_DIR}/kernel/kv/cases/*.c
       ${CMAKE_CURRENT_SOURCE_DIR}/kernel/kv/common/*.c)
  nuttx_add_application(
    NAME
    cmocka_kv_test
    STACKSIZE
    ${CONFIG_TESTING_TESTCASES_STACKSIZE}
    PRIORITY
    ${CONFIG_TESTING_TESTCASES_PRIORITY}
    SRCS
    kernel/kv/cmocka_kv_test.c
    ${KVDB_CSRCS}
    INCLUDE_DIRECTORIES
    ${INCDIR}
    DEPENDS
    cmocka)
endif()

if(CONFIG_CM_TIME_TEST)
  set(INCDIR ${CMAKE_CURRENT_SOURCE_DIR}/kernel/time/include
             ${NUTTX_DIR}/../apps/testing/cmocka/include ${NUTTX_DIR}/include)
  file(GLOB TIME_CSRCS ${CMAKE_CURRENT_SOURCE_DIR}/kernel/time/cases/*.c)
  nuttx_add_application(
    NAME
    cmocka_time_test
    STACKSIZE
    ${CONFIG_TESTING_TESTCASES_STACKSIZE}
    PRIORITY
    ${CONFIG_TESTING_TESTCASES_PRIORITY}
    SRCS
    kernel/time/cmocka_time_test.c
    ${TIME_CSRCS}
    INCLUDE_DIRECTORIES
    ${INCDIR}
    DEPENDS
    cmocka)
endif()

if(CONFIG_CM_SOCKET_TEST)
  set(INCDIR ${CMAKE_CURRENT_SOURCE_DIR}/kernel/socket/include
             ${NUTTX_DIR}/../apps/testing/cmocka/include)
  file(GLOB SOCKET_CSRCS ${CMAKE_CURRENT_SOURCE_DIR}/kernel/socket/cases/*.c)
  nuttx_add_application(
    NAME
    cmocka_socket_test
    STACKSIZE
    ${CONFIG_TESTING_TESTCASES_STACKSIZE}
    PRIORITY
    ${CONFIG_TESTING_TESTCASES_PRIORITY}
    SRCS
    kernel/socket/cmocka_socket_test.c
    ${SOCKET_CSRCS}
    INCLUDE_DIRECTORIES
    ${INCDIR}
    DEPENDS
    cmocka)
endif()

if(CONFIG_CM_PTHREAD_TEST)
  set(INCDIR ${CMAKE_CURRENT_SOURCE_DIR}/kernel/pthread/include
             ${NUTTX_DIR}/../apps/testing/cmocka/include)
  file(GLOB PTHREAD_CSRCS ${CMAKE_CURRENT_SOURCE_DIR}/kernel/pthread/cases/*.c
       ${CMAKE_CURRENT_SOURCE_DIR}/kernel/pthread/common/*.c)
  nuttx_add_application(
    NAME
    cmocka_pthread_test
    STACKSIZE
    ${CONFIG_TESTING_TESTCASES_STACKSIZE}
    PRIORITY
    ${CONFIG_TESTING_TESTCASES_PRIORITY}
    SRCS
    kernel/pthread/cmocka_pthread_test.c
    ${PTHREAD_CSRCS}
    INCLUDE_DIRECTORIES
    ${INCDIR}
    DEPENDS
    cmocka)
endif()

if(CONFIG_CM_MUTEX_TEST)
  set(INCDIR ${CMAKE_CURRENT_SOURCE_DIR}/kernel/mutex/include
             ${NUTTX_DIR}/../apps/testing/cmocka/include)
  file(GLOB MUTEX_CSRCS ${CMAKE_CURRENT_SOURCE_DIR}/kernel/mutex/cases/*.c
       ${CMAKE_CURRENT_SOURCE_DIR}/kernel/mutex/common/*.c)
  nuttx_add_application(
    NAME
    cmocka_mutex_test
    STACKSIZE
    ${CONFIG_TESTING_TESTCASES_STACKSIZE}
    PRIORITY
    ${CONFIG_TESTING_TESTCASES_PRIORITY}
    SRCS
    kernel/mutex/cmocka_mutex_test.c
    ${MUTEX_CSRCS}
    INCLUDE_DIRECTORIES
    ${INCDIR}
    DEPENDS
    cmocka)
endif()

if(CONFIG_TEST_TELEPHONY)
  set(INCDIR
      ${NUTTX_DIR}/../apps/frameworks/telephony/include
      ${NUTTX_DIR}/../apps/testing/cmocka/cmocka/include
      ${NUTTX_DIR}/../apps/testing/cmocka/include)
  file(
    GLOB
    TELEPHONY_CSRCS
    ${CMAKE_CURRENT_SOURCE_DIR}/kernel/telephony/telephony_sim_test.c
    ${CMAKE_CURRENT_SOURCE_DIR}/kernel/telephony/telephony_call_test.c
    ${CMAKE_CURRENT_SOURCE_DIR}/kernel/telephony/telephony_data_test.c
    ${CMAKE_CURRENT_SOURCE_DIR}/kernel/telephony/telephony_sms_test.c
    ${CMAKE_CURRENT_SOURCE_DIR}/kernel/telephony/telephony_network_test.c
    ${CMAKE_CURRENT_SOURCE_DIR}/kernel/telephony/telephony_common_test.c
    ${CMAKE_CURRENT_SOURCE_DIR}/kernel/telephony/telephony_ims_test.c)
  nuttx_add_application(
    NAME
    cmocka_telephony_test
    STACKSIZE
    ${CONFIG_TESTING_TESTCASES_STACKSIZE}
    PRIORITY
    ${CONFIG_TESTING_TESTCASES_PRIORITY}
    SRCS
    kernel/telephony/cmocka_telephony_test.c
    ${TELEPHONY_CSRCS}
    INCLUDE_DIRECTORIES
    ${INCDIR}
    DEPENDS
    cmocka)
endif()
