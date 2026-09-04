/****************************************************************************
 * apps/examples/lvgldemo/lvgldemo.c
 *
 * Licensed to the Apache Software Foundation (ASF) under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.  The
 * ASF licenses this file to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance with the
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
#include <unistd.h>
#include <sys/boardctl.h>

#include <lvgl/lvgl.h>
#include <lvgl/demos/lv_demos.h>
#ifdef CONFIG_LV_USE_NUTTX_LIBUV
#include <uv.h>
#endif

#include "boot_screen.h"
#include "ui_main.h"
#include "camera_module.h"

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/* Should we perform board-specific driver initialization? There are two
 * ways that board initialization can occur:  1) automatically via
 * board_late_initialize() during bootupif CONFIG_BOARD_LATE_INITIALIZE
 * or 2).
 * via a call to boardctl() if the interface is enabled
 * (CONFIG_BOARDCTL=y).
 * If this task is running as an NSH built-in application, then that
 * initialization has probably already been performed otherwise we do it
 * here.
 */

#undef NEED_BOARDINIT

#if defined(CONFIG_BOARDCTL) && !defined(CONFIG_NSH_ARCHINIT)
#  define NEED_BOARDINIT 1
#endif

#define BOOT_DURATION_MS 3000u

/* Starting-up status text (Chinese, \u-escaped) */

#define BOOT_STATUS_TEXT "\u6b63\u5728\u542f\u52a8"

/****************************************************************************
 * Private Data
 ****************************************************************************/

/* The LVGL watchdog in the official board bring-up (esp32p4_bringup.c)
 * references this heartbeat counter, which the demo main loop increments
 * on every iteration. The upstream board code does not provide the
 * symbol, so it is defined here.
 */

volatile uint32_t g_lvgl_heartbeat = 0;

/****************************************************************************
 * Private Functions
 ****************************************************************************/

#ifdef CONFIG_LV_USE_NUTTX_LIBUV
static void lv_nuttx_uv_loop(uv_loop_t *loop, lv_nuttx_result_t *result)
{
  lv_nuttx_uv_t uv_info;
  void *data;

  uv_loop_init(loop);

  lv_memset(&uv_info, 0, sizeof(uv_info));
  uv_info.loop = loop;
  uv_info.disp = result->disp;
  uv_info.indev = result->indev;
#ifdef CONFIG_UINPUT_TOUCH
  uv_info.uindev = result->utouch_indev;
#endif

#ifdef CONFIG_LV_USE_NUTTX_MOUSE
  uv_info.mouse_indev = result->mouse_indev;
#endif

  data = lv_nuttx_uv_init(&uv_info);
  uv_run(loop, UV_RUN_DEFAULT);
  lv_nuttx_uv_deinit(&data);
}
#endif

/****************************************************************************
 * Name: boot_screen_run
 *
 * Description:
 *   Show the boot screen (white background, openvela logo, progress
 *   bar) for BOOT_DURATION_MS. lv_timer_handler() must keep being
 *   called during this period, otherwise the progress bar will not
 *   move.
 ****************************************************************************/

static void boot_screen_run(void)
{
  static const char *const boot_dots[] =
  {
    BOOT_STATUS_TEXT,
    BOOT_STATUS_TEXT ".",
    BOOT_STATUS_TEXT "..",
    BOOT_STATUS_TEXT "...",
  };

  uint32_t t0 = lv_tick_get();

  boot_screen_show(lv_screen_active());

  while (lv_tick_get() - t0 < BOOT_DURATION_MS)
    {
      uint32_t dt = lv_tick_get() - t0;
      int pct = (int)((dt * 100u) / BOOT_DURATION_MS);

      boot_screen_update(pct, boot_dots[(dt / 300u) % 4]);

      lv_timer_handler();
      g_lvgl_heartbeat++;
      usleep(16 * 1000);
    }
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: main or lv_demos_main
 *
 * Description:
 *
 * Input Parameters:
 *   Standard argc and argv
 *
 * Returned Value:
 *   Zero on success; a positive, non-zero value on failure.
 *
 ****************************************************************************/

int main(int argc, FAR char *argv[])
{
  lv_nuttx_dsc_t info;
  lv_nuttx_result_t result;

#ifdef CONFIG_LV_USE_NUTTX_LIBUV
  uv_loop_t ui_loop;
  lv_memzero(&ui_loop, sizeof(ui_loop));
#endif

  if (lv_is_initialized())
    {
      LV_LOG_ERROR("LVGL already initialized! aborting.");
      return -1;
    }

#ifdef NEED_BOARDINIT
  /* Perform board-specific driver initialization */

  boardctl(BOARDIOC_INIT, 0);

#endif

  lv_init();

  lv_nuttx_dsc_init(&info);

#ifdef CONFIG_LV_USE_NUTTX_LCD
  info.fb_path = "/dev/lcd0";
#endif

#ifdef CONFIG_INPUT_TOUCHSCREEN
  info.input_path = CONFIG_EXAMPLES_LVGLDEMO_INPUT_DEVPATH;
#endif

  lv_nuttx_init(&info, &result);

  if (result.disp == NULL)
    {
      LV_LOG_ERROR("lv_demos initialization failure!");
      return 1;
    }

  /* Boot screen for about 3 seconds, then switch to the main UI */

  boot_screen_run();

  /* Enter the sign-language recognition main UI (light card layout) */

  ui_main_create(lv_screen_active());

  /* Start the camera: SC2336 init + CSI frame capture into the
   * preview canvas
   */

  if (camera_module_start(ui_main_get_camera_canvas()) != 0)
    {
      LV_LOG_WARN("camera module failed to start, continue without it");
    }

#ifdef CONFIG_LV_USE_NUTTX_LIBUV
  lv_nuttx_uv_loop(&ui_loop, &result);
#else
  while (1)
    {
      uint32_t idle;
      idle = lv_timer_handler();
      g_lvgl_heartbeat++;

      /* Minimum sleep of 1ms */

      idle = idle ? idle : 1;
      usleep(idle * 1000);
    }
#endif

demo_end:
  lv_nuttx_deinit(&result);
  lv_deinit();

  return 0;
}
