/****************************************************************************
 * apps/testing/testsuites/kernel/kv/cases/kv_test_027.c
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
 * Name: test_nuttx_kv27
 ****************************************************************************/

void test_nuttx_kv27(FAR void **state)
{
  int ret;
  char key[TEST_KEY_LENGTH] =
  {
    0
  };

  int32_t test_int32_1;
  int32_t test_int32_2;

  /* test key */

  sprintf(key, "persist.kv27_int32_1");
  ret = property_set_int32(key, 2147483647);
  assert_int_equal(ret, 0);
  ret = property_commit();
  if (ret != 0)
    {
      syslog(LOG_ERR, "Failed to commit after inserting data. Key:%s",
             key);
      assert_int_equal(ret, 0);
    }

  test_int32_1 = property_get_int32(key, 0);
  assert_int_equal(test_int32_1, INT32_MAX);

  ret = property_delete(key);
  assert_int_equal(ret, 0);
  ret = property_commit();
  if (ret != 0)
    {
      syslog(LOG_ERR, "Failed to commit after deleting data. Key:%s",
              key);
      assert_int_equal(ret, 0);
    }

  /* test key */

#ifdef CONFIG_LIBC_LONG_LONG
  int64_t test_int64_1;
  sprintf(key, "persist.kv27_int64_1");
  ret = property_set_int64(key, 9223372036854775807);
  assert_int_equal(ret, 0);
  ret = property_commit();
  if (ret != 0)
    {
      syslog(LOG_ERR, "Failed to commit after inserting data. Key:%s",
             key);
      assert_int_equal(ret, 0);
    }

  test_int64_1 = property_get_int64(key, 0);
  assert_int_equal(test_int64_1, INT64_MAX);

  ret = property_delete(key);
  assert_int_equal(ret, 0);
  ret = property_commit();
  if (ret != 0)
    {
      syslog(LOG_ERR, "Failed to commit after deleting data. Key:%s",
             key);
      assert_int_equal(ret, 0);
    }

#endif

  /* test key */

  sprintf(key, "persist.kv27_int32_2");
  ret = property_set_int32(key, -2147483648);
  assert_int_equal(ret, 0);
  ret = property_commit();
  if (ret != 0)
    {
      syslog(LOG_ERR, "Failed to commit after inserting data. Key:%s",
             key);
      assert_int_equal(ret, 0);
    }

  test_int32_2 = property_get_int32(key, 0);
  assert_int_equal(test_int32_2, INT32_MIN);

  ret = property_delete(key);
  assert_int_equal(ret, 0);
  ret = property_commit();
  if (ret != 0)
    {
      syslog(LOG_ERR, "Failed to commit after deleting data. Key:%s",
              key);
      assert_int_equal(ret, 0);
    }

  /* test key */

#ifdef CONFIG_LIBC_LONG_LONG
  int64_t test_int64_2;
  sprintf(key, "persist.kv27_int64_2");
  ret = property_set_int64(key, INT64_MIN);
  assert_int_equal(ret, 0);
  ret = property_commit();
  if (ret != 0)
    {
      syslog(LOG_ERR, "Failed to commit after inserting data. Key:%s",
             key);
      assert_int_equal(ret, 0);
    }

  test_int64_2 = property_get_int64(key, 0);
  assert_int_equal(test_int64_2, INT64_MIN);

  ret = property_delete(key);
  assert_int_equal(ret, 0);
  ret = property_commit();
  if (ret != 0)
    {
      syslog(LOG_ERR, "Failed to commit after deleting data. Key:%s",
             key);
      assert_int_equal(ret, 0);
    }
#endif
}
