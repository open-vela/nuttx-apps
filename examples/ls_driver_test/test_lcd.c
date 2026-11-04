/****************************************************************************
 * apps/examples/ls_driver_test/test_lcd.c
 *
 * Device status panel on the LS2K0300 RGB LCD.
 *
 * Drives the LEDs, the breathing PWM and the buzzer itself and draws
 * their state, so the picture and the physical board stay in step.  Read-
 * only peripherals (keys, ADC, temperature, light, RTC) are sampled once
 * per step and shown as text.
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Licensed to the Apache Software Foundation (ASF) under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.
 * The ASF licenses this file to you under the Apache License, Version 2.0
 * (the "License"); you may not use this file except in compliance with the
 * License.  You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.  See the
 * License for the specific language governing permissions and limitations
 * under the License.
 *
 ****************************************************************************/

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>

#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include <nuttx/arch.h>
#include <nuttx/ioexpander/gpio.h>
#include <nuttx/pinctrl/pinctrl.h>
#include <nuttx/timers/pwm.h>
#include <nuttx/timers/rtc.h>
#include <nuttx/video/fb.h>

#include "ls_driver_test.h"
#include "lcd_draw.h"
#include "lcd_sensors.h"

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#define LED_RED_GPIO     72
#define LED_GREEN_GPIO   73

#define PWM_GPIO         88
#define PWM_FUNCTION     2
#define PWM_DEVICE       "/dev/pwm2"
#define PWM_FREQUENCY    1000
#define PWM_DUTY_MAX     65536
#define PWM_BREATH_STEPS 10

#define KEY1_GPIO        87
#define KEY2_GPIO        86
#define BUZZER_GPIO      75
#define BUZZER_HALF_US   500
#define BUZZER_BEEP_MS   80

#define UI_TITLE         "LS2K0300 DEVICE STATUS"
#define UI_MARGIN        24
#define UI_TITLE_H       40
#define UI_ROW_Y         56
#define UI_ROW_H         40
#define UI_DOT_R         10
#define UI_DOT_X         (UI_MARGIN + UI_DOT_R)
#define UI_LABEL_X       (UI_MARGIN + UI_DOT_R * 2 + 24)
#define UI_TITLE_SCALE   2
#define UI_ROW_SCALE     1

#define LCD_DEFAULT_PERIOD_MS 500
#define LCD_PERIOD_MIN_MS    20

#define LCD_ADC_CHANNEL       0

/* Panel layout on 800x480: title at y=8, divider at y=40, then ten rows
 * of 40px starting at y=56.  Ten rows fit before y=480 with room to
 * spare, so adding a peripheral is one entry in g_items[] plus a case in
 * lcd_sample().
 *
 *   | LS2K0300 DEVICE STATUS      |
 *   +-----------------------------+
 *   |  (o) LED RED          ON    |
 *   |  (o) LED GREEN        OFF   |
 *   |  (o) PWM BLUE         45%   |  dot brightness tracks the PWM duty
 *   |  (o) KEY1         PRESSED   |  active low
 *   |  (o) KEY2        RELEASED   |
 *   |  (o) BUZZER          BEEP   |
 *   |  (o) ADC CH0        1234mV  |
 *   |  (o) TEMP           42.5C   |
 *   |  (o) LIGHT         12.34lx  |
 *   |  (o) RTC         12:34:56   |
 */

/****************************************************************************
 * Private Types
 ****************************************************************************/

enum lcd_item_kind_e
{
  LCD_ITEM_GPIO = 0,    /* on/off, driven by this test */
  LCD_ITEM_VALUE        /* sampled text */
};

struct lcd_status_item
{
  FAR const char *label;
  enum lcd_item_kind_e kind;
  int gpio;             /* LCD_ITEM_GPIO only; < 0 otherwise */
  uint32_t color;
  bool state;           /* LCD_ITEM_GPIO */
  bool ok;              /* LCD_ITEM_VALUE: last sample succeeded */
  char value[24];       /* LCD_ITEM_VALUE: right-hand text */
};

/****************************************************************************
 * Private Data
 ****************************************************************************/

