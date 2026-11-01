/****************************************************************************
 * apps/examples/ls_lvgl/ls_lvgl.c
 *
 * Device status console on the LS2K0300 800x480 RGB panel (M-L1/L2).
 *
 * Two columns: actuators on the left, sampled values on the right.  The
 * board keys drive it -- KEY2 walks the selection down the CONTROL column,
 * KEY1 toggles the selected item -- and the SSD1306 on I2C1 mirrors the
 * selection so the pick is visible without looking at the panel.
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
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include <nuttx/arch.h>
#include <nuttx/i2c/i2c_master.h>
#include <nuttx/ioexpander/gpio.h>
#include <nuttx/pinctrl/pinctrl.h>
#include <nuttx/timers/pwm.h>
#include <nuttx/timers/rtc.h>

#include <pthread.h>

#include <lvgl/lvgl.h>

#include "../ls_driver_test/oled_fonts.h"
#include "../ls_driver_test/lcd_sensors.h"

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#define UI_BG_COLOR       0x1a2332
#define UI_PANEL_COLOR    0x222e42
#define UI_SEL_COLOR      0x2f4463
#define UI_TEXT_COLOR     0xffffff
#define UI_DIM_COLOR      0x8a94a6
#define UI_ACCENT_COLOR   0x34c759

#define UI_MARGIN         16
#define UI_COL_GAP        16
#define UI_ROW_GAP        12
#define UI_ROW_H          60
#define UI_COL_W          ((800 - UI_MARGIN * 2 - UI_COL_GAP) / 2)

#define UI_VALUE_LEN      24

#define KEY1_GPIO         87      /* toggle the selected item */
#define KEY2_GPIO         86      /* move the selection */

#define LED_RED_GPIO      72
#define LED_GREEN_GPIO    73
#define BUZZER_GPIO       75

#define PWM_GPIO          88
#define PWM_FUNCTION      2
#define PWM_DEVICE        "/dev/pwm2"
#define PWM_FREQUENCY     1000
#define PWM_DUTY_MAX      65536
#define PWM_BREATH_STEPS  10

#define OLED_I2C_PATH     "/dev/i2c1"
#define OLED_ADDR         0x3c
#define OLED_CTRL_CMD     0x00
#define OLED_CTRL_DATA    0x40

#define BUZZER_HALF_US    500
#define BUZZER_BEEP_MS    80
#define BUZZER_PERIOD_MS  500

#define ACT_LED_RED       0
#define ACT_LED_GREEN     1
#define ACT_PWM           2
#define ACT_BUZZER        3

#define ROW_ADC           4
#define ROW_TEMP          5
#define ROW_LIGHT         6
#define ROW_RTC           7

#define TICK_MS           20


/* AI subtitle: agent status + reply shown at the bottom of the panel.
 * Written from other tasks via ls_lvgl_set_ai_text(), consumed by the
 * UI loop which owns all LVGL object access.
 */

#define AI_STATUS_LEN     32
#define AI_TEXT_LEN       256
#define AI_TEXT_LINES     2
#define AI_PANEL_H        96

/* Optional CJK font for the subtitle.  Without this file on the SD card
 * the subtitle falls back to Montserrat (ASCII only).  Copy
 * packages/demos/calculator/res/fonts/MiSans-Normal.ttf (or any CJK
 * ttf) onto the card to render Chinese agent replies.  The card is
 * FAT-mounted at /mnt, which is writable from a PC.
 */

#define AI_FONT_PATH      "/mnt/MiSans-Normal.ttf"

/****************************************************************************
 * Private Types
 ****************************************************************************/

enum row_kind_e
{
  ROW_SWITCH = 0,
  ROW_VALUE
};

struct ui_row_s
{
  FAR const char *caption;
  enum row_kind_e kind;
  int gpio;                     /* actuator gpio, < 0 for value rows */
  lv_obj_t *box;
  lv_obj_t *widget;
  char value_buf[UI_VALUE_LEN];
};

/* Actuator hook: KEY1 drives whatever the row's toggle does. */

typedef void (*row_toggle_fn)(bool on);

struct act_s
{
  FAR const char *name;
  int gpio;
  row_toggle_fn toggle;
};

/****************************************************************************
 * Private Function Prototypes
 ****************************************************************************/

static void act_led_red(bool on);
static void act_led_green(bool on);
static void act_pwm(bool on);
static void act_buzzer(bool on);
static void oled_sync(void);

