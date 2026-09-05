/****************************************************************************
 * apps/examples/uvc_test/uvc_test.c
 *
 * USB Camera (UVC) live preview on framebuffer/LCD
 * Captures YUYV frames and displays them on /dev/lcd0 in real time.
 ****************************************************************************/

#include <nuttx/config.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <syslog.h>
#include <sys/ioctl.h>
#include <sys/time.h>

#include <nuttx/video/video.h>
#include <nuttx/video/v4l2_cap.h>
#include <nuttx/video/fb.h>
#include <nuttx/lcd/lcd_dev.h>
#include "tjpgd.h"

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#define VIDEO_DEV_PATH    "/dev/video"
#define FB_DEV_PATH       "/dev/lcd0"
#define TEST_WIDTH        320
#define TEST_HEIGHT       240
#define TEST_BUF_COUNT    6   /* Deeper queue: more vacant buffers for the UVC
                               * stream thread to rotate through, so it never
                               * wraps onto the buffer the app is copying out.
                               */
#define PREVIEW_FRAMES    300   /* ~10s at 30fps; 0 = run forever */
#define SAVE_FRAMES       10    /* Save first N frames as .yuv files to /data/ */

/****************************************************************************
 * Private Types
 ****************************************************************************/

struct v_buffer
{
  uint8_t *start;
  uint32_t length;
};

/****************************************************************************
 * Private Data
 ****************************************************************************/

static struct fb_videoinfo_s g_vinfo;
static int                   g_fbfd = -1;
static int                   g_recfd = -1;  /* /data/capture.yuv */
static uint16_t              g_rgbbuf[TEST_WIDTH * TEST_HEIGHT];

/****************************************************************************
 * tjpgd callbacks for MJPEG decode
 ****************************************************************************/

struct mjpeg_iodev_s
{
  const uint8_t *data;
  size_t         len;
  size_t         pos;
};

static size_t mjpeg_input(JDEC *jd, uint8_t *buff, size_t nbyte)
{
  struct mjpeg_iodev_s *io = (struct mjpeg_iodev_s *)jd->device;
  size_t avail = io->len - io->pos;

  if (nbyte > avail)
    nbyte = avail;

  if (buff)
    memcpy(buff, io->data + io->pos, nbyte);

  io->pos += nbyte;
  return nbyte;
}

static int mjpeg_output(JDEC *jd, void *bitmap, JRECT *rect)
{
  uint8_t *src = (uint8_t *)bitmap;
  uint32_t x;
  uint32_t y;
  uint32_t w = rect->right - rect->left + 1;
  uint32_t cpw = jd->width < TEST_WIDTH ? jd->width : TEST_WIDTH;

  for (y = rect->top; y <= rect->bottom && y < TEST_HEIGHT; y++)
    {
      for (x = rect->left; x <= rect->right && x < TEST_WIDTH; x++)
        {
          uint8_t b = *src++;
          uint8_t g = *src++;
          uint8_t r = *src++;
          g_rgbbuf[y * cpw + x] =
            ((r & 0xf8) << 8) | ((g & 0xfc) << 3) | (b >> 3);
        }

      if (rect->right >= TEST_WIDTH)
        src += (rect->right - TEST_WIDTH + 1) * 3;
    }

  return 1;
}

/* Private snapshot of one captured frame (YUYV), so the shared V4L2 capture
 * buffer can be requeued immediately and not be overwritten mid-processing.
 */

static uint8_t               g_framecopy[TEST_WIDTH * TEST_HEIGHT * 2];

/****************************************************************************
 * Private Functions
 ****************************************************************************/

/* Clamp an int to [0,255] */

static inline uint8_t clamp8(int v)
{
  if (v < 0)
    {
      return 0;
    }

  if (v > 255)
    {
      return 255;
    }

  return (uint8_t)v;
}

/****************************************************************************
 * Name: fb_open
 *
 * Description:
 *   Open the framebuffer device and query its geometry.
 *
 ****************************************************************************/

