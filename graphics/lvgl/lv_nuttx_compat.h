/* SPDX-License-Identifier: Apache-2.0 */

#ifndef __APPS_GRAPHICS_LVGL_LV_NUTTX_COMPAT_H
#define __APPS_GRAPHICS_LVGL_LV_NUTTX_COMPAT_H

/* LVGL 9.2.1's NuttX image-cache source calls gettid() without including
 * NuttX's declaration.  The generic <unistd.h> selected by this old Make
 * build can be the host header, which leaves an external gettid() reference.
 * Use the NuttX pthread identity API instead; the value is only used to make
 * the private image-cache heap name unique.
 */

#include <pthread.h>

#define gettid() ((pid_t)pthread_self())

#endif /* __APPS_GRAPHICS_LVGL_LV_NUTTX_COMPAT_H */
