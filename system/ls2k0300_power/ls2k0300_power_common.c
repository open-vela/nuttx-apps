/****************************************************************************
 * apps/system/ls2k0300_power/ls2k0300_power_common.c
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
#include <time.h>
#include <syslog.h>
#include <unistd.h>

#include "ls2k0300_power.h"

/****************************************************************************
 * Private Functions
 ****************************************************************************/

/****************************************************************************
 * Name: get_current_timestamp
 *
 * Description:
 *   Get current timestamp string for logging
 *
 ****************************************************************************/

static void get_current_timestamp(FAR char *buffer, size_t size)
{
  time_t now;
  struct tm ts;

  now = time(NULL);
  localtime_r(&now, &ts);
  strftime(buffer, size, "%Y-%m-%d %H:%M:%S", &ts);
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: ls2k0300_power_log_command_execution
 *
 * Description:
 *   Log command execution with timestamp, user and result
 *
 ****************************************************************************/

void ls2k0300_power_log_command_execution(FAR const char *cmd,
                                          FAR const char *result,
                                          int error_code)
{
  char timestamp[64];
  uid_t uid;

  get_current_timestamp(timestamp, sizeof(timestamp));
  uid = getuid();

  if (error_code == 0)
    {
      syslog(LOG_INFO, "[%s] CMD: %s, UID: %d, RESULT: %s\n",
             timestamp, cmd, uid, result);
    }
  else
    {
      syslog(LOG_ERR, "[%s] CMD: %s, UID: %d, RESULT: %s, ERR: %d\n",
             timestamp, cmd, uid, result, error_code);
    }
}

/****************************************************************************
 * Name: ls2k0300_power_check_root_permission
 *
 * Description:
 *   Check if current user has root permission
 *
 ****************************************************************************/

int ls2k0300_power_check_root_permission(FAR const char *cmd)
{
  uid_t uid;

  uid = getuid();
  if (uid != 0)
    {
      printf("%s: Permission denied. Root access required.\n", cmd);
      ls2k0300_power_log_command_execution(cmd, "PERMISSION_DENIED", -EPERM);
      return -EPERM;
    }

  return OK;
}