/****************************************************************************
 * Private Data
 ****************************************************************************/

static struct ui_row_s g_rows[] =
{
  { "LED RED",   ROW_SWITCH, LED_RED_GPIO,   NULL, NULL, "" },
  { "LED GREEN", ROW_SWITCH, LED_GREEN_GPIO, NULL, NULL, "" },
  { "PWM BLUE",  ROW_SWITCH, PWM_GPIO,      NULL, NULL, "" },
  { "BUZZER",    ROW_SWITCH, BUZZER_GPIO,   NULL, NULL, "" },
  { "ADC CH0",   ROW_VALUE,  -1,             NULL, NULL, "--" },
  { "TEMP",      ROW_VALUE,  -1,             NULL, NULL, "--" },
  { "LIGHT",     ROW_VALUE,  -1,             NULL, NULL, "--" },
  { "RTC",       ROW_VALUE,  -1,             NULL, NULL, "--:--:--" },
};

#define UI_NROWS       (sizeof(g_rows) / sizeof(g_rows[0]))
#define UI_NACT        4

static struct act_s g_acts[] =
{
  { "LED RED",   LED_RED_GPIO,   act_led_red },
  { "LED GREEN", LED_GREEN_GPIO, act_led_green },
  { "PWM BLUE",  PWM_GPIO,       act_pwm },
  { "BUZZER",    BUZZER_GPIO,    act_buzzer },
};

static int g_selected;
static bool g_state[UI_NACT];

/* Actuator states posted by the agent's board tools.  The tools change the
 * hardware from their own thread, and LVGL objects may only be touched by
 * the UI thread, so the new state is posted here and applied on the next
 * tick - the same pattern the AI subtitle uses. */

static pthread_mutex_t g_act_lock = PTHREAD_MUTEX_INITIALIZER;
static bool g_act_pending[UI_NACT];
static bool g_act_dirty;
static bool g_pwm_taken;   /* the agent drives PWM2 now */
static int g_rtc_fd = -1;
static int g_oled_fd = -1;
static int g_pwm_fd = -1;
static int g_buzz_fd = -1;
static bool g_pwm_on;
static int g_pwm_phase;
static int g_thermal_fd = -1;
static bool g_adc_ok;
static bool g_light_ok;

/* AI subtitle state.  The buffers are written from other tasks via
 * ls_lvgl_set_ai_text(); only the UI loop touches the LVGL objects.
 */

static pthread_mutex_t g_ai_lock = PTHREAD_MUTEX_INITIALIZER;
static char g_ai_status[AI_STATUS_LEN];
static char g_ai_text[AI_TEXT_LEN];
static volatile bool g_ai_dirty;
static lv_obj_t *g_ai_status_label;
static lv_obj_t *g_ai_text_label;
#ifdef CONFIG_LV_USE_FREETYPE
static lv_font_t *g_ai_font;
#endif

static FAR const lv_font_t *ai_font(void)
{
#ifdef CONFIG_LV_USE_FREETYPE
  if (g_ai_font != NULL)
    {
      return g_ai_font;
    }
#endif
  return &lv_font_montserrat_20;
}

/****************************************************************************
 * Private Functions - Actuators
 ****************************************************************************/

static int act_gpio_open(int pin, bool output)
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
            (unsigned long)(output ? GPIO_OUTPUT_PIN : GPIO_INPUT_PIN)) < 0)
    {
      close(fd);
      return -errno;
    }

  return fd;
}

static void act_led_red(bool on)
{
  int fd = act_gpio_open(LED_RED_GPIO, true);

  if (fd >= 0)
    {
      ioctl(fd, GPIOC_WRITE, on ? 1 : 0);
      close(fd);
    }
}

static void act_led_green(bool on)
{
  int fd = act_gpio_open(LED_GREEN_GPIO, true);

  if (fd >= 0)
    {
      ioctl(fd, GPIOC_WRITE, on ? 1 : 0);
      close(fd);
    }
}

static void act_pwm(bool on)
{
  struct pwm_info_s info;

  if (g_pwm_fd < 0)
    {
      return;
    }

  g_pwm_on = on;
  memset(&info, 0, sizeof(info));
  info.frequency = PWM_FREQUENCY;
  info.duty = on ? (PWM_DUTY_MAX / 2) : 0;
  ioctl(g_pwm_fd, PWMIOC_SETCHARACTERISTICS, (unsigned long)(uintptr_t)&info);
}