static struct lcd_status_item g_items[] =
{
  { "LED RED",   LCD_ITEM_GPIO,  LED_RED_GPIO,   LCD_COLOR_RED,
    false, false, "" },
  { "LED GREEN", LCD_ITEM_GPIO,  LED_GREEN_GPIO, LCD_COLOR_GREEN,
    false, false, "" },
  { "PWM BLUE",  LCD_ITEM_VALUE, -1,             LCD_COLOR_BLUE,
    false, false, "--" },
  { "KEY1",      LCD_ITEM_VALUE, KEY1_GPIO,      LCD_COLOR_TEXT,
    false, false, "--" },
  { "KEY2",      LCD_ITEM_VALUE, KEY2_GPIO,      LCD_COLOR_TEXT,
    false, false, "--" },
  { "BUZZER",    LCD_ITEM_VALUE, BUZZER_GPIO,    LCD_COLOR_TEXT,
    false, false, "IDLE" },
  { "ADC CH0",   LCD_ITEM_VALUE, -1,             LCD_COLOR_GREEN,
    false, false, "--" },
  { "TEMP",      LCD_ITEM_VALUE, -1,             LCD_COLOR_GREEN,
    false, false, "--" },
  { "LIGHT",     LCD_ITEM_VALUE, -1,             LCD_COLOR_GREEN,
    false, false, "--" },
  { "RTC",       LCD_ITEM_VALUE, -1,             LCD_COLOR_TEXT,
    false, false, "--:--:--" },
};

#define LCD_NITEMS        (sizeof(g_items) / sizeof(g_items[0]))

#define LCD_IDX_LED_RED   0
#define LCD_IDX_LED_GREEN 1
#define LCD_IDX_PWM       2
#define LCD_IDX_KEY1      3
#define LCD_IDX_KEY2      4
#define LCD_IDX_BUZZER    5
#define LCD_IDX_ADC       6
#define LCD_IDX_TEMP      7
#define LCD_IDX_LIGHT     8
#define LCD_IDX_RTC       9

static int g_led_fds[LCD_NITEMS];
static int g_thermal_fd = -1;
static int g_pwm_fd = -1;
static int g_rtc_fd = -1;
static bool g_adc_ok;
static bool g_light_ok;
static bool g_pwm_ok;
static int g_pwm_phase;

/* The register dump lives in the arch layer; this tree declares such
 * helpers with plain externs (see ls2k0300_bringup.c).
 */

extern void ls2k0300_dc_dump(FAR const char *tag);

/****************************************************************************
 * Private Functions
 ****************************************************************************/

static uint32_t lcd_dim_color(uint32_t rgb, int percent)
{
  uint32_t r = ((rgb >> 16) & 0xff) * (uint32_t)percent / 100;
  uint32_t g = ((rgb >> 8) & 0xff) * (uint32_t)percent / 100;
  uint32_t b = (rgb & 0xff) * (uint32_t)percent / 100;

  return (r << 16) | (g << 8) | b;
}

static int lcd_gpio_setup(void)
{
  char path[32];
  int i;

  for (i = 0; i < (int)LCD_NITEMS; i++)
    {
      g_led_fds[i] = -1;

      if (g_items[i].kind != LCD_ITEM_GPIO || g_items[i].gpio < 0)
        {
          continue;
        }

      if (pinctrl_set_gpio_function(g_items[i].gpio) < 0)
        {
          printf("[LCD] ERROR: pinctrl for GPIO%d failed\n",
                 g_items[i].gpio);
          return -EIO;
        }

      snprintf(path, sizeof(path), "/dev/gpio%d", g_items[i].gpio);
      g_led_fds[i] = open(path, O_RDWR);
      if (g_led_fds[i] < 0)
        {
          printf("[LCD] ERROR: open %s failed: %d\n", path, errno);
          return -errno;
        }

      if (ioctl(g_led_fds[i], GPIOC_SETPINTYPE,
                (unsigned long)GPIO_OUTPUT_PIN) < 0)
        {
          printf("[LCD] ERROR: GPIOC_SETPINTYPE on GPIO%d: %d\n",
                 g_items[i].gpio, errno);
          return -errno;
        }
    }

  return OK;
}

