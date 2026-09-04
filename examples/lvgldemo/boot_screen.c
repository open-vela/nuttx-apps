/****************************************************************************
 * apps/examples/lvgldemo/boot_screen.c
 *
 * 开机界面：白底 + 居中 openvela logo（RGB565 图像数据）+ 底部进度条。
 * 主循环通过 boot_screen_update() 推进进度与"正在启动"状态文字。
 ****************************************************************************/

#include "boot_screen.h"
#include "logo_data.h"

#include <stdint.h>

LV_FONT_DECLARE(zh_font_16);

/* 与主界面一致的浅色主题配色 */
#define C_PROGRESS_BG   lv_color_hex(0xe5e7eb)
#define C_PROGRESS_BAR  lv_color_hex(0x3b82f6)
#define C_STATUS_TEXT   lv_color_hex(0x6b7280)

#define BOOT_BAR_W      420
#define BOOT_BAR_H      8
#define BOOT_BAR_Y      220   /* 相对屏幕中心的纵向偏移 */
#define BOOT_TEXT_Y     185

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

void boot_screen_show(lv_obj_t *scr)
{
  lv_obj_t *img;

  lv_obj_set_style_bg_color(scr, lv_color_white(), 0);
  lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);

  /* 居中 openvela logo，略向上留出底部进度区 */
  img = lv_image_create(scr);
  lv_image_set_src(img, &boot_logo);
  lv_obj_align(img, LV_ALIGN_CENTER, 0, -40);

  /* 底部进度条 */
  s_progress = lv_bar_create(scr);
  lv_obj_remove_style_all(s_progress);
  lv_obj_set_size(s_progress, BOOT_BAR_W, BOOT_BAR_H);
  lv_obj_align(s_progress, LV_ALIGN_CENTER, 0, BOOT_BAR_Y);
  lv_obj_set_style_bg_color(s_progress, C_PROGRESS_BG, 0);
  lv_obj_set_style_bg_opa(s_progress, LV_OPA_COVER, 0);
  lv_obj_set_style_radius(s_progress, BOOT_BAR_H / 2, 0);
  lv_obj_set_style_bg_color(s_progress, C_PROGRESS_BAR, LV_PART_INDICATOR);
  lv_obj_set_style_bg_opa(s_progress, LV_OPA_COVER, LV_PART_INDICATOR);
  lv_obj_set_style_radius(s_progress, BOOT_BAR_H / 2, LV_PART_INDICATOR);
  lv_bar_set_range(s_progress, 0, 100);
  lv_bar_set_value(s_progress, 0, LV_ANIM_OFF);

  /* 状态文字（进度条上方） */
  s_status = lv_label_create(scr);
  lv_obj_set_style_text_color(s_status, C_STATUS_TEXT, 0);
  lv_obj_set_style_text_font(s_status, &zh_font_16, 0);
  lv_label_set_text(s_status, "正在启动");
  lv_obj_align(s_status, LV_ALIGN_CENTER, 0, BOOT_TEXT_Y);
}

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