/* Triangle ramp, applied once per tick while breathing is enabled. */

static void act_pwm_tick(void)
{
  struct pwm_info_s info;
  int period = 2 * PWM_BREATH_STEPS;
  int p;
  int pct;

  if (!g_pwm_on || g_pwm_fd < 0)
    {
      return;
    }

  p = g_pwm_phase++ % period;
  pct = (p < PWM_BREATH_STEPS) ? (p * 100) / PWM_BREATH_STEPS
                               : ((period - p) * 100) / PWM_BREATH_STEPS;

  memset(&info, 0, sizeof(info));
  info.frequency = PWM_FREQUENCY;
  info.duty = (uint32_t)pct * PWM_DUTY_MAX / 100;
  ioctl(g_pwm_fd, PWMIOC_SETCHARACTERISTICS, (unsigned long)(uintptr_t)&info);
}

/* The buzzer just latches; act_buzzer_tick() does the periodic beeping so a
 * long key press never blocks the panel.
 */

/* Put every actuator in its off state.  The GPIO pins keep whatever
 * level they powered up with, so without this the green LED sits lit
 * from the moment the panel starts. */
static void act_all_off(void)
{
  act_led_red(false);
  act_led_green(false);
  act_pwm(false);
  act_buzzer(false);
}

static void act_buzzer(bool on)
{
  if (!on && g_buzz_fd >= 0)
    {
      ioctl(g_buzz_fd, GPIOC_WRITE, 0);
    }
}

static void act_buzzer_tick(int tick)
{
  int edges = (BUZZER_BEEP_MS * 1000) / BUZZER_HALF_US;
  int i;

  if (!g_state[ACT_BUZZER] || g_buzz_fd < 0)
    {
      return;
    }

  /* One burst every BUZZER_PERIOD_MS, matching the earlier behaviour. */

  if ((tick % (BUZZER_PERIOD_MS / TICK_MS)) != 0)
    {
      return;
    }

  /* A passive buzzer needs a real square wave; usleep() cannot resolve
   * below one tick, so busy-wait the half period.
   */

  for (i = 0; i < edges; i++)
    {
      ioctl(g_buzz_fd, GPIOC_WRITE, (i & 1) ? 0 : 1);
      up_udelay(BUZZER_HALF_US);
    }

  ioctl(g_buzz_fd, GPIOC_WRITE, 0);
}

/* Sample the value column.  Temperature needs no bus; ADC and light come
 * from the shared helpers in ls_driver_test.
 */

static void ui_sample(void)
{
  uint16_t raw;
  unsigned int lux_x100;
  int32_t milli;

  if (g_thermal_fd >= 0 &&
      read(g_thermal_fd, &milli, sizeof(milli)) == sizeof(milli))
    {
      snprintf(g_rows[ROW_TEMP].value_buf, UI_VALUE_LEN, "%d.%dC",
               (int)(milli / 1000), (int)(abs(milli % 1000) / 100));
    }

  if (g_adc_ok && mcp3204_read_average(0, &raw) == 0)
    {
      snprintf(g_rows[ROW_ADC].value_buf, UI_VALUE_LEN, "%umV",
               (unsigned int)mcp3204_raw_to_mv(raw));
    }

  if (g_light_ok && bh1750_read_lux(&lux_x100) == OK)
    {
      snprintf(g_rows[ROW_LIGHT].value_buf, UI_VALUE_LEN, "%u.%02ulx",
               lux_x100 / 100, lux_x100 % 100);
    }

  lv_label_set_text(g_rows[ROW_ADC].widget, g_rows[ROW_ADC].value_buf);
  lv_label_set_text(g_rows[ROW_TEMP].widget, g_rows[ROW_TEMP].value_buf);
  lv_label_set_text(g_rows[ROW_LIGHT].widget, g_rows[ROW_LIGHT].value_buf);
}

static bool key_pressed(int fd)
{
  bool v = true;

  if (fd < 0)
    {
      return false;
    }

  ioctl(fd, GPIOC_READ, (unsigned long)(uintptr_t)&v);
  return !v;                    /* keys are active low */
}

/****************************************************************************
 * Private Functions - SSD1306 on I2C1
 ****************************************************************************/

