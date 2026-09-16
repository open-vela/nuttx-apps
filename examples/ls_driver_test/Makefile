include $(APPDIR)/Make.defs

PROGNAME  = $(CONFIG_EXAMPLES_LS_DRIVER_TEST_PROGNAME)
PRIORITY  = $(CONFIG_EXAMPLES_LS_DRIVER_TEST_PRIORITY)
STACKSIZE = $(CONFIG_EXAMPLES_LS_DRIVER_TEST_STACKSIZE)
HEAPSIZE  = $(CONFIG_EXAMPLES_LS_DRIVER_TEST_HEAPSIZE)
MODULE    = $(CONFIG_EXAMPLES_LS_DRIVER_TEST)

MAINSRC = ls_driver_test.c

CSRCS = test_common.c test_led.c test_key.c test_thermal.c test_pwm.c \
        test_watchdog.c test_rtc.c test_adc.c

include $(APPDIR)/Application.mk
