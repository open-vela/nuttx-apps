# ##############################################################################
# apps/testing/drivers/drivertest/CMakeLists.txt
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

if(CONFIG_TESTING_DRIVER_TEST)
  if(CONFIG_TESTING_DRIVER_TEST_SIMPLE)
    nuttx_add_application(
      NAME
      cmocka_driver_simple
      PRIORITY
      ${CONFIG_TESTING_DRIVER_TEST_PRIORITY}
      STACKSIZE
      ${CONFIG_TESTING_DRIVER_TEST_STACKSIZE}
      MODULE
      ${CONFIG_TESTING_DRIVER_TEST}
      DEPENDS
      cmocka
      SRCS
      drivertest_simple.c)
  endif()

  if(CONFIG_BCH)
    nuttx_add_application(
      NAME
      cmocka_driver_block
      PRIORITY
      ${CONFIG_TESTING_DRIVER_TEST_PRIORITY}
      STACKSIZE
      ${CONFIG_TESTING_DRIVER_TEST_STACKSIZE}
      MODULE
      ${CONFIG_TESTING_DRIVER_TEST}
      INCLUDE_DIRECTORIES
      ${NUTTX_DIR}/fs
      DEPENDS
      cmocka
      SRCS
      drivertest_block.c)
  endif()

  if(CONFIG_RTC AND CONFIG_SIG_EVTHREAD)
    nuttx_add_application(
      NAME
      cmocka_driver_rtc
      PRIORITY
      ${CONFIG_TESTING_DRIVER_TEST_PRIORITY}
      STACKSIZE
      ${CONFIG_TESTING_DRIVER_TEST_STACKSIZE}
      MODULE
      ${CONFIG_TESTING_DRIVER_TEST}
      DEPENDS
      cmocka
      SRCS
      drivertest_rtc.c)
  endif()

  if(CONFIG_TIMER)
    nuttx_add_application(
      NAME
      cmocka_driver_timer
      PRIORITY
      ${CONFIG_TESTING_DRIVER_TEST_PRIORITY}
      STACKSIZE
      ${CONFIG_TESTING_DRIVER_TEST_STACKSIZE}
      MODULE
      ${CONFIG_TESTING_DRIVER_TEST}
      DEPENDS
      cmocka
      SRCS
      drivertest_timer.c)
  endif()

  if(CONFIG_ONESHOT)
    nuttx_add_application(
      NAME
      cmocka_driver_oneshot
      PRIORITY
      ${CONFIG_TESTING_DRIVER_TEST_PRIORITY}
      STACKSIZE
      ${CONFIG_TESTING_DRIVER_TEST_STACKSIZE}
      MODULE
      ${CONFIG_TESTING_DRIVER_TEST}
      DEPENDS
      cmocka
      SRCS
      drivertest_oneshot.c)
  endif()

  if(NOT CONFIG_DISABLE_POSIX_TIMERS AND CONFIG_SIG_EVTHREAD)
    nuttx_add_application(
      NAME
      cmocka_posix_timer
      PRIORITY
      ${CONFIG_TESTING_DRIVER_TEST_PRIORITY}
      STACKSIZE
      ${CONFIG_TESTING_DRIVER_TEST_STACKSIZE}
      MODULE
      ${CONFIG_TESTING_DRIVER_TEST}
      DEPENDS
      cmocka
      SRCS
      drivertest_posix_timer.c)
  endif()

  if(CONFIG_WATCHDOG AND CONFIG_BOARDCTL_RESET_CAUSE)
    nuttx_add_application(
      NAME
      cmocka_driver_watchdog
      PRIORITY
      ${CONFIG_TESTING_DRIVER_TEST_PRIORITY}
      STACKSIZE
      ${CONFIG_TESTING_DRIVER_TEST_STACKSIZE}
      MODULE
      ${CONFIG_TESTING_DRIVER_TEST}
      DEPENDS
      cmocka
      SRCS
      drivertest_watchdog.c)
  endif()

  if(CONFIG_PWM)
    nuttx_add_application(
      NAME
      cmocka_driver_pwm
      PRIORITY
      ${CONFIG_TESTING_DRIVER_TEST_PRIORITY}
      STACKSIZE
      ${CONFIG_TESTING_DRIVER_TEST_STACKSIZE}
      MODULE
      ${CONFIG_TESTING_DRIVER_TEST}
      DEPENDS
      cmocka
      SRCS
      drivertest_pwm.c)
  endif()

  if(CONFIG_ADC)
    nuttx_add_application(
      NAME
      cmocka_driver_adc
      PRIORITY
      ${CONFIG_TESTING_DRIVER_TEST_PRIORITY}
      STACKSIZE
      ${CONFIG_TESTING_DRIVER_TEST_STACKSIZE}
      MODULE
      ${CONFIG_TESTING_DRIVER_TEST}
      DEPENDS
      cmocka
      SRCS
      drivertest_adc.c)
  endif()

  if(CONFIG_I2C OR CONFIG_SPI)
    if(CONFIG_SENSORS_BMI160)
      nuttx_add_application(
        NAME
        cmocka_driver_i2c_spi
        PRIORITY
        ${CONFIG_TESTING_DRIVER_TEST_PRIORITY}
        STACKSIZE
        ${CONFIG_TESTING_DRIVER_TEST_STACKSIZE}
        MODULE
        ${CONFIG_TESTING_DRIVER_TEST}
        DEPENDS
        cmocka
        SRCS
        drivertest_i2c_spi.c)
    endif()
  endif()

  if(CONFIG_I2C_DRIVER AND CONFIG_I2C_SLAVE_DRIVER)
    nuttx_add_application(
      NAME
      cmocka_driver_i2c_write
      PRIORITY
      ${CONFIG_TESTING_DRIVER_TEST_PRIORITY}
      STACKSIZE
      ${CONFIG_TESTING_DRIVER_TEST_STACKSIZE}
      MODULE
      ${CONFIG_TESTING_DRIVER_TEST}
      DEPENDS
      cmocka
      SRCS
      drivertest_i2c_write.c)

    nuttx_add_application(
      NAME
      cmocka_driver_i2c_read
      PRIORITY
      ${CONFIG_TESTING_DRIVER_TEST_PRIORITY}
      STACKSIZE
      ${CONFIG_TESTING_DRIVER_TEST_STACKSIZE}
      MODULE
      ${CONFIG_TESTING_DRIVER_TEST}
      DEPENDS
      cmocka
      SRCS
      drivertest_i2c_read.c)
  endif()

  if(CONFIG_I2C_DRIVER)
    nuttx_add_application(
      NAME
      cmocka_driver_i2cdev_master
      PRIORITY
      ${CONFIG_TESTING_DRIVER_TEST_PRIORITY}
      STACKSIZE
      ${CONFIG_TESTING_DRIVER_TEST_STACKSIZE}
      MODULE
      ${CONFIG_TESTING_DRIVER_TEST}
      DEPENDS
      cmocka
      SRCS
      drivertest_i2cdev_master.c)
  endif()

  if(CONFIG_I2C_SLAVE_DRIVER)
    nuttx_add_application(
      NAME
      cmocka_driver_i2cdev_slave
      PRIORITY
      ${CONFIG_TESTING_DRIVER_TEST_PRIORITY}
      STACKSIZE
      ${CONFIG_TESTING_DRIVER_TEST_STACKSIZE}
      MODULE
      ${CONFIG_TESTING_DRIVER_TEST}
      DEPENDS
      cmocka
      SRCS
      drivertest_i2cdev_slave.c)
  endif()

  if(CONFIG_SPI_DRIVER)
    nuttx_add_application(
      NAME
      cmocka_driver_spidev_master
      PRIORITY
      ${CONFIG_TESTING_DRIVER_TEST_PRIORITY}
      STACKSIZE
      ${CONFIG_TESTING_DRIVER_TEST_STACKSIZE}
      MODULE
      ${CONFIG_TESTING_DRIVER_TEST}
      DEPENDS
      cmocka
      SRCS
      drivertest_spidev_master.c)
  endif()

  if(CONFIG_SPI_SLAVE_DRIVER)
    nuttx_add_application(
      NAME
      cmocka_driver_spidev_slave
      PRIORITY
      ${CONFIG_TESTING_DRIVER_TEST_PRIORITY}
      STACKSIZE
      ${CONFIG_TESTING_DRIVER_TEST_STACKSIZE}
      MODULE
      ${CONFIG_TESTING_DRIVER_TEST}
      DEPENDS
      cmocka
      SRCS
      drivertest_spidev_slave.c)
  endif()

  if(CONFIG_DEV_GPIO)
    nuttx_add_application(
      NAME
      cmocka_driver_gpio
      PRIORITY
      ${CONFIG_TESTING_DRIVER_TEST_PRIORITY}
      STACKSIZE
      ${CONFIG_TESTING_DRIVER_TEST_STACKSIZE}
      MODULE
      ${CONFIG_TESTING_DRIVER_TEST}
      DEPENDS
      cmocka
      SRCS
      drivertest_gpio.c)
  endif()

  if(CONFIG_RELAY)
    nuttx_add_application(
      NAME
      cmocka_driver_relay
      PRIORITY
      ${CONFIG_TESTING_DRIVER_TEST_PRIORITY}
      STACKSIZE
      ${CONFIG_TESTING_DRIVER_TEST_STACKSIZE}
      MODULE
      ${CONFIG_TESTING_DRIVER_TEST}
      DEPENDS
      cmocka
      SRCS
      drivertest_relay.c)
  endif()

  if(CONFIG_SERIAL)
    nuttx_add_application(
      NAME
      cmocka_driver_uart
      PRIORITY
      ${CONFIG_TESTING_DRIVER_TEST_PRIORITY}
      STACKSIZE
      ${CONFIG_TESTING_DRIVER_TEST_STACKSIZE}
      MODULE
      ${CONFIG_TESTING_DRIVER_TEST}
      DEPENDS
      cmocka
      SRCS
      drivertest_uart.c)
  endif()

  if(CONFIG_AUDIO)
    nuttx_add_application(
      NAME
      cmocka_driver_audio
      PRIORITY
      ${CONFIG_TESTING_DRIVER_TEST_PRIORITY}
      STACKSIZE
      ${CONFIG_TESTING_DRIVER_TEST_STACKSIZE}
      MODULE
      ${CONFIG_TESTING_DRIVER_TEST}
      DEPENDS
      cmocka
      SRCS
      drivertest_audio.c)
  endif()

  if(CONFIG_DEVFREQ AND NOT CONFIG_CPUFREQ_DEFAULT_GOV_NULL)
    nuttx_add_application(
      NAME
      cmocka_driver_cpufreq
      PRIORITY
      ${CONFIG_TESTING_DRIVER_TEST_PRIORITY}
      STACKSIZE
      ${CONFIG_TESTING_DRIVER_TEST_STACKSIZE}
      MODULE
      ${CONFIG_TESTING_DRIVER_TEST}
      DEPENDS
      cmocka
      SRCS
      drivertest_cpufreq.c)
  endif()

  if(CONFIG_DEVFREQ)
    nuttx_add_application(
      NAME
      cmocka_driver_devfreq
      PRIORITY
      ${CONFIG_TESTING_DRIVER_TEST_PRIORITY}
      STACKSIZE
      ${CONFIG_TESTING_DRIVER_TEST_STACKSIZE}
      MODULE
      ${CONFIG_TESTING_DRIVER_TEST}
      DEPENDS
      cmocka
      SRCS
      drivertest_devfreq.c)
  endif()

  if(CONFIG_VIDEO_FB)
    nuttx_add_application(
      NAME
      cmocka_driver_framebuffer
      PRIORITY
      ${CONFIG_TESTING_DRIVER_TEST_PRIORITY}
      STACKSIZE
      ${CONFIG_TESTING_DRIVER_TEST_STACKSIZE}
      MODULE
      ${CONFIG_TESTING_DRIVER_TEST}
      DEPENDS
      cmocka
      SRCS
      drivertest_framebuffer.c)
  endif()

  if(CONFIG_LCD_DEV)
    nuttx_add_application(
      NAME
      cmocka_driver_lcd
      PRIORITY
      ${CONFIG_TESTING_DRIVER_TEST_PRIORITY}
      STACKSIZE
      ${CONFIG_TESTING_DRIVER_TEST_STACKSIZE}
      MODULE
      ${CONFIG_TESTING_DRIVER_TEST}
      DEPENDS
      cmocka
      SRCS
      drivertest_lcd.c)
  endif()

  if(CONFIG_MTDLOG)
    nuttx_add_application(
      NAME
      cmocka_driver_mtd_log
      PRIORITY
      ${CONFIG_TESTING_DRIVER_TEST_PRIORITY}
      STACKSIZE
      ${CONFIG_TESTING_DRIVER_TEST_STACKSIZE}
      MODULE
      ${CONFIG_TESTING_DRIVER_TEST}
      DEPENDS
      cmocka
      SRCS
      drivertest_mtd_log.c)
  endif()

  if(CONFIG_REGULATOR)
    nuttx_add_application(
      NAME
      cmocka_driver_regulator
      PRIORITY
      ${CONFIG_TESTING_DRIVER_TEST_PRIORITY}
      STACKSIZE
      ${CONFIG_TESTING_DRIVER_TEST_STACKSIZE}
      MODULE
      ${CONFIG_TESTING_DRIVER_TEST}
      DEPENDS
      cmocka
      SRCS
      drivertest_regulator.c)
  endif()

  if(CONFIG_GRAPHICS_LVGL AND CONFIG_LV_USE_NUTTX_TOUCHSCREEN)
    nuttx_add_application(
      NAME
      cmocka_driver_touchpanel
      PRIORITY
      ${CONFIG_TESTING_DRIVER_TEST_PRIORITY}
      STACKSIZE
      ${CONFIG_TESTING_DRIVER_TEST_STACKSIZE}
      MODULE
      ${CONFIG_TESTING_DRIVER_TEST}
      DEPENDS
      cmocka
      SRCS
      drivertest_touchpanel.c)
  endif()

  if(CONFIG_ARCH_CHIP_MPS2_AN500)
    if(CONFIG_ARCH_IRQPRIO)
      nuttx_add_application(
        NAME
        cmocka_driver_mps2
        PRIORITY
        ${CONFIG_TESTING_DRIVER_TEST_PRIORITY}
        STACKSIZE
        ${CONFIG_TESTING_DRIVER_TEST_STACKSIZE}
        MODULE
        ${CONFIG_TESTING_DRIVER_TEST}
        DEPENDS
        cmocka
        SRCS
        drivertest_mps2.c)

      nuttx_add_application(
        NAME
        cmocka_driver_mps2_zerointerrupt
        PRIORITY
        ${CONFIG_TESTING_DRIVER_TEST_PRIORITY}
        STACKSIZE
        ${CONFIG_TESTING_DRIVER_TEST_STACKSIZE}
        MODULE
        ${CONFIG_TESTING_DRIVER_TEST}
        DEPENDS
        cmocka
        SRCS
        drivertest_mps2_zerointerrupt.c)
    endif()

    nuttx_add_application(
      NAME
      cmocka_driver_mps2_isr_signal
      PRIORITY
      ${CONFIG_TESTING_DRIVER_TEST_PRIORITY}
      STACKSIZE
      ${CONFIG_TESTING_DRIVER_TEST_STACKSIZE}
      MODULE
      ${CONFIG_TESTING_DRIVER_TEST}
      DEPENDS
      cmocka
      SRCS
      drivertest_mps2_isr_signal.c)
  endif()

  if(CONFIG_PM)
    nuttx_add_application(
      NAME
      cmocka_driver_pm
      PRIORITY
      ${CONFIG_TESTING_DRIVER_TEST_PRIORITY}
      STACKSIZE
      ${CONFIG_TESTING_DRIVER_TEST_STACKSIZE}
      MODULE
      ${CONFIG_TESTING_DRIVER_TEST}
      DEPENDS
      cmocka
      SRCS
      drivertest_pm.c)

    nuttx_add_application(
      NAME
      cmocka_driver_pm_callback
      PRIORITY
      ${CONFIG_TESTING_DRIVER_TEST_PRIORITY}
      STACKSIZE
      ${CONFIG_TESTING_DRIVER_TEST_STACKSIZE}
      MODULE
      ${CONFIG_TESTING_DRIVER_TEST}
      DEPENDS
      cmocka
      SRCS
      drivertest_pm_callback.c)
  endif()

  if(CONFIG_PM AND CONFIG_PM_GOVERNOR_STABILITY)
    nuttx_add_application(
      NAME
      cmocka_driver_pm_gov_stability
      PRIORITY
      ${CONFIG_TESTING_DRIVER_TEST_PRIORITY}
      STACKSIZE
      ${CONFIG_TESTING_DRIVER_TEST_STACKSIZE}
      MODULE
      ${CONFIG_TESTING_DRIVER_TEST}
      DEPENDS
      cmocka
      SRCS
      drivertest_pm_gov_stability.c)
  endif()

  if(CONFIG_PM AND CONFIG_SMP)
    nuttx_add_application(
      NAME
      cmocka_driver_pm_smp
      PRIORITY
      ${CONFIG_TESTING_DRIVER_TEST_PRIORITY}
      STACKSIZE
      ${CONFIG_TESTING_DRIVER_TEST_STACKSIZE}
      MODULE
      ${CONFIG_TESTING_DRIVER_TEST}
      DEPENDS
      cmocka
      SRCS
      drivertest_pm_smp.c)
  endif()

  if(CONFIG_PM_RUNTIME)
    nuttx_add_application(
      NAME
      cmocka_driver_pm_runtime
      PRIORITY
      ${CONFIG_TESTING_DRIVER_TEST_PRIORITY}
      STACKSIZE
      ${CONFIG_TESTING_DRIVER_TEST_STACKSIZE}
      MODULE
      ${CONFIG_TESTING_DRIVER_TEST}
      DEPENDS
      cmocka
      SRCS
      drivertest_pm_runtime.c)
  endif()

  if(CONFIG_BATTERY_GAUGE)
    nuttx_add_application(
      NAME
      cmocka_driver_battery_gauge
      PRIORITY
      ${CONFIG_TESTING_DRIVER_TEST_PRIORITY}
      STACKSIZE
      ${CONFIG_TESTING_DRIVER_TEST_STACKSIZE}
      MODULE
      ${CONFIG_TESTING_DRIVER_TEST}
      DEPENDS
      cmocka
      SRCS
      drivertest_battery_gauge.c)
  endif()

endif()

nuttx_add_subdirectory()
