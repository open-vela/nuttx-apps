/****************************************************************************
 * apps/examples/ls_driver_test/test_buzzer.c
 *
 * Buzzer test, ported from HarmonyOS loong_hat 03_buzzer demo.
 *
 * Function:
 *   - Passive buzzer on GPIO75, driven by GPIO toggling (square wave)
 *   - KEY1 (GPIO87): trigger a beep
 *   - KEY2 (GPIO86): exit program
 *   - Buzzer auto-beeps every 2 seconds if no key pressed
 *
 ****************************************************************************/

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>

#include <sys/ioctl.h>
#include <stdbool.h>
#include <stdio.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>

#include <nuttx/arch.h>
#include <nuttx/ioexpander/gpio.h>

#include "ls_driver_test.h"

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#define BUZZER_GPIO             75
#define KEY1_GPIO               87
#define KEY2_GPIO               86

/* Tone parameters: 1kHz, half-cycle = 500us */

#define BUZZER_HALF_CYCLE_US    500
#define BEEP_DURATION_MS        100
#define AUTO_BEEP_INTERVAL_MS   2000

#define KEY_SCAN_DELAY_MS       10
#define KEY_DEBOUNCE_MS         20

/****************************************************************************
 * Private Functions - GPIO
 ****************************************************************************/

static int init_buzzer_gpio(int gpio)
{
  int ret;
  char path[32];
  int fd;

  ret = pinctrl_set_gpio_function(gpio);
  if (ret < 0)
    {
      printf("[BUZZER] ERROR: pinctrl failed for GPIO%d\n", gpio);
      return ret;
    }

  snprintf(path, sizeof(path), "/dev/gpio%d", gpio);
  fd = open(path, O_RDWR);
  if (fd < 0)
    {
      printf("[BUZZER] ERROR: open %s failed: %d\n", path, errno);
      return -errno;
    }

  ret = ioctl(fd, GPIOC_SETPINTYPE, (unsigned long)GPIO_OUTPUT_PIN);
  if (ret < 0)
    {
      printf("[BUZZER] ERROR: GPIOC_SETPINTYPE(OUTPUT) on GPIO%d: %d\n",
             gpio, errno);
      close(fd);
      return -errno;
    }

  /* Buzzer off */

  ioctl(fd, GPIOC_WRITE, 0);
  return fd;
}

static int init_key_gpio(int gpio)
{
  int ret;
  char path[32];
  int fd;

  ret = pinctrl_set_gpio_function(gpio);
  if (ret < 0)
    {
      printf("[BUZZER] ERROR: pinctrl failed for GPIO%d\n", gpio);
      return ret;
    }

  snprintf(path, sizeof(path), "/dev/gpio%d", gpio);
  fd = open(path, O_RDWR);
  if (fd < 0)
    {
      printf("[BUZZER] ERROR: open %s failed: %d\n", path, errno);
      return -errno;
    }

  ret = ioctl(fd, GPIOC_SETPINTYPE, (unsigned long)GPIO_INPUT_PIN);
  if (ret < 0)
    {
      printf("[BUZZER] ERROR: GPIOC_SETPINTYPE(INPUT) on GPIO%d: %d\n",
             gpio, errno);
      close(fd);
      return -errno;
    }

  return fd;
}

static bool get_key_value(int fd)
{
  bool value = true;

  ioctl(fd, GPIOC_READ, (unsigned long)(uintptr_t)&value);
  return value;
}

static int is_key_pressed(int fd, bool *last)
{
  bool value = get_key_value(fd);

  /* Detect falling edge: HIGH -> LOW = press */

  if (!value && *last)
    {
      up_mdelay(KEY_DEBOUNCE_MS);
      value = get_key_value(fd);
      if (!value)
        {
          *last = value;
          return 1;
        }
    }

  *last = value;
  return 0;
}

/****************************************************************************
 * Private Functions - Buzzer
 ****************************************************************************/

static void buzzer_beep(int fd, unsigned int duration_ms)
{
  unsigned int loops;
  unsigned int i;

  loops = (duration_ms * 1000u) / (BUZZER_HALF_CYCLE_US * 2u);

  for (i = 0; i < loops; i++)
    {
      ioctl(fd, GPIOC_WRITE, 1);
      up_udelay(BUZZER_HALF_CYCLE_US);

      ioctl(fd, GPIOC_WRITE, 0);
      up_udelay(BUZZER_HALF_CYCLE_US);
    }

  /* Ensure buzzer is off */

  ioctl(fd, GPIOC_WRITE, 0);
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

int test_buzzer(void)
{
  int fd_buzzer;
  int fd_key1;
  int fd_key2;
  bool key1_last;
  bool key2_last;
  unsigned int auto_tick = 0;

  printf("[BUZZER] === Buzzer Test (loong_hat 03_buzzer) ===\n");
  printf("[BUZZER] Buzzer on GPIO%d, %dHz tone\n",
         BUZZER_GPIO, 1000000 / (BUZZER_HALF_CYCLE_US * 2));
  printf("[BUZZER] KEY1(GPIO%d): manual beep\n", KEY1_GPIO);
  printf("[BUZZER] KEY2(GPIO%d): exit\n", KEY2_GPIO);
  printf("[BUZZER] Auto-beep every %dms\n\n", AUTO_BEEP_INTERVAL_MS);

  /* Initialize buzzer GPIO */

  fd_buzzer = init_buzzer_gpio(BUZZER_GPIO);
  if (fd_buzzer < 0)
    {
      return fd_buzzer;
    }

  /* Initialize keys */

  fd_key1 = init_key_gpio(KEY1_GPIO);
  if (fd_key1 < 0)
    {
      close(fd_buzzer);
      return fd_key1;
    }

  fd_key2 = init_key_gpio(KEY2_GPIO);
  if (fd_key2 < 0)
    {
      close(fd_buzzer);
      close(fd_key1);
      return fd_key2;
    }

  key1_last = get_key_value(fd_key1);
  key2_last = get_key_value(fd_key2);

  printf("[BUZZER] Ready. KEY1=beep, KEY2=exit\n\n");

  /* Initial beep to confirm buzzer works */

  printf("[BUZZER] Initial beep...\n");
  buzzer_beep(fd_buzzer, BEEP_DURATION_MS);

  /* Main loop */

  while (1)
    {
      /* Check KEY1 - manual beep */

      if (is_key_pressed(fd_key1, &key1_last))
        {
          printf("[BUZZER] KEY1 pressed, beep!\n");
          buzzer_beep(fd_buzzer, BEEP_DURATION_MS);
          auto_tick = 0;
        }

      /* Check KEY2 - exit */

      if (is_key_pressed(fd_key2, &key2_last))
        {
          printf("\n[BUZZER] KEY2 pressed, exiting...\n");
          break;
        }

      /* Auto beep at interval */

      auto_tick += KEY_SCAN_DELAY_MS;
      if (auto_tick >= AUTO_BEEP_INTERVAL_MS)
        {
          auto_tick = 0;
          printf("[BUZZER] auto beep\n");
          buzzer_beep(fd_buzzer, BEEP_DURATION_MS);
        }

      up_mdelay(KEY_SCAN_DELAY_MS);
    }

  /* Cleanup - ensure buzzer off */

  ioctl(fd_buzzer, GPIOC_WRITE, 0);

  close(fd_key2);
  close(fd_key1);
  close(fd_buzzer);

  printf("[BUZZER] Test complete.\n");
  return OK;
}
