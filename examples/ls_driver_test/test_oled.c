/****************************************************************************
 * apps/examples/ls_driver_test/test_oled.c
 *
 * SSD1306 OLED test for LS2K0300 via I2C.
 * Ported from HarmonyOS loong_hat_09_OLED demo.
 *
 * Hardware:
 *   - I2C1 SCL: GPIO50 (pinctrl50)
 *   - I2C1 SDA: GPIO51 (pinctrl51)
 *   - OLED addr: 0x3C (SSD1306, 128x64)
 *
 ****************************************************************************/

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>
#include <nuttx/arch.h>

#include <sys/ioctl.h>
#include <stdio.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>

#include <nuttx/i2c/i2c_master.h>
#include <nuttx/pinctrl/pinctrl.h>

#include "ls_driver_test.h"
#include "oled_fonts.h"

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#define OLED_I2C_PATH       "/dev/i2c1"
#define OLED_I2C_FREQ       I2C_SPEED_STANDARD
#define OLED_ADDR           0x3c
#define OLED_WIDTH          128
#define OLED_PAGES          8
#define OLED_I2C_CMD        0x00
#define OLED_I2C_DATA       0x40

#define I2C1_SCL_PIN        50
#define I2C1_SDA_PIN        51

typedef enum
{
  FONT6X8 = 1,
  FONT8X16,
} oled_font_e;

/****************************************************************************
 * Private Functions
 ****************************************************************************/

static int oled_pinctrl_setup(void)
{
  struct pinctrl_param_s param;
  int fd;
  int ret;

  printf("[OLED] Opening /dev/pinctrl0...\n");
  fd = open("/dev/pinctrl0", O_RDWR);
  if (fd < 0)
    {
      printf("[OLED] ERROR: Failed to open /dev/pinctrl0: %d\n", errno);
      return -errno;
    }

  printf("[OLED] pinctrl0 opened, fd=%d\n", fd);

  /* Set GPIO50 (SCL) to MAIN function (I2C1, function=3) */

  param.pin = I2C1_SCL_PIN;
  param.para.function = 3; /* MAIN_FUNC */
  printf("[OLED] Setting GPIO%d to function %d...\n",
         I2C1_SCL_PIN, param.para.function);
  ret = ioctl(fd, PINCTRLC_SETFUNCTION, (unsigned long)&param);
  if (ret < 0)
    {
      printf("[OLED] ERROR: PINCTRLC_SETFUNCTION GPIO%d failed: "
             "ret=%d errno=%d\n",
             I2C1_SCL_PIN, ret, errno);
      close(fd);
      return -errno;
    }

  printf("[OLED] GPIO%d set OK\n", I2C1_SCL_PIN);

  /* Set GPIO51 (SDA) to MAIN function (I2C1, function=3) */

  param.pin = I2C1_SDA_PIN;
  param.para.function = 3; /* MAIN_FUNC */
  printf("[OLED] Setting GPIO%d to function %d...\n",
         I2C1_SDA_PIN, param.para.function);
  ret = ioctl(fd, PINCTRLC_SETFUNCTION, (unsigned long)&param);
  if (ret < 0)
    {
      printf("[OLED] ERROR: PINCTRLC_SETFUNCTION GPIO%d failed: "
             "ret=%d errno=%d\n",
             I2C1_SDA_PIN, ret, errno);
      close(fd);
      return -errno;
    }

  printf("[OLED] GPIO%d set OK\n", I2C1_SDA_PIN);

  close(fd);
  printf("[OLED] Pinctrl: GPIO%d/GPIO%d set to I2C1 MAIN function\n",
         I2C1_SCL_PIN, I2C1_SDA_PIN);
  return OK;
}

static int oled_write_byte(int fd, uint8_t reg, uint8_t data)
{
  struct i2c_msg_s msg;
  struct i2c_transfer_s trans;
  uint8_t buf[2];
  int ret;

  buf[0] = reg;
  buf[1] = data;

  msg.frequency = OLED_I2C_FREQ;
  msg.addr      = OLED_ADDR;
  msg.flags     = 0;
  msg.buffer    = buf;
  msg.length    = 2;

  trans.msgv = &msg;
  trans.msgc = 1;

  ret = ioctl(fd, I2CIOC_TRANSFER, (unsigned long)&trans);
  if (ret < 0)
    {
      printf("[OLED] I2C WRITE failed: addr=0x%02x reg=0x%02x data=0x%02x "
             "ret=%d errno=%d\n", OLED_ADDR, reg, data, ret, errno);
    }

  return ret;
}

static int oled_write_cmd(int fd, uint8_t cmd)
{
  return oled_write_byte(fd, OLED_I2C_CMD, cmd);
}

static int oled_write_data(int fd, uint8_t data)
{
  return oled_write_byte(fd, OLED_I2C_DATA, data);
}

static int oled_init(int fd)
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
      int ret = oled_write_cmd(fd, init_cmds[i]);
      if (ret < 0)
        {
          printf("[OLED] init cmd 0x%02x failed: %d\n", init_cmds[i], ret);
          return ret;
        }
    }

  return OK;
}

