/****************************************************************************
 * apps/testing/testsuites/kernel/kv/cases/kv_test_008.c
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
#include <string.h>
#include <inttypes.h>
#include <stdio.h>
#include <time.h>
#include <syslog.h>
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
 * Name: test_nuttx_kv08
 ****************************************************************************/

void test_nuttx_kv08(FAR void **state)
{
  char key[TEST_KEY_LENGTH] =
  {
    0
  };

  char data[TEST_VALUE_LENGTH] =
  {
    0
  };

  char value[TEST_VALUE_LENGTH] =
  {
    0
  };

  int ret;

  for (int i = 0; i < 100; i++)
    {
      sprintf(key, "test_key_%s_%d", __func__, i);
      sprintf(data, "test_data_%s_new", __func__);
      ret = property_set(key, data);
      assert_int_equal(ret, 0);
      property_get(key, value, "");
      assert_int_equal(strcmp(value, data), 0);
      property_delete(key);
    }
}
