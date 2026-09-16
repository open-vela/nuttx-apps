/****************************************************************************
 * apps/examples/ls_driver_test/test_common.c
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

#include <nuttx/ioexpander/gpio.h>
#include <nuttx/pinctrl/pinctrl.h>

#include "ls_driver_test.h"

/****************************************************************************
 * Public Functions
 ****************************************************************************/

int pinctrl_set_gpio_function(int pin)
{
  int fd;
  int ret;

  fd = open("/dev/pinctrl0", O_RDWR);
  if (fd < 0)
    {
      printf("[Pinctrl] ERROR: Failed to open /dev/pinctrl0: %d\n", errno);
      return -errno;
    }

  ret = ioctl(fd, PINCTRLC_SELECTGPIO, (unsigned long)pin);
  if (ret < 0)
    {
      printf("[Pinctrl] ERROR: PINCTRLC_SELECTGPIO pin %d failed: %d\n",
             pin, errno);
      close(fd);
      return -errno;
    }

  close(fd);
  return OK;
}
