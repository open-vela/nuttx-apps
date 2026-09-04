/****************************************************************************
 * apps/examples/lvgldemo/camera_module.c
 *
 * 摄像头模块：SC2336(MIPI-CSI) 采集并显示到 LVGL 预览 canvas。
 *
 *  - SC2336 通过 /dev/i2c0 初始化（RAW8 1280x720 30fps，2 lane，24MHz，
 *    336Mbps —— 与 ESP-IDF 已验证配置一致）
 *  - 帧经芯片侧 esp32p4_mipi_csi 驱动 DMA 进 PSRAM 双缓冲
 *  - LVGL 定时器轮询新帧：裁剪到预览区宽高比，最近邻缩放为灰度 RGB565
 *
 * SPDX-License-Identifier: Apache-2.0
 ****************************************************************************/

#include <nuttx/config.h>
#include <nuttx/i2c/i2c_master.h>

#include <debug.h>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include "lvgl/lvgl.h"
#include "camera_module.h"
#include "ui_main.h"

/****************************************************************************
 * 芯片侧 CSI 驱动声明（内核编译，这里 extern 引用）
 ****************************************************************************/

struct esp32p4_mipi_csi_config_s
{
  uint32_t h_res;
  uint32_t v_res;
  uint32_t lanes_num;
  uint32_t lane_bit_rate_mbps;
  uint32_t in_bpp;
};

typedef void (*esp32p4_mipi_csi_frame_cb_t)(void *buf, size_t len, void *arg);

extern int esp32p4_mipi_csi_initialize(
  const struct esp32p4_mipi_csi_config_s *cfg);
extern int esp32p4_mipi_csi_start(esp32p4_mipi_csi_frame_cb_t frame_cb,
                                  void *arg);
extern int esp32p4_mipi_csi_stop(void);
extern uint32_t esp32p4_mipi_csi_frame_count(void);
extern size_t esp32p4_mipi_csi_framelen(void);
extern void *esp32p4_mipi_csi_get_frame(void);

/****************************************************************************
 * SC2336 寄存器表（ESP-IDF esp_cam_sensor 已验证，Apache-2.0）
 ****************************************************************************/

#define SC2336_REG_SLEEP_MODE  0x0100
#define SC2336_REG_SOFTWARE_RST 0x0103
#define SC2336_REG_END         0xffff

