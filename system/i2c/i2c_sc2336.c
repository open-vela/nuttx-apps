/****************************************************************************
 * apps/system/i2c/i2c_sc2336.c
 *
 * SPDX-License-Identifier: Apache-2.0
 ****************************************************************************/

#include <nuttx/config.h>

#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <poll.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include <nuttx/i2c/i2c_master.h>
#include <nuttx/compiler.h>

#ifdef CONFIG_VIDEO_STREAM
#  include <nuttx/video/imgdata.h>
#  include <nuttx/video/imgsensor.h>
#  include <nuttx/video/v4l2_cap.h>
#  include <nuttx/video/video.h>
#  include <nuttx/wqueue.h>
#endif

#include "esp_cache.h"
#include "esp_private/esp_cache_private.h"
#include "esp_heap_caps.h"
#include "esp_ldo_regulator.h"
#include "esp_private/esp_clk_tree_common.h"
#include "esp_private/periph_ctrl.h"
#include "hal/dw_gdma_hal.h"
#include "hal/dw_gdma_ll.h"
#include "hal/mipi_csi_brg_ll.h"
#include "hal/mipi_csi_hal.h"
#include "hal/mipi_csi_ll.h"
#include "hal/mipi_csi_host_ll.h"

#include "i2ctool.h"

#define SC2336_I2C_BUS       0
#define SC2336_I2C_ADDR      0x30
#define SC2336_SENSOR_ID_H   0x3107
#define SC2336_SENSOR_ID_L   0x3108
#define SC2336_EXPECTED_PID  0xcb3a
#define SC2336_REG_END       0xffff
#define SC2336_REG_TEST_PATTERN 0x4501
#define SC2336_FRAME_BYTES   2592000
#define SC2336_DMA_ITEMS     324000
#define SC2336_DMA_CHANNEL   0
#define SC2336_CAPTURE_TIMEOUT_MS 2000
#define SC2336_CSI_LDO_CHAN_ID 3
#define SC2336_CSI_LDO_VOLTAGE_MV 2500
#define SC2336_VIDEO_DEVPATH  "/dev/video0"
#define SC2336_VIDEO_DMA_CHANNEL 1

struct sc2336_regval_s
{
  uint16_t reg;
  uint8_t value;
};

/*
 * 1920x1080 RAW10, 24 MHz input, 2-lane MIPI, 405 Mbps/lane.
 * The sequence is derived from Espressif's Apache-2.0 SC2336 reference
 * configuration.  It deliberately leaves 0x0100 at 0x00 (stream off).
 */
static const struct sc2336_regval_s g_sc2336_1080p30_regs[] =
{
  {0x0103, 0x01}, {0x0100, 0x00}, {0x36e9, 0x80}, {0x37f9, 0x80},
  {0x301f, 0x02}, {0x3106, 0x05}, {0x320c, 0x08}, {0x320d, 0xca},
  {0x320e, 0x04}, {0x320f, 0xb0}, {0x3248, 0x04}, {0x3249, 0x0b},
  {0x3253, 0x08}, {0x3301, 0x09}, {0x3302, 0xff}, {0x3303, 0x10},
  {0x3306, 0x60}, {0x3307, 0x02}, {0x330a, 0x01}, {0x330b, 0x10},
  {0x330c, 0x16}, {0x330d, 0xff}, {0x3318, 0x02}, {0x3321, 0x0a},
  {0x3327, 0x0e}, {0x332b, 0x12}, {0x3333, 0x10}, {0x3334, 0x40},
  {0x335e, 0x06}, {0x335f, 0x0a}, {0x3364, 0x1f}, {0x337c, 0x02},
  {0x337d, 0x0e}, {0x3390, 0x09}, {0x3391, 0x0f}, {0x3392, 0x1f},
  {0x3393, 0x20}, {0x3394, 0x20}, {0x3395, 0xff}, {0x33a2, 0x04},
  {0x33b1, 0x80}, {0x33b2, 0x68}, {0x33b3, 0x42}, {0x33f9, 0x70},
  {0x33fb, 0xd0}, {0x33fc, 0x0f}, {0x33fd, 0x1f}, {0x349f, 0x03},
  {0x34a6, 0x0f}, {0x34a7, 0x1f}, {0x34a8, 0x42}, {0x34a9, 0x06},
  {0x34aa, 0x01}, {0x34ab, 0x23}, {0x34ac, 0x01}, {0x34ad, 0x84},
  {0x3630, 0xf4}, {0x3633, 0x22}, {0x3639, 0xf4}, {0x363c, 0x47},
  {0x3670, 0x09}, {0x3674, 0xf4}, {0x3675, 0xfb}, {0x3676, 0xed},
  {0x367c, 0x09}, {0x367d, 0x0f}, {0x3690, 0x33}, {0x3691, 0x33},
  {0x3692, 0x43}, {0x3698, 0x89}, {0x3699, 0x96}, {0x369a, 0xd0},
  {0x369b, 0xd0}, {0x369c, 0x09}, {0x369d, 0x0f}, {0x36a2, 0x09},
  {0x36a3, 0x0f}, {0x36a4, 0x1f}, {0x36d0, 0x01}, {0x36ea, 0x09},
  {0x36eb, 0x0c}, {0x36ec, 0x1c}, {0x36ed, 0x28}, {0x3722, 0xe1},
  {0x3724, 0x41}, {0x3725, 0xc1}, {0x3728, 0x20}, {0x37fa, 0x09},
  {0x37fb, 0x32}, {0x37fc, 0x11}, {0x37fd, 0x37}, {0x3900, 0x0d},
  {0x3905, 0x98}, {0x391b, 0x81}, {0x391c, 0x10}, {0x3933, 0x81},
  {0x3934, 0xc5}, {0x3940, 0x68}, {0x3941, 0x00}, {0x3942, 0x01},
  {0x3943, 0xc6}, {0x3952, 0x02}, {0x3953, 0x0f}, {0x3e01, 0x37},
  {0x3e02, 0xe0}, {0x3e08, 0x1f}, {0x3e1b, 0x14}, {0x440e, 0x02},
  {0x4509, 0x38}, {0x4819, 0x06}, {0x481b, 0x03}, {0x481d, 0x0b},
  {0x481f, 0x03}, {0x4821, 0x08}, {0x4823, 0x03}, {0x4825, 0x03},
  {0x4827, 0x03}, {0x4829, 0x05}, {0x5799, 0x06}, {0x5ae0, 0xfe},
  {0x5ae1, 0x40}, {0x5ae2, 0x30}, {0x5ae3, 0x28}, {0x5ae4, 0x20},
  {0x5ae5, 0x30}, {0x5ae6, 0x28}, {0x5ae7, 0x20}, {0x5ae8, 0x3c},
  {0x5ae9, 0x30}, {0x5aea, 0x28}, {0x5aeb, 0x3c}, {0x5aec, 0x30},
  {0x5aed, 0x28}, {0x5aee, 0xfe}, {0x5aef, 0x40}, {0x5af4, 0x30},
  {0x5af5, 0x28}, {0x5af6, 0x20}, {0x5af7, 0x30}, {0x5af8, 0x28},
  {0x5af9, 0x20}, {0x5afa, 0x3c}, {0x5afb, 0x30}, {0x5afc, 0x28},
  {0x5afd, 0x3c}, {0x5afe, 0x30}, {0x5aff, 0x28}, {0x36e9, 0x53},
  {0x37f9, 0x53}, {SC2336_REG_END, 0x00}
};

/*
 * Official Espressif SC2336 fallback mode: 1920x1080 RAW10, 24 MHz input,
 * 1-lane MIPI, 660 Mbps/lane, 25 fps.  This is a diagnostic mode only.  The
 * camera adapter is wired for two lanes, so the normal 2-lane mode remains
 * the production configuration.
 */
static const struct sc2336_regval_s g_sc2336_1080p25_1lane_regs[] =
{
  {0x0103, 0x01}, {0x0100, 0x00}, {0x36e9, 0x80}, {0x37f9, 0x80},
  {0x3018, 0x12}, {0x3019, 0x0e}, {0x301f, 0x29}, {0x3106, 0x05},
  {0x320e, 0x04}, {0x320f, 0xb0}, {0x3248, 0x04}, {0x3249, 0x0b},
  {0x3253, 0x08}, {0x3301, 0x09}, {0x3302, 0xff}, {0x3303, 0x10},
  {0x3306, 0x60}, {0x3307, 0x02}, {0x330a, 0x01}, {0x330b, 0x10},
  {0x330c, 0x16}, {0x330d, 0xff}, {0x3318, 0x02}, {0x3321, 0x0a},
  {0x3327, 0x0e}, {0x332b, 0x12}, {0x3333, 0x10}, {0x3334, 0x40},
  {0x335e, 0x06}, {0x335f, 0x0a}, {0x3364, 0x1f}, {0x337c, 0x02},
  {0x337d, 0x0e}, {0x3390, 0x09}, {0x3391, 0x0f}, {0x3392, 0x1f},
  {0x3393, 0x20}, {0x3394, 0x20}, {0x3395, 0xff}, {0x33a2, 0x04},
  {0x33b1, 0x80}, {0x33b2, 0x68}, {0x33b3, 0x42}, {0x33f9, 0x70},
  {0x33fb, 0xd0}, {0x33fc, 0x0f}, {0x33fd, 0x1f}, {0x349f, 0x03},
  {0x34a6, 0x0f}, {0x34a7, 0x1f}, {0x34a8, 0x42}, {0x34a9, 0x06},
  {0x34aa, 0x01}, {0x34ab, 0x23}, {0x34ac, 0x01}, {0x34ad, 0x84},
  {0x3630, 0xf4}, {0x3633, 0x22}, {0x3639, 0xf4}, {0x363c, 0x47},
  {0x3641, 0x03}, {0x3670, 0x09}, {0x3674, 0xf4}, {0x3675, 0xfb},
  {0x3676, 0xed}, {0x367c, 0x09}, {0x367d, 0x0f}, {0x3690, 0x33},
  {0x3691, 0x33}, {0x3692, 0x43}, {0x3698, 0x89}, {0x3699, 0x96},
  {0x369a, 0xd0}, {0x369b, 0xd0}, {0x369c, 0x09}, {0x369d, 0x0f},
  {0x36a2, 0x09}, {0x36a3, 0x0f}, {0x36a4, 0x1f}, {0x36d0, 0x01},
  {0x36ec, 0x0c}, {0x3722, 0xe1}, {0x3724, 0x41}, {0x3725, 0xc1},
  {0x3728, 0x20}, {0x3900, 0x0d}, {0x3905, 0x98}, {0x391b, 0x81},
  {0x391c, 0x10}, {0x3933, 0x81}, {0x3934, 0xc5}, {0x3940, 0x68},
  {0x3941, 0x00}, {0x3942, 0x01}, {0x3943, 0xc6}, {0x3952, 0x02},
  {0x3953, 0x0f}, {0x3e01, 0x37}, {0x3e02, 0xe0}, {0x3e08, 0x1f},
  {0x3e1b, 0x14}, {0x440e, 0x02}, {0x4509, 0x38}, {0x4819, 0x08},
  {0x481b, 0x05}, {0x481d, 0x12}, {0x481f, 0x04}, {0x4821, 0x0a},
  {0x4823, 0x04}, {0x4825, 0x04}, {0x4827, 0x04}, {0x4829, 0x07},
  {0x5799, 0x06}, {0x5ae0, 0xfe}, {0x5ae1, 0x40}, {0x5ae2, 0x30},
  {0x5ae3, 0x28}, {0x5ae4, 0x20}, {0x5ae5, 0x30}, {0x5ae6, 0x28},
  {0x5ae7, 0x20}, {0x5ae8, 0x3c}, {0x5ae9, 0x30}, {0x5aea, 0x28},
  {0x5aeb, 0x3c}, {0x5aec, 0x30}, {0x5aed, 0x28}, {0x5aee, 0xfe},
  {0x5aef, 0x40}, {0x5af4, 0x30}, {0x5af5, 0x28}, {0x5af6, 0x20},
  {0x5af7, 0x30}, {0x5af8, 0x28}, {0x5af9, 0x20}, {0x5afa, 0x3c},
  {0x5afb, 0x30}, {0x5afc, 0x28}, {0x5afd, 0x3c}, {0x5afe, 0x30},
  {0x5aff, 0x28}, {0x36e9, 0x20}, {0x37f9, 0x27}, {SC2336_REG_END, 0x00}
};

