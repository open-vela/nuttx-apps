/****************************************************************************
 * apps/testing/testsuites/kernel/kv/cmocka_kv_test.c
 *
 * Licensed to the Apache Software Foundation (ASF) under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.
 *The ASF licenses this file to you under the Apache License, Version 2.0
 *(the "License"); you may not use this file except in compliance with
 *the License.  You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 *WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or
 *implied.  See the License for the specific language governing
 *permissions and limitations under the License.
 *
 ****************************************************************************/

/****************************************************************************
 * Included Files
 ****************************************************************************/
#include <nuttx/config.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <stdint.h>
#include <stdio.h>
#include "KvTest.h"
#include <cmocka.h>

/****************************************************************************
 * Private Functions
 ****************************************************************************/

/****************************************************************************
 * Name: cmocka_sched_test_main
 ****************************************************************************/

int main(int argc, char *argv[])
{
  /* Add Test Cases */

  const struct CMUnitTest nuttx_kvdb_test_suites[] =
  {
#ifdef CONFIG_KVDB_TEMPORARY_STORAGE
      cmocka_unit_test_setup_teardown(test_nuttx_kv01, test_nuttx_kv_setup,
                                      test_nuttx_kv_teardown),
      cmocka_unit_test_setup_teardown(test_nuttx_kv02, test_nuttx_kv_setup,
                                      test_nuttx_kv_teardown),
      cmocka_unit_test_setup_teardown(test_nuttx_kv03, test_nuttx_kv_setup,
                                      test_nuttx_kv_teardown),
      cmocka_unit_test_setup_teardown(test_nuttx_kv04, test_nuttx_kv_setup,
                                      test_nuttx_kv_teardown),
      cmocka_unit_test_setup_teardown(test_nuttx_kv05, test_nuttx_kv_setup,
                                      test_nuttx_kv_teardown),
      cmocka_unit_test_setup_teardown(test_nuttx_kv06, test_nuttx_kv_setup,
                                      test_nuttx_kv_teardown),
      cmocka_unit_test_setup_teardown(test_nuttx_kv07, test_nuttx_kv_setup,
                                      test_nuttx_kv_teardown),
      cmocka_unit_test_setup_teardown(test_nuttx_kv08, test_nuttx_kv_setup,
                                      test_nuttx_kv_teardown),
      cmocka_unit_test_setup_teardown(test_nuttx_kv09, test_nuttx_kv_setup,
                                      test_nuttx_kv_teardown),
      cmocka_unit_test_setup_teardown(test_nuttx_kv10, test_nuttx_kv_setup,
                                      test_nuttx_kv_teardown),
      cmocka_unit_test_setup_teardown(test_nuttx_kv11, test_nuttx_kv_setup,
                                      test_nuttx_kv_teardown),
      cmocka_unit_test_setup_teardown(test_nuttx_kv12, test_nuttx_kv_setup,
                                      test_nuttx_kv_teardown),
      cmocka_unit_test_setup_teardown(test_nuttx_kv13, test_nuttx_kv_setup,
                                      test_nuttx_kv_teardown),
#endif
      cmocka_unit_test_setup_teardown(test_nuttx_kv14, test_nuttx_kv_setup,
                                      test_nuttx_kv_teardown),
#ifdef CONFIG_KVDB_TEMPORARY_STORAGE
      cmocka_unit_test_setup_teardown(test_nuttx_kv15, test_nuttx_kv_setup,
                                      test_nuttx_kv_teardown),
      cmocka_unit_test_setup_teardown(test_nuttx_kv16, test_nuttx_kv_setup,
                                      test_nuttx_kv_teardown),
      cmocka_unit_test_setup_teardown(test_nuttx_kv17, test_nuttx_kv_setup,
                                      test_nuttx_kv_teardown),
      cmocka_unit_test_setup_teardown(test_nuttx_kv18, test_nuttx_kv_setup,
                                      test_nuttx_kv_teardown),
      cmocka_unit_test_setup_teardown(test_nuttx_kv19, test_nuttx_kv_setup,
                                      test_nuttx_kv_teardown),
      cmocka_unit_test_setup_teardown(test_nuttx_kv20, test_nuttx_kv_setup,
                                      test_nuttx_kv_teardown),
#endif
#ifdef CONFIG_KVDB_SERVER
      cmocka_unit_test_setup_teardown(test_nuttx_kv21, test_nuttx_kv_setup,
                                      test_nuttx_kv_teardown),
#endif
#ifdef CONFIG_KVDB_TEMPORARY_STORAGE
      cmocka_unit_test_setup_teardown(test_nuttx_kv22, test_nuttx_kv_setup,
                                      test_nuttx_kv_teardown),
      cmocka_unit_test_setup_teardown(test_nuttx_kv23, test_nuttx_kv_setup,
                                      test_nuttx_kv_teardown),
#endif
      cmocka_unit_test_setup_teardown(test_nuttx_kv24, test_nuttx_kv_setup,
                                      test_nuttx_kv_teardown),
      cmocka_unit_test_setup_teardown(test_nuttx_kv25, test_nuttx_kv_setup,
                                      test_nuttx_kv_teardown),
      cmocka_unit_test_setup_teardown(test_nuttx_kv26, test_nuttx_kv_setup,
                                      test_nuttx_kv_teardown),
      cmocka_unit_test_setup_teardown(test_nuttx_kv27, test_nuttx_kv_setup,
                                      test_nuttx_kv_teardown),
      cmocka_unit_test_setup_teardown(test_nuttx_kv28, test_nuttx_kv_setup,
                                      test_nuttx_kv_teardown),
      cmocka_unit_test_setup_teardown(test_nuttx_kv29, test_nuttx_kv_setup,
                                      test_nuttx_kv_teardown),
      cmocka_unit_test_setup_teardown(test_nuttx_kv30, test_nuttx_kv_setup,
                                      test_nuttx_kv_teardown),
  };

  /* Run Test cases */

  cmocka_run_group_tests(nuttx_kvdb_test_suites, NULL, NULL);
  return 0;
}
