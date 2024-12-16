/****************************************************************************
 * apps/testing/nettest/utils/nettest_netdump.c
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

#include <net/if.h>
#include <netpacket/packet.h>
#include <stdio.h>

#include <nuttx/net/net.h>
#include <nuttx/net/tcp.h>

#include "utils.h"

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#define TEST_DUMP_WIAT_TIME      10
#define TEST_WAIT_TRANS_FIN_TIME 10000

/****************************************************************************
 * Private Functions
 ****************************************************************************/

/****************************************************************************
 * Name: socket_open
 ****************************************************************************/

static int socket_open(int ifindex)
{
  int sd;
  struct sockaddr_ll addr;

  sd = socket(PF_PACKET, SOCK_RAW | SOCK_NONBLOCK, 0);
  if (sd < 0)
    {
      perror("ERROR: failed to create packet socket");
      return -errno;
    }

  /* Prepare sockaddr struct */

  addr.sll_family = AF_PACKET;
  addr.sll_ifindex = ifindex;
  if (bind(sd, (FAR const struct sockaddr *)&addr, sizeof(addr)) < 0)
    {
      perror("ERROR: binding socket failed");
      close(sd);
      return -errno;
    }

  return sd;
}

/****************************************************************************
 * Name: nettest_netdump_stop_work
 ****************************************************************************/

static void nettest_netdump_stop_work(FAR void *arg)
{
  FAR struct net_dump_s *netdump_state = (FAR struct net_dump_s *)arg;
  netdump_state->count = 0;
}

/****************************************************************************
 * Name: nettest_tcpdump
 ****************************************************************************/

