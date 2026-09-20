/****************************************************************************
 * apps/examples/campreview/campreview_main.c
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
 * Camera preview: RGB565 frames from /dev/video0 onto RGB565 /dev/fb0.
 *
 * There are two paths, picked at runtime from what fb0 reports.
 *
 * Zero-copy (fb0 is double buffered: yres_virtual covers two frames and a
 * single mmap covers both):
 *
 *   The two framebuffer halves are handed to the capture driver as
 *   V4L2_MEMORY_USERPTR buffers, so the camera DMA writes pixels straight
 *   into display memory. Each dequeued frame is published with
 *   FBIOPAN_DISPLAY at the matching yoffset and requeued. No memcpy runs
 *   at all, and the CPU never writes the framebuffer, so its cache lines
 *   stay clean and cannot race the CSI frame-done invalidate.
 *
 * Copy (fb0 exposes a single buffer):
 *
 *   Two private capture buffers, each dequeued frame copied into the
 *   mmap'ed framebuffer, then FBIOPAN_DISPLAY to write it back for the
 *   display DMA.
 *
 * FBIOPAN_DISPLAY is the only ioctl that reaches the driver's pandisplay
 * hook, which is where both the buffer flip and the cache writeback live.
 * FBIO_UPDATE is not usable: it needs CONFIG_FB_UPDATE and routes to
 * updatearea, which this framebuffer does not implement.
 ****************************************************************************/

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <time.h>
#include <unistd.h>
#include <malloc.h>

#include <sys/ioctl.h>
#include <sys/mman.h>

#include <nuttx/video/fb.h>
#include <nuttx/video/video.h>
#include <nuttx/video/v4l2_cap.h>

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#define CAMPREVIEW_VIDEO_DEV  "/dev/video0"
#define CAMPREVIEW_FB_DEV     "/dev/fb0"

/* Two capture buffers so the sensor can fill one while the other is on
 * screen.  More would only add latency, the panel cannot show them.
 */

#define CAMPREVIEW_BUFNUM     2

/* Buffer pointers handed to VIDIOC_QBUF must be at least 32-byte aligned.
 * Use a full 64-byte cache line, which is what the CSI driver aligns its
 * own DMA buffers to, so that a DMA write never shares a cache line with
 * anything else.  The framebuffer halves are already 64-byte aligned.
 */

#define CAMPREVIEW_BUFALIGN   64

/* Print a progress line every N frames instead of once per frame. */

#define CAMPREVIEW_LOG_PERIOD 60

/****************************************************************************
 * Private Types
 ****************************************************************************/

struct campreview_buf_s
{
  FAR uint8_t *start;
  uint32_t     length;
};

struct campreview_state_s
{
  int    fb_fd;
  int    v_fd;
  bool   streaming;
  bool   zerocopy;                     /* Camera DMA writes fb directly */

  FAR uint8_t *fbmem;                  /* mmap'ed framebuffer */
  size_t       fblen;

  struct fb_videoinfo_s vinfo;
  struct fb_planeinfo_s pinfo;

  uint32_t framesize;                  /* Bytes in one captured frame */
  uint32_t halfsize;                   /* Bytes in one framebuffer half */
  uint8_t  nbuffers;                   /* Framebuffer halves: 1 or 2 */
  FAR uint8_t *halves[CAMPREVIEW_BUFNUM];

  /* Private capture buffers, fallback (copy) path only. */

  struct campreview_buf_s bufs[CAMPREVIEW_BUFNUM];
  uint8_t nbufs;
};

/****************************************************************************
 * Private Data
 ****************************************************************************/

static volatile bool g_campreview_stop;

/****************************************************************************
 * Private Functions
 ****************************************************************************/

/****************************************************************************
 * Name: campreview_usage
 ****************************************************************************/