static int sc2336_write_reg(int fd, uint16_t reg, uint8_t value)
{
  uint8_t data[3] =
    {
      (uint8_t)(reg >> 8),
      (uint8_t)(reg & 0xff),
      value
    };
  struct i2c_msg_s msg =
    {
      .frequency = I2C_SPEED_STANDARD,
      .addr      = SC2336_I2C_ADDR,
      .flags     = 0,
      .buffer    = data,
      .length    = sizeof(data)
    };

  return i2cdev_transfer(fd, &msg, 1);
}

static int sc2336_read_reg(int fd, uint16_t reg, FAR uint8_t *value)
{
  uint8_t regaddr[2] =
    {
      (uint8_t)(reg >> 8),
      (uint8_t)(reg & 0xff)
    };
  struct i2c_msg_s msg[2] =
    {
      {
        .frequency = I2C_SPEED_STANDARD,
        .addr      = SC2336_I2C_ADDR,
        .flags     = I2C_M_NOSTOP,
        .buffer    = regaddr,
        .length    = sizeof(regaddr)
      },
      {
        .frequency = I2C_SPEED_STANDARD,
        .addr      = SC2336_I2C_ADDR,
        .flags     = I2C_M_READ,
        .buffer    = value,
        .length    = 1
      }
    };

  return i2cdev_transfer(fd, msg, 2);
}

static int sc2336_set_test_pattern(int fd, bool enable)
{
  uint8_t value;
  int ret;

  ret = sc2336_read_reg(fd, SC2336_REG_TEST_PATTERN, &value);
  if (ret < 0)
    {
      return ret;
    }

  if (enable)
    {
      value |= 1 << 3;
    }
  else
    {
      value &= ~(1 << 3);
    }

  ret = sc2336_write_reg(fd, SC2336_REG_TEST_PATTERN, value);
  if (ret < 0)
    {
      return ret;
    }

  ret = sc2336_read_reg(fd, SC2336_REG_TEST_PATTERN, &value);
  if (ret >= 0)
    {
      printf("SC2336 test pattern: %s readback=0x%02x\n",
             enable ? "on" : "off", value);
    }

  return ret;
}

static void sc2336_print_mipi_regs(int fd, FAR const char *tag)
{
  static const uint16_t regs[] =
    {
      0x0100, 0x301f, 0x3106, 0x4819, 0x481b, 0x481d,
      0x481f, 0x4821, 0x4823, 0x4825, 0x4827, 0x4829
    };
  uint8_t value;

  printf("SC2336 MIPI regs (%s):", tag);
  for (unsigned int i = 0; i < sizeof(regs) / sizeof(regs[0]); i++)
    {
      if (sc2336_read_reg(fd, regs[i], &value) < 0)
        {
          printf(" 0x%04x=ERR", regs[i]);
        }
      else
        {
          printf(" 0x%04x=0x%02x", regs[i], value);
        }
    }

  printf("\n");
}

static int sc2336_apply_regs(int fd,
                             FAR const struct sc2336_regval_s *regs,
                             FAR const char *mode)
{
  size_t i;
  int ret;

  for (i = 0; regs[i].reg != SC2336_REG_END; i++)
    {
      ret = sc2336_write_reg(fd, regs[i].reg, regs[i].value);
      if (ret < 0)
        {
          printf("SC2336 %s write failed at 0x%04x: %d\n",
                 mode, regs[i].reg, ret);
          return ret;
        }

      if (regs[i].reg == 0x0103)
        {
          usleep(5000);
        }
    }

  return OK;
}

static int sc2336_apply_1080p30(int fd)
{
  return sc2336_apply_regs(fd, g_sc2336_1080p30_regs, "2-lane init");
}

static int sc2336_apply_1080p25_1lane(int fd)
{
  return sc2336_apply_regs(fd, g_sc2336_1080p25_1lane_regs,
                           "1-lane init");
}

#ifdef CONFIG_VIDEO_STREAM

static int sc2336_csi_probe(unsigned int lanes, unsigned int lane_rate_mbps);

struct sc2336_video_s
{
  struct imgdata_s data;
  struct imgsensor_s sensor;
  int i2c_fd;
  bool registered;
  bool csi_ready;
  bool capturing;
  esp_ldo_channel_handle_t ldo_mipi_phy;
  dw_gdma_hal_context_t dma;
  dw_gdma_dev_t *dma_dev;
  uint8_t *frame;
  uint32_t frame_size;
  imgdata_capture_t capture_cb;
  FAR void *capture_arg;
  struct work_s poll_work;
  unsigned int trace_poll_count;
};

static struct sc2336_video_s g_sc2336_video;

/*
 * Keep the CSI bring-up observable without stopping the CPU in the middle of
 * a live MIPI transfer.  A debugger breakpoint after stream-on can change the
 * timing being investigated, so the trace deliberately snapshots the live
 * Host, Bridge and GDMA state at software stage boundaries instead.
 */
static void sc2336_trace_stage(FAR const char *stage,
                               FAR csi_brg_dev_t *bridge_dev,
                               FAR csi_host_dev_t *host_dev,
                               FAR dw_gdma_dev_t *dma_dev,
                               unsigned int dma_channel)
{
  printf("SC2336 TRACE %s\n", stage);

  if (host_dev != NULL)
    {
      printf("SC2336 TRACE host rx=0x%08" PRIx32
             " stop=0x%08" PRIx32
             " main=0x%08" PRIx32
             " pktfatal=0x%08" PRIx32
             " dataid=0x%08" PRIx32
             " ecc=0x%08" PRIx32 "\n",
             host_dev->phy_rx.val,
             host_dev->phy_stopstate.val,
             host_dev->int_st_main.val,
             host_dev->int_st_pkt_fatal.val,
             host_dev->int_st_data_id.val,
             host_dev->int_st_ecc_corrected.val);
    }

  if (bridge_dev != NULL)
    {
      printf("SC2336 TRACE bridge en=0x%08" PRIx32
             " raw=0x%08" PRIx32
             " st=0x%08" PRIx32
             " dma=0x%08" PRIx32
             " cm=0x%08" PRIx32 "\n",
             bridge_dev->csi_en.val,
             bridge_dev->int_raw.val,
             bridge_dev->int_st.val,
             bridge_dev->dma_req_cfg.val,
             bridge_dev->host_cm_ctrl.val);
    }

  if (dma_dev != NULL)
    {
      printf("SC2336 TRACE dma cfg=0x%08" PRIx32
             " chen=0x%08" PRIx32
             " block=0x%08" PRIx32
             " status=0x%08" PRIx32
             " int=0x%08" PRIx32 "\n",
             dma_dev->cfg0.val,
             dma_dev->chen0.val,
             dma_dev->ch[dma_channel].block_ts0.val,
             dma_dev->ch[dma_channel].status0.val,
             dma_dev->ch[dma_channel].int_st0.val);
    }
}

#define SC2336_VIDEO_FROM_SENSOR(p) \
  ((FAR struct sc2336_video_s *)((uintptr_t)(p) - \
                                 offsetof(struct sc2336_video_s, sensor)))
#define SC2336_VIDEO_FROM_DATA(p) \
  ((FAR struct sc2336_video_s *)((uintptr_t)(p) - \
                                 offsetof(struct sc2336_video_s, data)))

static bool sc2336_video_sensor_available(FAR struct imgsensor_s *sensor);
static int sc2336_video_sensor_init(FAR struct imgsensor_s *sensor);
static int sc2336_video_sensor_uninit(FAR struct imgsensor_s *sensor);
static FAR const char *sc2336_video_sensor_name(FAR struct imgsensor_s *sensor);
static int sc2336_video_sensor_validate(FAR struct imgsensor_s *sensor,
  imgsensor_stream_type_t type, uint8_t nr_fmt,
  FAR imgsensor_format_t *fmt, FAR imgsensor_interval_t *interval);
static int sc2336_video_sensor_start(FAR struct imgsensor_s *sensor,
  imgsensor_stream_type_t type, uint8_t nr_fmt,
  FAR imgsensor_format_t *fmt, FAR imgsensor_interval_t *interval);
static int sc2336_video_sensor_stop(FAR struct imgsensor_s *sensor,
  imgsensor_stream_type_t type);
static int sc2336_video_sensor_interval(FAR struct imgsensor_s *sensor,
  imgsensor_stream_type_t type, FAR imgsensor_interval_t *interval);
static int sc2336_video_data_init(FAR struct imgdata_s *data);
static int sc2336_video_data_uninit(FAR struct imgdata_s *data);
static int sc2336_video_data_set_buf(FAR struct imgdata_s *data,
  uint8_t nr_fmt, FAR imgdata_format_t *fmt, FAR uint8_t *addr,
  uint32_t size);
static int sc2336_video_data_validate(FAR struct imgdata_s *data,
  uint8_t nr_fmt, FAR imgdata_format_t *fmt,
  FAR imgdata_interval_t *interval);
