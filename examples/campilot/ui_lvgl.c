/****************************************************************************
 * apps/examples/campilot/ui_lvgl.c
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 ****************************************************************************/

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>

#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include <errno.h>
#include <time.h>
#include <unistd.h>

#include <lvgl/lvgl.h>

#if LV_USE_NUTTX
#  include <lvgl/src/drivers/nuttx/lv_nuttx_fbdev.h>
#endif

#include "ui_lvgl.h"
#include "ui_state_machine.h"

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#define UI_FB_PATH     "/dev/fb0"

/* Panel geometry and card layout.  Three cards are laid out horizontally
 * with a fixed gap; a swipe translates the card track by one card width.
 */

#define UI_CARD_W      1024
#define UI_CARD_H      600
#define UI_CARD_GAP    20
#define UI_SWIPE_MS    200
#define UI_MAX_POINTS  5

/* The board GT911 shim lives in the board source tree and is linked into
 * the flat build.  Forward-declare the interface here so the app does not
 * need to reach into the board source include path.  The struct below MUST
 * match gt911_board.h exactly.
 */

struct gt911_touch_s
{
  uint16_t x;
  uint16_t y;
  bool pressed;
};

#ifdef CONFIG_ESP32P4_GT911_SHIM
int gt911_board_init(void);
int gt911_board_poll(struct gt911_touch_s *touches, int max_points);
#endif

/* Prefer larger fonts when built in, otherwise fall back to the default
 * (Montserrat 14, always enabled).  The 7" 1024x600 panel needs a large
 * title font to be legible at arm's length.
 */

#if LV_FONT_MONTSERRAT_48
#  define UI_TITLE_FONT (&lv_font_montserrat_48)
#elif LV_FONT_MONTSERRAT_24
#  define UI_TITLE_FONT (&lv_font_montserrat_24)
#else
#  define UI_TITLE_FONT (LV_FONT_DEFAULT)
#endif

#if LV_FONT_MONTSERRAT_24
#  define UI_BODY_FONT (&lv_font_montserrat_24)
#else
#  define UI_BODY_FONT (LV_FONT_DEFAULT)
#endif

/****************************************************************************
 * Private Data
 ****************************************************************************/

static lv_display_t *g_disp;
static lv_indev_t   *g_indev;
static lv_obj_t     *g_track;
static lv_obj_t     *g_result_label;

static struct ui_state_s g_state;
static struct ui_event_queue_s g_events;

/* Gesture state shared between the indev read callback and the main loop. */

static struct
{
  bool down;
  int down_x;
  int down_y;
  int last_x;
  int last_y;
} g_gesture;

/****************************************************************************
 * Private Functions
 ****************************************************************************/

/****************************************************************************
 * Name: ui_lvgl_millis
 *
 * Description:
 *   LVGL tick source: monotonic clock in milliseconds.
 *
 ****************************************************************************/

static uint32_t ui_lvgl_millis(void)
{
  struct timespec ts;

  clock_gettime(CLOCK_MONOTONIC, &ts);
  return (uint32_t)(ts.tv_sec * 1000 + ts.tv_nsec / 1000000);
}

/****************************************************************************
 * Name: ui_gt911_read_cb
 *
 * Description:
 *   LVGL pointer input device read callback.  Polls the GT911 shim and
 *   runs gesture detection (swipe vs tap) on every touch lift, posting the
 *   resulting event to the UI event queue.
 *
 ****************************************************************************/

