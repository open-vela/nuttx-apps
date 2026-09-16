/****************************************************************************
 * apps/examples/ls_driver_test/test_eeprom.c
 *
 * EEPROM + BH1750 light sensor test for LS2K0300 (LOONG-HAT).
 *
 * Function:
 *   1. On startup, read and print EEPROM contents
 *   2. When KEY1 (GPIO87) is pressed, read BH1750 light sensor 5 times,
 *      print each reading, then overwrite EEPROM with the new data
 *   3. When KEY2 (GPIO86) is pressed, exit the program
 *
 * Hardware (LOONG-HAT):
 *   - I2C1 SCL: GPIO50, SDA: GPIO51
 *   - EEPROM addr: 0x50 (AT24C02)
 *   - BH1750 addr: 0x23 (or 0x5C)
 *   - KEY1: GPIO87, KEY2: GPIO86
 *
 ****************************************************************************/

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>

#include <sys/ioctl.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <stdint.h>

#include <nuttx/i2c/i2c_master.h>
#include <nuttx/ioexpander/gpio.h>
#include <nuttx/arch.h>

#include "ls_driver_test.h"

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#define I2C1_PATH           "/dev/i2c1"
#define I2C_FREQ            I2C_SPEED_STANDARD

/* EEPROM (AT24C02) definitions */

#define EEPROM_ADDR         0x50
#define EEPROM_SIZE         256
#define EEPROM_PAGE_SIZE    8
#define EEPROM_WRITE_DELAY  20

/* EEPROM memory layout:
 *   0x0000        - magic (0xA5 = valid data)
 *   0x0001        - number of stored readings (uint8_t)
 *   0x0010..0x0019 - 5 x uint16_t light readings (lux * 100)
 */

#define EEPROM_MAGIC_ADDR   0x0000
#define EEPROM_COUNT_ADDR   0x0001
#define EEPROM_DATA_ADDR    0x0010
#define EEPROM_MAGIC_VALUE  0xA5
#define EEPROM_MAX_READINGS 5

/* BH1750 definitions */

#define BH1750_ADDR_LOW     0x23
#define BH1750_ADDR_HIGH    0x5c
#define BH1750_POWER_ON     0x01
#define BH1750_RESET        0x07
#define BH1750_H_MODE       0x10

/* GPIO definitions */

#define KEY1_GPIO           87
#define KEY2_GPIO           86

/* Timing */

#define KEY_SCAN_DELAY_MS   10
#define KEY_DEBOUNCE_MS     20
#define BH1750_READ_DELAY   180

/****************************************************************************
 * Private Data
 ****************************************************************************/

static int g_i2c_fd = -1;
static uint8_t g_bh1750_addr = BH1750_ADDR_LOW;

/****************************************************************************
 * Private Functions - I2C Helper
 ****************************************************************************/

static int eeprom_i2c_write(uint8_t addr, const uint8_t *data, int len)
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

static int eeprom_i2c_read(uint8_t addr, uint8_t *data, int len)
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
 * Private Functions - EEPROM
 ****************************************************************************/

static int eeprom_read_byte(uint16_t mem_addr, uint8_t *data)
{
  uint8_t addr[2];
  int ret;

  addr[0] = (uint8_t)(mem_addr >> 8);
  addr[1] = (uint8_t)(mem_addr & 0xff);

  ret = eeprom_i2c_write(EEPROM_ADDR, addr, 2);
  if (ret < 0)
    {
      return ret;
    }

  return eeprom_i2c_read(EEPROM_ADDR, data, 1);
}

static int eeprom_write_byte(uint16_t mem_addr, uint8_t data)
{
  uint8_t buf[3];

  buf[0] = (uint8_t)(mem_addr >> 8);
  buf[1] = (uint8_t)(mem_addr & 0xff);
  buf[2] = data;

  return eeprom_i2c_write(EEPROM_ADDR, buf, 3);
}

static int eeprom_write_page(uint16_t mem_addr,
                             const uint8_t *data, int len)
{
  uint8_t buf[2 + EEPROM_PAGE_SIZE];
  int i;

  if (len > EEPROM_PAGE_SIZE)
    {
      len = EEPROM_PAGE_SIZE;
    }

  buf[0] = (uint8_t)(mem_addr >> 8);
  buf[1] = (uint8_t)(mem_addr & 0xff);
  for (i = 0; i < len; i++)
    {
      buf[2 + i] = data[i];
    }

  return eeprom_i2c_write(EEPROM_ADDR, buf, 2 + len);
}

