/****************************************************************************
 * apps/testing/testsuites/kernel/kv/cases/kv_test_021.c
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
#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include <syslog.h>
#include <stdint.h>
#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <stdio.h>
#include <cmocka.h>
#include <time.h>
#include <string.h>
#include <unistd.h>
#include "KvTest.h"
#include "kvdb.h"

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: test_nuttx_kv21
 ****************************************************************************/

void test_nuttx_kv21(FAR void **state)
{
  int ret = 0;
  char key[TEST_KEY_LENGTH] =
  {
    0
  };

  char value[TEST_VALUE_LENGTH] =
  {
    0
  };

  char data[TEST_VALUE_LENGTH] =
  {
    0
  };

  strcpy(key, "persist.test.kv.reload");
  strcpy(value, "test");

  /* get key */

  ret = property_get(key, data, "");
  assert_int_equal(ret, strlen(value));
  assert_int_equal(strcmp(value, data), 0);

  /* delete persist key */

  ret = property_delete(key);
  assert_int_equal(ret, 0);

  property_reload();

  /* get key */

  ret = property_get(key, data, "");
  assert_int_equal(ret, strlen(value));
  assert_int_equal(strcmp(value, data), 0);
}
