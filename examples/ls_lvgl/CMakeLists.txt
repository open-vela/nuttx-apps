# ##############################################################################
# apps/examples/ls_lvgl/CMakeLists.txt
#
# SPDX-License-Identifier: Apache-2.0
#
# ##############################################################################

if(CONFIG_EXAMPLES_LS_LVGL)
  nuttx_add_application(
    NAME
    ls_lvgl
    PRIORITY
    ${CONFIG_EXAMPLES_LS_LVGL_PRIORITY}
    STACKSIZE
    ${CONFIG_EXAMPLES_LS_LVGL_STACKSIZE}
    MODULE
    ${CONFIG_EXAMPLES_LS_LVGL}
    DEPENDS
    lvgl
    SRCS
    ls_lvgl.c)
endif()
