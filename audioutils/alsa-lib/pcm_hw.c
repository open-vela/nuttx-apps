/****************************************************************************
 * apps/audioutils/alsa-lib/pcm_hw.c
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

#include <fcntl.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/param.h>
#include <unistd.h>

#include "pcm_local.h"

/****************************************************************************
 * Private Types
 ****************************************************************************/

typedef struct
{
  int fd;
  bool setup;
  bool xrun;
  snd_pcm_uframes_t offset;
  FAR struct audio_status_s *status;
  FAR snd_pcm_channel_area_t **areas;
} snd_pcm_hw_t;

/****************************************************************************
 * Private Functions
 ****************************************************************************/

static void snd_pcm_hw_deinit(FAR snd_pcm_t *pcm)
{
  FAR snd_pcm_hw_t *hw = pcm->private_data;

  munmap(hw->status, sizeof(struct audio_status_s));
  ioctl(hw->fd, AUDIOIOC_RELEASE, 0);
  close(hw->fd);

  hw->status = MAP_FAILED;
  hw->fd = -1;
}

static int snd_pcm_hw_init(FAR snd_pcm_t *pcm)
{
  FAR snd_pcm_hw_t *hw = pcm->private_data;
  char path[64];
  int ret;

  /* Open device */

  snprintf(path, sizeof(path), CONFIG_AUDIOUTILS_ALSA_LIB_DEV_PATH "/%s",
           pcm->name);
  hw->fd = open(path, O_RDWR | O_CLOEXEC);
  if (hw->fd < 0)
    {
      return -errno;
    }

  /* Reserve */

  ret = ioctl(hw->fd, AUDIOIOC_RESERVE, 0);
  if (ret < 0)
    {
      ret = -errno;
      goto out;
    }

  hw->status = mmap(0, sizeof(struct audio_status_s),
                    PROT_READ, MAP_SHARED, hw->fd, 0);
  if (hw->status == MAP_FAILED)
    {
      ret = -errno;
      goto out_release;
    }

  return 0;

out_release:
  ioctl(hw->fd, AUDIOIOC_RELEASE, 0);
out:
  close(hw->fd);
  return ret;
}

static snd_pcm_state_t snd_pcm_hw_state(FAR snd_pcm_t *pcm)
{
  FAR snd_pcm_hw_t *hw = pcm->private_data;

  if (hw->xrun)
    {
      return SND_PCM_STATE_XRUN;
    }
  else if (hw->setup && hw->status->state == AUDIO_STATE_OPEN)
    {
      return SND_PCM_STATE_SETUP;
    }
  else if (pcm->appl < hw->status->head ||
           (hw->status->state == AUDIO_STATE_RUNNING &&
            pcm->appl <= hw->status->tail))
    {
      hw->xrun = true;
      return SND_PCM_STATE_XRUN;
    }

  return hw->status->state;
}

static int snd_pcm_hw_close(FAR snd_pcm_t *pcm)
{
  FAR snd_pcm_hw_t *hw = pcm->private_data;
  struct audio_buf_desc_s buf_desc;
  unsigned int i;

  switch (hw->status->state)
    {
    case SND_PCM_STATE_PAUSED:
    case SND_PCM_STATE_RUNNING:
      ioctl(hw->fd, AUDIOIOC_STOP, 0);
    default:
      break;
    }

  buf_desc.u.buffer = NULL;

  if (hw->status->state == AUDIO_STATE_DRAINING ||
      hw->status->state == AUDIO_STATE_OPEN)
    {
      while (pcm->appl > hw->status->tail)
        {
          snd_pcm_wait(pcm, -1);
        }

      for (i = 0; i < pcm->periods; i++)
        {
          ioctl(hw->fd, AUDIOIOC_FREEBUFFER, &buf_desc);
        }
    }

  snd_pcm_hw_deinit(pcm);
  free(hw);

  return 0;
}

