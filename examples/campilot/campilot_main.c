/****************************************************************************
 * apps/examples/campilot/campilot_main.c
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

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <malloc.h>
#include <poll.h>
#include <sys/ioctl.h>
#include <sys/videoio.h>
#include <unistd.h>

#include <mimo_client.h>

#ifdef CONFIG_EXAMPLES_CAMPILOT_UI
#  include "ui_lvgl.h"
#endif

#ifdef CONFIG_EXAMPLES_CAMPILOT_FEISHU
#  include <feishu_notify.h>
#endif

#ifdef CONFIG_EXAMPLES_CAMPILOT_FEISHU
static void push_to_feishu(const char *kind, const char *content);
#endif

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/* Camera / JPEG device paths */

#define CSI_DEV     "/dev/video0"
#define JPEG_DEV    "/dev/video1"

/* Capture geometry.  Must match the CSI driver's native RGB565 frame; the
 * JPEG encoder clamps OUTPUT height to 768 so 600 is fine.
 */

#define CAP_W       1024
#define CAP_H       600

/* The CSI driver needs at least two ring buffers to keep streaming. */

#define CAM_BUF_NUM 2
#define BUF_ALIGN   64

/* JPEG capture buffer: the driver reports the RGB565 size as the CAPTURE
 * sizeimage, which is a generous upper bound for a compressed frame.
 */

#define JPEG_BUF_SIZE   (CAP_W * CAP_H * 2)

/****************************************************************************
 * Private Functions
 ****************************************************************************/

/****************************************************************************
 * Name: xioctl
 *
 * Description:
 *   ioctl() wrapper that retries on EINTR.
 *
 ****************************************************************************/

static int xioctl(int fd, int req, void *arg)
{
  int r;

  do
    {
      r = ioctl(fd, req, arg);
    }
  while (r == -1 && errno == EINTR);

  return r;
}

/****************************************************************************
 * Name: capture_frame
 *
 * Description:
 *   Grab one RGB565 frame from the CSI camera.  Returns a malloc'd buffer
 *   holding the frame and sets *frame_len to its byte count.  The camera
 *   device is closed before returning so the sensor is free again.
 *
 ****************************************************************************/

