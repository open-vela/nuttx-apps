/****************************************************************************
 * apps/examples/lvgldemo/camera_module.h
 *
 * Camera module: SC2336 (MIPI-CSI) capture and display into the LVGL
 * preview canvas. Provides a frame-capture API for AI inference.
 *
 * SPDX-License-Identifier: Apache-2.0
 ****************************************************************************/

#ifndef __APPS_EXAMPLES_LVGLDEMO_CAMERA_MODULE_H
#define __APPS_EXAMPLES_LVGLDEMO_CAMERA_MODULE_H

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include "lvgl/lvgl.h"

/****************************************************************************
 * Public Types
 ****************************************************************************/

/* Captured frame */

struct camera_frame_s
{
  const uint8_t *data;   /* Frame data pointer (RAW8) */
  uint32_t width;        /* Frame width */
  uint32_t height;       /* Frame height */
  uint32_t size;         /* Frame size in bytes */
  uint32_t frame_count;  /* Frame counter */
};

/* Frame callback type: called whenever a new frame arrives */

typedef void (*camera_frame_callback_t)(const struct camera_frame_s *frame,
                                        void *user_data);

/****************************************************************************
 * Public Function Prototypes
 ****************************************************************************/

/* Initialize the sensor + CSI and start capture/display.  canvas is the
 * preview canvas.  Returns 0 on success.
 */

int camera_module_start(lv_obj_t *canvas);
void camera_module_stop(void);

/* Get the latest frame (for AI inference).  Returns the frame pointer,
 * or NULL when no new frame is available.  The caller must not free the
 * returned pointer; it stays valid until the next call.
 */

const struct camera_frame_s *camera_module_get_frame(void);

/* Register a frame callback: invoked automatically on every new frame.
 * Used by the AI inference module for asynchronous processing.
 */

int camera_module_register_frame_callback(camera_frame_callback_t callback,
                                          void *user_data);

/* Unregister the frame callback */

void camera_module_unregister_frame_callback(void);

/* Check whether a new frame is available */

bool camera_module_has_new_frame(void);

#endif /* __APPS_EXAMPLES_LVGLDEMO_CAMERA_MODULE_H */