static int lvoled_xfer(FAR const uint8_t *data, int len)
{
  struct i2c_msg_s msg;
  struct i2c_transfer_s trans;

  memset(&msg, 0, sizeof(msg));
  msg.addr = OLED_ADDR;
  msg.frequency = 400000;
  msg.buffer = (FAR uint8_t *)data;
  msg.length = len;

  memset(&trans, 0, sizeof(trans));
  trans.msgv = &msg;
  trans.msgc = 1;

  return ioctl(g_oled_fd, I2CIOC_TRANSFER, (unsigned long)&trans) < 0
           ? -errno : OK;
}

static int lvoled_cmd(uint8_t cmd)
{
  uint8_t buf[2] = { OLED_CTRL_CMD, cmd };
  return lvoled_xfer(buf, 2);
}

static int lvoled_data(FAR const uint8_t *data, int len)
{
  uint8_t buf[17];

  while (len > 0)
    {
      int n = len > 16 ? 16 : len;

      buf[0] = OLED_CTRL_DATA;
      memcpy(buf + 1, data, n);
      if (lvoled_xfer(buf, n + 1) < 0)
        {
          return -EIO;
        }

      data += n;
      len -= n;
    }

  return OK;
}

static void lvoled_init(void)
{
  static const uint8_t seq[] =
  {
    0xae, 0x20, 0x10, 0xb0, 0xc8, 0x00, 0x10, 0x40, 0x81, 0xff,
    0xa1, 0xa6, 0xa8, 0x3f, 0xa4, 0xd3, 0x00, 0xd5, 0x80, 0xd9,
    0xf1, 0xda, 0x12, 0xdb, 0x40, 0x8d, 0x14, 0xaf
  };

  size_t i;

  for (i = 0; i < sizeof(seq); i++)
    {
      lvoled_cmd(seq[i]);
    }
}

/* Two 8x16 lines.  A glyph is 16px tall and the SSD1306 addresses 8 rows
 * per page, so each line spans two pages: the name on pages 0-1 and the
 * state on pages 2-3, leaving a blank band between them.
 */

static void lvoled_show(FAR const char *text, FAR const char *state)
{
  uint8_t page[128];
  int line;
  int half;
  int col;
  int ch;

  for (line = 0; line < 2; line++)
    {
      FAR const char *s = (line == 0) ? text : state;

      for (half = 0; half < 2; half++)
        {
          lvoled_cmd(0xb0 + (line * 2 + half));
          lvoled_cmd(0x00);
          lvoled_cmd(0x10);

          memset(page, 0, sizeof(page));
          for (col = 0; col < 16 && s[col] != '\0'; col++)
            {
              int c = (unsigned char)s[col];

              if (c < ' ' || c > '~')
                {
                  c = ' ';
                }

              for (ch = 0; ch < 8; ch++)
                {
                  page[col * 8 + ch] =
                      F8X16[(c - ' ') * 16 + half * 8 + ch];
                }
            }

          lvoled_data(page, 128);
        }
    }
}

/****************************************************************************
 * Private Functions - UI
 ****************************************************************************/

static void ui_select(int index)
{
  size_t i;

  if (index < 0)
    {
      index = (int)UI_NACT - 1;
    }
  else if (index >= (int)UI_NACT)
    {
      index = 0;
    }

  g_selected = index;

  for (i = 0; i < UI_NROWS; i++)
    {
      lv_color_t c = lv_color_hex((int)i == index ? UI_SEL_COLOR
                                                  : UI_PANEL_COLOR);

      lv_obj_set_style_bg_color(g_rows[i].box, c, 0);
    }

  oled_sync();
  printf("[UI] selected: %s\n", g_acts[index].name);
}

static void oled_sync(void)
{
  if (g_oled_fd >= 0)
    {
      lvoled_show(g_acts[g_selected].name,
                  g_state[g_selected] ? "STATE: ON" : "STATE: OFF");
    }
}

static void ui_toggle_selected(void)
{
  struct act_s *act = &g_acts[g_selected];

  g_state[g_selected] = !g_state[g_selected];
  act->toggle(g_state[g_selected]);

  if (g_rows[g_selected].widget != NULL)
    {
      if (g_state[g_selected])
        {
          lv_obj_add_state(g_rows[g_selected].widget, LV_STATE_CHECKED);
        }
      else
        {
          lv_obj_clear_state(g_rows[g_selected].widget, LV_STATE_CHECKED);
        }
    }

  printf("[UI] %s -> %s\n", act->name, g_state[g_selected] ? "ON" : "OFF");
  oled_sync();
}

