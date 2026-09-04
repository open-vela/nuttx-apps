/****************************************************************************
 * apps/examples/lvgldemo/ui_main.c
 *
 * 手语识别主界面：浅色卡片布局。
 *   左  ：摄像头预览卡片（占位，camera 模块接入后写画面）
 *   右上：识别结果卡片
 *   右下：扬声器按钮卡片（点击播放提示音）
 * 移植自 ESP-IDF 版 ui_main.c，仅保留纯 LVGL 逻辑。
 ****************************************************************************/

#include "ui_main.h"
#include "lvgl/lvgl.h"
#include "logo_data.h"
#include <string.h>

/* 中文字体 + FontAwesome 图标（由 lv_font_conv 生成，含 fallback） */
LV_FONT_DECLARE(zh_font_16);
LV_FONT_DECLARE(zh_font_40);

/* 扬声器提示音入口（由 audio 模块实现；未实现时静默） */
extern void board_audio_beep(void);

/* 摄像头预览 canvas（camera 模块写入画面） */
static lv_obj_t *s_cam_canvas = NULL;
static lv_obj_t *s_cam_status = NULL;

/* 常用手语词汇（识别结果演示轮换） */
static const char *const g_vocab[] =
{
  "你好", "谢谢", "对不起", "再见", "辛苦了",
  "请问", "请稍等", "可以", "好的", "我爱你",
};
#define VOCAB_NUM (sizeof(g_vocab) / sizeof(g_vocab[0]))

static lv_obj_t *s_word_label = NULL;
static lv_obj_t *s_conf_label = NULL;
static lv_obj_t *s_speaker_btn = NULL;
static lv_obj_t *s_speaker_icon = NULL;
static lv_obj_t *s_speaker_label = NULL;
static uint8_t s_vocab_idx = 0;

lv_obj_t *ui_main_get_camera_canvas(void)
{
  return s_cam_canvas;
}

const char *ui_main_get_current_word(void)
{
  return g_vocab[s_vocab_idx % VOCAB_NUM];
}

void ui_main_set_camera_status(const char *text)
{
  if (s_cam_status != NULL && text != NULL)
    {
      lv_label_set_text(s_cam_status, text);
    }
}

/* ---- 浅色主题配色 ---- */
#define C_BG           lv_color_hex(0xf2f4f8)
#define C_CARD         lv_color_hex(0xffffff)
#define C_CARD_CAM     lv_color_hex(0xeef1f6)
#define C_PRIMARY      lv_color_hex(0x3b82f6)
#define C_PRIMARY_DIM  lv_color_hex(0x2563eb)
#define C_TEXT         lv_color_hex(0x1f2937)
#define C_TEXT_DIM     lv_color_hex(0x6b7280)
#define C_OK           lv_color_hex(0x10b981)
#define C_BORDER       lv_color_hex(0xe5e7eb)

/* ---- 几何 ---- */
#define UI_MARGIN   20
#define UI_GAP      20
#define CAM_W       600
#define CAM_H       560
#define SIDE_X      (UI_MARGIN + CAM_W + UI_GAP)
#define SIDE_W      (1024 - UI_MARGIN - SIDE_X)
#define RESULT_H    350
#define SPEAKER_Y   (UI_MARGIN + RESULT_H + UI_GAP)
#define SPEAKER_H   (UI_MARGIN + CAM_H - SPEAKER_Y)

/* 通用卡片样式 */
static void style_card(lv_obj_t *obj)
{
  lv_obj_remove_style_all(obj);
  lv_obj_set_style_bg_color(obj, C_CARD, 0);
  lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, 0);
  lv_obj_set_style_radius(obj, 18, 0);
  lv_obj_set_style_shadow_width(obj, 14, 0);
  lv_obj_set_style_shadow_color(obj, lv_color_hex(0x94a3b8), 0);
  lv_obj_set_style_shadow_opa(obj, LV_OPA_30, 0);
  lv_obj_set_style_border_width(obj, 1, 0);
  lv_obj_set_style_border_color(obj, C_BORDER, 0);
}

static void card_title(lv_obj_t *parent, const char *text, int y)
{
  lv_obj_t *bar = lv_obj_create(parent);
  lv_obj_remove_style_all(bar);
  lv_obj_set_style_bg_color(bar, C_PRIMARY, 0);
  lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, 0);
  lv_obj_set_style_radius(bar, 2, 0);
  lv_obj_set_size(bar, 5, 22);
  lv_obj_set_pos(bar, 20, y + 2);

  lv_obj_t *lbl = lv_label_create(parent);
  lv_obj_set_style_text_color(lbl, C_TEXT, 0);
  lv_obj_set_style_text_font(lbl, &zh_font_16, 0);
  lv_label_set_text(lbl, text);
  lv_obj_set_pos(lbl, 34, y);
}

