/****************************************************************************
 * apps/examples/camcap/camcap_main.c
 * Capture one frame from /dev/video0 (RGB565) and save to /data/.
 ****************************************************************************/

#include <nuttx/config.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <malloc.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <nuttx/video/video.h>

#define CAP_DEV   "/dev/video0"
#define OUT_FILE  "/tmp/capture.rgb565"
#define CAP_W     1024
#define CAP_H     600
#define BUF_NUM   2
#define BUF_ALIGN 64

static int xioctl(int fd, int req, void *arg)
{
  int r;
  do { r = ioctl(fd, req, arg); } while (r == -1 && errno == EINTR);
  return r;
}

int main(int argc, char *argv[])
{
  int fd = -1, outfd = -1, ret = EXIT_FAILURE;
  enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  struct v4l2_format fmt;
  struct v4l2_requestbuffers req;
  struct v4l2_buffer buf;
  void *bufs[BUF_NUM];
  size_t framesize;
  int i;
  int nbufs = 0;

  memset(bufs, 0, sizeof(bufs));   /* init before any error path frees them */
  printf("camcap: capturing %dx%d from %s\n", CAP_W, CAP_H, CAP_DEV);
  fflush(stdout);

  fd = open(CAP_DEV, O_RDWR, 0);
  if (fd < 0) { printf("ERROR: open %s: %d\n", CAP_DEV, errno); goto out; }

  /* Set format: RGB565 */

  memset(&fmt, 0, sizeof(fmt));
  fmt.type                = type;
  fmt.fmt.pix.width       = CAP_W;
  fmt.fmt.pix.height      = CAP_H;
  fmt.fmt.pix.field       = V4L2_FIELD_ANY;
  fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_RGB565;
  if (xioctl(fd, VIDIOC_S_FMT, &fmt) < 0)
    { printf("ERROR: S_FMT: %d\n", errno); goto out; }
  framesize = fmt.fmt.pix.sizeimage;
  printf("  format: %lux%lu size=%lu\n",
         (unsigned long)fmt.fmt.pix.width, (unsigned long)fmt.fmt.pix.height, (unsigned long)framesize);
  fflush(stdout);

  /* Request USERPTR buffers (ring mode required by CSI driver) */

  memset(&req, 0, sizeof(req));
  req.type   = type;
  req.memory = V4L2_MEMORY_USERPTR;
  req.count  = BUF_NUM;
  req.mode   = V4L2_BUF_MODE_RING;
  if (xioctl(fd, VIDIOC_REQBUFS, &req) < 0)
    { printf("ERROR: REQBUFS: %d\n", errno); goto out; }
  printf("  buffers: %lu\n", (unsigned long)req.count);
  fflush(stdout);

  /* Allocate and queue buffers */

  for (i = 0; i < req.count; i++)
    {
      bufs[i] = memalign(BUF_ALIGN, framesize);
      if (!bufs[i])
        { printf("ERROR: memalign %d\n", i); goto out; }
      nbufs++;
      memset(&buf, 0, sizeof(buf));
      buf.type      = type;
      buf.memory    = V4L2_MEMORY_USERPTR;
      buf.index     = i;
      buf.m.userptr = (uintptr_t)bufs[i];
      buf.length    = framesize;
      if (xioctl(fd, VIDIOC_QBUF, &buf) < 0)
        { printf("ERROR: QBUF %d: %d\n", i, errno); goto out; }
    }

  /* Start streaming */

  if (xioctl(fd, VIDIOC_STREAMON, &type) < 0)
    { printf("ERROR: STREAMON: %d\n", errno); goto out; }
  printf("  streaming, waiting...\n");
  fflush(stdout);

  /* Dequeue one frame */

  memset(&buf, 0, sizeof(buf));
  buf.type   = type;
  buf.memory = V4L2_MEMORY_USERPTR;
  if (xioctl(fd, VIDIOC_DQBUF, &buf) < 0)
    { printf("ERROR: DQBUF: %d\n", errno); goto out; }
  printf("  frame %lu: %lu bytes\n", (unsigned long)buf.index, (unsigned long)buf.bytesused);
  fflush(stdout);

  /* Save to file */

  outfd = open(OUT_FILE, O_WRONLY | O_CREAT | O_TRUNC, 0644);
  if (outfd < 0)
    { printf("ERROR: open %s: %d\n", OUT_FILE, errno); goto out; }
  {
    ssize_t n = write(outfd, bufs[buf.index], buf.bytesused);
    if (n < 0) { printf("ERROR: write: %d\n", errno); goto out; }
    printf("  wrote %ld bytes to %s\n", (long)n, OUT_FILE);
    fflush(stdout);
  }

  xioctl(fd, VIDIOC_QBUF, &buf);
  xioctl(fd, VIDIOC_STREAMOFF, &type);
  ret = EXIT_SUCCESS;

out:
  if (outfd >= 0) close(outfd);
  if (fd >= 0) close(fd);
  for (i = 0; i < nbufs; i++) free(bufs[i]);
  return ret;
}