static void campreview_usage(FAR const char *progname)
{
  printf("Usage: %s [nframes]\n", progname);
  printf("       %s -h\n", progname);
  printf("\n");
  printf("Preview RGB565 frames from %s on the RGB565 framebuffer %s.\n",
         CAMPREVIEW_VIDEO_DEV, CAMPREVIEW_FB_DEV);
  printf("\n");
  printf("If %s is double buffered the capture DMA fills the framebuffer\n",
         CAMPREVIEW_FB_DEV);
  printf("halves directly and pages between them, with no copy at all.\n");
  printf("Otherwise each frame is copied into the single buffer.\n");
  printf("\n");
  printf("  nframes  Number of frames to preview.  0 (the default) means\n");
  printf("           run until an error occurs or SIGINT is received.\n");
}

/****************************************************************************
 * Name: campreview_sigint
 ****************************************************************************/

static void campreview_sigint(int signo)
{
  g_campreview_stop = true;
}

/****************************************************************************
 * Name: campreview_now_ms
 ****************************************************************************/

static uint64_t campreview_now_ms(void)
{
  struct timespec ts;

  clock_gettime(CLOCK_MONOTONIC, &ts);
  return (uint64_t)ts.tv_sec * 1000 + (uint64_t)(ts.tv_nsec / 1000000);
}

/****************************************************************************
 * Name: campreview_free_bufs
 *
 * Description:
 *   Release the private capture buffers.  Only the copy path allocates
 *   any, so this is a no-op on the zero-copy path.
 *
 ****************************************************************************/

static void campreview_free_bufs(FAR struct campreview_state_s *st)
{
  uint8_t i;

  for (i = 0; i < st->nbufs; i++)
    {
      free(st->bufs[i].start);
      st->bufs[i].start  = NULL;
      st->bufs[i].length = 0;
    }

  st->nbufs = 0;
}

/****************************************************************************
 * Name: campreview_fb_open
 *
 * Description:
 *   Open the framebuffer, query its geometry, work out how many buffers it
 *   exposes and map the whole thing into the process.
 *
 *   mmap() is used rather than pinfo.fbmem directly because that is what
 *   apps/examples/fb does: in a KERNEL build only mmap() returns an address
 *   the application may touch.  The map covers pinfo.fblen, so on a double
 *   buffered device it covers both halves and buffer i starts at
 *   fbmem + i * stride * yres.
 *
 ****************************************************************************/

