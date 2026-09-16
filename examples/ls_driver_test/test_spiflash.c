/****************************************************************************
 * apps/examples/ls_driver_test/test_spiflash.c
 *
 * SPI Flash + BH1750 light sensor test for LS2K0300 (LOONG-HAT).
 *
 * Function:
 *   1. On startup, read and print stored data from SPI Flash
 *   2. Press KEY1 (GPIO87): read BH1750 light sensor 5 times via I2C1,
 *      save readings to SPI Flash, then read back and print
 *   3. Press KEY2 (GPIO86): exit program
 *
 * Hardware (LOONG-HAT 40PIN header):
 *   - I2C1 SCL: GPIO50, SDA: GPIO51
 *   - BH1750 addr: 0x23 (or 0x5C)
 *   - SPI2 SCK : GPIO64, MISO: GPIO65, MOSI: GPIO66
 *   - Flash CS : GPIO85
 *   - KEY1     : GPIO87
 *   - KEY2     : GPIO86
 *
 * Flash memory layout (last 4KB sector + 0x100):
 *   0x0000        - magic (0xA5 = valid data)
 *   0x0001        - number of stored readings (uint8_t)
 *   0x0010..0x0019 - 5 x uint16_t light readings (lux * 100)
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

#include <nuttx/ioexpander/gpio.h>
#include <nuttx/i2c/i2c_master.h>
#include <nuttx/arch.h>

#include "ls_driver_test.h"

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/* SPI2 controller base (uncached, PHYS 0x1610c000) */

#define SPI2_BASE               (0x8000000000000000UL | 0x1610c000UL)

/* GENERAL_CFG5 for SPI2/3 clock gate */

#define LS_GENERAL_CFG5_ADDR    (0x8000000000000000UL | 0x16000114UL)

/* SPI IO register offsets (LS2K0300 manual ch.10) */

#define LS_SPI_IO_CR1           0x00
#define LS_SPI_IO_CR3           0x08
#define LS_SPI_IO_CR4           0x0c
#define LS_SPI_IO_SR1           0x14
#define LS_SPI_IO_CFG1          0x20
#define LS_SPI_IO_CFG2          0x24
#define LS_SPI_IO_CFG3          0x28
#define LS_SPI_IO_DR            0x40

/* CR1 bits */

#define LS_SPI_CR1_SPE          (1U << 0)
#define LS_SPI_CR1_CSTART       (1U << 1)
#define LS_SPI_CR1_AUTOSUS      (1U << 2)

/* CFG1: 8-bit frame (DSIZE=7), MSB first, CPOL=0 CPHA=0 (mode 0) */

#define LS_SPI_CFG1_DSIZE_8BIT  (7U << 8)

/* CFG2: baud ratio divider */

#define LS_SPI_CFG2_BRINT_SHIFT 8

/* CFG3: master, full duplex, software CS */

#define LS_SPI_CFG3_MSTR        (1U << 0)
#define LS_SPI_CFG3_DIE         (1U << 2)
#define LS_SPI_CFG3_DOE         (1U << 3)
#define LS_SPI_CFG3_SSMODE_SW   (1U << 8)

/* SR1 status bits */

#define LS_SPI_SR1_RXA          (1U << 0)
#define LS_SPI_SR1_TXA          (1U << 1)
#define LS_SPI_SR1_RXE          (1U << 4)
#define LS_SPI_SR1_EOT          (1U << 15)

#define LS_SPI_SR1_W1C_FLAGS    (LS_SPI_SR1_EOT | (1U << 11) | \
                                 (1U << 10) | (1U << 9) | (1U << 8))

/* APB ~200MHz, use 1MHz SPI clock */

#define SPI_APB_HZ              (200UL * 1000000UL)
#define SPI_CLOCK_HZ            1000000UL
#define SPI_WAIT_TIMEOUT_US     10000U

/* GPIO pin definitions */

#define FLASH_CS_PIN            85
#define KEY1_GPIO               87
#define KEY2_GPIO               86

/* I2C configuration */

#define I2C1_PATH               "/dev/i2c1"
#define I2C_FREQ                I2C_SPEED_STANDARD