static int snd_pcm_hw_query_format(int fd, FAR unsigned int *values,
                                   size_t num)
{
  struct audio_caps_s caps;
  size_t i;

  caps.ac_len = sizeof(struct audio_caps_s);
  caps.ac_type = AUDIO_TYPE_QUERY;
  caps.ac_subtype = AUDIO_TYPE_QUERY;
  if (ioctl(fd, AUDIOIOC_GETCAPS, (unsigned long)&caps) < 0)
    {
      return -errno;
    }

  if ((caps.ac_format.hw & (1 << (AUDIO_FMT_PCM - 1))) == 0)
    {
      return -EPERM;
    }

  caps.ac_subtype = AUDIO_FMT_PCM;
  if (ioctl(fd, AUDIOIOC_GETCAPS, (unsigned long)&caps) < 0)
    {
      return -errno;
    }

  for (i = 0; i < sizeof(caps.ac_controls.b); i++)
    {
      if (caps.ac_controls.b[i] == AUDIO_SUBFMT_END)
        {
          break;
        }

      values[i] = caps.ac_controls.b[i];
    }

  return i == 0 ? -EPERM : i;
}

static int snd_pcm_hw_query_channel(int fd, snd_pcm_stream_t stream,
                                    FAR unsigned int *values, size_t num)
{
  struct audio_caps_s caps;

  caps.ac_len = sizeof(struct audio_caps_s);
  caps.ac_type = stream == SND_PCM_STREAM_PLAYBACK ? AUDIO_TYPE_OUTPUT
                                                   : AUDIO_TYPE_INPUT;
  caps.ac_subtype = AUDIO_TYPE_QUERY;
  if (ioctl(fd, AUDIOIOC_GETCAPS, (unsigned long)&caps) < 0)
    {
      return -errno;
    }

  if ((caps.ac_channels & 0xf0) == 0)
    {
      values[0] = 1;
      values[1] = caps.ac_channels;
    }
  else
    {
      values[0] = caps.ac_channels >> 4;
      values[1] = caps.ac_channels & 0x0f;
    }

  return 2;
}