static int sc2336_video_data_start(FAR struct imgdata_s *data,
  uint8_t nr_fmt, FAR imgdata_format_t *fmt,
  FAR imgdata_interval_t *interval, FAR imgdata_capture_t callback,
  FAR void *arg);
static int sc2336_video_data_stop(FAR struct imgdata_s *data);
static void sc2336_video_poll(FAR void *arg);

static const struct imgsensor_ops_s g_sc2336_video_sensor_ops =
{
  .is_available           = sc2336_video_sensor_available,
  .init                   = sc2336_video_sensor_init,
  .uninit                 = sc2336_video_sensor_uninit,
  .get_driver_name        = sc2336_video_sensor_name,
  .validate_frame_setting = sc2336_video_sensor_validate,
  .start_capture          = sc2336_video_sensor_start,
  .stop_capture           = sc2336_video_sensor_stop,
  .get_frame_interval     = sc2336_video_sensor_interval,
};

static const struct imgdata_ops_s g_sc2336_video_data_ops =
{
  .init                   = sc2336_video_data_init,
  .uninit                 = sc2336_video_data_uninit,
  .set_buf                = sc2336_video_data_set_buf,
  .validate_frame_setting = sc2336_video_data_validate,
  .start_capture          = sc2336_video_data_start,
  .stop_capture           = sc2336_video_data_stop,
};

static const struct v4l2_fmtdesc g_sc2336_video_fmts[] =
{
  {
    .index       = 0,
    .type        = V4L2_BUF_TYPE_VIDEO_CAPTURE,
    .pixelformat = V4L2_PIX_FMT_SBGGR10P,
    .description = "SC2336 Bayer BGGR RAW10 packed",
  },
};

static const struct v4l2_frmsizeenum g_sc2336_video_sizes[] =
{
  {
    .index        = 0,
    .buf_type     = V4L2_BUF_TYPE_VIDEO_CAPTURE,
    .pixel_format = V4L2_PIX_FMT_SBGGR10P,
    .type         = V4L2_FRMSIZE_TYPE_DISCRETE,
    .discrete     = { .width = 1920, .height = 1080 },
  },
};

static const struct v4l2_frmivalenum g_sc2336_video_intervals[] =
{
  {
    .index        = 0,
    .buf_type     = V4L2_BUF_TYPE_VIDEO_CAPTURE,
    .pixel_format = V4L2_PIX_FMT_SBGGR10P,
    .width        = 1920,
    .height       = 1080,
    .type         = V4L2_FRMIVAL_TYPE_DISCRETE,
    .discrete     = { .numerator = 1, .denominator = 30 },
  },
};

static bool sc2336_video_sensor_available(FAR struct imgsensor_s *sensor)
{
  FAR struct sc2336_video_s *priv =
    SC2336_VIDEO_FROM_SENSOR(sensor);
  uint8_t pid_h;
  uint8_t pid_l;

  if (priv->i2c_fd < 0 ||
      sc2336_read_reg(priv->i2c_fd, SC2336_SENSOR_ID_H, &pid_h) < 0 ||
      sc2336_read_reg(priv->i2c_fd, SC2336_SENSOR_ID_L, &pid_l) < 0)
    {
      return false;
    }

  return (((uint16_t)pid_h << 8) | pid_l) == SC2336_EXPECTED_PID;
}

static int sc2336_video_sensor_init(FAR struct imgsensor_s *sensor)
{
  FAR struct sc2336_video_s *priv =
    SC2336_VIDEO_FROM_SENSOR(sensor);

  return sc2336_apply_1080p30(priv->i2c_fd);
}

static int sc2336_video_sensor_uninit(FAR struct imgsensor_s *sensor)
{
  FAR struct sc2336_video_s *priv =
    SC2336_VIDEO_FROM_SENSOR(sensor);

  return sc2336_write_reg(priv->i2c_fd, 0x0100, 0x00);
}

static FAR const char *sc2336_video_sensor_name(FAR struct imgsensor_s *sensor)
{
  UNUSED(sensor);
  return "SC2336";
}

static int sc2336_video_sensor_validate(FAR struct imgsensor_s *sensor,
                                        imgsensor_stream_type_t type,
                                        uint8_t nr_fmt,
                                        FAR imgsensor_format_t *fmt,
                                        FAR imgsensor_interval_t *interval)
{
  UNUSED(sensor);
  UNUSED(type);

  if (nr_fmt < 1 || fmt == NULL ||
      fmt[IMGSENSOR_FMT_MAIN].width != 1920 ||
      fmt[IMGSENSOR_FMT_MAIN].height != 1080 ||
      fmt[IMGSENSOR_FMT_MAIN].pixelformat != IMGSENSOR_PIX_FMT_SBGGR10P)
    {
      return -EINVAL;
    }

  if (interval != NULL &&
      (interval->numerator != 1 || interval->denominator != 30))
    {
      return -EINVAL;
    }

  return OK;
}

static int sc2336_video_sensor_start(FAR struct imgsensor_s *sensor,
                                     imgsensor_stream_type_t type,
                                     uint8_t nr_fmt,
                                     FAR imgsensor_format_t *fmt,
                                     FAR imgsensor_interval_t *interval)
{
  FAR struct sc2336_video_s *priv =
    SC2336_VIDEO_FROM_SENSOR(sensor);
  uint8_t stream;
  int ret;

  ret = sc2336_video_sensor_validate(sensor, type, nr_fmt, fmt, interval);
  if (ret >= 0)
    {
      ret = sc2336_write_reg(priv->i2c_fd, 0x0100, 0x01);
      if (ret >= 0)
        {
          if (sc2336_read_reg(priv->i2c_fd, 0x0100, &stream) >= 0)
            {
              printf("SC2336 V4L2: stream-on readback=0x%02x\n", stream);
            }
          else
            {
              printf("SC2336 V4L2: stream-on readback failed\n");
            }

          sc2336_trace_stage("V6 sensor-stream-on",
                             MIPI_CSI_BRG_LL_GET_HW(0),
                             MIPI_CSI_HOST_LL_GET_HW(0),
                             priv->dma_dev,
                             SC2336_VIDEO_DMA_CHANNEL);
        }
    }

  return ret;
}

static int sc2336_video_sensor_stop(FAR struct imgsensor_s *sensor,
                                    imgsensor_stream_type_t type)
{
  FAR struct sc2336_video_s *priv =
    SC2336_VIDEO_FROM_SENSOR(sensor);

  UNUSED(type);
  return sc2336_write_reg(priv->i2c_fd, 0x0100, 0x00);
}

static int sc2336_video_sensor_interval(FAR struct imgsensor_s *sensor,
                                        imgsensor_stream_type_t type,
                                        FAR imgsensor_interval_t *interval)
{
  UNUSED(sensor);
  UNUSED(type);

  if (interval == NULL)
    {
      return -EINVAL;
    }

  interval->numerator = 1;
  interval->denominator = 30;
  return OK;
}

static int sc2336_video_data_init(FAR struct imgdata_s *data)
{
  FAR struct sc2336_video_s *priv =
    SC2336_VIDEO_FROM_DATA(data);
  esp_ldo_channel_config_t ldo_config =
    {
      .chan_id    = SC2336_CSI_LDO_CHAN_ID,
      .voltage_mv = SC2336_CSI_LDO_VOLTAGE_MV,
    };
  dw_gdma_hal_config_t dma_config;
  FAR csi_brg_dev_t *bridge_dev;
  int ret;

  memset(&dma_config, 0, sizeof(dma_config));

  if (priv->csi_ready)
    {
      return OK;
    }

  ret = esp_ldo_acquire_channel(&ldo_config, &priv->ldo_mipi_phy);
  if (ret < 0)
    {
      printf("SC2336 V4L2: CSI PHY LDO acquire failed: %d\n", ret);
      return ret;
    }

  sc2336_trace_stage("V1 ldo-ready", NULL, NULL, NULL,
                     SC2336_VIDEO_DMA_CHANNEL);

  ret = sc2336_csi_probe(2, 405);
  if (ret < 0)
    {
      (void)esp_ldo_release_channel(priv->ldo_mipi_phy);
      priv->ldo_mipi_phy = NULL;
      return ret;
    }

  bridge_dev = MIPI_CSI_BRG_LL_GET_HW(0);
  sc2336_trace_stage("V2 csi-probe-ready", bridge_dev,
                     MIPI_CSI_HOST_LL_GET_HW(0), NULL,
                     SC2336_VIDEO_DMA_CHANNEL);

  /* LCD DSI uses GDMA channel 0.  The CSI data path owns channel 1 and must
   * not reset the shared controller when the display is already active. */
  PERIPH_RCC_ATOMIC()
    {
      dw_gdma_ll_enable_bus_clock(0, true);
    }

  dw_gdma_hal_init(&priv->dma, &dma_config);
  priv->dma_dev = priv->dma.dev;
  if (priv->dma_dev == NULL)
    {
      (void)esp_ldo_release_channel(priv->ldo_mipi_phy);
      priv->ldo_mipi_phy = NULL;
      return -ENODEV;
    }

  sc2336_trace_stage("V3 dma-handle-ready", bridge_dev,
                     MIPI_CSI_HOST_LL_GET_HW(0), priv->dma_dev,
                     SC2336_VIDEO_DMA_CHANNEL);

  priv->csi_ready = true;
  printf("SC2336 V4L2: NuttX CSI data path ready, GDMA channel %d\n",
         SC2336_VIDEO_DMA_CHANNEL);
  return OK;
}

static int sc2336_video_data_uninit(FAR struct imgdata_s *data)
{
  FAR struct sc2336_video_s *priv =
    SC2336_VIDEO_FROM_DATA(data);

  (void)sc2336_video_data_stop(data);
  if (priv->ldo_mipi_phy != NULL)
    {
      (void)esp_ldo_release_channel(priv->ldo_mipi_phy);
      priv->ldo_mipi_phy = NULL;
    }

  priv->csi_ready = false;
  return OK;
}

static int sc2336_video_data_set_buf(FAR struct imgdata_s *data,
                                     uint8_t nr_fmt,
                                     FAR imgdata_format_t *fmt,
                                     FAR uint8_t *addr, uint32_t size)
{
  FAR struct sc2336_video_s *priv =
    SC2336_VIDEO_FROM_DATA(data);

  UNUSED(nr_fmt);
  UNUSED(fmt);
  if (addr == NULL || ((uintptr_t)addr & 0x3f) != 0 ||
      size < SC2336_FRAME_BYTES)
    {
      return -EINVAL;
    }

  priv->frame = addr;
  priv->frame_size = SC2336_FRAME_BYTES;
  return OK;
}