static const struct
{
  uint16_t reg;
  uint8_t  val;
} sc2336_mipi_2lane_24Minput_1280x720_raw8_30fps[] =
{
  { 0x0103, 0x01 },
  { 0x0100, 0x00 },
  { 0x36e9, 0x80 },
  { 0x37f9, 0x80 },
  { 0x301f, 0x8e },
  { 0x3031, 0x08 },
  { 0x3037, 0x00 },
  { 0x3106, 0x05 },
  { 0x3200, 0x01 },
  { 0x3201, 0x34 },
  { 0x3202, 0x00 },
  { 0x3203, 0xb4 },
  { 0x3204, 0x06 },
  { 0x3205, 0x53 },
  { 0x3206, 0x03 },
  { 0x3207, 0x8b },
  { 0x3208, 0x05 },
  { 0x3209, 0x00 },
  { 0x320a, 0x02 },
  { 0x320b, 0xd0 },
  { 0x320c, 0x08 },
  { 0x320d, 0xc0 },
  { 0x320e, 0x04 },
  { 0x320f, 0xe2 },
  { 0x3210, 0x00 },
  { 0x3211, 0x10 },
  { 0x3212, 0x00 },
  { 0x3213, 0x04 },
  { 0x3248, 0x04 },
  { 0x3249, 0x0b },
  { 0x3253, 0x08 },
  { 0x3301, 0x09 },
  { 0x3302, 0xff },
  { 0x3303, 0x10 },
  { 0x3306, 0x68 },
  { 0x3307, 0x02 },
  { 0x330a, 0x01 },
  { 0x330b, 0x18 },
  { 0x330c, 0x16 },
  { 0x330d, 0xd3 },
  { 0x3318, 0x02 },
  { 0x3321, 0x0a },
  { 0x3327, 0x0e },
  { 0x332b, 0x12 },
  { 0x3333, 0x10 },
  { 0x3334, 0x40 },
  { 0x335e, 0x06 },
  { 0x335f, 0x0a },
  { 0x3364, 0x1f },
  { 0x337c, 0x02 },
  { 0x337d, 0x0e },
  { 0x3390, 0x09 },
  { 0x3391, 0x0f },
  { 0x3392, 0x1f },
  { 0x3393, 0x20 },
  { 0x3394, 0x20 },
  { 0x3395, 0xff },
  { 0x33a2, 0x04 },
  { 0x33b1, 0x80 },
  { 0x33b2, 0x68 },
  { 0x33b3, 0x42 },
  { 0x33f9, 0x78 },
  { 0x33fb, 0xe0 },
  { 0x33fc, 0x0f },
  { 0x33fd, 0x1f },
  { 0x349f, 0x03 },
  { 0x34a6, 0x0f },
  { 0x34a7, 0x1f },
  { 0x34a8, 0x42 },
  { 0x34a9, 0x06 },
  { 0x34aa, 0x01 },
  { 0x34ab, 0x28 },
  { 0x34ac, 0x01 },
  { 0x34ad, 0x90 },
  { 0x3630, 0xf4 },
  { 0x3633, 0x22 },
  { 0x3639, 0xf4 },
  { 0x363c, 0x47 },
  { 0x3670, 0x09 },
  { 0x3674, 0xf4 },
  { 0x3675, 0xfb },
  { 0x3676, 0xed },
  { 0x367c, 0x09 },
  { 0x367d, 0x0f },
  { 0x3690, 0x22 },
  { 0x3691, 0x22 },
  { 0x3692, 0x22 },
  { 0x3698, 0x89 },
  { 0x3699, 0x96 },
  { 0x369a, 0xd0 },
  { 0x369b, 0xd0 },
  { 0x369c, 0x09 },
  { 0x369d, 0x0f },
  { 0x36a2, 0x09 },
  { 0x36a3, 0x0f },
  { 0x36a4, 0x1f },
  { 0x36d0, 0x01 },
  { 0x36ea, 0x0e },
  { 0x36eb, 0x0a },
  { 0x36ec, 0x1a },
  { 0x36ed, 0x18 },
  { 0x3722, 0xe1 },
  { 0x3724, 0x41 },
  { 0x3725, 0xc1 },
  { 0x3728, 0x20 },
  { 0x37fa, 0x15 },
  { 0x37fb, 0x32 },
  { 0x37fc, 0x11 },
  { 0x37fd, 0x17 },
  { 0x3900, 0x0d },
  { 0x3905, 0x98 },
  { 0x391b, 0x81 },
  { 0x391c, 0x10 },
  { 0x3933, 0x81 },
  { 0x3934, 0xc5 },
  { 0x3940, 0x68 },
  { 0x3941, 0x00 },
  { 0x3942, 0x01 },
  { 0x3943, 0xc6 },
  { 0x3952, 0x02 },
  { 0x3953, 0x0f },
  { 0x3e01, 0x37 },
  { 0x3e02, 0xe0 },
  { 0x3e08, 0x1f },
  { 0x3e1b, 0x14 },
  { 0x4509, 0x38 },
  { 0x4819, 0x06 },
  { 0x481b, 0x04 },
  { 0x481d, 0x0c },
  { 0x481f, 0x03 },
  { 0x4821, 0x0a },
  { 0x4823, 0x03 },
  { 0x4825, 0x03 },
  { 0x4827, 0x03 },
  { 0x4829, 0x05 },
  { 0x5799, 0x06 },
  { 0x5ae0, 0xfe },
  { 0x5ae1, 0x40 },
  { 0x5ae2, 0x30 },
  { 0x5ae3, 0x28 },
  { 0x5ae4, 0x20 },
  { 0x5ae5, 0x30 },
  { 0x5ae6, 0x28 },
  { 0x5ae7, 0x20 },
  { 0x5ae8, 0x3c },
  { 0x5ae9, 0x30 },
  { 0x5aea, 0x28 },
  { 0x5aeb, 0x3c },
  { 0x5aec, 0x30 },
  { 0x5aed, 0x28 },
  { 0x5aee, 0xfe },
  { 0x5aef, 0x40 },
  { 0x5af4, 0x30 },
  { 0x5af5, 0x28 },
  { 0x5af6, 0x20 },
  { 0x5af7, 0x30 },
  { 0x5af8, 0x28 },
  { 0x5af9, 0x20 },
  { 0x5afa, 0x3c },
  { 0x5afb, 0x30 },
  { 0x5afc, 0x28 },
  { 0x5afd, 0x3c },
  { 0x5afe, 0x30 },
  { 0x5aff, 0x28 },
  { 0x36e9, 0x54 },
  { 0x37f9, 0x54 },
  { SC2336_REG_END, 0x00 },
};

