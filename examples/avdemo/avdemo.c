/****************************************************************************
 * apps/examples/avdemo/avdemo.c
 *
 * SPDX-License-Identifier: Apache-2.0
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

#include <errno.h>
#include <fcntl.h>
#include <mqueue.h>
#include <pthread.h>
#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/boardctl.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include <nuttx/audio/audio.h>
#include <nuttx/video/video.h>

#include <lvgl/lvgl.h>

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#define AVDEMO_CAM_WIDTH    1024
#define AVDEMO_CAM_HEIGHT   600
#define AVDEMO_CAM_BPP      2
#define AVDEMO_CAM_FRAMELEN (AVDEMO_CAM_WIDTH * AVDEMO_CAM_HEIGHT * \
                             AVDEMO_CAM_BPP)
#define AVDEMO_CAM_NBUFS    4
#define AVDEMO_CAM_ALIGN    64

#define AVDEMO_AUD_RATE     16000
#define AVDEMO_AUD_BPS      16
#define AVDEMO_AUD_CHANS    1

/* Waveform: every audio buffer is reduced to min/max pairs, one pair per
 * AVDEMO_WAVE_CHUNK samples.  The chart keeps AVDEMO_WAVE_POINTS points.
 */

#define AVDEMO_WAVE_CHUNK   256
#define AVDEMO_WAVE_POINTS  128
#define AVDEMO_WAVE_RING    256

#ifndef CONFIG_EXAMPLES_AVDEMO_VIDEO_DEVPATH
#  define CONFIG_EXAMPLES_AVDEMO_VIDEO_DEVPATH "/dev/video0"
#endif

#ifndef CONFIG_EXAMPLES_AVDEMO_AUDIO_DEVPATH
#  define CONFIG_EXAMPLES_AVDEMO_AUDIO_DEVPATH "/dev/audio/pcm_in0"
#endif

/****************************************************************************
 * Private Types
 ****************************************************************************/

struct avdemo_cam_s
{
  int fd;                                /* Video device, -1 if absent */
  int nbufs;                             /* Buffers granted by REQBUFS */
  FAR uint8_t *bufs[AVDEMO_CAM_NBUFS];   /* Capture buffers */
  int shown;                             /* Buffer index on screen, -1 */
  bool streaming;
  uint32_t frames;                       /* Total frames displayed */
  uint32_t fps_frames;                   /* Frames since last stat tick */
  lv_obj_t *img;
  lv_image_dsc_t dsc;
};

struct avdemo_aud_s
{
  int fd;                                /* Audio device, -1 if absent */
  mqd_t mq;
  char mqname[24];
  pthread_t thread;
  bool running;
  volatile bool started;                 /* Capture reached AUDIOIOC_START */
  FAR struct ap_buffer_s **apbs;
  int napbs;

  /* Waveform sample ring, filled by the audio thread, drained by an LVGL
   * timer in the UI thread.
   */

  pthread_mutex_t lock;
  int16_t ring[AVDEMO_WAVE_RING];
  volatile uint16_t widx;
  uint16_t ridx;
  volatile int16_t peak;                 /* Peak |sample| of last buffer */
};

/****************************************************************************
 * Private Data
 ****************************************************************************/

static struct avdemo_cam_s g_cam;
static struct avdemo_aud_s g_aud;

/* When true (Wi-Fi panel open) the camera keeps streaming - buffers
 * are still drained and requeued so the capture pipeline never
 * starves - but the on-screen image is not updated: a full-screen
 * 15 fps repaint under the panel makes the whole UI unusably slow.
 */

static bool g_cam_ui_paused;
static lv_obj_t *g_chart;
static lv_chart_series_t *g_series;
static lv_obj_t *g_status;

#ifdef CONFIG_EXAMPLES_LVGLDEMO
/* Shared with lvgldemo: the board watchdog thread monitors this counter
 * to detect a stalled UI loop, whichever demo is running.
 */

extern volatile uint32_t g_lvgl_heartbeat;
#  define AVDEMO_HEARTBEAT() do { g_lvgl_heartbeat++; } while (0)
#else
#  define AVDEMO_HEARTBEAT()
#endif

/****************************************************************************
 * Private Functions
 ****************************************************************************/

/****************************************************************************
 * Name: avdemo_cam_qbuf
 ****************************************************************************/

static int avdemo_cam_qbuf(int index)
{
  struct v4l2_buffer buf;

  memset(&buf, 0, sizeof(buf));
  buf.type      = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  buf.memory    = V4L2_MEMORY_USERPTR;
  buf.index     = index;
  buf.m.userptr = (unsigned long)(uintptr_t)g_cam.bufs[index];
  buf.length    = AVDEMO_CAM_FRAMELEN;

  return ioctl(g_cam.fd, VIDIOC_QBUF, (uintptr_t)&buf);
}

/****************************************************************************
 * Name: avdemo_cam_setup
 *
 * Description:
 *   Open /dev/video0, negotiate RGB565 and start streaming into three
 *   user buffers.  Failure is tolerated: the demo runs without video.
 *
 ****************************************************************************/

