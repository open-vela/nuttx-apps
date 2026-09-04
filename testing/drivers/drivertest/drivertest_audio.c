/****************************************************************************
 * apps/testing/drivers/drivertest/drivertest_audio.c
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

#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <sys/param.h>
#include <time.h>

#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <assert.h>
#include <cmocka.h>
#include <errno.h>
#include <dirent.h>
#include <mqueue.h>
#include <debug.h>
#include <unistd.h>

#include <nuttx/audio/audio.h>

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#define OPTARG_TO_VALUE(value, type, base)                            \
  do                                                                  \
    {                                                                 \
      FAR char *ptr;                                                  \
      value = (type)strtoul(optarg, &ptr, base);                      \
      if (*ptr != '\0')                                               \
        {                                                             \
          printf("Parameter error: %s\n", optarg);                    \
          audio_test_help(argv[0], EXIT_FAILURE);                     \
        }                                                             \
    } while (0)

#define AUDIO_TEST_PROMPT_RATE       44100
#define AUDIO_TEST_PROMPT_FREQ       1000
#define AUDIO_TEST_PROMPT_AMPLITUDE  12000
#define AUDIO_TEST_NR_Q              27
#define AUDIO_TEST_NR_B0             133889942LL
#define AUDIO_TEST_NR_B1            -267574370LL
#define AUDIO_TEST_NR_B2             133889942LL
#define AUDIO_TEST_NR_A1            -267574370LL
#define AUDIO_TEST_NR_A2             133562155LL

/****************************************************************************
 * Private Types
 ****************************************************************************/

struct audio_state_s
{
  char       outfile[PATH_MAX];
  char       outdev[PATH_MAX];
  char       indev[PATH_MAX];
  int        outdev_fd;
  int        indev_fd;
  int        out_fd;

  uint32_t   samprate;
  uint32_t   format;
  uint32_t   bpsamp;
  uint32_t   chans;
  uint32_t   chmap;

  char       mqname[16];
  int        direction;
  uint32_t   duration;
  uint32_t   idx;
  FAR uint8_t *capture_buf;
  uint32_t   capture_len;
  uint32_t   capture_cap;
  uint32_t   playback_pos;
  int64_t    nr_x1;
  int64_t    nr_x2;
  int64_t    nr_y1;
  int64_t    nr_y2;
  bool       playback_from_capture;
  mqd_t      mq;

#ifdef CONFIG_AUDIO_MULTI_SESSION
  FAR void    *session;
#endif
};

/****************************************************************************
 * Private Functions
 ****************************************************************************/

static int audio_test_enqueuebuffer(FAR struct audio_state_s *state,
                                    FAR struct ap_buffer_s *apb,
                                    int direction);
static int audio_test_alloc_buffer(FAR struct audio_state_s *state,
                                   FAR struct ap_buffer_s **bufs,
                                   FAR struct ap_buffer_info_s *buf_info,
                                   int direction);
static int audio_test_free_buffer(FAR struct audio_state_s *state,
                                  FAR struct ap_buffer_s **bufs,
                                  FAR struct ap_buffer_info_s *buf_info,
                                  int direction);
static int audio_test_start(FAR struct audio_state_s *state, int direction);

static int audio_test_capture_alloc(FAR struct audio_state_s *state,
                                    FAR struct ap_buffer_info_s *buf_info)
{
  uint64_t capacity;

  free(state->capture_buf);
  state->capture_buf = NULL;
  state->capture_len = 0;
  state->capture_cap = 0;
  state->playback_pos = 0;
  state->playback_from_capture = false;

  capacity = (uint64_t)state->samprate * state->duration *
             state->chans * ((state->bpsamp + 7) / 8);
  capacity += (uint64_t)buf_info->buffer_size * buf_info->nbuffers;

  if (capacity == 0 || capacity > UINT32_MAX)
    {
      return -EFBIG;
    }

  state->capture_buf = malloc((size_t)capacity);
  if (state->capture_buf == NULL)
    {
      syslog(LOG_ERR, "capture cache alloc %lu bytes failed\n",
             (unsigned long)capacity);
      return -ENOMEM;
    }

  state->capture_cap = (uint32_t)capacity;
  syslog(LOG_INFO, "Capture cache %" PRIu32 " bytes\n",
         state->capture_cap);
  return OK;
}

static int audio_test_capture_flush(FAR struct audio_state_s *state)
{
  FAR const uint8_t *buffer;
  uint32_t remaining;

  if (state->capture_buf == NULL || state->capture_len == 0)
    {
      return OK;
    }

  buffer = state->capture_buf;
  remaining = state->capture_len;
  while (remaining > 0)
    {
      ssize_t nwritten = write(state->out_fd, buffer, remaining);
      if (nwritten < 0)
        {
          return -errno;
        }

      if (nwritten == 0)
        {
          return -EIO;
        }

      buffer += nwritten;
      remaining -= nwritten;
    }

  return OK;
}

static int64_t audio_test_q27_round(int64_t value)
{
  if (value < 0)
    {
      return -(((-value) + (1LL << (AUDIO_TEST_NR_Q - 1))) >>
               AUDIO_TEST_NR_Q);
    }

  return (value + (1LL << (AUDIO_TEST_NR_Q - 1))) >> AUDIO_TEST_NR_Q;
}