static void ui_make_row(FAR struct ui_row_s *row, FAR lv_obj_t *parent)
{
  lv_obj_t *box = lv_obj_create(parent);
  lv_obj_t *caption;
  lv_obj_t *right;

  lv_obj_remove_style_all(box);
  lv_obj_set_size(box, UI_COL_W, UI_ROW_H);
  lv_obj_set_flex_flow(box, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(box, LV_FLEX_ALIGN_SPACE_BETWEEN,
                        LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_hor(box, 14, 0);
  lv_obj_set_style_bg_color(box, lv_color_hex(UI_PANEL_COLOR), 0);
  lv_obj_set_style_bg_opa(box, LV_OPA_COVER, 0);
  lv_obj_set_style_radius(box, 10, 0);

  caption = lv_label_create(box);
  lv_label_set_text(caption, row->caption);
  lv_obj_set_style_text_color(caption, lv_color_hex(UI_TEXT_COLOR), 0);
  lv_obj_set_style_text_font(caption, &lv_font_montserrat_20, 0);

  if (row->kind == ROW_SWITCH)
    {
      right = lv_switch_create(box);
    }
  else
    {
      right = lv_label_create(box);
      lv_label_set_text(right, row->value_buf);
      lv_obj_set_style_text_color(right, lv_color_hex(UI_ACCENT_COLOR), 0);
      lv_obj_set_style_text_font(right, &lv_font_montserrat_20, 0);
    }

  row->box = box;
  row->widget = right;
}

static void ui_build(void)
{
  lv_obj_t *scr = lv_screen_active();
  lv_obj_t *title;
  lv_obj_t *cols;
  lv_obj_t *left;
  lv_obj_t *right;
  size_t i;

  lv_obj_set_style_bg_color(scr, lv_color_hex(UI_BG_COLOR), 0);
  lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
  lv_obj_set_style_pad_all(scr, UI_MARGIN, 0);
  lv_obj_set_flex_flow(scr, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_style_pad_row(scr, UI_ROW_GAP, 0);

  title = lv_label_create(scr);
  lv_label_set_text(title, "LS2K0300 DEVICE STATUS");
  lv_obj_set_style_text_color(title, lv_color_hex(UI_TEXT_COLOR), 0);
  lv_obj_set_style_text_font(title, &lv_font_montserrat_24, 0);

  cols = lv_obj_create(scr);
  lv_obj_remove_style_all(cols);
  lv_obj_set_size(cols, 800 - UI_MARGIN * 2, LV_SIZE_CONTENT);
  lv_obj_set_flex_flow(cols, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(cols, LV_FLEX_ALIGN_SPACE_BETWEEN,
                        LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
  lv_obj_set_style_pad_column(cols, UI_COL_GAP, 0);
  lv_obj_set_style_pad_row(cols, UI_ROW_GAP, 0);

  left = lv_obj_create(cols);
  lv_obj_remove_style_all(left);
  lv_obj_set_size(left, UI_COL_W, LV_SIZE_CONTENT);
  lv_obj_set_flex_flow(left, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_style_pad_row(left, UI_ROW_GAP, 0);

  right = lv_obj_create(cols);
  lv_obj_remove_style_all(right);
  lv_obj_set_size(right, UI_COL_W, LV_SIZE_CONTENT);
  lv_obj_set_flex_flow(right, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_style_pad_row(right, UI_ROW_GAP, 0);

  for (i = 0; i < UI_NROWS; i++)
    {
      ui_make_row(&g_rows[i], (int)i < (int)UI_NACT ? left : right);
    }

  /* AI subtitle panel at the bottom: status line + agent reply text. */

  {
    lv_obj_t *ai_panel = lv_obj_create(scr);
    lv_obj_t *ai_caption;

    lv_obj_remove_style_all(ai_panel);
    lv_obj_set_size(ai_panel, 800 - UI_MARGIN * 2, AI_PANEL_H);
    lv_obj_set_style_bg_color(ai_panel, lv_color_hex(UI_PANEL_COLOR), 0);
    lv_obj_set_style_bg_opa(ai_panel, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(ai_panel, 10, 0);
    lv_obj_set_style_pad_all(ai_panel, 12, 0);
    lv_obj_set_flex_flow(ai_panel, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(ai_panel, 6, 0);

    ai_caption = lv_label_create(ai_panel);
    lv_label_set_text(ai_caption, "AI AGENT");
    lv_obj_set_style_text_color(ai_caption, lv_color_hex(UI_DIM_COLOR), 0);
    lv_obj_set_style_text_font(ai_caption, &lv_font_montserrat_20, 0);

    g_ai_status_label = lv_label_create(ai_panel);
    lv_label_set_text(g_ai_status_label, "idle");
    lv_obj_set_style_text_color(g_ai_status_label,
                                lv_color_hex(UI_ACCENT_COLOR), 0);
    lv_obj_set_style_text_font(g_ai_status_label, ai_font(), 0);

    g_ai_text_label = lv_label_create(ai_panel);
    lv_label_set_text(g_ai_text_label, "");
    lv_label_set_long_mode(g_ai_text_label, LV_LABEL_LONG_DOT);
    lv_obj_set_width(g_ai_text_label, 800 - UI_MARGIN * 2 - 24);
    lv_obj_set_style_text_color(g_ai_text_label,
                                lv_color_hex(UI_TEXT_COLOR), 0);
    lv_obj_set_style_text_font(g_ai_text_label, ai_font(), 0);
  }
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: utf8_copy
 *
 * Description:
 *   Copy a UTF-8 string into a fixed buffer without cutting a multi-byte
 *   character in half.  A truncated 3 byte CJK character leaves invalid
 *   UTF-8 behind, which LVGL then renders as garbage.
 *
 ****************************************************************************/

static void utf8_copy(FAR char *dst, size_t dstlen, FAR const char *src)
{
  FAR const char *end;
  size_t n;

  if (dstlen == 0)
    {
      return;
    }

  if (src == NULL)
    {
      dst[0] = '\0';
      return;
    }

  n = strlen(src);
  if (n < dstlen)
    {
      memcpy(dst, src, n + 1);
      return;
    }

  /* Find the last character boundary that still fits. */

  end = src + dstlen - 1;
  while (end > src && (((unsigned char)*end & 0xc0) == 0x80))
    {
      end--;
    }

  n = (size_t)(end - src);
  memcpy(dst, src, n);
  dst[n] = '\0';
}

/****************************************************************************
 * Name: ls_lvgl_set_ai_text
 *
 * Description:
 *   Post an AI agent status and reply text to the panel subtitle.  Safe
 *   to call from any task (e.g. the ai_agent dispatch thread); the UI
 *   loop applies the update on its next tick.
 *
 * Input Parameters:
 *   status - Short status string ("thinking", "speaking", ...), or NULL
 *            to leave the status unchanged.
 *   text   - Reply text to display, or NULL to clear.
 *
 ****************************************************************************/

void ls_lvgl_set_actuator(FAR const char *name, bool on)
{
  int i;

  if (name == NULL) {
    return;
  }

  pthread_mutex_lock(&g_act_lock);

  for (i = 0; i < UI_NACT; i++)
    {
      if (strcmp(g_acts[i].name, name) == 0) {
        g_act_pending[i] = on;
        g_act_dirty = true;
        break;
      }
    }

  if (strcmp(name, "PWM BLUE") == 0) {
    g_pwm_taken = true;
    g_act_dirty = true;
  }

  pthread_mutex_unlock(&g_act_lock);
}

void ls_lvgl_set_ai_text(FAR const char *status, FAR const char *text)
{
  pthread_mutex_lock(&g_ai_lock);

  if (status != NULL)
    {
      utf8_copy(g_ai_status, AI_STATUS_LEN, status);
    }

  if (text != NULL)
    {
      utf8_copy(g_ai_text, AI_TEXT_LEN, text);
    }
  else
    {
      g_ai_text[0] = '\0';
    }

  g_ai_dirty = true;
  pthread_mutex_unlock(&g_ai_lock);
}

int main(int argc, FAR char *argv[])
{
  lv_nuttx_dsc_t info;
  lv_nuttx_result_t result;
  int key1;
  int key2;
  int tick = 0;
  bool prev1 = false;
  bool prev2 = false;

  printf("=== LS2K0300 LVGL Console ===\n");
  printf("KEY2(GPIO%d): next item   KEY1(GPIO%d): toggle\n",
         KEY2_GPIO, KEY1_GPIO);

  lv_init();
  lv_nuttx_dsc_init(&info);
  lv_nuttx_init(&info, &result);
  if (result.disp == NULL)
    {
      printf("ERROR: lv_nuttx_init failed (no /dev/fb0?)\n");
      return -ENODEV;
    }

#ifdef CONFIG_LV_USE_FREETYPE
  /* Optional CJK font for the AI subtitle.  Without the TTF on the card
   * this fails harmlessly and the subtitle falls back to Montserrat
   * (ASCII only).  Every failure path says why - a silent fall back here
   * just looks like "Chinese does not work".
   */

  /* lv_freetype_init() refuses to run twice and reports that as a failure,
   * but the context it leaves behind is fully usable.  A NuttX builtin's
   * statics outlive the task, so a second ls_lvgl in the same boot lands
   * here - and reusing that context is the right answer: the face ids are
   * looked up by path, so lv_freetype_font_create() below hits the same
   * cache node instead of leaking a second one.  Tearing the old context
   * down instead would free the caches that the labels built by the
   * previous run still point into, and the next allocation walks a
   * corrupted heap.
   */

  if (lv_freetype_init(128) != LV_RESULT_OK)
    {
      printf("NOTE: freetype was already initialized, reusing it\n");
    }
  else
    {
      int font_fd = open(AI_FONT_PATH, O_RDONLY);

      if (font_fd < 0)
        {
          printf("WARNING: %s not readable (%d), "
                 "AI subtitle limited to ASCII\n",
                 AI_FONT_PATH, errno);
        }
      else
        {
          off_t font_size = lseek(font_fd, 0, SEEK_END);

          close(font_fd);
          printf("CJK font: %s, %ld bytes\n",
                 AI_FONT_PATH, (long)font_size);

          g_ai_font = lv_freetype_font_create(AI_FONT_PATH,
                                              LV_FREETYPE_FONT_RENDER_MODE_BITMAP,
                                              20, LV_FREETYPE_FONT_STYLE_NORMAL);
          if (g_ai_font == NULL)
            {
              printf("WARNING: freetype could not load %s, "
                     "AI subtitle limited to ASCII\n",
                     AI_FONT_PATH);
            }
          else
            {
              lv_font_glyph_dsc_t gd;

              printf("CJK font loaded (line_height=%d base_line=%d)\n",
                     g_ai_font->line_height, g_ai_font->base_line);

              /* A font object that loads is not the same as one that can
               * draw Chinese: probe a real CJK codepoint so a broken
               * face is reported as such rather than as garbled text.
               */

              if (!lv_font_get_glyph_dsc(g_ai_font, &gd,
                                         (uint32_t)0x4e2d, 0)
                  || gd.is_placeholder)
                {
                  printf("WARNING: font has no glyph for U+4E2D "
                         "(wrong face or not a CJK font)\n");
                }
              else
                {
                  printf("CJK glyph ok: adv_w=%d box=%dx%d "
                         "resolved=%p\n",
                         (int)gd.adv_w,
                         (int)gd.box_w, (int)gd.box_h,
                         (const void *)gd.resolved_font);
                }
            }
        }
    }
#endif

  ui_build();

  g_rtc_fd = open("/dev/rtc0", O_RDONLY);
  g_thermal_fd = open("/dev/thermal0", O_RDONLY);
  g_adc_ok = (mcp3204_init() == 0);
  if (!g_adc_ok)
    {
      printf("WARNING: MCP3204 unavailable, ADC row stays --\n");
    }

  if (sensor_bus_open() == OK && bh1750_init() == OK)
    {
      g_light_ok = true;
    }
  else
    {
      printf("WARNING: BH1750 unavailable, LIGHT row stays --\n");
    }

  g_buzz_fd = act_gpio_open(BUZZER_GPIO, true);
  key1 = act_gpio_open(KEY1_GPIO, false);
  key2 = act_gpio_open(KEY2_GPIO, false);

  /* PWM2 on GPIO88: mux the pin to its alternate function and start. */

  {
    struct pinctrl_param_s param;
    int pfd = open("/dev/pinctrl0", O_RDWR);

    if (pfd >= 0)
      {
        memset(&param, 0, sizeof(param));
        param.pin = PWM_GPIO;
        param.para.function = PWM_FUNCTION;
        ioctl(pfd, PINCTRLC_SETFUNCTION, (unsigned long)(uintptr_t)&param);
        close(pfd);
      }
  }

  g_pwm_fd = open(PWM_DEVICE, O_RDONLY);
  if (g_pwm_fd >= 0)
    {
      struct pwm_info_s info;

      memset(&info, 0, sizeof(info));
      info.frequency = PWM_FREQUENCY;
      info.duty = 0;
      ioctl(g_pwm_fd, PWMIOC_SETCHARACTERISTICS,
            (unsigned long)(uintptr_t)&info);
      ioctl(g_pwm_fd, PWMIOC_START, 0);
    }

  /* The pins keep the level they powered up with, so start from a known
   * state: everything off. */

  act_all_off();

  /* The OLED is optional -- the console works without it. */

  {
    struct pinctrl_param_s param;
    int pfd = open("/dev/pinctrl0", O_RDWR);

    if (pfd >= 0)
      {
        memset(&param, 0, sizeof(param));
        param.pin = 50;
        param.para.function = 3;
        ioctl(pfd, PINCTRLC_SETFUNCTION, (unsigned long)(uintptr_t)&param);
        param.pin = 51;
        ioctl(pfd, PINCTRLC_SETFUNCTION, (unsigned long)(uintptr_t)&param);
        close(pfd);
      }
  }

  g_oled_fd = open(OLED_I2C_PATH, O_RDWR);
  if (g_oled_fd >= 0)
    {
      lvoled_init();
    }
  else
    {
      printf("WARNING: %s unavailable (%d), OLED mirror disabled\n",
             OLED_I2C_PATH, errno);
    }

  ui_select(0);
  printf("console up, Ctrl-C to stop\n");

  for (; ; )
    {
      struct rtc_time rtctime;
      bool now1;
      bool now2;

      /* Keys are active low. */

      now1 = key_pressed(key1);
      now2 = key_pressed(key2);

      if (now2 && !prev2)
        {
          ui_select(g_selected + 1);
        }

      /* KEY1 is both the row toggle and the microphone key: a tap
       * toggles the selected row, a hold starts listening and the
       * release ends the utterance.
       */

      if (now1 && !prev1)
        {
          ui_toggle_selected();
        }

      prev1 = now1;
      prev2 = now2;

      act_pwm_tick();
      act_buzzer_tick(tick++);

      /* Values refresh every 50 ticks (1 s); the RTC is cheap enough to
       * follow every tick.
       */

      if ((tick % 50) == 0)
        {
          ui_sample();
        }

      /* Apply actuator states posted by the agent's tools. */

      if (g_act_dirty)
        {
          bool pending[UI_NACT];
          bool taken;
          int i;

          pthread_mutex_lock(&g_act_lock);
          memcpy(pending, g_act_pending, sizeof(pending));
          taken = g_pwm_taken;
          g_act_dirty = false;
          pthread_mutex_unlock(&g_act_lock);

          for (i = 0; i < UI_NACT; i++)
            {
              if (g_state[i] == pending[i]) {
                continue;
              }

              g_state[i] = pending[i];

              if (g_rows[i].widget != NULL)
                {
                  if (pending[i]) {
                    lv_obj_add_state(g_rows[i].widget, LV_STATE_CHECKED);
                  } else {
                    lv_obj_clear_state(g_rows[i].widget, LV_STATE_CHECKED);
                  }
                }
            }

          /* The agent owns PWM2, so the panel must stop writing the duty
           * or the two would fight over the breathing animation.
           */

          if (taken) {
            g_pwm_on = false;
          }
        }

      /* Apply AI subtitle updates posted from other tasks. */

      if (g_ai_dirty)
        {
          char status[AI_STATUS_LEN];
          char text[AI_TEXT_LEN];

          pthread_mutex_lock(&g_ai_lock);
          memcpy(status, g_ai_status, AI_STATUS_LEN);
          memcpy(text, g_ai_text, AI_TEXT_LEN);
          g_ai_dirty = false;
          pthread_mutex_unlock(&g_ai_lock);

          lv_label_set_text(g_ai_status_label, status);
          lv_label_set_text(g_ai_text_label, text);
        }

      if (g_rtc_fd >= 0 &&
          ioctl(g_rtc_fd, RTC_RD_TIME,
                (unsigned long)(uintptr_t)&rtctime) == 0)
        {
          snprintf(g_rows[UI_NROWS - 1].value_buf, UI_VALUE_LEN,
                   "%02d:%02d:%02d",
                   rtctime.tm_hour, rtctime.tm_min, rtctime.tm_sec);
          lv_label_set_text(g_rows[UI_NROWS - 1].widget,
                            g_rows[UI_NROWS - 1].value_buf);
        }

      lv_timer_handler();
      usleep(TICK_MS * 1000);
    }

  /* Not reached. */

  lv_nuttx_deinit(&result);
  lv_deinit();
  return OK;
}