static void avdemo_cam_setup(void)
{
  struct v4l2_format fmt;
  struct v4l2_requestbuffers req;
  enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  int i;

  g_cam.fd = open(CONFIG_EXAMPLES_AVDEMO_VIDEO_DEVPATH,
                  O_RDWR | O_NONBLOCK);
  if (g_cam.fd < 0)
    {
      static uint32_t quiet;

      if ((quiet++ % 10) == 0)
        {
          printf("avdemo: no camera (%s: %d)\n",
                 CONFIG_EXAMPLES_AVDEMO_VIDEO_DEVPATH, errno);
        }

      return;
    }

  memset(&fmt, 0, sizeof(fmt));
  fmt.type                = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  fmt.fmt.pix.width       = AVDEMO_CAM_WIDTH;
  fmt.fmt.pix.height      = AVDEMO_CAM_HEIGHT;
  fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_RGB565;
  fmt.fmt.pix.field       = V4L2_FIELD_ANY;

  if (ioctl(g_cam.fd, VIDIOC_S_FMT, (uintptr_t)&fmt) < 0)
    {
      printf("avdemo: VIDIOC_S_FMT failed: %d\n", errno);
      goto errout;
    }

  memset(&req, 0, sizeof(req));
  req.type   = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  req.memory = V4L2_MEMORY_USERPTR;
  req.count  = AVDEMO_CAM_NBUFS;
  req.mode   = V4L2_BUF_MODE_FIFO;

  if (ioctl(g_cam.fd, VIDIOC_REQBUFS, (uintptr_t)&req) < 0)
    {
      printf("avdemo: VIDIOC_REQBUFS failed: %d\n", errno);
      goto errout;
    }

  /* The framework may grant fewer containers than requested */

  g_cam.nbufs = (req.count < AVDEMO_CAM_NBUFS) ?
                (int)req.count : AVDEMO_CAM_NBUFS;

  for (i = 0; i < g_cam.nbufs; i++)
    {
      g_cam.bufs[i] = memalign(AVDEMO_CAM_ALIGN, AVDEMO_CAM_FRAMELEN);
      if (g_cam.bufs[i] == NULL)
        {
          printf("avdemo: frame buffer alloc failed\n");
          goto errout;
        }

      if (avdemo_cam_qbuf(i) < 0)
        {
          printf("avdemo: VIDIOC_QBUF(%d) failed: %d\n", i, errno);
          goto errout;
        }
    }

  if (ioctl(g_cam.fd, VIDIOC_STREAMON, (uintptr_t)&type) < 0)
    {
      printf("avdemo: VIDIOC_STREAMON failed: %d\n", errno);
      goto errout;
    }

  g_cam.shown     = -1;
  g_cam.streaming = true;
  printf("avdemo: camera streaming %dx%d RGB565\n",
         AVDEMO_CAM_WIDTH, AVDEMO_CAM_HEIGHT);
  return;

errout:

  /* Quiesce the driver BEFORE freeing the buffers: the queued buffers
   * are live DMA targets from the moment they are QBUF'd, and close()
   * is what stops the capture DMA.  Freeing first hands the memory to
   * the heap while the sensor may still be streaming into it — that
   * exact sequence buried an LVGL draw unit under a camera frame and
   * hard-crashed the system (illegal instruction at 0x4b464b46).
   */

  ioctl(g_cam.fd, VIDIOC_STREAMOFF, (uintptr_t)&type);
  close(g_cam.fd);
  g_cam.fd = -1;

  for (i = 0; i < AVDEMO_CAM_NBUFS; i++)
    {
      free(g_cam.bufs[i]);
      g_cam.bufs[i] = NULL;
    }

  g_cam.nbufs = 0;
}

/****************************************************************************
 * Name: avdemo_cam_timer_cb
 *
 * Description:
 *   LVGL timer: dequeue the newest completed frame (non-blocking), point
 *   the on-screen image at it and requeue the frame it replaces.
 *
 ****************************************************************************/

static void avdemo_cam_timer_cb(FAR lv_timer_t *timer)
{
  struct v4l2_buffer buf;
  int prev;

  if (!g_cam.streaming)
    {
      /* Camera absent or failed: retry the whole setup every ~3 s so a
       * flaky cable or late module attach recovers without a reboot.
       */

      static uint32_t retry;

      if (++retry >= 90)
        {
          retry = 0;
          avdemo_cam_setup();
        }

      return;
    }

  /* Drain every completed frame, keep only the newest for display and
   * requeue the rest immediately.  If the framework ever finds no
   * vacant container at a frame boundary it stops and restarts the
   * whole pipeline (sensor I2C writes included), so starving it is
   * expensive and destabilizing.
   */

  bool got = false;

  for (; ; )
    {
      memset(&buf, 0, sizeof(buf));
      buf.type   = V4L2_BUF_TYPE_VIDEO_CAPTURE;
      buf.memory = V4L2_MEMORY_USERPTR;

      if (ioctl(g_cam.fd, VIDIOC_DQBUF, (uintptr_t)&buf) < 0)
        {
          break;                         /* EAGAIN: no more frames */
        }

      prev        = g_cam.shown;
      g_cam.shown = buf.index;
      got         = true;

      g_cam.frames++;
      g_cam.fps_frames++;

      if (prev >= 0)
        {
          avdemo_cam_qbuf(prev);
        }
    }

  if (!got || g_cam_ui_paused)
    {
      return;
    }

  static bool src_set = false;

  g_cam.dsc.data = g_cam.bufs[g_cam.shown];
  if (!src_set)
    {
      src_set = true;
      lv_image_set_src(g_cam.img, &g_cam.dsc);
      lv_obj_remove_flag(g_cam.img, LV_OBJ_FLAG_HIDDEN);
    }
  else
    {
      lv_obj_invalidate(g_cam.img);
    }
}