static int16_t audio_test_clip_s16(int64_t value)
{
  if (value > INT16_MAX)
    {
      return INT16_MAX;
    }

  if (value < INT16_MIN)
    {
      return INT16_MIN;
    }

  return (int16_t)value;
}

static void audio_test_capture_denoise(FAR struct audio_state_s *state,
                                       FAR uint8_t *buffer,
                                       uint32_t nbytes)
{
  uint32_t i;

  if (state->samprate != 44100 || state->bpsamp != 16 ||
      state->chans != 1)
    {
      return;
    }

  for (i = 0; i + 1 < nbytes; i += 2)
    {
      int64_t sample;
      int64_t filtered;
      int16_t output;

      sample = (int16_t)(buffer[i] | (buffer[i + 1] << 8));
      filtered = AUDIO_TEST_NR_B0 * sample +
                 AUDIO_TEST_NR_B1 * state->nr_x1 +
                 AUDIO_TEST_NR_B2 * state->nr_x2 -
                 AUDIO_TEST_NR_A1 * state->nr_y1 -
                 AUDIO_TEST_NR_A2 * state->nr_y2;
      filtered = audio_test_q27_round(filtered);
      output = audio_test_clip_s16(filtered);

      state->nr_x2 = state->nr_x1;
      state->nr_x1 = sample;
      state->nr_y2 = state->nr_y1;
      state->nr_y1 = output;

      buffer[i] = (uint8_t)output;
      buffer[i + 1] = (uint8_t)((uint16_t)output >> 8);
    }
}

/****************************************************************************
 * Name: audio_test_writebuffer
 ****************************************************************************/

static int audio_test_writebuffer(FAR struct audio_state_s *state,
                                  FAR struct ap_buffer_s *apb)
{
  ssize_t nwritten;

  if (state->out_fd == -1 && state->capture_buf == NULL)
    {
      return -ENODATA;
    }

  audio_test_capture_denoise(state, apb->samp, apb->nbytes);

  if (state->capture_buf != NULL)
    {
      if (apb->nbytes > state->capture_cap - state->capture_len)
        {
          return -ENOSPC;
        }

      memcpy(state->capture_buf + state->capture_len, apb->samp,
             apb->nbytes);
      state->capture_len += apb->nbytes;
      state->idx += apb->nbytes;

      apb->curbyte = 0;
      apb->flags   = 0;

      return OK;
    }

  nwritten = write(state->out_fd, apb->samp, apb->nbytes);
  if (nwritten < 0)
    {
      return -errno;
    }

  if (nwritten != apb->nbytes)
    {
      return -EIO;
    }

  state->idx += nwritten;

  apb->curbyte = 0;
  apb->flags   = 0;

  return OK;
}

/****************************************************************************
 * Name: player_readbuffer
 ****************************************************************************/

static int audio_test_readbuffer(FAR struct audio_state_s *state,
                                 FAR struct ap_buffer_s *apb)
{
  ssize_t nread;

  if (state->playback_from_capture)
    {
      uint32_t remaining;

      if (state->playback_pos >= state->capture_len)
        {
          return -ENODATA;
        }

      remaining = state->capture_len - state->playback_pos;
      apb->nbytes = MIN(apb->nmaxbytes, remaining);
      memcpy(apb->samp, state->capture_buf + state->playback_pos,
             apb->nbytes);
      state->playback_pos += apb->nbytes;
      apb->curbyte = 0;
      apb->flags = 0;

      if (state->playback_pos >= state->capture_len)
        {
          apb->flags |= AUDIO_APB_FINAL;
        }

      return OK;
    }

  if (state->out_fd == -1)
    {
      return -ENODATA;
    }

  nread = read(state->out_fd, apb->samp, apb->nmaxbytes);
  if (nread < 0)
    {
      int err = errno;

      apb->nbytes = 0;
      close(state->out_fd);
      state->out_fd = -1;
      return -err;
    }

  apb->nbytes  = nread;
  apb->curbyte = 0;
  apb->flags   = 0;

  if (apb->nbytes < apb->nmaxbytes)
    {
      close(state->out_fd);
      state->out_fd = -1;
      apb->flags |= AUDIO_APB_FINAL;
    }

  return OK;
}

/****************************************************************************
 * Name: audio_test_receive_msg
 ****************************************************************************/

static ssize_t audio_test_receive_msg(mqd_t mq, FAR struct audio_msg_s *msg,
                                      FAR unsigned int *prio)
{
  struct timespec abstime;

  clock_gettime(CLOCK_REALTIME, &abstime);
  abstime.tv_nsec += 100 * 1000 * 1000;
  if (abstime.tv_nsec >= 1000 * 1000 * 1000)
    {
      abstime.tv_sec++;
      abstime.tv_nsec -= 1000 * 1000 * 1000;
    }

  return mq_timedreceive(mq, (FAR char *)msg, sizeof(*msg), prio, &abstime);
}

/****************************************************************************
 * Name: audio_test_fill_prompt_buffer
 ****************************************************************************/

