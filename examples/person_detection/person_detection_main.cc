/****************************************************************************
 * apps/examples/person_detection/person_detection_main.cc
 *
 * Person Detection Demo for openvela/NuttX
 *
 * Camera capture logic copied from uvc_test to ensure identical V4L2/tjpgd
 * behavior.  Adds TFLite Micro inference + LCD overlay.
 *
 * Usage:
 *   person_detection              - Run with a blank (zero) image
 *   person_detection -t <file>   - Run with test BMP image
 *   person_detection -c          - Camera: one-shot detection
 *   person_detection -c -r 0     - Camera: live preview + detection
 *   person_detection -c -r <N>   - Camera: N frames
 ****************************************************************************/

#include <nuttx/config.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cerrno>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/time.h>

#include <nuttx/video/video.h>
#include <nuttx/video/v4l2_cap.h>
#include <nuttx/video/fb.h>
#include <nuttx/lcd/lcd_dev.h>

#include "tensorflow/lite/micro/micro_interpreter.h"
#include "tensorflow/lite/micro/micro_log.h"
#include "tensorflow/lite/micro/micro_mutable_op_resolver.h"
#include "tensorflow/lite/micro/system_setup.h"
#include "tensorflow/lite/schema/schema_generated.h"

#include "person_detect_model_data.inc"
#include "tjpgd.h"

/****************************************************************************
 * Pre-processor Definitions (copied from uvc_test where applicable)
 ****************************************************************************/

#define NUM_COLS       96
#define NUM_ROWS       96
#define NUM_CHANNELS   1
#define CATEGORY_COUNT 2
#define PERSON_INDEX   1
#define NOT_PERSON_IDX 0
#define PERSON_THRESHOLD 50  /* Only report PERSON when P > this */
#define TENSOR_ARENA_SIZE (136 * 1024)
#define BMP_HEADER_SIZE  1078

#define VIDEO_DEV_PATH "/dev/video"
#define FB_DEV_PATH    "/dev/lcd0"
#define TEST_WIDTH     320
#define TEST_HEIGHT    240
#define TEST_BUF_COUNT 6
#define FONT_W  5
#define FONT_H  7
#define FONT_SCALE 2

/****************************************************************************
 * Private Types (copied from uvc_test)
 ****************************************************************************/

struct v_buffer
{
  uint8_t *start;
  uint32_t length;
};

/****************************************************************************
 * Forward declarations
 ****************************************************************************/

static uint16_t g_rgbbuf[TEST_WIDTH * TEST_HEIGHT];

/****************************************************************************
 * tjpgd callbacks (copied from uvc_test)
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
  if (nbyte > avail) nbyte = avail;
  if (buff) memcpy(buff, io->data + io->pos, nbyte);
  io->pos += nbyte;
  return nbyte;
}

/* Output: write to g_rgbbuf (RGB565 for LCD) and optionally g_graybuf */

static int8_t  *g_inf_gray = nullptr;  /* inference grayscale buffer */
static uint32_t g_inf_srcw = 0;
static uint32_t g_inf_srch = 0;

static int mjpeg_output(JDEC *jd, void *bitmap, JRECT *rect)
{
  uint8_t *src = (uint8_t *)bitmap;
  uint32_t x;
  uint32_t y;
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

          /* Also convert to grayscale for inference */

          if (g_inf_gray && g_inf_srcw > 0 && g_inf_srch > 0)
            {
              int dx = (int)(x * NUM_COLS / g_inf_srcw);
              int dy = (int)(y * NUM_ROWS / g_inf_srch);
              if (dx >= NUM_COLS) dx = NUM_COLS - 1;
              if (dy >= NUM_ROWS) dy = NUM_ROWS - 1;
              int gray = (77 * r + 150 * g + 29 * b) >> 8;
              g_inf_gray[dy * NUM_COLS + dx] =
                  static_cast<int8_t>(gray - 128);
            }
        }

      if (rect->right >= TEST_WIDTH)
        src += (rect->right - TEST_WIDTH + 1) * 3;
    }

  return 1;
}