static int campreview_fb_open(FAR struct campreview_state_s *st)
{
  uint8_t i;

  st->fb_fd = open(CAMPREVIEW_FB_DEV, O_RDWR);
  if (st->fb_fd < 0)
    {
      printf("campreview: ERROR: open %s failed: %d\n",
             CAMPREVIEW_FB_DEV, errno);
      return -errno;
    }

  if (ioctl(st->fb_fd, FBIOGET_VIDEOINFO,
            (unsigned long)((uintptr_t)&st->vinfo)) < 0)
    {
      printf("campreview: ERROR: FBIOGET_VIDEOINFO failed: %d\n", errno);
      return -errno;
    }

  if (ioctl(st->fb_fd, FBIOGET_PLANEINFO,
            (unsigned long)((uintptr_t)&st->pinfo)) < 0)
    {
      printf("campreview: ERROR: FBIOGET_PLANEINFO failed: %d\n", errno);
      return -errno;
    }

  printf("campreview: fb %ux%u fmt=%u bpp=%u stride=%u fblen=%zu "
         "yres_virtual=%u\n",
         st->vinfo.xres, st->vinfo.yres, st->vinfo.fmt,
         st->pinfo.bpp, st->pinfo.stride, st->pinfo.fblen,
         (unsigned)st->pinfo.yres_virtual);

  if (st->vinfo.fmt != FB_FMT_RGB16_565 || st->pinfo.bpp != 16)
    {
      printf("campreview: ERROR: need FB_FMT_RGB16_565 (%d) at 16bpp, "
             "got fmt=%u bpp=%u\n",
             FB_FMT_RGB16_565, st->vinfo.fmt, st->pinfo.bpp);
      return -ENOTSUP;
    }

  if (st->vinfo.xres == 0 || st->vinfo.yres == 0 ||
      st->pinfo.stride < (unsigned)st->vinfo.xres * 2 ||
      st->pinfo.fblen < (size_t)st->pinfo.stride * st->vinfo.yres)
    {
      printf("campreview: ERROR: inconsistent fb geometry\n");
      return -EINVAL;
    }

  st->halfsize  = (uint32_t)st->pinfo.stride * st->vinfo.yres;
  st->framesize = (uint32_t)st->vinfo.xres * st->vinfo.yres * 2;

  /* Double buffered when the virtual height covers two frames AND the
   * mapping is large enough to hold both.  Both conditions matter: the
   * first says a pan can select a half, the second says one mmap reaches
   * it.
   *
   * A half is also required to be exactly one packed frame.  The camera
   * DMA writes rows back to back, so a framebuffer stride wider than
   * xres * 2 could not be filled directly; such a device is driven down
   * the copy path, which handles the padding row by row.
   */

  if (st->pinfo.yres_virtual >= (unsigned)st->vinfo.yres * 2 &&
      st->pinfo.fblen >= (size_t)st->halfsize * 2 &&
      st->halfsize == st->framesize)
    {
      st->nbuffers = 2;
    }
  else
    {
      st->nbuffers = 1;
    }

  st->fblen = st->pinfo.fblen;
  st->fbmem = mmap(NULL, st->fblen, PROT_READ | PROT_WRITE,
                   MAP_SHARED | MAP_FILE, st->fb_fd, 0);
  if (st->fbmem == MAP_FAILED)
    {
      printf("campreview: ERROR: mmap failed: %d\n", errno);
      st->fbmem = NULL;
      return -errno;
    }

  for (i = 0; i < st->nbuffers; i++)
    {
      st->halves[i] = st->fbmem + (size_t)i * st->halfsize;
    }

  st->zerocopy = (st->nbuffers == 2);

  return OK;
}

/****************************************************************************
 * Name: campreview_video_open
 *
 * Description:
 *   Open the capture device, set RGB565 at the panel resolution, queue the
 *   user-pointer buffers and start streaming.
 *
 *   The buffers are the two framebuffer halves on the zero-copy path and
 *   two freshly allocated private buffers on the copy path.  Either way
 *   V4L2_MEMORY_USERPTR with V4L2_BUF_MODE_RING is used, mirroring
 *   apps/examples/camera/camera_main.c, which is the pattern known to work
 *   with this driver stack.
 *
 ****************************************************************************/