static FAR void *nettest_netdump(pthread_addr_t pvarg)
{
  int sd;
  ssize_t len;
  uint8_t buf[MAX_NETDEV_PKTSIZE];
  FAR struct buf_node_s *buf_node;
  FAR struct net_dump_s *netdump_state = (FAR struct net_dump_s *)pvarg;

  sd = socket_open(netdump_state->ifindex);
  if (sd < 0)
    {
      return NULL;
    }

  while (netdump_state->count > 0)
    {
      len = read(sd, buf, sizeof(buf));

      if ((len == 0) || (len == -1 && errno == EAGAIN))
        {
          usleep(TICK_PER_MSEC);
          continue;
        }
      else if (len < 0)
        {
          break;
        }

      work_queue(USRWORK, &netdump_state->work, nettest_netdump_stop_work,
                 netdump_state, TEST_DUMP_WIAT_TIME * TICK_PER_MSEC);

      if (netdump_state->filter(buf) == false)
        {
          continue;
        }
      else
        {
          netdump_state->count--;
          buf_node = malloc(sizeof(struct buf_node_s));
          if (buf_node == NULL)
            {
              perror("ERROR: failed to allocate memory");
              break;
            }

          memcpy(buf_node->buf, buf, len);
          buf_node->len = len;
          sq_addlast(buf_node, &netdump_state->data);
        }
    }

  work_cancel_sync(USRWORK, &netdump_state->work);
  close(sd);

  return NULL;
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: nettest_l3len
 ****************************************************************************/

uint8_t nettest_l3len(enum net_lltype_e lltype, uint8_t len)
{
  assert(lltype == NET_LL_ETHERNET || lltype == NET_LL_LOOPBACK);
  switch (lltype)
    {
      case NET_LL_ETHERNET:
        return len - ETH_HDRLEN;

      case NET_LL_LOOPBACK:
      default:
        return len;
    }
}

/****************************************************************************
 * Name: nettest_l3buf
 ****************************************************************************/

FAR uint8_t *nettest_l3buf(enum net_lltype_e lltype, FAR uint8_t *l2buf)
{
  assert(lltype == NET_LL_ETHERNET || lltype == NET_LL_LOOPBACK);
  switch (lltype)
    {
      case NET_LL_ETHERNET:
        return l2buf + ETH_HDRLEN;

      case NET_LL_LOOPBACK:
      default:
        return l2buf;
    }
}

/****************************************************************************
 * Name: nettest_l3proto
 ****************************************************************************/

uint8_t nettest_l3proto(FAR uint8_t *l3buf)
{
  return l3buf[0] & IP_VERSION_MASK;
}

/****************************************************************************
 * Name: nettest_l4buf
 ****************************************************************************/

FAR uint8_t *nettest_l4buf(FAR uint8_t *l3buf)
{
  assert(nettest_l3proto(l3buf) == IPv4_VERSION ||
         nettest_l3proto(l3buf) == IPv6_VERSION);
  switch (nettest_l3proto(l3buf))
    {
      case IPv4_VERSION:
        return l3buf + IPv4_HDRLEN;

      case IPv6_VERSION:
        return l3buf + IPv6_HDRLEN;

      default:
        return NULL;
    }
}

/****************************************************************************
 * Name: nettest_l4proto
 ****************************************************************************/

uint8_t nettest_l4proto(FAR uint8_t *l3buf)
{
  assert(nettest_l3proto(l3buf) == IPv4_VERSION ||
         nettest_l3proto(l3buf) == IPv6_VERSION);
  switch (nettest_l3proto(l3buf))
    {
      case IPv4_VERSION:
        FAR struct ipv4_hdr_s *ipv4 = (FAR struct ipv4_hdr_s *)l3buf;
        return ipv4->proto;

      case IPv6_VERSION:
        FAR struct ipv6_hdr_s *ipv6 = (FAR struct ipv6_hdr_s *)l3buf;
        return ipv6->proto;

      default:
        return 0;
    }
}

/****************************************************************************
 * Name: nettest_filter_tcp_state
 ****************************************************************************/

bool nettest_filter_tcp_state(enum net_lltype_e lltype, uint16_t tcp_state,
                              FAR uint8_t *buf)
{
  FAR uint8_t *l3buf = nettest_l3buf(lltype, buf);
  FAR uint8_t *l4buf = nettest_l4buf(l3buf);

  if (nettest_l4proto(l3buf) == IP_PROTO_TCP)
    {
      FAR struct tcp_hdr_s *th = (FAR struct tcp_hdr_s *)l4buf;
      return (th->flags & tcp_state) != 0;
    }

  return false;
}

/****************************************************************************
 * Name: nettest_netdump_stop
 ****************************************************************************/

void nettest_netdump_stop(FAR struct net_dump_s *netdump_state)
{
  if (netdump_state->tid > 0)
    {
      /* Delay ensures all packets are caught */

      usleep(TEST_WAIT_TRANS_FIN_TIME);
      nettest_netdump_stop_work(netdump_state);
      pthread_join(netdump_state->tid, NULL);
      netdump_state->tid = -1;
    }
}

/****************************************************************************
 * Name: nettest_dump_destroy
 ****************************************************************************/

void nettest_dump_destroy(FAR struct net_dump_s *netdump_state)
{
  FAR struct buf_node_s *node;
  FAR struct buf_node_s *tmp;

  nettest_netdump_stop(netdump_state);

  sq_for_every_safe(&netdump_state->data, node, tmp)
    {
      sq_rem(node, &netdump_state->data);
      free(node);
    }
}

/****************************************************************************
 * Name: nettest_dump_setup
 ****************************************************************************/

int nettest_dump_setup(FAR struct net_dump_s *cfg,
                       FAR const char *netdev_name,
                       nettest_filter_t filter, uint16_t count)
{
  cfg->filter = filter;
  sq_init(&cfg->data);
  memset(&cfg->work, 0, sizeof(struct work_s));

  cfg->ifindex = if_nametoindex(netdev_name);
  if (cfg->ifindex == 0)
    {
      printf("Failed to get index of device %s\n", netdev_name);
      return NULL;
    }

  cfg->count = (count == 0) ? INT32_MAX : count;

  return pthread_create(&cfg->tid, NULL, nettest_netdump, cfg);
}