/****************************************************************************
 * Private Data
 ****************************************************************************/

static const char *g_category_labels[CATEGORY_COUNT] =
{
  "notperson",
  "person"
};

static uint8_t  g_tensor_arena[TENSOR_ARENA_SIZE] __attribute__((aligned(16)));
static struct fb_videoinfo_s g_vinfo;
static int g_fbfd = -1;

/* Snapshot buffer (copied from uvc_test) */

static uint8_t g_framecopy[TEST_WIDTH * TEST_HEIGHT * 2];

/* Inference grayscale buffer */

static int8_t g_graybuf[NUM_ROWS * NUM_COLS];

/****************************************************************************
 * 5x7 Bitmap Font (ASCII 32-90)
 ****************************************************************************/

static const uint8_t g_font5x7[][FONT_W] =
{
  {0x00,0x00,0x00,0x00,0x00},{0x00,0x00,0x5F,0x00,0x00},
  {0x00,0x07,0x00,0x07,0x00},{0x14,0x7F,0x14,0x7F,0x14},
  {0x24,0x2A,0x7F,0x2A,0x12},{0x23,0x13,0x08,0x64,0x62},
  {0x36,0x49,0x55,0x22,0x50},{0x00,0x05,0x03,0x00,0x00},
  {0x00,0x1C,0x22,0x41,0x00},{0x00,0x41,0x22,0x1C,0x00},
  {0x08,0x2A,0x1C,0x2A,0x08},{0x08,0x08,0x3E,0x08,0x08},
  {0x00,0x50,0x30,0x00,0x00},{0x08,0x08,0x08,0x08,0x08},
  {0x00,0x60,0x60,0x00,0x00},{0x20,0x10,0x08,0x04,0x02},
  {0x3E,0x51,0x49,0x45,0x3E},{0x00,0x42,0x7F,0x40,0x00},
  {0x42,0x61,0x51,0x49,0x46},{0x21,0x41,0x45,0x4B,0x31},
  {0x18,0x14,0x12,0x7F,0x10},{0x27,0x45,0x45,0x45,0x39},
  {0x3C,0x4A,0x49,0x49,0x30},{0x01,0x71,0x09,0x05,0x03},
  {0x36,0x49,0x49,0x49,0x36},{0x06,0x49,0x49,0x29,0x1E},
  {0x00,0x36,0x36,0x00,0x00},{0x00,0x56,0x36,0x00,0x00},
  {0x00,0x08,0x14,0x22,0x41},{0x14,0x14,0x14,0x14,0x14},
  {0x41,0x22,0x14,0x08,0x00},{0x02,0x01,0x51,0x09,0x06},
  {0x32,0x49,0x79,0x41,0x3E},{0x7E,0x11,0x11,0x11,0x7E},
  {0x7F,0x49,0x49,0x49,0x36},{0x3E,0x41,0x41,0x41,0x22},
  {0x7F,0x41,0x41,0x22,0x1C},{0x7F,0x49,0x49,0x49,0x41},
  {0x7F,0x09,0x09,0x01,0x01},{0x3E,0x41,0x41,0x51,0x32},
  {0x7F,0x08,0x08,0x08,0x7F},{0x00,0x41,0x7F,0x41,0x00},
  {0x20,0x40,0x41,0x3F,0x01},{0x7F,0x08,0x14,0x22,0x41},
  {0x7F,0x40,0x40,0x40,0x40},{0x7F,0x02,0x04,0x02,0x7F},
  {0x7F,0x04,0x08,0x10,0x7F},{0x3E,0x41,0x41,0x41,0x3E},
  {0x7F,0x09,0x09,0x09,0x06},{0x3E,0x41,0x51,0x21,0x5E},
  {0x7F,0x09,0x19,0x29,0x46},{0x46,0x49,0x49,0x49,0x31},
  {0x01,0x01,0x7F,0x01,0x01},{0x3F,0x40,0x40,0x40,0x3F},
  {0x1F,0x20,0x40,0x20,0x1F},{0x7F,0x20,0x18,0x20,0x7F},
  {0x63,0x14,0x08,0x14,0x63},{0x03,0x04,0x78,0x04,0x03},
  {0x61,0x51,0x49,0x45,0x43},
};