static int sc2336_video_data_validate(FAR struct imgdata_s *data,
                                      uint8_t nr_fmt,
                                      FAR imgdata_format_t *fmt,
                                      FAR imgdata_interval_t *interval)
{
  UNUSED(data);
  if (nr_fmt < 1 || fmt == NULL ||
      fmt[IMGDATA_FMT_MAIN].width != 1920 ||
      fmt[IMGDATA_FMT_MAIN].height != 1080 ||
      fmt[IMGDATA_FMT_MAIN].pixelformat != IMGDATA_PIX_FMT_SBGGR10P)
    {
      return -ENOTSUP;
    }

  if (interval != NULL &&
      (interval->numerator != 1 || interval->denominator != 30))
    {
      return -EINVAL;
    }

  return OK;
}

static void sc2336_video_finish(FAR struct sc2336_video_s *priv,
                                uint8_t result)
{
  imgdata_capture_t callback = priv->capture_cb;
  FAR void *arg = priv->capture_arg;

  mipi_csi_brg_ll_enable(MIPI_CSI_BRG_LL_GET_HW(0), false);
  dw_gdma_ll_channel_enable(priv->dma_dev, SC2336_VIDEO_DMA_CHANNEL, false);
  dw_gdma_ll_channel_clear_intr(priv->dma_dev, SC2336_VIDEO_DMA_CHANNEL,
                                UINT32_MAX);
  priv->capturing = false;
  if (result == 0)
    {
      (void)esp_cache_msync(priv->frame, priv->frame_size,
                            ESP_CACHE_MSYNC_FLAG_DIR_M2C);
    }

  if (callback != NULL)
    {
      callback(result, result == 0 ? priv->frame_size : 0, NULL, arg);
    }
}

static void sc2336_video_poll(FAR void *arg)
{
  FAR struct sc2336_video_s *priv = arg;
  uint32_t status;

  if (!priv->capturing || priv->dma_dev == NULL)
    {
      return;
    }

  priv->trace_poll_count++;
  if (priv->trace_poll_count == 1)
    {
      sc2336_trace_stage("V8 poll-1ms", MIPI_CSI_BRG_LL_GET_HW(0),
                         MIPI_CSI_HOST_LL_GET_HW(0), priv->dma_dev,
                         SC2336_VIDEO_DMA_CHANNEL);
    }
  else if (priv->trace_poll_count == 10)
    {
      sc2336_trace_stage("V8 poll-10ms", MIPI_CSI_BRG_LL_GET_HW(0),
                         MIPI_CSI_HOST_LL_GET_HW(0), priv->dma_dev,
                         SC2336_VIDEO_DMA_CHANNEL);
    }
  else if (priv->trace_poll_count == 100)
    {
      sc2336_trace_stage("V8 poll-100ms", MIPI_CSI_BRG_LL_GET_HW(0),
                         MIPI_CSI_HOST_LL_GET_HW(0), priv->dma_dev,
                         SC2336_VIDEO_DMA_CHANNEL);
    }
  else if (priv->trace_poll_count == 500)
    {
      sc2336_trace_stage("V8 poll-500ms", MIPI_CSI_BRG_LL_GET_HW(0),
                         MIPI_CSI_HOST_LL_GET_HW(0), priv->dma_dev,
                         SC2336_VIDEO_DMA_CHANNEL);
    }
  else if (priv->trace_poll_count == 1000)
    {
      sc2336_trace_stage("V8 poll-1000ms", MIPI_CSI_BRG_LL_GET_HW(0),
                         MIPI_CSI_HOST_LL_GET_HW(0), priv->dma_dev,
                         SC2336_VIDEO_DMA_CHANNEL);
    }

  status = dw_gdma_ll_channel_get_intr_status(priv->dma_dev,
                                               SC2336_VIDEO_DMA_CHANNEL);
  if ((status & DW_GDMA_LL_CHANNEL_EVENT_DMA_TFR_DONE) != 0)
    {
      printf("SC2336 V4L2: frame complete, status=0x%08" PRIx32 "\n",
             status);
      sc2336_video_finish(priv, 0);
    }
  else if ((status & (DW_GDMA_LL_CHANNEL_EVENT_SRC_DEC_ERR |
                      DW_GDMA_LL_CHANNEL_EVENT_DST_DEC_ERR |
                      DW_GDMA_LL_CHANNEL_EVENT_SRC_SLV_ERR |
                      DW_GDMA_LL_CHANNEL_EVENT_DST_SLV_ERR)) != 0)
    {
      printf("SC2336 V4L2: GDMA error, status=0x%08" PRIx32 "\n", status);
      sc2336_video_finish(priv, 1);
    }
  else
    {
      (void)work_queue(HPWORK, &priv->poll_work, sc2336_video_poll, priv,
                       MSEC2TICK(1));
    }
}

static int sc2336_video_data_start(FAR struct imgdata_s *data,
                                   uint8_t nr_fmt,
                                   FAR imgdata_format_t *fmt,
                                   FAR imgdata_interval_t *interval,
                                   FAR imgdata_capture_t callback,
                                   FAR void *arg)
{
  FAR struct sc2336_video_s *priv =
    SC2336_VIDEO_FROM_DATA(data);
  FAR csi_brg_dev_t *bridge_dev;

  UNUSED(nr_fmt);
  UNUSED(fmt);
  UNUSED(interval);
  if (!priv->csi_ready || priv->dma_dev == NULL || priv->frame == NULL)
    {
      return -EINVAL;
    }

  if (priv->capturing)
    {
      return -EBUSY;
    }

  bridge_dev = MIPI_CSI_BRG_LL_GET_HW(0);
  dw_gdma_ll_channel_set_trans_flow(priv->dma_dev,
                                    SC2336_VIDEO_DMA_CHANNEL,
                                    DW_GDMA_ROLE_PERIPH_CSI,
                                    DW_GDMA_ROLE_MEM,
                                    DW_GDMA_FLOW_CTRL_SRC);
  dw_gdma_ll_channel_set_src_multi_block_type(
    priv->dma_dev, SC2336_VIDEO_DMA_CHANNEL, DW_GDMA_BLOCK_TRANSFER_CONTIGUOUS);
  dw_gdma_ll_channel_set_dst_multi_block_type(
    priv->dma_dev, SC2336_VIDEO_DMA_CHANNEL, DW_GDMA_BLOCK_TRANSFER_CONTIGUOUS);
  dw_gdma_ll_channel_set_src_handshake_interface(
    priv->dma_dev, SC2336_VIDEO_DMA_CHANNEL, DW_GDMA_HANDSHAKE_HW);
  dw_gdma_ll_channel_set_dst_handshake_interface(
    priv->dma_dev, SC2336_VIDEO_DMA_CHANNEL, DW_GDMA_HANDSHAKE_HW);
  dw_gdma_ll_channel_set_src_handshake_periph(
    priv->dma_dev, SC2336_VIDEO_DMA_CHANNEL, DW_GDMA_ROLE_PERIPH_CSI);
  dw_gdma_ll_channel_set_priority(priv->dma_dev, SC2336_VIDEO_DMA_CHANNEL, 1);
  dw_gdma_ll_channel_set_src_outstanding_limit(priv->dma_dev,
                                               SC2336_VIDEO_DMA_CHANNEL, 5);
  dw_gdma_ll_channel_set_dst_outstanding_limit(priv->dma_dev,
                                               SC2336_VIDEO_DMA_CHANNEL, 5);
  dw_gdma_ll_channel_set_src_periph_status_addr(priv->dma_dev,
                                                SC2336_VIDEO_DMA_CHANNEL,
                                                MIPI_CSI_BRG_MEM_BASE);
  dw_gdma_ll_channel_set_dst_periph_status_addr(priv->dma_dev,
                                                SC2336_VIDEO_DMA_CHANNEL, 0);
  dw_gdma_ll_channel_set_src_addr(priv->dma_dev, SC2336_VIDEO_DMA_CHANNEL,
                                  MIPI_CSI_BRG_MEM_BASE);
  dw_gdma_ll_channel_set_dst_addr(priv->dma_dev, SC2336_VIDEO_DMA_CHANNEL,
                                  (uint32_t)(uintptr_t)priv->frame);
  dw_gdma_ll_channel_set_src_master_port(priv->dma_dev,
                                         SC2336_VIDEO_DMA_CHANNEL,
                                         MIPI_CSI_BRG_MEM_BASE);
  dw_gdma_ll_channel_set_dst_master_port(priv->dma_dev,
                                         SC2336_VIDEO_DMA_CHANNEL,
                                         (intptr_t)priv->frame);
  dw_gdma_ll_channel_set_src_burst_mode(priv->dma_dev,
                                        SC2336_VIDEO_DMA_CHANNEL,
                                        DW_GDMA_BURST_MODE_FIXED);
  dw_gdma_ll_channel_set_dst_burst_mode(priv->dma_dev,
                                        SC2336_VIDEO_DMA_CHANNEL,
                                        DW_GDMA_BURST_MODE_INCREMENT);
  dw_gdma_ll_channel_set_src_trans_width(priv->dma_dev,
                                         SC2336_VIDEO_DMA_CHANNEL,
                                         DW_GDMA_TRANS_WIDTH_64);
  dw_gdma_ll_channel_set_dst_trans_width(priv->dma_dev,
                                         SC2336_VIDEO_DMA_CHANNEL,
                                         DW_GDMA_TRANS_WIDTH_64);
  dw_gdma_ll_channel_set_src_burst_items(priv->dma_dev,
                                         SC2336_VIDEO_DMA_CHANNEL,
                                         DW_GDMA_BURST_ITEMS_512);
  dw_gdma_ll_channel_set_dst_burst_items(priv->dma_dev,
                                         SC2336_VIDEO_DMA_CHANNEL,
                                         DW_GDMA_BURST_ITEMS_512);
  dw_gdma_ll_channel_set_src_burst_len(priv->dma_dev,
                                       SC2336_VIDEO_DMA_CHANNEL, 16);
  dw_gdma_ll_channel_set_dst_burst_len(priv->dma_dev,
                                       SC2336_VIDEO_DMA_CHANNEL, 16);
  dw_gdma_ll_channel_set_trans_block_size(priv->dma_dev,
                                          SC2336_VIDEO_DMA_CHANNEL,
                                          SC2336_DMA_ITEMS);
  dw_gdma_ll_channel_enable_intr_generation(priv->dma_dev,
                                            SC2336_VIDEO_DMA_CHANNEL,
                                            UINT32_MAX, true);
  dw_gdma_ll_channel_clear_intr(priv->dma_dev, SC2336_VIDEO_DMA_CHANNEL,
                                UINT32_MAX);

  sc2336_trace_stage("V4 dma-programmed", bridge_dev,
                     MIPI_CSI_HOST_LL_GET_HW(0), priv->dma_dev,
                     SC2336_VIDEO_DMA_CHANNEL);

  mipi_csi_brg_ll_enable_color_conversion(bridge_dev, true);
  mipi_csi_brg_ll_set_color_mode_bypass(bridge_dev, true);
  /* Espressif's SC2336 MIPI metadata uses line_sync_en=false. */
  mipi_csi_brg_ll_enable_has_hsync(bridge_dev, false);
  mipi_csi_brg_ll_set_data_type_min(bridge_dev, 0x12);
  mipi_csi_brg_ll_set_data_type_max(bridge_dev, 0x2f);
  mipi_csi_brg_ll_set_burst_len(bridge_dev, 512);
  (void)esp_cache_msync(priv->frame, priv->frame_size,
                        ESP_CACHE_MSYNC_FLAG_DIR_M2C);
  priv->capture_cb = callback;
  priv->capture_arg = arg;
  priv->capturing = true;
  priv->trace_poll_count = 0;
  dw_gdma_ll_channel_enable(priv->dma_dev, SC2336_VIDEO_DMA_CHANNEL, true);
  mipi_csi_brg_ll_enable(bridge_dev, true);
  sc2336_trace_stage("V5 dma-bridge-enabled", bridge_dev,
                     MIPI_CSI_HOST_LL_GET_HW(0), priv->dma_dev,
                     SC2336_VIDEO_DMA_CHANNEL);
  (void)work_queue(HPWORK, &priv->poll_work, sc2336_video_poll, priv, 1);
  return OK;
}

