/****************************************************************************
 * apps/examples/ls_driver_test/test_sensor_oled.c
 *
 * BH1750 light sensor + SSD1306 OLED display
 * Ported from HarmonyOS loong_hat demo for NuttX.
 *
 * Hardware (LOONG-HAT):
 *   - I2C1 SCL: GPIO50, SDA: GPIO51
 *   - BH1750 addr: 0x23 (or 0x5C)
 *   - OLED   addr: 0x3C
 *
 ****************************************************************************/

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>

#include <sys/ioctl.h>
#include <stdio.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <stdint.h>

#include <nuttx/i2c/i2c_master.h>

#include "ls_driver_test.h"
#include "oled_fonts.h"

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#define I2C1_PATH           "/dev/i2c1"
#define I2C_FREQ            I2C_SPEED_STANDARD

/* BH1750 definitions */

#define BH1750_ADDR_LOW     0x23
#define BH1750_ADDR_HIGH    0x5c
#define BH1750_POWER_ON     0x01
#define BH1750_RESET        0x07
#define BH1750_H_MODE       0x10

/* OLED definitions */

#define OLED_ADDR           0x3c
#define OLED_WIDTH          128
#define OLED_PAGES          8
#define OLED_I2C_CMD        0x00
#define OLED_I2C_DATA       0x40

/* Display update interval (seconds) */

#define SENSOR_INTERVAL_SEC 2

/****************************************************************************
 * Private Types
 ****************************************************************************/

typedef enum
{
  FONT6X8 = 1,
  FONT8X16,
} oled_font_e;

/****************************************************************************
 * Private Data
 ****************************************************************************/

static int g_i2c_fd = -1;
static uint8_t g_bh1750_addr = BH1750_ADDR_LOW;

/****************************************************************************
 * Private Functions - I2C Helper
 ****************************************************************************/

static int sensor_i2c_write(uint8_t addr, const uint8_t *data, int len)
{
  struct i2c_msg_s msg;
  struct i2c_transfer_s trans;

  msg.frequency = I2C_FREQ;
  msg.addr      = addr;
  msg.flags     = 0;
  msg.buffer    = (FAR uint8_t *)data;
  msg.length    = len;

  trans.msgv = &msg;
  trans.msgc = 1;

  return ioctl(g_i2c_fd, I2CIOC_TRANSFER, (unsigned long)&trans);
}

static int sensor_i2c_read(uint8_t addr, uint8_t *data, int len)
{
  struct i2c_msg_s msg;
  struct i2c_transfer_s trans;

  msg.frequency = I2C_FREQ;
  msg.addr      = addr;
  msg.flags     = I2C_M_READ;
  msg.buffer    = data;
  msg.length    = len;

  trans.msgv = &msg;
  trans.msgc = 1;

  return ioctl(g_i2c_fd, I2CIOC_TRANSFER, (unsigned long)&trans);
}

/****************************************************************************
 * Private Functions - BH1750 Light Sensor
 ****************************************************************************/

static int bh1750_write_cmd(uint8_t cmd)
{
  return sensor_i2c_write(g_bh1750_addr, &cmd, 1);
}

static int bh1750_init(void)
{
  int ret;

  /* Try low address first */

  g_bh1750_addr = BH1750_ADDR_LOW;
  ret = bh1750_write_cmd(BH1750_POWER_ON);
  if (ret < 0)
    {
      printf("[BH1750] probe addr 0x%02x failed\n", BH1750_ADDR_LOW);
      g_bh1750_addr = BH1750_ADDR_HIGH;
      ret = bh1750_write_cmd(BH1750_POWER_ON);
    }

  if (ret < 0)
    {
      printf("[BH1750] probe addr 0x%02x failed\n", BH1750_ADDR_HIGH);
      printf("[BH1750] no ACK, check wiring\n");
      return ret;
    }

  printf("[BH1750] detected addr=0x%02x\n", g_bh1750_addr);
  usleep(20000);

  ret = bh1750_write_cmd(BH1750_RESET);
  if (ret < 0)
    {
      printf("[BH1750] reset failed\n");
      return ret;
    }
  usleep(20000);

  ret = bh1750_write_cmd(BH1750_H_MODE);
  if (ret < 0)
    {
      printf("[BH1750] set H-mode failed\n");
      return ret;
    }
  usleep(180000);

  printf("[BH1750] init OK\n");
  return OK;
}