/****************************************************************************
 * Helpers
 ****************************************************************************/

static void draw_pixel(int x, int y, uint16_t color)
{
  if (x >= 0 && x < (int)TEST_WIDTH && y >= 0 && y < (int)TEST_HEIGHT)
    g_rgbbuf[y * TEST_WIDTH + x] = color;
}

static void draw_filled_rect(int x0, int y0, int x1, int y1,
                              uint16_t color)
{
  for (int y = y0; y <= y1; y++)
    for (int x = x0; x <= x1; x++)
      draw_pixel(x, y, color);
}

static void draw_char(int x, int y, char c, uint16_t color, int scale)
{
  if (c < 32 || c > 90) return;
  int idx = c - 32;
  if (idx < 0 || idx >= (int)(sizeof(g_font5x7) / FONT_W)) return;
  const uint8_t *glyph = g_font5x7[idx];
  for (int col = 0; col < FONT_W; col++)
    {
      uint8_t line = glyph[col];
      for (int row = 0; row < FONT_H; row++)
        if (line & (1 << row))
          for (int sy = 0; sy < scale; sy++)
            for (int sx = 0; sx < scale; sx++)
              draw_pixel(x + col * scale + sx, y + row * scale + sy, color);
    }
}

static void draw_string(int x, int y, const char *str,
                         uint16_t color, int scale)
{
  while (*str)
    {
      draw_char(x, y, *str, color, scale);
      x += (FONT_W + 1) * scale;
      str++;
    }
}

static void overlay_result(int8_t person_score, int detected)
{
  uint16_t bg = detected ? ((0 << 11) | (40 << 5) | 0)    /* green */
                         : ((20 << 11) | (0 << 5) | 0);   /* red */
  uint16_t fg = 0xFFFF;
  const char *label = detected ? "PERSON" : "NO PERSON";

  draw_filled_rect(0, 0, TEST_WIDTH - 1, (FONT_H + 4) * FONT_SCALE, bg);
  draw_string(4, 2, label, fg, FONT_SCALE);

  char s[16];
  snprintf(s, sizeof(s), "P:%d", person_score);
  draw_string(TEST_WIDTH - (FONT_W + 1) * FONT_SCALE * 6, 2, s, fg,
              FONT_SCALE);
}

/****************************************************************************
 * fb_open / fb_blit (copied from uvc_test)
 ****************************************************************************/

static int fb_open(void)
{
  g_fbfd = open(FB_DEV_PATH, O_RDWR);
  if (g_fbfd < 0)
    {
      printf("ERROR: open %s failed: %d\n", FB_DEV_PATH, errno);
      return -errno;
    }

  int ret = ioctl(g_fbfd, LCDDEVIO_GETVIDEOINFO, (uintptr_t)&g_vinfo);
  if (ret < 0)
    {
      printf("ERROR: LCDDEVIO_GETVIDEOINFO failed: %d\n", errno);
      close(g_fbfd);
      g_fbfd = -1;
      return -errno;
    }

  printf("LCD: %ux%u fmt=%u\n", g_vinfo.xres, g_vinfo.yres, g_vinfo.fmt);
  return OK;
}

static void fb_blit(uint32_t w, uint32_t h)
{
  struct lcddev_area_s area;
  memset(&area, 0, sizeof(area));
  area.row_start = 0;
  area.row_end   = h - 1;
  area.col_start = 0;
  area.col_end   = w - 1;
  area.stride    = w * 2;
  area.data      = (FAR uint8_t *)g_rgbbuf;
  ioctl(g_fbfd, LCDDEVIO_PUTAREA, (uintptr_t)&area);
}

/****************************************************************************
 * BMP Loader
 ****************************************************************************/

