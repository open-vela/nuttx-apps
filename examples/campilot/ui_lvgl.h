/****************************************************************************
 * apps/examples/campilot/ui_lvgl.h
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

#ifndef __APPS_EXAMPLES_CAMPILOT_UI_LVGL_H
#define __APPS_EXAMPLES_CAMPILOT_UI_LVGL_H

/****************************************************************************
 * Public Function Prototypes
 ****************************************************************************/

/****************************************************************************
 * Name: ui_lvgl_main
 *
 * Description:
 *   Initialize LVGL over the framebuffer, register the GT911 touch shim as
 *   a pointer input device, render the three-screen card UI, and run the
 *   LVGL timer loop.  Does not return until the UI is torn down.
 *
 * Input Parameters:
 *   argc, argv - Standard main() arguments (currently unused).
 *
 * Returned Value:
 *   Zero (OK) on success; a negative value on failure.
 *
 ****************************************************************************/

int ui_lvgl_main(int argc, char *argv[]);

#endif /* __APPS_EXAMPLES_CAMPILOT_UI_LVGL_H */