static int snd_pcm_hw_query_rate(int fd, snd_pcm_stream_t stream,
                                 FAR unsigned int *values, size_t num)
{
  struct audio_caps_s caps;
  size_t i;

  caps.ac_len = sizeof(struct audio_caps_s);
  caps.ac_type = stream == SND_PCM_STREAM_PLAYBACK ? AUDIO_TYPE_OUTPUT
                                                   : AUDIO_TYPE_INPUT;
  caps.ac_subtype = AUDIO_TYPE_QUERY;
  if (ioctl(fd, AUDIOIOC_GETCAPS, (unsigned long)&caps) < 0)
    {
      return -errno;
    }

  for (i = 0; i < num && caps.ac_controls.hw[0]; i++)
    {
      if (caps.ac_controls.hw[0] & AUDIO_SAMP_RATE_8K)
        {
          caps.ac_controls.hw[0] &= ~AUDIO_SAMP_RATE_8K;
          values[i] = 8000;
        }
      else if (caps.ac_controls.hw[0] & AUDIO_SAMP_RATE_11K)
        {
          caps.ac_controls.hw[0] &= ~AUDIO_SAMP_RATE_11K;
          values[i] = 11025;
        }
      else if (caps.ac_controls.hw[0] & AUDIO_SAMP_RATE_12K)
        {
          caps.ac_controls.hw[0] &= ~AUDIO_SAMP_RATE_12K;
          values[i] = 12000;
        }
      else if (caps.ac_controls.hw[0] & AUDIO_SAMP_RATE_16K)
        {
          caps.ac_controls.hw[0] &= ~AUDIO_SAMP_RATE_16K;
          values[i] = 16000;
        }
      else if (caps.ac_controls.hw[0] & AUDIO_SAMP_RATE_22K)
        {
          caps.ac_controls.hw[0] &= ~AUDIO_SAMP_RATE_22K;
          values[i] = 22050;
        }
      else if (caps.ac_controls.hw[0] & AUDIO_SAMP_RATE_24K)
        {
          caps.ac_controls.hw[0] &= ~AUDIO_SAMP_RATE_24K;
          values[i] = 24000;
        }
      else if (caps.ac_controls.hw[0] & AUDIO_SAMP_RATE_32K)
        {
          caps.ac_controls.hw[0] &= ~AUDIO_SAMP_RATE_32K;
          values[i] = 32000;
        }
      else if (caps.ac_controls.hw[0] & AUDIO_SAMP_RATE_44K)
        {
          caps.ac_controls.hw[0] &= ~AUDIO_SAMP_RATE_44K;
          values[i] = 44100;
        }
      else if (caps.ac_controls.hw[0] & AUDIO_SAMP_RATE_48K)
        {
          caps.ac_controls.hw[0] &= ~AUDIO_SAMP_RATE_48K;
          values[i] = 48000;
        }
      else if (caps.ac_controls.hw[0] & AUDIO_SAMP_RATE_96K)
        {
          caps.ac_controls.hw[0] &= ~AUDIO_SAMP_RATE_96K;
          values[i] = 96000;
        }
      else if (caps.ac_controls.hw[0] & AUDIO_SAMP_RATE_128K)
        {
          caps.ac_controls.hw[0] &= ~AUDIO_SAMP_RATE_128K;
          values[i] = 128000;
        }
      else if (caps.ac_controls.hw[0] & AUDIO_SAMP_RATE_160K)
        {
          caps.ac_controls.hw[0] &= ~AUDIO_SAMP_RATE_160K;
          values[i] = 160000;
        }
      else if (caps.ac_controls.hw[0] & AUDIO_SAMP_RATE_172K)
        {
          caps.ac_controls.hw[0] &= ~AUDIO_SAMP_RATE_172K;
          values[i] = 172000;
        }
      else if (caps.ac_controls.hw[0] & AUDIO_SAMP_RATE_192K)
        {
          caps.ac_controls.hw[0] &= ~AUDIO_SAMP_RATE_192K;
          values[i] = 192000;
        }
      else
        {
          break;
        }
    }

  return i;
}

static int snd_pcm_hw_hw_refine(FAR snd_pcm_t *pcm,
                                FAR snd_pcm_hw_params_t *params)
{
  FAR snd_pcm_hw_t *hw = pcm->private_data;
  snd_pcm_format_mask_t format_mask;
  snd_pcm_format_t format;
  unsigned int value[32];
  unsigned int channels;
  unsigned int rate;
  bool changed;
  int ret;
  int i;

  snd_pcm_hw_params_get_format_mask(params, &format_mask);
  ret = snd_pcm_hw_query_format(hw->fd, value, 32);
  if (ret < 0)
    {
      return ret;
    }

  if (format_mask)
    {
      changed = true;
      for (i = 0; i < ret; i++)
        {
          if (snd_pcm_format_mask_test(&format_mask, value[i]))
            {
              changed = false;
              break;
            }
        }

      if (changed)
        {
          format = value[ret - 1];
          snd_pcm_hw_params_set_format(pcm, params, format);
        }
    }
  else
    {
      for (i = 0; i < ret; i++)
        {
          snd_pcm_format_mask_set(&format_mask, value[i]);
        }

      snd_pcm_hw_params_set_format_mask(pcm, params, &format_mask);
    }

  ret = snd_pcm_hw_params_get_channels_min(params, &channels);
  if (ret < 0)
    {
      return ret;
    }

  ret = snd_pcm_hw_query_channel(hw->fd, pcm->stream, value, 32);
  if (ret < 0)
    {
      return ret;
    }

  if (channels)
    {
      if (channels < value[0] || channels > value[1])
        {
          channels = value[1];
          snd_pcm_hw_params_set_channels(pcm, params, channels);
        }
    }
  else
    {
      snd_pcm_hw_params_set_channels_min(pcm, params, &value[0]);
      snd_pcm_hw_params_set_channels_max(pcm, params, &value[1]);
    }

  ret = snd_pcm_hw_params_get_rate_min(params, &rate, 0);
  if (ret < 0)
    {
      return ret;
    }

  ret = snd_pcm_hw_query_rate(hw->fd, pcm->stream, value, 32);
  if (ret < 0)
    {
      return ret;
    }

  if (rate)
    {
      changed = true;
      for (i = 0; i < ret; i++)
        {
          if (value[i] == rate)
            {
              changed = false;
              break;
            }
        }

      if (changed)
        {
          rate = value[ret - 1];
          snd_pcm_hw_params_set_rate(pcm, params, rate, 0);
        }
    }
  else
    {
      snd_pcm_hw_params_set_rate_min(pcm, params, &value[0], NULL);
      snd_pcm_hw_params_set_rate_max(pcm, params, &value[MAX(ret - 1, 0)],
                                     NULL);
    }

  return 0;
}