static void lcd_gpio_release(void)
{
  int i;

  for (i = 0; i < (int)LCD_NITEMS; i++)
    {
      if (g_led_fds[i] >= 0)
        {
          close(g_led_fds[i]);
          g_led_fds[i] = -1;
        }
    }
}

/* Keys and the buzzer are plain GPIOs too, but the keys are inputs and
 * the buzzer is not part of the driven LED set, so they are handled here.
 */

static int lcd_gpio_open_input(int pin)
{
  char path[32];
  int fd;

  if (pinctrl_set_gpio_function(pin) < 0)
    {
      return -EIO;
    }

  snprintf(path, sizeof(path), "/dev/gpio%d", pin);
  fd = open(path, O_RDWR);
  if (fd < 0)
    {
      return -errno;
    }

  if (ioctl(fd, GPIOC_SETPINTYPE,
            (unsigned long)GPIO_INPUT_PIN) < 0)
    {
      close(fd);
      return -errno;
    }

  return fd;
}

static int lcd_gpio_open_output(int pin)
{
  char path[32];
  int fd;

  if (pinctrl_set_gpio_function(pin) < 0)
    {
      return -EIO;
    }

  snprintf(path, sizeof(path), "/dev/gpio%d", pin);
  fd = open(path, O_RDWR);
  if (fd < 0)
    {
      return -errno;
    }

  if (ioctl(fd, GPIOC_SETPINTYPE,
            (unsigned long)GPIO_OUTPUT_PIN) < 0)
    {
      close(fd);
      return -errno;
    }

  ioctl(fd, GPIOC_WRITE, 0);
  return fd;
}

static bool lcd_gpio_read(int fd)
{
  bool value = true;

  ioctl(fd, GPIOC_READ, (unsigned long)(uintptr_t)&value);
  return value;
}

/* Breathing PWM on GPIO88/PWM2.  GPIO88 doubles as the blue LED, so the
 * on-screen dot tracks the same duty the hardware is running.
 */

static bool lcd_pwm_setup(void)
{
  struct pinctrl_param_s param;
  struct pwm_info_s info;
  int pfd;

  pfd = open("/dev/pinctrl0", O_RDWR);
  if (pfd >= 0)
    {
      memset(&param, 0, sizeof(param));
      param.pin = PWM_GPIO;
      param.para.function = PWM_FUNCTION;
      if (ioctl(pfd, PINCTRLC_SETFUNCTION,
                (unsigned long)(uintptr_t)&param) < 0)
        {
          printf("[LCD] ERROR: GPIO%d -> function %d failed: %d\n",
                 PWM_GPIO, PWM_FUNCTION, errno);
        }

      close(pfd);
    }

  g_pwm_fd = open(PWM_DEVICE, O_RDONLY);
  if (g_pwm_fd < 0)
    {
      printf("[LCD] %s unavailable (%d)\n", PWM_DEVICE, errno);
      return false;
    }

  memset(&info, 0, sizeof(info));
  info.frequency = PWM_FREQUENCY;
  info.duty = 0;
  if (ioctl(g_pwm_fd, PWMIOC_SETCHARACTERISTICS,
            (unsigned long)(uintptr_t)&info) < 0 ||
      ioctl(g_pwm_fd, PWMIOC_START, 0) < 0)
    {
      printf("[LCD] PWM start failed: %d\n", errno);
      close(g_pwm_fd);
      g_pwm_fd = -1;
      return false;
    }

  return true;
}

static void lcd_pwm_release(void)
{
  if (g_pwm_fd >= 0)
    {
      ioctl(g_pwm_fd, PWMIOC_STOP, 0);
      close(g_pwm_fd);
      g_pwm_fd = -1;
    }
}

/* Triangle ramp: 0 -> 100% -> 0 over 2 * PWM_BREATH_STEPS steps. */

static int lcd_pwm_duty_percent(void)
{
  int period = 2 * PWM_BREATH_STEPS;
  int p = g_pwm_phase % period;

  return (p < PWM_BREATH_STEPS)
           ? (p * 100) / PWM_BREATH_STEPS
           : ((period - p) * 100) / PWM_BREATH_STEPS;
}

