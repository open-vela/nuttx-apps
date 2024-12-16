/****************************************************************************
 * apps/testing/nettest/utils/utils.h
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

#ifndef __APPS_TESTING_NETTEST_UTILS_UTILS_H
#define __APPS_TESTING_NETTEST_UTILS_UTILS_H

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <pthread.h>

#include <nuttx/net/net.h>
#include <nuttx/net/netconfig.h>

typedef CODE bool (*nettest_filter_t)(FAR uint8_t *);

/****************************************************************************
 * Public Types
 ****************************************************************************/

#ifdef CONFIG_NET_PKT
struct buf_node_s
{
  FAR struct buf_node_s *flink;
  uint8_t len;
  uint8_t buf[MAX_NETDEV_PKTSIZE];
};

struct net_dump_s
{
  struct work_s work;
  pthread_t tid;

  sq_queue_t data;

  nettest_filter_t filter;
  int ifindex;
  int count;
};
#endif

/****************************************************************************
 * Public Function Prototypes
 ****************************************************************************/

/****************************************************************************
 * Name: nettest_lo_addr
 ****************************************************************************/

#if defined (CONFIG_TESTING_NET_TCP)
socklen_t nettest_lo_addr(FAR struct sockaddr *addr, int family);

/****************************************************************************
 * Name: nettest_destroy_tcp_lo_server
 ****************************************************************************/

int nettest_destroy_tcp_lo_server(pthread_t server_tid);

/****************************************************************************
 * Name: nettest_create_tcp_lo_server
 ****************************************************************************/

pthread_t nettest_create_tcp_lo_server(void);
#endif

/****************************************************************************
 * Name: nettest_l3len
 ****************************************************************************/

#ifdef CONFIG_NET_PKT
uint8_t nettest_l3len(enum net_lltype_e lltype, uint8_t len);

/****************************************************************************
 * Name: nettest_l3buf
 ****************************************************************************/

FAR uint8_t *nettest_l3buf(enum net_lltype_e lltype, FAR uint8_t *l2buf);

/****************************************************************************
 * Name: nettest_l3proto
 ****************************************************************************/

uint8_t nettest_l3proto(FAR uint8_t *l3buf);

/****************************************************************************
 * Name: nettest_l4buf
 ****************************************************************************/

FAR uint8_t *nettest_l4buf(FAR uint8_t *l3buf);

/****************************************************************************
 * Name: nettest_l4proto
 ****************************************************************************/

uint8_t nettest_l4proto(FAR uint8_t *l3buf);

/****************************************************************************
 * Name: nettest_filter_tcp_state
 ****************************************************************************/

bool nettest_filter_tcp_state(enum net_lltype_e lltype, uint16_t tcp_state,
                              FAR uint8_t *buf);

/****************************************************************************
 * Name: nettest_netdump_stop
 ****************************************************************************/

void nettest_netdump_stop(FAR struct net_dump_s *netdump_state);

/****************************************************************************
 * Name: nettest_dump_destroy
 ****************************************************************************/

void nettest_dump_destroy(FAR struct net_dump_s *tcpdump_state);

/****************************************************************************
 * Name: nettest_dump_setup
 ****************************************************************************/

int nettest_dump_setup(FAR struct net_dump_s *cfg,
                       FAR const char *netdev_name,
                       nettest_filter_t filter, uint16_t count);
#endif
#endif /* __APPS_TESTING_NETTEST_UTILS_UTILS_H */