/****************************************************************************
 * Name: avdemo_aud_push
 *
 * Description:
 *   Reduce one captured PCM buffer to waveform min/max pairs (audio
 *   thread context, no LVGL calls here).
 *
 ****************************************************************************/

static void avdemo_aud_push(FAR const int16_t *samp, size_t nsamples)
{
  size_t i;
  size_t j;
  int16_t peak = 0;

  pthread_mutex_lock(&g_aud.lock);

  for (i = 0; i + AVDEMO_WAVE_CHUNK <= nsamples; i += AVDEMO_WAVE_CHUNK)
    {
      int16_t maxv = -32768;
      int16_t minv = 32767;

      for (j = 0; j < AVDEMO_WAVE_CHUNK; j++)
        {
          int16_t s = samp[i + j];
          if (s > maxv)
            {
              maxv = s;
            }

          if (s < minv)
            {
              minv = s;
            }
        }

      g_aud.ring[g_aud.widx++ % AVDEMO_WAVE_RING] = maxv;
      g_aud.ring[g_aud.widx++ % AVDEMO_WAVE_RING] = minv;

      if (maxv > peak)
        {
          peak = maxv;
        }

      if ((int16_t)-minv > peak)
        {
          peak = -minv;
        }
    }

  g_aud.peak = peak;
  pthread_mutex_unlock(&g_aud.lock);
}

/****************************************************************************
 * Name: avdemo_aud_enqueue
 ****************************************************************************/

static int avdemo_aud_enqueue(FAR struct ap_buffer_s *apb)
{
  struct audio_buf_desc_s desc;

  apb->nbytes  = apb->nmaxbytes;
  apb->curbyte = 0;
  apb->flags   = 0;

  memset(&desc, 0, sizeof(desc));
  desc.numbytes = apb->nbytes;
  desc.u.buffer = apb;

  return ioctl(g_aud.fd, AUDIOIOC_ENQUEUEBUFFER, (uintptr_t)&desc);
}

/****************************************************************************
 * Name: avdemo_aud_thread
 *
 * Description:
 *   Microphone capture pump: configure /dev/audio/pcm_in0 for 16 kHz,
 *   16-bit mono PCM and feed dequeued buffers into the waveform ring.
 *
 ****************************************************************************/