static int snd_pcm_hw_hw_params(FAR snd_pcm_t *pcm,
                                FAR snd_pcm_hw_params_t *params)
{
  FAR snd_pcm_hw_t *hw = pcm->private_data;
  struct audio_caps_desc_s caps_desc;
  struct audio_buf_desc_s buf_desc;
  struct ap_buffer_info_s buf_info;
  snd_pcm_uframes_t period_size;
  unsigned int period_bytes;
  struct audio_info_s info;
  unsigned int period_time;
  snd_pcm_format_t format;
  unsigned int channels;
  unsigned int periods;
  unsigned int rate;
  unsigned int i;
  int ret;

  snd_pcm_hw_params_get_format(params, &format);
  snd_pcm_hw_params_get_channels(params, &channels);
  snd_pcm_hw_params_get_rate(params, &rate, NULL);
  snd_pcm_hw_params_get_periods(params, &periods, NULL);
  snd_pcm_hw_params_get_period_size(params, &period_size, NULL);

  memset(&caps_desc, 0, sizeof(caps_desc));
  caps_desc.caps.ac_len = sizeof(struct audio_caps_s);
  caps_desc.caps.ac_type = pcm->stream == SND_PCM_STREAM_PLAYBACK ?
                           AUDIO_TYPE_OUTPUT : AUDIO_TYPE_INPUT;
  caps_desc.caps.ac_channels = channels;
  caps_desc.caps.ac_controls.hw[0] = rate;
  caps_desc.caps.ac_controls.b[3] = rate >> 16;
  caps_desc.caps.ac_controls.b[2] = snd_pcm_format_physical_width(format);
  caps_desc.caps.ac_format.b[0] = format;
  caps_desc.caps.ac_subtype = AUDIO_FMT_PCM;

  ret = ioctl(hw->fd, AUDIOIOC_CONFIGURE, &caps_desc);
  if (ret < 0)
    {
      return -errno;
    }

  SNDINFO("configure. f:%u, ch:%u, rate:%u", format, channels, rate);

  period_bytes = snd_pcm_format_size(format, period_size * channels);
  buf_info.nbuffers = periods;
  buf_info.buffer_size = period_bytes;
  ioctl(hw->fd, AUDIOIOC_SETBUFFERINFO, &buf_info);

  ret = ioctl(hw->fd, AUDIOIOC_GETAUDIOINFO, &info);
  if (ret < 0)
    {
      return -errno;
    }

  snd_pcm_hw_params_set_format(pcm, params, info.subformat);
  snd_pcm_hw_params_set_channels(pcm, params, info.channels);
  snd_pcm_hw_params_set_rate(pcm, params, info.samplerate, 0);

  ret = ioctl(hw->fd, AUDIOIOC_GETBUFFERINFO, &buf_info);
  if (ret < 0)
    {
      return ret;
    }

  if (buf_info.nbuffers != periods || buf_info.buffer_size != period_bytes)
    {
      periods = buf_info.nbuffers;
      period_size = 8 * buf_info.buffer_size /
                    (snd_pcm_format_physical_width(format) * info.channels);
      period_time = period_size * 1000000 / rate;
      snd_pcm_hw_params_set_periods(pcm, params, periods, 0);
      snd_pcm_hw_params_set_period_size(pcm, params, period_size, 0);
      snd_pcm_hw_params_set_period_time(pcm, params, period_time, 0);
      snd_pcm_hw_params_set_buffer_size(pcm, params, period_size * periods);
      snd_pcm_hw_params_set_buffer_time(pcm, params, period_time * periods,
                                        0);
    }

  pcm->start_threshold = period_size * periods;
  SNDINFO("config buffer info. n:%u, size:%u", buf_info.nbuffers,
          buf_info.buffer_size);

  for (i = 0; i < buf_info.nbuffers; i++)
    {
      buf_desc.numbytes = buf_info.buffer_size;
      buf_desc.u.pbuffer = NULL;

      ret = ioctl(hw->fd, AUDIOIOC_ALLOCBUFFER, &buf_desc);
      if (ret < 0)
        {
          return -errno;
        }

      if (pcm->stream == SND_PCM_STREAM_CAPTURE)
        {
          ret = ioctl(hw->fd, AUDIOIOC_ENQUEUEBUFFER, &buf_desc);
          if (ret < 0)
            {
              return -errno;
            }

          pcm->appl++;
        }
    }

  hw->setup = true;

  return 0;
}