static void ui_gt911_read_cb(lv_indev_t *indev, lv_indev_data_t *data)
{
  (void)indev;

#ifdef CONFIG_ESP32P4_GT911_SHIM
  struct gt911_touch_s touches[UI_MAX_POINTS];
  int n = gt911_board_poll(touches, UI_MAX_POINTS);

  if (n > 0)
    {
      /* Finger down: remember the press origin once. */

      if (!g_gesture.down)
        {
          g_gesture.down = true;
          g_gesture.down_x = touches[0].x;
          g_gesture.down_y = touches[0].y;
        }

      g_gesture.last_x = touches[0].x;
      g_gesture.last_y = touches[0].y;

      data->point.x = touches[0].x;
      data->point.y = touches[0].y;
      data->state   = LV_INDEV_STATE_PRESSED;
    }
  else if (n == 0)
    {
      /* Lift: classify the down/up pair into an event. */

      if (g_gesture.down)
        {
          enum ui_event_e ev;

          ev = ui_sm_detect_gesture(g_gesture.down_x, g_gesture.down_y,
                                    g_gesture.last_x, g_gesture.last_y);
          if (ev != EVENT_NONE)
            {
              ui_eq_post(&g_events, ev);
            }
        }
      else
        {
          /* The press was missed (the GT911 lift is reported but the brief
           * down was not polled).  Treat the release itself as a tap so the
           * capture still triggers without relying on the press coordinate.
           */

          ui_eq_post(&g_events, EVENT_TAP);
        }

      g_gesture.down = false;
      data->point.x = g_gesture.last_x;
      data->point.y = g_gesture.last_y;
      data->state   = LV_INDEV_STATE_RELEASED;
    }
  else
    {
      /* -EAGAIN: no new data; report the last known state. */

      data->point.x = g_gesture.last_x;
      data->point.y = g_gesture.last_y;
      data->state   = g_gesture.down ? LV_INDEV_STATE_PRESSED
                                     : LV_INDEV_STATE_RELEASED;
    }
#else
  data->state = LV_INDEV_STATE_RELEASED;
#endif
}

/****************************************************************************
 * Name: ui_anim_x_cb
 *
 * Description:
 *   lv_anim exec callback that moves an object horizontally.
 *
 ****************************************************************************/

static void ui_anim_x_cb(void *var, int32_t value)
{
  lv_obj_set_x((lv_obj_t *)var, value);
}

/****************************************************************************
 * Name: ui_animate_to
 *
 * Description:
 *   Slide the card track so that the given screen is centered in the
 *   viewport, over UI_SWIPE_MS milliseconds.
 *
 ****************************************************************************/

static void ui_animate_to(enum ui_screen_e screen)
{
  lv_anim_t a;
  int32_t target = -(int32_t)screen * (UI_CARD_W + UI_CARD_GAP);

  lv_anim_init(&a);
  lv_anim_set_var(&a, g_track);
  lv_anim_set_exec_cb(&a, ui_anim_x_cb);
  lv_anim_set_values(&a, lv_obj_get_x(g_track), target);
  lv_anim_set_duration(&a, UI_SWIPE_MS);
  lv_anim_set_path_cb(&a, lv_anim_path_ease_out);
  lv_anim_start(&a);
}

/****************************************************************************
 * Name: ui_make_card
 *
 * Description:
 *   Create one card container on the track and return it.  The card fills
 *   the panel and is positioned at the given slot index.
 *
 ****************************************************************************/

static lv_obj_t *ui_make_card(int slot, lv_color_t bg)
{
  lv_obj_t *card = lv_obj_create(g_track);

  lv_obj_set_size(card, UI_CARD_W, UI_CARD_H);
  lv_obj_set_pos(card, slot * (UI_CARD_W + UI_CARD_GAP), 0);
  lv_obj_set_style_bg_color(card, bg, 0);
  lv_obj_set_style_bg_opa(card, LV_OPA_COVER, 0);
  lv_obj_set_style_radius(card, 16, 0);
  lv_obj_set_style_border_width(card, 0, 0);
  lv_obj_set_style_pad_all(card, 24, 0);
  lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);

  return card;
}

/****************************************************************************
 * Name: ui_make_title
 *
 * Description:
 *   Add a title label to a card.
 *
 ****************************************************************************/

static void ui_make_title(lv_obj_t *card, const char *title)
{
  lv_obj_t *label = lv_label_create(card);

  lv_label_set_text(label, title);
  lv_obj_set_style_text_font(label, UI_TITLE_FONT, 0);
  lv_obj_set_style_text_color(label, lv_color_white(), 0);
  lv_obj_align(label, LV_ALIGN_TOP_LEFT, 0, 16);
}

/****************************************************************************
 * Name: ui_btn_cb
 *
 * Description:
 *   Shared LVGL button click handler.  The event to post is carried in the
 *   event user-data and forwarded to the UI event queue, where the state
 *   machine drives the corresponding transition.
 *
 ****************************************************************************/