static FAR void *avdemo_aud_thread(FAR void *arg)
{
  struct audio_caps_desc_s cap_desc;
  struct ap_buffer_info_s buf_info;
  struct audio_msg_s msg;
  struct mq_attr attr;
  unsigned int prio;
  ssize_t size;
  int attempt = 0;
  int ret;
  int i;

retry:
  if (!g_aud.running)
    {
      return NULL;
    }

  if (attempt++ > 0)
    {
      /* The codec device registers asynchronously during board bring-up,
       * and its I2C configuration must land before the first LVGL render
       * starts hammering the PSRAM (writes issued after that point have
       * been observed to NACK for the rest of the session).  Retry fast
       * while the quiet boot window lasts, then back off.
       */

      if (attempt <= 20)
        {
          usleep(100 * 1000);
        }
      else
        {
          sleep(3);
        }
    }

  g_aud.fd = open(CONFIG_EXAMPLES_AVDEMO_AUDIO_DEVPATH, O_RDWR);
  if (g_aud.fd < 0)
    {
      if (attempt == 1)
        {
          printf("avdemo: no microphone (%s: %d)\n",
                 CONFIG_EXAMPLES_AVDEMO_AUDIO_DEVPATH, errno);
        }

      goto retry;
    }

  ret = ioctl(g_aud.fd, AUDIOIOC_RESERVE, 0);
  if (ret < 0)
    {
      printf("avdemo: audio reserve failed: %d\n", errno);
      goto errout_close;
    }

  memset(&cap_desc, 0, sizeof(cap_desc));
  cap_desc.caps.ac_len            = sizeof(struct audio_caps_s);
  cap_desc.caps.ac_type           = AUDIO_TYPE_INPUT;
  cap_desc.caps.ac_channels       = AVDEMO_AUD_CHANS;
  cap_desc.caps.ac_controls.hw[0] = AVDEMO_AUD_RATE;
  cap_desc.caps.ac_controls.b[2]  = AVDEMO_AUD_BPS;
  cap_desc.caps.ac_controls.b[3]  = AVDEMO_AUD_RATE >> 16;
  cap_desc.caps.ac_subtype        = AUDIO_FMT_PCM;

  ret = ioctl(g_aud.fd, AUDIOIOC_CONFIGURE, (uintptr_t)&cap_desc);
  if (ret < 0)
    {
      printf("avdemo: audio configure failed: %d\n", errno);
      goto errout_release;
    }

  if (ioctl(g_aud.fd, AUDIOIOC_GETBUFFERINFO, (uintptr_t)&buf_info) < 0)
    {
      buf_info.nbuffers    = 4;
      buf_info.buffer_size = 8192;
    }

  snprintf(g_aud.mqname, sizeof(g_aud.mqname), "/tmp/%0lx",
           (unsigned long)((uintptr_t)&g_aud));

  memset(&attr, 0, sizeof(attr));
  attr.mq_maxmsg  = buf_info.nbuffers + 4;
  attr.mq_msgsize = sizeof(struct audio_msg_s);

  g_aud.mq = mq_open(g_aud.mqname, O_RDWR | O_CREAT, 0644, &attr);
  if (g_aud.mq == (mqd_t)-1)
    {
      printf("avdemo: mq_open failed: %d\n", errno);
      goto errout_release;
    }

  ret = ioctl(g_aud.fd, AUDIOIOC_REGISTERMQ, (uintptr_t)g_aud.mq);
  if (ret < 0)
    {
      printf("avdemo: audio registermq failed: %d\n", errno);
      goto errout_mq;
    }

  g_aud.napbs = buf_info.nbuffers;
  g_aud.apbs  = calloc(g_aud.napbs, sizeof(FAR struct ap_buffer_s *));
  if (g_aud.apbs == NULL)
    {
      goto errout_mq;
    }

  for (i = 0; i < g_aud.napbs; i++)
    {
      struct audio_buf_desc_s desc;

      memset(&desc, 0, sizeof(desc));
      desc.numbytes  = buf_info.buffer_size;
      desc.u.pbuffer = &g_aud.apbs[i];

      ret = ioctl(g_aud.fd, AUDIOIOC_ALLOCBUFFER, (uintptr_t)&desc);
      if (ret != sizeof(desc))
        {
          printf("avdemo: audio allocbuffer failed: %d\n", errno);
          goto errout_bufs;
        }

      if (avdemo_aud_enqueue(g_aud.apbs[i]) < 0)
        {
          printf("avdemo: audio enqueue failed: %d\n", errno);
          goto errout_bufs;
        }
    }

  ret = ioctl(g_aud.fd, AUDIOIOC_START, 0);
  if (ret < 0)
    {
      printf("avdemo: audio start failed: %d\n", errno);
      goto errout_bufs;
    }

  printf("avdemo: microphone capturing %d Hz %d-bit mono\n",
         AVDEMO_AUD_RATE, AVDEMO_AUD_BPS);

  g_aud.started = true;

  while (g_aud.running)
    {
      size = mq_receive(g_aud.mq, (FAR char *)&msg, sizeof(msg), &prio);
      if (size != sizeof(msg))
        {
          continue;
        }

      if (msg.msg_id == AUDIO_MSG_DEQUEUE)
        {
          FAR struct ap_buffer_s *apb = msg.u.ptr;

          avdemo_aud_push((FAR const int16_t *)apb->samp,
                          apb->nbytes / sizeof(int16_t));

          if (g_aud.running)
            {
              avdemo_aud_enqueue(apb);
            }
        }
      else if (msg.msg_id == AUDIO_MSG_COMPLETE)
        {
          break;
        }
    }

  ioctl(g_aud.fd, AUDIOIOC_STOP, 0);

errout_bufs:
  for (i = 0; i < g_aud.napbs; i++)
    {
      if (g_aud.apbs[i] != NULL)
        {
          struct audio_buf_desc_s desc;

          memset(&desc, 0, sizeof(desc));
          desc.u.buffer = g_aud.apbs[i];
          ioctl(g_aud.fd, AUDIOIOC_FREEBUFFER, (uintptr_t)&desc);
        }
    }

  free(g_aud.apbs);
  g_aud.apbs = NULL;
  ioctl(g_aud.fd, AUDIOIOC_UNREGISTERMQ, (uintptr_t)g_aud.mq);

errout_mq:
  mq_close(g_aud.mq);
  mq_unlink(g_aud.mqname);

errout_release:
  ioctl(g_aud.fd, AUDIOIOC_RELEASE, 0);

errout_close:
  close(g_aud.fd);
  g_aud.fd = -1;

  /* Transient bus glitches at boot are common on this board: retry the
   * whole bring-up instead of giving up for the session.
   */

  goto retry;
}

/****************************************************************************
 * Name: avdemo_wave_timer_cb
 *
 * Description:
 *   LVGL timer: move freshly captured waveform points from the audio
 *   ring into the chart.
 *
 ****************************************************************************/

