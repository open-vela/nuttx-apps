/****************************************************************************
 * apps/system/i2c/i2c_gt911.c
 *
 * SPDX-License-Identifier: Apache-2.0
 ****************************************************************************/

#include <nuttx/config.h>

#include <errno.h>
#include <stdlib.h>
#include <unistd.h>

#include <nuttx/i2c/i2c_master.h>

#include "i2ctool.h"

#define GT911_I2C_BUS       0
#define GT911_I2C_ADDR      0x5d
#define GT911_ID_REG        0x8140
#define GT911_STATUS_REG    0x814e
#define GT911_POINT1_REG    0x8150

static int gt911_read(int fd, uint16_t reg, FAR uint8_t *data,
                      size_t length)
{
  uint8_t regaddr[2] =
    {
      (uint8_t)(reg >> 8),
      (uint8_t)(reg & 0xff)
    };
  struct i2c_msg_s msg[2] =
    {
      {
        .frequency = I2C_SPEED_STANDARD,
        .addr      = GT911_I2C_ADDR,
        .flags     = I2C_M_NOSTOP,
        .buffer    = regaddr,
        .length    = sizeof(regaddr)
      },
      {
        .frequency = I2C_SPEED_STANDARD,
        .addr      = GT911_I2C_ADDR,
        .flags     = I2C_M_READ,
        .buffer    = data,
        .length    = length
      }
    };

  return i2cdev_transfer(fd, msg, 2);
}

static int gt911_clear_status(int fd)
{
  uint8_t data[3] =
    {
      (uint8_t)(GT911_STATUS_REG >> 8),
      (uint8_t)(GT911_STATUS_REG & 0xff),
      0
    };
  struct i2c_msg_s msg =
    {
      .frequency = I2C_SPEED_STANDARD,
      .addr      = GT911_I2C_ADDR,
      .flags     = 0,
      .buffer    = data,
      .length    = sizeof(data)
    };

  return i2cdev_transfer(fd, &msg, 1);
}

int i2ccmd_gt911(FAR struct i2ctool_s *i2ctool, int argc, FAR char **argv)
{
  uint8_t id[4];
  uint8_t status;
  uint8_t point[6];
  long samples = 1;
  long delay_ms = 100;
  int fd;
  int ret = OK;
  int i;

  UNUSED(i2ctool);

  if (argc > 1)
    {
      samples = strtol(argv[1], NULL, 10);
    }

  if (argc > 2)
    {
      delay_ms = strtol(argv[2], NULL, 10);
    }

  if (argc > 3 || samples < 1 || samples > 1000 || delay_ms < 0 ||
      delay_ms > 10000)
    {
      printf("Usage: i2c gt911 [samples 1..1000] [delay_ms 0..10000]\n");
      return -EINVAL;
    }

  fd = i2cdev_open(GT911_I2C_BUS);
  if (fd < 0)
    {
      printf("GT911: open /dev/i2c%d failed: %d\n", GT911_I2C_BUS, errno);
      return -errno;
    }

  printf("GT911 polling check: bus=%d addr=0x%02x samples=%ld\n",
         GT911_I2C_BUS, GT911_I2C_ADDR, samples);

  for (i = 0; i < samples; i++)
    {
      ret = gt911_read(fd, GT911_ID_REG, id, sizeof(id));
      if (ret < 0)
        {
          printf("GT911 ID read failed: %d\n", ret);
          break;
        }

      ret = gt911_read(fd, GT911_STATUS_REG, &status, sizeof(status));
      if (ret < 0)
        {
          printf("GT911 status read failed: %d\n", ret);
          break;
        }

      printf("[%d] ID=%02x %02x %02x %02x status=0x%02x ready=%d points=%d",
             i, id[0], id[1], id[2], id[3], status,
             (status & 0x80) != 0, status & 0x0f);

      if ((status & 0x80) != 0 && (status & 0x0f) != 0)
        {
          ret = gt911_read(fd, GT911_POINT1_REG, point, sizeof(point));
          if (ret < 0)
            {
              printf(" point read failed: %d\n", ret);
              break;
            }

          printf(" x=%u y=%u size=%u",
                 (unsigned int)point[0] | ((unsigned int)point[1] << 8),
                 (unsigned int)point[2] | ((unsigned int)point[3] << 8),
                 (unsigned int)point[4] | ((unsigned int)point[5] << 8));
        }

      printf("\n");

      /* GT911 requires the host to clear the status register after each
       * sample.  This is the normal I2C data handshake, not GPIO IRQ setup. */

      ret = gt911_clear_status(fd);
      if (ret < 0)
        {
          printf(" status clear failed: %d\n", ret);
          break;
        }

      if (i + 1 < samples && delay_ms > 0)
        {
          usleep((unsigned int)delay_ms * 1000);
        }
    }

  close(fd);
  return ret;
}
