/****************************************************************************
 * apps/examples/ls_driver_test/test_thermal.c
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

#include <stdio.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>

#include "ls_driver_test.h"

/****************************************************************************
 * Public Functions
 ****************************************************************************/

int test_thermal(void)
{
  int fd;
  int32_t temp;
  int ret;

  printf("[Thermal] Testing thermal sensor...\n");

  fd = open("/dev/thermal0", O_RDONLY);
  if (fd < 0)
    {
      printf("[Thermal] ERROR: Failed to open /dev/thermal0: %d\n", errno);
      return -errno;
    }

  ret = read(fd, &temp, sizeof(temp));
  if (ret == sizeof(temp))
    {
      printf("[Thermal] Temperature: %d.%d C\n", temp / 1000,
             temp < 0 ? (-temp % 1000) : (temp % 1000));
    }
  else
    {
      printf("[Thermal] ERROR: Failed to read temperature: %d\n", errno);
    }

  close(fd);
  printf("[Thermal] Done.\n\n");
  return OK;
}