static void avdemo_wave_timer_cb(FAR lv_timer_t *timer)
{
  int budget = AVDEMO_WAVE_RING;

  pthread_mutex_lock(&g_aud.lock);

  while (g_aud.ridx != g_aud.widx && budget-- > 0)
    {
      lv_chart_set_next_value(g_chart, g_series,
                              g_aud.ring[g_aud.ridx++ % AVDEMO_WAVE_RING]);
    }

  /* If the UI fell far behind, jump to the newest data */

  if ((uint16_t)(g_aud.widx - g_aud.ridx) > AVDEMO_WAVE_RING)
    {
      g_aud.ridx = g_aud.widx;
    }

  pthread_mutex_unlock(&g_aud.lock);
}

/****************************************************************************
 * Name: avdemo_stat_timer_cb
 ****************************************************************************/

static void avdemo_stat_timer_cb(FAR lv_timer_t *timer)
{
  int level = (g_aud.peak * 100) / 32767;
  static unsigned ticks;

  printf("avdemo: cam=%d fps=%u frames=%u mic=%d peak=%d%%\n",
         g_cam.streaming, (unsigned)g_cam.fps_frames,
         (unsigned)g_cam.frames, g_aud.fd, level);

  /* Self-healing: every 5 s force a whole-screen invalidation.  LVGL's
   * dirty-area bookkeeping has been seen to lose the resume of the
   * refresh timer mid-session; the periodic invalidation bounds any
   * such stall to 5 s for the cost of one extra full redraw.
   */

  if ((++ticks % 5) == 0)
    {
      lv_obj_invalidate(lv_screen_active());
    }

  lv_label_set_text_fmt(g_status, "CAM %s %u fps   MIC %s %d%%",
                        g_cam.streaming ? "" : "--",
                        (unsigned)g_cam.fps_frames,
                        g_aud.fd >= 0 ? "" : "--",
                        level);
  g_cam.fps_frames = 0;
}

/****************************************************************************
 * Wi-Fi panel (esp-hosted C6 radio via the board layer)
 ****************************************************************************/

#ifdef CONFIG_ESP32P4_SDMMC

int esph_ui_request_scan(void);
int esph_ui_request_connect(FAR const char *ssid, FAR const char *pass);
int esph_ui_copy_status(FAR char *dst, size_t dstlen);
int esph_ui_copy_scan(FAR char *dst, size_t dstlen, FAR int *count);

static lv_obj_t *g_wifi_panel;
static lv_obj_t *g_wifi_status;
static lv_obj_t *g_wifi_list;
static lv_obj_t *g_wifi_pass;
static lv_obj_t *g_wifi_kb;
static char g_wifi_sel[33];
static char g_wifi_scan_last[512];

static void avdemo_wifi_ssid_cb(FAR lv_event_t *e)
{
  FAR lv_obj_t *btn = lv_event_get_target(e);
  FAR const char *txt = lv_list_get_button_text(g_wifi_list, btn);
  uint32_t i;

  if (txt == NULL)
    {
      return;
    }

  strncpy(g_wifi_sel, txt, sizeof(g_wifi_sel) - 1);
  g_wifi_sel[sizeof(g_wifi_sel) - 1] = '\0';

  for (i = 0; i < lv_obj_get_child_count(g_wifi_list); i++)
    {
      lv_obj_set_style_bg_color(lv_obj_get_child(g_wifi_list, i),
                                lv_color_white(), LV_PART_MAIN);
    }

  lv_obj_set_style_bg_color(btn, lv_palette_lighten(LV_PALETTE_GREEN, 3),
                            LV_PART_MAIN);
}

static void avdemo_wifi_scan_cb(FAR lv_event_t *e)
{
  (void)e;
  esph_ui_request_scan();
}

static void avdemo_wifi_connect_cb(FAR lv_event_t *e)
{
  (void)e;

  if (g_wifi_sel[0] == '\0')
    {
      lv_label_set_text(g_wifi_status, "Pick a network first");
      return;
    }

  esph_ui_request_connect(g_wifi_sel, lv_textarea_get_text(g_wifi_pass));
}

static void avdemo_wifi_close_cb(FAR lv_event_t *e)
{
  (void)e;
  lv_obj_add_flag(g_wifi_panel, LV_OBJ_FLAG_HIDDEN);

  /* Resume the camera view */

  g_cam_ui_paused = false;
  if (g_cam.img != NULL && g_cam.streaming)
    {
      lv_obj_remove_flag(g_cam.img, LV_OBJ_FLAG_HIDDEN);
      lv_obj_invalidate(g_cam.img);
    }
}

