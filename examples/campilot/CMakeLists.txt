# ##############################################################################
# apps/examples/campilot/CMakeLists.txt
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

if(CONFIG_EXAMPLES_CAMPILOT)
  set(CAMPILOT_SRCS campilot_main.c)
  set(CAMPILOT_DEPENDS mimo_client)

  if(CONFIG_EXAMPLES_CAMPILOT_UI)
    list(APPEND CAMPILOT_SRCS ui_state_machine.c ui_lvgl.c)
    list(APPEND CAMPILOT_DEPENDS lvgl)
  endif()

  if(CONFIG_EXAMPLES_CAMPILOT_FEISHU)
    list(APPEND CAMPILOT_DEPENDS feishu_notify)
  endif()

  nuttx_add_application(
    NAME ${CONFIG_EXAMPLES_CAMPILOT_PROGNAME}
    PRIORITY ${CONFIG_EXAMPLES_CAMPILOT_PRIORITY}
    STACKSIZE ${CONFIG_EXAMPLES_CAMPILOT_STACKSIZE}
    MODULE ${CONFIG_EXAMPLES_CAMPILOT}
    SRCS ${CAMPILOT_SRCS}
    DEPENDS ${CAMPILOT_DEPENDS}
  )
endif()