static void ui_btn_cb(lv_event_t *e)
{
  if (lv_event_get_code(e) == LV_EVENT_CLICKED)
    {
      enum ui_event_e ev =
        (enum ui_event_e)(intptr_t)lv_event_get_user_data(e);

      ui_eq_post(&g_events, ev);
    }
}

/****************************************************************************
 * Name: ui_add_button
 *
 * Description:
 *   Create a white button on the given card with a symbol + label and wire
 *   it to ui_btn_cb with the given event.
 *
 ****************************************************************************/

static lv_obj_t *ui_add_button(lv_obj_t *card, lv_align_t align,
                               lv_coord_t x_ofs, lv_coord_t y_ofs,
                               lv_coord_t w, lv_coord_t h,
                               const char *text, enum ui_event_e ev)
{
  lv_obj_t *btn = lv_button_create(card);
  lv_obj_t *label;

  lv_obj_set_size(btn, w, h);
  lv_obj_align(btn, align, x_ofs, y_ofs);
  lv_obj_set_style_bg_color(btn, lv_color_white(), 0);
  lv_obj_add_event_cb(btn, ui_btn_cb, LV_EVENT_CLICKED,
                      (void *)(intptr_t)ev);

  label = lv_label_create(btn);
  lv_label_set_text(label, text);
  lv_obj_set_style_text_font(label, UI_BODY_FONT, 0);
  lv_obj_center(label);

  return btn;
}

/****************************************************************************
 * Name: ui_create_camera_card
 *
 * Description:
 *   Build the left camera preview card.
 *
 ****************************************************************************/

static void ui_create_camera_card(void)
{
  lv_obj_t *card;
  lv_obj_t *hint;

  card = ui_make_card(0, lv_palette_main(LV_PALETTE_INDIGO));
  ui_make_title(card, LV_SYMBOL_VIDEO " CAMERA");

  hint = lv_label_create(card);
  lv_label_set_text(hint, "Tap screen to capture\n"
                          "Swipe left/right to change screen");
  lv_obj_set_style_text_color(hint, lv_color_white(), 0);
  lv_obj_set_style_text_font(hint, UI_BODY_FONT, 0);
  lv_obj_align(hint, LV_ALIGN_TOP_LEFT, 0, 140);
}

/****************************************************************************
 * Name: ui_create_result_card
 *
 * Description:
 *   Build the middle recognition-result card.  Stores the result text label
 *   in g_result_label for later updates.
 *
 ****************************************************************************/

static void ui_create_result_card(void)
{
  lv_obj_t *card;

  card = ui_make_card(1, lv_palette_main(LV_PALETTE_TEAL));
  ui_make_title(card, LV_SYMBOL_IMAGE " RESULT");

  g_result_label = lv_label_create(card);
  lv_label_set_long_mode(g_result_label, LV_LABEL_LONG_WRAP);
  lv_obj_set_width(g_result_label, UI_CARD_W - 48);
  lv_label_set_text(g_result_label, "(no result yet)");
  lv_obj_set_style_text_color(g_result_label, lv_color_white(), 0);
  lv_obj_set_style_text_font(g_result_label, UI_BODY_FONT, 0);
  lv_obj_align(g_result_label, LV_ALIGN_TOP_LEFT, 0, 140);

  /* "Again" button: re-capture (back to camera and shoot again). */

  ui_add_button(card, LV_ALIGN_BOTTOM_LEFT, 24, -24, 220, 56,
                LV_SYMBOL_REFRESH " Again", EVENT_TAP);

  /* "Speak" button: TTS playback (reserved for M5). */

  ui_add_button(card, LV_ALIGN_BOTTOM_RIGHT, -24, -24, 220, 56,
                LV_SYMBOL_PLAY " Speak", EVENT_TTS_START);
}

/****************************************************************************
 * Name: ui_create_voice_card
 *
 * Description:
 *   Build the right voice dialog card.
 *
 ****************************************************************************/

