/****************************************************************************
 * apps/testing/testsuites/kernel/kv/cases/kv_test_018.c
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
#include <stdio.h>
#include <syslog.h>
#include <stdint.h>
#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>
#include <time.h>
#include "KvTest.h"
#include "kvdb.h"

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: test_nuttx_kv18
 ****************************************************************************/

void test_nuttx_kv18(FAR void **state)
{
  char key[TEST_KEY_LENGTH] =
  {
    0
  };

  int64_t setint64_data1 = INT64_MIN + 1;
  int64_t setint64_data2 = -6727346527346527834;
  int64_t setint64_data3 = 0;
  int64_t setint64_data4 = 8757357537575373757;
  int64_t setint64_data5 = 9223372036854775807;
  int setint64_ret;

  sprintf(key, "test_key_%s_1", __func__);
  setint64_ret = property_set_int64(key, setint64_data1);
  assert_int_equal(setint64_ret, 0);
  property_delete(key);

  sprintf(key, "test_key_%s_2", __func__);
  setint64_ret = property_set_int64(key, setint64_data2);
  assert_int_equal(setint64_ret, 0);
  property_delete(key);

  sprintf(key, "test_key_%s_3", __func__);
  setint64_ret = property_set_int64(key, setint64_data3);
  assert_int_equal(setint64_ret, 0);
  property_delete(key);

  sprintf(key, "test_key_%s_4", __func__);
  setint64_ret = property_set_int64(key, setint64_data4);
  assert_int_equal(setint64_ret, 0);
  property_delete(key);

  sprintf(key, "test_key_%s_5", __func__);
  setint64_ret = property_set_int64(key, setint64_data5);
  assert_int_equal(setint64_ret, 0);
  property_delete(key);
}