static int campreview_video_open(FAR struct campreview_state_s *st)
{
  enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  struct v4l2_requestbuffers req;
  struct v4l2_format fmt;
  struct v4l2_buffer buf;
  FAR uint8_t *addr;
  uint8_t i;

  st->v_fd = open(CAMPREVIEW_VIDEO_DEV, O_RDWR);
  if (st->v_fd < 0)
    {
      printf("campreview: ERROR: open %s failed: %d\n",
             CAMPREVIEW_VIDEO_DEV, errno);
      return -errno;
    }

  memset(&fmt, 0, sizeof(fmt));
  fmt.type                = type;
  fmt.fmt.pix.width       = st->vinfo.xres;
  fmt.fmt.pix.height      = st->vinfo.yres;
  fmt.fmt.pix.field       = V4L2_FIELD_ANY;
  fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_RGB565;

  if (ioctl(st->v_fd, VIDIOC_S_FMT, (uintptr_t)&fmt) < 0)
    {
      printf("campreview: ERROR: VIDIOC_S_FMT RGB565 %ux%u failed: %d\n",
             st->vinfo.xres, st->vinfo.yres, errno);
      return -errno;
    }

  memset(&req, 0, sizeof(req));
  req.type   = type;
  req.memory = V4L2_MEMORY_USERPTR;
  req.count  = CAMPREVIEW_BUFNUM;
  req.mode   = V4L2_BUF_MODE_RING;

  if (ioctl(st->v_fd, VIDIOC_REQBUFS, (uintptr_t)&req) < 0)
    {
      printf("campreview: ERROR: VIDIOC_REQBUFS failed: %d\n", errno);
      return -errno;
    }

  /* Allocate the private buffers on the copy path only.  On the zero-copy
   * path the framebuffer halves ARE the capture buffers, so nothing is
   * allocated and nothing has to be freed.
   */

  if (!st->zerocopy)
    {
      for (i = 0; i < CAMPREVIEW_BUFNUM; i++)
        {
          st->bufs[i].start = memalign(CAMPREVIEW_BUFALIGN, st->framesize);
          if (st->bufs[i].start == NULL)
            {
              printf("campreview: ERROR: out of memory for buffer %d "
                     "(%lu B)\n", i, (unsigned long)st->framesize);
              return -ENOMEM;
            }

          st->bufs[i].length = st->framesize;
          st->nbufs          = i + 1;
        }
    }

  for (i = 0; i < CAMPREVIEW_BUFNUM; i++)
    {
      addr = st->zerocopy ? st->halves[i] : st->bufs[i].start;

      memset(&buf, 0, sizeof(buf));
      buf.type      = type;
      buf.memory    = V4L2_MEMORY_USERPTR;
      buf.index     = i;
      buf.m.userptr = (uintptr_t)addr;
      buf.length    = st->framesize;

      if (ioctl(st->v_fd, VIDIOC_QBUF, (uintptr_t)&buf) < 0)
        {
          printf("campreview: ERROR: VIDIOC_QBUF %d failed: %d\n",
                 i, errno);
          return -errno;
        }
    }

  if (ioctl(st->v_fd, VIDIOC_STREAMON, (uintptr_t)&type) < 0)
    {
      printf("campreview: ERROR: VIDIOC_STREAMON failed: %d\n", errno);
      return -errno;
    }

  st->streaming = true;
  return OK;
}

/****************************************************************************
 * Name: campreview_cleanup
 ****************************************************************************/

static void campreview_cleanup(FAR struct campreview_state_s *st)
{
  enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

  if (st->streaming)
    {
      ioctl(st->v_fd, VIDIOC_STREAMOFF, (uintptr_t)&type);
      st->streaming = false;
    }

  /* Stop the capture DMA before dropping the buffers it writes into.  The
   * private buffers only exist on the copy path; campreview_free_bufs()
   * walks nbufs, which stays 0 on the zero-copy path, so the framebuffer
   * halves are never handed to free().
   */

  campreview_free_bufs(st);

  if (st->fbmem != NULL)
    {
      munmap(st->fbmem, st->fblen);
      st->fbmem = NULL;
    }

  if (st->v_fd >= 0)
    {
      close(st->v_fd);
      st->v_fd = -1;
    }

  if (st->fb_fd >= 0)
    {
      close(st->fb_fd);
      st->fb_fd = -1;
    }
}

/****************************************************************************
 * Name: campreview_pan
 *
 * Description:
 *   Make framebuffer half 'index' the one the display scans out.  On a
 *   single-buffer device index is always 0 and the pan is just the cache
 *   writeback the display DMA needs.
 *
 ****************************************************************************/

static int campreview_pan(FAR struct campreview_state_s *st, uint8_t index)
{
  st->pinfo.yoffset = (uint32_t)index * st->vinfo.yres;

  return ioctl(st->fb_fd, FBIOPAN_DISPLAY,
               (unsigned long)((uintptr_t)&st->pinfo));
}

/****************************************************************************
 * Name: campreview_report_fps
 *
 * Description:
 *   Print a rate line every CAMPREVIEW_LOG_PERIOD frames and restart the
 *   measurement window.  '*mark' is the timestamp the window started at.
 *
 ****************************************************************************/

