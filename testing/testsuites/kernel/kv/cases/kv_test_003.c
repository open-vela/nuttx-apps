/****************************************************************************
 * apps/testing/testsuites/kernel/kv/cases/kv_test_003.c
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
#include <stdlib.h>
#include <inttypes.h>
#include <syslog.h>
#include <time.h>
#include <stdio.h>
#include <stdint.h>
#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>
#include "KvTest.h"
#include "kvdb.h"

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: test_nuttx_kv03
 ****************************************************************************/

void test_nuttx_kv03(FAR void **state)
{
  int ret;
  char key[TEST_KEY_LENGTH] =
  {
    0
  };

  int32_t test_int32_1;
  int32_t test_int32_2;
  int64_t test_int64_1;
  int64_t test_int64_2;

  /* test key */

  sprintf(key, "test_key_%s_1", __func__);
  ret = property_set_int32(key, 2147483647);
  assert_int_equal(ret, 0);
  test_int32_1 = property_get_int32(key, 0);
  assert_int_equal(test_int32_1, INT32_MAX);
  property_delete(key);

  /* test key */

  sprintf(key, "test_key_%s_2", __func__);
  ret = property_set_int64(key, 9223372036854775807);
  assert_int_equal(ret, 0);
  test_int64_1 = property_get_int64(key, 0);
  assert_int_equal(test_int64_1, INT64_MAX);
  property_delete(key);

  /* test key */

  sprintf(key, "test_key_%s_3", __func__);
  ret = property_set_int32(key, -2147483648);
  assert_int_equal(ret, 0);
  test_int32_2 = property_get_int32(key, 0);
  assert_int_equal(test_int32_2, INT32_MIN);
  property_delete(key);

  /* test key */

  sprintf(key, "test_key_%s_4", __func__);
  ret = property_set_int64(key, INT64_MIN);
  assert_int_equal(ret, 0);
  test_int64_2 = property_get_int64(key, 0);
  assert_int_equal(test_int64_2, INT64_MIN);
  property_delete(key);
}
