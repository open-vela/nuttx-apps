/****************************************************************************
 * apps/examples/ls_driver_test/test_led.c
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
#include <nuttx/ioexpander/gpio.h>

#include "ls_driver_test.h"

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#define LED_RED_GPIO    72
#define LED_GREEN_GPIO  73
#define LED_COUNT       2

/****************************************************************************
 * Private Data
 ****************************************************************************/

static const int led_pins[LED_COUNT] =
{
  LED_RED_GPIO,
  LED_GREEN_GPIO,
};

/****************************************************************************
 * Public Functions
 ****************************************************************************/

int test_led(void)
{
  int fds[LED_COUNT];
  int ret;
  char path[32];
  int i;
  int j;
  int ok = 0;

  printf("[LED] Testing %d LEDs (active-low)...\n", LED_COUNT);

  /* Initialize all LEDs */

  for (i = 0; i < LED_COUNT; i++)
    {
      fds[i] = -1;
      printf("[LED] Setting up GPIO%d...\n", led_pins[i]);

      ret = pinctrl_set_gpio_function(led_pins[i]);
      if (ret < 0)
        {
          printf("[LED] ERROR: Failed to set pinctrl for GPIO%d\n",
                 led_pins[i]);
          goto cleanup;
        }

      printf("[LED] Pinctrl: GPIO%d set to GPIO function\n", led_pins[i]);

      snprintf(path, sizeof(path), "/dev/gpio%d", led_pins[i]);

      fds[i] = open(path, O_RDWR);
      if (fds[i] < 0)
        {
          printf("[LED] ERROR: Failed to open %s: %d\n", path, errno);
          ret = -errno;
          goto cleanup;
        }

      ret = ioctl(fds[i], GPIOC_SETPINTYPE,
                  (unsigned long)GPIO_OUTPUT_PIN);
      if (ret < 0)
        {
          printf("[LED] ERROR: GPIOC_SETPINTYPE(OUTPUT) failed on "
                 "GPIO%d: %d\n",
                 led_pins[i], errno);
          ret = -errno;
          goto cleanup;
        }

      ok++;
    }

  printf("[LED] Blinking all %d LEDs 3 times (low=ON, high=OFF)...\n",
         LED_COUNT);

  for (i = 0; i < 3; i++)
    {
      /* Turn all LEDs ON */

      for (j = 0; j < LED_COUNT; j++)
        {
          ret = ioctl(fds[j], GPIOC_WRITE, 0);
          if (ret < 0)
            {
              printf("[LED] ERROR: GPIOC_WRITE(0) failed on GPIO%d: %d\n",
                     led_pins[j], errno);
            }
          else
            {
              printf("[LED] LED ON (GPIO%d=LOW)\n", led_pins[j]);
            }
        }

      up_mdelay(500);

      /* Turn all LEDs OFF */

      for (j = 0; j < LED_COUNT; j++)
        {
          ret = ioctl(fds[j], GPIOC_WRITE, 1);
          if (ret < 0)
            {
              printf("[LED] ERROR: GPIOC_WRITE(1) failed on GPIO%d: %d\n",
                     led_pins[j], errno);
            }
          else
            {
              printf("[LED] LED OFF (GPIO%d=HIGH)\n", led_pins[j]);
            }
        }

      up_mdelay(500);
    }

  /* Leave all LEDs OFF */

  for (j = 0; j < LED_COUNT; j++)
    {
      ret = ioctl(fds[j], GPIOC_WRITE, 1);
      if (ret < 0)
        {
          printf("[LED] ERROR: GPIOC_WRITE(1) final failed on GPIO%d: %d\n",
                 led_pins[j], errno);
        }
      else
        {
          printf("[LED] LED left OFF (GPIO%d=HIGH)\n", led_pins[j]);
        }
    }

cleanup:
  for (i = 0; i < ok; i++)
    {
      close(fds[i]);
    }

  printf("[LED] Done.\n\n");
  return ret < 0 ? ret : OK;
}
