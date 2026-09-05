/****************************************************************************
 * apps/examples/face_detection/face_detection_main.cc
 *
 * Frontal Face Detection Demo for openvela/NuttX
 *
 * Derived from apps/examples/person_detection.  The camera capture, JPEG
 * decode and LCD blit paths are kept identical to person_detection (which
 * in turn copied them from uvc_test) so behaviour on the R528 board is the
 * same.  The differences are:
 *
 *   1. Pre-processing  : 128x128 RGB FLOAT32 (BlazeFace) instead of 96x96 gray.
 *   2. Model           : MediaPipe BlazeFace front/short-range (SSD, 896
 *                        anchors).  This is the PINTO_model_zoo #030
 *                        "integer_quant" model: int8 weights internally,
 *                        but float32 input and output tensors (a QUANTIZE op
 *                        wraps the input, DEQUANTIZE ops wrap the outputs).
 *   3. Post-processing  : anchor decode -> sigmoid score -> NMS -> 6 face
 *                        keypoints -> frontal-face geometry test.
 *
 * A face is reported as FRONTAL when it is squarely facing the camera,
 * judged from the two eye keypoints and the nose keypoint:
 *   - roll (head tilt)  : the two eyes are roughly on a horizontal line
 *   - yaw  (left/right) : the nose sits near the midpoint of the two eyes
 *   - a basic vertical sanity check (nose below eyes, mouth below nose)
 *
 * Usage:
 *   face_detection              - Run with a blank (zero) image
 *   face_detection -t <file>    - Run with a 128x128 binary PPM (P6) image
 *   face_detection -c           - Camera: one-shot detection
 *   face_detection -c -r 0      - Camera: live preview + detection (forever)
 *   face_detection -c -r <N>    - Camera: N frames
 ****************************************************************************/

#include <nuttx/config.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cerrno>
#include <cmath>
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

#include "face_detect_model_data.inc"
#include "tjpgd.h"

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/* Model input geometry (BlazeFace short-range) */

#define IN_W          128
#define IN_H          128
#define IN_CH         3

/* BlazeFace SSD head geometry (short-range 128x128 model) */

#define NUM_ANCHORS   896
#define NUM_COORDS    16     /* 4 box + 6 keypoints * 2 */
#define NUM_KP        6
#define BOX_SCALE     128.0f /* x/y/w/h scale == input size */

/* Detection thresholds (tunable) */

#define SCORE_THRESH  0.6f   /* sigmoid score gate (front model ref uses 0.75) */
#define NMS_IOU       0.30f  /* non-max-suppression overlap */
#define MAX_CAND      128    /* candidates kept above SCORE_THRESH */
#define MAX_DET       16     /* faces surviving NMS that we draw */

/* Frontal-face geometry thresholds (tunable, relative to eye distance) */

#define ROLL_THRESH   0.40f  /* |eyeL.y - eyeR.y| / eyeDist */
#define YAW_THRESH    0.35f  /* |nose.x - eyeMid.x| / eyeDist */

/* Keypoint indices in the BlazeFace output (left/right labels are only
 * a convention; the frontal test below is symmetric so it does not matter
 * which physical eye is index 0 vs 1).
 */

#define KP_EYE0       0
#define KP_EYE1       1
#define KP_NOSE       2
#define KP_MOUTH      3
#define KP_EAR0       4
#define KP_EAR1       5

#ifdef CONFIG_EXAMPLES_FACE_DETECTION_ARENA_SIZE
#  define TENSOR_ARENA_SIZE CONFIG_EXAMPLES_FACE_DETECTION_ARENA_SIZE
#else
#  define TENSOR_ARENA_SIZE (512 * 1024)
#endif

#define VIDEO_DEV_PATH "/dev/video"
#define FB_DEV_PATH    "/dev/lcd0"
#define TEST_WIDTH     320
#define TEST_HEIGHT    240
#define TEST_BUF_COUNT 6
#define FONT_W  5
#define FONT_H  7
#define FONT_SCALE 2

/****************************************************************************
 * Private Types
 ****************************************************************************/