/* BH1750 definitions */

#define BH1750_ADDR_LOW         0x23
#define BH1750_ADDR_HIGH        0x5c
#define BH1750_POWER_ON         0x01
#define BH1750_RESET            0x07
#define BH1750_H_MODE           0x10
#define BH1750_READ_DELAY_MS    180

/* SPI Flash commands */

#define SPIFLASH_CMD_WRITE_ENABLE     0x06
#define SPIFLASH_CMD_READ_STATUS      0x05
#define SPIFLASH_CMD_READ_JEDEC_ID    0x9f
#define SPIFLASH_CMD_READ_DATA        0x03
#define SPIFLASH_CMD_PAGE_PROGRAM     0x02
#define SPIFLASH_CMD_SECTOR_ERASE_4K  0x20
#define SPIFLASH_CMD_RELEASE_PD       0xab  /* Release power-down / reset */
#define SPIFLASH_CMD_ENABLE_RESET     0x66
#define SPIFLASH_CMD_RESET            0x99
#define SPIFLASH_STATUS_WIP           0x01

/* Flash parameters */

#define SPIFLASH_SECTOR_SIZE          4096U
#define SPIFLASH_TEST_OFFSET          0x100U

/* Flash data layout */

#define FLASH_MAGIC_ADDR              0x0000
#define FLASH_COUNT_ADDR              0x0001
#define FLASH_DATA_ADDR               0x0010
#define FLASH_MAGIC_VALUE             0xA5
#define FLASH_MAX_READINGS            5

/* Timing */

#define KEY_SCAN_DELAY_MS             10
#define KEY_DEBOUNCE_MS               20

/* Register access macros */

#define SPI_REG32(off)  (*(volatile uint32_t *)(SPI2_BASE + (off)))
#define SPI_REG8(off)   (*(volatile uint8_t *)(SPI2_BASE + (off)))
#define CFG5_REG        (*(volatile uint32_t *)LS_GENERAL_CFG5_ADDR)

/****************************************************************************
 * Private Function Prototypes
 ****************************************************************************/

static void gpio_write(int fd, bool value);

/****************************************************************************
 * Private Data
 ****************************************************************************/

static int g_fd_flash_cs = -1;
static int g_fd_key1 = -1;
static int g_fd_key2 = -1;
static int g_i2c_fd = -1;
static uint8_t g_bh1750_addr = BH1750_ADDR_LOW;

/****************************************************************************
 * Private Functions - SPI2 Hardware
 ****************************************************************************/

static void spi2_bus_reset(void)
{
  uint32_t val;
  uint32_t i;

  val = SPI_REG32(LS_SPI_IO_CR1);
  val &= ~(LS_SPI_CR1_CSTART | LS_SPI_CR1_AUTOSUS);
  SPI_REG32(LS_SPI_IO_CR1) = val;

  SPI_REG32(LS_SPI_IO_CR4) = 0;
  SPI_REG32(LS_SPI_IO_CR3) = 0;
  up_udelay(10);

  SPI_REG32(LS_SPI_IO_SR1) = LS_SPI_SR1_W1C_FLAGS;

  for (i = 0; i < 4; i++)
    {
      if (SPI_REG32(LS_SPI_IO_SR1) & LS_SPI_SR1_RXE)
        {
          break;
        }

      (void)SPI_REG8(LS_SPI_IO_DR);
    }

  SPI_REG32(LS_SPI_IO_CR1) = LS_SPI_CR1_SPE;
  gpio_write(g_fd_flash_cs, true);
  up_udelay(10);
}

static void spi2_abort(void)
{
  uint32_t val;
  uint32_t i;

  val = SPI_REG32(LS_SPI_IO_CR1);
  val &= ~(LS_SPI_CR1_CSTART | LS_SPI_CR1_AUTOSUS);
  SPI_REG32(LS_SPI_IO_CR1) = val;

  SPI_REG32(LS_SPI_IO_CR4) = 0;
  SPI_REG32(LS_SPI_IO_CR3) = 0;
  up_udelay(10);

  SPI_REG32(LS_SPI_IO_SR1) = LS_SPI_SR1_W1C_FLAGS;

  for (i = 0; i < 4; i++)
    {
      if (SPI_REG32(LS_SPI_IO_SR1) & LS_SPI_SR1_RXE)
        {
          break;
        }

      (void)SPI_REG8(LS_SPI_IO_DR);
    }

  gpio_write(g_fd_flash_cs, true);
  up_udelay(1);
}