static int snd_pcm_hw_munmap(FAR snd_pcm_t *pcm)
{
  FAR snd_pcm_hw_t *hw = pcm->private_data;
  unsigned int i;
  FAR void *addr;
  ssize_t len;

  if (hw->areas == NULL)
    {
      return 0;
    }

  len = snd_pcm_frames_to_bytes(pcm, pcm->period_size);
  if (len < 0)
    {
      return len;
    }

  for (i = 0; i < pcm->periods; i++)
    {
      if (hw->areas[i] == NULL)
        {
          continue;
        }

      addr = hw->areas[i][0].addr;
      if (addr == NULL || addr == MAP_FAILED)
        {
          continue;
        }

      if (munmap(addr, len) < 0)
        {
          SNDERR("munmap failed addr: %p len: %zu\n", addr, len);
        }
    }

  free(hw->areas);
  hw->areas = NULL;

  return 0;
}

static int snd_pcm_hw_mmap(FAR snd_pcm_t *pcm)
{
  FAR snd_pcm_hw_t *hw = pcm->private_data;
  size_t areas_size;
  size_t ptr_size;
  unsigned int ch;
  unsigned int i;
  FAR void *addr;
  ssize_t len;
  int ret;

  ptr_size = pcm->periods * sizeof(hw->areas[0]);
  areas_size = pcm->channels * sizeof(hw->areas[0][0]);
  len = snd_pcm_frames_to_bytes(pcm, pcm->period_size);
  if (len < 0)
    {
      return len;
    }

  hw->areas = calloc(1, ptr_size + pcm->periods * areas_size);
  if (!hw->areas)
    {
      return -ENOMEM;
    }

  for (i = 0; i < pcm->periods; i++)
    {
      hw->areas[i] =
          (FAR snd_pcm_channel_area_t *)((FAR uint8_t *)hw->areas +
                                         ptr_size + i * areas_size);

      addr = mmap(0, len, PROT_READ | PROT_WRITE, MAP_SHARED, hw->fd,
                  i * len);
      if (addr == MAP_FAILED)
        {
          ret = -errno;
          SNDERR("mmap failed: %d\n", ret);
          snd_pcm_hw_munmap(pcm);

          return ret;
        }

      for (ch = 0; ch < pcm->channels; ch++)
        {
          hw->areas[i][ch].addr = addr;
          hw->areas[i][ch].first = ch * pcm->sample_bits;
          hw->areas[i][ch].step = pcm->frame_bits;
        }
    }

  return 0;
}

static int snd_pcm_hw_prepare(FAR snd_pcm_t *pcm)
{
  FAR snd_pcm_hw_t *hw = pcm->private_data;
  int ret = 0;

  if (hw->xrun)
    {
      ret = snd_pcm_reset(pcm);
      hw->xrun = false;
    }

  return ret;
}

