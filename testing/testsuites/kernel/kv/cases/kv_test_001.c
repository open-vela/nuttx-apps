/****************************************************************************
 * apps/testing/testsuites/kernel/kv/cases/kv_test_001.c
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
#include <string.h>
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
 * Name: test_nuttx_kv01
 ****************************************************************************/

void test_nuttx_kv01(FAR void **state)
{
  int ret;
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

  for (int i = 1; i <= 1000; i++)
    {
      /* test key */

      sprintf(key, "test_key_%s_%d", __func__, i);

      /* test data */

      sprintf(data, "test_data_%s_%d", __func__, i);
      ret = property_set(key, data);
      assert_int_equal(ret, 0);

      ret = property_get(key, value, "");
      property_delete(key);

      assert_int_equal(ret, strlen(data));
      assert_int_equal(strcmp(value, data), 0);
    }
}