static int bh1750_read_lux(unsigned int *lux_x100)
{
  uint8_t buf[2] =
  {
    0
  };

  int ret;
  unsigned int raw;

  ret = sensor_i2c_read(g_bh1750_addr, buf, 2);
  if (ret < 0)
    {
      printf("[BH1750] read failed: %d\n", ret);
      return ret;
    }

  raw = ((unsigned int)buf[0] << 8) | buf[1];
  *lux_x100 = (raw * 1000 + 6) / 12;

  return OK;
}

/****************************************************************************
 * Private Functions - OLED Display
 ****************************************************************************/

static int oled_write_byte(uint8_t reg, uint8_t data)
{
  uint8_t buf[2];

  buf[0] = reg;
  buf[1] = data;

  return sensor_i2c_write(OLED_ADDR, buf, 2);
}

static int oled_write_cmd(uint8_t cmd)
{
  return oled_write_byte(OLED_I2C_CMD, cmd);
}

static int oled_write_data(uint8_t data)
{
  return oled_write_byte(OLED_I2C_DATA, data);
}

static int oled_init_device(void)
{
  static const uint8_t init_cmds[] =
  {
    0xae, 0x00, 0x10, 0x40, 0xb0, 0x81, 0xff, 0xa1,
    0xa6, 0xa8, 0x3f, 0xc8, 0xd3, 0x00, 0xd5, 0x80,
    0xd8, 0x05, 0xd9, 0xf1, 0xda, 0x12, 0xdb, 0x30,
    0x8d, 0x14, 0xaf,
  };
  int i;

  for (i = 0; i < (int)(sizeof(init_cmds) / sizeof(init_cmds[0])); i++)
    {
      int ret = oled_write_cmd(init_cmds[i]);
      if (ret < 0)
        {
          printf("[OLED] init cmd 0x%02x failed: %d\n", init_cmds[i], ret);
          return ret;
        }
    }

  printf("[OLED] init OK\n");
  return OK;
}

static void oled_set_position(uint8_t x, uint8_t y)
{
  oled_write_cmd(0xb0 + y);
  oled_write_cmd(x & 0x0f);
  oled_write_cmd(((x & 0xf0) >> 4) | 0x10);
}

static void oled_fill_screen(uint8_t fill)
{
  uint8_t page;
  uint8_t col;

  for (page = 0; page < OLED_PAGES; page++)
    {
      oled_set_position(0, page);
      for (col = 0; col < OLED_WIDTH; col++)
        {
          oled_write_data(fill);
        }
    }
}

static void oled_show_char(uint8_t x, uint8_t y, char ch, oled_font_e font)
{
  uint8_t c;
  uint8_t i;

  if (ch < ' ' || ch > '~')
    {
      ch = ' ';
    }

  c = (uint8_t)(ch - ' ');

  if (font == FONT8X16)
    {
      oled_set_position(x, y);
      for (i = 0; i < 8; i++)
        {
          oled_write_data(F8X16[c * 16 + i]);
        }

      oled_set_position(x, y + 1);
      for (i = 0; i < 8; i++)
        {
          oled_write_data(F8X16[c * 16 + 8 + i]);
        }
    }
  else
    {
      oled_set_position(x, y);
      for (i = 0; i < 6; i++)
        {
          oled_write_data(g_font_6x8[c][i]);
        }
    }
}

