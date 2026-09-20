/****************************************************************************
 * apps/examples/jpegenc/jpegenc_main.c
 *
 * Encode one RGB565 frame to JPEG using the ESP32-P4 hardware JPEG encoder
 * (/dev/video1, V4L2 M2M).
 *
 * Input:  /data/capture.rgb565  (1024x600 RGB565, from camcap)
 * Output: /data/capture.jpg
 *
 * SPDX-License-Identifier: Apache-2.0
 ****************************************************************************/

#include <syslog.h>
#include <nuttx/config.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <malloc.h>
#include <poll.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/videoio.h>

/* RAM debug markers (digits, distinct from arch markers W C i s b e P z c D
 * d m n o q E X).  dbg_mark_char() records to the app-only marker address
 * (0x5010ffe0) even when the USB console is wedged; the esp_bringup boot
 * dump reads it back after a warm reset.  Boot's dbg_putc() markers live at
 * 0x5010fff0 and would clobber this address, hence the separation.
 */

extern void dbg_mark_char(int ch);
#define MARK(c) dbg_mark_char(c)

/* Print the previous run's last RAM debug marker (survives warm reset). */

#define PREV_MARK_ADDR ((volatile uint32_t *)0x5010ffe0)

#define M2M_DEV     "/dev/video1"
#define IN_FILE     "/tmp/capture.rgb565"
#define OUT_FILE    "/tmp/capture.jpg"
#define W           1024
#define H           600

/* RGB565 input frame size (2 bytes/pixel) */
#define OUT_SIZE    (W * H * 2)
/* JPEG output buffer: driver uses W*H for capture sizeimage, but allow slack */
#define CAP_SIZE    (W * H * 2)

static int xioctl(int fd, int req, void *arg)
{
  int r;
  do { r = ioctl(fd, req, arg); } while (r == -1 && errno == EINTR);
  return r;
}