struct v_buffer
{
  uint8_t *start;
  uint32_t length;
};

/* One decoded face detection, all coordinates normalised to [0,1] */

struct detection_s
{
  float score;
  float xmin, ymin, xmax, ymax;
  float kp[NUM_KP][2];   /* [k][0]=x, [k][1]=y */
  int   frontal;
};

/****************************************************************************
 * Private Data
 ****************************************************************************/

static uint16_t g_rgbbuf[TEST_WIDTH * TEST_HEIGHT];   /* RGB565 for LCD */
static uint8_t  g_tensor_arena[TENSOR_ARENA_SIZE] __attribute__((aligned(16)));
static struct fb_videoinfo_s g_vinfo;
static int g_fbfd = -1;

/* Snapshot buffer (copied from uvc_test) */

static uint8_t g_framecopy[TEST_WIDTH * TEST_HEIGHT * 2];

/* Model input staging buffer: 128x128x3 FLOAT32, RGB normalised to [-1,1] */

static float g_inbuf[IN_W * IN_H * IN_CH];

/* Pre-computed SSD anchor centres (w = h = 1.0, fixed_anchor_size) */

static float g_anchor_cx[NUM_ANCHORS];
static float g_anchor_cy[NUM_ANCHORS];

/* Decoded detections */

static detection_s g_cand[MAX_CAND];
static detection_s g_det[MAX_DET];

/****************************************************************************
 * tjpgd callbacks
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

/* When set, mjpeg_output also fills g_inbuf with 128x128 RGB FLOAT32.
 * The source image (g_inf_srcw x g_inf_srch) is squished into 128x128 by
 * independent nearest-neighbour scaling of x and y (same approach as
 * person_detection's grayscale path).
 */

static float   *g_inf_rgb = nullptr;
static uint32_t g_inf_srcw = 0;
static uint32_t g_inf_srch = 0;

static inline float normalise_u8(uint8_t v)
{
  /* map [0,255] pixel -> [-1,1] real (BlazeFace front expects this range) */
  return (float)v / 127.5f - 1.0f;
}

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

          /* RGB565 for LCD preview */

          g_rgbbuf[y * cpw + x] =
            ((r & 0xf8) << 8) | ((g & 0xfc) << 3) | (b >> 3);

          /* 128x128 RGB FLOAT32 for inference */

          if (g_inf_rgb && g_inf_srcw > 0 && g_inf_srch > 0)
            {
              int dx = (int)(x * IN_W / g_inf_srcw);
              int dy = (int)(y * IN_H / g_inf_srch);
              if (dx >= IN_W) dx = IN_W - 1;
              if (dy >= IN_H) dy = IN_H - 1;
              float *p = &g_inf_rgb[(dy * IN_W + dx) * IN_CH];
              p[0] = normalise_u8(r);
              p[1] = normalise_u8(g);
              p[2] = normalise_u8(b);
            }
        }

      if (rect->right >= TEST_WIDTH)
        src += (rect->right - TEST_WIDTH + 1) * 3;
    }

  return 1;
}

/****************************************************************************
 * 5x7 Bitmap Font (ASCII 32-90) — copied from person_detection
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
 * Drawing helpers (operate on g_rgbbuf, RGB565)
 ****************************************************************************/

static void draw_pixel(int x, int y, uint16_t color)
{
  if (x >= 0 && x < (int)TEST_WIDTH && y >= 0 && y < (int)TEST_HEIGHT)
    g_rgbbuf[y * TEST_WIDTH + x] = color;
}

static void draw_filled_rect(int x0, int y0, int x1, int y1, uint16_t color)
{
  for (int y = y0; y <= y1; y++)
    for (int x = x0; x <= x1; x++)
      draw_pixel(x, y, color);
}

