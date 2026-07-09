# ##############################################################################
# apps/graphics/lvgl/CMakeLists.txt
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

if(CONFIG_GRAPHICS_LVGL)

  # ############################################################################
  # Config and Fetch lvgl
  # ############################################################################
  set(LVGL_DIR ${CMAKE_CURRENT_LIST_DIR}/lvgl)

  if(NOT EXISTS ${LVGL_DIR})
    FetchContent_Declare(
      lvgl_fetch
      DOWNLOAD_DIR ${CMAKE_CURRENT_LIST_DIR}
      URL "https://github.com/lvgl/lvgl/archive/refs/tags/v9.2.1.zip"
          SOURCE_DIR
          ${CMAKE_CURRENT_LIST_DIR}/lvgl
          BINARY_DIR
          ${CMAKE_BINARY_DIR}/apps/graphics/lvgl/lvgl
          CONFIGURE_COMMAND
          ""
          BUILD_COMMAND
          ""
          INSTALL_COMMAND
          ""
          TEST_COMMAND
          ""
      DOWNLOAD_NO_PROGRESS true
      TIMEOUT 120)

    FetchContent_GetProperties(lvgl_fetch)

    if(NOT lvgl_fetch_POPULATED)
      FetchContent_Populate(lvgl_fetch)
    endif()
  endif()

  # ############################################################################
  # Flags and Sources
  # ############################################################################

  # ############################################################################
  # Library Configuration
  # ############################################################################

  file(
    GLOB_RECURSE
    SRCS
    "${LVGL_DIR}/src/*.S"
    "${LVGL_DIR}/src/*.c"
    "${LVGL_DIR}/src/*.cpp"
    "${LVGL_DIR}/demos/*.c"
    "${LVGL_DIR}/examples/*.c")

  # GLOB_RECURSE above blindly pulls every source file under src/, including
  # hardware-accelerated draw backends and arch-specific asm that only build on
  # specific platforms. Filter them out when the corresponding Kconfig option is
  # not set, so unrelated targets don't try to compile them.

  # blend/neon/*.S is ARMv7 (AArch32) inline asm and only assembles under an ARM
  # 32-bit / M-profile / R-profile toolchain.
  if(NOT CONFIG_ARCH_ARMV7A
     AND NOT CONFIG_ARCH_ARMV7M
     AND NOT CONFIG_ARCH_ARMV8M
     AND NOT CONFIG_ARCH_ARMV7R
     AND NOT CONFIG_ARCH_ARMV8R)
    list(FILTER SRCS EXCLUDE REGEX "/blend/neon/.*\\.S$")
  endif()

  # Allwinner G2D backend; needs g2d_driver.h and hal_cache.h from the Allwinner
  # BSP, absent on non-Allwinner targets.
  if(NOT CONFIG_LV_USE_DRAW_G2D)
    list(FILTER SRCS EXCLUDE REGEX "/draw/sunxi_g2d/.*\\.(c|cpp)$")
  endif()

  # VeriSilicon VG-Lite backend.
  if(NOT CONFIG_LV_USE_DRAW_VG_LITE)
    list(FILTER SRCS EXCLUDE REGEX "/draw/vg_lite/.*\\.(c|cpp)$")
    list(FILTER SRCS EXCLUDE REGEX
         "/draw/vg_lite_vector_optimize/.*\\.(c|cpp)$")
  endif()

  # NXP i.MX RT VG-Lite / PXP backends.
  if(NOT CONFIG_LV_USE_DRAW_VGLITE)
    list(FILTER SRCS EXCLUDE REGEX "/draw/nxp/vglite/.*\\.(c|cpp)$")
    list(FILTER SRCS EXCLUDE REGEX "/draw/nxp/pxp/.*\\.(c|cpp)$")
  endif()

  nuttx_add_library(lvgl)

  target_sources(lvgl PRIVATE ${SRCS})
  target_include_directories(lvgl PRIVATE ${LVGL_DIR})
  target_compile_options(lvgl PRIVATE -Wno-shadow)

  if(CONFIG_LV_USE_LIBPNG)
    target_include_directories(
      lvgl PRIVATE ${NUTTX_APPS_DIR}/external/libpng
                   ${NUTTX_APPS_DIR}/external/libpng/libpng)
  endif()

  if(CONFIG_LV_USE_LIBWEBP)
    target_include_directories(
      lvgl PRIVATE ${NUTTX_APPS_DIR}/external/libwebp/libwebp/src)
  endif()

  if(CONFIG_LV_USE_LIBJPEG_TURBO)
    target_include_directories(
      lvgl PRIVATE ${NUTTX_APPS_DIR}/external/libjpeg-turbo/libjpeg-turbo)
  endif()

  if(CONFIG_LV_USE_FFMPEG)
    target_include_directories(lvgl
                               PRIVATE ${NUTTX_APPS_DIR}/external/ffmpeg/ffmpeg)
  endif()

  if(CONFIG_LV_USE_RLOTTIE)
    target_include_directories(
      lvgl PRIVATE ${NUTTX_APPS_DIR}/external/rlottie/rlottie/inc)
  endif()

  if(CONFIG_LVX_USE_QRSCAN)
    target_include_directories(
      lvgl PRIVATE ${NUTTX_APPS_DIR}/external/quirc/quirc/lib)
  endif()

  if(CONFIG_LV_GPU_G2D_INCLUDE_PATH)
    target_include_directories(
      lvgl PRIVATE ${NUTTX_APPS_DIR}/../${CONFIG_LV_GPU_G2D_INCLUDE_PATH})
  endif()

  if(CONFIG_LV_USE_DRAW_VG_LITE)
    target_include_directories(
      lvgl PRIVATE ${NUTTX_APPS_DIR}/../${CONFIG_LV_DRAW_VG_LITE_INCLUDE})
  endif()

  if(NOT CONFIG_LV_ASSERT_HANDLER_INCLUDE STREQUAL "")
    target_compile_definitions(lvgl PRIVATE "LV_ASSERT_HANDLER=ASSERT(0)\;")
  endif()

  if(CONFIG_LV_USE_PERF_MONITOR AND CONFIG_SCHED_CPULOAD)
    target_compile_definitions(lvgl
                               PRIVATE LV_SYSMON_GET_IDLE=lv_nuttx_get_idle)
  endif()

  if(CONFIG_SCHED_INSTRUMENTATION
     AND CONFIG_LV_USE_PROFILER
     AND NOT CONFIG_LV_USE_PROFILER_BUILTIN)
    target_compile_definitions(
      lvgl
      PRIVATE
        "LV_PROFILER_BEGIN=sched_note_beginex(NOTE_TAG_GRAPHICS, __func__)")
    target_compile_definitions(
      lvgl
      PRIVATE "LV_PROFILER_END=sched_note_endex(NOTE_TAG_GRAPHICS, __func__)")

    # ##########################################################################
    set(LVGL_PROFILER_TAG_DEF
        [=[
#define LV_PROFILER_BEGIN_TAG(str) sched_note_beginex(NOTE_TAG_GRAPHICS, str)
#define LV_PROFILER_END_TAG(str) sched_note_endex(NOTE_TAG_GRAPHICS, str)
      ]=])

    file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/def_lvgl_profiler.h
         "${LVGL_PROFILER_TAG_DEF}")
    target_compile_options(
      lvgl PRIVATE -include "${CMAKE_CURRENT_BINARY_DIR}/def_lvgl_profiler.h")
    # ##########################################################################

  endif()

  if(NOT ${CONFIG_LV_OPTLEVEL} STREQUAL "")
    target_compile_options(lvgl PRIVATE ${CONFIG_LV_OPTLEVEL})
  endif()

  # this macro should be visiable to LVGL and all libraries that use LVGL
  target_compile_options(
    lvgl PUBLIC "-DLV_ATTRIBUTE_MEM_ALIGN=aligned_data(LV_DRAW_BUF_ALIGN)")

  set_property(
    TARGET nuttx
    APPEND
    PROPERTY NUTTX_INCLUDE_DIRECTORIES ${CMAKE_CURRENT_LIST_DIR}
             ${CMAKE_CURRENT_LIST_DIR}/lvgl)

  add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/lvgl/tests/cmake_nuttx)
endif()
