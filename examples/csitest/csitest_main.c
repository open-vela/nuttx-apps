/****************************************************************************
 * apps/examples/csitest/csitest_main.c
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

/* Minimal V4L2 capture test for the MIPI CSI camera pipeline.
 *
 * Flow: open /dev/video0, query the driver name, set RGB565 1024x600,
 * request two user-pointer buffers (FIFO mode), queue them, start
 * streaming and dequeue/requeue frames while printing statistics.
 *
 * Usage: csitest [-n <frames>]
 */

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>

#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <poll.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/ioctl.h>

#include <nuttx/video/video.h>

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#define CSITEST_DEVPATH        "/dev/video0"

#define CSITEST_WIDTH          1024
#define CSITEST_HEIGHT         600
#define CSITEST_BUFNUM         2
#define CSITEST_BUFSIZE        (CSITEST_WIDTH * CSITEST_HEIGHT * 2)
#define CSITEST_BUFALIGN       64

#define CSITEST_DEFAULT_FRAMES 10
#define CSITEST_POLL_TIMEO_MS  5000

#define CSITEST_HEXDUMP_BYTES  16

/****************************************************************************
 * Private Functions
 ****************************************************************************/

/****************************************************************************
 * Name: csitest_now_ms
 *
 * Description:
 *   Return a monotonic timestamp in milliseconds.
 *
 ****************************************************************************/