static int snd_pcm_hw_reset(FAR snd_pcm_t *pcm)
{
  FAR snd_pcm_hw_t *hw = pcm->private_data;

  SNDINFO("device:%s head:%ld tail:%ld\n", pcm->name, hw->status->head,
          hw->status->tail);

  if (ioctl(hw->fd, AUDIOIOC_RESETSTATUS, 0) < 0)
    {
      SNDERR("reset error:%d\n", -errno);
      return -errno;
    }

  hw->setup = false;
  pcm->appl = hw->status->head;
  SNDINFO("reset appl:%ld", pcm->appl);

  return 0;
}

static int snd_pcm_hw_start(FAR snd_pcm_t *pcm)
{
  FAR snd_pcm_hw_t *hw = pcm->private_data;
  int ret;

  ret = ioctl(hw->fd, AUDIOIOC_START, 0);
  if (ret < 0)
    {
      SNDERR("start error:%d\n", ret);
      return -errno;
    }

  return ret;
}

static int snd_pcm_hw_drop(FAR snd_pcm_t *pcm)
{
  return -ENOTSUP;
}

static int snd_pcm_hw_drain(FAR snd_pcm_t *pcm)
{
  FAR snd_pcm_hw_t *hw = pcm->private_data;

  hw->setup = true;

  switch (snd_pcm_hw_state(pcm))
    {
    case SND_PCM_STATE_PREPARED:
      snd_pcm_hw_start(pcm);
      break;
    case SND_PCM_STATE_SETUP:
      return 0;
    default:
      break;
    }

  if (pcm->stream == SND_PCM_STREAM_PLAYBACK && hw->offset)
    {
      snd_pcm_mmap_commit(pcm, hw->offset, pcm->period_size - hw->offset);
    }

  ioctl(hw->fd, AUDIOIOC_STOP, 0);

  while (snd_pcm_hw_state(pcm) == SND_PCM_STATE_DRAINING)
    {
      if (pcm->mode & SND_PCM_NONBLOCK)
        {
          return snd_pcm_hw_state(pcm) == SND_PCM_STATE_SETUP ? 0 : -EAGAIN;
        }

      snd_pcm_wait(pcm, -1);
    }

  return 0;
}

static int snd_pcm_hw_pause(FAR snd_pcm_t *pcm, int enable)
{
  FAR snd_pcm_hw_t *hw = pcm->private_data;
  int ret;

  ret = ioctl(hw->fd, enable ? AUDIOIOC_PAUSE : AUDIOIOC_RESUME, 0);
  if (ret < 0)
    {
      SNDERR("pause:%d, ret:%d", enable, ret);
      return -errno;
    }

  return ret;
}

static int snd_pcm_hw_delay(FAR snd_pcm_t *pcm,
                            FAR snd_pcm_sframes_t *delayp)
{
  FAR snd_pcm_hw_t *hw = pcm->private_data;
  snd_pcm_sframes_t avail;
  int ret;

  *delayp = 0;
  ret = ioctl(hw->fd, AUDIOIOC_GETLATENCY, delayp);
  if (ret < 0)
    {
      return -errno;
    }

  avail = (pcm->appl - hw->status->head) * pcm->period_size;
  *delayp += avail > 0 ? avail : 0;
  *delayp += hw->offset;
  return 0;
}

static int snd_pcm_hw_resume(FAR snd_pcm_t *pcm)
{
  snd_pcm_hw_prepare(pcm);
  return 0;
}

