# ##############################################################################
# apps/examples/ls_driver_test/CMakeLists.txt
#
# SPDX-License-Identifier: Apache-2.0
#
# ##############################################################################

if(CONFIG_EXAMPLES_LS_DRIVER_TEST)
  nuttx_add_application(
    NAME
    ls_driver_test
    PRIORITY
    ${CONFIG_EXAMPLES_LS_DRIVER_TEST_PRIORITY}
    STACKSIZE
    ${CONFIG_EXAMPLES_LS_DRIVER_TEST_STACKSIZE}
    MODULE
    ${CONFIG_EXAMPLES_LS_DRIVER_TEST}
    SRCS
    ls_driver_test.c
    test_common.c
    test_led.c
    test_oled.c
    test_key.c
    test_thermal.c
    test_pwm.c
    test_watchdog.c
    test_rtc.c
    test_adc.c
    test_sensor_oled.c
    test_eeprom.c
    test_buzzer.c
    test_spiflash.c
    test_uart2.c)
endif()