static uint8_t *capture_frame(size_t *frame_len)
{
  enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  struct v4l2_requestbuffers req;
  struct v4l2_format fmt;
  struct v4l2_buffer buf;
  uint8_t *bufs[CAM_BUF_NUM];
  uint8_t *frame = NULL;
  size_t framesize;
  int nbufs = 0;
  int fd;
  int i;

  memset(bufs, 0, sizeof(bufs));
  *frame_len = 0;

  fd = open(CSI_DEV, O_RDWR, 0);
  if (fd < 0)
    {
      printf("[campilot] open %s failed: %d\n", CSI_DEV, errno);
      return NULL;
    }

  /* RGB565 at the panel's native geometry */

  memset(&fmt, 0, sizeof(fmt));
  fmt.type                = type;
  fmt.fmt.pix.width       = CAP_W;
  fmt.fmt.pix.height      = CAP_H;
  fmt.fmt.pix.field       = V4L2_FIELD_ANY;
  fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_RGB565;
  if (xioctl(fd, VIDIOC_S_FMT, &fmt) < 0)
    {
      printf("[campilot] camera S_FMT failed: %d\n", errno);
      goto errout;
    }

  framesize = fmt.fmt.pix.sizeimage;

  /* The CSI driver only implements USERPTR in ring mode */

  memset(&req, 0, sizeof(req));
  req.type   = type;
  req.memory = V4L2_MEMORY_USERPTR;
  req.count  = CAM_BUF_NUM;
  req.mode   = V4L2_BUF_MODE_RING;
  if (xioctl(fd, VIDIOC_REQBUFS, &req) < 0)
    {
      printf("[campilot] camera REQBUFS failed: %d\n", errno);
      goto errout;
    }

  for (i = 0; i < req.count && i < CAM_BUF_NUM; i++)
    {
      bufs[i] = memalign(BUF_ALIGN, framesize);
      if (bufs[i] == NULL)
        {
          printf("[campilot] out of memory for frame buffer %d\n", i);
          goto errout;
        }

      nbufs++;

      memset(&buf, 0, sizeof(buf));
      buf.type      = type;
      buf.memory    = V4L2_MEMORY_USERPTR;
      buf.index     = i;
      buf.m.userptr = (unsigned long)bufs[i];
      buf.length    = framesize;
      if (xioctl(fd, VIDIOC_QBUF, &buf) < 0)
        {
          printf("[campilot] camera QBUF %d failed: %d\n", i, errno);
          goto errout;
        }
    }

  if (xioctl(fd, VIDIOC_STREAMON, &type) < 0)
    {
      printf("[campilot] camera STREAMON failed: %d\n", errno);
      goto errout;
    }

  memset(&buf, 0, sizeof(buf));
  buf.type   = type;
  buf.memory = V4L2_MEMORY_USERPTR;
  if (xioctl(fd, VIDIOC_DQBUF, &buf) < 0)
    {
      printf("[campilot] camera DQBUF failed: %d\n", errno);
      xioctl(fd, VIDIOC_STREAMOFF, &type);
      goto errout;
    }

  /* Hand the dequeued buffer to the caller and drop the rest.  The buffer
   * is identified by its userptr, not by buf.index (the CSI driver reuses
   * indices in ring mode).
   */

  for (i = 0; i < nbufs; i++)
    {
      if ((unsigned long)bufs[i] == buf.m.userptr)
        {
          frame = bufs[i];
          bufs[i] = NULL;
          break;
        }
    }

  if (frame == NULL && nbufs > 0)
    {
      /* Fall back to the reported index */

      frame = bufs[buf.index % nbufs];
      bufs[buf.index % nbufs] = NULL;
    }

  *frame_len = buf.bytesused ? buf.bytesused : framesize;

  xioctl(fd, VIDIOC_STREAMOFF, &type);

errout:
  close(fd);
  for (i = 0; i < nbufs; i++)
    {
      free(bufs[i]);
    }

  return frame;
}

/****************************************************************************
 * Name: encode_jpeg
 *
 * Description:
 *   Feed one RGB565 frame to the hardware JPEG encoder (/dev/video1, V4L2
 *   M2M) and return a malloc'd buffer holding the JPEG bitstream.
 *
 ****************************************************************************/

