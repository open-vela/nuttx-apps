/****************************************************************************
 * apps/netutils/iperf/iperf_main.c
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

#include <nuttx/config.h>

#include <arpa/inet.h>
#include <getopt.h>
#include <net/if.h>
#include <strings.h>
#include <sys/time.h>
#include <sys/vm_sockets.h>

#include "iperf.h"
#include "netutils/netlib.h"

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#ifdef CONFIG_NETUTILS_IPERFTEST_DEVNAME
#  define DEVNAME CONFIG_NETUTILS_IPERFTEST_DEVNAME
#else
#  define DEVNAME "wlan0"
#endif

#define IPERF_DEFAULT_PORT     5001
#define IPERF_DEFAULT_INTERVAL 3
#define IPERF_DEFAULT_TIME     30

/****************************************************************************
 * Private Types
 ****************************************************************************/

struct wifi_iperf_s
{
  FAR char *client;
  FAR char *local;
  FAR char *rpmsg;
  FAR char *vsock;
  FAR char *bind;
  int port;
  int interval;
  int abort;
  int time;
};

/****************************************************************************
 * Private Functions
 ****************************************************************************/

/****************************************************************************
 * Name: iperf_showusage
 *
 * Description:
 *   Show usage of the demo program and exit
 *
 ****************************************************************************/

static void iperf_showusage(FAR const char *progname)
{
  printf("USAGE: %s [-sua] [-c <ip|cpu>] [-p <port>] [-i <interval>] "
         "[-t <time>] [--local <path>] [--rpmsg <name>] "
         "[--vsock <host/guest>]\n", progname);
  printf("iperf command:\n"
         "  -c, --client=<ip>    run in client mode, connecting to <host>\n"
         "  -s, --server         run in server mode\n"
         "  -u, --udp            use UDP rather than TCP\n"
         "  --local=[<path>]     use local socket\n"
         "  --rpmsg=<name>       use RPMsg socket\n"
         "  --vsock=<host/guest> use Vsocket\n"
         "  -B, --bind=<ip>      ip to bind\n"
         "  -p, --port=<port>    server port to listen on/connect to\n"
         "  -i, --interval=<interval> seconds between periodic bandwidth"
         " reports\n"
         "  -t, --time=<time>    time in seconds to transmit for (default %d"
         " secs)\n"
         "  -a, --abort          abort running iperf\n",
         IPERF_DEFAULT_TIME);
}

/****************************************************************************
 * Name: iperf_printcfg
 *
 * Description:
 *   Print config line before start
 *
 ****************************************************************************/