static void ui_create_voice_card(void)
{
  lv_obj_t *card;

  card = ui_make_card(2, lv_palette_main(LV_PALETTE_DEEP_ORANGE));
  ui_make_title(card, LV_SYMBOL_AUDIO " VOICE");

  /* "Hold to talk" button: ASR (reserved for M5). */

  ui_add_button(card, LV_ALIGN_CENTER, 0, -40, 320, 72,
                LV_SYMBOL_AUDIO " Hold to talk", EVENT_TTS_START);

  /* "Playback" button: TTS (reserved for M5). */

  ui_add_button(card, LV_ALIGN_CENTER, 0, 80, 220, 56,
                LV_SYMBOL_VOLUME_MAX " Playback", EVENT_TTS_START);
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: ui_lvgl_main
 ****************************************************************************/

int ui_lvgl_main(int argc, char *argv[])
{
  lv_obj_t *screen;

  (void)argc;
  (void)argv;

#if !LV_USE_NUTTX
  printf("[ui] LVGL built without LV_USE_NUTTX; cannot open %s\n", UI_FB_PATH);
  return -ENOTSUP;
#else
  if (lv_is_initialized())
    {
      printf("[ui] LVGL already initialized, aborting\n");
      return -1;
    }

  ui_sm_init(&g_state);
  ui_eq_init(&g_events);
  memset(&g_gesture, 0, sizeof(g_gesture));

  /* Initialize the GT911 shim; the UI still runs without touch if the
   * controller is absent so the cards can be verified visually.
   */

#ifdef CONFIG_ESP32P4_GT911_SHIM
  if (gt911_board_init() < 0)
    {
      printf("[ui] GT911 init failed, continuing without touch\n");
    }
#endif

  /* Create the display over the framebuffer. */

  lv_init();
  lv_tick_set_cb(ui_lvgl_millis);

  g_disp = lv_nuttx_fbdev_create();
  if (g_disp == NULL)
    {
      printf("[ui] fbdev display create failed\n");
      return -1;
    }

  if (lv_nuttx_fbdev_set_file(g_disp, UI_FB_PATH) != 0)
    {
      printf("[ui] fbdev open %s failed\n", UI_FB_PATH);
      lv_display_delete(g_disp);
      g_disp = NULL;
      return -1;
    }

#ifdef CONFIG_ESP32P4_GT911_SHIM
  g_indev = lv_indev_create();
  if (g_indev != NULL)
    {
      lv_indev_set_type(g_indev, LV_INDEV_TYPE_POINTER);
      lv_indev_set_read_cb(g_indev, ui_gt911_read_cb);
    }
#endif

  /* Build the three-card track on the active screen. */

  screen  = lv_screen_active();
  g_track = lv_obj_create(screen);
  lv_obj_set_size(g_track, 3 * UI_CARD_W + 2 * UI_CARD_GAP, UI_CARD_H);
  lv_obj_set_pos(g_track, 0, 0);
  lv_obj_set_style_pad_all(g_track, 0, 0);
  lv_obj_set_style_bg_opa(g_track, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(g_track, 0, 0);
  lv_obj_clear_flag(g_track, LV_OBJ_FLAG_SCROLLABLE);

  ui_create_camera_card();
  ui_create_result_card();
  ui_create_voice_card();

  for (; ; )
    {
      enum ui_event_e ev;
      enum ui_screen_e before;
      enum ui_screen_e after;
      uint32_t idle;

      /* Drain pending events and drive the state machine. */

      while (ui_eq_trywait(&g_events, &ev) == 0)
        {
          before = g_state.screen;
          after  = ui_sm_transition(&g_state, ev);
          if (after != before)
            {
              ui_animate_to(after);
            }
        }

      /* Stub capture: a tap on the camera screen sets capturing; complete
       * the flow with a canned result until the real camera->MiMo pipeline
       * is wired in.
       */

      if (g_state.capturing)
        {
          snprintf(g_state.last_result, sizeof(g_state.last_result),
                   "Recognition result (placeholder)\n"
                   "Tap to capture again");
          ui_eq_post(&g_events, EVENT_CAPTURE_DONE);
          ui_eq_post(&g_events, EVENT_RECOGNITION_DONE);
        }

      /* Keep the result label in sync with the current screen. */

      if (g_state.screen == SCREEN_RESULT)
        {
          lv_label_set_text(g_result_label, g_state.last_result);
        }

      /* Render and process input, then sleep until the next tick. */

      idle = lv_timer_handler();
      usleep((idle > 0 ? idle : 1) * 1000);
    }

  return 0;
#endif
}