static void campreview_report_fps(unsigned long framecnt,
                                  FAR uint64_t *mark)
{
  uint64_t now;
  uint64_t elapsed;
  unsigned fps10 = 0;

  if ((framecnt % CAMPREVIEW_LOG_PERIOD) != 0)
    {
      return;
    }

  now     = campreview_now_ms();
  elapsed = now - *mark;

  if (elapsed > 0)
    {
      fps10 = (unsigned)((CAMPREVIEW_LOG_PERIOD * 10000ull) / elapsed);
    }

  printf("campreview: %lu frames, %u.%u fps\n",
         framecnt, fps10 / 10, fps10 % 10);
  *mark = now;
}

/****************************************************************************
 * Name: campreview_loop_zerocopy
 *
 * Description:
 *   Dequeue, publish, requeue.  No pixel ever moves under CPU control.
 *
 *   The camera DMA has already written the dequeued half and the CSI
 *   frame-done ISR has invalidated it, so the half is coherent and this
 *   task's cache lines over it are clean.  All that is left is to tell the
 *   display which half to scan out.
 *
 *   Requeueing the half that was just made front means the camera may
 *   start refilling a buffer that is still being scanned out, which can
 *   tear.  With only two buffers there is no way around it: one is on
 *   screen and the other must be in flight or frames get dropped.
 *   Eliminating it needs a third buffer, or vsync feedback so the requeue
 *   waits until the half is no longer front.
 *
 ****************************************************************************/

static int campreview_loop_zerocopy(FAR struct campreview_state_s *st,
                                    unsigned long nframes)
{
  enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  unsigned long framecnt = 0;
  bool panwarned = false;
  uint64_t mark;

  mark = campreview_now_ms();

  while (!g_campreview_stop && (nframes == 0 || framecnt < nframes))
    {
      struct v4l2_buffer buf;
      uintptr_t filled;
      uint8_t index;

      memset(&buf, 0, sizeof(buf));
      buf.type   = type;
      buf.memory = V4L2_MEMORY_USERPTR;

      if (ioctl(st->v_fd, VIDIOC_DQBUF, (uintptr_t)&buf) < 0)
        {
          if (errno == EINTR)
            {
              continue;
            }

          printf("campreview: ERROR: VIDIOC_DQBUF failed: %d\n", errno);
          return -errno;
        }

      /* Work out which half came back.  The driver hands the pointer back
       * in buf.m.userptr, so match it against the two half addresses
       * rather than trusting buf.index.
       */

      filled = (uintptr_t)buf.m.userptr;
      if (filled == (uintptr_t)st->halves[1])
        {
          index = 1;
        }
      else if (filled == (uintptr_t)st->halves[0])
        {
          index = 0;
        }
      else
        {
          printf("campreview: ERROR: unexpected userptr %p\n",
                 (FAR void *)filled);
          return -EINVAL;
        }

      /* Publish the half the camera just filled. */

      if (campreview_pan(st, index) < 0 && !panwarned)
        {
          printf("campreview: WARNING: FBIOPAN_DISPLAY failed: %d\n",
                 errno);
          panwarned = true;
        }

      /* Hand the same half back for a future frame. */

      if (ioctl(st->v_fd, VIDIOC_QBUF, (uintptr_t)&buf) < 0)
        {
          printf("campreview: ERROR: VIDIOC_QBUF failed: %d\n", errno);
          return -errno;
        }

      framecnt++;
      campreview_report_fps(framecnt, &mark);
    }

  printf("campreview: stopped after %lu frames\n", framecnt);
  return OK;
}

/****************************************************************************
 * Name: campreview_loop_copy
 *
 * Description:
 *   Fallback for a single-buffer framebuffer: dequeue, copy, pan, requeue.
 *
 *   Both the capture buffer and the framebuffer are RGB565, so the frame
 *   is a straight copy with no colour-space work at all.  The pan is what
 *   performs the cache writeback; skipping it leaves the display DMA on
 *   stale pixels.
 *
 *   The copy cannot be avoided here.  fb0 only exposes the buffer the
 *   display DMA is reading, so letting the camera DMA write it would scan
 *   out memory that is being overwritten and would put the same region
 *   under both an ISR-side invalidate and a task-side writeback.
 *
 ****************************************************************************/