static void draw_rect_outline(int x0, int y0, int x1, int y1, uint16_t color)
{
  for (int x = x0; x <= x1; x++)
    {
      draw_pixel(x, y0, color);     draw_pixel(x, y0 + 1, color);
      draw_pixel(x, y1, color);     draw_pixel(x, y1 - 1, color);
    }
  for (int y = y0; y <= y1; y++)
    {
      draw_pixel(x0, y, color);     draw_pixel(x0 + 1, y, color);
      draw_pixel(x1, y, color);     draw_pixel(x1 - 1, y, color);
    }
}

static void draw_marker(int cx, int cy, uint16_t color)
{
  draw_filled_rect(cx - 1, cy - 1, cx + 1, cy + 1, color);
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

static void draw_string(int x, int y, const char *str, uint16_t color, int scale)
{
  while (*str)
    {
      draw_char(x, y, *str, color, scale);
      x += (FONT_W + 1) * scale;
      str++;
    }
}

/* RGB565 colour constants */

#define COL_GREEN  ((0 << 11) | (63 << 5) | 0)
#define COL_YELLOW ((31 << 11) | (63 << 5) | 0)
#define COL_RED    ((31 << 11) | (0 << 5) | 0)
#define COL_WHITE  0xFFFF
#define COL_CYAN   ((0 << 11) | (63 << 5) | 31)

/****************************************************************************
 * BlazeFace anchor generation
 *
 * Reproduces MediaPipe's SsdAnchorsCalculator for the face_detection
 * short-range 128x128 model:
 *   num_layers=4, strides={8,16,16,16}, anchor_offset={0.5,0.5},
 *   aspect_ratios={1.0}, interpolated_scale_aspect_ratio=1.0,
 *   fixed_anchor_size=true  => every anchor has w=h=1, only centres vary.
 * Produces exactly 896 anchors: 16x16x2 (stride 8) + 8x8x6 (stride 16).
 ****************************************************************************/

static int generate_anchors(void)
{
  static const int strides[4] = {8, 16, 16, 16};
  const int num_layers = 4;
  const float off = 0.5f;
  int count = 0;
  int layer = 0;

  while (layer < num_layers)
    {
      /* Merge consecutive layers that share the same stride. Each merged
       * layer contributes 2 anchors per cell (aspect 1.0 + interpolated).
       */

      int last = layer;
      int anchors_per_cell = 0;
      while (last < num_layers && strides[last] == strides[layer])
        {
          anchors_per_cell += 2;
          last++;
        }

      int stride = strides[layer];
      int fm_w = (IN_W + stride - 1) / stride;
      int fm_h = (IN_H + stride - 1) / stride;

      for (int y = 0; y < fm_h; y++)
        for (int x = 0; x < fm_w; x++)
          for (int a = 0; a < anchors_per_cell; a++)
            {
              if (count < NUM_ANCHORS)
                {
                  g_anchor_cx[count] = ((float)x + off) / (float)fm_w;
                  g_anchor_cy[count] = ((float)y + off) / (float)fm_h;
                  count++;
                }
            }

      layer = last;
    }

  return count;
}

/****************************************************************************
 * Post-processing helpers
 ****************************************************************************/

static inline float sigmoidf(float x)
{
  if (x < -100.0f) x = -100.0f;   /* score_clipping_thresh */
  if (x >  100.0f) x =  100.0f;
  return 1.0f / (1.0f + expf(-x));
}

static float iou(const detection_s *a, const detection_s *b)
{
  float x0 = fmaxf(a->xmin, b->xmin);
  float y0 = fmaxf(a->ymin, b->ymin);
  float x1 = fminf(a->xmax, b->xmax);
  float y1 = fminf(a->ymax, b->ymax);
  float iw = fmaxf(0.0f, x1 - x0);
  float ih = fmaxf(0.0f, y1 - y0);
  float inter = iw * ih;
  float ua = (a->xmax - a->xmin) * (a->ymax - a->ymin) +
             (b->xmax - b->xmin) * (b->ymax - b->ymin) - inter;
  return ua > 0.0f ? inter / ua : 0.0f;
}

/* Decode all 896 anchors -> candidate detections above SCORE_THRESH.
 * box_t  : regressor tensor data (FLOAT32), layout [896][16]
 * score_t: classificator tensor data (FLOAT32, raw logits), layout [896][1]
 * reverse_output_order=true => raw box is [xc, yc, w, h].
 */

static int decode_detections(const float *box_t, const float *score_t)
{
  int n = 0;

  for (int i = 0; i < NUM_ANCHORS && n < MAX_CAND; i++)
    {
      float score = sigmoidf(score_t[i]);
      if (score < SCORE_THRESH)
        continue;

      const int base = i * NUM_COORDS;
      float acx = g_anchor_cx[i];
      float acy = g_anchor_cy[i];

      float xc = box_t[base + 0] / BOX_SCALE + acx;
      float yc = box_t[base + 1] / BOX_SCALE + acy;
      float w  = box_t[base + 2] / BOX_SCALE;
      float h  = box_t[base + 3] / BOX_SCALE;

      detection_s *d = &g_cand[n++];
      d->score = score;
      d->xmin = xc - w * 0.5f;
      d->ymin = yc - h * 0.5f;
      d->xmax = xc + w * 0.5f;
      d->ymax = yc + h * 0.5f;
      d->frontal = 0;

      for (int k = 0; k < NUM_KP; k++)
        {
          int off = base + 4 + k * 2;
          d->kp[k][0] = box_t[off + 0] / BOX_SCALE + acx;
          d->kp[k][1] = box_t[off + 1] / BOX_SCALE + acy;
        }
    }

  return n;
}

/* Greedy non-maximum suppression. Returns count written to g_det. */

static int nms(int ncand)
{
  static uint8_t removed[MAX_CAND];
  memset(removed, 0, sizeof(removed));
  int ndet = 0;

  for (int iter = 0; iter < ncand && ndet < MAX_DET; iter++)
    {
      /* pick highest remaining score */

      int best = -1;
      float best_score = -1.0f;
      for (int i = 0; i < ncand; i++)
        if (!removed[i] && g_cand[i].score > best_score)
          {
            best_score = g_cand[i].score;
            best = i;
          }

      if (best < 0)
        break;

      removed[best] = 1;
      g_det[ndet++] = g_cand[best];

      /* suppress overlapping candidates */

      for (int i = 0; i < ncand; i++)
        if (!removed[i] && iou(&g_cand[best], &g_cand[i]) > NMS_IOU)
          removed[i] = 1;
    }

  return ndet;
}

/* Frontal-face test using the eye and nose keypoints. Coordinates are
 * normalised [0,1]; the ratios below are scale-invariant.
 */

static int is_frontal(const detection_s *d)
{
  float e0x = d->kp[KP_EYE0][0], e0y = d->kp[KP_EYE0][1];
  float e1x = d->kp[KP_EYE1][0], e1y = d->kp[KP_EYE1][1];
  float nx  = d->kp[KP_NOSE][0], ny  = d->kp[KP_NOSE][1];
  float my  = d->kp[KP_MOUTH][1];

  float dx = e1x - e0x;
  float dy = e1y - e0y;
  float eye_dist = sqrtf(dx * dx + dy * dy);
  if (eye_dist < 1e-4f)
    return 0;

  float eye_mid_x = (e0x + e1x) * 0.5f;
  float eye_mid_y = (e0y + e1y) * 0.5f;

  /* roll: eyes should be roughly on a horizontal line */
  float roll = fabsf(dy) / eye_dist;

  /* yaw: nose should sit near the horizontal midpoint of the two eyes */
  float yaw = fabsf(nx - eye_mid_x) / eye_dist;

  /* vertical sanity: nose below eyes, mouth below nose */
  int vertical_ok = (ny > eye_mid_y) && (my > ny);

  return (roll < ROLL_THRESH) && (yaw < YAW_THRESH) && vertical_ok;
}

/****************************************************************************
 * fb_open / fb_blit (copied from person_detection)
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
 * Overlay detections on the RGB565 preview
 ****************************************************************************/

static void overlay_faces(int ndet, int any_frontal)
{
  uint16_t bg = any_frontal ? COL_GREEN : COL_RED;
  const char *label = any_frontal ? "FRONTAL FACE"
                    : (ndet > 0 ? "FACE (TURNED)" : "NO FACE");

  draw_filled_rect(0, 0, TEST_WIDTH - 1, (FONT_H + 4) * FONT_SCALE, bg);
  draw_string(4, 2, label, COL_WHITE, FONT_SCALE);

  for (int i = 0; i < ndet; i++)
    {
      detection_s *d = &g_det[i];
      int x0 = (int)(d->xmin * TEST_WIDTH);
      int y0 = (int)(d->ymin * TEST_HEIGHT);
      int x1 = (int)(d->xmax * TEST_WIDTH);
      int y1 = (int)(d->ymax * TEST_HEIGHT);
      uint16_t c = d->frontal ? COL_GREEN : COL_YELLOW;
      draw_rect_outline(x0, y0, x1, y1, c);

      for (int k = 0; k < NUM_KP; k++)
        {
          int kx = (int)(d->kp[k][0] * TEST_WIDTH);
          int ky = (int)(d->kp[k][1] * TEST_HEIGHT);
          draw_marker(kx, ky, COL_CYAN);
        }
    }
}

/****************************************************************************
 * PPM (P6, 128x128, 8-bit RGB) loader for test images
 ****************************************************************************/

static int load_ppm_rgb(const char *filepath, float *dst)
{
  int fd = open(filepath, O_RDONLY);
  if (fd < 0) { printf("ERROR: open %s: %d\n", filepath, errno); return -errno; }

  /* Parse a minimal binary PPM header: "P6\n<w> <h>\n<maxval>\n" */

  char hdr[64];
  int n = read(fd, hdr, sizeof(hdr) - 1);
  if (n < 2 || hdr[0] != 'P' || hdr[1] != '6')
    { printf("ERROR: not a P6 PPM\n"); close(fd); return -EINVAL; }
  hdr[n] = '\0';

  int w = 0, h = 0, maxv = 0, fields = 0;
  int pos = 2;
  while (fields < 3 && pos < n)
    {
      while (pos < n && (hdr[pos] == ' ' || hdr[pos] == '\n' ||
                         hdr[pos] == '\r' || hdr[pos] == '\t'))
        pos++;
      if (pos < n && hdr[pos] == '#')     /* skip comment line */
        { while (pos < n && hdr[pos] != '\n') pos++; continue; }
      int val = 0, got = 0;
      while (pos < n && hdr[pos] >= '0' && hdr[pos] <= '9')
        { val = val * 10 + (hdr[pos] - '0'); pos++; got = 1; }
      if (got)
        {
          if (fields == 0) w = val;
          else if (fields == 1) h = val;
          else maxv = val;
          fields++;
        }
    }
  pos++;  /* single whitespace after maxval */

  if (w != IN_W || h != IN_H || maxv != 255)
    {
      printf("ERROR: PPM must be %dx%d maxval 255 (got %dx%d %d)\n",
             IN_W, IN_H, w, h, maxv);
      close(fd);
      return -EINVAL;
    }

  /* Reposition file to the start of pixel data */

  lseek(fd, pos, SEEK_SET);

  uint8_t rgb[3];
  for (int i = 0; i < IN_W * IN_H; i++)
    {
      if (read(fd, rgb, 3) != 3) { close(fd); return -EIO; }
      dst[i * 3 + 0] = normalise_u8(rgb[0]);
      dst[i * 3 + 1] = normalise_u8(rgb[1]);
      dst[i * 3 + 2] = normalise_u8(rgb[2]);
    }

  close(fd);
  return 0;
}

/****************************************************************************
 * Locate the box (dim2==16) and score (dim2==1) output tensors, since
 * their order is not guaranteed across model conversions.
 ****************************************************************************/

static int identify_outputs(tflite::MicroInterpreter *interp,
                            TfLiteTensor **box, TfLiteTensor **score)
{
  *box = nullptr;
  *score = nullptr;
  size_t count = interp->outputs_size();

  for (size_t i = 0; i < count; i++)
    {
      TfLiteTensor *t = interp->output(i);
      int last = t->dims->data[t->dims->size - 1];
      if (last == NUM_COORDS) *box = t;
      else if (last == 1)     *score = t;
    }

  return (*box && *score) ? 0 : -1;
}

/****************************************************************************
 * print_usage
 ****************************************************************************/

static void print_usage(const char *progname)
{
  printf("Usage: %s [options]\n", progname);
  printf("  -c         Camera mode\n");
  printf("  -t <file>  Load 128x128 P6 PPM image\n");
  printf("  -r <N>     N frames (0=forever)\n");
  printf("  -h         Help\n");
}

/****************************************************************************
 * Run one inference on the currently-staged g_inbuf and report results.
 * Returns number of faces (post-NMS); *out_frontal set if any is frontal.
 ****************************************************************************/

static int run_inference(tflite::MicroInterpreter *interp,
                         TfLiteTensor *input, TfLiteTensor *box,
                         TfLiteTensor *score, int *out_frontal)
{
  memcpy(input->data.f, g_inbuf, sizeof(g_inbuf));

  if (interp->Invoke() != kTfLiteOk)
    {
      printf("ERROR: Invoke failed\n");
      *out_frontal = 0;
      return 0;
    }

  int ncand = decode_detections(box->data.f, score->data.f);
  int ndet = nms(ncand);

  int any_frontal = 0;
  for (int i = 0; i < ndet; i++)
    {
      g_det[i].frontal = is_frontal(&g_det[i]);
      if (g_det[i].frontal) any_frontal = 1;
    }

  *out_frontal = any_frontal;
  return ndet;
}

/****************************************************************************
 * main
 ****************************************************************************/

extern "C" int main(int argc, char *argv[])
{
  const tflite::Model *model = nullptr;
  TfLiteTensor *input = nullptr;
  TfLiteTensor *box = nullptr;
  TfLiteTensor *score = nullptr;
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
  printf("=== Frontal Face Detection Demo ===\n");
  fflush(stdout);

  model = tflite::GetModel(g_face_detect_model_data);
  if (model->version() != TFLITE_SCHEMA_VERSION)
    {
      printf("ERROR: Model version %lu != schema %d\n",
             (unsigned long)model->version(), TFLITE_SCHEMA_VERSION);
      printf("Have you generated face_detect_model_data.inc?\n");
      printf("See tools/tflite_to_inc.py and README.md\n");
      return 1;
    }

  /* Op set for BlazeFace short-range.  If Invoke/AllocateTensors reports a
   * missing builtin opcode, add the matching AddXxx() here — the message
   * names the opcode.  The set below covers the common short-range graph.
   */

  tflite::MicroMutableOpResolver<14> op_resolver;
  op_resolver.AddConv2D();
  op_resolver.AddDepthwiseConv2D();
  op_resolver.AddMaxPool2D();
  op_resolver.AddAveragePool2D();
  op_resolver.AddAdd();
  op_resolver.AddPad();
  op_resolver.AddReshape();
  op_resolver.AddConcatenation();
  op_resolver.AddLogistic();
  op_resolver.AddQuantize();
  op_resolver.AddDequantize();

  tflite::MicroInterpreter interpreter(
      model, op_resolver, g_tensor_arena, TENSOR_ARENA_SIZE);

  if (interpreter.AllocateTensors() != kTfLiteOk)
    {
      printf("ERROR: AllocateTensors failed (arena too small or missing op?)\n");
      return 1;
    }

  input = interpreter.input(0);
  if (identify_outputs(&interpreter, &box, &score) != 0)
    {
      printf("ERROR: could not find [%d]-wide box + [1]-wide score outputs\n",
             NUM_COORDS);
      return 1;
    }

  if (input->type != kTfLiteFloat32)
    printf("WARNING: input tensor is not float32 (type=%d)\n", input->type);

  int na = generate_anchors();
  printf("Model loaded, arena: %zu/%d bytes, anchors: %d\n",
         interpreter.arena_used_bytes(), TENSOR_ARENA_SIZE, na);
  fflush(stdout);

  /* =========================================================================
   * Camera mode — capture/decode/blit path identical to person_detection
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
      int fb_ok = (fb_open() == OK);

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
            { printf("ERROR: memalign failed\n"); }

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

      ret = ioctl(fd, VIDIOC_STREAMON, (uintptr_t)&type);
      if (ret < 0)
        { printf("ERROR: STREAMON failed: %d\n", errno); goto cleanup; }

      printf("Camera streaming (%dx%d MJPEG, %d bufs)\n",
             TEST_WIDTH, TEST_HEIGHT, TEST_BUF_COUNT);
      fflush(stdout);

      /* Set up RGB inference context for the decode callback */

      g_inf_rgb  = g_inbuf;
      g_inf_srcw = TEST_WIDTH;
      g_inf_srch = TEST_HEIGHT;

      while (run_forever || frames < iterations)
        {
          struct timeval t0, t1, t2, t3, t4;
          gettimeofday(&t0, NULL);

          memset(&buf, 0, sizeof(buf));
          buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
          buf.memory = V4L2_MEMORY_USERPTR;

          ret = ioctl(fd, VIDIOC_DQBUF, (uintptr_t)&buf);
          if (ret < 0)
            { printf("ERROR: DQBUF failed: %d\n", errno); break; }

          uint32_t n = buf.bytesused;
          if (n > sizeof(g_framecopy))
            n = sizeof(g_framecopy);
          memcpy(g_framecopy, (const void *)buf.m.userptr, n);

          ret = ioctl(fd, VIDIOC_QBUF, (uintptr_t)&buf);
          if (ret < 0)
            { printf("ERROR: QBUF failed: %d\n", errno); break; }

          gettimeofday(&t1, NULL);

          if (n > 1024)
            {
              static uint8_t jdwork[4096];
              JDEC jdec;
              JRESULT res;
              struct mjpeg_iodev_s iodev;

              iodev.data = g_framecopy;
              iodev.len  = n;
              iodev.pos  = 0;

              memset(g_inbuf, 0, sizeof(g_inbuf));

              res = jd_prepare(&jdec, mjpeg_input, jdwork,
                               sizeof(jdwork), &iodev);
              if (res == JDR_OK)
                jd_decomp(&jdec, mjpeg_output, 0);

              gettimeofday(&t2, NULL);
              frames++;

              int any_frontal = 0;
              int ndet = run_inference(&interpreter, input, box, score,
                                       &any_frontal);

              gettimeofday(&t3, NULL);

              overlay_faces(ndet, any_frontal);

              if (fb_ok)
                fb_blit(jdec.width < TEST_WIDTH ? jdec.width : TEST_WIDTH,
                        jdec.height < TEST_HEIGHT ? jdec.height : TEST_HEIGHT);

              gettimeofday(&t4, NULL);

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

              printf("\rFrame %d: %s  faces=%d   ", frames,
                     any_frontal ? "FRONTAL " : (ndet ? "TURNED  " : "NO FACE "),
                     ndet);
              fflush(stdout);
            }
        }

      printf("\nDone. %d frames.\n", frames);

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
          if (load_ppm_rgb(image_path, g_inbuf) < 0)
            return 1;
        }
      else
        {
          printf("Using blank image\n");
          for (size_t i = 0; i < IN_W * IN_H * IN_CH; i++)
            g_inbuf[i] = 0.0f;
        }

      int any_frontal = 0;
      int ndet = run_inference(&interpreter, input, box, score, &any_frontal);

      printf("  faces=%d  frontal=%s\n", ndet, any_frontal ? "YES" : "NO");
      for (int i = 0; i < ndet; i++)
        {
          detection_s *d = &g_det[i];
          printf("   [%d] score=%.2f box=(%.2f,%.2f)-(%.2f,%.2f) %s\n",
                 i, d->score, d->xmin, d->ymin, d->xmax, d->ymax,
                 d->frontal ? "FRONTAL" : "turned");
        }
    }

  printf("Done.\n");
  return 0;
}