/* 摄像头预览卡片 */
static void create_camera_card(lv_obj_t *scr)
{
  lv_obj_t *card = lv_obj_create(scr);
  style_card(card);
  lv_obj_set_pos(card, UI_MARGIN, UI_MARGIN);
  lv_obj_set_size(card, CAM_W, CAM_H);

  card_title(card, "摄像头", 18);

  lv_obj_t *status = lv_label_create(card);
  lv_obj_set_style_text_color(status, C_OK, 0);
  lv_obj_set_style_text_font(status, &zh_font_16, 0);
  lv_label_set_text(status, LV_SYMBOL_BULLET " 待机");
  lv_obj_align(status, LV_ALIGN_TOP_RIGHT, -20, 18);
  s_cam_status = status;

  /* 显示区 */
  lv_obj_t *disp = lv_obj_create(card);
  lv_obj_remove_style_all(disp);
  lv_obj_set_style_bg_color(disp, C_CARD_CAM, 0);
  lv_obj_set_style_bg_opa(disp, LV_OPA_COVER, 0);
  lv_obj_set_style_radius(disp, 14, 0);
  lv_obj_set_pos(disp, 20, 56);
  lv_obj_set_size(disp, CAM_W - 40, CAM_H - 56 - 86);

  /* 摄像头预览 canvas（camera 模块写入 RGB565 帧） */
  s_cam_canvas = lv_canvas_create(disp);
  lv_obj_remove_style_all(s_cam_canvas);
  lv_obj_set_size(s_cam_canvas, CAM_W - 40, CAM_H - 56 - 86);
  lv_obj_center(s_cam_canvas);

  /* 无画面时的占位提示 */
  lv_obj_t *cam_tip = lv_label_create(disp);
  lv_obj_set_style_text_color(cam_tip, C_TEXT_DIM, 0);
  lv_obj_set_style_text_font(cam_tip, &zh_font_16, 0);
  lv_label_set_text(cam_tip, "摄像头画面区域");
  lv_obj_align(cam_tip, LV_ALIGN_BOTTOM_MID, 0, -24);

  /* 底部提示条 */
  lv_obj_t *bar = lv_obj_create(card);
  lv_obj_remove_style_all(bar);
  lv_obj_set_style_bg_color(bar, C_CARD_CAM, 0);
  lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, 0);
  lv_obj_set_style_radius(bar, 12, 0);
  lv_obj_set_pos(bar, 20, CAM_H - 66);
  lv_obj_set_size(bar, CAM_W - 40, 46);

  lv_obj_t *tip = lv_label_create(bar);
  lv_obj_set_style_text_color(tip, C_TEXT_DIM, 0);
  lv_obj_set_style_text_font(tip, &zh_font_16, 0);
  lv_label_set_text(tip, LV_SYMBOL_WARNING " 请对准镜头，保持手部清晰");
  lv_obj_center(tip);
}

/* 识别结果卡片 */
static void create_result_card(lv_obj_t *scr)
{
  lv_obj_t *card = lv_obj_create(scr);
  style_card(card);
  lv_obj_set_pos(card, SIDE_X, UI_MARGIN);
  lv_obj_set_size(card, SIDE_W, RESULT_H);

  card_title(card, "识别结果", 18);

  lv_obj_t *word = lv_label_create(card);
  lv_obj_set_style_text_color(word, C_TEXT, 0);
  lv_obj_set_style_text_font(word, &zh_font_40, 0);
  lv_label_set_text(word, g_vocab[0]);
  lv_obj_align(word, LV_ALIGN_CENTER, 0, -24);
  s_word_label = word;

  /* 置信度进度条 */
  lv_obj_t *bar = lv_bar_create(card);
  lv_obj_remove_style_all(bar);
  lv_obj_set_size(bar, SIDE_W - 60, 10);
  lv_obj_set_style_bg_color(bar, lv_color_hex(0xe5e7eb), 0);
  lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, 0);
  lv_obj_set_style_radius(bar, 5, 0);
  lv_obj_align(bar, LV_ALIGN_BOTTOM_MID, 0, -58);
  lv_obj_set_style_bg_color(bar, C_PRIMARY, LV_PART_INDICATOR);
  lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, LV_PART_INDICATOR);
  lv_obj_set_style_radius(bar, 5, LV_PART_INDICATOR);
  lv_bar_set_range(bar, 0, 100);
  lv_bar_set_value(bar, 96, LV_ANIM_ON);

  lv_obj_t *conf = lv_label_create(card);
  lv_obj_set_style_text_color(conf, C_TEXT_DIM, 0);
  lv_obj_set_style_text_font(conf, &zh_font_16, 0);
  lv_label_set_text(conf, "置信度 96%");
  lv_obj_align(conf, LV_ALIGN_BOTTOM_MID, 0, -32);
  s_conf_label = conf;
}