static uint8_t *encode_jpeg(const uint8_t *frame, size_t frame_len,
                            size_t *jpeg_len)
{
  enum v4l2_buf_type out_type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
  enum v4l2_buf_type cap_type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  struct v4l2_requestbuffers req;
  struct v4l2_format fmt;
  struct v4l2_buffer buf;
  struct pollfd pfd;
  uint8_t *capbuf = NULL;
  uint8_t *jpeg = NULL;
  int pret = 0;
  int tries;
  int fd;

  *jpeg_len = 0;

  fd = open(JPEG_DEV, O_RDWR, 0);
  if (fd < 0)
    {
      printf("[campilot] open %s failed: %d\n", JPEG_DEV, errno);
      return NULL;
    }

  /* OUTPUT queue: RGB565 source frame */

  memset(&fmt, 0, sizeof(fmt));
  fmt.type                = out_type;
  fmt.fmt.pix.width       = CAP_W;
  fmt.fmt.pix.height      = CAP_H;
  fmt.fmt.pix.field       = V4L2_FIELD_ANY;
  fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_RGB565;
  if (xioctl(fd, VIDIOC_S_FMT, &fmt) < 0)
    {
      printf("[campilot] jpeg OUTPUT S_FMT failed: %d\n", errno);
      goto errout;
    }

  /* CAPTURE queue: JPEG bitstream */

  memset(&fmt, 0, sizeof(fmt));
  fmt.type                = cap_type;
  fmt.fmt.pix.width       = CAP_W;
  fmt.fmt.pix.height      = CAP_H;
  fmt.fmt.pix.field       = V4L2_FIELD_ANY;
  fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_JPEG;
  if (xioctl(fd, VIDIOC_S_FMT, &fmt) < 0)
    {
      printf("[campilot] jpeg CAPTURE S_FMT failed: %d\n", errno);
      goto errout;
    }

  memset(&req, 0, sizeof(req));
  req.type   = out_type;
  req.memory = V4L2_MEMORY_USERPTR;
  req.count  = 1;
  if (xioctl(fd, VIDIOC_REQBUFS, &req) < 0)
    {
      printf("[campilot] jpeg OUTPUT REQBUFS failed: %d\n", errno);
      goto errout;
    }

  memset(&req, 0, sizeof(req));
  req.type   = cap_type;
  req.memory = V4L2_MEMORY_USERPTR;
  req.count  = 1;
  if (xioctl(fd, VIDIOC_REQBUFS, &req) < 0)
    {
      printf("[campilot] jpeg CAPTURE REQBUFS failed: %d\n", errno);
      goto errout;
    }

  capbuf = memalign(BUF_ALIGN, JPEG_BUF_SIZE);
  if (capbuf == NULL)
    {
      printf("[campilot] out of memory for JPEG buffer\n");
      goto errout;
    }

  /* Queue the destination first so the encode triggered by the OUTPUT QBUF
   * has somewhere to write.
   */

  memset(&buf, 0, sizeof(buf));
  buf.type      = cap_type;
  buf.memory    = V4L2_MEMORY_USERPTR;
  buf.index     = 0;
  buf.m.userptr = (unsigned long)capbuf;
  buf.length    = JPEG_BUF_SIZE;
  if (xioctl(fd, VIDIOC_QBUF, &buf) < 0)
    {
      printf("[campilot] jpeg CAPTURE QBUF failed: %d\n", errno);
      goto errout;
    }

  memset(&buf, 0, sizeof(buf));
  buf.type      = out_type;
  buf.memory    = V4L2_MEMORY_USERPTR;
  buf.index     = 0;
  buf.m.userptr = (unsigned long)frame;
  buf.length    = frame_len;
  buf.bytesused = frame_len;
  if (xioctl(fd, VIDIOC_QBUF, &buf) < 0)
    {
      printf("[campilot] jpeg OUTPUT QBUF failed: %d\n", errno);
      goto errout;
    }

  if (xioctl(fd, VIDIOC_STREAMON, &out_type) < 0 ||
      xioctl(fd, VIDIOC_STREAMON, &cap_type) < 0)
    {
      printf("[campilot] jpeg STREAMON failed: %d\n", errno);
      goto errout;
    }

  /* DQBUF returns EAGAIN until the encode completes */

  pfd.fd      = fd;
  pfd.events  = POLLIN;
  pfd.revents = 0;
  for (tries = 0; tries < 50; tries++)
    {
      pret = poll(&pfd, 1, 100);
      if (pret < 0)
        {
          printf("[campilot] jpeg poll failed: %d\n", errno);
          goto stream_off;
        }

      if (pret > 0 && (pfd.revents & POLLIN) != 0)
        {
          break;
        }
    }

  if (pret <= 0 || (pfd.revents & POLLIN) == 0)
    {
      printf("[campilot] jpeg encode timeout\n");
      goto stream_off;
    }

  memset(&buf, 0, sizeof(buf));
  buf.type   = cap_type;
  buf.memory = V4L2_MEMORY_USERPTR;
  if (xioctl(fd, VIDIOC_DQBUF, &buf) < 0)
    {
      printf("[campilot] jpeg CAPTURE DQBUF failed: %d\n", errno);
      goto stream_off;
    }

  if (buf.bytesused == 0)
    {
      printf("[campilot] jpeg encode produced no data\n");
      goto stream_off;
    }

  /* Copy into a right-sized buffer so the 1.2MB scratch can be released
   * before the base64 expansion in mimo_vision_build_body().
   *
   * Do NOT re-queue the capture buffer: the M2M core would fire
   * capture_available and re-encode the already consumed frame.
   */

  jpeg = malloc(buf.bytesused);
  if (jpeg == NULL)
    {
      printf("[campilot] out of memory for JPEG copy\n");
      goto stream_off;
    }

  memcpy(jpeg, capbuf, buf.bytesused);
  *jpeg_len = buf.bytesused;

stream_off:
  xioctl(fd, VIDIOC_STREAMOFF, &cap_type);
  xioctl(fd, VIDIOC_STREAMOFF, &out_type);

errout:
  close(fd);
  free(capbuf);
  return jpeg;
}