static int spi2_xfer_byte(uint8_t tx, uint8_t *rx)
{
  uint32_t val;
  uint32_t sr1;
  uint32_t i;

  /* Enable SPI */

  val = SPI_REG32(LS_SPI_IO_CR1);
  val |= LS_SPI_CR1_SPE;
  SPI_REG32(LS_SPI_IO_CR1) = val;

  /* Clear CSTART/AUTOSUS for clean rising edge */

  val = SPI_REG32(LS_SPI_IO_CR1);
  val &= ~(LS_SPI_CR1_CSTART | LS_SPI_CR1_AUTOSUS);
  SPI_REG32(LS_SPI_IO_CR1) = val;

  /* Clear W1C status flags */

  SPI_REG32(LS_SPI_IO_SR1) = LS_SPI_SR1_W1C_FLAGS;

  /* TSIZE = 0 (single frame) */

  SPI_REG32(LS_SPI_IO_CR3) = 0;

  /* Wait for TX FIFO available */

  for (i = 0; i < SPI_WAIT_TIMEOUT_US; i++)
    {
      sr1 = SPI_REG32(LS_SPI_IO_SR1);
      if (sr1 & LS_SPI_SR1_TXA)
        {
          break;
        }

      up_udelay(1);
    }

  if (i >= SPI_WAIT_TIMEOUT_US)
    {
      spi2_abort();
      return -1;
    }

  /* Write TX byte (8-bit access!) */

  SPI_REG8(LS_SPI_IO_DR) = tx;

  /* Start transfer: set CSTART only, no AUTOSUS */

  val = SPI_REG32(LS_SPI_IO_CR1);
  val |= LS_SPI_CR1_CSTART;
  SPI_REG32(LS_SPI_IO_CR1) = val;

  /* Wait for EOT (end of transfer) */

  for (i = 0; i < SPI_WAIT_TIMEOUT_US; i++)
    {
      sr1 = SPI_REG32(LS_SPI_IO_SR1);
      if (sr1 & LS_SPI_SR1_EOT)
        {
          break;
        }

      up_udelay(1);
    }

  if (i >= SPI_WAIT_TIMEOUT_US)
    {
      spi2_abort();
      return -1;
    }

  /* Clear EOT */

  SPI_REG32(LS_SPI_IO_SR1) = LS_SPI_SR1_EOT;

  /* Wait for RX FIFO has data */

  for (i = 0; i < SPI_WAIT_TIMEOUT_US; i++)
    {
      sr1 = SPI_REG32(LS_SPI_IO_SR1);
      if (sr1 & LS_SPI_SR1_RXA)
        {
          break;
        }

      up_udelay(1);
    }

  if (i >= SPI_WAIT_TIMEOUT_US)
    {
      spi2_abort();
      return -1;
    }

  /* Read RX byte (8-bit access!) */

  if (rx != NULL)
    {
      *rx = SPI_REG8(LS_SPI_IO_DR);
    }
  else
    {
      (void)SPI_REG8(LS_SPI_IO_DR);
    }

  return 0;
}

static int spi2_init(void)
{
  uint32_t brint;

  CFG5_REG |= (0x3u << 17);

  SPI_REG32(LS_SPI_IO_CFG3) = LS_SPI_CFG3_MSTR | LS_SPI_CFG3_DIE |
                               LS_SPI_CFG3_DOE | LS_SPI_CFG3_SSMODE_SW;
  SPI_REG32(LS_SPI_IO_CFG1) = LS_SPI_CFG1_DSIZE_8BIT;

  brint = SPI_APB_HZ / SPI_CLOCK_HZ;
  if (brint < 2)
    {
      brint = 2;
    }

  if (brint > 255)
    {
      brint = 255;
    }

  SPI_REG32(LS_SPI_IO_CFG2) = brint << LS_SPI_CFG2_BRINT_SHIFT;
  SPI_REG32(LS_SPI_IO_CR1) = LS_SPI_CR1_SPE;

  gpio_write(g_fd_flash_cs, true);
  up_udelay(10);

  return OK;
}