static int campreview_loop_copy(FAR struct campreview_state_s *st,
                                unsigned long nframes)
{
  enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  unsigned long framecnt = 0;
  bool panwarned = false;
  uint32_t srcstride = (uint32_t)st->vinfo.xres * 2;
  uint32_t nrows = st->vinfo.yres;
  uint64_t mark;

  mark = campreview_now_ms();

  while (!g_campreview_stop && (nframes == 0 || framecnt < nframes))
    {
      struct v4l2_buffer buf;
      FAR const uint8_t *src;
      FAR uint8_t *dst;
      uint32_t row;

      memset(&buf, 0, sizeof(buf));
      buf.type   = type;
      buf.memory = V4L2_MEMORY_USERPTR;

      if (ioctl(st->v_fd, VIDIOC_DQBUF, (uintptr_t)&buf) < 0)
        {
          if (errno == EINTR)
            {
              continue;
            }

          printf("campreview: ERROR: VIDIOC_DQBUF failed: %d\n", errno);
          return -errno;
        }

      src = (FAR const uint8_t *)(uintptr_t)buf.m.userptr;
      dst = st->fbmem;

      if (st->pinfo.stride == srcstride)
        {
          /* Contiguous: one memcpy for the whole frame. */

          memcpy(dst, src, (size_t)srcstride * nrows);
        }
      else
        {
          /* Copy row by row so that a framebuffer stride wider than
           * xres * 2 is handled correctly.
           */

          for (row = 0; row < nrows; row++)
            {
              memcpy(dst, src, srcstride);
              src += srcstride;
              dst += st->pinfo.stride;
            }
        }

      if (campreview_pan(st, 0) < 0 && !panwarned)
        {
          printf("campreview: WARNING: FBIOPAN_DISPLAY failed: %d\n",
                 errno);
          panwarned = true;
        }

      /* Recycle the capture buffer. */

      if (ioctl(st->v_fd, VIDIOC_QBUF, (uintptr_t)&buf) < 0)
        {
          printf("campreview: ERROR: VIDIOC_QBUF failed: %d\n", errno);
          return -errno;
        }

      framecnt++;
      campreview_report_fps(framecnt, &mark);
    }

  printf("campreview: stopped after %lu frames\n", framecnt);
  return OK;
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: main
 ****************************************************************************/

int main(int argc, FAR char *argv[])
{
  struct campreview_state_s st;
  unsigned long nframes = 0;
  int ret;

  if (argc > 1)
    {
      if (strcmp(argv[1], "-h") == 0 || strcmp(argv[1], "--help") == 0)
        {
          campreview_usage(argv[0]);
          return EXIT_SUCCESS;
        }

      nframes = strtoul(argv[1], NULL, 0);
    }

  memset(&st, 0, sizeof(st));
  st.fb_fd = -1;
  st.v_fd  = -1;

  g_campreview_stop = false;
  signal(SIGINT, campreview_sigint);

  ret = campreview_fb_open(&st);
  if (ret < 0)
    {
      campreview_cleanup(&st);
      return EXIT_FAILURE;
    }

  ret = campreview_video_open(&st);
  if (ret < 0)
    {
      campreview_cleanup(&st);
      return EXIT_FAILURE;
    }

  printf("campreview: preview %ux%u RGB565 -> RGB565, %s, %s\n",
         st.vinfo.xres, st.vinfo.yres,
         st.zerocopy ? "zero-copy" : "copy",
         nframes == 0 ? "until stopped" : "limited run");

  if (st.zerocopy)
    {
      ret = campreview_loop_zerocopy(&st, nframes);
    }
  else
    {
      ret = campreview_loop_copy(&st, nframes);
    }

  campreview_cleanup(&st);
  return ret < 0 ? EXIT_FAILURE : EXIT_SUCCESS;
}
