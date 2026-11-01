/****************************************************************************
 * apps/examples/ls_driver_test/lcd_sensors.h
 *
 * Sensor helpers the LCD status panel samples.  Declared in their own
 * header rather than in ls_driver_test.h on purpose: test_eeprom.c and
 * test_spiflash.c carry their own private static copies of the BH1750
 * code, and a shared declaration would collide with those.
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Licensed to the Apache Software Foundation (ASF) under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.
 * The ASF licenses this file to you under the Apache License, Version 2.0
 * (the "License"); you may not use this file except in compliance with the
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

#ifndef __APPS_EXAMPLES_LS_DRIVER_TEST_LCD_SENSORS_H
#define __APPS_EXAMPLES_LS_DRIVER_TEST_LCD_SENSORS_H

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>

#include <stdint.h>

/****************************************************************************
 * Public Function Prototypes
 ****************************************************************************/

/* MCP3204 over hardware SPI2 (test_adc.c) */

int  mcp3204_init(void);
void mcp3204_deinit(void);
int  mcp3204_read_average(uint8_t channel, uint16_t *raw);
uint32_t mcp3204_raw_to_mv(uint16_t raw);

/* BH1750 on I2C1 (test_sensor_oled.c) */

int  sensor_bus_open(void);
void sensor_bus_close(void);
int  bh1750_init(void);
int  bh1750_read_lux(unsigned int *lux_x100);

#endif /* __APPS_EXAMPLES_LS_DRIVER_TEST_LCD_SENSORS_H */