static int load_bmp_grayscale(const char *filepath, int8_t *image_data)
{
  uint8_t header[BMP_HEADER_SIZE];
  uint8_t pixel;
  int fd = open(filepath, O_RDONLY);
  if (fd < 0) { printf("ERROR: open %s: %d\n", filepath, errno); return -errno; }
  if (read(fd, header, BMP_HEADER_SIZE) != BMP_HEADER_SIZE) { close(fd); return -EIO; }
  if (header[0] != 'B' || header[1] != 'M') { close(fd); return -EINVAL; }
  for (int i = 0; i < NUM_ROWS * NUM_COLS; i++)
    {
      if (read(fd, &pixel, 1) != 1) { close(fd); return -EIO; }
      int row = i / NUM_COLS, col = i % NUM_COLS;
      image_data[(NUM_ROWS - 1 - row) * NUM_COLS + col] =
          static_cast<int8_t>(static_cast<int>(pixel) - 128);
    }
  close(fd);
  return 0;
}

/****************************************************************************
 * print_usage
 ****************************************************************************/

static void print_usage(const char *progname)
{
  printf("Usage: %s [options]\n", progname);
  printf("  -c         Camera mode\n");
  printf("  -t <file>  Load BMP image\n");
  printf("  -r <N>     N frames (0=forever)\n");
  printf("  -h         Help\n");
}

/****************************************************************************
 * main
 ****************************************************************************/

