/****************************************************************************
 * apps/examples/lvgldemo/ui_main.h
 *
 * 手语识别主界面（浅色卡片布局：左摄像头 / 右上识别结果 / 右下扬声器）。
 * 移植自 ESP-IDF 版 ui_main.c，仅保留纯 LVGL 逻辑。
 ****************************************************************************/

#ifndef __APPS_EXAMPLES_LVGLDEMO_UI_MAIN_H
#define __APPS_EXAMPLES_LVGLDEMO_UI_MAIN_H

#include "lvgl/lvgl.h"

/* 创建手语识别主界面 */
void ui_main_create(lv_obj_t *scr);

/* 获取摄像头预览 canvas（供 camera 模块写入画面），未创建时返回 NULL */
lv_obj_t *ui_main_get_camera_canvas(void);

/* 获取当前展示词汇（供语音播报使用） */
const char *ui_main_get_current_word(void);

/* 设置摄像头卡片右上角状态文字（供 camera 模块显示初始化状态机） */
void ui_main_set_camera_status(const char *text);

#endif /* __APPS_EXAMPLES_LVGLDEMO_UI_MAIN_H */