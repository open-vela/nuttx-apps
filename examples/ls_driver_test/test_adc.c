/****************************************************************************
 * apps/examples/ls_driver_test/test_adc.c
 *
 * MCP3204 ADC test - reads CH0 via hardware SPI2.
 *
 * SPI0 40Pin -> LS2K0300 SPI2 controller:
 *   SPI2_CLK  = GPIO64 (MAIN_FUNC)
 *   SPI2_MISO = GPIO65 (MAIN_FUNC)
 *   SPI2_MOSI = GPIO66 (MAIN_FUNC)
 *   CSN1      = GPIO67 (GPIO, software CS)
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
#include <stdint.h>

#include <nuttx/arch.h>
#include <nuttx/ioexpander/gpio.h>

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

/* APB ~200MHz, MCP3204 max 1.2MHz, use 1MHz */

#define SPI_APB_HZ              (200UL * 1000000UL)
#define SPI_CLOCK_HZ            1000000UL
#define SPI_WAIT_TIMEOUT_US     10000U

/* MCP3204 CS pin */

#define MCP3204_CS_PIN          67

/* MCP3204 parameters */

#define MCP3204_ADC_MAX         4095U
#define MCP3204_VREF_MV        3300U
#define MCP3204_SAMPLE_COUNT    8
#define MCP3204_DETECT_CH       0

/* Test rounds */

#define ADC_SAMPLE_COUNT        30

/* Register access - DR MUST use 8-bit access, 32-bit corrupts data! */

#define SPI_REG32(off) (*(volatile uint32_t *)(SPI2_BASE + (off)))
#define SPI_REG8(off)  (*(volatile uint8_t *)(SPI2_BASE + (off)))
#define CFG5_REG      (*(volatile uint32_t *)LS_GENERAL_CFG5_ADDR)

static int g_fd_cs = -1;

/****************************************************************************
 * SPI2 Hardware Functions
 ****************************************************************************/

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

  ioctl(g_fd_cs, GPIOC_WRITE, 1);
  up_udelay(1);
}

static int spi2_xfer_byte(uint8_t tx, uint8_t *rx)
{
  uint32_t val;
  uint32_t sr1;
  uint32_t i;

  val = SPI_REG32(LS_SPI_IO_CR1);
  val |= LS_SPI_CR1_SPE;
  SPI_REG32(LS_SPI_IO_CR1) = val;

  val = SPI_REG32(LS_SPI_IO_CR1);
  val &= ~(LS_SPI_CR1_CSTART | LS_SPI_CR1_AUTOSUS);
  SPI_REG32(LS_SPI_IO_CR1) = val;

  SPI_REG32(LS_SPI_IO_CR4) = 0;
  SPI_REG32(LS_SPI_IO_CR3) = 0;
  up_udelay(5);

  SPI_REG32(LS_SPI_IO_SR1) = LS_SPI_SR1_W1C_FLAGS;
  up_udelay(1);

  for (i = 0; i < 4; i++)
    {
      sr1 = SPI_REG32(LS_SPI_IO_SR1);
      if (sr1 & LS_SPI_SR1_RXE)
        {
          break;
        }

      (void)SPI_REG8(LS_SPI_IO_DR);
    }

  SPI_REG32(LS_SPI_IO_CR3) = 0;
  SPI_REG8(LS_SPI_IO_DR) = tx;

  val = SPI_REG32(LS_SPI_IO_CR1);
  val |= LS_SPI_CR1_CSTART;
  SPI_REG32(LS_SPI_IO_CR1) = val;

  /* Wait for EOT (End Of Transfer) first */

  for (i = 0; i < SPI_WAIT_TIMEOUT_US; i++)
    {
      sr1 = SPI_REG32(LS_SPI_IO_SR1);
      if (sr1 & LS_SPI_SR1_EOT)
        {
          SPI_REG32(LS_SPI_IO_SR1) = LS_SPI_SR1_EOT;
          break;
        }

      up_udelay(1);
    }

  if (i >= SPI_WAIT_TIMEOUT_US)
    {
      spi2_abort();
      return -1;
    }

  /* Then wait for RXA and read data */

  for (i = 0; i < SPI_WAIT_TIMEOUT_US; i++)
    {
      sr1 = SPI_REG32(LS_SPI_IO_SR1);
      if (sr1 & LS_SPI_SR1_RXA)
        {
          if (rx != NULL)
            {
              *rx = SPI_REG8(LS_SPI_IO_DR);
            }
          else
            {
              (void)SPI_REG8(LS_SPI_IO_DR);
            }

          val = SPI_REG32(LS_SPI_IO_CR1);
          val &= ~(LS_SPI_CR1_CSTART | LS_SPI_CR1_AUTOSUS);
          SPI_REG32(LS_SPI_IO_CR1) = val;
          return 0;
        }

      up_udelay(1);
    }

  spi2_abort();
  return -1;
}