static void audio_test_fill_prompt_buffer(FAR struct ap_buffer_s *apb,
                                          FAR uint32_t *phase)
{
  uint32_t period = AUDIO_TEST_PROMPT_RATE / AUDIO_TEST_PROMPT_FREQ;
  uint32_t half_period;
  uint32_t nsamples;
  uint32_t i;

  if (period < 2)
    {
      period = 2;
    }

  half_period = period / 2;
  nsamples = apb->nmaxbytes / sizeof(int16_t);

  for (i = 0; i < nsamples; i++)
    {
      int16_t sample = ((*phase % period) < half_period) ?
                       AUDIO_TEST_PROMPT_AMPLITUDE :
                       -AUDIO_TEST_PROMPT_AMPLITUDE;

      apb->samp[i * 2] = sample & 0xff;
      apb->samp[i * 2 + 1] = ((uint16_t)sample >> 8) & 0xff;
      (*phase)++;
    }

  apb->nbytes = nsamples * sizeof(int16_t);
  apb->curbyte = 0;
  apb->flags = 0;
}

/****************************************************************************
 * Name: audio_test_prompt_tone
 ****************************************************************************/

static void audio_test_prompt_tone(FAR struct audio_state_s *state)
{
  FAR struct ap_buffer_s **bufs = NULL;
  struct ap_buffer_info_s buf_info;
  struct audio_caps_desc_s cap_desc;
  struct audio_msg_s msg;
  struct mq_attr attr;
  unsigned int prio;
  uint32_t phase = 0;
  bool close_output = false;
  bool registered = false;
  bool reserved = false;
  int unconsumed = 0;
  int queued = 0;
  int ret;
  int x;

  memset(&buf_info, 0, sizeof(buf_info));
  memset(&cap_desc, 0, sizeof(cap_desc));

  if (state->outdev_fd < 0)
    {
      state->outdev_fd = open(state->outdev, O_RDWR | O_CLOEXEC);
      if (state->outdev_fd < 0)
        {
          syslog(LOG_ERR, "prompt tone: open %s failed errno=%d\n",
                 state->outdev, errno);
          return;
        }

      close_output = true;
    }

#ifdef CONFIG_AUDIO_MULTI_SESSION
  ret = ioctl(state->outdev_fd, AUDIOIOC_RESERVE,
              (unsigned long)&state->session);
#else
  ret = ioctl(state->outdev_fd, AUDIOIOC_RESERVE, 0);
#endif
  if (ret < 0)
    {
      syslog(LOG_ERR, "prompt tone: reserve failed ret=%d errno=%d\n",
             ret, errno);
      goto out_close;
    }

  reserved = true;

#ifdef CONFIG_AUDIO_MULTI_SESSION
  cap_desc.session = state->session;
#endif
  cap_desc.caps.ac_len            = sizeof(struct audio_caps_s);
  cap_desc.caps.ac_type           = AUDIO_TYPE_OUTPUT;
  cap_desc.caps.ac_channels       = 1;
  cap_desc.caps.ac_chmap          = 0;
  cap_desc.caps.ac_controls.hw[0] = AUDIO_TEST_PROMPT_RATE;
  cap_desc.caps.ac_controls.b[3]  = AUDIO_TEST_PROMPT_RATE >> 16;
  cap_desc.caps.ac_controls.b[2]  = 16;
  cap_desc.caps.ac_subtype        = AUDIO_FMT_PCM;

  ret = ioctl(state->outdev_fd, AUDIOIOC_CONFIGURE,
              (unsigned long)&cap_desc);
  if (ret < 0)
    {
      syslog(LOG_ERR, "prompt tone: configure failed ret=%d errno=%d\n",
             ret, errno);
      goto out_release;
    }

  ret = ioctl(state->outdev_fd, AUDIOIOC_REGISTERMQ,
              (unsigned long)state->mq);
  if (ret < 0)
    {
      syslog(LOG_ERR, "prompt tone: register mq failed ret=%d errno=%d\n",
             ret, errno);
      goto out_release;
    }

  registered = true;

  ret = ioctl(state->outdev_fd, AUDIOIOC_GETBUFFERINFO,
              (unsigned long)&buf_info);
  if (ret < 0)
    {
      buf_info.buffer_size = CONFIG_AUDIO_BUFFER_NUMBYTES;
      buf_info.nbuffers = CONFIG_AUDIO_NUM_BUFFERS;
    }

  bufs = (FAR struct ap_buffer_s **)
         calloc(buf_info.nbuffers, sizeof(FAR void *));
  if (bufs == NULL)
    {
      syslog(LOG_ERR, "prompt tone: alloc pointer array failed\n");
      goto out_unregister;
    }

  ret = audio_test_alloc_buffer(state, bufs, &buf_info, AUDIO_TYPE_OUTPUT);
  if (ret < 0)
    {
      syslog(LOG_ERR, "prompt tone: alloc audio buffer failed ret=%d\n",
             ret);
      goto out_unregister;
    }

  for (x = 0; x < buf_info.nbuffers; x++)
    {
      audio_test_fill_prompt_buffer(bufs[x], &phase);
      if (x == buf_info.nbuffers - 1)
        {
          bufs[x]->flags |= AUDIO_APB_FINAL;
        }

      ret = audio_test_enqueuebuffer(state, bufs[x], AUDIO_TYPE_OUTPUT);
      if (ret != OK)
        {
          syslog(LOG_ERR, "prompt tone: enqueue failed ret=%d\n", ret);
          break;
        }

      queued++;
    }

  if (queued == 0)
    {
      goto out_unregister;
    }

  ret = audio_test_start(state, AUDIO_TYPE_OUTPUT);
  if (ret < 0)
    {
      syslog(LOG_ERR, "prompt tone: start failed ret=%d errno=%d\n",
             ret, errno);
      goto out_unregister;
    }

  syslog(LOG_INFO, "prompt tone before capture\n");
  unconsumed = queued;

  while (true)
    {
      ret = mq_receive(state->mq, (FAR char *)&msg, sizeof(msg), &prio);
      if (ret != sizeof(msg))
        {
          continue;
        }

      if (msg.msg_id == AUDIO_MSG_DEQUEUE)
        {
          unconsumed--;
        }
      else if (msg.msg_id == AUDIO_MSG_COMPLETE)
        {
          break;
        }
    }

  do
    {
      ret = mq_getattr(state->mq, &attr);
      if (ret < 0 || (attr.mq_curmsgs == 0 && unconsumed <= 0))
        {
          break;
        }

      ret = mq_receive(state->mq, (FAR char *)&msg, sizeof(msg), &prio);
      if (ret == sizeof(msg) && msg.msg_id == AUDIO_MSG_DEQUEUE)
        {
          unconsumed--;
        }
    }
  while (ret >= 0);

out_unregister:
  if (registered)
    {
      ioctl(state->outdev_fd, AUDIOIOC_UNREGISTERMQ,
            (unsigned long)state->mq);
    }

out_release:
  if (reserved)
    {
#ifdef CONFIG_AUDIO_MULTI_SESSION
      ioctl(state->outdev_fd, AUDIOIOC_RELEASE, (unsigned long)state->session);
#else
      ioctl(state->outdev_fd, AUDIOIOC_RELEASE, 0);
#endif
    }

  if (bufs != NULL)
    {
      audio_test_free_buffer(state, bufs, &buf_info, AUDIO_TYPE_OUTPUT);
      free(bufs);
    }

out_close:
  if (close_output)
    {
      close(state->outdev_fd);
      state->outdev_fd = -1;
    }
}