#define SC2336_TABLE_SIZE \
  (sizeof(sc2336_mipi_2lane_24Minput_1280x720_raw8_30fps) / \
   sizeof(sc2336_mipi_2lane_24Minput_1280x720_raw8_30fps[0]))

/****************************************************************************
 * Private Data
 ****************************************************************************/

#define SC2336_I2C_BUS   "/dev/i2c0"
#define SC2336_I2C_ADDR  0x30
#define SC2336_I2C_FREQ  400000

#define CAM_W   1280
#define CAM_H   720
#define VIEW_W  560
#define VIEW_H  418

static int g_i2c_fd = -1;
static lv_obj_t *s_canvas = NULL;
static uint16_t *s_view_buf = NULL;
static lv_timer_t *s_poll_timer = NULL;
static uint32_t s_last_frames = 0;
static bool s_running = false;
static pthread_t s_init_thread;

/* 帧捕获相关变量 */
static struct camera_frame_s s_current_frame;
static camera_frame_callback_t s_frame_callback = NULL;
static void *s_frame_callback_arg = NULL;
static bool s_new_frame_available = false;

/* 摄像头初始化状态机（便于定位卡在哪一步 / 在屏幕上显示） */
typedef enum
{
  CAM_STATE_IDLE = 0,
  CAM_STATE_SENSOR,    /* SC2336 I2C 初始化 */
  CAM_STATE_CSI_INIT,  /* MIPI-CSI 驱动初始化 */
  CAM_STATE_CSI_START, /* MIPI-CSI 启动 + 分配帧缓冲 */
  CAM_STATE_STREAMING, /* 取帧显示中 */
  CAM_STATE_ERROR,     /* 初始化失败 */
} cam_state_t;

static volatile cam_state_t s_state = CAM_STATE_IDLE;
static const char *s_fail_step = NULL;  /* 失败的具体步骤（sensor/csi_init/...） */
static int s_fail_rc = 0;               /* 失败的错误码/寄存器 */
static int s_fail_errno = 0;            /* 失败时的 errno */

static const char *cam_state_str(cam_state_t s)
{
  switch (s)
    {
      case CAM_STATE_IDLE:     return "IDLE";
      case CAM_STATE_SENSOR:   return "SENSOR";
      case CAM_STATE_CSI_INIT: return "CSI_INIT";
      case CAM_STATE_CSI_START:return "CSI_START";
      case CAM_STATE_STREAMING:return "STREAMING";
      case CAM_STATE_ERROR:    return "ERROR";
      default:                 return "?";
    }
}

/* 状态进度直接写到 UART0（/dev/ttyS0），绕过自循环的 USB 控制台，LA 可抓 */
static void cam_trace(const char *step, int rc)
{
  int fd = open("/dev/ttyS0", O_WRONLY | O_NONBLOCK);
  char buf[96];
  int n;

  if (fd < 0)
    {
      return;
    }

  n = snprintf(buf, sizeof(buf), "[CAM] %s -> %s rc=%d\n",
               step, cam_state_str(s_state), rc);
  if (n > 0)
    {
      write(fd, buf, n);
    }

  close(fd);
}

/****************************************************************************
 * Private Functions
 ****************************************************************************/

static int sc2336_write_reg(uint16_t reg, uint8_t val)
{
  struct i2c_msg_s msg;
  struct i2c_transfer_s xfer;
  uint8_t buf[3];
  int ret;

  buf[0] = (uint8_t)(reg >> 8);
  buf[1] = (uint8_t)reg;
  buf[2] = val;

  msg.frequency = SC2336_I2C_FREQ;
  msg.addr      = SC2336_I2C_ADDR;
  msg.flags     = 0;
  msg.buffer    = buf;
  msg.length    = 3;

  xfer.msgv = &msg;
  xfer.msgc = 1;

  ret = ioctl(g_i2c_fd, I2CIOC_TRANSFER, (unsigned long)&xfer);
  if (ret < 0)
    {
      _err("SC2336: i2c write reg 0x%04x failed: %d\n", reg, errno);
      cam_trace("i2c_wr", reg | (errno << 16));
    }

  return ret;
}