static snd_pcm_sframes_t snd_pcm_hw_writen(FAR snd_pcm_t *pcm,
                                           FAR void **bufs,
                                           snd_pcm_uframes_t size)
{
  FAR const snd_pcm_channel_area_t *areas;
  FAR const uint8_t *src;
  FAR uint8_t *dst;
  FAR uint8_t *out;
  snd_pcm_uframes_t xfer = 0;
  snd_pcm_uframes_t offset;
  snd_pcm_uframes_t frames;
  unsigned int sample_bytes;
  unsigned int channel;
  snd_pcm_uframes_t frame;
  ssize_t len;
  int ret = 0;

  if (bufs == NULL || pcm->sample_bits == 0 ||
      (pcm->sample_bits & 7) != 0)
    {
      return -EINVAL;
    }

  sample_bytes = pcm->sample_bits / 8;

  for (channel = 0; channel < pcm->channels; channel++)
    {
      if (bufs[channel] == NULL)
        {
          return -EINVAL;
        }
    }

  while (size > 0)
    {
      frames = size;
      ret = snd_pcm_mmap_begin(pcm, &areas, &offset, &frames);
      if (ret < 0)
        {
          break;
        }

      if (frames == 0)
        {
          if (pcm->mode & SND_PCM_NONBLOCK)
            {
              ret = -EAGAIN;
              break;
            }

          ret = snd_pcm_wait(pcm, -1);
          if (ret < 0)
            {
              break;
            }

          continue;
        }

      for (channel = 0; channel < pcm->channels; channel++)
        {
          if (((areas[channel].first | areas[channel].step) & 7) != 0)
            {
              ret = -ENOTSUP;
              break;
            }

          src = (FAR const uint8_t *)bufs[channel] +
                xfer * sample_bytes;
          dst = (FAR uint8_t *)areas[channel].addr +
                areas[channel].first / 8 +
                offset * areas[channel].step / 8;

          for (frame = 0; frame < frames; frame++)
            {
              memcpy(dst, src, sample_bytes);
              src += sample_bytes;
              dst += areas[channel].step / 8;
            }
        }

      if (ret < 0)
        {
          break;
        }

      len = snd_pcm_frames_to_bytes(pcm, frames);
      if (len < 0)
        {
          ret = len;
          break;
        }

      out = (FAR uint8_t *)areas[0].addr + areas[0].first / 8 +
            offset * areas[0].step / 8;

      if (pcm->volume != 1.0)
        {
          snd_pcm_softvol_scale(pcm->format, pcm->volume, out,
                                snd_pcm_bytes_to_samples(pcm, len));
        }

      ret = snd_pcm_mmap_commit(pcm, offset, frames);
      if (ret < 0)
        {
          break;
        }

      size -= frames;
      xfer += frames;

      if (snd_pcm_state(pcm) == SND_PCM_STATE_PREPARED &&
          pcm->period_size * pcm->appl >= pcm->start_threshold)
        {
          ret = snd_pcm_start(pcm);
          if (ret < 0)
            {
              return ret;
            }
        }
    }

  return xfer > 0 ? xfer : ret;
}

static snd_pcm_sframes_t snd_pcm_hw_avail_update(FAR snd_pcm_t *pcm)
{
  FAR snd_pcm_hw_t *hw = pcm->private_data;
  unsigned long used;

  if (snd_pcm_hw_state(pcm) == SND_PCM_STATE_XRUN)
    {
      return -EPIPE;
    }

  used = pcm->appl - hw->status->tail;

  return pcm->period_size * (pcm->periods - used);
}

static int snd_pcm_hw_mmap_begin(FAR snd_pcm_t *pcm,
                                 FAR const snd_pcm_channel_area_t **areas,
                                 FAR snd_pcm_uframes_t *offset,
                                 FAR snd_pcm_uframes_t *frames)
{
  FAR snd_pcm_hw_t *hw = pcm->private_data;
  snd_pcm_sframes_t avail;

  avail = snd_pcm_hw_avail_update(pcm);
  if (avail < 0)
    {
      return avail;
    }

  avail = MIN(avail, pcm->period_size);

  if (avail > hw->offset)
    {
      avail -= hw->offset;
    }
  else
    {
      avail = 0;
    }

  *areas = hw->areas[pcm->appl % pcm->periods];
  *offset = hw->offset;
  *frames = MIN(avail, *frames);

  return 0;
}

