/****************************************************************************
 * apps/examples/lvgldemo/boot_screen.h
 *
 * 开机界面：白底 + 居中 openvela logo + 底部进度条。
 ****************************************************************************/

#ifndef __APPS_EXAMPLES_LVGLDEMO_BOOT_SCREEN_H
#define __APPS_EXAMPLES_LVGLDEMO_BOOT_SCREEN_H

#include "lvgl/lvgl.h"

/* 显示开机界面（须在 LVGL 初始化后调用） */
void boot_screen_show(lv_obj_t *scr);

/* 推进开机进度：percent 0-100，status 为状态文字（可 NULL 保持原样） */
void boot_screen_update(int percent, const char *status);

#endif /* __APPS_EXAMPLES_LVGLDEMO_BOOT_SCREEN_H */
