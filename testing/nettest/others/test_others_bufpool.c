/****************************************************************************
 * apps/testing/nettest/others/test_others_bufpool.c
 *
 * Licensed to the Apache Software Foundation (ASF) under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.  The
 * ASF licenses this file to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance with the
 * License.  You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.  See the
 * License for the specific language governing permissions and limitations
 * under the License.
 *
 ****************************************************************************/

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <errno.h>
#include <setjmp.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <cmocka.h>

#include "../../nuttx/net/utils/utils.h"

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#define TEST_PREALLOC  10
#define TEST_DYNALLOC  1
#define TEST_MAXALLOC  20
#define TEST_LOOP_CNT  5
#define TEST_BUF_SIZE  32

/****************************************************************************
 * Private Types
 ****************************************************************************/

struct test_buf_s
{
  sq_entry_t node;
  char buf[TEST_BUF_SIZE];
};

/****************************************************************************
 * Private Data
 ****************************************************************************/

const static struct test_buf_s g_zero;

/****************************************************************************
 * Private Functions
 ****************************************************************************/

/****************************************************************************
 * Name: test_net_bufpool_alloc_to
 ****************************************************************************/

static void test_net_bufpool_alloc_to(FAR struct net_bufpool_s *pool,
                                      FAR sq_queue_t *allocated)
{
  FAR struct test_buf_s *node;
  assert_true(NET_BUFPOOL_TEST(*pool) == OK);

  /* Allocate a buffer from the pool and check it's zeroed */

  node = NET_BUFPOOL_ALLOC(*pool);
  assert_non_null(node);
  assert_true(memcmp(node, &g_zero, sizeof(*node)) == 0);

  /* Add the allocated buffer to the allocated list */

  sq_addlast(&node->node, allocated);
}

/****************************************************************************
 * Name: test_net_bufpool_free_from
 ****************************************************************************/

static void test_net_bufpool_free_from(FAR struct net_bufpool_s *pool,
                                       FAR sq_queue_t *allocated)
{
  while (!sq_empty(allocated))
    {
      FAR struct test_buf_s *node = (FAR void *)sq_remfirst(allocated);

      /* Write random data to the buffer and free it */

      arc4random_buf(node, sizeof(*node));
      NET_BUFPOOL_FREE(*pool, node);
      assert_true(NET_BUFPOOL_TEST(*pool) == OK);
    }
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: test_others_bufpool
 ****************************************************************************/

void test_others_bufpool(FAR void **state)
{
  NET_BUFPOOL_DECLARE(fixed, sizeof(struct test_buf_s), TEST_PREALLOC, 0, 0);
  NET_BUFPOOL_DECLARE(limited, sizeof(struct test_buf_s), TEST_PREALLOC,
                               TEST_DYNALLOC, TEST_MAXALLOC);
  NET_BUFPOOL_DECLARE(unlimited, sizeof(struct test_buf_s), TEST_PREALLOC,
                                 TEST_DYNALLOC, 0);
  sq_queue_t allocated_fixed;
  sq_queue_t allocated_limited;
  sq_queue_t allocated_unlimited;
  FAR sq_entry_t *node;
  int loop;
  int i;

#ifdef NET_BUFPOOL_INIT
  NET_BUFPOOL_INIT(fixed);
  NET_BUFPOOL_INIT(limited);
  NET_BUFPOOL_INIT(unlimited);
#endif
  sq_init(&allocated_fixed);
  sq_init(&allocated_limited);
  sq_init(&allocated_unlimited);

  for (loop = 0; loop < TEST_LOOP_CNT; loop++)
    {
      for (i = 0; i < TEST_PREALLOC; i++)
        {
          test_net_bufpool_alloc_to(&fixed, &allocated_fixed);
          test_net_bufpool_alloc_to(&limited, &allocated_limited);
          test_net_bufpool_alloc_to(&unlimited, &allocated_unlimited);
        }

      for (i = TEST_PREALLOC; i < TEST_MAXALLOC; i++)
        {
          test_net_bufpool_alloc_to(&limited, &allocated_limited);
          test_net_bufpool_alloc_to(&unlimited, &allocated_unlimited);
        }

      node = NET_BUFPOOL_TRYALLOC(fixed);
      assert_null(node);
      assert_true(NET_BUFPOOL_TEST(fixed) == -ENOSPC);

      node = NET_BUFPOOL_TRYALLOC(limited);
      assert_null(node);
      assert_true(NET_BUFPOOL_TEST(limited) == -ENOSPC);

      node = NET_BUFPOOL_TRYALLOC(unlimited);
      assert_non_null(node);
      assert_true(NET_BUFPOOL_TEST(unlimited) == OK);
      NET_BUFPOOL_FREE(unlimited, node);

      test_net_bufpool_free_from(&fixed, &allocated_fixed);
      test_net_bufpool_free_from(&limited, &allocated_limited);
      test_net_bufpool_free_from(&unlimited, &allocated_unlimited);
    }
}
