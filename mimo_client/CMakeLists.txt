# ##############################################################################
# apps/mimo_client/CMakeLists.txt
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

if(CONFIG_MIMO_CLIENT)
  set(INCDIR ${CMAKE_CURRENT_SOURCE_DIR}/include)

  set_property(TARGET nuttx APPEND
    PROPERTY NUTTX_INCLUDE_DIRECTORIES ${INCDIR})
  set_property(TARGET nuttx APPEND
    PROPERTY NUTTX_CXX_INCLUDE_DIRECTORIES ${INCDIR})

  nuttx_add_kernel_library(mimo_client SPLIT)
  target_sources(mimo_client PRIVATE
    mimo_client.c
    mimo_http.c
    mimo_modality.c
    mimo_text.c
    mimo_vision.c
    mimo_tts.c
    mimo_asr.c
  )
  target_compile_options(mimo_client PRIVATE -Wno-error=undef)
  include_directories(${INCDIR})
endif()
