# ##############################################################################
# apps/audioutils/alsa-lib/CMakeLists.txt
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

if(CONFIG_AUDIOUTILS_ALSA_LIB)
  target_sources(
    apps
    PRIVATE bits_convert.c
            channels_map.c
            error.c
            pcm_hw.c
            pcm_params.c
            pcm_softvol.c
            pcm.c)

  if(CONFIG_AUDIOUTILS_ALSA_LIB_DEVICE_DMIX)
    target_sources(apps PRIVATE pcm_dmix.c)
  endif()

  set_source_files_properties(pcm_params.c TARGET_DIRECTORY apps
                              PROPERTIES COMPILE_FLAGS "-Wno-array-bounds")
  target_compile_definitions(apps PRIVATE OUTSIDE_SPEEX RANDOM_PREFIX=nuttx)

  if(CONFIG_AUDIOUTILS_ALSA_LIB_APLAY)
    nuttx_add_application(
      NAME
      aplay
      SRCS
      aplay.c
      STACKSIZE
      ${CONFIG_DEFAULT_TASK_STACKSIZE}
      PRIORITY
      SCHED_PRIORITY_DEFAULT)
  endif()

  if(CONFIG_AUDIOUTILS_ALSA_LIB_ARECORD)
    nuttx_add_application(
      NAME
      arecord
      SRCS
      arecord.c
      STACKSIZE
      ${CONFIG_DEFAULT_TASK_STACKSIZE}
      PRIORITY
      SCHED_PRIORITY_DEFAULT)
  endif()

  if(CONFIG_AUDIOUTILS_ALSA_LIB_ALSACTL)
    nuttx_add_application(
      NAME
      alsactl
      SRCS
      alsactl.c
      STACKSIZE
      ${CONFIG_DEFAULT_TASK_STACKSIZE}
      PRIORITY
      SCHED_PRIORITY_DEFAULT)
  endif()

  set_property(
    TARGET nuttx
    APPEND
    PROPERTY NUTTX_INCLUDE_DIRECTORIES ${CMAKE_CURRENT_SOURCE_DIR}/include)
endif()