static int eeprom_read_bytes(uint16_t mem_addr,
                             uint8_t *data, int len)
{
  uint8_t addr[2];
  int ret;

  addr[0] = (uint8_t)(mem_addr >> 8);
  addr[1] = (uint8_t)(mem_addr & 0xff);

  ret = eeprom_i2c_write(EEPROM_ADDR, addr, 2);
  if (ret < 0)
    {
      return ret;
    }

  return eeprom_i2c_read(EEPROM_ADDR, data, len);
}

static void eeprom_print_contents(void)
{
  uint8_t magic;
  uint8_t count;
  uint8_t i;
  int ret;

  printf("[EEPROM] ---- Current EEPROM Contents ----\n");

  ret = eeprom_read_byte(EEPROM_MAGIC_ADDR, &magic);
  if (ret < 0)
    {
      printf("[EEPROM] ERROR: read magic failed: %d\n", ret);
      return;
    }

  printf("[EEPROM] Magic: 0x%02X (%s)\n",
         magic, (magic == EEPROM_MAGIC_VALUE) ? "valid" : "invalid");

  if (magic != EEPROM_MAGIC_VALUE)
    {
      printf("[EEPROM] No valid data stored\n");
      printf("[EEPROM] ---- End of EEPROM ----\n\n");
      return;
    }

  ret = eeprom_read_byte(EEPROM_COUNT_ADDR, &count);
  if (ret < 0)
    {
      printf("[EEPROM] ERROR: read count failed: %d\n", ret);
      return;
    }

  printf("[EEPROM] Stored readings: %u\n", count);

  if (count > EEPROM_MAX_READINGS)
    {
      count = EEPROM_MAX_READINGS;
    }

  for (i = 0; i < count; i++)
    {
      uint8_t buf[2];
      uint16_t lux_x100;

      ret = eeprom_read_bytes(EEPROM_DATA_ADDR + i * 2, buf, 2);
      if (ret < 0)
        {
          printf("[EEPROM]   [%u] read error: %d\n", i, ret);
          continue;
        }

      lux_x100 = ((uint16_t)buf[0] << 8) | buf[1];
      printf("[EEPROM]   [%u] %u.%02u lux\n",
             i, lux_x100 / 100, lux_x100 % 100);
    }

  printf("[EEPROM] ---- End of EEPROM ----\n\n");
}

static int eeprom_save_readings(const uint16_t *readings, int count)
{
  uint8_t buf[EEPROM_MAX_READINGS * 2];
  int i;
  int ret;
  int page_off;
  int page_len;

  if (count > EEPROM_MAX_READINGS)
    {
      count = EEPROM_MAX_READINGS;
    }

  /* Pack readings into byte array (big-endian uint16_t) */

  for (i = 0; i < count; i++)
    {
      buf[i * 2]     = (uint8_t)(readings[i] >> 8);
      buf[i * 2 + 1] = (uint8_t)(readings[i] & 0xff);
    }

  /* Write count */

  ret = eeprom_write_byte(EEPROM_COUNT_ADDR, (uint8_t)count);
  if (ret < 0)
    {
      printf("[EEPROM] ERROR: write count failed: %d\n", ret);
      return ret;
    }

  usleep(EEPROM_WRITE_DELAY * 1000);

  /* Write data in page-sized chunks */

  for (page_off = 0; page_off < count * 2; page_off += EEPROM_PAGE_SIZE)
    {
      page_len = count * 2 - page_off;
      if (page_len > EEPROM_PAGE_SIZE)
        {
          page_len = EEPROM_PAGE_SIZE;
        }

      ret = eeprom_write_page(EEPROM_DATA_ADDR + page_off,
                              &buf[page_off], page_len);
      if (ret < 0)
        {
          printf("[EEPROM] ERROR: write data at 0x%04X failed: %d\n",
                 EEPROM_DATA_ADDR + page_off, ret);
          return ret;
        }

      usleep(EEPROM_WRITE_DELAY * 1000);
    }

  /* Write magic last (signals data is valid) */

  ret = eeprom_write_byte(EEPROM_MAGIC_ADDR, EEPROM_MAGIC_VALUE);
  if (ret < 0)
    {
      printf("[EEPROM] ERROR: write magic failed: %d\n", ret);
      return ret;
    }

  usleep(EEPROM_WRITE_DELAY * 1000);

  /* Verify */

  for (i = 0; i < count; i++)
    {
      uint8_t verify[2];
      uint16_t stored;

      ret = eeprom_read_bytes(EEPROM_DATA_ADDR + i * 2, verify, 2);
      if (ret < 0)
        {
          printf("[EEPROM] ERROR: verify read [%d] failed: %d\n", i, ret);
          return ret;
        }

      stored = ((uint16_t)verify[0] << 8) | verify[1];
      if (stored != readings[i])
        {
          printf("[EEPROM] ERROR: verify mismatch [%d]: "
                 "expected %u, got %u\n", i, readings[i], stored);
          return -EIO;
        }
    }

  printf("[EEPROM] Saved and verified %d readings\n", count);
  return OK;
}

