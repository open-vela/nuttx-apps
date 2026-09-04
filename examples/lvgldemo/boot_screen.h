/****************************************************************************
 * apps/examples/lvgldemo/boot_screen.h
 *
 * Boot screen: white background with a centered openvela logo and a
 * progress bar at the bottom.
 *
 * SPDX-License-Identifier: Apache-2.0
 ****************************************************************************/

#ifndef __APPS_EXAMPLES_LVGLDEMO_BOOT_SCREEN_H
#define __APPS_EXAMPLES_LVGLDEMO_BOOT_SCREEN_H

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include "lvgl/lvgl.h"

/****************************************************************************
 * Public Function Prototypes
 ****************************************************************************/

/* Show the boot screen (must be called after LVGL initialization) */

void boot_screen_show(lv_obj_t *scr);

/* Advance the boot progress: percent 0-100, status is the status text
 * (pass NULL to keep the current text)
 */

void boot_screen_update(int percent, const char *status);

#endif /* __APPS_EXAMPLES_LVGLDEMO_BOOT_SCREEN_H */