static void lcd_pwm_advance(void)
{
  struct pwm_info_s info;
  int pct;

  if (!g_pwm_ok)
    {
      return;
    }

  pct = lcd_pwm_duty_percent();
  g_pwm_phase++;

  memset(&info, 0, sizeof(info));
  info.frequency = PWM_FREQUENCY;
  info.duty = (uint32_t)pct * PWM_DUTY_MAX / 100;
  ioctl(g_pwm_fd, PWMIOC_SETCHARACTERISTICS,
        (unsigned long)(uintptr_t)&info);
}

/* A passive buzzer needs a real square wave, and sub-tick sleeps cannot
 * produce one: usleep() resolves to CONFIG_USEC_PER_TICK (10 ms) at
 * best, so 200 half-cycles of usleep(500) stretched a 100 ms beep into
 * several seconds of wall time and stalled the whole panel -- serial
 * output and the LCD alike.  Busy-wait the half-cycle instead: 500 us x
 * 160 edges is a bounded 80 ms stall and keeps a clean 1 kHz tone.
 */

static void lcd_buzzer_beep(int fd)
{
  int edges = (BUZZER_BEEP_MS * 1000) / BUZZER_HALF_US;
  int i;

  for (i = 0; i < edges; i++)
    {
      ioctl(fd, GPIOC_WRITE, (i & 1) ? 0 : 1);
      up_udelay(BUZZER_HALF_US);
    }

  ioctl(fd, GPIOC_WRITE, 0);
}

/* Bring up whatever is present.  A missing peripheral is not fatal: its
 * row shows "--" with a hollow dot.
 */

static void lcd_sensors_setup(void)
{
  g_thermal_fd = open("/dev/thermal0", O_RDONLY);
  if (g_thermal_fd < 0)
    {
      printf("[LCD] thermal0 unavailable (%d)\n", errno);
    }

  g_rtc_fd = open("/dev/rtc0", O_RDWR);
  if (g_rtc_fd < 0)
    {
      printf("[LCD] rtc0 unavailable (%d)\n", errno);
    }

  g_adc_ok = (mcp3204_init() == 0);
  if (!g_adc_ok)
    {
      printf("[LCD] MCP3204 unavailable\n");
    }

  if (sensor_bus_open() == OK && bh1750_init() == OK)
    {
      g_light_ok = true;
    }
  else
    {
      printf("[LCD] BH1750 unavailable\n");
    }

  g_pwm_ok = lcd_pwm_setup();
}

static void lcd_sensors_release(void)
{
  lcd_pwm_release();

  if (g_adc_ok)
    {
      mcp3204_deinit();
    }

  sensor_bus_close();

  if (g_rtc_fd >= 0)
    {
      close(g_rtc_fd);
      g_rtc_fd = -1;
    }

  if (g_thermal_fd >= 0)
    {
      close(g_thermal_fd);
      g_thermal_fd = -1;
    }
}

static void lcd_sample(void)
{
  struct lcd_status_item *pwm = &g_items[LCD_IDX_PWM];
  struct lcd_status_item *adc = &g_items[LCD_IDX_ADC];
  struct lcd_status_item *temp = &g_items[LCD_IDX_TEMP];
  struct lcd_status_item *lux = &g_items[LCD_IDX_LIGHT];
  struct lcd_status_item *rtc = &g_items[LCD_IDX_RTC];
  uint16_t raw;

  pwm->ok = g_pwm_ok;
  if (g_pwm_ok)
    {
      int pct = lcd_pwm_duty_percent();

      snprintf(pwm->value, sizeof(pwm->value), "%d%%", pct);

      /* The dot itself breathes with the hardware.
       */

      pwm->color = lcd_dim_color(LCD_COLOR_BLUE, pct);
    }

  adc->ok = false;
  if (g_adc_ok && mcp3204_read_average(LCD_ADC_CHANNEL, &raw) == 0)
    {
      snprintf(adc->value, sizeof(adc->value), "%umV",
               (unsigned int)mcp3204_raw_to_mv(raw));
      adc->ok = true;
    }

  temp->ok = false;
  if (g_thermal_fd >= 0)
    {
      int32_t milli;

      if (read(g_thermal_fd, &milli, sizeof(milli)) == sizeof(milli))
        {
          snprintf(temp->value, sizeof(temp->value), "%d.%dC",
                   (int)(milli / 1000), (int)(abs(milli % 1000) / 100));
          temp->ok = true;
        }
    }

  lux->ok = false;
  if (g_light_ok)
    {
      unsigned int lux_x100;

      if (bh1750_read_lux(&lux_x100) == OK)
        {
          snprintf(lux->value, sizeof(lux->value), "%u.%02ulx",
                   lux_x100 / 100, lux_x100 % 100);
          lux->ok = true;
        }
    }

  rtc->ok = false;
  if (g_rtc_fd >= 0)
    {
      struct rtc_time rtctime;

      if (ioctl(g_rtc_fd, RTC_RD_TIME,
                (unsigned long)(uintptr_t)&rtctime) == 0)
        {
          snprintf(rtc->value, sizeof(rtc->value), "%02d:%02d:%02d",
                   rtctime.tm_hour, rtctime.tm_min, rtctime.tm_sec);
          rtc->ok = true;
        }
    }
}