extern "C" int main(int argc, char *argv[])
{
  const tflite::Model *model = nullptr;
  TfLiteTensor *input = nullptr;
  TfLiteTensor *output = nullptr;
  const char *image_path = nullptr;
  int iterations = 1;
  int use_camera = 0;
  int opt;

  while ((opt = getopt(argc, argv, "ct:r:h")) != -1)
    switch (opt)
      {
        case 'c': use_camera = 1; break;
        case 't': image_path = optarg; break;
        case 'r': iterations = atoi(optarg); break;
        default: print_usage(argv[0]); return (opt == 'h') ? 0 : 1;
      }

  tflite::InitializeTarget();
  printf("=== Person Detection Demo ===\n");
  fflush(stdout);

  model = tflite::GetModel(g_person_detect_model_data);
  if (model->version() != TFLITE_SCHEMA_VERSION)
    { printf("ERROR: Model version mismatch\n"); return 1; }

  tflite::MicroMutableOpResolver<5> op_resolver;
  op_resolver.AddConv2D(tflite::Register_CONV_2D_INT8());
  op_resolver.AddDepthwiseConv2D(tflite::Register_DEPTHWISE_CONV_2D_INT8());
  op_resolver.AddAveragePool2D(tflite::Register_AVERAGE_POOL_2D_INT8());
  op_resolver.AddReshape();
  op_resolver.AddSoftmax(tflite::Register_SOFTMAX_INT8());

  tflite::MicroInterpreter interpreter(
      model, op_resolver, g_tensor_arena, TENSOR_ARENA_SIZE);

  if (interpreter.AllocateTensors() != kTfLiteOk)
    { printf("ERROR: AllocateTensors failed\n"); return 1; }

  input  = interpreter.input(0);
  output = interpreter.output(0);

  printf("Model loaded, arena: %zu/%d bytes\n",
         interpreter.arena_used_bytes(), TENSOR_ARENA_SIZE);
  fflush(stdout);

  /* =========================================================================
   * Camera mode — logic copied from uvc_test + inference overlay
   * ========================================================================= */

  if (use_camera)
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
      int run_forever = (iterations == 0);
      int person_score = 0;
      int no_person_score = 0;
      int detected = 0;
      int fb_ok = (fb_open() == OK);

      /* --- Step 1: Initialize video driver (same as uvc_test) --- */

      ret = capture_initialize(VIDEO_DEV_PATH);
      if (ret != 0 && ret != -EEXIST)
        {
          printf("ERROR: capture_initialize failed: %d\n", ret);
          return 1;
        }

      fd = open(VIDEO_DEV_PATH, O_RDWR);
      if (fd < 0)
        {
          printf("ERROR: open %s failed: %d\n", VIDEO_DEV_PATH, errno);
          return 1;
        }

      /* --- Step 2: Set MJPEG format (same as uvc_test) --- */

      memset(&fmt, 0, sizeof(fmt));
      fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
      fmt.fmt.pix.width = TEST_WIDTH;
      fmt.fmt.pix.height = TEST_HEIGHT;
      fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_JPEG;
      fmt.fmt.pix.field = V4L2_FIELD_NONE;

      ret = ioctl(fd, VIDIOC_S_FMT, (uintptr_t)&fmt);
      if (ret < 0)
        { printf("ERROR: VIDIOC_S_FMT failed: %d\n", errno); close(fd); return 1; }

      if (fmt.fmt.pix.sizeimage == 0)
        fmt.fmt.pix.sizeimage = TEST_WIDTH * TEST_HEIGHT * 2;

      /* --- Step 3: Request buffers (same as uvc_test) --- */

      memset(&req, 0, sizeof(req));
      req.count = TEST_BUF_COUNT;
      req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
      req.memory = V4L2_MEMORY_USERPTR;

      ret = ioctl(fd, VIDIOC_REQBUFS, (uintptr_t)&req);
      if (ret < 0)
        { printf("ERROR: VIDIOC_REQBUFS failed: %d\n", errno); close(fd); return 1; }

      buffers = (struct v_buffer *)malloc(req.count * sizeof(struct v_buffer));
      if (!buffers)
        { close(fd); return 1; }

      for (i = 0; i < (int)req.count; i++)
        {
          buffers[i].length = fmt.fmt.pix.sizeimage;
          buffers[i].start = (uint8_t *)memalign(64, buffers[i].length);
          if (!buffers[i].start)
            { printf("ERROR: memalign failed\n"); /* cleanup below */ }

          memset(&buf, 0, sizeof(buf));
          buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
          buf.memory = V4L2_MEMORY_USERPTR;
          buf.index = i;
          buf.m.userptr = (uintptr_t)buffers[i].start;
          buf.length = buffers[i].length;

          ret = ioctl(fd, VIDIOC_QBUF, (uintptr_t)&buf);
          if (ret < 0)
            { printf("ERROR: QBUF failed %d: %d\n", i, errno); }
        }

      /* --- Step 4: Start streaming (same as uvc_test) --- */

      ret = ioctl(fd, VIDIOC_STREAMON, (uintptr_t)&type);
      if (ret < 0)
        { printf("ERROR: STREAMON failed: %d\n", errno); goto cleanup; }

      printf("Camera streaming (%dx%d MJPEG, %d bufs)\n",
             TEST_WIDTH, TEST_HEIGHT, TEST_BUF_COUNT);
      fflush(stdout);

      /* --- Step 5: Capture loop (copied from uvc_test) --- */

      /* Set up grayscale inference context */

      g_inf_gray = g_graybuf;
      g_inf_srcw = TEST_WIDTH;
      g_inf_srch = TEST_HEIGHT;

      while (run_forever || frames < iterations)
        {
          struct timeval t0, t1, t2, t3, t4;
          gettimeofday(&t0, NULL);

          /* DQBUF — same as uvc_test */

          memset(&buf, 0, sizeof(buf));
          buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
          buf.memory = V4L2_MEMORY_USERPTR;

          ret = ioctl(fd, VIDIOC_DQBUF, (uintptr_t)&buf);
          if (ret < 0)
            {
              printf("ERROR: DQBUF failed: %d\n", errno);
              break;
            }

          /* Snapshot to local buffer — same as uvc_test */

          uint32_t n = buf.bytesused;
          if (n > sizeof(g_framecopy))
            n = sizeof(g_framecopy);
          memcpy(g_framecopy, (const void *)buf.m.userptr, n);

          /* Requeue immediately — same as uvc_test */

          ret = ioctl(fd, VIDIOC_QBUF, (uintptr_t)&buf);
          if (ret < 0)
            {
              printf("ERROR: QBUF failed: %d\n", errno);
              break;
            }

          gettimeofday(&t1, NULL);

          /* Decode JPEG — same as uvc_test */

          if (n > 1024)
            {
              static uint8_t jdwork[4096];
              JDEC jdec;
              JRESULT res;
              struct mjpeg_iodev_s iodev;

              iodev.data = g_framecopy;
              iodev.len  = n;
              iodev.pos  = 0;

              memset(g_graybuf, 0, sizeof(g_graybuf));

              res = jd_prepare(&jdec, mjpeg_input, jdwork,
                               sizeof(jdwork), &iodev);
              if (res == JDR_OK)
                {
                  jd_decomp(&jdec, mjpeg_output, 0);
                }

              gettimeofday(&t2, NULL);

              frames++;

              /* Run inference */

              memcpy(input->data.int8, g_graybuf, NUM_ROWS * NUM_COLS);

              if (interpreter.Invoke() == kTfLiteOk)
                {
                  person_score = output->data.int8[PERSON_INDEX];
                  no_person_score = output->data.int8[NOT_PERSON_IDX];
                  detected = (person_score > PERSON_THRESHOLD);
                }

              gettimeofday(&t3, NULL);

              /* Overlay result on the decoded RGB565 image */

              overlay_result(person_score, detected);

              /* Blit to LCD */

              if (fb_ok)
                fb_blit(jdec.width < TEST_WIDTH ? jdec.width : TEST_WIDTH,
                        jdec.height < TEST_HEIGHT ? jdec.height : TEST_HEIGHT);

              gettimeofday(&t4, NULL);

              /* Print timing breakdown every 10 frames */

              if (frames % 10 == 0)
                {
                  int cap_us  = (t1.tv_sec - t0.tv_sec) * 1000000 +
                                (t1.tv_usec - t0.tv_usec);
                  int jpeg_us = (t2.tv_sec - t1.tv_sec) * 1000000 +
                                (t2.tv_usec - t1.tv_usec);
                  int infer_us = (t3.tv_sec - t2.tv_sec) * 1000000 +
                                 (t3.tv_usec - t2.tv_usec);
                  int blit_us = (t4.tv_sec - t3.tv_sec) * 1000000 +
                                (t4.tv_usec - t3.tv_usec);
                  int total_us = (t4.tv_sec - t0.tv_sec) * 1000000 +
                                 (t4.tv_usec - t0.tv_usec);
                  printf("\nTiming: cap=%dms jpeg=%dms infer=%dms blit=%dms total=%dms (%dfps)\n",
                         cap_us/1000, jpeg_us/1000, infer_us/1000,
                         blit_us/1000, total_us/1000,
                         total_us > 0 ? 1000000 / total_us : 0);
                }

              printf("\rFrame %d: %s (P:%d N:%d)   ",
                     frames, detected ? "PERSON  " : "NO PERSON",
                     person_score, no_person_score);
              fflush(stdout);
            }
        }

      printf("\nDone. %d frames.\n", frames);

      /* --- Cleanup (same as uvc_test) --- */

cleanup:
      ioctl(fd, VIDIOC_STREAMOFF, (uintptr_t)&type);
      for (i = 0; i < (int)req.count; i++)
        if (buffers[i].start) free(buffers[i].start);
      free(buffers);
      close(fd);
      if (fb_ok) close(g_fbfd);
      return 0;
    }

  /* =========================================================================
   * Single image mode
   * ========================================================================= */

  printf("Single image mode\n");
  fflush(stdout);

  for (int iter = 0; iter < iterations; iter++)
    {
      if (image_path)
        {
          printf("Loading: %s\n", image_path);
          if (load_bmp_grayscale(image_path, input->data.int8) < 0)
            return 1;
        }
      else
        {
          printf("Using blank image\n");
          memset(input->data.int8, 0, NUM_ROWS * NUM_COLS);
        }

      if (interpreter.Invoke() != kTfLiteOk)
        { printf("ERROR: Invoke failed\n"); return 1; }

      int8_t ps = output->data.int8[PERSON_INDEX];
      int8_t ns = output->data.int8[NOT_PERSON_IDX];
      printf("  notperson: %d  person: %d  -> %s\n",
             ns, ps, ps > PERSON_THRESHOLD ? "PERSON DETECTED" : "No person");
    }

  printf("Done.\n");
  return 0;
}
