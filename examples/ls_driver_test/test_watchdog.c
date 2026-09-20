/****************************************************************************
 * apps/examples/ls_driver_test/test_watchdog.c
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

#include <sys/ioctl.h>
#include <stdio.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>

#include <nuttx/arch.h>
#include <nuttx/timers/watchdog.h>

#include "ls_driver_test.h"

/****************************************************************************
 * Public Functions
 ****************************************************************************/

int test_watchdog(void)
{
  int fd;
  int ret;

  printf("[Watchdog] Testing watchdog timer...\n");

  fd = open("/dev/watchdog0", O_RDWR);
  if (fd < 0)
    {
      printf("[Watchdog] ERROR: Failed to open /dev/watchdog0: %d\n", errno);
      return -errno;
    }

  ret = ioctl(fd, WDIOC_SETTIMEOUT, 5000);
  if (ret < 0)
    {
      printf("[Watchdog] ERROR: WDIOC_SETTIMEOUT failed: %d\n", errno);
      close(fd);
      return -errno;
    }

  printf("[Watchdog] Set timeout to 5000 ms\n");

  ret = ioctl(fd, WDIOC_START, 0);
  if (ret < 0)
    {
      printf("[Watchdog] ERROR: WDIOC_START failed: %d\n", errno);
      close(fd);
      return -errno;
    }

  printf("[Watchdog] Watchdog started\n");

  ioctl(fd, WDIOC_KEEPALIVE, 0);
  printf("[Watchdog] Keeping alive for 3 seconds...\n");
  up_mdelay(1000);
  ioctl(fd, WDIOC_KEEPALIVE, 0);
  printf("[Watchdog] Keepalive 1\n");

  up_mdelay(1000);
  ioctl(fd, WDIOC_KEEPALIVE, 0);
  printf("[Watchdog] Keepalive 2\n");

  up_mdelay(1000);
  ioctl(fd, WDIOC_KEEPALIVE, 0);
  printf("[Watchdog] Keepalive 3\n");

  ret = ioctl(fd, WDIOC_STOP, 0);
  if (ret < 0)
    {
      printf("[Watchdog] ERROR: WDIOC_STOP failed: %d\n", errno);
    }
  else
    {
      printf("[Watchdog] Watchdog stopped\n");
    }

  close(fd);
  printf("[Watchdog] Done.\n\n");
  return OK;
}
