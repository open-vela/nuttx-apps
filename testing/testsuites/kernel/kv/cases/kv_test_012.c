/****************************************************************************
 * apps/testing/testsuites/kernel/kv/cases/kv_test_012.c
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
#include <stdint.h>
#include <stdio.h>
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
 * Name: test_nuttx_kv12
 ****************************************************************************/

void test_nuttx_kv12(FAR void **state)
{
  char key[TEST_KEY_LENGTH] =
  {
    0
  };

  int32_t getint32_data1 = -2102323233;
  int32_t getint32_data2 = 1054545457;
  int32_t getint32_value;
  int getint32_ret;
  sprintf(key, "test_key_%s", __func__);
  getint32_ret = property_set_int32(key, getint32_data1);
  assert_int_equal(getint32_ret, 0);

  getint32_value = property_get_int32(key, 0);
  assert_int_equal(getint32_value, getint32_data1);
  property_delete(key);

  getint32_ret = property_set_int32(key, getint32_data2);
  assert_int_equal(getint32_ret, 0);

  getint32_value = property_get_int32(key, 0);
  assert_int_equal(getint32_value, getint32_data2);
  property_delete(key);
}