static void avdemo_wifi_timer_cb(FAR lv_timer_t *timer)
{
  char buf[512];
  int count = 0;

  (void)timer;

  if (g_wifi_panel == NULL ||
      lv_obj_has_flag(g_wifi_panel, LV_OBJ_FLAG_HIDDEN))
    {
      return;
    }

  esph_ui_copy_status(buf, sizeof(buf));
  lv_label_set_text(g_wifi_status, buf);

  esph_ui_copy_scan(buf, sizeof(buf), &count);
  if (strcmp(buf, g_wifi_scan_last) != 0)
    {
      FAR char *save;
      FAR char *tok;

      strncpy(g_wifi_scan_last, buf, sizeof(g_wifi_scan_last) - 1);
      g_wifi_scan_last[sizeof(g_wifi_scan_last) - 1] = '\0';

      lv_obj_clean(g_wifi_list);
      tok = strtok_r(buf, "\n", &save);
      while (tok != NULL)
        {
          FAR lv_obj_t *btn =
            lv_list_add_button(g_wifi_list, LV_SYMBOL_WIFI, tok);
          lv_obj_add_event_cb(btn, avdemo_wifi_ssid_cb,
                              LV_EVENT_CLICKED, NULL);
          tok = strtok_r(NULL, "\n", &save);
        }
    }
}

static void avdemo_wifi_open_cb(FAR lv_event_t *e)
{
  FAR lv_obj_t *top;
  FAR lv_obj_t *btn;
  FAR lv_obj_t *lbl;

  (void)e;

  /* Pause the camera view: a full-screen 15 fps repaint underneath
   * makes the panel unusably laggy.  Streaming continues (buffers
   * keep cycling), only the display update stops.
   */

  g_cam_ui_paused = true;
  if (g_cam.img != NULL)
    {
      lv_obj_add_flag(g_cam.img, LV_OBJ_FLAG_HIDDEN);
    }

  if (g_wifi_panel != NULL)
    {
      lv_obj_remove_flag(g_wifi_panel, LV_OBJ_FLAG_HIDDEN);
      return;
    }

  top = lv_layer_top();

  g_wifi_panel = lv_obj_create(top);
  lv_obj_set_size(g_wifi_panel, 660, 584);
  lv_obj_center(g_wifi_panel);
  lv_obj_set_style_pad_all(g_wifi_panel, 10, LV_PART_MAIN);
  lv_obj_remove_flag(g_wifi_panel, LV_OBJ_FLAG_SCROLLABLE);

  lbl = lv_label_create(g_wifi_panel);
  lv_label_set_text(lbl, "Wi-Fi");
  lv_obj_align(lbl, LV_ALIGN_TOP_LEFT, 0, 0);

  btn = lv_button_create(g_wifi_panel);
  lv_obj_set_size(btn, 60, 36);
  lv_obj_align(btn, LV_ALIGN_TOP_RIGHT, 0, -4);
  lv_obj_add_event_cb(btn, avdemo_wifi_close_cb, LV_EVENT_CLICKED, NULL);
  lbl = lv_label_create(btn);
  lv_label_set_text(lbl, LV_SYMBOL_CLOSE);
  lv_obj_center(lbl);

  btn = lv_button_create(g_wifi_panel);
  lv_obj_set_size(btn, 90, 36);
  lv_obj_align(btn, LV_ALIGN_TOP_RIGHT, -70, -4);
  lv_obj_add_event_cb(btn, avdemo_wifi_scan_cb, LV_EVENT_CLICKED, NULL);
  lbl = lv_label_create(btn);
  lv_label_set_text(lbl, "Scan");
  lv_obj_center(lbl);

  g_wifi_status = lv_label_create(g_wifi_panel);
  lv_label_set_text(g_wifi_status, "...");
  lv_obj_set_width(g_wifi_status, 640);
  lv_label_set_long_mode(g_wifi_status, LV_LABEL_LONG_DOT);
  lv_obj_align(g_wifi_status, LV_ALIGN_TOP_LEFT, 0, 40);

  g_wifi_list = lv_list_create(g_wifi_panel);
  lv_obj_set_size(g_wifi_list, 640, 170);
  lv_obj_align(g_wifi_list, LV_ALIGN_TOP_LEFT, 0, 66);

  g_wifi_pass = lv_textarea_create(g_wifi_panel);
  lv_obj_set_size(g_wifi_pass, 470, 44);
  lv_obj_align(g_wifi_pass, LV_ALIGN_TOP_LEFT, 0, 244);
  lv_textarea_set_one_line(g_wifi_pass, true);
  lv_textarea_set_password_mode(g_wifi_pass, true);
  lv_textarea_set_placeholder_text(g_wifi_pass, "Password");

  btn = lv_button_create(g_wifi_panel);
  lv_obj_set_size(btn, 150, 44);
  lv_obj_align(btn, LV_ALIGN_TOP_RIGHT, 0, 244);
  lv_obj_add_event_cb(btn, avdemo_wifi_connect_cb, LV_EVENT_CLICKED, NULL);
  lbl = lv_label_create(btn);
  lv_label_set_text(lbl, "Connect");
  lv_obj_center(lbl);

  g_wifi_kb = lv_keyboard_create(g_wifi_panel);
  lv_obj_set_size(g_wifi_kb, 640, 230);
  lv_obj_align(g_wifi_kb, LV_ALIGN_BOTTOM_MID, 0, 0);
  lv_keyboard_set_textarea(g_wifi_kb, g_wifi_pass);

  lv_timer_create(avdemo_wifi_timer_cb, 700, NULL);
}