/* 读 SC2336 寄存器（SCCB：写 2 字节寄存器地址，再读 1 字节） */
static int sc2336_read_reg(uint16_t reg, uint8_t *val)
{
  struct i2c_msg_s msg[2];
  struct i2c_transfer_s xfer;
  uint8_t addr[2];
  int ret;

  addr[0] = (uint8_t)(reg >> 8);
  addr[1] = (uint8_t)reg;

  msg[0].frequency = SC2336_I2C_FREQ;
  msg[0].addr      = SC2336_I2C_ADDR;
  msg[0].flags     = I2C_M_NOSTOP;  /* SCCB：写完地址不释放总线，直接续读 */
  msg[0].buffer    = addr;
  msg[0].length    = 2;

  msg[1].frequency = SC2336_I2C_FREQ;
  msg[1].addr      = SC2336_I2C_ADDR;
  msg[1].flags     = I2C_M_READ;
  msg[1].buffer    = val;
  msg[1].length    = 1;

  xfer.msgv = msg;
  xfer.msgc = 2;

  ret = ioctl(g_i2c_fd, I2CIOC_TRANSFER, (unsigned long)&xfer);
  if (ret < 0)
    {
      _err("SC2336: i2c read reg 0x%04x failed: %d\n", reg, errno);
    }

  return ret;
}

/* 写完整初始化表并开流 */
static int sc2336_init(void)
{
  size_t i;
  int ret;

  g_i2c_fd = open(SC2336_I2C_BUS, O_RDWR);
  if (g_i2c_fd < 0)
    {
      _err("SC2336: open %s failed: %d\n", SC2336_I2C_BUS, errno);
      cam_trace("i2c_open", errno);
      s_fail_step = "open";
      s_fail_rc = errno;
      return -1;
    }

  /* 读取芯片 ID（0x3107=0xcb, 0x3108=0x3a），确认传感器在总线上 */
  {
    uint8_t id_hi = 0;
    uint8_t id_lo = 0;
    int id_ok;

    ret = sc2336_read_reg(0x3107, &id_hi);
    if (ret >= 0)
      {
        ret = sc2336_read_reg(0x3108, &id_lo);
      }

    id_ok = (ret >= 0 && id_hi == 0xcb && id_lo == 0x3a);
    cam_trace("chip_id", (id_hi << 8) | id_lo);
    if (!id_ok)
      {
        _err("SC2336: bad chip id 0x%02x%02x (ret=%d)\n", id_hi, id_lo, ret);
        s_fail_step = "chip_id";
        s_fail_rc = (id_hi << 8) | id_lo;
        return -1;
      }
  }

  /* 软复位(0x0103)后必须等传感器重新稳定（模拟/时钟域重新上电），
   * 否则后续 0x0100 写会 NACK/卡总线。ESP-IDF 依赖完整上电时序，
   * 这里显式等待 50ms（此前因无等待导致 0x0100 失败）。 */
  ret = sc2336_write_reg(SC2336_REG_SOFTWARE_RST, 0x01);
  if (ret < 0)
    {
      cam_trace("i2c_wr", SC2336_REG_SOFTWARE_RST);
      s_fail_step = "i2c_wr";
      s_fail_rc = SC2336_REG_SOFTWARE_RST;
      s_fail_errno = errno;
      return -1;
    }

  usleep(50 * 1000);

  /* 从表第二项开始写（表首就是刚写过的软复位 0x0103） */
  for (i = 1; i < SC2336_TABLE_SIZE; i++)
    {
      uint16_t reg = sc2336_mipi_2lane_24Minput_1280x720_raw8_30fps[i].reg;
      uint8_t val = sc2336_mipi_2lane_24Minput_1280x720_raw8_30fps[i].val;

      if (reg == SC2336_REG_END)
        {
          break;
        }

      ret = sc2336_write_reg(reg, val);
      if (ret < 0)
        {
          cam_trace("i2c_wr", reg);
          s_fail_step = "i2c_wr";
          s_fail_rc = reg;
          s_fail_errno = errno;
          return -1;
        }

      /* 每个寄存器写之间给 ~10ms settle（SC2336 模拟/时序寄存器需要较长时间） */
      usleep(10 * 1000);
    }

  /* 开流（从 sleep 进入 streaming） */
  ret = sc2336_write_reg(SC2336_REG_SLEEP_MODE, 0x01);
  if (ret < 0)
    {
      cam_trace("stream_on", ret);
      s_fail_step = "stream";
      s_fail_rc = ret;
      return -1;
    }

  _info("SC2336: 1280x720 RAW8 streaming started\n");
  return 0;
}