static void oled_set_position(int fd, uint8_t x, uint8_t y)
{
  oled_write_cmd(fd, 0xb0 + y);
  oled_write_cmd(fd, x & 0x0f);
  oled_write_cmd(fd, ((x & 0xf0) >> 4) | 0x10);
}

static void oled_fill_screen(int fd, uint8_t fill)
{
  uint8_t page;
  uint8_t col;

  for (page = 0; page < OLED_PAGES; page++)
    {
      oled_set_position(fd, 0, page);
      for (col = 0; col < OLED_WIDTH; col++)
        {
          oled_write_data(fd, fill);
        }
    }
}

static void oled_show_char(int fd, uint8_t x, uint8_t y,
                           char ch, oled_font_e font)
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
      oled_set_position(fd, x, y);
      for (i = 0; i < 8; i++)
        {
          oled_write_data(fd, F8X16[c * 16 + i]);
        }

      oled_set_position(fd, x, y + 1);
      for (i = 0; i < 8; i++)
        {
          oled_write_data(fd, F8X16[c * 16 + 8 + i]);
        }
    }
  else
    {
      oled_set_position(fd, x, y);
      for (i = 0; i < 6; i++)
        {
          oled_write_data(fd, g_font_6x8[c][i]);
        }
    }
}

static void oled_show_string(int fd, uint8_t x, uint8_t y,
                             const char *str, oled_font_e font)
{
  uint8_t step = (font == FONT8X16) ? 8 : 6;

  if (str == NULL)
    {
      return;
    }

  while (*str != '\0')
    {
      oled_show_char(fd, x, y, *str, font);
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

static void oled_i2c_scan(int fd)
{
  struct i2c_msg_s msg;
  struct i2c_transfer_s trans;
  uint8_t buf;
  int addr;
  int found = 0;
  int ret;

  printf("[OLED] Scanning I2C1 bus (0x03 - 0x77)...\n");

  for (addr = 0x03; addr < 0x78; addr++)
    {
      msg.frequency = OLED_I2C_FREQ;
      msg.addr      = addr;
      msg.flags     = I2C_M_READ;
      msg.buffer    = &buf;
      msg.length    = 1;

      trans.msgv = &msg;
      trans.msgc = 1;

      ret = ioctl(fd, I2CIOC_TRANSFER, (unsigned long)&trans);
      if (ret == 1)
        {
          printf("[OLED]   Found device at 0x%02x\n", addr);
          found++;
        }
    }

  if (found == 0)
    {
      printf("[OLED]   No devices found on I2C1!\n");
      printf("[OLED]   Check: 1) Wiring 2) Pull-up resistors"
             " 3) OLED power\n");
    }
  else
    {
      printf("[OLED]   %d device(s) found\n", found);
    }
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

int test_oled(void)
{
  int fd;
  int ret;

  printf("[OLED] === SSD1306 OLED Test Start ===\n");
  printf("[OLED] Target: I2C1 addr=0x%02x SCL=GPIO%d SDA=GPIO%d\n",
         OLED_ADDR, I2C1_SCL_PIN, I2C1_SDA_PIN);

  /* Configure GPIO50/51 as I2C1 function */

  printf("[OLED] Step 1: Configure pinctrl...\n");
  ret = oled_pinctrl_setup();
  if (ret < 0)
    {
      printf("[OLED] Pinctrl setup FAILED: %d\n", ret);
      return ret;
    }

  printf("[OLED] Step 2: Open I2C1 device...\n");

  /* Open I2C1 device */

  fd = open(OLED_I2C_PATH, O_RDWR);
  if (fd < 0)
    {
      printf("[OLED] ERROR: Failed to open %s: errno=%d\n",
             OLED_I2C_PATH, errno);
      return -errno;
    }

  printf("[OLED] I2C1 opened: %s fd=%d\n", OLED_I2C_PATH, fd);

  /* Scan I2C bus for devices */

  printf("[OLED] Step 3: Scanning I2C bus...\n");
  oled_i2c_scan(fd);

  /* Initialize OLED */

  printf("[OLED] Step 4: Initializing OLED...\n");
  ret = oled_init(fd);
  if (ret < 0)
    {
      printf("[OLED] ERROR: OLED init failed: %d\n", ret);
      close(fd);
      return ret;
    }

  printf("[OLED] SSD1306 initialized\n");

  /* Fill screen and show test text */

  oled_fill_screen(fd, 0x00);
  oled_show_string(fd, 16, 0, "Loong Hat", FONT8X16);
  oled_show_string(fd, 0, 2, "SSD1306 OLED", FONT8X16);
  oled_show_string(fd, 0, 4, "I2C1 Test OK", FONT8X16);
  oled_show_string(fd, 0, 6, "128x64 Ready!", FONT6X8);

  printf("[OLED] Display test text written\n");

  /* Leave the display on for 3 seconds */

  up_mdelay(3000);

  /* Clear screen */

  oled_fill_screen(fd, 0x00);
  oled_show_string(fd, 0, 0, "OLED Test Done", FONT8X16);

  printf("[OLED] Done.\n\n");

  close(fd);
  return OK;
}
