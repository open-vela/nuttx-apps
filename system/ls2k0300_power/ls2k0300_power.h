/****************************************************************************
 * apps/system/ls2k0300_power/ls2k0300_power.h
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

#ifndef __APPS_SYSTEM_LS2K0300_POWER_LS2K0300_POWER_H
#define __APPS_SYSTEM_LS2K0300_POWER_LS2K0300_POWER_H

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>
#include <sys/types.h>

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#define LS2K0300_POWEROFF_OFFSET     0x14
#define LS2K0300_REBOOT_VALUE        1
#define LS2K0300_POWEROFF_VALUE      0x3c00

/****************************************************************************
 * Public Function Prototypes
 ****************************************************************************/

void ls2k0300_power_log_command_execution(FAR const char *cmd,
                                          FAR const char *result,
                                          int error_code);
int ls2k0300_power_check_root_permission(FAR const char *cmd);

#endif /* __APPS_SYSTEM_LS2K0300_POWER_LS2K0300_POWER_H */