/****************************************************************************
 * Name: audio_test_enqueuebuffer
 ****************************************************************************/

static int audio_test_enqueuebuffer(FAR struct audio_state_s *state,
                                    FAR struct ap_buffer_s *apb,
                                    int direction)
{
  struct audio_buf_desc_s bufdesc;
  int ret = 0;
  int fd;

  fd = direction == AUDIO_TYPE_OUTPUT ?
       state->outdev_fd :
       state->indev_fd;

  if (direction == AUDIO_TYPE_INPUT)
    {
      apb->nbytes = apb->nmaxbytes;
      apb->curbyte = 0;
      apb->flags = 0;
    }

#ifdef CONFIG_AUDIO_MULTI_SESSION
  bufdesc.session  = state->session;
#endif

  bufdesc.numbytes = apb->nbytes;
  bufdesc.u.buffer = apb;

  ret = ioctl(fd, AUDIOIOC_ENQUEUEBUFFER, (unsigned long)&bufdesc);
  if (ret < 0)
    {
      return -errno;
    }

  return OK;
}

static int audio_test_alloc_buffer(FAR struct audio_state_s *state,
                                   FAR struct ap_buffer_s **bufs,
                                   FAR struct ap_buffer_info_s *buf_info,
                                   int direction)
{
  struct audio_buf_desc_s buf_desc;
  int ret = 0;
  int fd;
  int x;

  fd = direction == AUDIO_TYPE_OUTPUT ?
       state->outdev_fd :
       state->indev_fd;

  for (x = 0; x < buf_info->nbuffers; x++)
    {
#ifdef CONFIG_AUDIO_MULTI_SESSION
      buf_desc.session = state->session;
#endif
      buf_desc.numbytes = buf_info->buffer_size;
      buf_desc.u.pbuffer = &bufs[x];

      ret = ioctl(fd, AUDIOIOC_ALLOCBUFFER, (unsigned long)&buf_desc);
      if (ret != sizeof(buf_desc))
        {
          goto err_out;
        }
    }

  return OK;

err_out:
  audio_test_free_buffer(state, bufs, buf_info, direction);
  return ret < 0 ? ret : -EIO;
}

static int audio_test_free_buffer(FAR struct audio_state_s *state,
                                  FAR struct ap_buffer_s **bufs,
                                  FAR struct ap_buffer_info_s *buf_info,
                                  int direction)
{
  struct audio_buf_desc_s buf_desc;
  int ret = 0;
  int fd;
  int x;

  fd = direction == AUDIO_TYPE_OUTPUT ?
       state->outdev_fd :
       state->indev_fd;

  for (x = 0; x < buf_info->nbuffers; x++)
    {
      if (bufs[x] != NULL)
        {
#ifdef CONFIG_AUDIO_MULTI_SESSION
          buf_desc.session = state->session;
#endif
          buf_desc.u.buffer = bufs[x];
          ret = ioctl(fd, AUDIOIOC_FREEBUFFER, (unsigned long)&buf_desc);
          if (ret < 0)
            {
              break;
            }

          bufs[x] = NULL;
        }
    }

  return ret;
}

