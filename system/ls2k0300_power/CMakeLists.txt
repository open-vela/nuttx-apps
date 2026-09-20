# ##############################################################################
# apps/system/ls2k0300_power/CMakeLists.txt
#
# SPDX-License-Identifier: Apache-2.0
#
# Licensed to the Apache Software Foundation (ASF) under one or more contributor
# license agreements.  See the NOTICE file distributed with this work for
# additional information regarding copyright ownership.  The ASF licenses this
# file to you under the Apache License, Version 2.0 (the "License"); you may not
# use this file except in compliance with the License.  You may obtain a copy of
# the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
# WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.  See the
# License for the specific language governing permissions and limitations under
# the License.
#
# ##############################################################################

if(CONFIG_SYSTEM_LS2K0300_POWER)
  if(CONFIG_SYSTEM_LS2K0300_REBOOT)
    nuttx_add_application(
      MODULE
      ${CONFIG_SYSTEM_LS2K0300_REBOOT}
      NAME
      ${CONFIG_SYSTEM_LS2K0300_REBOOT_PROGNAME}
      SRCS
      ls2k0300_reboot_main.c
      ls2k0300_power_common.c
      STACKSIZE
      ${CONFIG_SYSTEM_LS2K0300_REBOOT_STACKSIZE}
      PRIORITY
      ${CONFIG_SYSTEM_LS2K0300_REBOOT_PRIORITY}
      INCLUDE_DIRECTORIES
      ${NUTTX_DIR}/arch/loongarch/src/ls2k0300/hardware)
  endif()

  if(CONFIG_SYSTEM_LS2K0300_POWEROFF)
    nuttx_add_application(
      MODULE
      ${CONFIG_SYSTEM_LS2K0300_POWEROFF}
      NAME
      ${CONFIG_SYSTEM_LS2K0300_POWEROFF_PROGNAME}
      SRCS
      ls2k0300_poweroff_main.c
      ls2k0300_power_common.c
      STACKSIZE
      ${CONFIG_SYSTEM_LS2K0300_POWEROFF_STACKSIZE}
      PRIORITY
      ${CONFIG_SYSTEM_LS2K0300_POWEROFF_PRIORITY}
      INCLUDE_DIRECTORIES
      ${NUTTX_DIR}/arch/loongarch/src/ls2k0300/hardware)
  endif()
endif()