/* 扬声器按钮点击回调：播放提示音 + 显示播报反馈 */
static void speaker_click_cb(lv_event_t *e)
{
  lv_obj_t *btn = lv_event_get_target(e);
  lv_obj_t *icon = lv_obj_get_child(btn, 0);

  /* 播放提示音（真实扬声器输出） */
  board_audio_beep();

  if (strcmp(lv_label_get_text(icon), LV_SYMBOL_VOLUME_MAX) == 0)
    {
      lv_label_set_text(icon, LV_SYMBOL_MUTE);
      lv_obj_set_style_bg_color(btn, C_TEXT_DIM, 0);
      lv_obj_set_style_bg_grad_color(btn, lv_color_hex(0x9aa3b0), 0);
      lv_label_set_text(s_speaker_label, "已静音 · 再次点击恢复播报");
    }
  else
    {
      lv_label_set_text(icon, LV_SYMBOL_VOLUME_MAX);
      lv_obj_set_style_bg_color(btn, C_PRIMARY, 0);
      lv_obj_set_style_bg_grad_color(btn, C_PRIMARY_DIM, 0);
      lv_label_set_text(s_speaker_label, "已播报识别结果");
    }
}

/* 词汇轮换定时器：每 2 秒切换识别结果词汇 */
static void vocab_timer_cb(lv_timer_t *timer)
{
  s_vocab_idx = (s_vocab_idx + 1) % VOCAB_NUM;
  if (s_word_label != NULL)
    {
      lv_label_set_text(s_word_label, g_vocab[s_vocab_idx]);
    }

  if (s_conf_label != NULL)
    {
      char buf[32];
      snprintf(buf, sizeof(buf), "置信度 %d%%",
               88 + (s_vocab_idx * 3) % 10);
      lv_label_set_text(s_conf_label, buf);
    }
}

/* 扬声器按钮卡片 */
static void create_speaker_card(lv_obj_t *scr)
{
  lv_obj_t *card = lv_obj_create(scr);
  style_card(card);
  lv_obj_set_pos(card, SIDE_X, SPEAKER_Y);
  lv_obj_set_size(card, SIDE_W, SPEAKER_H);

  card_title(card, "语音播报", 18);

  /* 圆形扬声器按钮（带点击反馈） */
  lv_obj_t *btn = lv_obj_create(card);
  lv_obj_remove_style_all(btn);
  lv_obj_set_size(btn, 76, 76);
  lv_obj_set_style_bg_color(btn, C_PRIMARY, 0);
  lv_obj_set_style_bg_grad_color(btn, C_PRIMARY_DIM, 0);
  lv_obj_set_style_bg_grad_dir(btn, LV_GRAD_DIR_VER, 0);
  lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, 0);
  lv_obj_set_style_radius(btn, 38, 0);
  lv_obj_set_style_shadow_width(btn, 16, 0);
  lv_obj_set_style_shadow_color(btn, C_PRIMARY, 0);
  lv_obj_set_style_shadow_opa(btn, LV_OPA_40, 0);
  lv_obj_set_style_bg_color(btn, lv_color_hex(0x1e40af), LV_STATE_PRESSED);
  lv_obj_set_style_transform_scale(btn, 230, LV_STATE_PRESSED);
  lv_obj_set_style_shadow_width(btn, 6, LV_STATE_PRESSED);
  lv_obj_add_flag(btn, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_align(btn, LV_ALIGN_CENTER, 0, -8);

  lv_obj_t *icon = lv_label_create(btn);
  lv_obj_set_style_text_color(icon, lv_color_white(), 0);
  lv_obj_set_style_text_font(icon, &zh_font_16, 0);
  lv_label_set_text(icon, LV_SYMBOL_VOLUME_MAX);
  lv_obj_center(icon);
  s_speaker_icon = icon;

  lv_obj_t *label = lv_label_create(card);
  lv_obj_set_style_text_color(label, C_TEXT_DIM, 0);
  lv_obj_set_style_text_font(label, &zh_font_16, 0);
  lv_label_set_text(label, "点击播报识别结果");
  lv_obj_align(label, LV_ALIGN_BOTTOM_MID, 0, -14);
  s_speaker_label = label;

  lv_obj_add_event_cb(btn, speaker_click_cb, LV_EVENT_CLICKED, NULL);
}

/* 创建主界面 */
void ui_main_create(lv_obj_t *scr)
{
  /* 清掉开机界面残留 */
  lv_obj_clean(scr);

  lv_obj_set_style_bg_color(scr, C_BG, 0);
  lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);

  create_camera_card(scr);
  create_result_card(scr);
  create_speaker_card(scr);

  /* 词汇轮换演示（每 2 秒切换） */
  lv_timer_create(vocab_timer_cb, 2000, NULL);
}