static void iperf_printcfg(FAR struct iperf_cfg_t *cfg)
{
  printf("\n mode=%s%s-%s ",
         cfg->flag & IPERF_FLAG_LOCAL ? "local-":
         cfg->flag & IPERF_FLAG_RPMSG ? "rpmsg-":
         cfg->flag & IPERF_FLAG_VSOCK ? "vsock-":"",
         cfg->flag & IPERF_FLAG_TCP ? "tcp":"udp",
         cfg->flag & IPERF_FLAG_SERVER ? "server":"client");

  if (cfg->flag & IPERF_FLAG_LOCAL)
    {
      printf("path=%s, ", cfg->path);
    }
  else if (cfg->flag & IPERF_FLAG_RPMSG)
    {
      printf("cpu=%s, name=%s, ", cfg->host, cfg->path);
    }
  else if (cfg->flag & IPERF_FLAG_VSOCK)
    {
      printf("cid=%" PRIu32 ", ", cfg->cid);
    }
  else
    {
      printf("sip=%" PRId32 ".%" PRId32 ".%" PRId32 ".%" PRId32 ":%d,"
             "dip=%" PRId32 ".%" PRId32 ".%" PRId32 ".%" PRId32 ":%d, ",
             cfg->sip & 0xff, (cfg->sip >> 8) & 0xff,
             (cfg->sip >> 16) & 0xff, (cfg->sip >> 24) & 0xff, cfg->sport,
             cfg->dip & 0xff, (cfg->dip >> 8) & 0xff,
             (cfg->dip >> 16) & 0xff, (cfg->dip >> 24) & 0xff, cfg->dport);
    }

  printf("interval=%" PRId32 ", time=%" PRId32 "\n",
         cfg->interval, cfg->time);
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

int main(int argc, FAR char *argv[])
{
  struct wifi_iperf_s iperf;
  struct iperf_cfg_t cfg;
  struct in_addr addr;
  int opt;

  struct option loptions[] =
  {
    {"client", required_argument, 0, 'c'},
    {"server", no_argument, 0, 's'},
    {"udp", no_argument, 0, 'u'},
    {"local", optional_argument, 0, 'L'},
    {"rpmsg", required_argument, 0, 'R'},
    {"vsock", required_argument, 0, 'V'},
    {"bind", required_argument, 0, 'B'},
    {"port", required_argument, 0, 'p'},
    {"interval", required_argument, 0, 'i'},
    {"time", required_argument, 0, 't'},
    {"abort", no_argument, 0, 'a'},
    {0, 0, 0, 0}
  };

#ifdef CONFIG_NET_IPv4
  char inetaddr[INET_ADDRSTRLEN];
#endif

  bzero(&iperf, sizeof(iperf));
  bzero(&addr, sizeof(addr));
  bzero(&cfg, sizeof(cfg));

  cfg.flag |= IPERF_FLAG_TCP;
  cfg.interval = IPERF_DEFAULT_INTERVAL;
  cfg.sport = IPERF_DEFAULT_PORT;
  cfg.dport = IPERF_DEFAULT_PORT;

  while ((opt = getopt_long(argc, argv, "suac:B:p:i:t:",
                            loptions, NULL)) != -1)
    {
      switch (opt)
        {
          case 's':
            cfg.flag |= IPERF_FLAG_SERVER;
            break;
          case 'u':
            cfg.flag &= ~IPERF_FLAG_TCP;
            cfg.flag |= IPERF_FLAG_UDP;
            break;
          case 'a':
            iperf.abort = 1;
            break;
          case 'c':
            cfg.flag |= IPERF_FLAG_CLIENT;
            iperf.client = optarg;
            break;
          case 'B':
            iperf.bind = optarg;
            break;
          case 'p':
            iperf.port = strtoul(optarg, NULL, 0);
            break;
          case 'i':
            iperf.interval = strtoul(optarg, NULL, 0);
            break;
          case 't':
            iperf.time = strtoul(optarg, NULL, 0);
            break;
          case 'L':
            cfg.flag |= IPERF_FLAG_LOCAL;
            iperf.local = optarg;
            break;
          case 'R':
            cfg.flag |= IPERF_FLAG_RPMSG;
            iperf.rpmsg = optarg;
            break;
          case 'V':
            cfg.flag |= IPERF_FLAG_VSOCK;
            iperf.vsock = optarg;
            break;
          case '?':
            iperf_showusage(argv[0]);
            exit(EXIT_FAILURE);
      }
    }

  if (iperf.abort != 0)
    {
      iperf_stop();
      printf("ERROR: abort: %d\n", iperf.abort);
      goto out;
    }

  if (((cfg.flag & IPERF_FLAG_SERVER) == 0 &&
       (cfg.flag & IPERF_FLAG_CLIENT) == 0) ||
      ((cfg.flag & IPERF_FLAG_SERVER) != 0 &&
       (cfg.flag & IPERF_FLAG_CLIENT) != 0))
    {
      printf("ERROR: should specific client/server mode\n");
      goto out;
    }

  if (cfg.flag & IPERF_FLAG_SERVER)
    {
      cfg.host = "";
    }
  else
    {
      cfg.dip  = inet_addr(iperf.client);
      cfg.host = iperf.client;
    }

  if (cfg.flag & IPERF_FLAG_LOCAL)
    {
      if (iperf.local)
        {
          cfg.path = iperf.local;
        }
      else if (iperf.client)
        {
          cfg.path = iperf.client;
        }
      else
        {
          printf("ERROR: should specific local socket path\n");
          goto out;
        }
    }
  else if (cfg.flag & IPERF_FLAG_RPMSG)
    {
      cfg.path = iperf.rpmsg;
    }
  else if (cfg.flag & IPERF_FLAG_VSOCK)
    {
      if (strncmp(iperf.vsock, "guest", 5) == 0)
        {
          cfg.flag |= IPERF_FLAG_VGUEST;
        }

      if (cfg.flag & IPERF_FLAG_CLIENT)
        {
          FAR char *endptr;

          cfg.cid = strtoul(iperf.client, &endptr, 0);
          if (errno != 0 || *endptr != '\0')
            {
              cfg.cid = 0;
              strncpy((FAR char *)&cfg.cid, iperf.client,
                      MIN(sizeof(cfg.cid), strlen(iperf.client)));
            }
        }
      else
        {
          cfg.cid = VMADDR_CID_ANY;
        }
    }
  else
    {
#ifdef CONFIG_NET_IPv4
      if (iperf.bind)
        {
          addr.s_addr = inet_addr(iperf.bind);
          if (addr.s_addr == INADDR_NONE)
            {
              printf("ERROR: access IP is 0xffffffff\n");
              goto out;
            }
        }
      else
        {
          netlib_get_ipv4addr(DEVNAME, &addr);
          if (addr.s_addr == 0)
            {
              printf("ERROR: access IP is 0x00\n");
              goto out;
            }
        }

      printf("     IP: %s\n", inet_ntoa_r(addr, inetaddr, sizeof(inetaddr)));
      cfg.sip = addr.s_addr;
#else
      printf("ERROR: IPv4 Not Enabled\n");
      goto out;
#endif
    }

  if (iperf.port != 0)
    {
      if (cfg.flag & IPERF_FLAG_SERVER)
        {
          cfg.sport = iperf.port;
        }
      else
        {
          cfg.dport = iperf.port;
        }
    }

  if (iperf.interval > 0)
    {
      cfg.interval = iperf.interval;
    }

  if (iperf.time == 0)
    {
      if (cfg.flag & IPERF_FLAG_SERVER)
        {
          cfg.time = 0;
        }
      else
        {
          cfg.time = IPERF_DEFAULT_TIME;
        }
    }
  else
    {
      cfg.time = iperf.time;
      if (cfg.time != 0 && cfg.time <= cfg.interval)
        {
          cfg.time = cfg.interval;
        }
    }

  iperf_printcfg(&cfg);
  iperf_start(&cfg);
  return 0;

out:
  iperf_showusage(argv[0]);
  return 0;
}
