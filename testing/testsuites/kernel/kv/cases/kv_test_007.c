/****************************************************************************
 * apps/testing/testsuites/kernel/kv/cases/kv_test_007.c
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
#include <string.h>
#include <time.h>
#include <stdint.h>
#include <stdio.h>
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
 * Name: test_nuttx_kv07
 ****************************************************************************/

void test_nuttx_kv07(FAR void **state)
{
  char key[TEST_KEY_LENGTH] =
  {
    0
  };

  char old_data_string[TEST_VALUE_LENGTH] =
  {
    0
  };

  char new_data_string[TEST_VALUE_LENGTH] =
  {
    0
  };

  char value[TEST_VALUE_LENGTH] =
  {
    0
  };

  int32_t data_32;
  int32_t value_32;
  bool data_bool = false;
  bool value_bool;
  int ret;

  sprintf(key, "test_key_%s", __func__);
  sprintf(old_data_string, "test_data_%s_new", __func__);
  ret = property_set(key, old_data_string);
  assert_int_equal(ret, 0);

  sprintf(new_data_string, "test_data_%s_old", __func__);
  ret = property_set(key, new_data_string);
  assert_int_equal(ret, 0);

  property_get(key, value, "");
  assert_int_equal(strcmp(value, new_data_string), 0);
  property_delete(key);

  sprintf(key, "test_key_%s", __func__);
  data_32 = rand() % INT32_MAX + 1;
  ret = property_set_int32(key, data_32);
  assert_int_equal(ret, 0);

  value_32 = property_get_int32(key, 0);
  assert_int_equal(value_32, data_32);

#ifdef CONFIG_LIBC_LONG_LONG
  int64_t data_64, value_64;
  data_64 = rand() % INT64_MAX + 1;
  ret = property_set_int64(key, data_64);
  assert_int_equal(ret, 0);

  value_64 = property_get_int64(key, 0);
  assert_int_equal(value_64, data_64);
#endif

  ret = property_set_bool(key, data_bool);
  assert_int_equal(ret, 0);

  value_bool = property_get_bool(key, 0);
  assert_int_equal(value_bool, data_bool);

  property_delete(key);
}