/****************************************************************************
 * Name: capture_jpeg
 *
 * Description:
 *   Capture one frame from CSI camera and JPEG-encode it.
 *   Returns malloc'd JPEG buffer, sets *jpeg_len.
 *   Returns NULL on failure.
 *
 ****************************************************************************/

static uint8_t *capture_jpeg(size_t *jpeg_len)
{
  uint8_t *frame;
  uint8_t *jpeg;
  size_t frame_len;

  *jpeg_len = 0;

  frame = capture_frame(&frame_len);
  if (frame == NULL)
    {
      return NULL;
    }

  printf("[campilot] captured %dx%d RGB565 (%zu bytes)\n",
         CAP_W, CAP_H, frame_len);

  jpeg = encode_jpeg(frame, frame_len, jpeg_len);
  free(frame);

  return jpeg;
}

/****************************************************************************
 * Name: cmd_vision
 *
 * Description:
 *   Capture a frame and send to MiMo vision API.
 *   Usage: campilot once [prompt]
 *
 ****************************************************************************/

static int cmd_vision(const char *prompt)
{
  struct mimo_req_s req;
  struct mimo_rsp_s rsp;
  uint8_t *jpeg_data;
  size_t jpeg_len;
  int ret;

  printf("[campilot] Capturing frame...\n");

  jpeg_data = capture_jpeg(&jpeg_len);
  if (!jpeg_data || jpeg_len == 0)
    {
      printf("[campilot] Capture failed\n");
      return -1;
    }

  printf("[campilot] Encoded: %zu bytes JPEG\n", jpeg_len);

  /* Send to MiMo vision API */

  memset(&req, 0, sizeof(req));
  req.modality  = MIMO_VISION;
  req.jpeg_data = jpeg_data;
  req.jpeg_len  = jpeg_len;
  req.prompt    = prompt ? prompt : "Describe this image in detail.";

  printf("[campilot] Uploading to MiMo (%zu bytes JPEG + base64)...\n",
         jpeg_len);

  ret = mimo_request(&req, &rsp);

  free(jpeg_data);

  if (ret < 0)
    {
      printf("[campilot] MiMo request failed: %s\n", rsp.error_msg);
      mimo_free_response(&rsp);
      return -1;
    }

  printf("[campilot] HTTP %d\n", rsp.http_status);

  if (rsp.content)
    {
      printf("Result: %s\n", rsp.content);
#ifdef CONFIG_EXAMPLES_CAMPILOT_FEISHU
      push_to_feishu("vision", rsp.content);
#endif
    }
  else
    {
      printf("[campilot] No content in response\n");
    }

  mimo_free_response(&rsp);
  return 0;
}

/****************************************************************************
 * Name: cmd_text
 *
 * Description:
 *   Send a text query to MiMo text API.
 *   Usage: campilot text <message>
 *
 ****************************************************************************/