/* LVGL 定时器：有新帧则裁剪缩放为灰度并刷新 canvas */
static void camera_poll_cb(lv_timer_t *timer)
{
  const uint8_t *src;
  uint16_t *dst;
  uint32_t frames;
  uint32_t oy;

  (void)timer;

  frames = esp32p4_mipi_csi_frame_count();
  if (frames == s_last_frames)
    {
      return;
    }

  s_last_frames = frames;
  src = (const uint8_t *)esp32p4_mipi_csi_get_frame();
  if (src == NULL)
    {
      return;
    }

  /* 裁剪到预览区宽高比（560/418 ≈ 1.34），横向居中 */
  const uint32_t crop_w = 965;
  const uint32_t crop_x = 157;

  for (oy = 0; oy < VIEW_H; oy++)
    {
      uint32_t sy = (oy * CAM_H) / VIEW_H;
      const uint8_t *row = src + (size_t)sy * CAM_W + crop_x;
      uint32_t ox;

      dst = s_view_buf + (size_t)oy * VIEW_W;
      for (ox = 0; ox < VIEW_W; ox++)
        {
          uint32_t sx = (ox * crop_w) / VIEW_W;
          uint8_t v = row[sx];
          uint16_t gray = (uint16_t)(((v >> 3) << 11) |
                                     ((v >> 2) << 5) |
                                     (v >> 3));
          dst[ox] = gray;
        }
    }

  lv_obj_invalidate(s_canvas);

  /* 更新帧数据结构供AI推理使用 */
  s_current_frame.data = src;
  s_current_frame.width = CAM_W;
  s_current_frame.height = CAM_H;
  s_current_frame.size = CAM_W * CAM_H;
  s_current_frame.frame_count = frames;
  s_new_frame_available = true;

  /* 调用帧回调（如果已注册） */
  if (s_frame_callback != NULL)
    {
      s_frame_callback(&s_current_frame, s_frame_callback_arg);
    }
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/* 在 LVGL 线程中设置 canvas 缓冲并创建轮询定时器（lv_async_call 回调） */
static void camera_display_setup(void *user_data)
{
  (void)user_data;

  if (s_canvas == NULL || s_view_buf == NULL)
    {
      return;
    }

  lv_canvas_set_buffer(s_canvas, (void *)s_view_buf, VIEW_W, VIEW_H,
                       LV_COLOR_FORMAT_RGB565);
  s_last_frames = 0;
  s_poll_timer = lv_timer_create(camera_poll_cb, 30, NULL);
  if (s_poll_timer == NULL)
    {
      _err("CAM: create poll timer failed\n");
      return;
    }

  s_running = true;
  _info("CAM: camera started (%ux%u RAW8 -> %ux%u view)\n",
        CAM_W, CAM_H, VIEW_W, VIEW_H);
}

/* 独立线程做阻塞式初始化（传感器 I2C / CSI 时钟 / DMA）。
 * 状态机：IDLE -> SENSOR -> CSI_INIT -> CSI_START -> STREAMING / ERROR。
 * 即使某步挂起，也不会阻塞 LVGL 主线程与心跳，系统照常运行。
 */
static void *camera_init_thread(void *arg)
{
  struct esp32p4_mipi_csi_config_s csi_cfg;
  int ret;

  (void)arg;

  /* 预览缓冲（PSRAM/堆，CPU 读写，无 DMA） */
  s_view_buf = (uint16_t *)malloc(VIEW_W * VIEW_H * 2);
  if (s_view_buf == NULL)
    {
      s_state = CAM_STATE_ERROR;
      s_fail_step = "malloc";
      s_fail_rc = -1;
      cam_trace("malloc", -1);
      return NULL;
    }

  /* 状态：SC2336 传感器初始化 */
  s_state = CAM_STATE_SENSOR;
  cam_trace("sensor", 0);
  ret = sc2336_init();
  if (ret != 0)
    {
      s_state = CAM_STATE_ERROR;
      cam_trace("sensor", ret);
      free(s_view_buf);
      s_view_buf = NULL;
      return NULL;
    }

  /* 状态：CSI 驱动初始化（RAW8 1280x720, 2 lane, 336Mbps） */
  s_state = CAM_STATE_CSI_INIT;
  cam_trace("csi_init", 0);
  memset(&csi_cfg, 0, sizeof(csi_cfg));
  csi_cfg.h_res              = CAM_W;
  csi_cfg.v_res              = CAM_H;
  csi_cfg.lanes_num          = 2;
  csi_cfg.lane_bit_rate_mbps = 336;
  csi_cfg.in_bpp             = 8;
  ret = esp32p4_mipi_csi_initialize(&csi_cfg);
  if (ret != OK)
    {
      s_state = CAM_STATE_ERROR;
      s_fail_step = "csi_init";
      s_fail_rc = ret;
      cam_trace("csi_init", ret);
      free(s_view_buf);
      s_view_buf = NULL;
      return NULL;
    }

  /* 状态：CSI 启动（分配 PSRAM 双缓冲 + DMA 开始） */
  s_state = CAM_STATE_CSI_START;
  cam_trace("csi_start", 0);
  ret = esp32p4_mipi_csi_start(NULL, NULL);
  if (ret != OK)
    {
      s_state = CAM_STATE_ERROR;
      s_fail_step = "csi_start";
      s_fail_rc = ret;
      cam_trace("csi_start", ret);
      return NULL;
    }

  /* 状态：流式取帧中 */
  s_state = CAM_STATE_STREAMING;
  cam_trace("streaming", 0);

  /* 初始化完成：交给 LVGL 线程设置显示 */
  lv_async_call(camera_display_setup, NULL);
  return NULL;
}

/* LVGL 定时器：把摄像头状态机文本刷新到屏幕右上角状态标签 */
static void camera_status_cb(lv_timer_t *timer)
{
  char buf[48];

  (void)timer;

  if (s_canvas == NULL)
    {
      return;
    }

  if (s_state == CAM_STATE_ERROR && s_fail_step != NULL)
    {
      if (s_fail_errno != 0)
        {
          snprintf(buf, sizeof(buf), "%s %s@%s r%d e%d",
                   LV_SYMBOL_WARNING, cam_state_str(s_state),
                   s_fail_step, s_fail_rc, s_fail_errno);
        }
      else
        {
          snprintf(buf, sizeof(buf), "%s %s@%s rc=%d",
                   LV_SYMBOL_WARNING, cam_state_str(s_state),
                   s_fail_step, s_fail_rc);
        }
    }
  else
    {
      snprintf(buf, sizeof(buf), "%s %s",
               (s_state == CAM_STATE_STREAMING) ?
               LV_SYMBOL_BULLET : LV_SYMBOL_WARNING,
               cam_state_str(s_state));
    }

  ui_main_set_camera_status(buf);
}

int camera_module_start(lv_obj_t *canvas)
{
  if (s_running)
    {
      return 0;
    }

  s_canvas = canvas;
  if (s_canvas == NULL)
    {
      return -1;
    }

  /* 状态显示定时器（LVGL 线程内创建，刷新初始化状态机到屏幕） */
  lv_timer_create(camera_status_cb, 250, NULL);

  /* 非阻塞：初始化在独立线程中执行，避免挂起卡死主界面 */
  if (pthread_create(&s_init_thread, NULL, camera_init_thread, NULL) != 0)
    {
      _err("CAM: failed to create init thread\n");
      return -1;
    }

  return 0;
}

void camera_module_stop(void)
{
  if (!s_running)
    {
      return;
    }

  if (s_poll_timer != NULL)
    {
      lv_timer_delete(s_poll_timer);
      s_poll_timer = NULL;
    }

  esp32p4_mipi_csi_stop();

  if (g_i2c_fd >= 0)
    {
      close(g_i2c_fd);
      g_i2c_fd = -1;
    }

  if (s_view_buf != NULL)
    {
      free(s_view_buf);
      s_view_buf = NULL;
    }

  s_running = false;
}

const struct camera_frame_s *camera_module_get_frame(void)
{
  if (!s_running || !s_new_frame_available)
    {
      return NULL;
    }

  s_new_frame_available = false;
  return &s_current_frame;
}

int camera_module_register_frame_callback(camera_frame_callback_t callback,
                                          void *user_data)
{
  if (callback == NULL)
    {
      return -EINVAL;
    }

  s_frame_callback = callback;
  s_frame_callback_arg = user_data;
  return 0;
}

void camera_module_unregister_frame_callback(void)
{
  s_frame_callback = NULL;
  s_frame_callback_arg = NULL;
}

bool camera_module_has_new_frame(void)
{
  return s_running && s_new_frame_available;
}
