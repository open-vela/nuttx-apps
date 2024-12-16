/****************************************************************************
 * apps/testing/nettest/tcp/test_tcp_common.c
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

#include <netinet/in.h>
#include <setjmp.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/time.h>

#include <cmocka.h>

#include "test_tcp.h"
#include "utils.h"

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#define NETDEV_NAME          "lo"
#define TEST_SND_RCV_TIMEOUT 10000

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: test_tcp_group_setup
 ****************************************************************************/

int test_tcp_group_setup(FAR void **state)
{
  FAR struct nettest_tcp_state_s *tcp_state = zalloc(sizeof(*tcp_state));
  assert_non_null(tcp_state);

  *state = tcp_state;

  tcp_state->server_tid = nettest_create_tcp_lo_server();
  assert_return_code(tcp_state->server_tid, errno);

  return 0;
}

/****************************************************************************
 * Name: test_tcp_group_teardown
 ****************************************************************************/

int test_tcp_group_teardown(FAR void **state)
{
  FAR struct nettest_tcp_state_s *tcp_state = *state;

  int ret = nettest_destroy_tcp_lo_server(tcp_state->server_tid);
  assert_return_code(ret, errno);

  free(tcp_state);

  return 0;
}

/****************************************************************************
 * Name: test_tcp_common_teardown
 ****************************************************************************/

int test_tcp_common_teardown(FAR void **state)
{
  FAR struct nettest_tcp_state_s *tcp_state = *state;

  if (tcp_state->client_fd > 0)
    {
      shutdown(tcp_state->client_fd, SHUT_RDWR);
      close(tcp_state->client_fd);
      tcp_state->client_fd = -1;
    }

#ifdef CONFIG_NET_PKT
  nettest_dump_destroy(&tcp_state->dump_state);
#endif

  return 0;
}

/****************************************************************************
 * Name: test_tcp_common_setup
 ****************************************************************************/

int test_tcp_common_setup(FAR struct nettest_tcp_state_s *tcp_state,
                          int family, nettest_filter_t filter)
{
  struct timeval tv;
  int ret;

  assert(family == AF_INET || family == AF_INET6);

  tcp_state->client_fd = socket(family, SOCK_STREAM, 0);
  assert_return_code(tcp_state->client_fd, errno);

  tv.tv_sec  = 0;
  tv.tv_usec = TEST_SND_RCV_TIMEOUT;
  ret = setsockopt(tcp_state->client_fd, SOL_SOCKET, SO_RCVTIMEO,
                   &tv, sizeof(tv));
  if (ret < 0)
    {
      goto errout;
    }

  ret = setsockopt(tcp_state->client_fd, SOL_SOCKET, SO_SNDTIMEO,
                   &tv, sizeof(tv));
  if (ret < 0)
    {
      goto errout;
    }

#ifdef CONFIG_NET_PKT
  if (filter != NULL)
    {
      ret = nettest_dump_setup(&tcp_state->dump_state, NETDEV_NAME,
                               filter, 0);
    }
#endif

  return ret;

errout:
  close(tcp_state->client_fd);
  return ret;
}