static int fb_open(void)
{
  int ret;

  g_fbfd = open(FB_DEV_PATH, O_RDWR);
  if (g_fbfd < 0)
    {
      printf("ERROR: open %s failed: %d\n", FB_DEV_PATH, errno);
      return -errno;
    }

  /* /dev/lcd0 is an lcddev character device, use LCDDEVIO_GETVIDEOINFO */

  ret = ioctl(g_fbfd, LCDDEVIO_GETVIDEOINFO, (uintptr_t)&g_vinfo);
  if (ret < 0)
    {
      printf("ERROR: LCDDEVIO_GETVIDEOINFO failed: %d\n", errno);
      close(g_fbfd);
      g_fbfd = -1;
      return -errno;
    }

  printf("LCD: %ux%u fmt=%u\n", g_vinfo.xres, g_vinfo.yres, g_vinfo.fmt);

  /* Also emit to syslog so it lands in the serial log for diagnosis. */

  syslog(LOG_ERR, "UVCTEST LCD: xres=%u yres=%u fmt=%u\n",
         g_vinfo.xres, g_vinfo.yres, g_vinfo.fmt);

  return OK;
}

/****************************************************************************
 * Name: fb_update
 *
 * Description:
 *   Notify the framebuffer driver that the content changed (for LCD
 *   drivers that need an explicit flush).
 *
 ****************************************************************************/

static void fb_blit(uint32_t w, uint32_t h)
{
  struct lcddev_area_s area;

  memset(&area, 0, sizeof(area));
  area.row_start = 0;
  area.row_end   = h - 1;
  area.col_start = 0;
  area.col_end   = w - 1;
  area.stride    = w * 2;          /* RGB565: 2 bytes/pixel */
  area.data      = (FAR uint8_t *)g_rgbbuf;

  ioctl(g_fbfd, LCDDEVIO_PUTAREA, (uintptr_t)&area);
}

/****************************************************************************
 * Name: yuyv_to_fb
 *
 * Description:
 *   Convert a YUYV 4:2:2 frame to the framebuffer's RGB565 format and
 *   blit it centered on the screen.
 *
 ****************************************************************************/