/****************************************************************************
 * Private Functions - GPIO Helpers
 ****************************************************************************/

static int gpio_open_output(int pin)
{
  int ret;
  char path[32];
  int fd;

  ret = pinctrl_set_gpio_function(pin);
  if (ret < 0)
    {
      printf("[SPIFLASH] ERROR: pinctrl GPIO%d: %d\n", pin, ret);
      return ret;
    }

  snprintf(path, sizeof(path), "/dev/gpio%d", pin);
  fd = open(path, O_RDWR);
  if (fd < 0)
    {
      printf("[SPIFLASH] ERROR: open %s: %d\n", path, errno);
      return -errno;
    }

  ret = ioctl(fd, GPIOC_SETPINTYPE, (unsigned long)GPIO_OUTPUT_PIN);
  if (ret < 0)
    {
      printf("[SPIFLASH] ERROR: set output GPIO%d: %d\n", pin, errno);
      close(fd);
      return -errno;
    }

  ioctl(fd, GPIOC_WRITE, (unsigned long)true);
  return fd;
}

static int gpio_open_input(int pin)
{
  int ret;
  char path[32];
  int fd;

  ret = pinctrl_set_gpio_function(pin);
  if (ret < 0)
    {
      printf("[SPIFLASH] ERROR: pinctrl GPIO%d: %d\n", pin, ret);
      return ret;
    }

  snprintf(path, sizeof(path), "/dev/gpio%d", pin);
  fd = open(path, O_RDWR);
  if (fd < 0)
    {
      printf("[SPIFLASH] ERROR: open %s: %d\n", path, errno);
      return -errno;
    }

  ret = ioctl(fd, GPIOC_SETPINTYPE, (unsigned long)GPIO_INPUT_PIN);
  if (ret < 0)
    {
      printf("[SPIFLASH] ERROR: set input GPIO%d: %d\n", pin, errno);
      close(fd);
      return -errno;
    }

  return fd;
}

static bool gpio_read(int fd)
{
  bool value = true;
  ioctl(fd, GPIOC_READ, (unsigned long)(uintptr_t)&value);
  return value;
}

static void gpio_write(int fd, bool value)
{
  ioctl(fd, GPIOC_WRITE, (unsigned long)value);
}