static int sc2336_video_data_stop(FAR struct imgdata_s *data)
{
  FAR struct sc2336_video_s *priv =
    SC2336_VIDEO_FROM_DATA(data);

  /* Mark the stream stopped before cancelling the poll work.  The callback
   * can be stopping the capture itself, and a synchronous cancellation from
   * that callback would deadlock.  A queued callback sees capturing=false
   * and exits without re-queuing itself. */
  priv->capturing = false;
  (void)work_cancel(HPWORK, &priv->poll_work);
  if (priv->dma_dev != NULL)
    {
      mipi_csi_brg_ll_enable(MIPI_CSI_BRG_LL_GET_HW(0), false);
      /* The official CSI controller stops a live channel by disabling it;
       * abort is reserved for a genuinely hung GDMA transaction. */
      dw_gdma_ll_channel_enable(priv->dma_dev, SC2336_VIDEO_DMA_CHANNEL,
                                false);
      dw_gdma_ll_channel_clear_intr(priv->dma_dev,
                                    SC2336_VIDEO_DMA_CHANNEL, UINT32_MAX);
    }

  priv->capture_cb = NULL;
  priv->capture_arg = NULL;
  return OK;
}

static int sc2336_video_register(int fd)
{
  FAR struct imgsensor_s *sensor;
  int ret;

  if (g_sc2336_video.registered)
    {
      return -EEXIST;
    }

  memset(&g_sc2336_video, 0, sizeof(g_sc2336_video));
  g_sc2336_video.i2c_fd = fd;
  g_sc2336_video.data.ops = &g_sc2336_video_data_ops;
  g_sc2336_video.sensor.ops = &g_sc2336_video_sensor_ops;
  g_sc2336_video.sensor.fmtdescs_num = sizeof(g_sc2336_video_fmts) /
                                        sizeof(g_sc2336_video_fmts[0]);
  g_sc2336_video.sensor.fmtdescs = g_sc2336_video_fmts;
  g_sc2336_video.sensor.frmsizes_num = sizeof(g_sc2336_video_sizes) /
                                       sizeof(g_sc2336_video_sizes[0]);
  g_sc2336_video.sensor.frmsizes = g_sc2336_video_sizes;
  g_sc2336_video.sensor.frmintervals_num =
    sizeof(g_sc2336_video_intervals) / sizeof(g_sc2336_video_intervals[0]);
  g_sc2336_video.sensor.frmintervals = g_sc2336_video_intervals;
  sensor = &g_sc2336_video.sensor;

  ret = capture_register(SC2336_VIDEO_DEVPATH, &g_sc2336_video.data,
                         &sensor, 1);
  if (ret < 0)
    {
      printf("SC2336 V4L2: register %s failed: %d\n",
             SC2336_VIDEO_DEVPATH, ret);
      return ret;
    }

  g_sc2336_video.registered = true;
  printf("SC2336 V4L2: registered %s (1920x1080 BGGR RAW10)\n",
         SC2336_VIDEO_DEVPATH);
  return OK;
}

static int sc2336_video_unregister(void)
{
  int ret = OK;

  if (g_sc2336_video.registered)
    {
      ret = capture_unregister(SC2336_VIDEO_DEVPATH);
      if (ret < 0)
        {
          printf("SC2336 V4L2: unregister %s failed: %d\n",
                 SC2336_VIDEO_DEVPATH, ret);
        }

      g_sc2336_video.registered = false;
    }

  if (g_sc2336_video.i2c_fd >= 0)
    {
      close(g_sc2336_video.i2c_fd);
      g_sc2336_video.i2c_fd = -1;
    }

  return ret;
}

static int sc2336_video_capture_once(void)
{
  struct v4l2_format fmt;
  struct v4l2_requestbuffers req;
  struct v4l2_buffer buf;
  struct pollfd pfd;
  enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  FAR uint8_t *frame = NULL;
  uint32_t checksum;
  unsigned int i;
  int video_fd = -1;
  int ret;

  if (!g_sc2336_video.registered)
    {
      printf("SC2336 V4L2: run 'i2c sc2336 video' first\n");
      return -ENODEV;
    }

  video_fd = open(SC2336_VIDEO_DEVPATH, O_RDWR | O_NONBLOCK);
  if (video_fd < 0)
    {
      printf("SC2336 V4L2: open %s failed: %d\n",
             SC2336_VIDEO_DEVPATH, errno);
      return -errno;
    }

  /* ESP32-P4 cache maintenance requires the PSRAM buffer to be aligned to
   * the 64-byte cache line used by the CSI/DMA path. */
  frame = memalign(64, SC2336_FRAME_BYTES);
  if (frame == NULL)
    {
      printf("SC2336 V4L2: frame buffer allocation failed\n");
      ret = -ENOMEM;
      goto cleanup;
    }

  memset(&fmt, 0, sizeof(fmt));
  fmt.type                = type;
  fmt.fmt.pix.width       = 1920;
  fmt.fmt.pix.height      = 1080;
  fmt.fmt.pix.field       = V4L2_FIELD_ANY;
  fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_SBGGR10P;
  fmt.fmt.pix.sizeimage   = SC2336_FRAME_BYTES;
  ret = ioctl(video_fd, VIDIOC_S_FMT, (uintptr_t)&fmt);
  if (ret < 0)
    {
      printf("SC2336 V4L2: S_FMT failed: %d\n", errno);
      ret = -errno;
      goto cleanup;
    }

  printf("SC2336 V4L2: format %" PRIu32 "x%" PRIu32
         " fourcc=0x%08" PRIx32
         " sizeimage=%" PRIu32 "\n",
         fmt.fmt.pix.width, fmt.fmt.pix.height,
         fmt.fmt.pix.pixelformat, fmt.fmt.pix.sizeimage);

  memset(&req, 0, sizeof(req));
  req.type   = type;
  req.memory = V4L2_MEMORY_USERPTR;
  req.count  = 1;
  req.mode   = V4L2_BUF_MODE_FIFO;
  ret = ioctl(video_fd, VIDIOC_REQBUFS, (uintptr_t)&req);
  if (ret < 0)
    {
      printf("SC2336 V4L2: REQBUFS failed: %d\n", errno);
      ret = -errno;
      goto cleanup;
    }

  memset(&buf, 0, sizeof(buf));
  buf.type         = type;
  buf.memory       = V4L2_MEMORY_USERPTR;
  buf.index        = 0;
  buf.m.userptr    = (uintptr_t)frame;
  buf.length       = SC2336_FRAME_BYTES;
  ret = ioctl(video_fd, VIDIOC_QBUF, (uintptr_t)&buf);
  if (ret < 0)
    {
      printf("SC2336 V4L2: QBUF failed: %d\n", errno);
      ret = -errno;
      goto cleanup;
    }

  ret = ioctl(video_fd, VIDIOC_STREAMON, (uintptr_t)&type);
  if (ret < 0)
    {
      printf("SC2336 V4L2: STREAMON failed: %d\n", errno);
      ret = -errno;
      goto cleanup;
    }

  printf("SC2336 V4L2: STREAMON, waiting for one frame\n");
  memset(&pfd, 0, sizeof(pfd));
  pfd.fd      = video_fd;
  pfd.events  = POLLIN;
  ret = poll(&pfd, 1, SC2336_CAPTURE_TIMEOUT_MS);
  if (ret == 0)
    {
      printf("SC2336 V4L2: frame timeout after %d ms\n",
             SC2336_CAPTURE_TIMEOUT_MS);
      ret = -ETIMEDOUT;
      goto streamoff;
    }

  if (ret < 0)
    {
      printf("SC2336 V4L2: poll failed: %d\n", errno);
      ret = -errno;
      goto streamoff;
    }

  if ((pfd.revents & POLLIN) == 0)
    {
      printf("SC2336 V4L2: poll events=0x%02" PRIx32 "\n", pfd.revents);
      ret = -EIO;
      goto streamoff;
    }

  memset(&buf, 0, sizeof(buf));
  buf.type   = type;
  buf.memory = V4L2_MEMORY_USERPTR;
  ret = ioctl(video_fd, VIDIOC_DQBUF, (uintptr_t)&buf);
  if (ret < 0)
    {
      printf("SC2336 V4L2: DQBUF failed: %d\n", errno);
      ret = -errno;
      goto streamoff;
    }

  checksum = 0;
  for (i = 0; i < 1024; i++)
    {
      checksum = (checksum << 5) - checksum + frame[i];
    }

  printf("SC2336 V4L2: FRAME DONE bytesused=%" PRIu32
         " flags=0x%08" PRIx32 " checksum1024=0x%08" PRIx32
         " sample=%02x %02x %02x %02x\n",
         buf.bytesused, buf.flags, checksum,
         frame[0], frame[1], frame[2], frame[3]);
  ret = OK;

streamoff:
  if (ioctl(video_fd, VIDIOC_STREAMOFF, (uintptr_t)&type) < 0 && ret == OK)
    {
      ret = -errno;
    }

cleanup:
  if (video_fd >= 0)
    {
      close(video_fd);
    }

  free(frame);
  (void)sc2336_video_unregister();
  return ret;
}

#endif /* CONFIG_VIDEO_STREAM */

