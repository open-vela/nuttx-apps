/****************************************************************************
 * apps/system/ls2k0300_power/ls2k0300_reboot_main.c
 *
 * SPDX-License-Identifier: Apache-2.0
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
#include <errno.h>
#include <arch/irq.h>

#include "ls2k0300_power.h"
#include "ls2k0300_memorymap.h"

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: reboot_main
 *
 * Description:
 *   Entry point for reboot command. Writes 1 to register LS2K0300_WDT_BASE
 *   to trigger system reboot.
 *
 ****************************************************************************/

int main(int argc, FAR char *argv[])
{
  volatile uint32_t *reg_addr;
  irqstate_t flags;
  int ret;

  printf("reboot: Initiating system reboot...\n");

  ret = ls2k0300_power_check_root_permission("reboot");
  if (ret < 0)
    {
      return ERROR;
    }

  ls2k0300_power_log_command_execution("reboot", "STARTING", 0);

  flags = up_irq_save();

  reg_addr = (volatile uint32_t *)LS2K0300_WDT_BASE;
  *reg_addr = LS2K0300_REBOOT_VALUE;

  up_irq_restore(flags);

  ls2k0300_power_log_command_execution("reboot", "REGISTER_WRITE_FAILED",
                                       -EIO);
  printf("reboot: Failed to trigger reboot. Register write failed.\n");

  return ERROR;
}