static void audio_test_help(FAR const char *progname, int exitcode)
{
  printf("Usage: %s\n"
         " -a <test case>\n"
         " -t <duration>\n"
         " -i <input  device e.g./dev/audio/pcm0c>\n"
         " -o <output device e.g./dev/audio/pcm0p>\n"
         " -p <output file>\n"
         " -f <format>\n"
         " -b <bytes per sample> \n"
         " -s <sample rate> \n"
         " -c <channles> \n",
         progname);
  printf(" [-a testcase] selects the testcase\n"
         " Case 1: Capture\n"
         " Case 2: Playack\n"
         " Case 3: First Capture, and then Playback\n"
         );
  printf(" -h shows this message and exits\n");

  exit(exitcode);
}

/****************************************************************************
 * Name: parse_commandline
 ****************************************************************************/

static void parse_commandline(FAR struct audio_state_s *state, int argc,
                              FAR char **argv)
{
  int option;

  while ((option = getopt(argc, argv, "a:t:o:i:f:p:c:b:s:")) != ERROR)
    {
      switch (option)
        {
          case 'a':
            {
              OPTARG_TO_VALUE(state->direction, uint32_t, 10);
              break;
            }

          case 't':
            {
              OPTARG_TO_VALUE(state->duration, uint32_t, 10);
              break;
            }

          case 'o':
            {
              strcpy(state->outdev, optarg);
              break;
            }

          case 'i':
            {
              strcpy(state->indev, optarg);
              break;
            }

          case 'f':
            {
              if (!strcmp(optarg, "mp3"))
                {
                  state->format = AUDIO_FMT_MP3;
                }
              else
                {
                  state->format = AUDIO_FMT_PCM;
                }
              break;
            }

          case 'p':
            {
              strcpy(state->outfile, optarg);
              break;
            }

         case 'c':
            {
              OPTARG_TO_VALUE(state->chans, uint32_t, 10);
              break;
            }

          case 'b':
            {
              OPTARG_TO_VALUE(state->bpsamp, uint32_t, 10);
              break;
            }

          case 's':
            {
              OPTARG_TO_VALUE(state->samprate, uint32_t, 10);
              break;
            }

          case '?':
            printf("Unknown option: %c\n", optopt);
            audio_test_help(argv[0], EXIT_FAILURE);
            break;
        }
    }
}

/****************************************************************************
 * Name: audio_test_prepare
 ****************************************************************************/

static int audio_test_prepare(FAR struct audio_state_s *state,
                              FAR struct ap_buffer_info_s *buf_info,
                              int direction)
{
  struct audio_caps_desc_s cap_desc;
  struct audio_caps_s caps;
  int ret = 0;
  int fd;

  fd = direction == AUDIO_TYPE_OUTPUT ?
       state->outdev_fd :
       state->indev_fd;

  memset(&caps, 0, sizeof(caps));
  memset(&cap_desc, 0, sizeof(cap_desc));
  memset(buf_info, 0, sizeof(*buf_info));

  if (direction == AUDIO_TYPE_OUTPUT)
    {
      if (state->capture_buf != NULL && state->capture_len > 0)
        {
          state->playback_pos = 0;
          state->playback_from_capture = true;
          state->out_fd = -1;
          syslog(LOG_INFO, "Playback from capture cache %" PRIu32
                 " bytes\n", state->capture_len);
        }
      else
        {
          state->out_fd = open(state->outfile, O_RDONLY);
        }
    }
  else
    {
      if (state->direction & AUDIO_TYPE_OUTPUT)
        {
          state->out_fd = -1;
        }
      else
        {
          state->out_fd = open(state->outfile, O_WRONLY | O_CREAT | O_TRUNC,
                               0666);
        }
    }

  if (state->out_fd == -1 && !state->playback_from_capture &&
      !(direction == AUDIO_TYPE_INPUT &&
        (state->direction & AUDIO_TYPE_OUTPUT) != 0))
    {
      return -ENOENT;
    }

  caps.ac_len  = sizeof(caps);
  caps.ac_type = AUDIO_TYPE_QUERY;
  caps.ac_subtype = AUDIO_TYPE_QUERY;

  ret = ioctl(fd, AUDIOIOC_GETCAPS, (unsigned long)&caps);
  if (ret != caps.ac_len)
    {
      return ret;
    }

  if ((caps.ac_controls.b[0] & direction) == 0 ||
      (caps.ac_format.hw & (1 << (state->format - 1))) == 0)
    {
      return -EINVAL;
    }

#ifdef CONFIG_AUDIO_MULTI_SESSION
  ret = ioctl(fd, AUDIOIOC_RESERVE, (unsigned long)&state->session);
#else
  ret = ioctl(fd, AUDIOIOC_RESERVE, 0);
#endif

#ifdef CONFIG_AUDIO_MULTI_SESSION
  cap_desc.session = state->session;
#endif
  cap_desc.caps.ac_len            = sizeof(struct audio_caps_s);
  cap_desc.caps.ac_type           = direction;
  cap_desc.caps.ac_channels       = state->chans;
  cap_desc.caps.ac_chmap          = state->chmap;
  cap_desc.caps.ac_controls.hw[0] = state->samprate;
  cap_desc.caps.ac_controls.b[3]  = state->samprate >> 16;
  cap_desc.caps.ac_controls.b[2]  = state->bpsamp;
  cap_desc.caps.ac_subtype        = state->format;
  ret = ioctl(fd, AUDIOIOC_CONFIGURE, (unsigned long)&cap_desc);
  if (ret < 0)
    {
      return -errno;
    }

  ret = ioctl(fd, AUDIOIOC_REGISTERMQ, (unsigned long)state->mq);
  if (ret < 0)
    {
      return -errno;
    }

  ret = ioctl(fd, AUDIOIOC_GETBUFFERINFO, (unsigned long)buf_info);
  if (ret < 0)
    {
      buf_info->buffer_size = CONFIG_AUDIO_BUFFER_NUMBYTES;
      buf_info->nbuffers = CONFIG_AUDIO_NUM_BUFFERS;
    }

  if (direction == AUDIO_TYPE_INPUT)
    {
      ret = audio_test_capture_alloc(state, buf_info);
      if (ret < 0)
        {
          return ret;
        }
    }

  return OK;
}