static snd_pcm_sframes_t snd_pcm_hw_mmap_commit(FAR snd_pcm_t *pcm,
                                                snd_pcm_uframes_t offset,
                                                snd_pcm_uframes_t size)
{
  FAR snd_pcm_hw_t *hw = pcm->private_data;
  struct audio_buf_desc_s desc;
  int ret;

  hw->offset += size;
  if (hw->offset >= pcm->period_size)
    {
      desc.numbytes = snd_pcm_frames_to_bytes(pcm, pcm->period_size);
      desc.u.buffer = NULL;

      ret = ioctl(hw->fd, AUDIOIOC_ENQUEUEBUFFER, &desc);
      if (ret < 0)
        {
          hw->offset -= size;
          return -errno;
        }

      hw->offset = 0;
      pcm->appl++;
    }

  return 0;
}

static int snd_pcm_hw_poll_descriptors_count(FAR snd_pcm_t *pcm)
{
  return 1;
}

static int snd_pcm_hw_poll_descriptors(FAR snd_pcm_t *pcm,
                                       FAR struct pollfd *pfds,
                                       unsigned int space)
{
  FAR snd_pcm_hw_t *hw = pcm->private_data;

  pfds->fd = hw->fd;
  pfds->events = POLLIN | POLLOUT;

  if (hw->status->state == SND_PCM_STATE_DRAINING)
    {
      pfds->events = POLLERR;
    }

  return 1;
}

static const snd_pcm_ops_t g_snd_pcm_hw_ops =
{
    .close = snd_pcm_hw_close,
    .hw_refine = snd_pcm_hw_hw_refine,
    .hw_params = snd_pcm_hw_hw_params,
    .dump = NULL,
    .prepare = snd_pcm_hw_prepare,
    .reset = snd_pcm_hw_reset,
    .start = snd_pcm_hw_start,
    .drop = snd_pcm_hw_drop,
    .drain = snd_pcm_hw_drain,
    .pause = snd_pcm_hw_pause,
    .state = snd_pcm_hw_state,
    .delay = snd_pcm_hw_delay,
    .resume = snd_pcm_hw_resume,
    .writei = snd_pcm_mmap_writei,
    .writen = snd_pcm_hw_writen,
    .readi = snd_pcm_mmap_readi,
    .avail_update = snd_pcm_hw_avail_update,
    .mmap = snd_pcm_hw_mmap,
    .munmap = snd_pcm_hw_munmap,
    .mmap_begin = snd_pcm_hw_mmap_begin,
    .mmap_commit = snd_pcm_hw_mmap_commit,
    .poll_descriptors_count = snd_pcm_hw_poll_descriptors_count,
    .poll_descriptors = snd_pcm_hw_poll_descriptors,
    .poll_revents = NULL,
};

/****************************************************************************
 * Public Functions
 ****************************************************************************/

int snd_pcm_hw_open(FAR snd_pcm_t **pcmp, FAR const char *name,
                    snd_pcm_stream_t stream, int mode)
{
  FAR snd_pcm_hw_t *hw;
  FAR snd_pcm_t *pcm;
  int ret;

  if (!pcmp || !name)
    {
      return -EINVAL;
    }

  if (stream != SND_PCM_STREAM_PLAYBACK && stream != SND_PCM_STREAM_CAPTURE)
    {
      return -EINVAL;
    }

  hw = calloc(1, sizeof(snd_pcm_hw_t));
  if (!hw)
    {
      return -ENOMEM;
    }

  ret = snd_pcm_new(&pcm, SND_PCM_TYPE_HW, name, stream, mode);
  if (ret < 0)
    {
      free(hw);
      return ret;
    }

  pcm->ops = &g_snd_pcm_hw_ops;
  pcm->private_data = hw;

  ret = snd_pcm_hw_init(pcm);
  if (ret < 0)
    {
      free(hw);
      snd_pcm_free(pcm);
      return ret;
    }

  *pcmp = pcm;
  return ret;
}
