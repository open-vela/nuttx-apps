# ##############################################################################
# apps/feishu_notify/CMakeLists.txt
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

if(CONFIG_FEISHU_NOTIFY)
  set(INCDIR ${CMAKE_CURRENT_SOURCE_DIR}/include)

  set_property(TARGET nuttx APPEND
    PROPERTY NUTTX_INCLUDE_DIRECTORIES ${INCDIR})
  set_property(TARGET nuttx APPEND
    PROPERTY NUTTX_CXX_INCLUDE_DIRECTORIES ${INCDIR})

  # Reusable notification library (linked by campilot etc.)
  nuttx_add_kernel_library(feishu_notify SPLIT)
  target_sources(feishu_notify PRIVATE
    feishu_notify.c
    feishu_http.c
  )
  target_compile_options(feishu_notify PRIVATE -Wno-error=undef)
  include_directories(${INCDIR})

  # "feishu" CLI application
  nuttx_add_application(
    NAME ${CONFIG_FEISHU_NOTIFY_PROGNAME}
    PRIORITY ${CONFIG_FEISHU_NOTIFY_PRIORITY}
    STACKSIZE ${CONFIG_FEISHU_NOTIFY_STACKSIZE}
    MODULE ${CONFIG_FEISHU_NOTIFY}
    SRCS feishu_main.c
    DEPENDS feishu_notify
  )
endif()