static void lcd_set_led(int index, bool on)
{
  if (g_led_fds[index] < 0)
    {
      return;
    }

  if (ioctl(g_led_fds[index], GPIOC_WRITE, on ? 1 : 0) < 0)
    {
      printf("[LCD] ERROR: GPIOC_WRITE on GPIO%d: %d\n",
             g_items[index].gpio, errno);
    }
}

/* Draw the whole panel.  800x480x4 is well under a millisecond of stores,
 * so a full redraw every tick is simpler than tracking dirty rows and
 * costs nothing at a 500 ms period.
 */

static void lcd_draw_panel(FAR struct lcd_s *lcd)
{
  int i;

  lcd_clear(lcd, LCD_COLOR_BG);

  lcd_draw_text(lcd, UI_MARGIN,
                (UI_TITLE_H - lcd_font_height(UI_TITLE_SCALE)) / 2,
                UI_TITLE, LCD_COLOR_TEXT, UI_TITLE_SCALE);
  lcd_draw_hline(lcd, 0, UI_TITLE_H, lcd->xres, LCD_COLOR_DIV);

  for (i = 0; i < (int)LCD_NITEMS; i++)
    {
      FAR struct lcd_status_item *item = &g_items[i];
      int row_y = UI_ROW_Y + i * UI_ROW_H;
      int cy = row_y + UI_ROW_H / 2;
      bool lit;
      FAR const char *text;
      uint32_t text_color;
      int tw;

      if (item->kind == LCD_ITEM_GPIO)
        {
          lit = item->state;
          text = lit ? "ON" : "OFF";
          text_color = lit ? item->color : LCD_COLOR_DIM;
        }
      else
        {
          lit = item->ok;
          text = item->ok ? item->value : "--";
          text_color = lit ? item->color : LCD_COLOR_DIM;
        }

      lcd_draw_circle(lcd, UI_DOT_X, cy, UI_DOT_R,
                      lit ? item->color : LCD_COLOR_DIM, lit);
      lcd_draw_text(lcd, UI_LABEL_X,
                    cy - lcd_font_height(UI_ROW_SCALE) / 2,
                    item->label, LCD_COLOR_TEXT, UI_ROW_SCALE);

      tw = lcd_text_width(text, UI_ROW_SCALE);
      lcd_draw_text(lcd, lcd->xres - UI_MARGIN - tw,
                    cy - lcd_font_height(UI_ROW_SCALE) / 2,
                    text, text_color, UI_ROW_SCALE);
    }
}

