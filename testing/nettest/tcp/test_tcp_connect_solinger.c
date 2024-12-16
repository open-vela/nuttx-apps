/****************************************************************************
 * apps/testing/nettest/tcp/test_tcp_connect_solinger.c
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
 * Name: test_tcp_connect_solinger_setup
 ****************************************************************************/

int test_tcp_connect_solinger_setup(FAR void **state)
{
  FAR struct nettest_tcp_state_s *tcp_state = *state;

  return test_tcp_common_setup(tcp_state, AF_INET, filter);
}

/****************************************************************************
 * Name: test_tcp_connect_solinger
 ****************************************************************************/

void test_tcp_connect_solinger(FAR void **state)
{
  FAR struct nettest_tcp_state_s *tcp_state = *state;
  struct sockaddr_in myaddr;
  struct linger so_linger;
  int addrlen;
  int ret;

  so_linger.l_onoff = TRUE;
  so_linger.l_linger = 0;
  ret = setsockopt(tcp_state->client_fd, SOL_SOCKET,
                   SO_LINGER, &so_linger, sizeof so_linger);
  assert_return_code(ret, errno);

  addrlen = nettest_lo_addr((FAR struct sockaddr *)&myaddr, AF_INET);

  ret = connect(tcp_state->client_fd, (FAR struct sockaddr *)&myaddr,
                addrlen);
  assert_return_code(ret, errno);

  close(tcp_state->client_fd);
  tcp_state->client_fd = -1;

  nettest_netdump_stop(&tcp_state->dump_state);

  assert_int_equal(sq_count(&tcp_state->dump_state.data), 1);
}