static int cmd_text(const char *message)
{
  struct mimo_req_s req;
  struct mimo_rsp_s rsp;
  int ret;

  if (!message || strlen(message) == 0)
    {
      printf("Usage: campilot text <message>\n");
      return -1;
    }

  printf("[campilot] Sending text query: %s\n", message);

  memset(&req, 0, sizeof(req));
  req.modality = MIMO_TEXT;
  req.prompt   = message;

  printf("[campilot] calling mimo_request()...\n");
  ret = mimo_request(&req, &rsp);
  printf("[campilot] mimo_request() returned %d\n", ret);

  if (ret < 0)
    {
      printf("[campilot] MiMo request failed: %s\n", rsp.error_msg);
      mimo_free_response(&rsp);
      return -1;
    }

  printf("[campilot] HTTP %d\n", rsp.http_status);

  if (rsp.content)
    {
      printf("MiMo: %s\n", rsp.content);
#ifdef CONFIG_EXAMPLES_CAMPILOT_FEISHU
      push_to_feishu("text", rsp.content);
#endif
    }

  mimo_free_response(&rsp);
  return 0;
}

#ifdef CONFIG_EXAMPLES_CAMPILOT_FEISHU
/****************************************************************************
 * Name: push_to_feishu
 *
 * Description:
 *   Push an analysis result (e.g. plant recognition) to the configured
 *   Feishu user via direct message. Failures are non-fatal: they only print
 *   a warning.
 *
 ****************************************************************************/

static void push_to_feishu(const char *kind, const char *content)
{
  const char *open_id = feishu_notify_get_default_open_id();
  char msg[1024];

  if (!open_id || open_id[0] == '\0')
    {
      printf("[campilot] no Feishu open_id configured, skip push\n");
      return;
    }

  snprintf(msg, sizeof(msg), "[campilot] %s:\n%s", kind, content);

  if (feishu_notify_send_dm(open_id, msg) < 0)
    {
      printf("[campilot] Feishu push failed\n");
    }
  else
    {
      printf("[campilot] result pushed to Feishu (DM)\n");
    }
}
#endif

/****************************************************************************
 * Name: print_usage
 ****************************************************************************/

static void print_usage(void)
{
  printf("Usage:\n");
  printf("  campilot once [prompt]  -- capture image and send to MiMo vision\n");
  printf("  campilot text <message>  -- send text query to MiMo\n");
#ifdef CONFIG_EXAMPLES_CAMPILOT_UI
  printf("  campilot ui  -- run the LVGL three-screen card UI\n");
#endif
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

int main(int argc, char *argv[])
{
  int ret;

  if (argc < 2)
    {
      print_usage();
      return -1;
    }

#ifdef CONFIG_EXAMPLES_CAMPILOT_UI
  if (strcmp(argv[1], "ui") == 0)
    {
      /* Run the LVGL card UI.  This blocks until torn down and does not
       * need the MiMo client (no network), so dispatch before init.
       */

      return ui_lvgl_main(argc - 1, &argv[1]);
    }
#endif

  /* Initialize MiMo client */

  ret = mimo_client_init();
  if (ret < 0)
    {
      printf("[campilot] MiMo client init failed\n");
      return -1;
    }

  if (strcmp(argv[1], "once") == 0)
    {
      const char *prompt = (argc > 2) ? argv[2] : NULL;
      ret = cmd_vision(prompt);
    }
  else if (strcmp(argv[1], "text") == 0)
    {
      if (argc < 3)
        {
          printf("Usage: campilot text <message>\n");
          mimo_client_deinit();
          return -1;
        }

      /* Concatenate remaining args as message */

      char message[512];
      message[0] = '\0';
      for (int i = 2; i < argc; i++)
        {
          if (i > 2) strlcat(message, " ", sizeof(message));
          strlcat(message, argv[i], sizeof(message));
        }

      ret = cmd_text(message);
    }
  else
    {
      print_usage();
      mimo_client_deinit();
      return -1;
    }

  mimo_client_deinit();
  return ret;
}