/****************************************************************************
 * Private Functions - BH1750 Light Sensor
 ****************************************************************************/

static int bh1750_write_cmd(uint8_t cmd)
{
  return eeprom_i2c_write(g_bh1750_addr, &cmd, 1);
}

static int bh1750_init(void)
{
  int ret;

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

  usleep(BH1750_READ_DELAY * 1000);

  printf("[BH1750] init OK\n");
  return OK;
}

static int bh1750_read_lux(uint16_t *lux_x100)
{
  uint8_t buf[2] =
  {
    0
  };

  int ret;
  uint16_t raw;

  ret = eeprom_i2c_read(g_bh1750_addr, buf, 2);
  if (ret < 0)
    {
      printf("[BH1750] read failed: %d\n", ret);
      return ret;
    }

  raw = ((uint16_t)buf[0] << 8) | buf[1];
  *lux_x100 = (uint16_t)((raw * 1000 + 6) / 12);

  return OK;
}

/****************************************************************************
 * Private Functions - GPIO (KEY1)
 ****************************************************************************/

static int init_key_gpio(int gpio)
{
  int ret;
  char path[32];
  int fd;

  ret = pinctrl_set_gpio_function(gpio);
  if (ret < 0)
    {
      printf("[EEPROM] ERROR: pinctrl failed for GPIO%d\n", gpio);
      return ret;
    }

  snprintf(path, sizeof(path), "/dev/gpio%d", gpio);
  fd = open(path, O_RDWR);
  if (fd < 0)
    {
      printf("[EEPROM] ERROR: open %s failed: %d\n", path, errno);
      return -errno;
    }

  ret = ioctl(fd, GPIOC_SETPINTYPE, (unsigned long)GPIO_INPUT_PIN);
  if (ret < 0)
    {
      printf("[EEPROM] ERROR: GPIOC_SETPINTYPE(INPUT) on GPIO%d: %d\n",
             gpio, errno);
      close(fd);
      return -errno;
    }

  return fd;
}

static bool get_key_value(int fd)
{
  bool value = true;

  ioctl(fd, GPIOC_READ, (unsigned long)(uintptr_t)&value);
  return value;
}

static int is_key_pressed(int fd, bool *last)
{
  bool value = get_key_value(fd);

  if (!value && *last)
    {
      up_mdelay(KEY_DEBOUNCE_MS);
      value = get_key_value(fd);
      if (!value)
        {
          *last = value;
          return 1;
        }
    }

  *last = value;
  return 0;
}

/****************************************************************************
 * Private Functions - Light Sensor Read and Save
 ****************************************************************************/