static int sc2336_csi_probe(unsigned int lanes, unsigned int lane_rate_mbps)
{
  mipi_csi_hal_context_t hal;
  mipi_csi_hal_config_t config =
    {
      .lanes_num          = lanes,
      .frame_width        = 1080,
      .frame_height       = 1920,
      .in_bpp              = 10,
      .out_bpp             = 10,
      .byte_swap_en       = false,
      .lane_bit_rate_mbps = lane_rate_mbps
    };

  /* The public upper camera controller is IDF/FreeRTOS based.  For this
   * first NuttX probe, bring up only the P4 lower CSI HAL and leave DMA and
   * frame queues out of scope. */
  (void)esp_clk_tree_enable_src((soc_module_clk_t)
                                MIPI_CSI_PHY_CLK_SRC_DEFAULT, true);

    PERIPH_RCC_ATOMIC()
    {
      /* Match the official CSI controller claim sequence: force the host
       * bus clock through a disabled state before resetting and enabling it.
       */
      mipi_csi_ll_enable_host_bus_clock(0, false);
      mipi_csi_ll_enable_host_bus_clock(0, true);
      mipi_csi_ll_reset_host_clock(0);
      mipi_csi_ll_enable_brg_module_clock(0, true);
      mipi_csi_ll_reset_brg_module_clock(0);
      mipi_csi_brg_ll_enable_clock(MIPI_CSI_BRG_LL_GET_HW(0), true);
      mipi_csi_ll_set_phy_clock_source(0, MIPI_CSI_PHY_CLK_SRC_DEFAULT);
      mipi_csi_ll_enable_phy_config_clock(0, false);
      mipi_csi_ll_enable_phy_config_clock(0, true);
    }

  mipi_csi_hal_init(&hal, &config);
  printf("SC2336 CSI lower HAL ready: 1920x1080 RAW10 %u-lane %u Mbps/lane\n",
         lanes, lane_rate_mbps);
  return OK;
}

