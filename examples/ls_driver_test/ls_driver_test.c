/****************************************************************************
 * apps/examples/ls_driver_test/ls_driver_test.c
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
#include <string.h>

#include "ls_driver_test.h"

/****************************************************************************
 * Private Functions
 ****************************************************************************/

static void show_usage(FAR const char *progname)
{
  printf("Usage: %s [test]\n", progname);
  printf("  test options:\n");
  printf("    led       - Test LED (GPIO%d, active-low)\n", LED_GPIO_PIN);
  printf("    oled      - Test SSD1306 OLED (I2C1, addr=0x3C)\n");
  printf("    key       - Test Key (GPIO%d)\n", KEY_GPIO_PIN);
  printf("    thermal   - Test thermal sensor\n");
  printf("    pwm       - PWM2 breathing LED (GPIO88)\n");
  printf("    watchdog  - Test watchdog timer\n");
  printf("    rtc       - Test RTC (real-time clock)\n");
  printf("    adc       - Test ADC (analog-to-digital)\n");
  printf("    sensor    - BH1750+OLED light sensor display\n");
  printf("    eeprom    - EEPROM + BH1750 light sensor test\n");
  printf("    buzzer    - Buzzer (GPIO%d) with KEY control\n", 75);
  printf("    spiflash  - SPI Flash + ADC (KEY1: read/save, KEY2: exit)\n");
  printf("    uart2     - UART2 echo (GPIO44 TX / GPIO45 RX)\n");
  printf("    all       - Run all tests (default)\n");
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

int main(int argc, FAR char *argv[])
{
  printf("=== LS2K0300 Driver Test ===\n\n");

  if (argc > 1)
    {
      if (strcmp(argv[1], "led") == 0)
        {
#if defined(CONFIG_LS2K0300_GPIO) && defined(CONFIG_LS2K0300_PINCTRL)
          test_led();
#else
          printf("GPIO and Pinctrl drivers not enabled\n");
#endif
        }
      else if (strcmp(argv[1], "oled") == 0)
        {
#ifdef CONFIG_I2C_DRIVER
          test_oled();
#else
          printf("I2C driver not enabled\n");
#endif
        }
      else if (strcmp(argv[1], "key") == 0)
        {
#if defined(CONFIG_LS2K0300_GPIO) && defined(CONFIG_LS2K0300_PINCTRL)
          test_key();
#else
          printf("GPIO and Pinctrl drivers not enabled\n");
#endif
        }
      else if (strcmp(argv[1], "thermal") == 0)
        {
#ifdef CONFIG_LS2K0300_THERMAL
          test_thermal();
#else
          printf("Thermal driver not enabled\n");
#endif
        }
      else if (strcmp(argv[1], "pwm") == 0)
        {
#ifdef CONFIG_LS2K0300_PWM
          test_pwm();
#else
          printf("PWM driver not enabled\n");
#endif
        }
      else if (strcmp(argv[1], "watchdog") == 0)
        {
#ifdef CONFIG_LS2K0300_WDT
          test_watchdog();
#else
          printf("Watchdog driver not enabled\n");
#endif
        }
      else if (strcmp(argv[1], "rtc") == 0)
        {
#ifdef CONFIG_LS2K0300_RTC
          test_rtc();
#else
          printf("RTC driver not enabled\n");
#endif
        }
      else if (strcmp(argv[1], "adc") == 0)
        {
#ifdef CONFIG_LS2K0300_ADC
          test_adc();
#else
          printf("ADC driver not enabled\n");
#endif
        }
      else if (strcmp(argv[1], "sensor") == 0)
        {
#ifdef CONFIG_I2C_DRIVER
          test_sensor_oled();
#else
          printf("I2C driver not enabled\n");
#endif
        }
      else if (strcmp(argv[1], "eeprom") == 0)
        {
#if defined(CONFIG_I2C_DRIVER) && defined(CONFIG_LS2K0300_GPIO) && \
    defined(CONFIG_LS2K0300_PINCTRL)
          test_eeprom();
#else
          printf("I2C, GPIO, and Pinctrl drivers not all enabled\n");
#endif
        }
      else if (strcmp(argv[1], "buzzer") == 0)
        {
#if defined(CONFIG_LS2K0300_GPIO) && defined(CONFIG_LS2K0300_PINCTRL)
          test_buzzer();
#else
          printf("GPIO and Pinctrl drivers not enabled\n");
#endif
        }
      else if (strcmp(argv[1], "spiflash") == 0)
        {
#if defined(CONFIG_LS2K0300_SPIIO) && defined(CONFIG_LS2K0300_GPIO) && \
    defined(CONFIG_LS2K0300_PINCTRL)
          test_spiflash();
#else
          printf("SPIIO, GPIO, and Pinctrl drivers not all enabled\n");
#endif
        }
      else if (strcmp(argv[1], "uart2") == 0)
        {
#if defined(CONFIG_LS2K0300_GPIO) && defined(CONFIG_LS2K0300_PINCTRL)
          test_uart2();
#else
          printf("GPIO and Pinctrl drivers not enabled\n");
#endif
        }
      else if (strcmp(argv[1], "-h") == 0 ||
               strcmp(argv[1], "--help") == 0)
        {
          show_usage(argv[0]);
        }
      else
        {
          printf("Unknown test: %s\n", argv[1]);
          show_usage(argv[0]);
        }
    }
  else
    {
#if defined(CONFIG_LS2K0300_GPIO) && defined(CONFIG_LS2K0300_PINCTRL)
      test_led();
      test_key();
#endif

#ifdef CONFIG_I2C_DRIVER
      test_oled();
#endif

#ifdef CONFIG_LS2K0300_THERMAL
      test_thermal();
#endif

#ifdef CONFIG_LS2K0300_PWM
      test_pwm();
#endif

#ifdef CONFIG_LS2K0300_WDT
      test_watchdog();
#endif

#ifdef CONFIG_LS2K0300_RTC
      test_rtc();
#endif

#ifdef CONFIG_LS2K0300_ADC
      test_adc();
#endif
    }

  printf("=== LS2K0300 Driver Test Complete ===\n");
  return 0;
}