static int key_scan(int fd, bool *last)
{
  bool value = gpio_read(fd);

  if (!value && *last)
    {
      up_mdelay(KEY_DEBOUNCE_MS);
      value = gpio_read(fd);
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
 * Private Functions - I2C Helper
 ****************************************************************************/

static int ls_i2c_write(uint8_t addr, const uint8_t *data, int len)
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

static int ls_i2c_read(uint8_t addr, uint8_t *data, int len)
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
  return ls_i2c_write(g_bh1750_addr, &cmd, 1);
}

static int bh1750_init(void)
{
  int ret;

  g_bh1750_addr = BH1750_ADDR_LOW;
  ret = bh1750_write_cmd(BH1750_POWER_ON);
  if (ret < 0)
    {
      printf("[SPIFLASH] BH1750 probe addr 0x%02x failed\n",
             BH1750_ADDR_LOW);
      g_bh1750_addr = BH1750_ADDR_HIGH;
      ret = bh1750_write_cmd(BH1750_POWER_ON);
    }

  if (ret < 0)
    {
      printf("[SPIFLASH] BH1750 probe addr 0x%02x failed\n",
             BH1750_ADDR_HIGH);
      printf("[SPIFLASH] No ACK, check wiring\n");
      return ret;
    }

  printf("[SPIFLASH] BH1750 detected addr=0x%02x\n", g_bh1750_addr);
  usleep(20000);

  ret = bh1750_write_cmd(BH1750_RESET);
  if (ret < 0)
    {
      printf("[SPIFLASH] BH1750 reset failed\n");
      return ret;
    }

  usleep(20000);

  ret = bh1750_write_cmd(BH1750_H_MODE);
  if (ret < 0)
    {
      printf("[SPIFLASH] BH1750 set H-mode failed\n");
      return ret;
    }

  usleep(BH1750_READ_DELAY_MS * 1000);

  printf("[SPIFLASH] BH1750 init OK\n");
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

  ret = ls_i2c_read(g_bh1750_addr, buf, 2);
  if (ret < 0)
    {
      printf("[SPIFLASH] BH1750 read failed: %d\n", ret);
      return ret;
    }

  raw = ((uint16_t)buf[0] << 8) | buf[1];
  *lux_x100 = (uint16_t)((raw * 1000 + 6) / 12);

  return OK;
}

/****************************************************************************
 * Private Functions - SPI Flash (CS on GPIO85)
 *
 * IMPORTANT: Do NOT call spi2_bus_reset() inside flash_xxx() functions!
 * Flash operations like erase/program require a clean CS sequence:
 *   CS low -> cmd+addr+data -> CS high -> wait ready
 * Calling spi2_bus_reset() mid-operation will toggle CS and abort the
 * flash operation. Only call spi2_bus_reset() BEFORE starting a new
 * top-level flash operation (erase, program, read).
 ****************************************************************************/

static void flash_cs_select(void)
{
  gpio_write(g_fd_flash_cs, false);
  up_udelay(1);
}

static void flash_cs_deselect(void)
{
  gpio_write(g_fd_flash_cs, true);
  up_udelay(10);  /* Longer delay for flash to process command */
}

static uint8_t flash_xfer(uint8_t tx)
{
  uint8_t rx = 0;
  spi2_xfer_byte(tx, &rx);
  return rx;
}

/* Software reset: enable_reset + reset, then wait for flash to recover */

static void flash_software_reset(void)
{
  flash_cs_select();
  flash_xfer(SPIFLASH_CMD_ENABLE_RESET);
  flash_cs_deselect();
  up_udelay(1);

  flash_cs_select();
  flash_xfer(SPIFLASH_CMD_RESET);
  flash_cs_deselect();
  up_mdelay(30);  /* Flash needs ~30ms to reset */
}

static void flash_read_jedec_id(uint8_t id[3])
{
  flash_cs_select();
  flash_xfer(SPIFLASH_CMD_READ_JEDEC_ID);
  id[0] = flash_xfer(0xff);
  id[1] = flash_xfer(0xff);
  id[2] = flash_xfer(0xff);
  flash_cs_deselect();
}

static int flash_id_valid(const uint8_t id[3])
{
  if ((id[0] == 0x00 && id[1] == 0x00 && id[2] == 0x00) ||
      (id[0] == 0xff && id[1] == 0xff && id[2] == 0xff))
    {
      return 0;
    }

  return 1;
}

/* Read status register - NO bus reset, this is called inside wait loops */

static uint8_t flash_read_status(void)
{
  uint8_t status;

  flash_cs_select();
  flash_xfer(SPIFLASH_CMD_READ_STATUS);
  status = flash_xfer(0xff);
  flash_cs_deselect();

  return status;
}

static void flash_write_enable(void)
{
  flash_cs_select();
  flash_xfer(SPIFLASH_CMD_WRITE_ENABLE);
  flash_cs_deselect();
  up_udelay(1);
}

static int flash_wait_ready(FAR const char *op)
{
  uint8_t status = 0xff;
  int i;

  for (i = 0; i < 500; i++)
    {
      status = flash_read_status();
      if ((status & SPIFLASH_STATUS_WIP) == 0)
        {
          return OK;
        }

      up_mdelay(10);
    }

  printf("[SPIFLASH] %s timeout, status=0x%02x\n", op, status);
  return -ETIMEDOUT;
}

static int flash_sector_erase(uint32_t addr)
{
  flash_write_enable();

  flash_cs_select();
  flash_xfer(SPIFLASH_CMD_SECTOR_ERASE_4K);
  flash_xfer((uint8_t)(addr >> 16));
  flash_xfer((uint8_t)(addr >> 8));
  flash_xfer((uint8_t)addr);
  flash_cs_deselect();

  return flash_wait_ready("erase");
}

static int flash_page_program(uint32_t addr, FAR const uint8_t *data,
                              uint32_t len)
{
  uint32_t i;

  flash_write_enable();

  flash_cs_select();
  flash_xfer(SPIFLASH_CMD_PAGE_PROGRAM);
  flash_xfer((uint8_t)(addr >> 16));
  flash_xfer((uint8_t)(addr >> 8));
  flash_xfer((uint8_t)addr);

  for (i = 0; i < len; i++)
    {
      flash_xfer(data[i]);
    }

  flash_cs_deselect();

  return flash_wait_ready("program");
}

static void flash_read_data(uint32_t addr, FAR uint8_t *data, uint32_t len)
{
  uint32_t i;

  flash_cs_select();
  flash_xfer(SPIFLASH_CMD_READ_DATA);
  flash_xfer((uint8_t)(addr >> 16));
  flash_xfer((uint8_t)(addr >> 8));
  flash_xfer((uint8_t)addr);

  for (i = 0; i < len; i++)
    {
      data[i] = flash_xfer(0xff);
    }

  flash_cs_deselect();
}

static uint32_t flash_capacity_bytes(const uint8_t id[3])
{
  if (id[2] < 16 || id[2] > 30)
    {
      return 0;
    }

  return (uint32_t)(1UL << id[2]);
}

/****************************************************************************
 * Private Functions - Flash Data Operations
 ****************************************************************************/

static void spiflash_print_stored_data(uint32_t base_addr)
{
  uint8_t magic;
  uint8_t count;
  uint8_t buf[2];
  uint16_t lux_x100;
  int i;

  printf("[SPIFLASH] ---- Stored Light Data in Flash ----\n");

  flash_read_data(base_addr + FLASH_MAGIC_ADDR, &magic, 1);
  printf("[SPIFLASH] Magic: 0x%02X (%s)\n",
         magic, (magic == FLASH_MAGIC_VALUE) ? "valid" : "invalid/empty");

  if (magic != FLASH_MAGIC_VALUE)
    {
      printf("[SPIFLASH] No valid data stored yet\n");
      printf("[SPIFLASH] ---- End ----\n\n");
      return;
    }

  flash_read_data(base_addr + FLASH_COUNT_ADDR, &count, 1);
  printf("[SPIFLASH] Stored readings: %u\n", count);

  if (count > FLASH_MAX_READINGS)
    {
      count = FLASH_MAX_READINGS;
    }

  for (i = 0; i < count; i++)
    {
      flash_read_data(base_addr + FLASH_DATA_ADDR + i * 2, buf, 2);
      lux_x100 = ((uint16_t)buf[0] << 8) | buf[1];
      printf("[SPIFLASH]   [%d] %u.%02u lux\n",
             i, lux_x100 / 100, lux_x100 % 100);
    }

  printf("[SPIFLASH] ---- End ----\n\n");
}

static int spiflash_save_readings(uint32_t base_addr,
                                  const uint16_t *readings, int count)
{
  uint8_t buf[FLASH_MAX_READINGS * 2];
  uint8_t verify_buf[2];
  uint16_t stored;
  uint8_t count_u8;
  int i;
  int ret;

  if (count > FLASH_MAX_READINGS)
    {
      count = FLASH_MAX_READINGS;
    }

  count_u8 = (uint8_t)count;

  for (i = 0; i < count; i++)
    {
      buf[i * 2]     = (uint8_t)(readings[i] >> 8);
      buf[i * 2 + 1] = (uint8_t)(readings[i] & 0xff);
    }

  /* Reset bus before flash operations */

  spi2_bus_reset();
  up_mdelay(10);

  /* Erase sector */

  printf("[SPIFLASH] Erasing sector 0x%06X...\n",
         (unsigned int)(base_addr & ~(SPIFLASH_SECTOR_SIZE - 1)));
  ret = flash_sector_erase(base_addr & ~(SPIFLASH_SECTOR_SIZE - 1));
  if (ret < 0)
    {
      printf("[SPIFLASH] ERROR: erase failed: %d\n", ret);
      return ret;
    }

  up_mdelay(50);

  /* Write count */

  ret = flash_page_program(base_addr + FLASH_COUNT_ADDR, &count_u8, 1);
  if (ret < 0)
    {
      printf("[SPIFLASH] ERROR: write count: %d\n", ret);
      return ret;
    }

  /* Write data */

  ret = flash_page_program(base_addr + FLASH_DATA_ADDR, buf, count * 2);
  if (ret < 0)
    {
      printf("[SPIFLASH] ERROR: write data: %d\n", ret);
      return ret;
    }

  /* Write magic last */

  buf[0] = FLASH_MAGIC_VALUE;
  ret = flash_page_program(base_addr + FLASH_MAGIC_ADDR, buf, 1);
  if (ret < 0)
    {
      printf("[SPIFLASH] ERROR: write magic: %d\n", ret);
      return ret;
    }

  /* Verify */

  for (i = 0; i < count; i++)
    {
      flash_read_data(base_addr + FLASH_DATA_ADDR + i * 2, verify_buf, 2);
      stored = ((uint16_t)verify_buf[0] << 8) | verify_buf[1];
      if (stored != readings[i])
        {
          printf("[SPIFLASH] ERROR: verify[%d] mismatch: "
                 "expected %u, got %u\n", i, readings[i], stored);
          return -EIO;
        }
    }

  printf("[SPIFLASH] Saved and verified %d readings\n", count);
  return OK;
}

/****************************************************************************
 * Private Functions - Read Light Sensor and Save
 ****************************************************************************/

static int read_light_and_save(uint32_t flash_addr)
{
  uint16_t readings[FLASH_MAX_READINGS];
  uint16_t lux_x100;
  int ret;
  int i;
  int valid = 0;

  printf("\n[SPIFLASH] === KEY1 pressed: reading BH1750 x%d ===\n",
         FLASH_MAX_READINGS);

  ret = bh1750_init();
  if (ret < 0)
    {
      printf("[SPIFLASH] BH1750 init failed, aborting\n");
      return ret;
    }

  for (i = 0; i < FLASH_MAX_READINGS; i++)
    {
      usleep(300000);

      ret = bh1750_read_lux(&lux_x100);
      if (ret < 0)
        {
          printf("[SPIFLASH]   [%d] BH1750 read failed: %d\n", i, ret);
          readings[i] = 0;
        }
      else
        {
          readings[i] = lux_x100;
          valid++;
          printf("[SPIFLASH]   [%d] %u.%02u lux\n",
                 i, lux_x100 / 100, lux_x100 % 100);
        }
    }

  if (valid == 0)
    {
      printf("[SPIFLASH] All readings failed, skip save\n");
      return -EIO;
    }

  printf("[SPIFLASH] Saving readings to SPI Flash...\n");
  ret = spiflash_save_readings(flash_addr, readings, FLASH_MAX_READINGS);
  if (ret < 0)
    {
      printf("[SPIFLASH] Save failed!\n");
      return ret;
    }

  printf("[SPIFLASH] Verifying flash contents:\n");
  spiflash_print_stored_data(flash_addr);

  return OK;
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

int test_spiflash(void)
{
  uint8_t id[3] =
  {
    0
  };

  uint32_t capacity;
  uint32_t sector_addr;
  uint32_t flash_addr;
  bool key1_last;
  bool key2_last;
  int ret;

  printf("[SPIFLASH] === SPI Flash + BH1750 Light Sensor Test ===\n");
  printf("[SPIFLASH] I2C1: SCL=GPIO50 SDA=GPIO51\n");
  printf("[SPIFLASH] BH1750 addr=0x%02X/0x%02X\n",
         BH1750_ADDR_LOW, BH1750_ADDR_HIGH);
  printf("[SPIFLASH] SPI Flash CS=GPIO%d\n", FLASH_CS_PIN);
  printf("[SPIFLASH] KEY1=GPIO%d: read light x5, save to flash\n",
         KEY1_GPIO);
  printf("[SPIFLASH] KEY2=GPIO%d: exit\n\n", KEY2_GPIO);

  /* Initialize GPIO */

  g_fd_flash_cs = gpio_open_output(FLASH_CS_PIN);
  if (g_fd_flash_cs < 0)
    {
      return g_fd_flash_cs;
    }

  g_fd_key1 = gpio_open_input(KEY1_GPIO);
  if (g_fd_key1 < 0)
    {
      close(g_fd_flash_cs);
      return g_fd_key1;
    }

  g_fd_key2 = gpio_open_input(KEY2_GPIO);
  if (g_fd_key2 < 0)
    {
      close(g_fd_key1);
      close(g_fd_flash_cs);
      return g_fd_key2;
    }

  printf("[SPIFLASH] GPIO: FL_CS=GPIO%d KEY1=GPIO%d KEY2=GPIO%d\n",
         FLASH_CS_PIN, KEY1_GPIO, KEY2_GPIO);

  /* Open I2C1 */

  g_i2c_fd = open(I2C1_PATH, O_RDWR);
  if (g_i2c_fd < 0)
    {
      printf("[SPIFLASH] ERROR: open %s: errno=%d\n", I2C1_PATH, errno);
      ret = -errno;
      goto cleanup_gpio;
    }

  printf("[SPIFLASH] I2C1 opened, fd=%d\n", g_i2c_fd);

  /* Init SPI2 */

  ret = spi2_init();
  if (ret < 0)
    {
      printf("[SPIFLASH] ERROR: SPI2 init: %d\n", ret);
      goto cleanup;
    }

  printf("[SPIFLASH] SPI2 init OK (1MHz, mode 0)\n");
  up_mdelay(20);

  /* Reset flash in case it was stuck from a previous run */

  spi2_bus_reset();
  flash_software_reset();
  spi2_bus_reset();

  /* Detect flash */

  flash_read_jedec_id(id);
  printf("[SPIFLASH] Flash JEDEC ID: %02X %02X %02X\n",
         id[0], id[1], id[2]);

  if (!flash_id_valid(id))
    {
      printf("[SPIFLASH] ERROR: No valid SPI Flash detected!\n");
      ret = -ENODEV;
      goto cleanup;
    }

  capacity = flash_capacity_bytes(id);
  if (capacity >= SPIFLASH_SECTOR_SIZE * 2)
    {
      sector_addr = capacity - SPIFLASH_SECTOR_SIZE;
    }
  else
    {
      sector_addr = 0;
    }

  flash_addr = sector_addr + SPIFLASH_TEST_OFFSET;

  if (capacity != 0)
    {
      printf("[SPIFLASH] Flash capacity: %u bytes (%u KB)\n",
             capacity, capacity / 1024);
    }

  printf("[SPIFLASH] Flash status: 0x%02X\n", flash_read_status());
  printf("[SPIFLASH] Sector: 0x%06X, data addr: 0x%06X\n\n",
         (unsigned int)sector_addr, (unsigned int)flash_addr);

  /* Read current flash contents */

  printf("[SPIFLASH] Current flash contents:\n");
  spiflash_print_stored_data(flash_addr);

  /* Key init */

  key1_last = gpio_read(g_fd_key1);
  key2_last = gpio_read(g_fd_key2);
  printf("[SPIFLASH] KEY1(GPIO%d) ready, waiting...\n", KEY1_GPIO);
  printf("[SPIFLASH] KEY2(GPIO%d) press to exit\n\n", KEY2_GPIO);

  /* Main loop */

  while (1)
    {
      if (key_scan(g_fd_key1, &key1_last))
        {
          ret = read_light_and_save(flash_addr);
          if (ret < 0)
            {
              printf("[SPIFLASH] read_light_and_save failed: %d\n", ret);
            }

          printf("\n[SPIFLASH] Waiting for next KEY1...\n\n");
        }

      if (key_scan(g_fd_key2, &key2_last))
        {
          printf("\n[SPIFLASH] KEY2 pressed, exiting...\n");
          break;
        }

      up_mdelay(KEY_SCAN_DELAY_MS);
    }

  ret = OK;

cleanup:
  spi2_bus_reset();
  close(g_i2c_fd);
cleanup_gpio:
  close(g_fd_key2);
  close(g_fd_key1);
  close(g_fd_flash_cs);

  printf("[SPIFLASH] Test complete.\n");
  return ret;
}
