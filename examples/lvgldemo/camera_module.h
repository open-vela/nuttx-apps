/****************************************************************************
 * apps/examples/lvgldemo/camera_module.h
 *
 * 摄像头模块：SC2336(MIPI-CSI) 采集并显示到 LVGL 预览 canvas。
 * 提供帧捕获API供AI推理使用。
 ****************************************************************************/

#ifndef __APPS_EXAMPLES_LVGLDEMO_CAMERA_MODULE_H
#define __APPS_EXAMPLES_LVGLDEMO_CAMERA_MODULE_H

#include "lvgl/lvgl.h"

/****************************************************************************
 * 帧数据结构
 ****************************************************************************/

struct camera_frame_s
{
  const uint8_t *data;   /* 帧数据指针（RAW8格式） */
  uint32_t width;        /* 帧宽度 */
  uint32_t height;       /* 帧高度 */
  uint32_t size;         /* 帧数据大小（字节） */
  uint32_t frame_count;  /* 帧计数 */
};

/* 帧回调函数类型：当新帧到来时调用 */
typedef void (*camera_frame_callback_t)(const struct camera_frame_s *frame,
                                        void *user_data);

/****************************************************************************
 * Public Function Prototypes
 ****************************************************************************/

/* 初始化传感器 + CSI 并启动取帧显示。canvas 为预览画布。返回 0 成功。 */
int camera_module_start(lv_obj_t *canvas);
void camera_module_stop(void);

/* 获取当前最新帧数据（用于AI推理）。
 * 返回帧数据指针，如果无新帧返回NULL。
 * 调用者不应free返回的指针，该指针在下次调用前有效。
 */
const struct camera_frame_s *camera_module_get_frame(void);

/* 注册帧回调函数：当新帧到来时自动调用。
 * 用于AI推理模块注册回调，实现异步处理。
 */
int camera_module_register_frame_callback(camera_frame_callback_t callback,
                                          void *user_data);

/* 注销帧回调函数 */
void camera_module_unregister_frame_callback(void);

/* 检查是否有新帧可用 */
bool camera_module_has_new_frame(void);

#endif /* __APPS_EXAMPLES_LVGLDEMO_CAMERA_MODULE_H */