static int audio_test_start(FAR struct audio_state_s *state, int direction)
{
  int ret = 0;
  int fd;

  fd = direction == AUDIO_TYPE_OUTPUT ?
       state->outdev_fd :
       state->indev_fd;

#ifdef CONFIG_AUDIO_MULTI_SESSION
  ret = ioctl(fd, AUDIOIOC_START,
                    (unsigned long)state->session);
#else
  ret = ioctl(fd, AUDIOIOC_START, 0);
#endif

  return ret;
}

static bool audio_test_timeout(FAR struct audio_state_s *state,
                               int direction, struct timeval start)
{
  struct timeval now;
  struct timeval delta;
  struct timeval wait;
  uint64_t bytes_per_sec;
  uint64_t playback_bytes;

  if (direction == AUDIO_TYPE_OUTPUT)
    {
      bytes_per_sec = (uint64_t)state->samprate * state->chans *
                      ((state->bpsamp + 7) / 8);
      playback_bytes = state->playback_from_capture ? state->capture_len : 0;

      if (bytes_per_sec > 0 && playback_bytes > 0)
        {
          wait.tv_sec = (playback_bytes + bytes_per_sec - 1) /
                        bytes_per_sec;
        }
      else
        {
          wait.tv_sec = state->duration;
        }

      wait.tv_sec += 3;
      wait.tv_usec = 0;

      gettimeofday(&now, NULL);
      timersub(&now, &start, &delta);
      return timercmp(&delta, &wait, > /* For checkpatch */);
    }

  wait.tv_sec = state->duration;
  wait.tv_usec = 0;

  gettimeofday(&now, NULL);
  timersub(&now, &start, &delta);
  return timercmp(&delta, &wait, > /* For checkpatch */);
}

static int audio_test_stop(FAR struct audio_state_s *state, int direction)
{
  int ret = 0;
  int fd;

  fd = direction == AUDIO_TYPE_OUTPUT ?
       state->outdev_fd :
       state->indev_fd;

#ifdef CONFIG_AUDIO_MULTI_SESSION
  ret = ioctl(fd, AUDIOIOC_STOP,
                    (unsigned long)state->session);
#else
  ret = ioctl(fd, AUDIOIOC_STOP, 0);
#endif

  return ret;
}

static int audio_test_cleanup(FAR struct audio_state_s *state, int direction)
{
  int fd;
  int ret = 0;

  fd = direction == AUDIO_TYPE_OUTPUT ?
       state->outdev_fd :
       state->indev_fd;

  ioctl(fd, AUDIOIOC_UNREGISTERMQ, (unsigned long)state->mq);

#ifdef CONFIG_AUDIO_MULTI_SESSION
  ioctl(fd, AUDIOIOC_RELEASE, (unsigned long)state->session);
#else
  ioctl(fd, AUDIOIOC_RELEASE, 0);
#endif

  if (state->out_fd >= 0)
    {
      if (direction == AUDIO_TYPE_INPUT)
        {
          ret = fsync(state->out_fd);
          if (ret < 0)
            {
              syslog(LOG_ERR, "fsync %s failed errno=%d\n",
                     state->outfile, errno);
            }
        }

      if (close(state->out_fd) < 0 && ret == 0)
        {
          ret = -errno;
          syslog(LOG_ERR, "close %s failed errno=%d\n",
                 state->outfile, errno);
        }

      state->out_fd = -1;
    }

  if (direction != AUDIO_TYPE_INPUT ||
      (state->direction & AUDIO_TYPE_OUTPUT) == 0)
    {
      free(state->capture_buf);
      state->capture_buf = NULL;
      state->capture_len = 0;
      state->capture_cap = 0;
      state->playback_pos = 0;
      state->playback_from_capture = false;
    }

  return ret;
}

