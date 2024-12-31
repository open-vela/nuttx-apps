/****************************************************************************
 * apps/testing/testsuites/kernel/kv/cases/kv_test_014.c
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
#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <string.h>
#include <cmocka.h>
#include <time.h>
#include "KvTest.h"
#include "kvdb.h"

struct kv_struct
{
  char key[TEST_KEY_LENGTH];
  char value[TEST_VALUE_LENGTH];
  struct kv_struct *next;
};

static struct kv_struct *head;
static struct kv_struct *tail;
static void callback(const char *key, const char *value, void *cookie);
static void freedata(void);

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: test_nuttx_kv14
 ****************************************************************************/

void test_nuttx_kv14(FAR void **state)
{
  int ret = 0;
  char data[TEST_VALUE_LENGTH] =
  {
    0
  };

  head = (struct kv_struct *)zalloc(sizeof(struct kv_struct));
  tail = head;
  property_list(callback, NULL);

  for (struct kv_struct *item = head->next; item != NULL;
       item = item->next)
    {
      syslog(LOG_INFO, "%s: %s\n", item->key, item->value);

      ret = property_get(item->key, data, "");
      assert_int_equal(ret, strlen(item->value));
      assert_int_equal(strcmp(item->value, data), 0);
    }

  freedata();
}

static void callback(const char *key, const char *value, void *cookie)
{
  struct kv_struct *data =
      (struct kv_struct *)malloc(sizeof(struct kv_struct));
  strcpy(data->key, key);
  strcpy(data->value, value);
  data->next = NULL;

  tail->next = data;
  tail = data;
}

static void freedata(void)
{
  struct kv_struct *tmp;

  while (head)
    {
      tmp = head;
      head = head->next;
      free(tmp);
    };
}
