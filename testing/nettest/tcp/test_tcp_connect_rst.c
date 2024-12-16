/****************************************************************************
 * apps/testing/nettest/tcp/test_tcp_connect_rst.c
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

#include <setjmp.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <cmocka.h>

#include <nuttx/net/tcp.h>

#include "test_tcp.h"
#include "utils.h"

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#define TEST_WRONG_PORT 9999

/****************************************************************************
 * Private Functions
 ****************************************************************************/

static bool filter(FAR uint8_t *buf)
{
  return nettest_filter_tcp_state(NET_LL_LOOPBACK, TCP_RST, buf);
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: test_tcp_connect_rst_setup
 ****************************************************************************/

int test_tcp_connect_rst_setup(FAR void **state)
{
  FAR struct nettest_tcp_state_s *tcp_state = *state;

  return test_tcp_common_setup(tcp_state, AF_INET, filter);
}

/****************************************************************************
 * Name: test_tcp_connect_rst
 ****************************************************************************/

void test_tcp_connect_rst(FAR void **state)
{
  FAR struct nettest_tcp_state_s *tcp_state = *state;
  struct sockaddr_in myaddr;
  int ret;

  assert_false(TEST_WRONG_PORT == CONFIG_TESTING_NET_TCP_PORT);

  myaddr.sin_family      = AF_INET;
  myaddr.sin_port        = htons(TEST_WRONG_PORT);
  myaddr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

  ret = connect(tcp_state->client_fd, (FAR struct sockaddr *)&myaddr,
                sizeof(struct sockaddr_in));
  assert_true(ret < 0 && errno == ECONNREFUSED);

  close(tcp_state->client_fd);
  tcp_state->client_fd = -1;

  nettest_netdump_stop(&tcp_state->dump_state);

  assert_int_equal(sq_count(&tcp_state->dump_state.data), 1);
}
