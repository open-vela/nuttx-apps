/****************************************************************************
 * apps/examples/ls_driver_test/ls_driver_test.h
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

#ifndef __APPS_EXAMPLES_LS_DRIVER_TEST_H
#define __APPS_EXAMPLES_LS_DRIVER_TEST_H

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#define LED_GPIO_PIN     72
#define LED_GPIO_PIN2    73
#define KEY_GPIO_PIN     33

/****************************************************************************
 * Public Function Prototypes
 ****************************************************************************/

int pinctrl_set_gpio_function(int pin);
int test_led(void);
int test_oled(void);
int test_key(void);
int test_thermal(void);
int test_pwm(void);
int test_watchdog(void);
int test_rtc(void);
int test_adc(void);
int test_sensor_oled(void);

#endif /* __APPS_EXAMPLES_LS_DRIVER_TEST_H */
