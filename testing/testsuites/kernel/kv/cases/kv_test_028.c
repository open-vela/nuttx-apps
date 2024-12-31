/****************************************************************************
 * apps/testing/testsuites/kernel/kv/cases/kv_test_028.c
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
 * Name: test_nuttx_kv28
 ****************************************************************************/

void test_nuttx_kv28(FAR void **state)
{
  int ret;
  char key[TEST_KEY_LENGTH] =
  {
    0
  };

  int32_t test_int32;

  for (int i = 1; i <= 300; i++)
    {
      sprintf(key, "persist.kv28_int32_%d", i);
      ret = property_set_int32(key, i);
      assert_int_equal(ret, 0);
      ret = property_commit();
      if (ret != 0)
        {
          syslog(
              LOG_ERR,
              "Failed to commit after inserting data. Key:%s Value:%d",
              key, i);
          assert_int_equal(ret, 0);
        }

      test_int32 = property_get_int32(key, 0);
      assert_int_equal(test_int32, i);

      ret = property_delete(key);
      assert_int_equal(ret, 0);
      ret = property_commit();
      if (ret != 0)
        {
          syslog(LOG_ERR,
                 "Failed to commit after deleting data. Key:%s Value:%d",
                 key, i);
          assert_int_equal(ret, 0);
        }
    }

#ifdef CONFIG_LIBC_LONG_LONG
  int64_t test_int64;
  for (int i = 1; i <= 300; i++)
    {
      sprintf(key, "persist.kv28_int64_%d", i);
      ret = property_set_int64(key, i);
      assert_int_equal(ret, 0);
      ret = property_commit();
      if (ret != 0)
        {
          syslog(
              LOG_ERR,
              "Failed to commit after inserting data. Key:%s Value:%d",
              key, i);
          assert_int_equal(ret, 0);
        }

      test_int64 = property_get_int64(key, 0);
      assert_int_equal(test_int64, i);

      ret = property_delete(key);
      assert_int_equal(ret, 0);
      ret = property_commit();
      if (ret != 0)
        {
          syslog(LOG_ERR,
                 "Failed to commit after deleting data. Key:%s Value:%d",
                 key, i);
          assert_int_equal(ret, 0);
        }
    }
#endif
}