#ifdef CONFIG_AUDIO_NULL
static void audio_test_query_caps(int fd, uint8_t direction)
{
  struct audio_caps_s caps;
  int ret;

  memset(&caps, 0, sizeof(caps));
  caps.ac_len = sizeof(caps);
  caps.ac_type = AUDIO_TYPE_QUERY;
  caps.ac_subtype = AUDIO_TYPE_QUERY;

  ret = ioctl(fd, AUDIOIOC_GETCAPS, (unsigned long)&caps);
  assert_int_equal(ret, caps.ac_len);
  assert_true((caps.ac_controls.b[0] & direction) != 0);
  assert_true((caps.ac_format.hw & (1 << (AUDIO_FMT_PCM - 1))) != 0);

  memset(&caps, 0, sizeof(caps));
  caps.ac_len = sizeof(caps);
  caps.ac_type = direction;
  caps.ac_subtype = AUDIO_TYPE_QUERY;

  ret = ioctl(fd, AUDIOIOC_GETCAPS, (unsigned long)&caps);
  assert_int_equal(ret, caps.ac_len);
  assert_true(caps.ac_channels > 0);
}

static void drivertest_audio(void **audio_state)
{
  FAR struct audio_state_s *state;

  state = (struct audio_state_s *)*audio_state;

  if (state->direction & AUDIO_TYPE_INPUT)
    {
      audio_test_query_caps(state->indev_fd, AUDIO_TYPE_INPUT);
    }

  if (state->direction & AUDIO_TYPE_OUTPUT)
    {
      audio_test_query_caps(state->outdev_fd, AUDIO_TYPE_OUTPUT);
    }
}
#else
static void drivertest_audio(void **audio_state)
{
  FAR struct audio_state_s *state;
  FAR struct ap_buffer_s **bufs = NULL;
  struct ap_buffer_info_s buf_info;
  struct audio_msg_s msg;
  struct timeval start;
  struct timeval now;
  struct timeval elapsed;
  int directions[3] =
    {
      AUDIO_TYPE_INPUT,
      AUDIO_TYPE_OUTPUT,
      -1
    };

  struct mq_attr attr;
  int unconsumed = 0;
  unsigned int prio;
  bool streaming;
  bool running;
  int ret = 0;
  int direct;
  int enqueued;
  int x = 0;
  int i;

  state = (struct audio_state_s *)*audio_state;

  while (directions[x] != -1)
    {
      /* first round, test capture.
       * second round, test playback.
       * */

      streaming = running = true;
      if (!(directions[x] & state->direction))
        {
          x++;
          continue;
        }

      direct = directions[x];
      enqueued = 0;
      state->idx = 0;
      state->nr_x1 = 0;
      state->nr_x2 = 0;
      state->nr_y1 = 0;
      state->nr_y2 = 0;

      if (direct == AUDIO_TYPE_INPUT)
        {
          audio_test_prompt_tone(state);
        }

      ret = audio_test_prepare(state, &buf_info, direct);
      if (ret < 0)
        {
          syslog(LOG_ERR,
                 "audio_test_prepare failed direct=%d ret=%d errno=%d file=%s indev=%s outdev=%s\n",
                 direct, ret, errno, state->outfile, state->indev,
                 state->outdev);
        }

      assert_false(ret < 0);

      bufs = (FAR struct ap_buffer_s **)
              calloc(buf_info.nbuffers, sizeof(FAR void *));
      assert_true(bufs != NULL);

      ret = audio_test_alloc_buffer(state, bufs, &buf_info, direct);
      if (ret < 0)
        {
          free(bufs);
          bufs = NULL;
        }

      assert_false(ret < 0);

      for (i = 0; i < buf_info.nbuffers; i++)
        {
          if (direct == AUDIO_TYPE_OUTPUT)
              ret = audio_test_readbuffer(state, bufs[i]);

          if (ret < 0)
            {
              streaming = false;
            }
          else
            {
              ret = audio_test_enqueuebuffer(state, bufs[i], direct);
              assert_false(ret);
              if (ret == OK)
                {
                  enqueued++;
                }
            }
        }

      if (running)
        {
          ret = audio_test_start(state, direct);
          assert_false(ret < 0);
          gettimeofday(&start, NULL);
        }

      printf("Start %s. \n", direct == AUDIO_TYPE_OUTPUT ?
                             "Playback" : "Capture");

      unconsumed = enqueued;

      while (running)
        {
          ret = audio_test_receive_msg(state->mq, &msg, &prio);
          if (ret != sizeof(msg))
            {
              if (audio_test_timeout(state, direct, start))
                {
                  ret = audio_test_stop(state, direct);
                  assert_false(ret < 0);
                  running = false;
                }

              continue;
            }

          switch (msg.msg_id)
            {
              case AUDIO_MSG_DEQUEUE:
                unconsumed--;
                if (streaming)
                  {
                    if (direct == AUDIO_TYPE_INPUT)
                      {
                        ret = audio_test_writebuffer(state, msg.u.ptr);
                      }
                    else
                      {
                        ret = audio_test_readbuffer(state, msg.u.ptr);
                      }

                    if (ret != OK)
                      {
                        streaming = false;
                      }
                    else
                      {
                        ret = audio_test_enqueuebuffer(state,
                                                       msg.u.ptr, direct);
                        if (ret != OK)
                          {
                            close(state->out_fd);
                            state->out_fd = -1;
                            streaming = false;
                          }
                        else
                          {
                            unconsumed++;
                          }
                      }
                  }

                if (direct == AUDIO_TYPE_INPUT &&
                    audio_test_timeout(state, direct, start))
                  {
                    streaming = false;
                  }
                break;

                case AUDIO_MSG_COMPLETE:
                  running = false;
                  break;

                default:
                  break;
            }

          /* Capture stopped */

              if (audio_test_timeout(state, direct, start) || !streaming)
                {
                  ret = audio_test_stop(state, direct);
                  assert_false(ret < 0);
                  running = false;
                  unconsumed = 0;
                }
            }

      do
        {
          ret = mq_getattr(state->mq, &attr);
          assert_false(ret < 0);

          if (attr.mq_curmsgs == 0)
            {
              break;
            }

          ret = audio_test_receive_msg(state->mq, &msg, &prio);
          if (ret != sizeof(msg))
            {
              break;
            }

          if (msg.msg_id == AUDIO_MSG_DEQUEUE)
            {
              unconsumed--;
            }
        }
      while (ret >= 0);

      if (direct == AUDIO_TYPE_INPUT)
        {
          gettimeofday(&now, NULL);
          timersub(&now, &start, &elapsed);
          syslog(LOG_INFO, "Capture elapsed %ld.%06ld sec target=%" PRIu32 "\n",
                 (long)elapsed.tv_sec, (long)elapsed.tv_usec,
                 state->duration);
          assert_true(elapsed.tv_sec >= state->duration);
          if (state->direction & AUDIO_TYPE_OUTPUT)
            {
              syslog(LOG_INFO, "Capture cached %" PRIu32
                     " bytes for playback\n", state->idx);
            }
          else
            {
              ret = audio_test_capture_flush(state);
              assert_false(ret < 0);
              syslog(LOG_INFO, "Capture wrote %" PRIu32 " bytes to %s\n",
                     state->idx, state->outfile);
            }

          assert_true(state->idx > 0);
        }

      ret = audio_test_free_buffer(state, bufs, &buf_info, direct);
      assert_false(ret < 0);

      ret = audio_test_cleanup(state, direct);
      assert_false(ret < 0);

      memset(&buf_info, 0, sizeof(buf_info));
      free(bufs);
      bufs = NULL;

      x++;
    }

  return;
}
#endif

