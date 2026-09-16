/****************************************************************************
 * apps/examples/ls_driver_test/test_rtc.c
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
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <time.h>

#include <nuttx/arch.h>
#include <nuttx/timers/rtc.h>

#include "ls_driver_test.h"

/****************************************************************************
 * Public Functions
 ****************************************************************************/

int test_rtc(void)
{
  int fd;
  int ret;
  struct rtc_time rtctime;
  struct tm tm_buf;
  time_t t;
  char timebuf[64];

  printf("[RTC] Testing RTC...\n");

  fd = open("/dev/rtc0", O_RDWR);
  if (fd < 0)
    {
      printf("[RTC] ERROR: Failed to open /dev/rtc0: %d\n", errno);
      return -errno;
    }

  ret = ioctl(fd, RTC_RD_TIME, (unsigned long)(uintptr_t)&rtctime);
  if (ret < 0)
    {
      printf("[RTC] ERROR: RTC_RD_TIME failed: %d\n", errno);
      close(fd);
      return -errno;
    }

  printf("[RTC] Current RTC time: %04d-%02d-%02d %02d:%02d:%02d\n",
         rtctime.tm_year + 1900, rtctime.tm_mon + 1, rtctime.tm_mday,
         rtctime.tm_hour, rtctime.tm_min, rtctime.tm_sec);

  memset(&tm_buf, 0, sizeof(tm_buf));
  tm_buf.tm_sec   = rtctime.tm_sec;
  tm_buf.tm_min   = rtctime.tm_min;
  tm_buf.tm_hour  = rtctime.tm_hour;
  tm_buf.tm_mday  = rtctime.tm_mday;
  tm_buf.tm_mon   = rtctime.tm_mon;
  tm_buf.tm_year  = rtctime.tm_year;
  tm_buf.tm_isdst = -1;

  t = mktime(&tm_buf);
  if (t != (time_t)-1)
    {
      struct tm *tmp = localtime(&t);
      if (tmp)
        {
          strftime(timebuf, sizeof(timebuf), "%Y-%m-%d %H:%M:%S %A", tmp);
          printf("[RTC] Formatted time: %s\n", timebuf);
        }
    }

  printf("[RTC] Setting time to 2025-01-01 12:00:00...\n");

  memset(&rtctime, 0, sizeof(rtctime));
  rtctime.tm_year = 2025 - 1900;
  rtctime.tm_mon  = 1 - 1;
  rtctime.tm_mday = 1;
  rtctime.tm_hour = 12;
  rtctime.tm_min  = 0;
  rtctime.tm_sec  = 0;

  ret = ioctl(fd, RTC_SET_TIME, (unsigned long)(uintptr_t)&rtctime);
  if (ret < 0)
    {
      printf("[RTC] ERROR: RTC_SET_TIME failed: %d\n", errno);
      close(fd);
      return -errno;
    }

  printf("[RTC] Time set successfully\n");

  up_mdelay(2000);

  ret = ioctl(fd, RTC_RD_TIME, (unsigned long)(uintptr_t)&rtctime);
  if (ret < 0)
    {
      printf("[RTC] ERROR: RTC_RD_TIME after set failed: %d\n", errno);
      close(fd);
      return -errno;
    }

  printf("[RTC] RTC time after 2s: %04d-%02d-%02d %02d:%02d:%02d\n",
         rtctime.tm_year + 1900, rtctime.tm_mon + 1, rtctime.tm_mday,
         rtctime.tm_hour, rtctime.tm_min, rtctime.tm_sec);

  if (rtctime.tm_year + 1900 == 2025 &&
      rtctime.tm_mon + 1 == 1 &&
      rtctime.tm_mday == 1 &&
      rtctime.tm_hour == 12 &&
      rtctime.tm_min == 0 &&
      rtctime.tm_sec >= 2 && rtctime.tm_sec <= 3)
    {
      printf("[RTC] Time verification PASSED\n");
    }
  else
    {
      printf("[RTC] WARNING: Time verification unexpected, "
             "RTC may not be running\n");
    }

  close(fd);
  printf("[RTC] Done.\n\n");
  return OK;
}