static void avdemo_wifi_button(FAR lv_obj_t *scr)
{
  FAR lv_obj_t *btn;
  FAR lv_obj_t *lbl;

  btn = lv_button_create(scr);
  lv_obj_set_size(btn, 110, 44);
  lv_obj_align(btn, LV_ALIGN_TOP_RIGHT, -12, 8);
  lv_obj_add_event_cb(btn, avdemo_wifi_open_cb, LV_EVENT_CLICKED, NULL);
  lbl = lv_label_create(btn);
  lv_label_set_text(lbl, LV_SYMBOL_WIFI " Wi-Fi");
  lv_obj_center(lbl);
}
#endif /* CONFIG_ESP32P4_SDMMC */

/****************************************************************************
 * Name: avdemo_touch_test_cb
 *
 * Description:
 *   Touch verification: each tap increments the button label, giving
 *   instant on-screen proof that the touch chain works.
 *
 ****************************************************************************/

static void avdemo_touch_test_cb(FAR lv_event_t *e)
{
  static uint32_t taps;
  FAR lv_obj_t *lbl = (FAR lv_obj_t *)lv_event_get_user_data(e);

  lv_label_set_text_fmt(lbl, "touch %u", (unsigned)++taps);
}

/****************************************************************************
 * Name: avdemo_create_ui
 ****************************************************************************/

