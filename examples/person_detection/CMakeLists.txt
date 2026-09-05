# ##############################################################################
# apps/examples/person_detection/CMakeLists.txt
#
# Licensed to the Apache Software Foundation (ASF) under one or more
# contributor license agreements.  See the NOTICE file distributed with
# this work for additional information regarding copyright ownership.  The
# ASF licenses this file to you under the Apache License, Version 2.0 (the
# "License"); you may not use this file except in compliance with the
# License.  You may obtain a copy of the License at
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

if(CONFIG_EXAMPLES_PERSON_DETECTION)

  # TFLite Micro source directory
  set(TFLM_DIR ${CMAKE_CURRENT_LIST_DIR}/../../mlearning/tflite-micro/tflite-micro)

  # Compiler flags
  set(COMMON_FLAGS
      -fno-lto
      -fno-exceptions
      -DTF_LITE_STATIC_MEMORY
      -DTF_LITE_DISABLE_X86_NEON
      -Wno-sign-compare
      -Wno-unused-variable
      -Wno-undef
      -Wno-shadow
      -DTFLITE_WITH_STABLE_ABI=0
      -DTFLITE_USE_OPAQUE_DELEGATE=0
      -DTFLITE_SINGLE_ROUNDING=0
      -O3)

  if(NOT CONFIG_TFLITEMICRO_DEBUG)
    list(APPEND COMMON_FLAGS -DTF_LITE_STRIP_ERROR_STRINGS)
  endif()

  # TFLite Micro framework sources
  file(
    GLOB
    TFLITE_MICRO_SRCS
    ${TFLM_DIR}/tensorflow/lite/micro/kernels/*.cc
    ${TFLM_DIR}/tensorflow/lite/c/*.cc
    ${TFLM_DIR}/tensorflow/lite/schema/*.cc
    ${TFLM_DIR}/tensorflow/lite/core/c/*.cc
    ${TFLM_DIR}/tensorflow/lite/kernels/*.cc
    ${TFLM_DIR}/tensorflow/lite/kernels/internal/optimized/*.cc
    ${TFLM_DIR}/tensorflow/lite/kernels/internal/reference/*.cc
    ${TFLM_DIR}/tensorflow/lite/kernels/internal/*.cc
    ${TFLM_DIR}/tensorflow/lite/core/api/*.cc
    ${TFLM_DIR}/tensorflow/lite/micro/arena_allocator/*.cc
    ${TFLM_DIR}/tensorflow/lite/micro/memory_planner/*.cc
    ${TFLM_DIR}/tensorflow/lite/micro/*.cc
    ${TFLM_DIR}/tensorflow/lite/micro/tflite_bridge/*.cc)

  # Remove test files
  list(FILTER TFLITE_MICRO_SRCS EXCLUDE REGEX ".*test.cc")
  list(FILTER TFLITE_MICRO_SRCS EXCLUDE REGEX ".*test_common.cc")

  # Include directories
  set(INCDIR
      ${CMAKE_BINARY_DIR}/apps/math/gemmlowp/gemmlowp
      ${CMAKE_BINARY_DIR}/apps/math/ruy/ruy
      ${CMAKE_BINARY_DIR}/apps/math/kissfft/kissfft
      ${CMAKE_BINARY_DIR}/apps/math/tflite-micro/tflite-micro
      ${TFLM_DIR}
      ${CMAKE_CURRENT_LIST_DIR}/../../math/gemmlowp/gemmlowp
      ${CMAKE_CURRENT_LIST_DIR}/../../math/ruy/ruy
      ${CMAKE_CURRENT_LIST_DIR}/../../system/flatbuffers/flatbuffers/include
      ${CMAKE_CURRENT_LIST_DIR}/../../examples/uvc_test)

  # Create the application
  nuttx_add_application(
    NAME
    ${CONFIG_EXAMPLES_PERSON_DETECTION_PROGNAME}
    STACKSIZE
    ${CONFIG_EXAMPLES_PERSON_DETECTION_STACKSIZE}
    PRIORITY
    ${CONFIG_EXAMPLES_PERSON_DETECTION_PRIORITY}
    MODULE
    ${CONFIG_EXAMPLES_PERSON_DETECTION}
    SRCS
    person_detection_main.cc
    ${CMAKE_CURRENT_LIST_DIR}/../../examples/uvc_test/tjpgd.c
    ${TFLITE_MICRO_SRCS})

  # Set compile options for the application target
  set(TARGET_NAME apps_${CONFIG_EXAMPLES_PERSON_DETECTION_PROGNAME})
  target_include_directories(${TARGET_NAME} PRIVATE ${INCDIR})
  target_compile_options(${TARGET_NAME} PRIVATE ${COMMON_FLAGS})

endif()
