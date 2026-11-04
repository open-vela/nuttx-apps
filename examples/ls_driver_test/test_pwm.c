/****************************************************************************
 * apps/examples/ls_driver_test/test_pwm.c
 *
 * Hardware PWM breathing LED test, ported from HarmonyOS pwm_demo.
 *
 * Function:
 *   - PWM2 on GPIO88 (blue LED, second function)
 *   - Breathing LED effect: duty cycle ramps 0%→100%→0%
 *
 * Hardware:
 *   - GPIO88 configured as SECOND_FUNC (function=2) for PWM2 output
 *   - Blue LED (active-high) on GPIO88
 *
 ****************************************************************************/

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>

#include <sys/ioctl.h>
#include <stdio.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>

#include <nuttx/arch.h>
#include <nuttx/timers/pwm.h>
#include <nuttx/pinctrl/pinctrl.h>

#include "ls_driver_test.h"

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#define PWM_DEVICE          "/dev/pwm2"
#define PWM_GPIO            88
#define PWM_FUNCTION        2       /* SECOND_FUNC for PWM output */
#define PWM_FREQUENCY       1000    /* 1kHz */

/* Breathing parameters */

#define BREATH_STEPS        100     /* duty steps per ramp */
#define BREATH_DELAY_MS     30      /* ms per step */
#define BREATH_CYCLES       3       /* number of full breath cycles */

/* NuttX PWM duty range: 0 ~ 65536 */

#define PWM_DUTY_MAX        65536

/****************************************************************************
 * Private Functions
 ****************************************************************************/

static int pwm_pinctrl_setup(void)
{
  struct pinctrl_param_s param;
  int fd;
  int ret;

  printf("[PWM] Setting GPIO%d to function %d (PWM2)...\n",
         PWM_GPIO, PWM_FUNCTION);

  fd = open("/dev/pinctrl0", O_RDWR);
  if (fd < 0)
    {
      printf("[PWM] ERROR: open /dev/pinctrl0 failed: %d\n", errno);
      return -errno;
    }

  param.pin = PWM_GPIO;
  param.para.function = PWM_FUNCTION;
  ret = ioctl(fd, PINCTRLC_SETFUNCTION, (unsigned long)&param);
  if (ret < 0)
    {
      printf("[PWM] ERROR: PINCTRLC_SETFUNCTION GPIO%d func=%d failed: %d\n",
             PWM_GPIO, PWM_FUNCTION, errno);
      close(fd);
      return -errno;
    }

  printf("[PWM] GPIO%d set to function %d OK\n", PWM_GPIO, PWM_FUNCTION);
  close(fd);
  return OK;
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

int test_pwm(void)
{
  int fd;
  int ret;
  struct pwm_info_s info;
  int cycle;
  int step;
  uint32_t duty;

  printf("[PWM] === Hardware PWM Breathing LED Test ===\n");
  printf("[PWM] Device: %s (GPIO%d, second function)\n",
         PWM_DEVICE, PWM_GPIO);
  printf("[PWM] Frequency: %dHz\n", PWM_FREQUENCY);
  printf("[PWM] Breathing: %d steps, %dms/step, %d cycles\n\n",
         BREATH_STEPS, BREATH_DELAY_MS, BREATH_CYCLES);

  /* Configure GPIO88 pinmux to PWM function (MUST do this first!) */

  ret = pwm_pinctrl_setup();
  if (ret < 0)
    {
      printf("[PWM] ERROR: pinctrl setup failed\n");
      return ret;
    }

  /* Open PWM device */

  fd = open(PWM_DEVICE, O_RDONLY);
  if (fd < 0)
    {
      printf("[PWM] ERROR: open %s failed: %d\n", PWM_DEVICE, errno);
      return -errno;
    }

  printf("[PWM] %s opened, fd=%d\n", PWM_DEVICE, fd);

  /* Configure PWM */

  info.frequency = PWM_FREQUENCY;
  info.duty      = 0;

  ret = ioctl(fd, PWMIOC_SETCHARACTERISTICS,
              (unsigned long)(uintptr_t)&info);
  if (ret < 0)
    {
      printf("[PWM] ERROR: PWMIOC_SETCHARACTERISTICS failed: %d\n", errno);
      close(fd);
      return -errno;
    }

  /* Start PWM */

  ret = ioctl(fd, PWMIOC_START, 0);
  if (ret < 0)
    {
      printf("[PWM] ERROR: PWMIOC_START failed: %d\n", errno);
      close(fd);
      return -errno;
    }

  printf("[PWM] PWM started, breathing...\n\n");

  /* Breathing loop */

  for (cycle = 0; cycle < BREATH_CYCLES; cycle++)
    {
      printf("[PWM] Cycle %d/%d\n", cycle + 1, BREATH_CYCLES);

      /* Ramp up: 0% → 100% */

      for (step = 0; step <= BREATH_STEPS; step++)
        {
          duty = (uint32_t)step * PWM_DUTY_MAX / BREATH_STEPS;
          info.frequency = PWM_FREQUENCY;
          info.duty      = duty;

          ret = ioctl(fd, PWMIOC_SETCHARACTERISTICS,
                      (unsigned long)(uintptr_t)&info);
          if (ret < 0)
            {
              printf("[PWM] ERROR: set duty failed: %d\n", errno);
              break;
            }

          up_mdelay(BREATH_DELAY_MS);
        }

      /* Ramp down: 100% → 0% */

      for (step = BREATH_STEPS; step >= 0; step--)
        {
          duty = (uint32_t)step * PWM_DUTY_MAX / BREATH_STEPS;
          info.frequency = PWM_FREQUENCY;
          info.duty      = duty;

          ret = ioctl(fd, PWMIOC_SETCHARACTERISTICS,
                      (unsigned long)(uintptr_t)&info);
          if (ret < 0)
            {
              printf("[PWM] ERROR: set duty failed: %d\n", errno);
              break;
            }

          up_mdelay(BREATH_DELAY_MS);
        }
    }

  /* Stop PWM */

  info.frequency = PWM_FREQUENCY;
  info.duty      = 0;
  ioctl(fd, PWMIOC_SETCHARACTERISTICS, (unsigned long)(uintptr_t)&info);
  ioctl(fd, PWMIOC_STOP, 0);

  printf("\n[PWM] Breathing complete, PWM stopped.\n");
  close(fd);

  printf("[PWM] Done.\n");
  return OK;
}