static void avdemo_create_ui(void)
{
  lv_obj_t *scr = lv_screen_active();
  lv_obj_t *panel;
  lv_obj_t *tbtn;
  lv_obj_t *tlbl;

  lv_obj_set_style_bg_color(scr, lv_color_hex(0x101418), LV_PART_MAIN);

  /* Camera view: full-screen image, hidden until the first frame */

  g_cam.dsc.header.magic = LV_IMAGE_HEADER_MAGIC;
  g_cam.dsc.header.cf    = LV_COLOR_FORMAT_RGB565;
  g_cam.dsc.header.w     = AVDEMO_CAM_WIDTH;
  g_cam.dsc.header.h     = AVDEMO_CAM_HEIGHT;
  g_cam.dsc.header.stride = AVDEMO_CAM_WIDTH * AVDEMO_CAM_BPP;
  g_cam.dsc.data_size    = AVDEMO_CAM_FRAMELEN;
  g_cam.dsc.data         = NULL;

  g_cam.img = lv_image_create(scr);
  lv_obj_set_pos(g_cam.img, 0, 0);
  lv_obj_add_flag(g_cam.img, LV_OBJ_FLAG_HIDDEN);

  /* Waveform overlay along the bottom edge */

  panel = lv_obj_create(scr);
  lv_obj_set_size(panel, AVDEMO_CAM_WIDTH - 16, 168);
  lv_obj_align(panel, LV_ALIGN_BOTTOM_MID, 0, -8);
  lv_obj_set_style_bg_color(panel, lv_color_black(), LV_PART_MAIN);
  lv_obj_set_style_bg_opa(panel, LV_OPA_50, LV_PART_MAIN);
  lv_obj_set_style_border_width(panel, 0, LV_PART_MAIN);
  lv_obj_set_style_pad_all(panel, 8, LV_PART_MAIN);
  lv_obj_remove_flag(panel, LV_OBJ_FLAG_SCROLLABLE);

  g_chart = lv_chart_create(panel);
  lv_obj_set_size(g_chart, lv_pct(100), lv_pct(100));
  lv_obj_center(g_chart);
  lv_chart_set_type(g_chart, LV_CHART_TYPE_LINE);
  lv_chart_set_point_count(g_chart, AVDEMO_WAVE_POINTS);
  lv_chart_set_update_mode(g_chart, LV_CHART_UPDATE_MODE_SHIFT);
  lv_chart_set_range(g_chart, LV_CHART_AXIS_PRIMARY_Y, -32768, 32767);
  lv_chart_set_div_line_count(g_chart, 3, 0);
  lv_obj_set_style_bg_opa(g_chart, LV_OPA_TRANSP, LV_PART_MAIN);
  lv_obj_set_style_border_width(g_chart, 0, LV_PART_MAIN);
  lv_obj_set_style_size(g_chart, 0, 0, LV_PART_INDICATOR);

  g_series = lv_chart_add_series(g_chart, lv_palette_main(LV_PALETTE_GREEN),
                                 LV_CHART_AXIS_PRIMARY_Y);
  lv_chart_set_all_value(g_chart, g_series, 0);

  /* Status line, top-left */

  g_status = lv_label_create(scr);
  lv_obj_align(g_status, LV_ALIGN_TOP_LEFT, 12, 8);
  lv_obj_set_style_text_color(g_status, lv_color_white(), LV_PART_MAIN);
  lv_label_set_text(g_status, "starting...");

#ifdef CONFIG_ESP32P4_SDMMC
  avdemo_wifi_button(scr);
#endif

  /* Touch verification button, top-right (below the Wi-Fi button) */

  tbtn = lv_button_create(scr);
  lv_obj_set_size(tbtn, 110, 44);
  lv_obj_align(tbtn, LV_ALIGN_TOP_RIGHT, -12, 60);
  tlbl = lv_label_create(tbtn);
  lv_label_set_text(tlbl, "touch 0");
  lv_obj_center(tlbl);
  lv_obj_add_event_cb(tbtn, avdemo_touch_test_cb, LV_EVENT_CLICKED, tlbl);

  lv_timer_create(avdemo_cam_timer_cb, 33, NULL);
  lv_timer_create(avdemo_wave_timer_cb, 100, NULL);
  lv_timer_create(avdemo_stat_timer_cb, 1000, NULL);
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: main
 ****************************************************************************/

static void avdemo_sigusr1(int signo)
{
  /* No-op: the board watchdog sends SIGUSR1 to break this thread out of
   * a sleep whose timer wakeup was lost (a real platform failure mode
   * pinned down by stack scans).  The signal only needs to interrupt
   * the blocking call with EINTR.
   */

  (void)signo;
}

int main(int argc, FAR char *argv[])
{
  lv_nuttx_dsc_t info;
  lv_nuttx_result_t result;
  sigset_t sigset;
  struct sigaction act;

  /* Make the watchdog's rescue kick (SIGUSR1) actually deliverable:
   * install a no-op handler and unblock the signal.  Without this a
   * kthread keeps SIGUSR1 masked and every kick is silently swallowed.
   */

  memset(&act, 0, sizeof(act));
  act.sa_handler = avdemo_sigusr1;
  sigaction(SIGUSR1, &act, NULL);
  sigemptyset(&sigset);
  sigaddset(&sigset, SIGUSR1);
  sigprocmask(SIG_UNBLOCK, &sigset, NULL);

  if (lv_is_initialized())
    {
      printf("avdemo: LVGL already initialized (is lvgldemo running?)\n");
      return 1;
    }

#if defined(CONFIG_NSH_ARCHINIT) || !defined(CONFIG_BOARDCTL)
  /* Board is initialized by NSH */
#else
  boardctl(BOARDIOC_INIT, 0);
#endif

  g_cam.fd    = -1;
  g_cam.shown = -1;
  g_aud.fd    = -1;
  pthread_mutex_init(&g_aud.lock, NULL);

  /* Bring the microphone and camera up BEFORE any LVGL initialization.
   * The first LVGL render floods the PSRAM/AXI fabric, and I2C
   * configuration writes issued during or after that burst have been
   * observed (wire-level) to NACK for the rest of the session, on a
   * per-device lottery.  All writes issued in the quiet window before
   * the burst have never been seen to fail.  Order: audio first (I2C
   * writes only), then the camera (I2C writes, then its stream starts
   * its own heavy DMA traffic), then LVGL.
   */

  g_aud.running = true;
  if (pthread_create(&g_aud.thread, NULL, avdemo_aud_thread, NULL) != 0)
    {
      printf("avdemo: audio thread create failed\n");
      g_aud.running = false;
    }
  else
    {
      int waited;

      /* Multi-tick sleeps only: 1-tick sleeps (10 ms at the default
       * tick) intermittently lose their wakeup on this platform, and a
       * lost 10 ms sleep here parked the whole boot (stack-scan
       * verified).  15 x 100 ms bounds the wait to the same ~1.5 s.
       */

      for (waited = 0; waited < 15 && !g_aud.started; waited++)
        {
          usleep(100 * 1000);
        }
    }

  avdemo_cam_setup();

  lv_init();
  lv_nuttx_dsc_init(&info);

#ifdef CONFIG_INPUT_TOUCHSCREEN
  info.input_path = CONFIG_EXAMPLES_AVDEMO_INPUT_DEVPATH;
#endif

  lv_nuttx_init(&info, &result);

  if (result.disp == NULL)
    {
      printf("avdemo: display initialization failed\n");
      return 1;
    }

  if (result.indev != NULL)
    {
      lv_indev_set_display(result.indev, result.disp);
    }

  avdemo_create_ui();

  while (1)
    {
      uint32_t idle;

      /* The v9 refresh timer pauses itself after each run and is
       * supposed to be resumed by the next invalidation.  That resume
       * has been observed to go missing mid-session (UI frozen while
       * every other timer keeps running).  Forcing it back on each
       * loop iteration costs nothing when it is already running.
       */

      if (result.disp != NULL)
        {
          FAR lv_timer_t *refr = lv_display_get_refr_timer(result.disp);

          if (refr != NULL)
            {
              lv_timer_resume(refr);
            }
        }

      idle = lv_timer_handler();

      AVDEMO_HEARTBEAT();

      /* Same guard as lvgldemo: LV_NO_TIMER_READY (0xFFFFFFFF) would
       * overflow usleep into a ~71 minute sleep; cap the idle time.
       */

      if (idle > 33)
        {
          idle = 33;
        }
      else if (idle == 0)
        {
          idle = 1;
        }

      usleep(idle * 1000);
    }

  return 0;
}