static void lcd_show_dump(void)
{
  struct fb_videoinfo_s vinfo;
  struct lcd_s lcd;
  int fd;

  printf("[LCD] DC register dump:\n");
  ls2k0300_dc_dump("now");

  fd = open("/dev/fb0", O_RDWR);
  if (fd < 0)
    {
      printf("[LCD] ERROR: open /dev/fb0 failed: %d\n", errno);
      return;
    }

  if (ioctl(fd, FBIOGET_VIDEOINFO, (unsigned long)&vinfo) == 0)
    {
      printf("[LCD] videoinfo: fmt=%d xres=%d yres=%d nplanes=%d\n",
             vinfo.fmt, vinfo.xres, vinfo.yres, vinfo.nplanes);
    }

  close(fd);

  if (lcd_open(&lcd) == OK)
    {
      printf("[LCD] planeinfo: fb=%p stride=%d xres=%d yres=%d\n",
             lcd.fb, lcd.stride, lcd.xres, lcd.yres);
      lcd_close(&lcd);
    }
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

int test_lcd(int argc, FAR char *argv[])
{
  struct lcd_s lcd;
  unsigned int period = LCD_DEFAULT_PERIOD_MS;
  int key1_fd = -1;
  int key2_fd = -1;
  int buzz_fd = -1;
  int ret;
  int step = 0;

  if (argc > 1)
    {
      if (strcmp(argv[1], "dump") == 0)
        {
          lcd_show_dump();
          return OK;
        }

      period = (unsigned int)strtoul(argv[1], NULL, 0);
      if (period < LCD_PERIOD_MIN_MS)
        {
          printf("[LCD] period too small, using %d ms\n",
                 LCD_PERIOD_MIN_MS);
          period = LCD_PERIOD_MIN_MS;
        }
    }

  ret = lcd_open(&lcd);
  if (ret < 0)
    {
      return ret;
    }

  ret = lcd_gpio_setup();
  if (ret < 0)
    {
      lcd_close(&lcd);
      return ret;
    }

  key1_fd = lcd_gpio_open_input(KEY1_GPIO);
  key2_fd = lcd_gpio_open_input(KEY2_GPIO);
  buzz_fd = lcd_gpio_open_output(BUZZER_GPIO);
  if (key1_fd < 0 || key2_fd < 0 || buzz_fd < 0)
    {
      printf("[LCD] KEY1/KEY2/BUZZER setup failed\n");
    }

  lcd_sensors_setup();

  printf("[LCD] device status panel, %d ms per step, Ctrl-C to stop\n",
         period);
  printf("[LCD] press KEY1(GPIO%d) to beep\n", KEY1_GPIO);

  for (; ; )
    {
      bool pressed1;
      bool pressed2;
      int i;

      /* Walk the four LED combinations so both the picture and the
       * physical LEDs can be seen tracking each other.
       */

      for (i = 0; i < 2; i++)
        {
          bool on = (step >> i) & 1;

          g_items[i].state = on;
          lcd_set_led(i, on);
        }

      lcd_pwm_advance();

      /* Keys are active low. */

      pressed1 = (key1_fd >= 0) && !lcd_gpio_read(key1_fd);
      pressed2 = (key2_fd >= 0) && !lcd_gpio_read(key2_fd);
      g_items[LCD_IDX_KEY1].ok = (key1_fd >= 0);
      g_items[LCD_IDX_KEY2].ok = (key2_fd >= 0);
      strcpy(g_items[LCD_IDX_KEY1].value, pressed1 ? "PRESSED" : "RELEASED");
      strcpy(g_items[LCD_IDX_KEY2].value, pressed2 ? "PRESSED" : "RELEASED");

      if (pressed1 && buzz_fd >= 0)
        {
          strcpy(g_items[LCD_IDX_BUZZER].value, "BEEP");
          g_items[LCD_IDX_BUZZER].ok = true;
          lcd_buzzer_beep(buzz_fd);
        }
      else if (buzz_fd >= 0)
        {
          strcpy(g_items[LCD_IDX_BUZZER].value, "IDLE");
          g_items[LCD_IDX_BUZZER].ok = true;
        }

      lcd_sample();
      lcd_draw_panel(&lcd);

      printf("[LCD] step %d:", step);
      for (i = 0; i < (int)LCD_NITEMS; i++)
        {
          FAR struct lcd_status_item *item = &g_items[i];

          printf(" %s=%s", item->label,
                 item->kind == LCD_ITEM_GPIO
                   ? (item->state ? "ON" : "OFF")
                   : (item->ok ? item->value : "--"));
        }

      printf("\n");

      step = (step + 1) & 3;
      usleep(period * 1000);
    }

  /* Not reached: Ctrl-C ends the loop. */

  lcd_sensors_release();
  lcd_gpio_release();
  lcd_close(&lcd);
  return OK;
}