static int audio_test_setup(FAR void **audio_state)
{
  FAR struct audio_state_s *state;
  struct ap_buffer_info_s buf_info;
  struct mq_attr attr;
  int maxmsg = 0;
  int ret = 0;

  state = (struct audio_state_s *)*audio_state;

  if (state->direction & AUDIO_TYPE_OUTPUT)
    {
      state->outdev_fd = open(state->outdev, O_RDWR | O_CLOEXEC);
      assert_false(state->outdev_fd < 0);

      ret = ioctl(state->outdev_fd, AUDIOIOC_GETBUFFERINFO,
                  (unsigned long)&buf_info);
      maxmsg = MAX(maxmsg,
                   ret >= 0 ? buf_info.nbuffers : CONFIG_AUDIO_NUM_BUFFERS);
    }

  if (state->direction & AUDIO_TYPE_INPUT)
    {
      state->indev_fd = open(state->indev, O_RDWR | O_CLOEXEC);
      assert_false(state->indev_fd < 0);

      ret = ioctl(state->indev_fd, AUDIOIOC_GETBUFFERINFO,
                  (unsigned long)&buf_info);
      maxmsg = MAX(maxmsg,
                   ret >= 0 ? buf_info.nbuffers : CONFIG_AUDIO_NUM_BUFFERS);
    }

  attr.mq_maxmsg  = maxmsg + 8;
  attr.mq_msgsize = sizeof(struct audio_msg_s);
  attr.mq_curmsgs = 0;
  attr.mq_flags   = 0;

  snprintf(state->mqname, sizeof(state->mqname), "/tmp/%p",
           state);

  state->mq = mq_open(state->mqname, O_RDWR | O_CREAT, 0644, &attr);
  assert_false(state->mq < 0);

  return OK;
}

static int audio_test_teardown(FAR void **audio_state)
{
  FAR struct audio_state_s *state;
  state = (struct audio_state_s *)*audio_state;

  if (state->outdev_fd >= 0)
    {
      close(state->outdev_fd);
      state->outdev_fd = -1;
    }

  if (state->indev_fd >= 0)
    {
      close(state->indev_fd);
      state->indev_fd = -1;
    }

  mq_close(state->mq);
  mq_unlink(state->mqname);

  return 0;
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: loopback_test_main
 ****************************************************************************/

int main(int argc, FAR char *argv[])
{
  struct audio_state_s state = {
    .outdev    = "/dev/audio/pcm0p",
    .indev     = "/dev/audio/pcm0c",
    .duration  = 10,
    .chans     = 1,
    .bpsamp    = 16,
    .samprate  = 44100,
    .format    = AUDIO_FMT_PCM,
    .chmap     = 0,
    .outdev_fd = -1,
    .indev_fd  = -1,
    .out_fd    = -1,
  };

  parse_commandline(&state, argc, argv);

  const struct CMUnitTest tests[] =
    {
      cmocka_unit_test_prestate_setup_teardown(drivertest_audio,
                                               audio_test_setup,
                                               audio_test_teardown,
                                               &state),
    };

  return cmocka_run_group_tests(tests, NULL, NULL);
}
