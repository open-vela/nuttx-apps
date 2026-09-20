# ##############################################################################
# apps/examples/mimonet/CMakeLists.txt
#
# SPDX-License-Identifier: Apache-2.0
# ##############################################################################

if(CONFIG_EXAMPLES_MIMONET)
  nuttx_add_application(
    MODULE
    ${CONFIG_EXAMPLES_MIMONET}
    NAME
    ${CONFIG_EXAMPLES_MIMONET_PROGNAME}
    SRCS
    mimonet_main.c
    STACKSIZE
    ${CONFIG_EXAMPLES_MIMONET_STACKSIZE}
    PRIORITY
    ${CONFIG_EXAMPLES_MIMONET_PRIORITY})
endif()