static int sc2336_csi_capture(int fd, bool test_pattern, bool one_lane)
{
  esp_ldo_channel_handle_t ldo_mipi_phy = NULL;
  esp_ldo_channel_config_t ldo_config =
    {
      .chan_id    = SC2336_CSI_LDO_CHAN_ID,
      .voltage_mv = SC2336_CSI_LDO_VOLTAGE_MV,
    };
  dw_gdma_hal_context_t dma;
  dw_gdma_hal_config_t dma_config;
  dw_gdma_dev_t *dev;
  csi_brg_dev_t *bridge_dev;
  csi_host_dev_t *host_dev;
  uint8_t *frame;
  size_t cache_alignment;
  uint32_t intr_status;
  uint32_t checksum;
  uint32_t amount;
  uint32_t dma_global_cfg;
  uint32_t dma_channel_enable;
  uint32_t dma_sar;
  uint32_t dma_dar;
  uint32_t dma_block_ts;
  uint32_t dma_ctl0;
  uint32_t dma_ctl1;
  uint32_t dma_cfg0;
  uint32_t dma_cfg1;
  uint32_t dma_status0;
  uint32_t dma_status1;
  uint32_t dma_sstat0;
  uint32_t dma_dstat0;
  uint32_t dma_int0;
  uint32_t dma_int1;
  uint32_t bridge_csi_en;
  uint32_t bridge_dma_req_cfg;
  uint32_t bridge_buf_flow_ctl;
  uint32_t bridge_frame_cfg;
  uint32_t bridge_int_raw;
  uint32_t bridge_int_st;
  uint32_t bridge_host_ctrl;
  uint32_t bridge_reserved_044;
  uint32_t bridge_host_cm_ctrl;
  uint32_t bridge_host_size_ctrl;
  uint32_t bridge_data_type_cfg;
  uint32_t bridge_dma_req_interval;
  uint32_t bridge_dmablk_size;
  uint32_t bridge_mirror_csi_en;
  uint32_t bridge_mirror_dma_req_cfg;
  uint32_t bridge_mirror_buf_flow_ctl;
  uint32_t bridge_mirror_frame_cfg;
  uint32_t bridge_mirror_host_ctrl;
  uint32_t bridge_mirror_reserved_044;
  uint32_t bridge_mirror_host_cm_ctrl;
  uint32_t bridge_mirror_host_size_ctrl;
  uint32_t host_csi2_resetn;
  uint32_t host_phy_shutdownz;
  uint32_t host_dphy_rstz;
  uint32_t host_phy_rx;
  uint32_t host_phy_stopstate;
  uint32_t host_int_st_main;
  uint32_t host_int_st_phy_fatal;
  uint32_t host_int_st_pkt_fatal;
  uint32_t host_int_st_phy;
  uint32_t host_int_st_bndry_frame_fatal;
  uint32_t host_int_st_seq_frame_fatal;
  uint32_t host_int_st_crc_frame_fatal;
  uint32_t host_int_st_pld_crc_fatal;
  uint32_t host_int_st_data_id;
  uint32_t host_int_st_ecc_corrected;
  uint32_t host_n_lanes;
  uint32_t host_vc_extension;
  uint32_t host_scrambling;
  uint32_t host_phy_test_ctrl0;
  uint32_t host_phy_test_ctrl1;
  uint8_t stream;
  unsigned int elapsed;
  bool dma_started = false;
  int ret;

  ret = esp_ldo_acquire_channel(&ldo_config, &ldo_mipi_phy);
  if (ret < 0)
    {
      printf("SC2336 capture: MIPI CSI PHY LDO acquire failed: %d\n", ret);
      return ret;
    }

  ret = esp_cache_get_alignment(MALLOC_CAP_SPIRAM | MALLOC_CAP_DMA,
                                &cache_alignment);
  if (ret < 0)
    {
      printf("SC2336 capture: cache alignment query failed: %d\n", ret);
      (void)esp_ldo_release_channel(ldo_mipi_phy);
      return ret;
    }

  frame = heap_caps_aligned_calloc(cache_alignment, 1, SC2336_FRAME_BYTES,
                                   MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  if (frame == NULL)
    {
      printf("SC2336 capture: PSRAM frame allocation failed (%u bytes)\n",
             SC2336_FRAME_BYTES);
      (void)esp_ldo_release_channel(ldo_mipi_phy);
      return -ENOMEM;
    }

  printf("SC2336 capture: frame=%p size=%u bytes\n",
         frame, SC2336_FRAME_BYTES);

  ret = esp_cache_msync(frame, SC2336_FRAME_BYTES,
                        ESP_CACHE_MSYNC_FLAG_DIR_M2C);
  if (ret < 0)
    {
      printf("SC2336 capture: DMA buffer cache sync failed: %d\n", ret);
      free(frame);
      (void)esp_ldo_release_channel(ldo_mipi_phy);
      return ret;
    }

  ret = sc2336_csi_probe(one_lane ? 1 : 2, one_lane ? 660 : 405);
  if (ret < 0)
    {
      free(frame);
      (void)esp_ldo_release_channel(ldo_mipi_phy);
      return ret;
    }

  /* Match the official CSI controller DMA configuration, but keep the
   * control path NuttX-native and poll the completion status first. */
  PERIPH_RCC_ATOMIC()
    {
      dw_gdma_ll_enable_bus_clock(0, true);
      dw_gdma_ll_reset_register(0);
    }

  dw_gdma_hal_init(&dma, &dma_config);
  dev = dma.dev;
  if (dev == NULL)
    {
      printf("SC2336 capture: DW-GDMA HAL returned no device\n");
      free(frame);
      (void)esp_ldo_release_channel(ldo_mipi_phy);
      return -ENODEV;
    }

  dw_gdma_ll_channel_set_trans_flow(dev, SC2336_DMA_CHANNEL,
                                    DW_GDMA_ROLE_PERIPH_CSI,
                                    DW_GDMA_ROLE_MEM,
                                    DW_GDMA_FLOW_CTRL_SRC);
  dw_gdma_ll_channel_set_src_multi_block_type(
      dev, SC2336_DMA_CHANNEL, DW_GDMA_BLOCK_TRANSFER_CONTIGUOUS);
  dw_gdma_ll_channel_set_dst_multi_block_type(
      dev, SC2336_DMA_CHANNEL, DW_GDMA_BLOCK_TRANSFER_CONTIGUOUS);
  dw_gdma_ll_channel_set_src_handshake_interface(
      dev, SC2336_DMA_CHANNEL, DW_GDMA_HANDSHAKE_HW);
  dw_gdma_ll_channel_set_dst_handshake_interface(
      dev, SC2336_DMA_CHANNEL, DW_GDMA_HANDSHAKE_HW);
  dw_gdma_ll_channel_set_src_handshake_periph(
      dev, SC2336_DMA_CHANNEL, DW_GDMA_ROLE_PERIPH_CSI);
  dw_gdma_ll_channel_set_priority(dev, SC2336_DMA_CHANNEL, 1);
  dw_gdma_ll_channel_set_src_outstanding_limit(dev, SC2336_DMA_CHANNEL, 5);
  dw_gdma_ll_channel_set_dst_outstanding_limit(dev, SC2336_DMA_CHANNEL, 5);
  dw_gdma_ll_channel_set_src_periph_status_addr(
      dev, SC2336_DMA_CHANNEL, MIPI_CSI_BRG_MEM_BASE);
  dw_gdma_ll_channel_set_dst_periph_status_addr(dev, SC2336_DMA_CHANNEL, 0);

  dw_gdma_ll_channel_set_src_addr(dev, SC2336_DMA_CHANNEL,
                                  MIPI_CSI_BRG_MEM_BASE);
  dw_gdma_ll_channel_set_dst_addr(dev, SC2336_DMA_CHANNEL,
                                  (uint32_t)(uintptr_t)frame);
  dw_gdma_ll_channel_set_src_master_port(dev, SC2336_DMA_CHANNEL,
                                         MIPI_CSI_BRG_MEM_BASE);
  dw_gdma_ll_channel_set_dst_master_port(dev, SC2336_DMA_CHANNEL,
                                         (intptr_t)frame);
  dw_gdma_ll_channel_set_src_burst_mode(
      dev, SC2336_DMA_CHANNEL, DW_GDMA_BURST_MODE_FIXED);
  dw_gdma_ll_channel_set_dst_burst_mode(
      dev, SC2336_DMA_CHANNEL, DW_GDMA_BURST_MODE_INCREMENT);
  dw_gdma_ll_channel_set_src_trans_width(
      dev, SC2336_DMA_CHANNEL, DW_GDMA_TRANS_WIDTH_64);
  dw_gdma_ll_channel_set_dst_trans_width(
      dev, SC2336_DMA_CHANNEL, DW_GDMA_TRANS_WIDTH_64);
  dw_gdma_ll_channel_set_src_burst_items(
      dev, SC2336_DMA_CHANNEL, DW_GDMA_BURST_ITEMS_512);
  dw_gdma_ll_channel_set_dst_burst_items(
      dev, SC2336_DMA_CHANNEL, DW_GDMA_BURST_ITEMS_512);
  dw_gdma_ll_channel_set_src_burst_len(dev, SC2336_DMA_CHANNEL, 16);
  dw_gdma_ll_channel_set_dst_burst_len(dev, SC2336_DMA_CHANNEL, 16);
  dw_gdma_ll_channel_set_trans_block_size(dev, SC2336_DMA_CHANNEL,
                                          SC2336_DMA_ITEMS);
  dw_gdma_ll_channel_enable_intr_generation(dev, SC2336_DMA_CHANNEL,
                                            UINT32_MAX, true);
  dw_gdma_ll_channel_clear_intr(dev, SC2336_DMA_CHANNEL, UINT32_MAX);

  bridge_dev = MIPI_CSI_BRG_LL_GET_HW(0);
  host_dev = MIPI_CSI_HOST_LL_GET_HW(0);
  mipi_csi_brg_ll_enable_color_conversion(bridge_dev, true);
  mipi_csi_brg_ll_set_color_mode_bypass(bridge_dev, true);
  /* SC2336 is known to work with the bridge's default HSYNC interpretation;
   * the no-HSYNC A/B probe did not produce a CSI packet either. */
  /* Espressif's SC2336 MIPI metadata uses line_sync_en=false. */
  mipi_csi_brg_ll_enable_has_hsync(bridge_dev, false);
  /* Keep host_cm_ctrl.csi_host_cm_lane_num at the hardware reset default.
   * The official controller only uses this field for RGB888-to-RGB888 color
   * conversion; RAW10 uses the bridge bypass path. */
  /* Keep the official CSI bridge data-type window.  The sensor's RAW10
   * payload is 0x2b, but restricting the bridge to one type can discard
   * valid CSI packet traffic before it reaches the DMA request generator. */
  mipi_csi_brg_ll_set_data_type_min(bridge_dev, 0x12);
  mipi_csi_brg_ll_set_data_type_max(bridge_dev, 0x2f);
  mipi_csi_brg_ll_set_burst_len(bridge_dev, 512);
  dw_gdma_ll_channel_enable(dev, SC2336_DMA_CHANNEL, true);
  dma_started = true;
  mipi_csi_brg_ll_enable(bridge_dev, true);

  if (test_pattern)
    {
      ret = sc2336_set_test_pattern(fd, true);
      if (ret < 0)
        {
          printf("SC2336 capture: test pattern enable failed: %d\n", ret);
        }
    }

  sc2336_print_mipi_regs(fd, "before-stream");

  ret = sc2336_write_reg(fd, 0x0100, 0x01);
  if (ret < 0)
    {
      printf("SC2336 capture: stream-on failed: %d\n", ret);
      mipi_csi_brg_ll_enable(bridge_dev, false);
      dw_gdma_ll_channel_abort(dev, SC2336_DMA_CHANNEL);
      dw_gdma_hal_deinit(&dma);
      free(frame);
      (void)esp_ldo_release_channel(ldo_mipi_phy);
      return ret;
    }

  ret = sc2336_read_reg(fd, 0x0100, &stream);
  if (ret < 0)
    {
      printf("SC2336 capture: stream-on readback failed: %d\n", ret);
    }
  else
    {
      printf("SC2336 capture: stream-on readback=0x%02x\n", stream);
    }

  sc2336_print_mipi_regs(fd, "after-stream");

  printf("SC2336 capture: stream on, waiting for DMA frame\n");
  intr_status = 0;
  for (elapsed = 0; elapsed < SC2336_CAPTURE_TIMEOUT_MS; elapsed++)
    {
      intr_status = dw_gdma_ll_channel_get_intr_status(dev,
                                                        SC2336_DMA_CHANNEL);
      if ((intr_status & DW_GDMA_LL_CHANNEL_EVENT_DMA_TFR_DONE) != 0)
        {
          break;
        }

      usleep(1000);
    }

  /* Snapshot the live DMA state before stopping CSI or aborting the channel.
   * The abort sequence may change the transfer counters and status bits, so
   * values read after abort are not valid evidence of what happened at the
   * timeout.
   */
  dma_global_cfg = dev->cfg0.val;
  dma_channel_enable = dev->chen0.val;
  dma_sar = dev->ch[SC2336_DMA_CHANNEL].sar0.val;
  dma_dar = dev->ch[SC2336_DMA_CHANNEL].dar0.val;
  dma_block_ts = dev->ch[SC2336_DMA_CHANNEL].block_ts0.val;
  dma_ctl0 = dev->ch[SC2336_DMA_CHANNEL].ctl0.val;
  dma_ctl1 = dev->ch[SC2336_DMA_CHANNEL].ctl1.val;
  dma_cfg0 = dev->ch[SC2336_DMA_CHANNEL].cfg0.val;
  dma_cfg1 = dev->ch[SC2336_DMA_CHANNEL].cfg1.val;
  dma_status0 = dev->ch[SC2336_DMA_CHANNEL].status0.val;
  dma_status1 = dev->ch[SC2336_DMA_CHANNEL].status1.val;
  dma_sstat0 = dev->ch[SC2336_DMA_CHANNEL].sstat0.val;
  dma_dstat0 = dev->ch[SC2336_DMA_CHANNEL].dstat0.val;
  dma_int0 = dev->ch[SC2336_DMA_CHANNEL].int_st0.val;
  dma_int1 = dev->ch[SC2336_DMA_CHANNEL].int_st1.val;

  bridge_csi_en = bridge_dev->csi_en.val;
  bridge_dma_req_cfg = bridge_dev->dma_req_cfg.val;
  bridge_buf_flow_ctl = bridge_dev->buf_flow_ctl.val;
  bridge_frame_cfg = bridge_dev->frame_cfg.val;
  bridge_int_raw = bridge_dev->int_raw.val;
  bridge_int_st = bridge_dev->int_st.val;
  bridge_host_ctrl = bridge_dev->host_ctrl.val;
  bridge_reserved_044 = *(volatile uint32_t *)((uintptr_t)bridge_dev + 0x44);
  bridge_host_cm_ctrl = bridge_dev->host_cm_ctrl.val;
  bridge_host_size_ctrl = bridge_dev->host_size_ctrl.val;
  bridge_data_type_cfg = bridge_dev->data_type_cfg.val;
  bridge_dma_req_interval = bridge_dev->dma_req_interval.val;
  bridge_dmablk_size = bridge_dev->dmablk_size.val;
  /* hw_ver3 exposes a second bridge register window at +0x80.  The
   * official P4 CSI investigation uses it to distinguish the programmed
   * shadow registers from the active copy.  Read it without writing it. */
  bridge_mirror_csi_en = *(volatile uint32_t *)((uintptr_t)bridge_dev + 0x84);
  bridge_mirror_dma_req_cfg = *(volatile uint32_t *)((uintptr_t)bridge_dev + 0x88);
  bridge_mirror_buf_flow_ctl = *(volatile uint32_t *)((uintptr_t)bridge_dev + 0x8c);
  bridge_mirror_frame_cfg = *(volatile uint32_t *)((uintptr_t)bridge_dev + 0x94);
  bridge_mirror_host_ctrl = *(volatile uint32_t *)((uintptr_t)bridge_dev + 0xc0);
  bridge_mirror_reserved_044 = *(volatile uint32_t *)((uintptr_t)bridge_dev + 0xc4);
  bridge_mirror_host_cm_ctrl = *(volatile uint32_t *)((uintptr_t)bridge_dev + 0xc8);
  bridge_mirror_host_size_ctrl = *(volatile uint32_t *)((uintptr_t)bridge_dev + 0xcc);
  host_csi2_resetn = host_dev->csi2_resetn.val;
  host_phy_shutdownz = host_dev->phy_shutdownz.val;
  host_dphy_rstz = host_dev->dphy_rstz.val;
  host_phy_rx = host_dev->phy_rx.val;
  host_phy_stopstate = host_dev->phy_stopstate.val;
  host_int_st_main = host_dev->int_st_main.val;
  host_int_st_phy_fatal = host_dev->int_st_phy_fatal.val;
  host_int_st_pkt_fatal = host_dev->int_st_pkt_fatal.val;
  host_int_st_phy = host_dev->int_st_phy.val;
  host_int_st_bndry_frame_fatal = host_dev->int_st_bndry_frame_fatal.val;
  host_int_st_seq_frame_fatal = host_dev->int_st_seq_frame_fatal.val;
  host_int_st_crc_frame_fatal = host_dev->int_st_crc_frame_fatal.val;
  host_int_st_pld_crc_fatal = host_dev->int_st_pld_crc_fatal.val;
  host_int_st_data_id = host_dev->int_st_data_id.val;
  host_int_st_ecc_corrected = host_dev->int_st_ecc_corrected.val;
  host_n_lanes = host_dev->n_lanes.val;
  host_vc_extension = host_dev->vc_extension.val;
  host_scrambling = host_dev->scrambling.val;
  host_phy_test_ctrl0 = host_dev->phy_test_ctrl0.val;
  host_phy_test_ctrl1 = host_dev->phy_test_ctrl1.val;

  ret = sc2336_write_reg(fd, 0x0100, 0x00);
  if (ret < 0)
    {
      printf("SC2336 capture: stream-off failed: %d\n", ret);
    }

  mipi_csi_brg_ll_enable(bridge_dev, false);
  if (test_pattern)
    {
      (void)sc2336_set_test_pattern(fd, false);
    }
  if (dma_started &&
      (intr_status & DW_GDMA_LL_CHANNEL_EVENT_DMA_TFR_DONE) == 0)
    {
      dw_gdma_ll_channel_abort(dev, SC2336_DMA_CHANNEL);
    }

  dw_gdma_ll_channel_clear_intr(dev, SC2336_DMA_CHANNEL, UINT32_MAX);
  amount = dma_status0 & 0x003fffff;

  if ((intr_status & DW_GDMA_LL_CHANNEL_EVENT_DMA_TFR_DONE) == 0)
    {
      printf("SC2336 CSI bridge: en=0x%08" PRIx32
             " dma=0x%08" PRIx32 " flow=0x%08" PRIx32
             " frame=0x%08" PRIx32 " raw=0x%08" PRIx32
             " st=0x%08" PRIx32 " host=0x%08" PRIx32
             " res44=0x%08" PRIx32 "\n",
             bridge_csi_en, bridge_dma_req_cfg, bridge_buf_flow_ctl,
             bridge_frame_cfg, bridge_int_raw, bridge_int_st,
             bridge_host_ctrl, bridge_reserved_044);
      printf("SC2336 CSI bridge cfg: cm=0x%08" PRIx32
             " size=0x%08" PRIx32 " type=0x%08" PRIx32
             " interval=0x%08" PRIx32 " dmablk=0x%08" PRIx32 "\n",
             bridge_host_cm_ctrl, bridge_host_size_ctrl,
             bridge_data_type_cfg, bridge_dma_req_interval,
             bridge_dmablk_size);
      printf("SC2336 CSI bridge mirror: en=0x%08" PRIx32
             " dma=0x%08" PRIx32 " flow=0x%08" PRIx32
             " frame=0x%08" PRIx32 " host=0x%08" PRIx32
             " res44=0x%08" PRIx32 " cm=0x%08" PRIx32
             " size=0x%08" PRIx32 "\n",
             bridge_mirror_csi_en, bridge_mirror_dma_req_cfg,
             bridge_mirror_buf_flow_ctl, bridge_mirror_frame_cfg,
             bridge_mirror_host_ctrl, bridge_mirror_reserved_044,
             bridge_mirror_host_cm_ctrl, bridge_mirror_host_size_ctrl);
      printf("SC2336 CSI host: resetn=0x%08" PRIx32
             " shutdown=0x%08" PRIx32 " dphy=0x%08" PRIx32
             " rx=0x%08" PRIx32 " stop=0x%08" PRIx32
             " main=0x%08" PRIx32 " phyfatal=0x%08" PRIx32
             " pktfatal=0x%08" PRIx32 " phy=0x%08" PRIx32 "\n",
             host_csi2_resetn, host_phy_shutdownz, host_dphy_rstz,
             host_phy_rx, host_phy_stopstate, host_int_st_main,
             host_int_st_phy_fatal, host_int_st_pkt_fatal, host_int_st_phy);
      printf("SC2336 CSI host cfg: lanes=0x%08" PRIx32
             " vcx=0x%08" PRIx32 " scrambling=0x%08" PRIx32
             " bndry=0x%08" PRIx32 " seq=0x%08" PRIx32
             " crc=0x%08" PRIx32 " pldcrc=0x%08" PRIx32 "\n",
             host_n_lanes, host_vc_extension, host_scrambling,
             host_int_st_bndry_frame_fatal, host_int_st_seq_frame_fatal,
             host_int_st_crc_frame_fatal, host_int_st_pld_crc_fatal);
      printf("SC2336 CSI host packet: data_id=0x%08" PRIx32
             " ecc=0x%08" PRIx32 " test0=0x%08" PRIx32
             " test1=0x%08" PRIx32 "\n",
             host_int_st_data_id, host_int_st_ecc_corrected,
             host_phy_test_ctrl0, host_phy_test_ctrl1);
      printf("SC2336 DMA live: global_cfg=0x%08" PRIx32
             " chen=0x%08" PRIx32 " sar=0x%08" PRIx32
             " dar=0x%08" PRIx32 " block_ts=0x%08" PRIx32 "\n",
             dma_global_cfg, dma_channel_enable, dma_sar, dma_dar,
             dma_block_ts);
      printf("SC2336 DMA live: ctl0=0x%08" PRIx32
             " ctl1=0x%08" PRIx32 " cfg0=0x%08" PRIx32
             " cfg1=0x%08" PRIx32 " status0=0x%08" PRIx32
             " status1=0x%08" PRIx32 "\n",
             dma_ctl0, dma_ctl1, dma_cfg0, dma_cfg1, dma_status0,
             dma_status1);
      printf("SC2336 DMA live: sstat=0x%08" PRIx32
             " dstat=0x%08" PRIx32 " int0=0x%08" PRIx32
             " int1=0x%08" PRIx32 "\n",
             dma_sstat0, dma_dstat0, dma_int0, dma_int1);
      printf("SC2336 capture: DMA timeout after %u ms status=0x%08" PRIx32
             " amount=%" PRIu32 " source_status=0x%08" PRIx32
             " dest_status=0x%08" PRIx32 "\n",
             elapsed, intr_status, amount,
             dw_gdma_ll_channel_get_src_periph_status(dev,
                                                      SC2336_DMA_CHANNEL),
             dw_gdma_ll_channel_get_dst_periph_status(dev,
                                                      SC2336_DMA_CHANNEL));
      dw_gdma_hal_deinit(&dma);
      free(frame);
      (void)esp_ldo_release_channel(ldo_mipi_phy);
      return -ETIMEDOUT;
    }

  ret = esp_cache_msync(frame, SC2336_FRAME_BYTES,
                        ESP_CACHE_MSYNC_FLAG_DIR_M2C);
  if (ret < 0)
    {
      printf("SC2336 capture: cache sync failed: %d\n", ret);
      dw_gdma_hal_deinit(&dma);
      free(frame);
      (void)esp_ldo_release_channel(ldo_mipi_phy);
      return ret;
    }

  checksum = 0;
  for (unsigned int i = 0; i < 1024; i++)
    {
      checksum = (checksum << 5) - checksum + frame[i];
    }

  printf("SC2336 capture: FRAME DONE status=0x%08" PRIx32
         " amount=%" PRIu32 " checksum1024=0x%08" PRIx32
         " sample=%02x %02x %02x %02x\n",
         intr_status, amount, checksum, frame[0], frame[1], frame[2],
         frame[3]);

  dw_gdma_hal_deinit(&dma);
  free(frame);
  (void)esp_ldo_release_channel(ldo_mipi_phy);
  return ret < 0 ? ret : OK;
}

int i2ccmd_sc2336(FAR struct i2ctool_s *i2ctool, int argc,
                  FAR char **argv)
{
  uint8_t pid_h;
  uint8_t pid_l;
  uint8_t stream;
  uint8_t hts_h;
  uint8_t hts_l;
  uint8_t vts_h;
  uint8_t vts_l;
  uint16_t pid;
  uint16_t hts;
  uint16_t vts;
  bool init;
  bool csi;
  bool capture;
  bool capture_pattern;
  bool capture_1lane;
  bool video;
  bool v4l2;
  int fd;
  int ret;

  UNUSED(i2ctool);

  init = argc == 2 && strcmp(argv[1], "init") == 0;
  csi = argc == 2 && strcmp(argv[1], "csi") == 0;
  capture = argc == 2 && strcmp(argv[1], "capture") == 0;
  capture_pattern = argc == 3 && strcmp(argv[1], "capture") == 0 &&
                    strcmp(argv[2], "pattern") == 0;
  capture_1lane = argc == 3 && strcmp(argv[1], "capture") == 0 &&
                  strcmp(argv[2], "1lane") == 0;
  video = argc == 2 && strcmp(argv[1], "video") == 0;
  v4l2 = argc == 2 && strcmp(argv[1], "v4l2") == 0;
  init = init || csi || capture || capture_pattern || capture_1lane;
  if (argc != 1 && !init && !video && !v4l2)
    {
      printf("Usage: i2c sc2336 [init|csi|capture [pattern|1lane]|video|v4l2]\n");
      return -EINVAL;
    }

#ifdef CONFIG_VIDEO_STREAM
  if (v4l2)
    {
      if (!g_sc2336_video.registered)
        {
          return sc2336_video_capture_once();
        }

      fd = i2cdev_open(SC2336_I2C_BUS);
      if (fd < 0)
        {
          printf("SC2336 V4L2: open /dev/i2c%d failed: %d\n",
                 SC2336_I2C_BUS, errno);
          return -errno;
        }

      /* The video registration command may run in a child task.  Rebind
       * the sensor to an I2C descriptor owned by this capture command. */
      g_sc2336_video.i2c_fd = fd;
      return sc2336_video_capture_once();
    }
#endif

  fd = i2cdev_open(SC2336_I2C_BUS);
  if (fd < 0)
    {
      printf("SC2336: open /dev/i2c%d failed: %d\n",
             SC2336_I2C_BUS, errno);
      return -errno;
    }

  printf("SC2336 detect: bus=%d addr=0x%02x\n",
         SC2336_I2C_BUS, SC2336_I2C_ADDR);

  if (init)
    {
      if (capture_1lane)
        {
          printf("SC2336 init: 1920x1080 RAW10, 1-lane, 660 Mbps/lane, 25 fps\n");
          ret = sc2336_apply_1080p25_1lane(fd);
        }
      else
        {
          printf("SC2336 init: 1920x1080 RAW10, 2-lane, 405 Mbps/lane\n");
          ret = sc2336_apply_1080p30(fd);
        }
      if (ret < 0)
        {
          close(fd);
          return ret;
        }
    }

  ret = sc2336_read_reg(fd, SC2336_SENSOR_ID_H, &pid_h);
  if (ret < 0)
    {
      printf("SC2336 PID high-byte read failed: %d\n", ret);
      close(fd);
      return ret;
    }

  ret = sc2336_read_reg(fd, SC2336_SENSOR_ID_L, &pid_l);
  if (ret < 0)
    {
      printf("SC2336 PID low-byte read failed: %d\n", ret);
      close(fd);
      return ret;
    }

  if (init)
    {
      ret = sc2336_read_reg(fd, 0x0100, &stream);
      if (ret >= 0)
        {
          ret = sc2336_read_reg(fd, 0x320c, &hts_h);
        }

      if (ret >= 0)
        {
          ret = sc2336_read_reg(fd, 0x320d, &hts_l);
        }

      if (ret >= 0)
        {
          ret = sc2336_read_reg(fd, 0x320e, &vts_h);
        }

      if (ret >= 0)
        {
          ret = sc2336_read_reg(fd, 0x320f, &vts_l);
        }

      if (ret < 0)
        {
          printf("SC2336 init readback failed: %d\n", ret);
          close(fd);
          return ret;
        }

      hts = ((uint16_t)hts_h << 8) | hts_l;
      vts = ((uint16_t)vts_h << 8) | vts_l;
      printf("SC2336 init readback: stream=0x%02x HTS=0x%04x VTS=0x%04x\n",
             stream, hts, vts);
    }

  pid = ((uint16_t)pid_h << 8) | pid_l;
  printf("SC2336 PID=0x%04x (expected 0x%04x) %s\n",
         pid, SC2336_EXPECTED_PID,
         pid == SC2336_EXPECTED_PID ? "MATCH" : "MISMATCH");

#ifdef CONFIG_VIDEO_STREAM
  if (video && pid == SC2336_EXPECTED_PID)
    {
      ret = sc2336_video_register(fd);
      if (ret >= 0)
        {
          /* Keep the I2C descriptor for the registered sensor lifetime. */
          return OK;
        }
    }

#endif

  if (csi && pid == SC2336_EXPECTED_PID)
    {
      close(fd);
      return sc2336_csi_probe(2, 405);
    }

  if ((capture || capture_pattern || capture_1lane) &&
      pid == SC2336_EXPECTED_PID)
    {
      ret = sc2336_csi_capture(fd, capture_pattern, capture_1lane);
      close(fd);
      return ret;
    }

  close(fd);
  return pid == SC2336_EXPECTED_PID ? OK : -ENODEV;
}
