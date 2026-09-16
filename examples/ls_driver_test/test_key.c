/****************************************************************************
 * apps/examples/ls_driver_test/test_key.c
 *
 * Key + LED test, ported from HarmonyOS loong_hat 02_KEY demo.
 *
 * Function:
 *   - KEY1 (GPIO87) toggles red LED   (GPIO72)
 *   - KEY2 (GPIO86) toggles green LED (GPIO73)
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

#define LED_RED_GPIO            72
#define LED_GREEN_GPIO          73
#define KEY1_GPIO               87
#define KEY2_GPIO               86

#define KEY_SCAN_DELAY_MS       10
#define KEY_DEBOUNCE_MS         20
#define KEY_TEST_DURATION_MS    30000

/****************************************************************************
 * Private Types
 ****************************************************************************/

typedef struct
{
  int fd;
  bool last_value;
} key_state_s;

/****************************************************************************
 * Private Functions
 ****************************************************************************/

static int init_led_gpio(int gpio)
{
  int ret;
  char path[32];
  int fd;

  ret = pinctrl_set_gpio_function(gpio);
  if (ret < 0)
    {
      printf("[KEY] ERROR: pinctrl failed for GPIO%d\n", gpio);
      return ret;
    }

  snprintf(path, sizeof(path), "/dev/gpio%d", gpio);
  fd = open(path, O_RDWR);
  if (fd < 0)
    {
      printf("[KEY] ERROR: open %s failed: %d\n", path, errno);
      return -errno;
    }

  ret = ioctl(fd, GPIOC_SETPINTYPE, (unsigned long)GPIO_OUTPUT_PIN);
  if (ret < 0)
    {
      printf("[KEY] ERROR: GPIOC_SETPINTYPE(OUTPUT) on GPIO%d: %d\n",
             gpio, errno);
      close(fd);
      return -errno;
    }

  /* LED off (high = off for active-low) */

  ioctl(fd, GPIOC_WRITE, 1);
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
      printf("[KEY] ERROR: pinctrl failed for GPIO%d\n", gpio);
      return ret;
    }

  snprintf(path, sizeof(path), "/dev/gpio%d", gpio);
  fd = open(path, O_RDWR);
  if (fd < 0)
    {
      printf("[KEY] ERROR: open %s failed: %d\n", path, errno);
      return -errno;
    }

  ret = ioctl(fd, GPIOC_SETPINTYPE, (unsigned long)GPIO_INPUT_PIN);
  if (ret < 0)
    {
      printf("[KEY] ERROR: GPIOC_SETPINTYPE(INPUT) on GPIO%d: %d\n",
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

static int is_key_pressed(key_state_s *key)
{
  bool value = get_key_value(key->fd);

  /* Detect falling edge: HIGH -> LOW = press */

  if (!value && key->last_value)
    {
      up_mdelay(KEY_DEBOUNCE_MS);
      value = get_key_value(key->fd);
      if (!value)
        {
          key->last_value = value;
          return 1;
        }
    }

  key->last_value = value;
  return 0;
}

static void set_led(int fd, int on)
{
  /* Active-low: 0 = ON, 1 = OFF */

  ioctl(fd, GPIOC_WRITE, on ? 0 : 1);
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

int test_key(void)
{
  int fd_red;
  int fd_green;
  int fd_key1;
  int fd_key2;
  key_state_s key1;
  key_state_s key2;
  int red_on = 0;
  int green_on = 0;
  unsigned int total_tick = 0;

  printf("[KEY] === Key + LED Test (loong_hat 02_KEY) ===\n");
  printf("[KEY] KEY1(GPIO%d) -> toggle red LED(GPIO%d)\n",
         KEY1_GPIO, LED_RED_GPIO);
  printf("[KEY] KEY2(GPIO%d) -> toggle green LED(GPIO%d)\n",
         KEY2_GPIO, LED_GREEN_GPIO);
  printf("[KEY] Test duration: %d seconds\n\n",
         KEY_TEST_DURATION_MS / 1000);

  /* Initialize LEDs */

  fd_red = init_led_gpio(LED_RED_GPIO);
  if (fd_red < 0)
    {
      return fd_red;
    }

  fd_green = init_led_gpio(LED_GREEN_GPIO);
  if (fd_green < 0)
    {
      close(fd_red);
      return fd_green;
    }

  /* Initialize keys */

  fd_key1 = init_key_gpio(KEY1_GPIO);
  if (fd_key1 < 0)
    {
      close(fd_red);
      close(fd_green);
      return fd_key1;
    }

  fd_key2 = init_key_gpio(KEY2_GPIO);
  if (fd_key2 < 0)
    {
      close(fd_red);
      close(fd_green);
      close(fd_key1);
      return fd_key2;
    }

  /* Read initial key states */

  key1.fd = fd_key1;
  key1.last_value = get_key_value(fd_key1);
  key2.fd = fd_key2;
  key2.last_value = get_key_value(fd_key2);

  printf("[KEY] Initial: KEY1=%s KEY2=%s\n",
         key1.last_value ? "HIGH" : "LOW",
         key2.last_value ? "HIGH" : "LOW");
  printf("[KEY] Ready. Press keys to test...\n\n");

  /* Main loop - scan keys */

  while (total_tick < KEY_TEST_DURATION_MS)
    {
      /* Check KEY1 - toggle red LED */

      if (is_key_pressed(&key1))
        {
          red_on = !red_on;
          set_led(fd_red, red_on);
          printf("[KEY] KEY1 pressed, red %s\n",
                 red_on ? "ON" : "OFF");
        }

      /* Check KEY2 - toggle green LED */

      if (is_key_pressed(&key2))
        {
          green_on = !green_on;
          set_led(fd_green, green_on);
          printf("[KEY] KEY2 pressed, green %s\n",
                 green_on ? "ON" : "OFF");
        }

      up_mdelay(KEY_SCAN_DELAY_MS);
      total_tick += KEY_SCAN_DELAY_MS;
    }

  /* Turn off all LEDs */

  set_led(fd_red, 0);
  set_led(fd_green, 0);

  printf("\n[KEY] Test complete. All LEDs off.\n");

  close(fd_key2);
  close(fd_key1);
  close(fd_green);
  close(fd_red);

  return OK;
}