static void yuyv_to_fb(FAR const uint8_t *yuyv, uint32_t len)
{
  uint32_t panel_w = (g_vinfo.xres > 0) ? g_vinfo.xres : TEST_WIDTH;
  uint32_t panel_h = (g_vinfo.yres > 0) ? g_vinfo.yres : TEST_HEIGHT;
  uint32_t cpw = (panel_w < TEST_WIDTH)  ? panel_w : TEST_WIDTH;
  uint32_t cph = (panel_h < TEST_HEIGHT) ? panel_h : TEST_HEIGHT;
  uint32_t x;
  uint32_t y;

  for (y = 0; y < cph; y++)
    {
      FAR const uint8_t *src = yuyv + y * TEST_WIDTH * 2;
      FAR uint16_t *dst = &g_rgbbuf[y * cpw];

      if ((size_t)(y * TEST_WIDTH * 2) + cpw * 2 > len)
        {
          break;
        }

      for (x = 0; x + 1 < cpw; x += 2)
        {
          int y0 = src[0];
          int u  = src[1] - 128;
          int y1 = src[2];
          int v  = src[3] - 128;

          dst[x] = ((clamp8(y0 + ((179 * v) >> 7)) & 0xf8) << 8) |
                   ((clamp8(y0 - ((44 * u) >> 7) - ((91 * v) >> 7)) & 0xfc) << 3) |
                   (clamp8(y0 + ((227 * u) >> 7)) >> 3);

          dst[x + 1] = ((clamp8(y1 + ((179 * v) >> 7)) & 0xf8) << 8) |
                       ((clamp8(y1 - ((44 * u) >> 7) - ((91 * v) >> 7)) & 0xfc) << 3) |
                       (clamp8(y1 + ((227 * u) >> 7)) >> 3);

          src += 4;
        }
    }

  fb_blit(cpw, cph);
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

int uvc_test_main(int argc, char *argv[])
{
  struct v4l2_format fmt;
  struct v4l2_requestbuffers req;
  struct v4l2_buffer buf;
  struct v_buffer *buffers;
  enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  int fd;
  int ret;
  int i;
  int frames = 0;

  printf("=== USB Camera (UVC) Live Preview ===\n");

  /* Open the framebuffer for display */

  ret = fb_open();
  if (ret < 0)
    {
      printf("ERROR: framebuffer init failed\n");
      return ERROR;
    }

  /* DIAG: draw a static test pattern (colored horizontal bars) once, before
   * any camera data, to isolate the blit/LCD path from the camera data.  If
   * these bars appear clean and aligned, the LCD geometry+blit is correct and
   * any stripes are in the camera-frame layout; if the bars themselves are
   * striped/torn, the problem is in the blit or LCD driver path.
   */

  {
    uint32_t pw = (g_vinfo.xres > 0) ? g_vinfo.xres : TEST_WIDTH;
    uint32_t ph = (g_vinfo.yres > 0) ? g_vinfo.yres : TEST_HEIGHT;
    uint32_t xx;
    uint32_t yy;

    if (pw > TEST_WIDTH)  pw = TEST_WIDTH;
    if (ph > TEST_HEIGHT) ph = TEST_HEIGHT;

    for (yy = 0; yy < ph; yy++)
      {
        uint16_t color;
        if      (yy < ph / 4)     color = 0xf800;  /* red    */
        else if (yy < ph / 2)     color = 0x07e0;  /* green  */
        else if (yy < ph * 3 / 4) color = 0x001f;  /* blue   */
        else                      color = 0xffff;  /* white  */

        for (xx = 0; xx < pw; xx++)
          {
            g_rgbbuf[yy * pw + xx] = color;
          }
      }

    fb_blit(pw, ph);
    syslog(LOG_ERR, "UVCTEST testpattern blitted %ux%u\n", pw, ph);
    sleep(2);
  }

  /* Initialize video driver (creates /dev/video device) */

  ret = capture_initialize(VIDEO_DEV_PATH);
  if (ret != 0)
    {
      printf("ERROR: Failed to initialize video: %d (errno=%d)\n", ret, errno);
      return ERROR;
    }

  fd = open(VIDEO_DEV_PATH, O_RDWR);
  if (fd < 0)
    {
      printf("ERROR: Failed to open %s: %d\n", VIDEO_DEV_PATH, errno);
      return ERROR;
    }

  /* Set YUYV format */

  memset(&fmt, 0, sizeof(fmt));
  fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  fmt.fmt.pix.width = TEST_WIDTH;
  fmt.fmt.pix.height = TEST_HEIGHT;
  fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_JPEG;
  fmt.fmt.pix.field = V4L2_FIELD_NONE;

  ret = ioctl(fd, VIDIOC_S_FMT, (uintptr_t)&fmt);
  if (ret < 0)
    {
      printf("ERROR: VIDIOC_S_FMT failed: %d\n", errno);
      goto err_close;
    }

  if (fmt.fmt.pix.sizeimage == 0)
    {
      fmt.fmt.pix.sizeimage = TEST_WIDTH * TEST_HEIGHT * 2;
    }

  /* Request buffers */

  memset(&req, 0, sizeof(req));
  req.count = TEST_BUF_COUNT;
  req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  req.memory = V4L2_MEMORY_USERPTR;

  ret = ioctl(fd, VIDIOC_REQBUFS, (uintptr_t)&req);
  if (ret < 0)
    {
      printf("ERROR: VIDIOC_REQBUFS failed: %d\n", errno);
      goto err_close;
    }

  buffers = (struct v_buffer *)malloc(req.count * sizeof(struct v_buffer));
  if (buffers == NULL)
    {
      printf("ERROR: Failed to allocate buffer array\n");
      goto err_close;
    }

  for (i = 0; i < req.count; i++)
    {
      buffers[i].length = fmt.fmt.pix.sizeimage;
      buffers[i].start = (uint8_t *)memalign(64, buffers[i].length);
      if (buffers[i].start == NULL)
        {
          printf("ERROR: Failed to allocate buffer %d\n", i);
          goto err_free_buffers;
        }

      memset(&buf, 0, sizeof(buf));
      buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
      buf.memory = V4L2_MEMORY_USERPTR;
      buf.index = i;
      buf.m.userptr = (uintptr_t)buffers[i].start;
      buf.length = buffers[i].length;

      ret = ioctl(fd, VIDIOC_QBUF, (uintptr_t)&buf);
      if (ret < 0)
        {
          printf("ERROR: VIDIOC_QBUF failed for buffer %d: %d\n", i, errno);
          goto err_free_buffers;
        }
    }

  /* Start streaming */

  ret = ioctl(fd, VIDIOC_STREAMON, (uintptr_t)&type);
  if (ret < 0)
    {
      printf("ERROR: VIDIOC_STREAMON failed: %d\n", errno);
      goto err_free_buffers;
    }

  printf("Live preview started (press Ctrl-C to stop)\n");

  /* Preview loop */

  {
    struct timeval fps_tv;
    int fps_count = 0;

    gettimeofday(&fps_tv, NULL);

  while (PREVIEW_FRAMES == 0 || frames < PREVIEW_FRAMES)
    {
      memset(&buf, 0, sizeof(buf));
      buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
      buf.memory = V4L2_MEMORY_USERPTR;

      ret = ioctl(fd, VIDIOC_DQBUF, (uintptr_t)&buf);
      if (ret < 0)
        {
          printf("ERROR: VIDIOC_DQBUF failed: %d\n", errno);
          break;
        }

      /* FPS counter: print once per second */

      {
        struct timeval now;
        gettimeofday(&now, NULL);
        fps_count++;
        if (now.tv_sec > fps_tv.tv_sec)
          {
            syslog(LOG_ERR, "FPS: %d\n", fps_count);
            fps_count = 0;
            fps_tv = now;
          }
      }

      /* Snapshot the frame into a private buffer, then IMMEDIATELY requeue
       * the V4L2 buffer.  The UVC stream thread writes the next frame into
       * one of the shared capture buffers concurrently; if the app kept a
       * buffer for the whole convert+SPI-blit time (slow, per-row SPI), the
       * driver could wrap around and overwrite the buffer mid-read, showing
       * up as a moving horizontal "tear" band.  Copying out fast (one memcpy)
       * and requeuing at once shrinks that window to almost nothing.
       */

      {
        uint32_t n = buf.bytesused;
        if (n > sizeof(g_framecopy))
          {
            n = sizeof(g_framecopy);
          }

        memcpy(g_framecopy, (const void *)buf.m.userptr, n);

        /* Requeue immediately, before the slow convert + blit. */

        ret = ioctl(fd, VIDIOC_QBUF, (uintptr_t)&buf);
        if (ret < 0)
          {
            printf("ERROR: VIDIOC_QBUF failed: %d\n", errno);
            break;
          }

      /* MJPEG: decode JPEG via tjpgd and blit to LCD. */

      if (n > 1024)
        {
          static uint8_t jdwork[4096];  /* tjpgd work buffer */
          JDEC jdec;
          JRESULT res;
          struct mjpeg_iodev_s iodev;

          iodev.data = g_framecopy;
          iodev.len  = n;
          iodev.pos  = 0;

          res = jd_prepare(&jdec, mjpeg_input, jdwork,
                           sizeof(jdwork), &iodev);
          if (res == JDR_OK)
            {
              jd_decomp(&jdec, mjpeg_output, 0);
              fb_blit(jdec.width < TEST_WIDTH ? jdec.width : TEST_WIDTH,
                      jdec.height < TEST_HEIGHT ? jdec.height : TEST_HEIGHT);
            }
        }

      frames++;

      }
    }
  }  /* fps_tv scope */

  /* Stop streaming */

  ioctl(fd, VIDIOC_STREAMOFF, (uintptr_t)&type);
  printf("Preview stopped after %d frames\n", frames);

  /* Uninitialize video driver so /dev/video can be re-opened. */

  capture_uninitialize(VIDEO_DEV_PATH);

  ret = OK;

err_free_buffers:
  for (i = 0; i < req.count; i++)
    {
      if (buffers[i].start != NULL)
        {
          free(buffers[i].start);
        }
    }

  free(buffers);

err_close:
  close(fd);
  if (g_fbfd >= 0)
    {
      close(g_fbfd);
    }

  return ret;
}