static uint64_t csitest_now_ms(void)
{
  struct timespec ts;

  clock_gettime(CLOCK_MONOTONIC, &ts);
  return (uint64_t)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

/****************************************************************************
 * Name: csitest_print_frame
 *
 * Description:
 *   Print statistics of one captured frame: sequence number, size, the
 *   first bytes as hex, a simple additive checksum and the time since
 *   the previous frame.
 *
 ****************************************************************************/

static void csitest_print_frame(int frame, FAR struct v4l2_buffer *buf,
                                uint64_t delta_ms)
{
  FAR const uint8_t *data = (FAR const uint8_t *)buf->m.userptr;
  uint32_t checksum = 0;
  size_t nbytes;
  size_t i;

  nbytes = buf->bytesused;
  if (nbytes == 0 || nbytes > buf->length)
    {
      nbytes = buf->length;
    }

  for (i = 0; i < nbytes; i++)
    {
      checksum += data[i];
    }

  printf("frame %3d: idx=%" PRIu32 " bytes=%zu csum=0x%08" PRIx32
         " dt=%" PRIu64 "ms\n",
         frame, buf->index, nbytes, checksum, delta_ms);

  printf("  data:");
  for (i = 0; i < CSITEST_HEXDUMP_BYTES && i < nbytes; i++)
    {
      printf(" %02x", data[i]);
    }

  printf("\n");
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: main
 ****************************************************************************/

int main(int argc, FAR char *argv[])
{
  FAR uint8_t *buffers[CSITEST_BUFNUM];
  struct v4l2_capability cap;
  struct v4l2_format fmt;
  struct v4l2_requestbuffers req;
  struct v4l2_buffer buf;
  struct pollfd fds;
  enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  uint64_t last_ms;
  uint64_t now_ms;
  int frames = CSITEST_DEFAULT_FRAMES;
  int ret = EXIT_FAILURE;
  bool streaming = false;
  int fd;
  int i;

  /* Parse the optional "-n <frames>" argument */

  if (argc == 3 && strcmp(argv[1], "-n") == 0)
    {
      frames = atoi(argv[2]);
      if (frames <= 0)
        {
          fprintf(stderr, "Invalid frame count: %s\n", argv[2]);
          return EXIT_FAILURE;
        }
    }
  else if (argc != 1)
    {
      fprintf(stderr, "usage: %s [-n <frames>]\n", argv[0]);
      return EXIT_FAILURE;
    }

  memset(buffers, 0, sizeof(buffers));

  /* Open the capture device */

  fd = open(CSITEST_DEVPATH, 0);
  if (fd < 0)
    {
      perror("open " CSITEST_DEVPATH);
      return EXIT_FAILURE;
    }

  /* Query and show the driver name */

  memset(&cap, 0, sizeof(cap));
  if (ioctl(fd, VIDIOC_QUERYCAP, (uintptr_t)&cap) < 0)
    {
      perror("VIDIOC_QUERYCAP");
      goto errout;
    }

  printf("driver: %s\n", cap.driver);

  /* Set the capture format: RGB565 1024x600 */

  memset(&fmt, 0, sizeof(fmt));
  fmt.type                = type;
  fmt.fmt.pix.width       = CSITEST_WIDTH;
  fmt.fmt.pix.height      = CSITEST_HEIGHT;
  fmt.fmt.pix.field       = V4L2_FIELD_ANY;
  fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_RGB565;

  if (ioctl(fd, VIDIOC_S_FMT, (uintptr_t)&fmt) < 0)
    {
      perror("VIDIOC_S_FMT");
      goto errout;
    }

  /* Request user-pointer buffers in FIFO mode */

  memset(&req, 0, sizeof(req));
  req.type   = type;
  req.memory = V4L2_MEMORY_USERPTR;
  req.count  = CSITEST_BUFNUM;
  req.mode   = V4L2_BUF_MODE_FIFO;

  if (ioctl(fd, VIDIOC_REQBUFS, (uintptr_t)&req) < 0)
    {
      perror("VIDIOC_REQBUFS");
      goto errout;
    }

  /* Allocate and queue the frame buffers */

  for (i = 0; i < CSITEST_BUFNUM; i++)
    {
      buffers[i] = memalign(CSITEST_BUFALIGN, CSITEST_BUFSIZE);
      if (buffers[i] == NULL)
        {
          fprintf(stderr, "Out of memory for buffer %d (%d bytes)\n",
                  i, CSITEST_BUFSIZE);
          goto errout;
        }
    }

  for (i = 0; i < CSITEST_BUFNUM; i++)
    {
      memset(&buf, 0, sizeof(buf));
      buf.type      = type;
      buf.memory    = V4L2_MEMORY_USERPTR;
      buf.index     = i;
      buf.m.userptr = (uintptr_t)buffers[i];
      buf.length    = CSITEST_BUFSIZE;

      if (ioctl(fd, VIDIOC_QBUF, (uintptr_t)&buf) < 0)
        {
          perror("VIDIOC_QBUF");
          goto errout;
        }
    }

  /* Start streaming */

  if (ioctl(fd, VIDIOC_STREAMON, (uintptr_t)&type) < 0)
    {
      perror("VIDIOC_STREAMON");
      goto errout;
    }

  streaming = true;
  printf("capturing %d frames %dx%d RGB565 from %s\n",
         frames, CSITEST_WIDTH, CSITEST_HEIGHT, CSITEST_DEVPATH);

  last_ms = csitest_now_ms();

  /* Capture loop: dequeue, print statistics, requeue */

  for (i = 0; i < frames; i++)
    {
      fds.fd      = fd;
      fds.events  = POLLIN;
      fds.revents = 0;

      ret = poll(&fds, 1, CSITEST_POLL_TIMEO_MS);
      if (ret < 0)
        {
          perror("poll");
          ret = EXIT_FAILURE;
          goto errout;
        }
      else if (ret == 0)
        {
          fprintf(stderr, "Timeout waiting for frame %d (%d ms)\n",
                  i, CSITEST_POLL_TIMEO_MS);
          ret = EXIT_FAILURE;
          goto errout;
        }

      memset(&buf, 0, sizeof(buf));
      buf.type   = type;
      buf.memory = V4L2_MEMORY_USERPTR;

      if (ioctl(fd, VIDIOC_DQBUF, (uintptr_t)&buf) < 0)
        {
          perror("VIDIOC_DQBUF");
          ret = EXIT_FAILURE;
          goto errout;
        }

      now_ms = csitest_now_ms();
      csitest_print_frame(i, &buf, now_ms - last_ms);
      last_ms = now_ms;

      if (ioctl(fd, VIDIOC_QBUF, (uintptr_t)&buf) < 0)
        {
          perror("VIDIOC_QBUF");
          ret = EXIT_FAILURE;
          goto errout;
        }
    }

  printf("done: %d frames captured\n", frames);
  ret = EXIT_SUCCESS;

errout:
  if (streaming)
    {
      if (ioctl(fd, VIDIOC_STREAMOFF, (uintptr_t)&type) < 0)
        {
          perror("VIDIOC_STREAMOFF");
          ret = EXIT_FAILURE;
        }
    }

  close(fd);

  for (i = 0; i < CSITEST_BUFNUM; i++)
    {
      free(buffers[i]);
    }

  return ret;
}