static int read_and_save_light(void)
{
  uint16_t readings[EEPROM_MAX_READINGS];
  int bh1750_ok = 0;
  int i;
  int ret;

  printf("\n[EEPROM] === KEY1 pressed: reading light sensor 5 times ===\n");

  /* Initialize BH1750 */

  ret = bh1750_init();
  if (ret < 0)
    {
      printf("[EEPROM] BH1750 init failed, aborting\n");
      return ret;
    }

  bh1750_ok = 1;

  /* Read 5 times */

  for (i = 0; i < EEPROM_MAX_READINGS; i++)
    {
      usleep(300000);  /* 300ms between readings */

      if (bh1750_ok)
        {
          ret = bh1750_read_lux(&readings[i]);
          if (ret < 0)
            {
              printf("[EEPROM]   [%d] BH1750 read failed: %d\n", i, ret);
              readings[i] = 0;

              /* Try reinit */

              usleep(500000);
              bh1750_ok = (bh1750_init() == 0);
            }
          else
            {
              printf("[EEPROM]   [%d] light = %u.%02u lux\n",
                     i, readings[i] / 100, readings[i] % 100);
            }
        }
      else
        {
          readings[i] = 0;
          printf("[EEPROM]   [%d] sensor unavailable\n", i);
        }
    }

  /* Save to EEPROM */

  printf("[EEPROM] Saving readings to EEPROM...\n");
  ret = eeprom_save_readings(readings, EEPROM_MAX_READINGS);
  if (ret < 0)
    {
      printf("[EEPROM] Save failed!\n");
      return ret;
    }

  /* Print updated EEPROM contents */

  printf("[EEPROM] Verifying EEPROM contents:\n");
  eeprom_print_contents();

  return OK;
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

int test_eeprom(void)
{
  int fd_key1;
  int fd_key2;
  bool key1_last;
  bool key2_last;
  int ret;

  printf("[EEPROM] === EEPROM + BH1750 Light Sensor Test ===\n");
  printf("[EEPROM] I2C1: SCL=GPIO50 SDA=GPIO51\n");
  printf("[EEPROM] EEPROM addr=0x%02X, BH1750 addr=0x%02X/0x%02X\n",
         EEPROM_ADDR, BH1750_ADDR_LOW, BH1750_ADDR_HIGH);
  printf("[EEPROM] KEY1=GPIO%d: read light x5, overwrite EEPROM\n",
         KEY1_GPIO);
  printf("[EEPROM] KEY2=GPIO%d: exit program\n\n", KEY2_GPIO);

  /* Open I2C1 */

  printf("[EEPROM] Opening %s...\n", I2C1_PATH);
  g_i2c_fd = open(I2C1_PATH, O_RDWR);
  if (g_i2c_fd < 0)
    {
      printf("[EEPROM] ERROR: open %s failed: errno=%d\n",
             I2C1_PATH, errno);
      return -errno;
    }

  printf("[EEPROM] I2C1 opened, fd=%d\n\n", g_i2c_fd);

  /* Read and print current EEPROM contents */

  printf("[EEPROM] Reading current EEPROM contents...\n");
  eeprom_print_contents();

  /* Initialize KEY1 */

  fd_key1 = init_key_gpio(KEY1_GPIO);
  if (fd_key1 < 0)
    {
      close(g_i2c_fd);
      return fd_key1;
    }

  /* Initialize KEY2 */

  fd_key2 = init_key_gpio(KEY2_GPIO);
  if (fd_key2 < 0)
    {
      close(fd_key1);
      close(g_i2c_fd);
      return fd_key2;
    }

  key1_last = get_key_value(fd_key1);
  key2_last = get_key_value(fd_key2);
  printf("[EEPROM] KEY1(GPIO%d) ready, waiting for press...\n",
         KEY1_GPIO);
  printf("[EEPROM] KEY2(GPIO%d) ready, press to exit\n\n",
         KEY2_GPIO);

  /* Main loop - wait for KEY1 press, KEY2 to exit */

  while (1)
    {
      if (is_key_pressed(fd_key1, &key1_last))
        {
          ret = read_and_save_light();
          if (ret < 0)
            {
              printf("[EEPROM] read_and_save_light failed: %d\n", ret);
            }

          printf("\n[EEPROM] Waiting for next KEY1 press...\n\n");
        }

      if (is_key_pressed(fd_key2, &key2_last))
        {
          printf("\n[EEPROM] KEY2 pressed, exiting...\n");
          break;
        }

      up_mdelay(KEY_SCAN_DELAY_MS);
    }

  /* Cleanup */

  close(fd_key2);
  close(fd_key1);
  close(g_i2c_fd);
  g_i2c_fd = -1;

  printf("[EEPROM] Test complete.\n");
  return OK;
}