static int mcp3204_init(void)
{
  int ret;
  uint32_t brint;

  printf("[ADC] MCP3204 hardware SPI2 init\n");

  ret = pinctrl_set_gpio_function(MCP3204_CS_PIN);
  if (ret < 0)
    {
      printf("[ADC] ERROR: pinctrl CS GPIO%d: %d\n", MCP3204_CS_PIN, ret);
      return ret;
    }

  g_fd_cs = open("/dev/gpio67", O_RDWR);
  if (g_fd_cs < 0)
    {
      printf("[ADC] ERROR: open CS GPIO%d: %d\n", MCP3204_CS_PIN, errno);
      return -errno;
    }

  ret = ioctl(g_fd_cs, GPIOC_SETPINTYPE, (unsigned long)GPIO_OUTPUT_PIN);
  if (ret < 0)
    {
      printf("[ADC] ERROR: CS set output: %d\n", errno);
      close(g_fd_cs);
      g_fd_cs = -1;
      return -errno;
    }

  ioctl(g_fd_cs, GPIOC_WRITE, 1);

  CFG5_REG |= (0x3u << 17);

  SPI_REG32(LS_SPI_IO_CFG3) = LS_SPI_CFG3_MSTR | LS_SPI_CFG3_DIE |
                               LS_SPI_CFG3_DOE | LS_SPI_CFG3_SSMODE_SW;
  SPI_REG32(LS_SPI_IO_CFG1) = LS_SPI_CFG1_DSIZE_8BIT;

  brint = SPI_APB_HZ / SPI_CLOCK_HZ;
  if (brint < 2) brint = 2;
  if (brint > 255) brint = 255;
  SPI_REG32(LS_SPI_IO_CFG2) = brint << LS_SPI_CFG2_BRINT_SHIFT;

  SPI_REG32(LS_SPI_IO_CR1) = LS_SPI_CR1_SPE;

  printf("[ADC] SPI2 init done, Vref=%u mV\n", MCP3204_VREF_MV);
  return 0;
}

static void mcp3204_deinit(void)
{
  if (g_fd_cs >= 0)
    {
      ioctl(g_fd_cs, GPIOC_WRITE, 1);
      close(g_fd_cs);
      g_fd_cs = -1;
    }
}

static int mcp3204_read_raw(uint8_t channel, uint16_t *raw)
{
  uint8_t rx_hi = 0;
  uint8_t rx_lo = 0;

  ioctl(g_fd_cs, GPIOC_WRITE, 0);
  up_udelay(1);

  if (spi2_xfer_byte(0x06, NULL) != 0 ||
      spi2_xfer_byte((uint8_t)(channel << 6), &rx_hi) != 0 ||
      spi2_xfer_byte(0x00, &rx_lo) != 0)
    {
      ioctl(g_fd_cs, GPIOC_WRITE, 1);
      return -1;
    }

  ioctl(g_fd_cs, GPIOC_WRITE, 1);
  up_udelay(1);

  /* Reject 0xff - MISO stuck high, no valid data */

  if (rx_hi == 0xff || rx_lo == 0xff)
    {
      return -1;
    }

  *raw = (uint16_t)((((uint16_t)rx_hi & 0x0fu) << 8) | rx_lo);
  return 0;
}

static int mcp3204_read_average(uint8_t channel, uint16_t *raw)
{
  uint32_t sum = 0;
  uint16_t value = 0;
  int ret;
  int i;
  int valid = 0;

  for (i = 0; i < MCP3204_SAMPLE_COUNT * 8; i++)
    {
      ret = mcp3204_read_raw(channel, &value);
      if (ret != 0)
        {
          up_mdelay(2);
          continue;
        }

      sum += value;
      valid++;
      if (valid >= MCP3204_SAMPLE_COUNT)
        {
          break;
        }

      up_mdelay(2);
    }

  if (valid == 0)
    {
      return -1;
    }

  *raw = (uint16_t)((sum + (valid / 2)) / valid);
  return 0;
}

static uint32_t mcp3204_raw_to_mv(uint16_t raw)
{
  return ((uint32_t)raw * MCP3204_VREF_MV + (MCP3204_ADC_MAX / 2)) /
         MCP3204_ADC_MAX;
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

int test_adc(void)
{
  int ret;
  int round;
  uint16_t raw;
  uint32_t mv;

  printf("[ADC] === MCP3204 ADC CH%d Test ===\n", MCP3204_DETECT_CH);

  ret = mcp3204_init();
  if (ret < 0)
    {
      printf("[ADC] ERROR: MCP3204 init: %d\n", ret);
      return ret;
    }

  for (round = 0; round < ADC_SAMPLE_COUNT; round++)
    {
      ret = mcp3204_read_average(MCP3204_DETECT_CH, &raw);
      if (ret != 0)
        {
          printf("[ADC] ERROR: CH%d read: %d\n", MCP3204_DETECT_CH, ret);
          raw = 0;
        }

      mv = mcp3204_raw_to_mv(raw);

      printf("[ADC] %3d: CH%d raw=%4u voltage=%u.%03u V\n",
             round + 1, MCP3204_DETECT_CH, raw, mv / 1000, mv % 1000);

      up_mdelay(500);
    }

  mcp3204_deinit();

  printf("[ADC] Done.\n");
  return OK;
}