int main(int argc, char *argv[])
{
  int fd = -1, infd = -1, outfd = -1;
  int ret = EXIT_FAILURE;
  struct v4l2_format fmt;
  struct v4l2_requestbuffers req;
  struct v4l2_buffer buf;
  void *inbuf = NULL;
  void *capbuf = NULL;
  enum v4l2_buf_type out_type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
  enum v4l2_buf_type cap_type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  size_t n;

  MARK('@');                     /* main entry */
  MARK('A');                     /* before 1st printf */
  MARK('B');                     /* after skipped printf */
  dbg_mark_char('#');            /* RAM-write self-test: expect 0x23 */
  MARK('C');                     /* after dbg_mark */
  /* fflush(stdout); */
  MARK('a');                     /* fflush done, about to allocate input */

  /* Generate the RGB565 input frame in memory (skip file I/O for now) */

  inbuf = memalign(64, OUT_SIZE);
  MARK('b');                     /* input memalign done */
  if (!inbuf)
    { /* printf("ERROR: memalign input\n"); */ goto out; }
  printf("  inbuf @ %p (%d bytes)\n", inbuf, OUT_SIZE);
  /* fflush(stdout); */

  {
    uint8_t *p = (uint8_t *)inbuf;
    size_t i;
    for (i = 0; i < OUT_SIZE; i++)
      {
        p[i] = (uint8_t)(i * 31 + (i >> 8));
      }
  }
  n = OUT_SIZE;
  MARK('f');                     /* input filled in memory */
  printf("  [marker@f] 0x%02x\n", (int)(*PREV_MARK_ADDR));
  /* fflush(stdout); */
  *PREV_MARK_ADDR = 0xAA;        /* sentinel: survive warm reset */
  printf("  [sentinel] 0x%02x\n", (int)(*PREV_MARK_ADDR));
  /* fflush(stdout); */
  printf("  generated %ld bytes in memory\n", (long)n);
  /* fflush(stdout); */

  /* Open M2M device */

  MARK('0'); /* about to open M2M device */
  fd = open(M2M_DEV, O_RDWR, 0);
  if (fd < 0)
    { /* printf("ERROR: open %s: %d\n", M2M_DEV, errno); */ goto out; }
  MARK('1'); /* device open */

  /* Set OUTPUT queue format: RGB565 */

  memset(&fmt, 0, sizeof(fmt));
  fmt.type                = out_type;
  fmt.fmt.pix.width       = W;
  fmt.fmt.pix.height      = H;
  fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_RGB565;
  fmt.fmt.pix.field       = V4L2_FIELD_ANY;
  if (xioctl(fd, VIDIOC_S_FMT, &fmt) < 0)
    { /* printf("ERROR: OUTPUT S_FMT: %d\n", errno); */ goto out; }
  /* printf("  OUTPUT fmt: %ux%u sizeimage=%u\n",
         (unsigned int)fmt.fmt.pix.width, (unsigned int)fmt.fmt.pix.height,
         (unsigned int)fmt.fmt.pix.sizeimage); */
  /* fflush(stdout); */
  MARK('2'); /* output S_FMT ok */

  /* Set CAPTURE queue format: JPEG */

  memset(&fmt, 0, sizeof(fmt));
  fmt.type                = cap_type;
  fmt.fmt.pix.width       = W;
  fmt.fmt.pix.height      = H;
  fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_JPEG;
  fmt.fmt.pix.field       = V4L2_FIELD_ANY;
  if (xioctl(fd, VIDIOC_S_FMT, &fmt) < 0)
    { /* printf("ERROR: CAPTURE S_FMT: %d\n", errno); */ goto out; }
  /* printf("  CAPTURE fmt: %ux%u sizeimage=%u\n",
         (unsigned int)fmt.fmt.pix.width, (unsigned int)fmt.fmt.pix.height,
         (unsigned int)fmt.fmt.pix.sizeimage); */
  /* fflush(stdout); */
  MARK('3'); /* capture S_FMT ok */

  /* Request buffers on both queues (USERPTR) */

  memset(&req, 0, sizeof(req));
  req.type   = out_type;
  req.memory = V4L2_MEMORY_USERPTR;
  req.count  = 2;
  if (xioctl(fd, VIDIOC_REQBUFS, &req) < 0)
    { /* printf("ERROR: OUTPUT REQBUFS: %d\n", errno); */ goto out; }

  memset(&req, 0, sizeof(req));
  req.type   = cap_type;
  req.memory = V4L2_MEMORY_USERPTR;
  req.count  = 2;
  if (xioctl(fd, VIDIOC_REQBUFS, &req) < 0)
    { /* printf("ERROR: CAPTURE REQBUFS: %d\n", errno); */ goto out; }
  MARK('4'); /* reqbufs ok */

  /* Allocate capture buffer and queue it on the CAPTURE queue */

  capbuf = memalign(64, CAP_SIZE);
  if (!capbuf)
    { /* printf("ERROR: memalign capture\n"); */ goto out; }

  memset(&buf, 0, sizeof(buf));
  buf.type      = cap_type;
  buf.memory    = V4L2_MEMORY_USERPTR;
  buf.index     = 0;
  buf.m.userptr = (unsigned long)capbuf;
  buf.length    = CAP_SIZE;
  if (xioctl(fd, VIDIOC_QBUF, &buf) < 0)
    { /* printf("ERROR: CAPTURE QBUF: %d\n", errno); */ goto out; }
  MARK('5'); /* capture qbuf ok */

  /* Queue input frame on OUTPUT queue */

  memset(&buf, 0, sizeof(buf));
  buf.type      = out_type;
  buf.memory    = V4L2_MEMORY_USERPTR;
  buf.index     = 0;
  buf.m.userptr = (unsigned long)inbuf;
  buf.length    = OUT_SIZE;
  buf.bytesused = OUT_SIZE;
  if (xioctl(fd, VIDIOC_QBUF, &buf) < 0)
    { /* printf("ERROR: OUTPUT QBUF: %d\n", errno); */ goto out; }
  MARK('6'); /* output qbuf ok */

  /* Start both queues */

  if (xioctl(fd, VIDIOC_STREAMON, &out_type) < 0)
    { /* printf("ERROR: OUTPUT STREAMON: %d\n", errno); */ goto out; }
  if (xioctl(fd, VIDIOC_STREAMON, &cap_type) < 0)
    { /* printf("ERROR: CAPTURE STREAMON: %d\n", errno); */ goto out; }
  MARK('7'); /* streaming */
  /* printf("  streaming...\n"); */
  /* fflush(stdout); */

  /* Wait for the encoded frame to be ready (DQBUF returns EAGAIN until then) */

  {
    struct pollfd pfd;
    int pret;

    pfd.fd     = fd;
    pfd.events = POLLIN;
    pfd.revents = 0;
    for (int tries = 0; tries < 50; tries++)
      {
        pret = poll(&pfd, 1, 100);
        if (pret < 0)
          { /* printf("ERROR: poll: %d\n", errno); */ goto out; }
        if (pret > 0 && (pfd.revents & POLLIN))
          {
            MARK('8'); /* poll ready */
            break;
          }
      }

    if (pret <= 0 || !(pfd.revents & POLLIN))
      {
        /* printf("ERROR: timeout waiting for encoded frame\n"); */
        goto out;
      }
  }

  /* Dequeue encoded JPEG */

  memset(&buf, 0, sizeof(buf));
  buf.type   = cap_type;
  buf.memory = V4L2_MEMORY_USERPTR;
  if (xioctl(fd, VIDIOC_DQBUF, &buf) < 0)
    { /* printf("ERROR: CAPTURE DQBUF: %d\n", errno); */ goto out; }
  /* printf("  encoded frame %u: %u bytes\n",
         (unsigned int)buf.index, (unsigned int)buf.bytesused); */
  /* fflush(stdout); */
  MARK('9'); /* encoded frame dequeued */
  /* Record the JPEG size at a dedicated RAM slot so the boot dump can verify
   * the encode actually produced data even when the USB console is lossy.
   * 0x5010ffc4 is not used by the boot M dump. */
  *((volatile uint32_t *)0x5010ffc4) = (uint32_t)buf.bytesused;

  /* Write JPEG to file */

  outfd = open(OUT_FILE, O_WRONLY | O_CREAT | O_TRUNC, 0644);
  if (outfd < 0)
    {
      *((volatile uint32_t *)0x5010ffc8) = (uint32_t)errno; /* open errno */
      /* printf("ERROR: open %s: %d\n", OUT_FILE, errno); */
      goto out;
    }
  *((volatile uint32_t *)0x5010ffc8) = (uint32_t)outfd;      /* open ok: fd */
  MARK('>'); /* capture.jpg opened for write */

  n = write(outfd, capbuf, buf.bytesused);
  *((volatile uint32_t *)0x5010ffcc) = (uint32_t)n;          /* write ret */
  if (n < 0)
    { /* printf("ERROR: write: %d\n", errno); */ goto out; }
  /* Record the first 4 bytes of the JPEG bitstream (should be FF D8 FF ...
   * for a valid SOI) at 0x5010ffb0 so the boot M dump can verify the magic
   * even if the console wedges on the final fflush below. */
  {
    FAR const uint8_t *jp = (FAR const uint8_t *)capbuf;
    *((volatile uint32_t *)0x5010ffb0) =
      (uint32_t)jp[0] | ((uint32_t)jp[1] << 8) |
      ((uint32_t)jp[2] << 16) | ((uint32_t)jp[3] << 24);
  }
  MARK('z'); /* JPEG written to /tmp/capture.jpg (before console flush) */
  /* Record the boot's non-zeroed dispatch counter (0x5010ff00) at the last
   * pre-flush marker so the post-hang M dump can tell whether the tick kept
   * firing while the final printf was wedged.  Delta vs 0x5010ff00 at dump =
   * dispatches during hang + boot. */
  *((volatile uint32_t *)0x5010ff08) = *((volatile uint32_t *)0x5010ff00);

  /* Pinpoint where the console flush wedges.  0x5010ff9c stage marker:
   *   0x5052 "PR" before the printf      -> printf itself is stuck (xmit.lock)
   *   0x4646 "FF" printf returned        -> fflush is stuck (tcdrain/xmitsem)
   *   0x4642 "FB" fflush returned        -> neither stuck, DONE follows */
  *((volatile uint32_t *)0x5010ff9c) = 0x5052;
  /* Second dispatch snapshot, taken at the PR stage itself (just before the
   * blocking printf).  Also acts as the gate for the driver's block-time
   * snapshot in serial.c (jbk == 0x5052 -> record). */
  *((volatile uint32_t *)0x5010ff04) = *((volatile uint32_t *)0x5010ff00);
  printf("  wrote %ld bytes to %s\n", (long)n, OUT_FILE);
  *((volatile uint32_t *)0x5010ff9c) = 0x4646;
  /* fflush(stdout); */
  *((volatile uint32_t *)0x5010ff9c) = 0x4642;

  /* DONE sentinel (only reached on the success path): overwrite the jfd
   * slot so a boot M dump can distinguish "jpegnc finished the final
   * fflush" from "stuck inside fflush above".  If jfd reads 0x444F4E45 the
   * flush returned and the app completed cleanly; if it still reads the
   * open fd (4) the process is wedged in the console flush. */
  *((volatile uint32_t *)0x5010ffc8) = 0x444F4E45;

  /* Cleanup.
   *
   * Do NOT re-queue the capture buffer here: it was already DQBUF'd, and a
   * re-queue would make the v4l2_m2m core fire CODEC_CAPTURE_AVAILABLE ->
   * jpeg_encode_one again, encoding the (already consumed) output frame a
   * second time.  close() below releases all buffers anyway, so the re-queue
   * serves no purpose.
   */

  xioctl(fd, VIDIOC_STREAMOFF, &cap_type);
  xioctl(fd, VIDIOC_STREAMOFF, &out_type);
  ret = EXIT_SUCCESS;

out:
  if (outfd >= 0) close(outfd);
  if (infd >= 0) close(infd);
  if (fd >= 0) close(fd);
  free(inbuf);
  free(capbuf);
  return ret;
}