static void oled_show_string(uint8_t x, uint8_t y,
                             const char *str, oled_font_e font)
{
  uint8_t step = (font == FONT8X16) ? 8 : 6;

  if (str == NULL)
    {
      return;
    }

  while (*str != '\0')
    {
      oled_show_char(x, y, *str, font);
      x += step;
      if (x > (OLED_WIDTH - step))
        {
          x = 0;
          y += (font == FONT8X16) ? 2 : 1;
          if (y >= OLED_PAGES)
            {
              break;
            }
        }

      str++;
    }
}

/****************************************************************************
 * Private Functions - Display Helpers
 ****************************************************************************/

static void format_lux(char *buf, size_t len, unsigned int lux_x100)
{
  snprintf(buf, len, "Lux: %u.%02u", lux_x100 / 100, lux_x100 % 100);
}

static void oled_show_light_data(unsigned int lux_x100)
{
  char line[22];

  format_lux(line, sizeof(line), lux_x100);

  oled_fill_screen(0x00);
  oled_show_string(16, 0, "Loong Hat", FONT8X16);
  oled_show_string(0, 3, line, FONT8X16);
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

int test_sensor_oled(void)
{
  unsigned int lux_x100 = 0;
  int bh1750_ok = 0;
  int oled_ok = 0;
  int ret;
  int loop_count = 0;

  printf("[SENSOR] === BH1750 + OLED Test ===\n");
  printf("[SENSOR] I2C1: SCL=GPIO50 SDA=GPIO51\n");
  printf("[SENSOR] Devices: BH1750(0x23/0x5C) OLED(0x3C)\n\n");

  /* Open I2C1 */

  printf("[SENSOR] Opening %s...\n", I2C1_PATH);
  g_i2c_fd = open(I2C1_PATH, O_RDWR);
  if (g_i2c_fd < 0)
    {
      printf("[SENSOR] ERROR: open %s failed: errno=%d\n",
             I2C1_PATH, errno);
      return -errno;
    }

  printf("[SENSOR] I2C1 opened, fd=%d\n\n", g_i2c_fd);

  /* Initialize BH1750 */

  printf("[SENSOR] Init BH1750...\n");
  ret = bh1750_init();
  if (ret < 0)
    {
      printf("[SENSOR] BH1750 init FAILED (will skip light readings)\n\n");
    }
  else
    {
      bh1750_ok = 1;
    }

  /* Initialize OLED */

  printf("[SENSOR] Init OLED...\n");
  ret = oled_init_device();
  if (ret < 0)
    {
      printf("[SENSOR] OLED init FAILED\n");
      close(g_i2c_fd);
      return ret;
    }

  oled_ok = 1;

  /* Show initial screen */

  oled_fill_screen(0x00);
  oled_show_string(16, 0, "Loong Hat", FONT8X16);
  oled_show_string(0, 3, "Sensor Ready", FONT8X16);
  usleep(1000000);

  /* Main loop */

  printf("\n[SENSOR] Starting sensor loop (every %d sec)...\n\n",
         SENSOR_INTERVAL_SEC);

  while (loop_count < 30)
    {
      lux_x100 = 0;

      /* Read BH1750 */

      if (bh1750_ok)
        {
          ret = bh1750_read_lux(&lux_x100);
          if (ret < 0)
            {
              printf("[BH1750] read failed, reinit...\n");
              usleep(500000);
              bh1750_ok = (bh1750_init() == 0);
            }
          else
            {
              printf("[BH1750] light = %u.%02u lux\n",
                     lux_x100 / 100, lux_x100 % 100);
            }
        }

      /* Update OLED */

      if (oled_ok)
        {
          oled_show_light_data(lux_x100);
        }

      loop_count++;
      printf("[SENSOR] --- loop %d done ---\n", loop_count);
      sleep(SENSOR_INTERVAL_SEC);
    }

  /* Cleanup */

  if (oled_ok)
    {
      oled_fill_screen(0x00);
      oled_show_string(0, 3, "  Test Done!", FONT8X16);
    }

  printf("\n[SENSOR] Test complete (%d iterations)\n", loop_count);
  close(g_i2c_fd);
  g_i2c_fd = -1;

  return OK;
}
