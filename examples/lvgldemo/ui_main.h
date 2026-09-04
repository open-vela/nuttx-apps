/****************************************************************************
 * apps/examples/lvgldemo/ui_main.h
 *
 * Sign-language recognition main screen (light card layout: camera on
 * the left, recognition result top-right, speaker button bottom-right).
 * Ported from the ESP-IDF version of ui_main.c; only the pure LVGL
 * logic is kept.
 *
 * SPDX-License-Identifier: Apache-2.0
 ****************************************************************************/

#ifndef __APPS_EXAMPLES_LVGLDEMO_UI_MAIN_H
#define __APPS_EXAMPLES_LVGLDEMO_UI_MAIN_H

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include "lvgl/lvgl.h"

/****************************************************************************
 * Public Function Prototypes
 ****************************************************************************/

/* Create the sign-language recognition main screen */

void ui_main_create(lv_obj_t *scr);

/* Get the camera preview canvas (for the camera module to draw into);
 * returns NULL when not created
 */

lv_obj_t *ui_main_get_camera_canvas(void);

/* Get the currently displayed word (for the voice broadcast) */

const char *ui_main_get_current_word(void);

/* Set the status text at the top-right of the camera card (used by the
 * camera module to show the init state machine)
 */

void ui_main_set_camera_status(const char *text);

#endif /* __APPS_EXAMPLES_LVGLDEMO_UI_MAIN_H */
