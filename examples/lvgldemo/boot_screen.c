/****************************************************************************
 * apps/examples/lvgldemo/boot_screen.c
 *
 * Boot screen: white background, centered openvela logo (RGB565 data)
 * and a progress bar at the bottom. The main loop calls
 * boot_screen_update() to advance the progress and the status text.
 ****************************************************************************/

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <stdint.h>

#include "boot_screen.h"
#include "logo_data.h"

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/* Light theme colors, consistent with the main screen */

#define C_PROGRESS_BG   lv_color_hex(0xe5e7eb)
#define C_PROGRESS_BAR  lv_color_hex(0x3b82f6)
#define C_STATUS_TEXT   lv_color_hex(0x6b7280)

#define BOOT_BAR_W      420
#define BOOT_BAR_H      8
#define BOOT_BAR_Y      220  /* vertical offset from the screen center */
#define BOOT_TEXT_Y     185

/* Starting-up status text (Chinese, \u-escaped below) */

#define BOOT_STATUS_TEXT "\u6b63\u5728\u542f\u52a8"

/****************************************************************************
 * Private Data
 ****************************************************************************/

LV_FONT_DECLARE(zh_font_16);

static lv_obj_t *s_progress = NULL;
static lv_obj_t *s_status   = NULL;

static const lv_image_dsc_t boot_logo =
{
  .header =
    {
      .magic   = LV_IMAGE_HEADER_MAGIC,
      .cf      = LV_COLOR_FORMAT_RGB565,
      .w       = LOGO_W,
      .h       = LOGO_H,
      .stride  = LOGO_W * 2,
    },
  .data_size = LOGO_W * LOGO_H * 2,
  .data = (const uint8_t *)logo_data,
};

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: boot_screen_show
 *
 * Description:
 *   Draw the boot screen (logo + progress bar + status label) on the
 *   given screen object.
 *
 * Input Parameters:
 *   scr - The LVGL screen object to draw onto.
 *
 ****************************************************************************/

void boot_screen_show(lv_obj_t *scr)
{
  lv_obj_t *img;

  lv_obj_set_style_bg_color(scr, lv_color_white(), 0);
  lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);

  /* Centered openvela logo, shifted up to leave room for the bar */

  img = lv_image_create(scr);
  lv_image_set_src(img, &boot_logo);
  lv_obj_align(img, LV_ALIGN_CENTER, 0, -40);

  /* Bottom progress bar */

  s_progress = lv_bar_create(scr);
  lv_obj_remove_style_all(s_progress);
  lv_obj_set_size(s_progress, BOOT_BAR_W, BOOT_BAR_H);
  lv_obj_align(s_progress, LV_ALIGN_CENTER, 0, BOOT_BAR_Y);
  lv_obj_set_style_bg_color(s_progress, C_PROGRESS_BG, 0);
  lv_obj_set_style_bg_opa(s_progress, LV_OPA_COVER, 0);
  lv_obj_set_style_radius(s_progress, BOOT_BAR_H / 2, 0);
  lv_obj_set_style_bg_color(s_progress, C_PROGRESS_BAR,
                            LV_PART_INDICATOR);
  lv_obj_set_style_bg_opa(s_progress, LV_OPA_COVER, LV_PART_INDICATOR);
  lv_obj_set_style_radius(s_progress, BOOT_BAR_H / 2,
                          LV_PART_INDICATOR);
  lv_bar_set_range(s_progress, 0, 100);
  lv_bar_set_value(s_progress, 0, LV_ANIM_OFF);

  /* Status label above the progress bar */

  s_status = lv_label_create(scr);
  lv_obj_set_style_text_color(s_status, C_STATUS_TEXT, 0);
  lv_obj_set_style_text_font(s_status, &zh_font_16, 0);
  lv_label_set_text(s_status, BOOT_STATUS_TEXT);
  lv_obj_align(s_status, LV_ALIGN_CENTER, 0, BOOT_TEXT_Y);
}

/****************************************************************************
 * Name: boot_screen_update
 *
 * Description:
 *   Advance the boot progress bar and refresh the status text.
 *
 * Input Parameters:
 *   percent - Progress percentage (clamped to 0..100; negative clamps
 *             to 0).
 *   status  - New status text, or NULL to keep the current text.
 *
 ****************************************************************************/

void boot_screen_update(int percent, const char *status)
{
  if (s_progress != NULL)
    {
      if (percent < 0)
        {
          percent = 0;
        }
      else if (percent > 100)
        {
          percent = 100;
        }

      lv_bar_set_value(s_progress, percent, LV_ANIM_OFF);
    }

  if (s_status != NULL && status != NULL)
    {
      lv_label_set_text(s_status, status);
    }
}
