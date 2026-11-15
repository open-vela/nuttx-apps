/****************************************************************************
 * apps/examples/mmcdiag/mmcdiag_main.c
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
#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <stdlib.h>
#include <nuttx/cache.h>

/****************************************************************************
 * Public Function Prototypes
 ****************************************************************************/

int yuzuki_h616_sdhci_initialize(void);

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/* H616 SDMMC2 base */
#define SDMMC2_BASE 0x04022000UL
#define CCU_BASE 0x03001000UL

/* Registers */
#define GCTRL 0x00
#define CLKCR 0x04
#define TMOUT 0x08
#define WIDTH 0x0c
#define BLKSZ 0x10
#define BCNTR 0x14
#define CMDR 0x18
#define CARG 0x1c
#define RESP0 0x20
#define RESP1 0x24
#define RESP2 0x28
#define RESP3 0x2c
#define IMASK 0x30
#define MISTA 0x34
#define RINTR 0x38
#define STAS 0x3c
#define HWRST 0x78  /* card hardware reset (H616_SDXC_HWRST, active low) */
#define THLDC 0x100 /* card threshold control (H616_SDXC_THLDC) */
#define FIFO 0x200

#define SD2_CLK_CFG 0x838
#define SD_GATE_RESET 0x84c

#define CSDC_REG 0x54 /* card strobe / HS400 mode control */
#define NTSR_REG 0x5c /* new timing set (HS400 sampling) */
#define DSD_REG 0x10c /* eMMC4.5 DDR start-bit detect / HS400 enable */

/* Bits */
#define GCTRL_SOFT_RESET (1 << 0)
#define GCTRL_FIFO_RESET (1 << 1)
#define GCTRL_DMA_RESET (1 << 2)
#define GCTRL_INTERRUPT_ENABLE (1 << 4)
#define GCTRL_DMA_ENABLE (1 << 5)

#define CLKCR_MASK_DATA0 (1 << 31)
#define CLKCR_CARD_CLOCK_ON (1 << 16)

#define CMDR_START (1 << 31)
#define CMDR_UPCLK_ONLY (1 << 21)
#define CMDR_WAIT_PRE_OVER (1 << 13)
#define CMDR_SEND_INIT_SEQ (1 << 15)
#define CMDR_RESP_EXPECT (1 << 6)
#define CMDR_LONG_RESPONSE (1 << 7)
#define CMDR_CHECK_CRC (1 << 8)
#define CMDR_DATA_EXPECT (1 << 9)
#define CMDR_WRITE (1 << 10)

#define INT_COMMAND_DONE (1 << 2)
#define INT_RESP_ERROR (1 << 1)
#define INT_RESP_CRC_ERROR (1 << 6)
#define INT_RESP_TIMEOUT (1 << 8)

#define STAS_CARD_DATA_BUSY (1 << 9)

#define CCM_MMC_CTRL_ENABLE (1 << 31)
#define CCM_MMC_CTRL_OSCM24 (0x0 << 24)
#define CCM_MMC_CTRL_M(x) ((x) - 1)
#define CCM_MMC_CTRL_N(x) ((x) << 8)

#define DMAC 0x80
#define DLBA 0x84
#define IDST 0x88
#define IDIE 0x8c
#define CBCR 0x48
#define BBCR 0x4c

#define DMAC_SOFT_RESET (1 << 0)
#define DMAC_FIX_BURST (1 << 1)
#define DMAC_IDMA_ON (1 << 7)

/* vendor rtos-hal __mci_prepare_dma(): FTRGL = (BURST<<28)|(RXW<<16)|TXW */
#define FTRGL_VENDOR ((2u << 28) | (7u << 16) | 248u)

/* desc0: DIC(1) | LD(2) | FD(3) | CH(4) | ER(5) | OWN(31) */
#define DES0_DIC (1u << 1)
#define DES0_LD (1u << 2)
#define DES0_FD (1u << 3)
#define DES0_CH (1u << 4)
#define DES0_ER (1u << 5)
#define DES0_OWN (1u << 31)

/* EXT_CSD bytes as sent by the last CMD8 (SEND_EXT_CSD) read.  The card's
 * *persistent* configuration lives here: [179] PARTITION_CONFIG,
 * [183] BUS_WIDTH (0/1/2 = 1/4/8 bit), [185] HS_TIMING, [192] EXT_CSD_REV.
 */

#define EXT_BYTE(n)                                                          \
  ((unsigned)(((g_last_read[(n) >> 2] >> (((n) & 3u) << 3)) & 0xffu)))

/****************************************************************************
 * Private Data
 ****************************************************************************/

/* Which LBA the u_read17()/u_write17() helpers below read or write.  0x20000
 * is where boot.img lives; 0x20004 has distinctly different content, which
 * makes a "did the card really deliver this block?" check possible.
 */

static uint32_t g_read_lba = 0x00020000u;
static uint32_t g_read_cmd = 17u; /* u_read17: 17 = CMD17, 8 = CMD8 EXT_CSD */
static uint32_t g_last_read[128]; /* words the card actually sent last time */
static unsigned g_last_n = 0;
static int g_drv_style = 0; /* 1 = run the driver's pre-read steps */

/* 16-byte aligned so a single 64-byte dcache line covers descriptor+buffer
 * (mutually exclusive: one descriptor, 4 KB buffer).
 */

static uint8_t g_dmabuf[512] __attribute__((aligned(2048)));
static uint32_t *g_dmades = (uint32_t *)(void *)g_dmabuf; /* des at +0   */
static uint32_t *g_dmadat =
    (uint32_t *)(void *)(g_dmabuf + 16); /* data at +16 */

/****************************************************************************
 * Private Functions
 ****************************************************************************/

static inline uint32_t rd(uint32_t addr)
{
  return *(volatile uint32_t *)(uintptr_t)addr;
}

static inline void wr(uint32_t addr, uint32_t v)
{
  *(volatile uint32_t *)(uintptr_t)addr = v;
}

static void dump_regs(const char *tag)
{
  printf("[%s] GCTRL=%08x CLKCR=%08x WIDTH=%08x TMOUT=%08x FTRGL=%08x\n",
      tag,
         rd(SDMMC2_BASE + GCTRL), rd(SDMMC2_BASE + CLKCR),
         rd(SDMMC2_BASE + WIDTH), rd(SDMMC2_BASE + TMOUT),
         rd(SDMMC2_BASE + 0x40));
  printf("     CMDR=%08x CARG=%08x\n", rd(SDMMC2_BASE + CMDR),
         rd(SDMMC2_BASE + CARG));
  printf(
      "     RINTR=%08x MISTA=%08x IMASK=%08x STAS=%08x CCU838=%08x"
          "GATE=%08x\n",
      rd(SDMMC2_BASE + RINTR), rd(SDMMC2_BASE + MISTA),
      rd(SDMMC2_BASE + IMASK), rd(SDMMC2_BASE + STAS),
      rd(CCU_BASE + SD2_CLK_CFG), rd(CCU_BASE + SD_GATE_RESET));
  printf("     RESP0=%08x RESP1=%08x RESP2=%08x RESP3=%08x\n",
         rd(SDMMC2_BASE + RESP0), rd(SDMMC2_BASE + RESP1),
         rd(SDMMC2_BASE + RESP2), rd(SDMMC2_BASE + RESP3));
  printf("     PIO_PC_CFG0=%08x PIO_PC_CFG1=%08x PIO_PC_CFG2=%08x\n",
         rd(0x0300b000 + 0x48), rd(0x0300b000 + 0x4c),
             rd(0x0300b000 + 0x50));
}

static void clock_update(void)
{
  /* Mask DAT0, trigger clock update via CMDR, wait, unmask */

  uint32_t c = rd(SDMMC2_BASE + CLKCR);
  wr(SDMMC2_BASE + CLKCR, c | CLKCR_MASK_DATA0);
  wr(SDMMC2_BASE + CMDR, CMDR_START | CMDR_UPCLK_ONLY | CMDR_WAIT_PRE_OVER);
  volatile int t = 100000;
  while ((rd(SDMMC2_BASE + CMDR) & CMDR_START) && --t)
    ;
  c = rd(SDMMC2_BASE + CLKCR);
  wr(SDMMC2_BASE + CLKCR, (c & ~CLKCR_MASK_DATA0) | CLKCR_CARD_CLOCK_ON);
}

static void set_ccu_clock(uint32_t hz)
{
  uint32_t div;
  uint32_t n = 0;
  uint32_t m;
  uint32_t pll = CCM_MMC_CTRL_OSCM24;
  uint32_t pll_hz = 24000000UL;
  uint32_t regval;

  /* H616: gate=bit2, reset=bit18 in SD_GATE_RESET */

  regval = rd(CCU_BASE + SD_GATE_RESET);
  regval |= (1 << 18) | (1 << 2);
  wr(CCU_BASE + SD_GATE_RESET, regval);

  if (hz <= 24000000)
    {
      pll = CCM_MMC_CTRL_OSCM24;
      pll_hz = 24000000UL;
    }
  else
    {
      pll = (0x1 << 24);
      pll_hz = 1200000000UL;
    }

      /* PLL6X2 */

  div = (pll_hz + hz - 1) / hz;
  while (div > 16)
    {
      n++;
      div = (div + 1) / 2;
    }

  m = div;

  regval = CCM_MMC_CTRL_ENABLE | pll | CCM_MMC_CTRL_N(n) | CCM_MMC_CTRL_M(m);
  wr(CCU_BASE + SD2_CLK_CFG, regval);
}

static void controller_reset(void)
{
  wr(SDMMC2_BASE + GCTRL,
     GCTRL_SOFT_RESET | GCTRL_FIFO_RESET | GCTRL_DMA_RESET);
  volatile int t = 100000;
  while ((rd(SDMMC2_BASE + GCTRL) &
          (GCTRL_SOFT_RESET | GCTRL_FIFO_RESET | GCTRL_DMA_RESET)) &&
         --t)
    ;
  wr(SDMMC2_BASE + GCTRL, GCTRL_INTERRUPT_ENABLE);
  wr(SDMMC2_BASE + RINTR, 0xffffffff); /* clear interrupts */
  wr(SDMMC2_BASE + WIDTH, 0);          /* 1-bit */
}

static int send_cmd(uint32_t cmdidx, uint32_t arg, uint32_t cmdr_bits)
{
  wr(SDMMC2_BASE + CARG, arg);
  wr(SDMMC2_BASE + RINTR, 0xffffffff);
  wr(SDMMC2_BASE + CMDR, cmdr_bits | cmdidx | CMDR_START);

  volatile int t = 5000000;
  uint32_t mista;
  while (t--)
    {
      mista = rd(SDMMC2_BASE + MISTA);
      if (mista & INT_COMMAND_DONE)
        break;
      if (mista & (INT_RESP_ERROR | INT_RESP_CRC_ERROR | INT_RESP_TIMEOUT))
        {
          printf("cmd%d err MISTA=%08x\n", cmdidx, mista);
          wr(SDMMC2_BASE + RINTR, 0xffffffff);
          return -1;
        }
    }

  if (t <= 0)
    {
      printf("cmd%d TIMEOUT MISTA=%08x\n", cmdidx, rd(SDMMC2_BASE + MISTA));
      return -2;
    }

  wr(SDMMC2_BASE + RINTR, 0xffffffff);
  return 0;
}

static void controller_reset_driverstyle(void)
{
  /* Replicate h616_reset(): CCU clock enable + soft reset + FTRGL +
   * 1-bit + ID clock via CLKCR/UPCLK.
   */

  set_ccu_clock(400000);
  wr(SDMMC2_BASE + GCTRL,
     GCTRL_SOFT_RESET | GCTRL_FIFO_RESET | GCTRL_DMA_RESET);
  volatile int t = 100000;
  while ((rd(SDMMC2_BASE + GCTRL) &
          (GCTRL_SOFT_RESET | GCTRL_FIFO_RESET | GCTRL_DMA_RESET)) &&
         --t)
    ;
  wr(SDMMC2_BASE + GCTRL, GCTRL_INTERRUPT_ENABLE);
  wr(SDMMC2_BASE + 0x40, 0x20070008); /* FTRGL */
  wr(SDMMC2_BASE + WIDTH, 0);
  clock_update(); /* CLKCR div=0 + UPCLK trigger */
  wr(SDMMC2_BASE + RINTR, 0xffffffff);
}

static int send_cmd_driverstyle(uint32_t cmdidx, uint32_t arg,
                                uint32_t cmdr_bits)
{
  /* Replicate the h616_sdhci.c driver sequence: set IMASK (interrupt
   * enable), clear RINTR, issue command, poll MISTA.  Same as send_cmd
   * but with IMASK written first.
   */

  wr(SDMMC2_BASE + IMASK, INT_COMMAND_DONE | INT_RESP_ERROR |
                              INT_RESP_CRC_ERROR | INT_RESP_TIMEOUT);
  wr(SDMMC2_BASE + CARG, arg);
  wr(SDMMC2_BASE + RINTR, 0xffffffff);
  wr(SDMMC2_BASE + CMDR, cmdr_bits | cmdidx | CMDR_START);

  volatile int t = 5000000;
  uint32_t mista;
  while (t--)
    {
      mista = rd(SDMMC2_BASE + MISTA);
      if (mista & INT_COMMAND_DONE)
        break;
      if (mista & (INT_RESP_ERROR | INT_RESP_CRC_ERROR | INT_RESP_TIMEOUT))
        {
          printf("driverstyle cmd%d err MISTA=%08x\n", cmdidx, mista);
          wr(SDMMC2_BASE + RINTR, 0xffffffff);
          wr(SDMMC2_BASE + IMASK, 0);
          return -1;
        }
    }

  if (t <= 0)
    {
      printf("driverstyle cmd%d TIMEOUT MISTA=%08x\n", cmdidx,
             rd(SDMMC2_BASE + MISTA));
    }

  wr(SDMMC2_BASE + RINTR, 0xffffffff);
  wr(SDMMC2_BASE + IMASK, 0);
  return 0;
}

/* ---------- reliable command layer (polls RINTR, like the driver)
 */

static int send_cmd_ok(uint32_t cmdidx, uint32_t arg, uint32_t bits)
{
  volatile int t = 3000000;
  uint32_t r;

  wr(SDMMC2_BASE + RINTR, 0xffffffff);
  wr(SDMMC2_BASE + CARG, arg);
  wr(SDMMC2_BASE + CMDR, bits | (cmdidx & 0x3f) | CMDR_START);

  while (t--)
    {
      r = rd(SDMMC2_BASE + RINTR);
      if (r & INT_COMMAND_DONE)
        {
          wr(SDMMC2_BASE + RINTR, 0xffffffff);
          return 0;
        }

      if (r & (INT_RESP_ERROR | INT_RESP_CRC_ERROR | INT_RESP_TIMEOUT))
        {
          wr(SDMMC2_BASE + RINTR, 0xffffffff);
          return -1;
        }
    }

  return -2;
}

static void card_init(void)
{
  send_cmd_ok(0, 0xf0f0f0f0, CMDR_SEND_INIT_SEQ);
  send_cmd_ok(0, 0, CMDR_SEND_INIT_SEQ);
  send_cmd_ok(1, 0x40300000, CMDR_RESP_EXPECT);
  send_cmd_ok(2, 0, CMDR_RESP_EXPECT | CMDR_LONG_RESPONSE | CMDR_CHECK_CRC);
  send_cmd_ok(3, 0x00010000, CMDR_RESP_EXPECT | CMDR_CHECK_CRC);
  send_cmd_ok(9, 0x00010000,
              CMDR_RESP_EXPECT | CMDR_LONG_RESPONSE | CMDR_CHECK_CRC);
  send_cmd_ok(7, 0x00010000, CMDR_RESP_EXPECT | CMDR_CHECK_CRC);
  send_cmd_ok(16, 512, CMDR_RESP_EXPECT | CMDR_CHECK_CRC);
}

/* Forward decl of the shared descriptor (defined here so no cache concerns)
 */

/* parameterized data-read probe:
 *   mmcdiag t <bit31> <width> <flags>
 * flags: 1=write THLD  2=no WAIT_PRE_OVER  4=fifo+dma reset first
 *        8=CMD18       16=program IDMAC    32=clkcr div2
 */

/* ---------- DMAC / IDMAC data path probe ---------- */

/* One CMD8 (SEND_EXT_CSD) 512-byte data read with a fully programmed IDMAC.
 *   mode 0 = GCTRL AHB (bit31=1), 1 = GCTRL DMA (bit31=0)
 *   desc 0 = vendor/Linux/U-Boot descriptor, 1 = DIC kept set (U-Boot style)
 *   order 0 = burst|on -> IDIE -> DLBA, 1 = IDIE -> burst|on -> DLBA,
 *           2 = burst|on -> IDIE -> FTRGL(0x300f00f0) -> DLBA
 */

/* ---------- full register dump at the data-phase failure point ---------- */

static void regdump_at_failure(void)
{
  static const char *nm[32] = {
      "GCTRL", "CLKCR", "TMOUT", "WIDTH", "BLKSZ", "BCNTR", "CMDR",  "CARG",
      "RESP0", "RESP1", "RESP2", "RESP3", "IMASK", "MISTA", "RINTR", "STAS",
      "FTRGL", "FUNS",  "CBCR",  "BBCR",  "DBGC",  "CSDC",  "A12A",  "NTSR",
      "rsv",   "rsv",   "rsv",   "rsv",   "rsv",   "rsv",   "HWRST", "rsv1"
      };

  int i;
  volatile int t;

  /* 400 kHz ID mode, 1 bit, exactly like the failing probe */

  wr(SDMMC2_BASE + GCTRL,
     GCTRL_SOFT_RESET | GCTRL_FIFO_RESET | GCTRL_DMA_RESET);
  t = 100000;
  while ((rd(SDMMC2_BASE + GCTRL) &
          (GCTRL_SOFT_RESET | GCTRL_FIFO_RESET | GCTRL_DMA_RESET)) &&
         --t)
    ;
  wr(SDMMC2_BASE + GCTRL, GCTRL_INTERRUPT_ENABLE);
  set_ccu_clock(400000);
  clock_update();
  wr(SDMMC2_BASE + WIDTH, 0);
  wr(SDMMC2_BASE + TMOUT, 0xffffff40);
  wr(SDMMC2_BASE + 0x40, 0x20070008);
  wr(SDMMC2_BASE + IMASK, 0);
  wr(SDMMC2_BASE + RINTR, 0xffffffff);
  card_init();

  wr(SDMMC2_BASE + BLKSZ, 512);
  wr(SDMMC2_BASE + BCNTR, 512);
  wr(SDMMC2_BASE + RINTR, 0xffffffff);
  wr(SDMMC2_BASE + CARG, 0);
  wr(SDMMC2_BASE + CMDR, CMDR_START | CMDR_RESP_EXPECT | CMDR_CHECK_CRC |
                             CMDR_DATA_EXPECT | CMDR_WAIT_PRE_OVER | 8);
  t = 2000000;
  while (t-- && !(rd(SDMMC2_BASE + RINTR) &
                  ((1u << 3) | (1u << 7) | (1u << 9) | (1u << 8))))
    ;

  printf("=== CMD8 data-phase failure register dump ===\n");
  for (i = 0; i < 32; i += 4)
    printf("  +%02x %-6s=%08x %-6s=%08x %-6s=%08x %-6s=%08x\n", i * 4, nm[i],
           rd(SDMMC2_BASE + i * 4), nm[i + 1], rd(SDMMC2_BASE + (i + 1) * 4),
           nm[i + 2], rd(SDMMC2_BASE + (i + 2) * 4), nm[i + 3],
           rd(SDMMC2_BASE + (i + 3) * 4));
  printf("  +80 DMAC  =%08x DLBA  =%08x IDST  =%08x IDIE  =%08x\n",
         rd(SDMMC2_BASE + DMAC), rd(SDMMC2_BASE + DLBA),
         rd(SDMMC2_BASE + IDST), rd(SDMMC2_BASE + IDIE));
  printf("  +90 CHDA  =%08x CBDA  =%08x\n", rd(SDMMC2_BASE + 0x90),
         rd(SDMMC2_BASE + 0x94));
  printf("  +100 THLD  =%08x %08x %08x\n", rd(SDMMC2_BASE + 0x100),
         rd(SDMMC2_BASE + 0x104), rd(SDMMC2_BASE + 0x108));
  printf("  CCU838=%08x CCU84c=%08x\n", rd(CCU_BASE + SD2_CLK_CFG),
         rd(CCU_BASE + SD_GATE_RESET));

  /* PIO pin mux for port C.  H616 mmc2 = PC0,PC1,PC5,PC6,PC8,PC9,PC10,PC11,
   * PC13,PC14,PC15,PC16 and the "mmc2" mux value is 3.  Port C CFG registers
   * are at PIO + 0x48/0x4c/0x50 (bank*0x24 + index*4, 4 bits per pin).  If
   * these read 0 the DAT lines are still GPIO inputs and the card physically
   * cannot drive data to the controller -- which matches "command works, no
   * data, no CRC error, FIFO empty" exactly.
   */

    {
      static const uintptr_t pio = 0x0300b000;
      uint32_t c0 = *(volatile uint32_t *)(pio + 0x48);
      uint32_t c1 = *(volatile uint32_t *)(pio + 0x4c);
      uint32_t c2 = *(volatile uint32_t *)(pio + 0x50);
      uint32_t pull0 = *(volatile uint32_t *)(pio + 0x6c);
      uint32_t pull1 = *(volatile uint32_t *)(pio + 0x70);
      uint32_t drv0 = *(volatile uint32_t *)(pio + 0x5c);
      char line[128];
      int k;

      printf("  PC_CFG0=%08x PC_CFG1=%08x PC_CFG2=%08x\n", c0, c1, c2);
      printf("  PC_PULL0=%08x PC_PULL1=%08x PC_DRV0=%08x\n", pull0, pull1,
             drv0);
      line[0] = 0;
      for (k = 0; k <= 16; k++)
        {
          uint32_t v = (k < 8) ? c0 : (k < 16 ? c1 : c2);
          uint32_t mux = (v >> ((k & 7) * 4)) & 0xf;
          char tmp[12];
          if (mux == 0)
            {
              continue;
            }

          snprintf(tmp, sizeof(tmp), "PC%d=%u ", k, (unsigned)mux);
          if (strlen(line) + strlen(tmp) < sizeof(line) - 1)
            {
              strcat(line, tmp);
            }
        }

      printf("  muxed pins: %s\n", line[0] ? line : "(none)");
    }

  wr(SDMMC2_BASE + RINTR, 0xffffffff);
}

/* ---------- card speed-mode probe ----------
 * The eMMC bus width AND HS_TIMING are PERSISTENT CARD settings (EXT_CSD
 * bytes 183 / 185) that U-Boot leaves programmed.  If U-Boot left the card
 * in
 * DDR52 (the board dts asks for mmc-ddr-1_8v) while the controller runs SDR,
 * READs fail (the read direction samples on both edges) exactly where WRITEs
 * still work (a write does not sample).  That is the observed signature:
 * CMD24 write moves data (BBCR=0x200), every read transfers nothing
 * (CBCR=0).
 *
 * CMD6 (SWITCH) carries no data lines, so it always works -- this lets us
 * set
 * the CARD's mode and then try a read.
 *   mmcdiag v <idx>      idx 185 = HS_TIMING, 183 = BUS_WIDTH
 *   mmcdiag v <idx> <val>   set one, then CMD18 read
 *   mmcdiag v a          run the whole matrix below
 */

struct vmodes
{
  const char *name;
  int hs;       /* EXT_CSD byte 185 value (-1 = leave card alone) */
  int bw;       /* EXT_CSD byte 183 value (-1 = leave card alone) */
  int ddr;      /* GCTRL bit10 */
  uint32_t clk; /* CCU module clock */
};

/* ---------- card-state probe ----------
 * Every data command fails with CBCR=0 while non-data commands are perfect.
 * CMD13 (SEND_STATUS) returns R1 whose bits [12:9] are CURRENT_STATE, so use
 * it to see what the CARD thinks it is doing before and after a failed read.
 *   4=tran (ready for data) 5=data 6=rcv 7=prg 3=stby 0=idle
 *   mmcdiag s
 */

static void s_status(const char *tag)
{
  static const char *st[] =
  {
    "idle", "ready", "ident", "stby", "tran", "data",
    "rcv",  "prg",   "dis",   "btst", "slp"
  };

  uint32_t r = 0;
  volatile int t;

  wr(SDMMC2_BASE + RINTR, 0xffffffff);
  wr(SDMMC2_BASE + CARG, 0x00010000); /* RCA << 16 */
  wr(SDMMC2_BASE + CMDR,
      CMDR_START | CMDR_RESP_EXPECT | CMDR_CHECK_CRC | 13);
  t = 2000000;
  while (t--)
    {
      r = rd(SDMMC2_BASE + RINTR);
      if (r & INT_COMMAND_DONE)
        break;
      if (r & (INT_RESP_ERROR | INT_RESP_CRC_ERROR | INT_RESP_TIMEOUT))
        break;
    }

    {
      uint32_t resp = rd(SDMMC2_BASE + RESP0);
      uint32_t cs = (resp >> 9) & 0xf;
      printf("  [%s] R1=%08x CURRENT_STATE=%u(%s)"
          "RINTR=%08x\n", tag, resp, cs,
             cs < 11 ? st[cs] : "?", r);
      wr(SDMMC2_BASE + RINTR, 0xffffffff);
    }
}

static void s_probe(void)
{
  uint32_t rintr = 0;
  volatile int t;

  printf("=== card-state probe ===\n");

  wr(SDMMC2_BASE + RINTR, 0xffffffff);
  wr(SDMMC2_BASE + IMASK, 0);

  card_init(); /* CMD0..CMD16, leaves card in TRAN */
  s_status("after card_init");

  wr(SDMMC2_BASE + IMASK, 0);
  s_status("again");

  /* now one CMD17 and look at the card state right after */

  wr(SDMMC2_BASE + GCTRL,
     (rd(SDMMC2_BASE + GCTRL) & ~GCTRL_DMA_ENABLE) | GCTRL_INTERRUPT_ENABLE);
  wr(SDMMC2_BASE + BLKSZ, 512);
  wr(SDMMC2_BASE + BCNTR, 512);
  wr(SDMMC2_BASE + RINTR, 0xffffffff);
  wr(SDMMC2_BASE + CARG, g_read_lba);
  wr(SDMMC2_BASE + CMDR, CMDR_START | CMDR_RESP_EXPECT | CMDR_CHECK_CRC |
                             CMDR_DATA_EXPECT | CMDR_WAIT_PRE_OVER | 17);
  t = 4000000;
  while (t--)
    {
      rintr = rd(SDMMC2_BASE + RINTR);
      if (rintr & ((1u << 3) | (1u << 7) | (1u << 9) | (1u << 11)))
        break;
    }

  printf("  CMD17 RINTR=%08x STAS=%08x CBCR=%08x BBCR=%08x\n", rintr,
         rd(SDMMC2_BASE + STAS), rd(SDMMC2_BASE + CBCR),
         rd(SDMMC2_BASE + BBCR));
  wr(SDMMC2_BASE + RINTR, 0xffffffff);

  s_status("after CMD17");

  /* a second CMD13 tells whether the card is still responsive at all */

  s_status("after CMD17 #2");

  /* CMD12 STOP_TRANSMISSION: if the card went to DATA state for the read and
   * never left, this should bring it back to TRAN.  If CMD13 then answers
   * again, the card was merely stuck mid-transfer (recoverable), not dead.
   */

  wr(SDMMC2_BASE + RINTR, 0xffffffff);
  wr(SDMMC2_BASE + CARG, 0);
  wr(SDMMC2_BASE + CMDR, CMDR_START | CMDR_RESP_EXPECT | CMDR_CHECK_CRC |
                             0x4000 /* STOP_ABORT_CMD bit14 */ | 12);
  t = 2000000;
  while (t--)
    {
      if (rd(SDMMC2_BASE + RINTR) & (INT_COMMAND_DONE | INT_RESP_TIMEOUT))
        break;
    }

  printf("  CMD12 STOP_TRANSMISSION RINTR=%08x\n", rd(SDMMC2_BASE + RINTR));
  wr(SDMMC2_BASE + RINTR, 0xffffffff);
  s_status("after CMD12");

  /* Also try: gate the card clock off and on again (the standard trick to
   * abort a stuck data phase without a full card reset).
   */

    {
      uint32_t clk = rd(SDMMC2_BASE + CLKCR);
      wr(SDMMC2_BASE + CLKCR, clk & ~0x00010000u); /* CARD_CLOCK_OFF */
        {
          volatile int d = 200000;
          while (d--)
            ;
        }

      wr(SDMMC2_BASE + CLKCR, clk);
        {
          volatile int d = 200000;
          while (d--)
            ;
        }

      printf("  clock gate off/on done,"
          "CLKCR=%08x\n", rd(SDMMC2_BASE + CLKCR));
    }

  s_status("after clock gate");

  /* Last: does GCTRL bit31 (ACCESS_BY_AHB) actually stick?  Done last so it
   * cannot perturb the card checks above.  Vendor: AHB = BIT(31),
   * DMAEnb = BIT(5).
   */

    {
      uint32_t save = rd(SDMMC2_BASE + GCTRL);
      printf("  GCTRL entry      =%08x\n", save);
      wr(SDMMC2_BASE + GCTRL, 0x80000000u);
      printf("  GCTRL =bit31 only=%08x  (bit31 stuck: %s)\n",
             rd(SDMMC2_BASE + GCTRL),
             (rd(SDMMC2_BASE + GCTRL) & 0x80000000u) ? "YES" : "NO");
      wr(SDMMC2_BASE + GCTRL, save);
    }
}

/* ---------- bus-width hypothesis probe ----------
 * The card's bus width is a PERSISTENT card setting (EXT_CSD byte 183) that
 * U-Boot left at 8-bit, while the openvela controller runs 1-bit.  Only data
 * commands use the DAT lines, which is exactly where the failure is
 * (RESP_TIMEOUT on CMD8/CMD17 while every non-data command works).
 * CMD6 (SWITCH) carries no data, so it can always be sent.
 *   mmcdiag w <cardwidth> <hostwidth>
 *     cardwidth: 183 = leave card alone, 1/2/3 = set EXT_CSD BUS_WIDTH
 *     hostwidth: WIDTH register value (0=1bit, 1=4bit, 2=8bit)
 */

/* ---------- command sequence probe ----------
 * Sends CMD13 (SEND_STATUS) around the failing data command so we can see
 * which command FIRST goes unanswered and whether the card is still alive
 * afterwards.  CMD13 is an R1 status read with no data: it answers with the
 * current card state, so it distinguishes "card not selected / dead" from
 * "data-command-specific controller failure".
 */

/* ---------- U-Boot replica probe ----------
 * The only working reference on this board is U-Boot reading the same eMMC.
 * Captured U-Boot state after a successful 512-byte read:
 *   GCTRL=20000000 (bit31=0 -> DMA access, bit4 interrupt enable clear)
 *   WIDTH=00000002 (8-bit)  TMOUT=ffffffff  FTRGL=300f00f0
 *   DMAC=00000300  DLBA=1efb2e70 (own descriptor)  CBCR=00000200
 *   STAS=00008d06 (no DATA_FSM_BUSY)
 * This probe reproduces that state as closely as possible and issues CMD17
 * (READ_BLOCK, no long data-select bootstrap needed) on a freshly
 * initialised
 * card, so the card is in TRAN state rather than stuck in DATA state.
 *   mmcdiag u <variant>
 *     1 = U-Boot GCTRL + own descriptor + DMAC=0x300
 *     2 = U-Boot GCTRL only (no descriptor / DLBA / DMAC writes)
 *     3 = as 1 but TMOUT/FTRGL left at the openvela driver values
 */

/* ---------- incremental-hypothesis batch ----------
 * Registers U-Boot/Linux program that no openvela path ever has:
 *   DBGC (0x50) = 0xdeb   Linux sunxi_mmc_init_host writes it; we read 0x1
 *   NTSR (0x5c) bit31     U-Boot sets new-timing mode; we read 0
 *   GCTRL bit4            U-Boot's idle GCTRL=0x20000000 has bit4 clear
 * Plus a 50 MHz clock variant and a WRITE variant (CMD24) to localise
 * RX vs TX.
 *   mmcdiag k <variant>
 *     0 baseline  1 +DBGC  2 +NTSR  3 +both  4 GCTRL no-int-enable
 *     5 50MHz     6 +DBGC+NTSR+50MHz  7 CMD24 write  8 8-bit(host+card)
 */

static void batch_probe(int v)
{
  uint32_t rintr = 0;
  volatile int t;
  int done = 0;
  int wr_mode = (v == 7);
  const char *names[] =
  {
    "baseline",         "+DBGC",       "+NTSR",
                         "+DBGC+NTSR",       "GCTRL=0x0",   "50MHz",
    "+DBGC+NTSR+50MHz", "CMD24 WRITE", "8bit"
  };

  printf("k%d %s: ", v, names[v]);

  wr(SDMMC2_BASE + GCTRL,
     GCTRL_SOFT_RESET | GCTRL_FIFO_RESET | GCTRL_DMA_RESET);
  t = 100000;
  while ((rd(SDMMC2_BASE + GCTRL) &
          (GCTRL_SOFT_RESET | GCTRL_FIFO_RESET | GCTRL_DMA_RESET)) &&
         --t)
    ;
  wr(SDMMC2_BASE + GCTRL,
     (v == 4) ? 0 : (GCTRL_INTERRUPT_ENABLE | (1u << 29)));

  set_ccu_clock((v == 5 || v == 6) ? 50000000 : 400000);
  clock_update();
  wr(SDMMC2_BASE + WIDTH, (v == 8) ? 2 : 0);
  wr(SDMMC2_BASE + TMOUT, 0xffffffff);
  wr(SDMMC2_BASE + 0x40, 0x300f00f0);
  wr(SDMMC2_BASE + IMASK, 0);
  wr(SDMMC2_BASE + RINTR, 0xffffffff);
  card_init();

  if (v == 1 || v == 3 || v == 6)
    wr(SDMMC2_BASE + 0x50, 0xdeb);
  if (v == 2 || v == 3 || v == 6)
    wr(SDMMC2_BASE + 0x5c, 0x80000000);
  if (v == 8)
    {
      /* set the CARD to 8-bit too, so card and host match */

      send_cmd_ok(6, (3u << 24) | (183u << 16) | (3u << 8),
                  CMDR_RESP_EXPECT | CMDR_CHECK_CRC);
    }

  /* IDMAC + descriptor */

  g_dmades[0] = DES0_CH | DES0_FD | DES0_LD | DES0_ER | DES0_OWN | DES0_DIC;
  g_dmades[1] = 512;
  g_dmades[2] = ((uint32_t)(uintptr_t)g_dmadat) >> 2;
  g_dmades[3] = wr_mode ? 0x12345678 : 0;
  up_clean_dcache((uintptr_t)g_dmabuf, (uintptr_t)g_dmabuf + 64);

  wr(SDMMC2_BASE + GCTRL, rd(SDMMC2_BASE + GCTRL) | GCTRL_DMA_ENABLE);
  wr(SDMMC2_BASE + DMAC, DMAC_SOFT_RESET);
  t = 100000;
  while ((rd(SDMMC2_BASE + DMAC) & DMAC_SOFT_RESET) && --t)
    ;
  wr(SDMMC2_BASE + DMAC, DMAC_FIX_BURST | DMAC_IDMA_ON);
  wr(SDMMC2_BASE + IDIE, wr_mode ? (1u << 0) : (1u << 1));
  wr(SDMMC2_BASE + DLBA, ((uint32_t)(uintptr_t)g_dmades) >> 2);

  wr(SDMMC2_BASE + BLKSZ, 512);
  wr(SDMMC2_BASE + BCNTR, 512);
  wr(SDMMC2_BASE + RINTR, 0xffffffff);
  wr(SDMMC2_BASE + CARG, g_read_lba);
  wr(SDMMC2_BASE + CMDR, CMDR_START | CMDR_RESP_EXPECT | CMDR_CHECK_CRC |
                             CMDR_DATA_EXPECT | CMDR_WAIT_PRE_OVER |
                             (wr_mode ? 24 : 17) |
                             (wr_mode ? CMDR_WRITE : 0));

  t = 6000000;
  while (t--)
    {
      rintr = rd(SDMMC2_BASE + RINTR);
      if (rintr & (1u << 3))
        {
          done = 1;
          break;
        }

      if (rintr & ((1u << 7) | (1u << 9) | (1u << 11) | (1u << 8) | (1u
          << 1) |
                   (1u << 6) | (1u << 15)))
        {
          done = -1;
          break;
        }
    }

  printf("CMD%d %s RINTR=%08x STAS=%08x CBCR=%08x BBCR=%08x IDST=%08x\n",
         wr_mode ? 24 : 17,
         done > 0 ? "DATA_OVER" : (done < 0 ? "ERROR" : "TIMEOUT"), rintr,
         rd(SDMMC2_BASE + STAS), rd(SDMMC2_BASE + 0x48),
         rd(SDMMC2_BASE + 0x4c), rd(SDMMC2_BASE + IDST));
  printf("   GCTRL=%08x CLKCR=%08x DBGC=%08x NTSR=%08x CCU838=%08x\n",
         rd(SDMMC2_BASE + GCTRL), rd(SDMMC2_BASE + CLKCR),
         rd(SDMMC2_BASE + 0x50), rd(SDMMC2_BASE + 0x5c),
         rd(CCU_BASE + SD2_CLK_CFG));
  wr(SDMMC2_BASE + RINTR, 0xffffffff);
}

/* ---------- U-Boot golden replay probe ----------
 *
 * Tina U-Boot reads this card successfully at this prompt in exactly this
 * state (measured at 0x04022000 right after a good `mmc read`):
 *   GCTRL=0x20000000 (bit29 only -- DMA_ENB clear), CLKCR=0x00010000,
 *   TMOUT=0xffffffff, WIDTH=0x00000002 (8 bit), FTRGL=0x300f00f0,
 *   IMASK=0, CARG=0x00020000, CMDR=0x00002351, CBCR=0x200.
 * The payload moves through the CPU/FIFO (AHB) path: the IDMAC registers
 * (DMAC=0x300, DLBA=0x1efb2e70, IDST=0, IDIE=0) keep BOOT0's leftovers and
 * never change across repeated reads.  CONFIG_MMC_SUNXI_USE_DMA is unset in
 * that build, so sunxi_mmc.c:867 takes the `if (0)` branch and always calls
 * mmc_trans_data_by_cpu() (sunxi_mmc.c:360).
 *
 * Crucially that CARG is a BLOCK address (LBA 0x20000 = 64 MiB, where
 * boot.img lives -- U-Boot returns "ANDROID!").  Every CMD17/CMD18 probe in
 * this file used to write 0x00020000 * 512 (= LBA 0x4000000 = 32 GiB), i.e.
 * past the end of a 29.1 GiB card, so the card answered ADDRESS_OUT_OF_RANGE
 * and legitimately never drove DAT: CBCR=0 in every one of those runs.  That
 * byte-address bug is FIXED: those probes now write g_read_lba (the LBA
 * itself), which is what U-Boot and the Linux sunxi-mmc driver do.
 *
 * At the live U-Boot prompt the only poked register that breaks a working
 * read is WIDTH: `mw.l 0x402200c 0` + `mmc read` => "0 blocks read: ERROR",
 * while 50 MHz (CCU 0x8100000b), GCTRL=0x20 (DMA_ENB set, ACCESS_BY_AHB
 * clear), TMOUT=0xffffff40 and a full `mmc rescan` all still read OK.
 *
 * Variants: 0 exact replay   1 WIDTH=1 (4 bit)   2 WIDTH=0 (1 bit)
 *           3 replay after card_init()   4 exact replay without the AHB bit
 */

/* Probe helper: send a CMD6 (SWITCH) with or without the 512-byte data
 * phase.  The driver's h616_widebus() sends CMD6 *without* DATA_EXPECT,
 * which makes the card answer R1 with SWITCH_ERROR (bit7) -- this probes
 * whether that leaves the card's data path stuck.
 */

static void probe_cmd6(uint32_t arg, int with_data)
{
  volatile int t = 3000000;
  uint32_t rintr = 0;
  unsigned i = 0;

  wr(SDMMC2_BASE + GCTRL, rd(SDMMC2_BASE + GCTRL) | (1u << 31));
  wr(SDMMC2_BASE + BLKSZ, 512);
  wr(SDMMC2_BASE + BCNTR, 512);
  wr(SDMMC2_BASE + RINTR, 0xffffffff);
  wr(SDMMC2_BASE + CARG, arg);
  wr(SDMMC2_BASE + CMDR, CMDR_START | CMDR_WAIT_PRE_OVER |
                             (with_data ? CMDR_DATA_EXPECT : 0) |
                             CMDR_CHECK_CRC | CMDR_RESP_EXPECT | 6);

  while (t-- > 0)
    {
      rintr = rd(SDMMC2_BASE + RINTR);
      if ((rintr &
           (INT_COMMAND_DONE | INT_RESP_TIMEOUT | INT_RESP_CRC_ERROR)) != 0)
        break;
    }

  printf("   cmd6(arg=%08x, data=%d): RINTR=%08x R1=%08x STAS=%08x\n", arg,
         with_data, rintr, rd(SDMMC2_BASE + RESP0), rd(SDMMC2_BASE + STAS));

  if (with_data)
    {
      for (i = 0; i < 128; i++)
        {
          t = 2000000;
          while ((rd(SDMMC2_BASE + STAS) & (1u << 2)) != 0 && --t > 0)
            {
            }

          if (t <= 0)
            break;

          (void)rd(SDMMC2_BASE + FIFO);
        }

      printf("   cmd6 data: %u/128 words CBCR=%08x\n", i,
             rd(SDMMC2_BASE + 0x48));
    }

  wr(SDMMC2_BASE + RINTR, 0xffffffff);
}

static void dump_extcsd(const char *tag)
{
  unsigned i;

  printf("   %sextcsd n=%u:", tag, g_last_n);
  for (i = 179; i <= 201; i++)
    {
      printf(" %u=%02x", i, EXT_BYTE(i));
    }

  printf("\n   %sPARTITION_CONFIG=%u BUS_WIDTH=%u HS_TIMING=%u REV=%u "
         "CARD_TYPE=%u\n",
         tag, EXT_BYTE(179), EXT_BYTE(183), EXT_BYTE(185), EXT_BYTE(192),
         EXT_BYTE(194));
}

/* One CMD17 read (512 bytes from LBA 0x20000) on the *current* controller
 * and card state -- byte-identical to the sequence u_probe() used for
 * v0..v13, so that results from the card-state variants (14..19) can be
 * compared directly against u0.  `use_idmac` selects the internal DMA path
 * (only v9), everything else uses U-Boot's AHB/FIFO path.
 */

static void u_read17(const char *tag, int use_idmac)
{
  uint32_t rintr = 0;
  volatile int t;
  int done = 0;
  unsigned i = 0;
  unsigned w;
  uint32_t words[128];

  g_last_n = 0;

  if (g_drv_style)
    {
      /* The driver's own pre-read steps: h616_blocksetup()'s clock-update
       * dance, then h616_recvsetup()'s data-path selection (AHB) and data
       * interrupt mask -- all *before* CMDR, like U-Boot's
       * mmc_trans_data_by_cpu() does it.
       */

      wr(SDMMC2_BASE + CLKCR, rd(SDMMC2_BASE + CLKCR) | CLKCR_MASK_DATA0);
      wr(SDMMC2_BASE + CMDR,
          CMDR_START | CMDR_UPCLK_ONLY | CMDR_WAIT_PRE_OVER);
      t = 100000;
      while ((rd(SDMMC2_BASE + CMDR) & CMDR_START) != 0 && --t > 0)
        {
        }

      wr(SDMMC2_BASE + CLKCR,
         (rd(SDMMC2_BASE + CLKCR) & ~CLKCR_MASK_DATA0)
             | CLKCR_CARD_CLOCK_ON);
      wr(SDMMC2_BASE + GCTRL, rd(SDMMC2_BASE + GCTRL) | 0x80000000u);
      wr(SDMMC2_BASE + IMASK, 0x00000a88u);
      printf("   %sdrv-style preamble: GCTRL=%08x CLKCR=%08x"
          "IMASK=%08x\n", tag,
             rd(SDMMC2_BASE + GCTRL), rd(SDMMC2_BASE + CLKCR),
             rd(SDMMC2_BASE + IMASK));
    }

  if (use_idmac)
    {
      g_dmades[0] = DES0_CH | DES0_FD | DES0_LD | DES0_ER | DES0_OWN
          | DES0_DIC;
      g_dmades[1] = 512;
      g_dmades[2] = ((uint32_t)(uintptr_t)g_dmadat) >> 2;
      g_dmades[3] = 0;
      up_clean_dcache((uintptr_t)g_dmabuf, (uintptr_t)g_dmabuf + 64);

      wr(SDMMC2_BASE + GCTRL, rd(SDMMC2_BASE + GCTRL) | GCTRL_DMA_ENABLE);
      wr(SDMMC2_BASE + DMAC, DMAC_SOFT_RESET);
      t = 100000;
      while ((rd(SDMMC2_BASE + DMAC) & DMAC_SOFT_RESET) && --t > 0)
        {
        }

      wr(SDMMC2_BASE + DMAC, DMAC_FIX_BURST | DMAC_IDMA_ON);
      wr(SDMMC2_BASE + IDIE, 1u << 1);
      wr(SDMMC2_BASE + DLBA, ((uint32_t)(uintptr_t)g_dmades) >> 2);
    }

  wr(SDMMC2_BASE + BLKSZ, 512);
  wr(SDMMC2_BASE + BCNTR, 512);
  wr(SDMMC2_BASE + RINTR, 0xffffffff);
  wr(SDMMC2_BASE + CARG, (g_read_cmd == 17) ? g_read_lba : 0);
  wr(SDMMC2_BASE + CMDR, CMDR_START | CMDR_WAIT_PRE_OVER | CMDR_DATA_EXPECT |
                             CMDR_CHECK_CRC | CMDR_RESP_EXPECT | g_read_cmd);

  if (use_idmac)
    {
      t = 6000000;
      while (t-- > 0)
        {
          rintr = rd(SDMMC2_BASE + RINTR);
          if ((rintr & (1u << 3)) != 0)
            {
              done = 1;
              break;
            }

          if ((rintr & ((1u << 7) | (1u << 9) | (1u << 11) | (1u << 8) |
                        (1u << 1) | (1u << 6) | (1u << 15))) != 0)
            {
              done = -1;
              break;
            }
        }

      up_invalidate_dcache((uintptr_t)g_dmabuf, (uintptr_t)g_dmabuf + 64);
      i = 128;

      for (w = 0; w < 4; w++)
        {
          words[w] = g_dmadat[w];
        }

      printf(
          "   %sidmac: done=%d RINTR=%08x STAS=%08x CBCR=%08x IDST=%08x"
              "DMAC=%08x\n",
          tag, done, rintr, rd(SDMMC2_BASE + STAS), rd(SDMMC2_BASE + 0x48),
          rd(SDMMC2_BASE + IDST), rd(SDMMC2_BASE + DMAC));
    }
  else
    {
      /* U-Boot's mmc_trans_data_by_cpu(): wait until the FIFO has
       * something, then take one word, 128 times.
       */

      for (i = 0; i < 128; i++)
        {
          t = 2000000;
          while ((rd(SDMMC2_BASE + STAS) & (1u << 2)) != 0 && --t > 0)
            {
            }

          if (t <= 0)
            {
              done = -2;
              break;
            }

          words[i] = rd(SDMMC2_BASE + FIFO);
        }

      rintr = rd(SDMMC2_BASE + RINTR);
      printf(
          "   %scmd: lba=%05x RINTR=%08x RESP0=%08x STAS=%08x CBCR=%08x"
              "BBCR=%08x\n",
          tag, (unsigned)g_read_lba, rintr, rd(SDMMC2_BASE + RESP0),
          rd(SDMMC2_BASE + STAS), rd(SDMMC2_BASE + 0x48),
          rd(SDMMC2_BASE + 0x4c));
      printf("   %sfifo: got %u/128 words%s\n", tag, i,
             done == -2 ? " (STARVED - FIFO stayed empty)" : "");

      /* Content signature: an order-sensitive hash plus the number of
       * non-zero words.  Every entry of words[] came out of the data FIFO,
       * so these numbers identify the block the CARD actually sent.
       */

        {
          uint32_t h = 0;
          unsigned nz = 0;
          unsigned j;
          for (j = 0; j < i; j++)
            {
              g_last_read[j] = words[j];
              h = ((h << 1) | (h >> 31)) ^ words[j];
              nz += (words[j] != 0);
            }

          g_last_n = i;

          printf("   %ssum: n=%u nz=%u hash=%08x\n", tag, i, nz, h);
        }
    }

  for (w = 0; w < 8 && w < i; w++)
    {
      printf("   %sw%u=%08x", tag, w, words[w]);
    }

  printf(
      "\n   %safter: GCTRL=%08x WIDTH=%08x CLKCR=%08x TMOUT=%08x"
          "CCU838=%08x\n",
      tag, rd(SDMMC2_BASE + GCTRL), rd(SDMMC2_BASE + WIDTH),
      rd(SDMMC2_BASE + CLKCR), rd(SDMMC2_BASE + TMOUT),
      rd(CCU_BASE + SD2_CLK_CFG));
  wr(SDMMC2_BASE + RINTR, 0xffffffff);
}

/* Correct eMMC SWITCH (CMD6) *including* the mandatory 512-byte data phase.
 * The driver's h616_widebus() sends CMD6 with MMCSD_NODATAXFR and value 3,
 * which the card rejects with SWITCH_ERROR (R1 bit7) -- the card's bus width
 * never changes, so host (8-bit) and card (1-bit) keep disagreeing.
 *   EXT_CSD[185] HS_TIMING   value 1 = high speed
 *   EXT_CSD[183] BUS_WIDTH   value 0/1/2 = 1/4/8 bit (3 is NOT valid)
 */

static int cmd6_switch(uint32_t idx, uint32_t value)
{
  uint32_t r1;
  int i;
  int n = 0;
  volatile int t;

  wr(SDMMC2_BASE + BLKSZ, 512);
  wr(SDMMC2_BASE + BCNTR, 512);
  wr(SDMMC2_BASE + RINTR, 0xffffffff);
  wr(SDMMC2_BASE + CARG, (3u << 24) | (idx << 16) | (value << 8));
  wr(SDMMC2_BASE + GCTRL, rd(SDMMC2_BASE + GCTRL) | (1u << 31));
  wr(SDMMC2_BASE + GCTRL, rd(SDMMC2_BASE + GCTRL) & ~GCTRL_DMA_ENABLE);

  /* R1b response (48 bit, CRC) + 512-byte data phase */

  wr(SDMMC2_BASE + CMDR, CMDR_START | CMDR_WAIT_PRE_OVER | CMDR_DATA_EXPECT |
                             CMDR_CHECK_CRC | CMDR_RESP_EXPECT | 6);

  t = 1000000;
  while (!(rd(SDMMC2_BASE + RINTR) & ((1 << 2) | (1 << 8) | (1 << 6))) &&
         --t > 0)
    {
    }

  r1 = rd(SDMMC2_BASE + RESP0);

  /* drain the 512-byte data phase over the AHB/FIFO path */

  for (i = 0; i < 128; i++)
    {
      t = 200000;
      while ((rd(SDMMC2_BASE + STAS) & (1 << 2)) != 0 && --t > 0)
        {
        }

      (void)rd(SDMMC2_BASE + FIFO);
      n++;
    }

  printf("   cmd6_switch(idx=%u val=%u): RINTR=%08x R1=%08x SWITCH_ERROR=%d "
         "drained=%d CBCR=%08x STAS=%08x\n",
         (unsigned)idx, (unsigned)value, rd(SDMMC2_BASE + RINTR), r1,
         (int)((r1 >> 7) & 1), n, rd(SDMMC2_BASE + CBCR),
         rd(SDMMC2_BASE + STAS));
  wr(SDMMC2_BASE + RINTR, 0xffffffff);
  return (int)((r1 >> 7) & 1);
}

/* eMMC card init *with the waiting* the driver's mmcsd stack does.
 * mmcdiag's card_init() fires CMD2 immediately after CMD1, while the card
 * is still busy with its internal bring-up: CMD2 then gets no response at
 * all (RINTR=0x104 = COMMAND_DONE|RESP_TIMEOUT) and every later command
 * fails the same way.  That is exactly what u14..u23 show -- it is a probe
 * bug, not a card or controller fault.
 */

static int card_init_wait(void)
{
  uint32_t r1;
  int i;
  int ok = 0;

  send_cmd_ok(0, 0xf0f0f0f0, CMDR_SEND_INIT_SEQ);
  usleep(2000);
  send_cmd_ok(0, 0x00000000, CMDR_SEND_INIT_SEQ);
  usleep(2000);

  /* CMD1 returns the OCR (R3), NOT an R1: its bit31 is the card power-up
   * status, so bit31 == 1 means READY and bit31 == 0 means still busy.
   * This loop used to wait for bit31 == 0 and therefore left the card in
   * the busy state before CMD2 (measured on this board: CMD1 answered
   * 40ff8080, i.e. bit31 clear, and every following command returned the
   * same stale 40ff8080 with RESP_TIMEOUT on the data commands).
   */

  for (i = 0; i < 50; i++)
    {
      send_cmd_ok(1, 0x40300000, CMDR_RESP_EXPECT);
      r1 = rd(SDMMC2_BASE + RESP0);
      printf("   cmd1[%d] OCR=%08x"
          "ready=%d\n", i, r1, (int)((r1 >> 31) & 1));
      if (((r1 >> 31) & 1) == 1)
        {
          ok = 1;
          break;
        }

      usleep(20000);
    }

  if (!ok)
    {
      printf("   cmd1 never reported ready\n");
      return -1;
    }

  usleep(20000);
  send_cmd_ok(2, 0x00000000,
              CMDR_RESP_EXPECT | CMDR_LONG_RESPONSE | CMDR_CHECK_CRC);
  printf("   cmd2 RESP0=%08x\n", rd(SDMMC2_BASE + RESP0));
  usleep(20000);

  send_cmd_ok(3, 0x00010000, CMDR_RESP_EXPECT | CMDR_CHECK_CRC);
  printf("   cmd3 RESP0=%08x\n", rd(SDMMC2_BASE + RESP0));
  usleep(20000);

  /* CMD13 until the card leaves the init states (STBY = 3) */

  for (i = 0; i < 20; i++)
    {
      send_cmd_ok(13, 0x00010000, CMDR_RESP_EXPECT | CMDR_CHECK_CRC);
      r1 = rd(SDMMC2_BASE + RESP0);
      printf("   cmd13[%d] R1=%08x state=%u\n", i, r1,
             (unsigned)((r1 >> 9) & 0xf));
      if (((r1 >> 9) & 0xf) >= 3)
        break;
      usleep(20000);
    }

  send_cmd_ok(9, 0x00010000,
              CMDR_RESP_EXPECT | CMDR_LONG_RESPONSE | CMDR_CHECK_CRC);
  printf("   cmd9 RESP0=%08x\n", rd(SDMMC2_BASE + RESP0));
  usleep(20000);

  send_cmd_ok(7, 0x00010000, CMDR_RESP_EXPECT | CMDR_CHECK_CRC);
  printf("   cmd7 R1=%08x\n", rd(SDMMC2_BASE + RESP0));
  usleep(20000);

  send_cmd_ok(16, 512, CMDR_RESP_EXPECT | CMDR_CHECK_CRC);
  printf("   cmd16 R1=%08x\n", rd(SDMMC2_BASE + RESP0));
  return 0;
}

/* Exact replication of the driver's h616_reset() sequence
 * (src/nuttx/arch/arm64/src/h616/h616_sdhci.c:965):
 *   soft reset -> GCTRL=INTERRUPT_ENABLE -> WIDTH=1-bit -> RINTR clear ->
 *   CCU 400 kHz + UPCLK (nothing may be issued in between!) -> optional
 *   HWRST card hardware reset -> TMOUT/FTRGL/THLDC -> WIDTH=1-bit.
 *
 * HWRST is the piece mmcdiag never had; the driver comment says it is what
 * un-wedges an eMMC left stuck by a failed data command (otherwise every
 * later access reports CBCR=0 for the rest of the boot).
 */

static void drv_reset(int with_hwrst)
{
  volatile int t;

  wr(SDMMC2_BASE + GCTRL,
     GCTRL_SOFT_RESET | GCTRL_FIFO_RESET | GCTRL_DMA_RESET);
  t = 100000;
  while ((rd(SDMMC2_BASE + GCTRL) &
          (GCTRL_SOFT_RESET | GCTRL_FIFO_RESET | GCTRL_DMA_RESET)) &&
         --t > 0)
    {
    }

  wr(SDMMC2_BASE + GCTRL, GCTRL_INTERRUPT_ENABLE);
  wr(SDMMC2_BASE + WIDTH, 0); /* H616_SDXC_WIDTH1 */
  wr(SDMMC2_BASE + RINTR, 0xffffffff);

  /* CCU module clock, then the controller clock update -- no command may be
   * issued between the reset and this point (UPCLK would never complete).
   */

  set_ccu_clock(400000);
  clock_update();

  if (with_hwrst)
    {
      wr(SDMMC2_BASE + HWRST, 0); /* assert (active low) */
      usleep(10);
      wr(SDMMC2_BASE + HWRST, 1u); /* deassert            */
      usleep(300);
    }

  wr(SDMMC2_BASE + GCTRL,
     rd(SDMMC2_BASE + GCTRL) & ~((1u << 31) | GCTRL_INTERRUPT_ENABLE));
  wr(SDMMC2_BASE + TMOUT, 0xffffffff);
  wr(SDMMC2_BASE + 0x40, 0x300f00f0);  /* FTRGL */
  wr(SDMMC2_BASE + THLDC, 0x02000005); /* THLDC: H616 data phase */
  wr(SDMMC2_BASE + WIDTH, 0);
}

static void dump_datapath(const char *tag)
{
  printf(
      "   [%s] DBGC=%08x CSDC=%08x FUNS=%08x A12A=%08x NTSR=%08x FTRGL=%08x"
          "STAS=%08x\n",
      tag, rd(SDMMC2_BASE + 0x50), rd(SDMMC2_BASE + 0x54),
      rd(SDMMC2_BASE + 0x44), rd(SDMMC2_BASE + 0x58), rd(SDMMC2_BASE + 0x5c),
      rd(SDMMC2_BASE + 0x40), rd(SDMMC2_BASE + STAS));
  printf(
      "   [%s] THLDC=%08x 0x104=%08x 0x108=%08x 0x10c=%08x drv_dl=%08x"
          "samp_dl=%08x ds_dl=%08x\n",
      tag, rd(SDMMC2_BASE + 0x100), rd(SDMMC2_BASE + 0x104),
      rd(SDMMC2_BASE + 0x108), rd(SDMMC2_BASE + 0x10c),
      rd(SDMMC2_BASE + 0x140), rd(SDMMC2_BASE + 0x144),
      rd(SDMMC2_BASE + 0x148));
  printf(
      "   [%s] WIDTH=%08x GCTRL=%08x IMASK=%08x HWRST=%08x GATE=%08x"
          "PIO_PC=%08x/%08x/%08x CCU838=%08x\n",
      tag, rd(SDMMC2_BASE + WIDTH), rd(SDMMC2_BASE + GCTRL),
      rd(SDMMC2_BASE + IMASK), rd(SDMMC2_BASE + HWRST),
      rd(CCU_BASE + SD_GATE_RESET), rd(0x0300b000 + 0x48),
      rd(0x0300b000 + 0x4c), rd(0x0300b000 + 0x50),
      rd(CCU_BASE + SD2_CLK_CFG));
}

/* mmcdiag b <lba> [<count>]: read <count> 512-byte blocks from <lba> through
 * the CPU/AHB data path -- the path that is proven to work -- and print one
 * compact line per block.  This is the tool used to check that a candidate
 * eMMC area is *all zeros* before it is formatted for a file system.
 *
 * The controller and the card are put into a known state first: CIU reset
 * plus a card hardware reset (so the eMMC is back in its power-on 1-bit mode
 * and the controller is not waiting for HS400 strobes), re-init, 25 MHz,
 * host
 * 1-bit, and the CPU/AHB data path selected *before* CMDR.
 */

static void block_scan(uint32_t lba, int count)
{
  uint32_t w;
  uint32_t nz;
  uint32_t hash;
  uint32_t w0;
  int i;
  int j;
  int done;
  volatile int t;

  drv_reset(1); /* CIU reset + card hardware reset */

  if (card_init_wait() != 0)
    {
      printf("   block_scan: card init failed, aborting\n");
      return;
    }

  /* The card is back in its power-on mode: drop the HS400 state U-Boot left
   * in the controller (see the driver's h616_reset()).
   */

  wr(SDMMC2_BASE + DSD_REG, rd(SDMMC2_BASE + DSD_REG) & ~0x80000000u);
  wr(SDMMC2_BASE + CSDC_REG, rd(SDMMC2_BASE + CSDC_REG) & ~0xfu);
  wr(SDMMC2_BASE + NTSR_REG, 0);
  wr(SDMMC2_BASE + GCTRL,
     rd(SDMMC2_BASE + GCTRL) & ~(1u << 10)); /* DDR off */

  wr(SDMMC2_BASE + IMASK, 0); /* silence the driver's IRQ handler */
  set_ccu_clock(25000000);
  clock_update();
  wr(SDMMC2_BASE + WIDTH, 0); /* 1 bit, matches the post-HWRST card */

  printf(
      "   block_scan lba=%05x count=%d WIDTH=%08x GCTRL=%08x CCU838=%08x"
          "STAS=%08x\n",
      (unsigned)lba, count, rd(SDMMC2_BASE + WIDTH), rd(SDMMC2_BASE + GCTRL),
      rd(CCU_BASE + SD2_CLK_CFG), rd(SDMMC2_BASE + STAS));

  for (i = 0; i < count; i++)
    {
      /* ACCESS_BY_AHB must be set before CMDR, otherwise the data phase
       * never starts and the FIFO replays the previous transfer's first
       * word (u34/u37).
       */

      wr(SDMMC2_BASE + GCTRL,
         (rd(SDMMC2_BASE + GCTRL) | GCTRL_INTERRUPT_ENABLE | (1u << 31)) &
             ~(GCTRL_DMA_ENABLE | GCTRL_DMA_RESET));
      wr(SDMMC2_BASE + BLKSZ, 512);
      wr(SDMMC2_BASE + BCNTR, 512);
      wr(SDMMC2_BASE + RINTR, 0xffffffff);
      wr(SDMMC2_BASE + CARG, lba + (uint32_t)i); /* block address, not bytes */
      wr(SDMMC2_BASE + CMDR, CMDR_START | CMDR_WAIT_PRE_OVER |
                                 CMDR_DATA_EXPECT | CMDR_CHECK_CRC |
                                 CMDR_RESP_EXPECT | 17);

      nz = 0;
      hash = 0;
      w0 = 0;
      done = 0;
      for (j = 0; j < 128; j++)
        {
          t = 2000000;
          while ((rd(SDMMC2_BASE + STAS) & (1u << 2)) != 0 && --t > 0)
            {
            }

          if (t <= 0)
            {
              break;
            }

          w = rd(SDMMC2_BASE + FIFO);
          if (w != 0)
            {
              nz++;
            }

          hash = ((hash << 1) | (hash >> 31)) ^ w;
          if (j == 0)
            {
              w0 = w;
            }

          done = j + 1;
        }

      printf(
          "   blk lba=%05x got=%3d/128 nz=%3u hash=%08x w0=%08x RINTR=%08x"
              "STAS=%08x CBCR=%08x BBCR=%08x\n",
          (unsigned)(lba + (uint32_t)i), done, (unsigned)nz, hash, w0,
          rd(SDMMC2_BASE + RINTR), rd(SDMMC2_BASE + STAS),
          rd(SDMMC2_BASE + 0x48), rd(SDMMC2_BASE + 0x4c));
      wr(SDMMC2_BASE + RINTR, 0xffffffff);
    }
}

/* u39 helper: leave the controller in a *consistent* state after a CIU-only
 * reset, then read.  hs400=1 reproduces the state U-Boot boots this eMMC in
 * (dsbd bit31, csdc[3:0]=6, DDR bit clear); hs400=0 makes it plain SDR.
 */

static void u39_arm(int hs400, uint32_t clkhz, uint32_t width)
{
  drv_reset(0);               /* CIU-only reset: clears the wedge */
  wr(SDMMC2_BASE + IMASK, 0); /* silence the driver's IRQ handler */
  set_ccu_clock(clkhz);
  clock_update();

  if (hs400)
    {
      wr(SDMMC2_BASE + DSD_REG, rd(SDMMC2_BASE + DSD_REG) | 0x80000000u);
      wr(SDMMC2_BASE + CSDC_REG,
          (rd(SDMMC2_BASE + CSDC_REG) & ~0xfu) | 0x6u);
    }
  else
    {
      wr(SDMMC2_BASE + DSD_REG, rd(SDMMC2_BASE + DSD_REG) & ~0x80000000u);
      wr(SDMMC2_BASE + CSDC_REG, rd(SDMMC2_BASE + CSDC_REG) & ~0xfu);
      wr(SDMMC2_BASE + NTSR_REG, 0);
    }

  wr(SDMMC2_BASE + GCTRL,
     (rd(SDMMC2_BASE + GCTRL) & ~(1u << 10)) | 0x80000000u);
  wr(SDMMC2_BASE + WIDTH, width);
  printf("   arm hs400=%d clk=%u width=%u -> DSD=%08x CSDC=%08x NTSR=%08x "
         "GCTRL=%08x WIDTH=%08x STAS=%08x\n",
         hs400, clkhz, width, rd(SDMMC2_BASE + DSD_REG),
         rd(SDMMC2_BASE + CSDC_REG), rd(SDMMC2_BASE + NTSR_REG),
         rd(SDMMC2_BASE + GCTRL), rd(SDMMC2_BASE + WIDTH),
         rd(SDMMC2_BASE + STAS));
}

static void u_probe(int v)
{
  volatile int t;
  int use_idmac = (v == 9);
  static const char *names[] = {
      "exact U-Boot replay (8bit, no init)",
      "4-bit, no init",
      "1-bit, no init",
      "8-bit after card_init",
      "8-bit, no ACCESS_BY_AHB bit",
      "50MHz driver clock raise, 8-bit",
      "controller reset only, 8-bit",
      "reset + 50MHz, 8-bit",
      "reset + 50MHz, 1-bit (driver config)",
      "8-bit golden state, but IDMAC path",
      "driver CMD6 (NO data phase) then read",
      "proper CMD6 (512B data) then read",
      "CMD0 hardware reset then read",
      "reset + 50MHz + 1-bit + driver CMD6",
      "card_init then 1-bit host, read x2",
      "card_init then 8-bit host, read x2",
      "card_init + 1-bit + driver IMASK, read x2",
      "card_init + 1-bit + driver CMD13, read x2",
      "full driver replica, read x2",
      "full driver replica + 100ms settle, read x2",
      "DRIVER EXACT + HWRST + THLDC, read x2",
      "20 + 100ms settle, read x2",
      "HWRST then golden 8-bit read (no init), x2",
      "20 without HWRST (isolate HWRST), read x2",
      "proper card init (waits) 1-bit, read x2",
      "proper card init, host 8-bit, read x2",
      "proper init + real CMD6 BW=8 + 8-bit, x2",
      "proper init 1-bit + THLDC=0, read x2",
      "REAL driver init, then ladder 1-bit/8-bit/proper CMD6",
      "REAL driver init, then ladder 1-bit/4-bit/proper CMD6 BW=4",
      "golden control -> driver init -> CMD6 + partition fix ladder",
      "same ladder WITHOUT the PARTITION_CONFIG write",
      "golden/driver CCU bisection (SD2_CLK_CFG one register at a time)",
      "same CCU bisection WITHOUT the golden-state break steps",
      "golden-state sabotage ladder (one driver register at a time)",
      "working baseline + CCU clock bisection + driver-init read",
      "driver init then clock/width matrix (default-speed legal clock)",
      "AHB ordering + fixed driver init + sticky-state recovery",
      "golden-card step ladder: width sweep, EXT_CSD, driver reset bisect",
      "HS400 mode register bisect (dsbd/csdc) after the card reset",
      "driver init with the HS400-markers fix, then read"
      };

  printf("u%d %s:\n", v, names[v]);
  printf(
      "   before: GCTRL=%08x CLKCR=%08x WIDTH=%08x TMOUT=%08x FTRGL=%08x"
          "CCU838=%08x\n",
      rd(SDMMC2_BASE + GCTRL), rd(SDMMC2_BASE + CLKCR),
      rd(SDMMC2_BASE + WIDTH), rd(SDMMC2_BASE + TMOUT),
      rd(SDMMC2_BASE + 0x40), rd(CCU_BASE + SD2_CLK_CFG));

  /* --- driver-style controller reset (h616_reset): variants 6, 7, 8, 13 ---
   */

  if (v == 6 || v == 7 || v == 8 || v == 13)
    {
      wr(SDMMC2_BASE + GCTRL,
         GCTRL_SOFT_RESET | GCTRL_FIFO_RESET | GCTRL_DMA_RESET);
      t = 100000;
      while ((rd(SDMMC2_BASE + GCTRL) &
              (GCTRL_SOFT_RESET | GCTRL_FIFO_RESET | GCTRL_DMA_RESET)) &&
             --t > 0)
        {
        }

      wr(SDMMC2_BASE + GCTRL, GCTRL_INTERRUPT_ENABLE);
    }

  /* --- driver-style clock raise (h616_blocksetup): 50 MHz via CCU +
   * clock_update() (MASK_DATA0 -> UPCLK command -> unmask): 5, 7, 8 ---
   */

  if (v == 5 || v == 7 || v == 8 || v == 13)
    {
      set_ccu_clock(50000000);
      clock_update();
    }

  /* --- card commands sent *before* the read (10..13): the driver's
   * malformed CMD6 / a bare CMD0, to see which one poisons the data path
   * in the golden state (no controller reset, no clock change).
   */

  if (v == 10)
    {
      probe_cmd6(0x03b30000u, 0); /* h616_widebus(): no DATA_EXPECT */
    }
  else if (v == 11)
    {
      probe_cmd6(0x03b30000u, 1); /* correct SWITCH with 512B data */
    }
  else if (v == 12)
    {
      int rc = send_cmd_ok(0, 0xf0f0f0f0, CMDR_SEND_INIT_SEQ);

      printf("   cmd0(hw reset): rc=%d RINTR=%08x R1=%08x\n", rc,
             rd(SDMMC2_BASE + RINTR), rd(SDMMC2_BASE + RESP0));
    }
  else if (v == 13)
    {
      probe_cmd6(0x03b30000u, 0); /* the whole driver pre-read state */
    }

  /* --- HWRST variants (20..23): the driver's h616_reset() performs a real
   * card hardware reset (HWRST) that mmcdiag never had.  u14..u19 showed
   * mmcdiag's card_init() wedges the card permanently (RESP_TIMEOUT, CBCR=0)
   * with no way back -- with HWRST it should recover, exactly as the
   * driver's
   * own comment claims.
   *   20 = exact driver replica: h616_reset (HWRST + THLDC) at 400 kHz ->
   *        card init -> h616_blocksetup (100 MHz) -> 1-bit -> IMASK -> CMD13
   *   21 = 20 + 100 ms settle before the read
   *   22 = HWRST then BOOT0's golden 8-bit state (no card init)
   *   23 = 20 without HWRST (isolates HWRST's effect)
   */

  if (v >= 20 && v <= 23)
    {
      drv_reset(v != 23); /* driver h616_reset() */

      if (v == 22)
        {
          /* keep the BOOT0 8-bit/100 MHz host state, only the card was
           * hardware-reset by HWRST above
           */

          wr(CCU_BASE + SD2_CLK_CFG, 0x81000005);
          clock_update();
          wr(SDMMC2_BASE + WIDTH, 2);
        }
      else
        {
          card_init(); /* CMD0..CMD16 -> card is 1-bit */

          set_ccu_clock(100000000); /* h616_blocksetup(): 0x8100000b */
          clock_update();
          wr(SDMMC2_BASE + WIDTH, 0);      /* driver: 1-bit */
          wr(SDMMC2_BASE + IMASK, 0x0a88); /* h616_configxfrints() */

          /* h616_dbg_card_state(): CMD13 SEND_STATUS, CARG = RCA 1 */

          send_cmd_ok(13, 0x00010000, CMDR_RESP_EXPECT | CMDR_CHECK_CRC);

          if (v == 21)
            {
              usleep(100000);
            }
        }

      wr(SDMMC2_BASE + TMOUT, 0xffffffff);
      wr(SDMMC2_BASE + 0x40, 0x300f00f0);
      wr(SDMMC2_BASE + GCTRL, rd(SDMMC2_BASE + GCTRL) | (1u << 31));
      wr(SDMMC2_BASE + GCTRL, rd(SDMMC2_BASE + GCTRL) & ~GCTRL_DMA_ENABLE);

      printf(
          "   state: IMASK=%08x WIDTH=%08x STAS=%08x R1=%08x CLKCR=%08x"
              "CCU838=%08x HWRST=%08x THLDC=%08x\n",
          rd(SDMMC2_BASE + IMASK), rd(SDMMC2_BASE + WIDTH),
          rd(SDMMC2_BASE + STAS), rd(SDMMC2_BASE + RESP0),
          rd(SDMMC2_BASE + CLKCR), rd(CCU_BASE + SD2_CLK_CFG),
          rd(SDMMC2_BASE + HWRST), rd(SDMMC2_BASE + THLDC));

      u_read17("read#1 ", 0);
      u_read17("read#2 ", 0);
      return;
    }

  /* --- u24..u27: the driver's whole init done *properly* -- this time with
   * the waiting mmcsd does, which mmcdiag's card_init() gets wrong.
   * 24 = drv_reset + proper card init -> 100 MHz -> 1-bit -> IMASK -> CMD13
   *   25 = 24 but host 8-bit (WIDTH=2)
   *   26 = 24 then a PROPER CMD6 (HS=1, BUS_WIDTH=8-bit, with its 512-byte
   *        data phase) and host 8-bit = the h616_widebus() fix
   *   27 = 24 with THLDC cleared (card read threshold)
   */

  if (v >= 24 && v <= 27)
    {
      drv_reset(1);
      card_init_wait();
      set_ccu_clock(100000000);
      clock_update();

      if (v == 26)
        {
          cmd6_switch(185, 1); /* HS_TIMING = 1     */
          cmd6_switch(183, 2); /* BUS_WIDTH = 8-bit */
        }

      wr(SDMMC2_BASE + WIDTH, (v == 24 || v == 27) ? 0 : 2);
      if (v == 27)
        {
          wr(SDMMC2_BASE + THLDC, 0);
        }

      wr(SDMMC2_BASE + IMASK, (v == 24 || v == 27) ? 0x0a88 : 0);

      send_cmd_ok(13, 0x00010000, CMDR_RESP_EXPECT | CMDR_CHECK_CRC);

      wr(SDMMC2_BASE + TMOUT, 0xffffffff);
      wr(SDMMC2_BASE + 0x40, 0x300f00f0);
      wr(SDMMC2_BASE + GCTRL, rd(SDMMC2_BASE + GCTRL) | (1u << 31));
      wr(SDMMC2_BASE + GCTRL, rd(SDMMC2_BASE + GCTRL) & ~GCTRL_DMA_ENABLE);

      printf(
          "   state: IMASK=%08x WIDTH=%08x STAS=%08x R1=%08x CLKCR=%08x"
              "CCU838=%08x HWRST=%08x THLDC=%08x\n",
          rd(SDMMC2_BASE + IMASK), rd(SDMMC2_BASE + WIDTH),
          rd(SDMMC2_BASE + STAS), rd(SDMMC2_BASE + RESP0),
          rd(SDMMC2_BASE + CLKCR), rd(CCU_BASE + SD2_CLK_CFG),
          rd(SDMMC2_BASE + HWRST), rd(SDMMC2_BASE + THLDC));

      u_read17("read#1 ", 0);
      u_read17("read#2 ", 0);
      return;
    }

  /* --- u28/u29: run the REAL driver init (yuzuki_h616_sdhci_initialize)
   * and then ladder the host bus width / card bus-width switch from exactly
   * the state it leaves behind.  Every earlier variant hand-replicated the
   * driver's init (u14..u27) and ended up with RESP_TIMEOUT for every
   * command
   * after CMD1, while the driver's own init gets a real response for each
   * command -- so stop replicating it and reuse it.
   *   u28: driver init -> CMD13 -> dump
   *        step 1: host 1-bit, card as left (mirrors the driver's failing
   *                read -> expect CBCR=0 + DATA_TIMEOUT)
   *        step 2: host 8-bit, card untouched (is the card still 8-bit?)
   *        step 3: proper CMD6 HS=1 + BW=8, host 8-bit (the widebus fix)
   *        step 4: read again (stability)
   *   u29: same ladder but the card switch is BW=4 and the host is 4-bit
   */

  if (v >= 28 && v <= 29)
    {
      int rc;

      printf("   driver-init...\n");
      rc = yuzuki_h616_sdhci_initialize();
      printf("   driver-init returned %d\n", rc);
      dump_regs("after-driver-init");

      rc = send_cmd_ok(13, 0x00010000, CMDR_RESP_EXPECT | CMDR_CHECK_CRC);
      printf("   after-init CMD13 rc=%d R1=%08x RINTR=%08x STAS=%08x\n", rc,
             rd(SDMMC2_BASE + RESP0), rd(SDMMC2_BASE + RINTR),
             rd(SDMMC2_BASE + STAS));

      wr(SDMMC2_BASE + TMOUT, 0xffffffff);
      wr(SDMMC2_BASE + 0x40, 0x300f00f0);
      wr(SDMMC2_BASE + GCTRL, rd(SDMMC2_BASE + GCTRL) | (1u << 31));
      wr(SDMMC2_BASE + GCTRL, rd(SDMMC2_BASE + GCTRL) & ~GCTRL_DMA_ENABLE);

      printf("   --- step 1: host 1-bit, card as the driver left it ---\n");
      wr(SDMMC2_BASE + WIDTH, 0);
      u_read17("s1 ", 0);

      printf("   --- step 2: host 8-bit, card untouched ---\n");
      wr(SDMMC2_BASE + WIDTH, 2);
      u_read17("s2 ", 0);

      printf("   --- step 3: proper CMD6 (HS=1, BW=%d) + host %s ---\n",
             v == 29 ? 4 : 8, v == 29 ? "4-bit" : "8-bit");
      cmd6_switch(185, 1);
      cmd6_switch(183, v == 29 ? 1 : 2);
      wr(SDMMC2_BASE + WIDTH, v == 29 ? 1 : 2);
      u_read17("s3 ", 0);

      printf("   --- step 4: read again (stability) ---\n");
      u_read17("s4 ", 0);
      return;
    }

  /* --- u30/u31: in-run golden control.  Everything up to here compared the
   * golden state (BOOT0 left the card 8-bit and mid-TRAN) against the driver
   * state across *different boots*.  This variant reads the known-good LBA
   * 0x20000 (boot.img's ANDROID! header lives there, proven by U-Boot) in
   * BOOT0's own state first, then runs the real driver init and dumps the
   * vendor / data-path registers the driver may clobber, then tries to get
   * the read back with the proper CMD6 -- same LBA, same boot, so the data
   * itself is the control.
   *   [A] golden 8-bit read (expect ANDROID!)
   *   [B] yuzuki_h616_sdhci_initialize() + register dump
   *   [C] read in the driver's leftover 1-bit state
   *   [D] u30: CMD6 PARTITION_CONFIG=user + HS_TIMING=1 + BUS_WIDTH=8 (the
   *       real h616_widebus fix), host 8-bit; u31: same without the
   *       PARTITION_CONFIG write (isolates it)
   *   [E] read again + final dump
   */

  if (v == 30 || v == 31)
    {
      int rc;

      printf("   [A] golden (BOOT0) state, same LBA as always\n");
      dump_datapath("golden");
      wr(SDMMC2_BASE + WIDTH, 2);
      u_read17("g1 ", 0);

      printf("   [B] real driver init\n");
      rc = yuzuki_h616_sdhci_initialize();
      printf("   driver-init returned %d\n", rc);
      dump_datapath("after-driver-init");

      printf("   [C] read in the driver's own leftover state\n");
      u_read17("d1 ", 0);

      printf("   [D] card-side switch: %s + HS=1 + BW=8, host 8-bit\n",
             v == 30 ? "PARTITION_CONFIG=user (as BOOT0 does)"
                     : "no PARTITION_CONFIG write");
      if (v == 30)
        {
          cmd6_switch(179, 0); /* PARTITION_CONFIG: user area */
        }

      cmd6_switch(185, 1); /* HS_TIMING = 1 */
      cmd6_switch(183, 2); /* BUS_WIDTH = 8-bit */
      wr(SDMMC2_BASE + WIDTH, 2);
      dump_datapath("after-cmd6");
      u_read17("d2 ", 0);

      printf("   [E] read again + final dump\n");
      u_read17("d3 ", 0);
      dump_datapath("final");
      return;
    }

  /* --- u32/u33: same-boot CCU bisection.  u30 read LBA 0x20000 fine in the
   * golden state and starved after the driver init, and the only register
   * differences it could find (aside from IMASK, which u0 already clears
   * before its read and still starves) were CCU SD2_CLK_CFG (golden
   * 0x81000005 -> /6, driver 0x8100000b -> /12) and SD_GATE_RESET.  This
   * variant changes those one at a time inside a single boot, with the same
   * LBA read as the control:
   *   [A]  golden dump + read, capture the golden CCU values
   *   [A2] golden state, driver's SD2_CLK_CFG -> read
   *   [A3] back to golden SD2_CLK_CFG -> read
   *   [A4] re-assert golden SD_GATE_RESET -> read
   *   [B]  yuzuki_h616_sdhci_initialize() + dump
   *   [C]  read (driver clock + driver gate)
   *   [D]  driver state, golden SD2_CLK_CFG + golden SD_GATE_RESET -> read
   *   [E]  read again + final dump
   * u33 = the same ladder with the golden-state break steps skipped.
   */

  if (v == 32 || v == 33)
    {
      uint32_t gate_gold;
      uint32_t clk_gold;
      int rc;

      printf("   [A] golden (BOOT0) state control\n");
      dump_datapath("golden");
      gate_gold = rd(CCU_BASE + SD_GATE_RESET);
      clk_gold = rd(CCU_BASE + SD2_CLK_CFG);
      printf(
          "   [A] captured golden CCU: SD2_CLK_CFG=%08x"
              "SD_GATE_RESET=%08x\n",
          clk_gold, gate_gold);
      wr(SDMMC2_BASE + WIDTH, 2);
      g_read_lba = 0x00020000;
      u_read17("g1 ", 0);
      g_read_lba = 0x00020004;
      u_read17("g2 ", 0);

      if (v == 32)
        {
          printf("   [A2] golden state + driver's SD2_CLK_CFG"
              "(0x8100000b)\n");
          wr(CCU_BASE + SD2_CLK_CFG, 0x8100000b);
          clock_update();
          dump_datapath("golden+driverclk");
          g_read_lba = 0x00020000;
          u_read17("a1 ", 0);

          printf("   [A3] back to the golden SD2_CLK_CFG\n");
          wr(CCU_BASE + SD2_CLK_CFG, clk_gold);
          clock_update();
          g_read_lba = 0x00020000;
          u_read17("a2 ", 0);

          printf("   [A4] re-assert the golden SD_GATE_RESET\n");
          wr(CCU_BASE + SD_GATE_RESET, gate_gold);
          g_read_lba = 0x00020000;
          u_read17("a3 ", 0);
        }

      printf("   [B] real driver init\n");
      rc = yuzuki_h616_sdhci_initialize();
      printf("   driver-init returned %d\n", rc);
      dump_datapath("after-driver-init");

      printf("   [C] read in the driver's leftover state\n");
      g_read_lba = 0x00020000;
      u_read17("c1 ", 0);

      printf("   [D] driver state + golden CCU (CLK_CFG and GATE)\n");
      wr(CCU_BASE + SD_GATE_RESET, gate_gold);
      wr(CCU_BASE + SD2_CLK_CFG, clk_gold);
      clock_update();
      dump_datapath("driver+goldenclk");
      g_read_lba = 0x00020000;
      u_read17("d2 ", 0);

      printf("   [E] read again + final dump\n");
      g_read_lba = 0x00020000;
      u_read17("d3 ", 0);
      g_read_lba = 0x00020004;
      u_read17("d4 ", 0);
      dump_datapath("final");
      return;
    }

  /* --- u34: golden-state sabotage ladder.
   *
   * In the golden (BOOT0/U-Boot) state a CMD17 read of LBA 0x20000 returns
   * boot.img's ANDROID! header; after yuzuki_h616_sdhci_initialize() exactly
   * the same read starves (CBCR stays 0, the data FIFO never fills).  Every
   * variant that starved had GCTRL bit31 (ACCESS_BY_AHB) *set* -- that is
   * what h616_eventwait() leaves behind on the driver's CPU/PIO read path --
   * while the golden state, which reads fine, has it clear.  So reproduce
   * each driver-register value ONE AT A TIME *inside the golden state*,
   * where
   * the card itself is known to transfer, and read LBA 0x20000 (ANDROID!)
   * plus LBA 0x20004 (ARM64 kernel code) after every step.  Whatever step
   * turns 128/128 into a starved read is the root cause, and the matching
   * restore step shows whether it is reversible.
   *   [A] golden control          [B] GCTRL |= ACCESS_BY_AHB
   *   [C] restore GCTRL           [D] WIDTH = 1 (host 1-bit)
   *   [E] restore WIDTH           [F] IMASK = 0x0a88
   *   [G] restore IMASK           [H] CCU SD2_CLK_CFG = 0x8100000b
   *   [I] restore CCU             [J] golden SD_GATE_RESET + final dump
   */

  if (v == 34)
    {
      uint32_t gctrl_gold;
      uint32_t width_gold;
      uint32_t imask_gold;
      uint32_t clk_gold;
      uint32_t gate_gold;

      printf(
          "   [A] golden control: two known LBAs before touching"
              "anything\n");
      dump_datapath("golden");
      gctrl_gold = rd(SDMMC2_BASE + GCTRL);
      width_gold = rd(SDMMC2_BASE + WIDTH);
      imask_gold = rd(SDMMC2_BASE + IMASK);
      clk_gold = rd(CCU_BASE + SD2_CLK_CFG);
      gate_gold = rd(CCU_BASE + SD_GATE_RESET);
      printf("   captured GCTRL=%08x WIDTH=%08x IMASK=%08x SD2_CLK_CFG=%08x"
             " SD_GATE_RESET=%08x\n",
             gctrl_gold, width_gold, imask_gold, clk_gold, gate_gold);
      g_read_lba = 0x00020000;
      u_read17("A1 ", 0);
      g_read_lba = 0x00020004;
      u_read17("A2 ", 0);

      printf("   [B] GCTRL = golden | ACCESS_BY_AHB (bit31)\n");
      wr(SDMMC2_BASE + GCTRL, gctrl_gold | 0x80000000u);
      g_read_lba = 0x00020000;
      u_read17("B1 ", 0);
      g_read_lba = 0x00020004;
      u_read17("B2 ", 0);

      printf("   [C] GCTRL restored\n");
      wr(SDMMC2_BASE + GCTRL, gctrl_gold);
      g_read_lba = 0x00020000;
      u_read17("C1 ", 0);

      printf("   [D] WIDTH = 1 (host 1-bit, card still whatever BOOT0"
          "left)\n");
      wr(SDMMC2_BASE + WIDTH, 0);
      g_read_lba = 0x00020000;
      u_read17("D1 ", 0);

      printf("   [E] WIDTH restored\n");
      wr(SDMMC2_BASE + WIDTH, width_gold);
      g_read_lba = 0x00020000;
      u_read17("E1 ", 0);

      printf("   [F] IMASK = 0x00000a88 (the driver's value)\n");
      wr(SDMMC2_BASE + IMASK, 0x00000a88u);
      g_read_lba = 0x00020000;
      u_read17("F1 ", 0);

      printf("   [G] IMASK restored\n");
      wr(SDMMC2_BASE + IMASK, imask_gold);
      g_read_lba = 0x00020000;
      u_read17("G1 ", 0);

      printf("   [H] CCU SD2_CLK_CFG = 0x8100000b (the driver's value)\n");
      wr(CCU_BASE + SD2_CLK_CFG, 0x8100000bu);
      clock_update();
      g_read_lba = 0x00020000;
      u_read17("H1 ", 0);

      printf("   [I] CCU SD2_CLK_CFG restored\n");
      wr(CCU_BASE + SD2_CLK_CFG, clk_gold);
      clock_update();
      g_read_lba = 0x00020000;
      u_read17("I1 ", 0);

      printf("   [J] re-assert golden SD_GATE_RESET + final dump\n");
      wr(CCU_BASE + SD_GATE_RESET, gate_gold);
      clock_update();
      g_read_lba = 0x00020000;
      u_read17("J1 ", 0);
      dump_datapath("final");
      return;
    }

  /* --- u35: working-baseline + clock bisection + driver-init test.
   *
   * u34 showed two things every earlier variant had backwards:
   *   - with GCTRL bit31 (ACCESS_BY_AHB) SET, the CPU/FIFO read path works
   *     perfectly: u34 B1/B2 returned boot.img's LBA 0x20000 and 0x20004
   *     byte-for-byte (hash 644d09ff / 652044b1, nz 9 / 121);
   *   - with bit31 clear (ACCESS_BY_DMA -- the golden/BOOT0 value) the CPU
   *     read the FIRST word of the block 128 times and the next CMDR never
   *     completed again (RINTR stayed 0) -- the CIU parked in a DMA
   *     transfer with no descriptor programmed.
   * So: every read below runs with bit31 SET, and the only variable in
   * [B]/[C] is the CCU SD2_CLK_CFG value -- the golden 0x81000005 is
   * PLL6x2/6 = 200MHz, the driver's 0x8100000b is PLL6x2/12 = 100MHz
   * (H616_CCM_MMC_CTRL_M(x) = x-1, H616_PLL6X2_HZ = 1200000000).
   *   [A] golden + bit31 -> 3 reads (baseline, repeatability)
   *   [B] driver clock 100MHz -> read x2
   *   [C] golden clock 200MHz -> read
   *   [D] driver init, then golden clock + bit31 -> read x2  (DECISIVE)
   *   [E] driver clock again -> read
   *   [F] bit31 clear (control) -> read, then bit31 back -> read
   *   [W] bus-width register write/readback probe
   * Good data = LBA 0x20000: 52444e41 2144494f 00070000 40080000 / 0...
   *             LBA 0x20004: e59f0010 e59f1010 e3a02311 e3a03001 e1600070
   *                         eafffffe 8000ff04 40080040 (nz=121).
   */

  if (v == 35)
    {
      uint32_t gctrl_gold;
      uint32_t width_gold;
      uint32_t imask_gold;
      uint32_t clk_gold;
      uint32_t gate_gold;
      int rc;

      gctrl_gold = rd(SDMMC2_BASE + GCTRL);
      width_gold = rd(SDMMC2_BASE + WIDTH);
      imask_gold = rd(SDMMC2_BASE + IMASK);
      clk_gold = rd(CCU_BASE + SD2_CLK_CFG);
      gate_gold = rd(CCU_BASE + SD_GATE_RESET);
      dump_datapath("golden");
      printf("   captured GCTRL=%08x WIDTH=%08x IMASK=%08x SD2_CLK_CFG=%08x"
             " SD_GATE_RESET=%08x\n",
             gctrl_gold, width_gold, imask_gold, clk_gold, gate_gold);

      printf("   [A] golden + ACCESS_BY_AHB (bit31) -- working baseline\n");
      wr(SDMMC2_BASE + GCTRL, gctrl_gold | 0x80000000u);
      g_read_lba = 0x00020000;
      u_read17("a1 ", 0);
      g_read_lba = 0x00020004;
      u_read17("a2 ", 0);
      g_read_lba = 0x00020000;
      u_read17("a3 ", 0);

      printf("   [B] CCU SD2_CLK_CFG = 0x8100000b (driver, 100MHz) +"
          "update\n");
      wr(CCU_BASE + SD2_CLK_CFG, 0x8100000bu);
      clock_update();
      printf("   CCU838 now %08x\n", rd(CCU_BASE + SD2_CLK_CFG));
      g_read_lba = 0x00020000;
      u_read17("b1 ", 0);
      g_read_lba = 0x00020004;
      u_read17("b2 ", 0);

      printf("   [C] CCU SD2_CLK_CFG back to golden %08x (200MHz) +"
          "update\n",
             clk_gold);
      wr(CCU_BASE + SD2_CLK_CFG, clk_gold);
      clock_update();
      printf("   CCU838 now %08x\n", rd(CCU_BASE + SD2_CLK_CFG));
      g_read_lba = 0x00020000;
      u_read17("c1 ", 0);

      printf("   [D] DECISIVE: driver init, then golden clock + bit31\n");
      rc = yuzuki_h616_sdhci_initialize();
      printf("   driver-init returned %d\n", rc);
      dump_datapath("after-driver-init");
      wr(CCU_BASE + SD_GATE_RESET, gate_gold);
      wr(CCU_BASE + SD2_CLK_CFG, clk_gold);
      clock_update();
      wr(SDMMC2_BASE + GCTRL, rd(SDMMC2_BASE + GCTRL) | 0x80000000u);
      dump_datapath("driver+goldenclk+ahb");
      g_read_lba = 0x00020000;
      u_read17("d1 ", 0);
      g_read_lba = 0x00020004;
      u_read17("d2 ", 0);

      printf("   [E] driver clock again (100MHz) -> read\n");
      wr(CCU_BASE + SD2_CLK_CFG, 0x8100000bu);
      clock_update();
      g_read_lba = 0x00020000;
      u_read17("e1 ", 0);

      printf("   [F] ACCESS_BY_DMA control (bit31 clear) -> read, then"
          "back\n");
      wr(SDMMC2_BASE + GCTRL, gctrl_gold);
      g_read_lba = 0x00020000;
      u_read17("f1 ", 0);
      wr(SDMMC2_BASE + GCTRL, gctrl_gold | 0x80000000u);
      g_read_lba = 0x00020000;
      u_read17("f2 ", 0);

      printf("   [W] bus-width register write/readback probe\n");
      printf("   WIDTH before=%08x\n", rd(SDMMC2_BASE + WIDTH));
      wr(SDMMC2_BASE + WIDTH, 0);
      printf("   WIDTH after wr 0 = %08x\n", rd(SDMMC2_BASE + WIDTH));
      wr(SDMMC2_BASE + WIDTH, 2);
      printf("   WIDTH after wr 2 = %08x\n", rd(SDMMC2_BASE + WIDTH));
      wr(SDMMC2_BASE + WIDTH, 1);
      printf("   WIDTH after wr 1 = %08x\n", rd(SDMMC2_BASE + WIDTH));
      wr(SDMMC2_BASE + WIDTH, width_gold);
      printf("   WIDTH restored   = %08x\n", rd(SDMMC2_BASE + WIDTH));

      printf("   [Z] final dump\n");
      dump_datapath("final");
      return;
    }

  if (v == 36)
    {
      uint32_t gctrl_gold;
      uint32_t width_gold;
      uint32_t clk_gold;
      uint32_t gate_gold;
      int rc;

      gctrl_gold = rd(SDMMC2_BASE + GCTRL);
      width_gold = rd(SDMMC2_BASE + WIDTH);
      clk_gold = rd(CCU_BASE + SD2_CLK_CFG);
      gate_gold = rd(CCU_BASE + SD_GATE_RESET);
      dump_datapath("golden");
      printf("   captured GCTRL=%08x WIDTH=%08x SD2_CLK_CFG=%08x"
          "GATE=%08x\n",
             gctrl_gold, width_gold, clk_gold, gate_gold);

      /* [A] golden control.  The card is still in the HS200 / 8 bit state
       * U-Boot left behind, so 200MHz + host 8 bit is legal and must work.
       */

      printf("   [A] golden: AHB + host 8-bit @200MHz (control, must"
          "work)\n");
      wr(SDMMC2_BASE + GCTRL, gctrl_gold | 0x80000000u);
      wr(SDMMC2_BASE + WIDTH, 2);
      g_read_lba = 0x00020000;
      u_read17("a1 ", 0);

      /* [B] the real driver init.  h616_blocksetup() now uses
       * H616_SDMMC_MAXCLK = 26MHz and h616_widebus() no longer moves the
       * host to 8 bit, so the card must leave this init in 1 bit with the
       * host at 1 bit -- and the CMD8 (SEND_EXT_CSD) read inside the init
       * should finally complete.  Watch the [CPU0] driver log for it.
       */

      printf("   [B] real driver init (blocksetup clock = 26000000 Hz)\n");
      rc = yuzuki_h616_sdhci_initialize();
      printf("   driver-init returned %d\n", rc);
      dump_datapath("after-driver-init");
      printf(
          "   driver leftovers: GCTRL=%08x WIDTH=%08x IMASK=%08x"
              "CCU838=%08x\n",
          rd(SDMMC2_BASE + GCTRL), rd(SDMMC2_BASE + WIDTH),
          rd(SDMMC2_BASE + IMASK), rd(CCU_BASE + SD2_CLK_CFG));

      /* [C] THE TEST.  After a card hardware reset the eMMC is back in
       * default speed (<=26MHz) / 1 bit, so 26MHz + host 1 bit + the AHB
       * (bit31) FIFO path is the configuration that should read.
       */

      printf("   [C] 26MHz + 1-bit (card post-reset mode), AHB -> read\n");
      wr(SDMMC2_BASE + GCTRL, rd(SDMMC2_BASE + GCTRL) | 0x80000000u);
      wr(SDMMC2_BASE + WIDTH, 0);
      set_ccu_clock(26000000u);
      clock_update();
      printf("   CCU838=%08x WIDTH=%08x\n", rd(CCU_BASE + SD2_CLK_CFG),
             rd(SDMMC2_BASE + WIDTH));
      g_read_lba = 0x00020000;
      u_read17("c1 ", 0);
      g_read_lba = 0x00020004;
      u_read17("c2 ", 0);

      printf("   [D] 13MHz + 1-bit -> read\n");
      set_ccu_clock(13000000u);
      clock_update();
      g_read_lba = 0x00020000;
      u_read17("d1 ", 0);

      printf("   [E] 52MHz + 1-bit (HS rate only) -> read\n");
      set_ccu_clock(52000000u);
      clock_update();
      g_read_lba = 0x00020000;
      u_read17("e1 ", 0);

      printf("   [F] 100MHz + 1-bit (illegal for default speed) -> read\n");
      set_ccu_clock(100000000u);
      clock_update();
      g_read_lba = 0x00020000;
      u_read17("f1 ", 0);

      printf("   [G] 26MHz + host 8-bit -> read\n");
      set_ccu_clock(26000000u);
      clock_update();
      wr(SDMMC2_BASE + WIDTH, 2);
      g_read_lba = 0x00020000;
      u_read17("g1 ", 0);

      printf("   [H] 26MHz + host 4-bit -> read\n");
      wr(SDMMC2_BASE + WIDTH, 1);
      g_read_lba = 0x00020000;
      u_read17("h1 ", 0);

      printf("   [I] back to 1-bit @26MHz, read the second block\n");
      wr(SDMMC2_BASE + WIDTH, 0);
      g_read_lba = 0x00020004;
      u_read17("i1 ", 0);

      printf("   [Z] final dump\n");
      dump_datapath("final");
      return;
    }

  if (v == 37)
    {
      /* u37: ordering rule + the fixed driver + sticky-state recovery.
       *
       *   [A]  golden: AHB (bit31) selected BEFORE CMDR -> must read
       *   [A2] golden: DMA (bit31 clear) selected BEFORE CMDR -> junk
       *        (this pair is the whole ordering hypothesis)
       *   [B]  real driver init, now that h616_recvsetup() selects the AHB
       *        data path before CMDR: does its CMD8 (SEND_EXT_CSD) read
       *        finally deliver 512 bytes?  Watch the [CPU0] log.
       *   [C]  read after the driver init (expect the sticky data-busy
       *        state -> starve; prints STAS bit9 + DMAC/IDST)
       *   [D]  controller-only reset (NO card HWRST, the card is left
       *        selected in TRAN) then read -> does that clear it?
       *   [E]  [D] + host 8-bit
       *   [F]  [D] + host 4-bit
       */

      uint32_t gctrl_gold;
      uint32_t width_gold;
      uint32_t clk_gold;
      uint32_t gate_gold;
      uint32_t rstspin;
      int rc;

      gctrl_gold = rd(SDMMC2_BASE + GCTRL);
      width_gold = rd(SDMMC2_BASE + WIDTH);
      clk_gold = rd(CCU_BASE + SD2_CLK_CFG);
      gate_gold = rd(CCU_BASE + SD_GATE_RESET);
      dump_datapath("golden");
      printf("   captured GCTRL=%08x WIDTH=%08x SD2_CLK_CFG=%08x"
          "GATE=%08x\n",
             gctrl_gold, width_gold, clk_gold, gate_gold);
      printf("   golden STAS=%08x bit9=%u DMAC=%08x DLBA=%08x IDST=%08x\n",
             rd(SDMMC2_BASE + STAS),
             (rd(SDMMC2_BASE + STAS) & STAS_CARD_DATA_BUSY) ? 1u : 0u,
             rd(SDMMC2_BASE + DMAC), rd(SDMMC2_BASE + DLBA),
             rd(SDMMC2_BASE + IDST));

      printf("   [A] golden + AHB before CMDR + host 8-bit @200MHz ->"
          "read\n");
      wr(SDMMC2_BASE + GCTRL, gctrl_gold | 0x80000000u);
      wr(SDMMC2_BASE + WIDTH, 2);
      g_read_lba = 0x00020000;
      u_read17("a1 ", 0);

      printf("   [A2] golden + DMA (bit31 clear) before CMDR -> read\n");
      wr(SDMMC2_BASE + GCTRL, gctrl_gold & ~0x80000000u);
      g_read_lba = 0x00020000;
      u_read17("a2 ", 0);
      printf("   a2 state STAS=%08x WIDTH=%08x\n", rd(SDMMC2_BASE + STAS),
             rd(SDMMC2_BASE + WIDTH));

      printf("   [B] real driver init (fixed recvsetup: AHB before CMDR)\n");
      rc = yuzuki_h616_sdhci_initialize();
      printf("   driver-init returned %d\n", rc);
      dump_datapath("after-driver-init");
      printf(
          "   driver leftovers: GCTRL=%08x WIDTH=%08x IMASK=%08x"
              "CCU838=%08x\n",
          rd(SDMMC2_BASE + GCTRL), rd(SDMMC2_BASE + WIDTH),
          rd(SDMMC2_BASE + IMASK), rd(CCU_BASE + SD2_CLK_CFG));
      printf(
          "   leftovers STAS=%08x bit9=%u DMAC=%08x DLBA=%08x IDST=%08x"
              "IDIE=%08x\n",
          rd(SDMMC2_BASE + STAS),
          (rd(SDMMC2_BASE + STAS) & STAS_CARD_DATA_BUSY) ? 1u : 0u,
          rd(SDMMC2_BASE + DMAC), rd(SDMMC2_BASE + DLBA),
          rd(SDMMC2_BASE + IDST), rd(SDMMC2_BASE + IDIE));

      printf("   [C] after driver init: AHB + 1-bit @26MHz -> read\n");
      wr(SDMMC2_BASE + GCTRL, rd(SDMMC2_BASE + GCTRL) | 0x80000000u);
      wr(SDMMC2_BASE + WIDTH, 0);
      set_ccu_clock(26000000u);
      clock_update();
      printf("   CCU838=%08x WIDTH=%08x STAS=%08x\n",
             rd(CCU_BASE + SD2_CLK_CFG), rd(SDMMC2_BASE + WIDTH),
             rd(SDMMC2_BASE + STAS));
      g_read_lba = 0x00020000;
      u_read17("c1 ", 0);

      printf("   [D] controller-only reset (no card HWRST), then 1-bit"
          "read\n");
      wr(SDMMC2_BASE + GCTRL,
         GCTRL_SOFT_RESET | GCTRL_FIFO_RESET | GCTRL_DMA_RESET);
      rstspin = 200000;
      while ((rd(SDMMC2_BASE + GCTRL) &
              (GCTRL_SOFT_RESET | GCTRL_FIFO_RESET | GCTRL_DMA_RESET))
                  != 0 &&
             --rstspin > 0)
        {
        }

      printf("   reset done t=%u GCTRL=%08x STAS=%08x\n", (unsigned)rstspin,
             rd(SDMMC2_BASE + GCTRL), rd(SDMMC2_BASE + STAS));
      wr(SDMMC2_BASE + GCTRL, GCTRL_INTERRUPT_ENABLE);
      wr(SDMMC2_BASE + IMASK, 0);
      wr(SDMMC2_BASE + WIDTH, 0);
      wr(SDMMC2_BASE + RINTR, 0xffffffffu);
      set_ccu_clock(26000000u);
      clock_update();
      wr(SDMMC2_BASE + TMOUT, 0xffffffffu);
      wr(SDMMC2_BASE + 0x40, 0x300f00f0u); /* FTRGL */
      wr(SDMMC2_BASE + THLDC, 0x02000005u);
      wr(SDMMC2_BASE + GCTRL, GCTRL_INTERRUPT_ENABLE | 0x80000000u);
      printf(
          "   after reset: GCTRL=%08x WIDTH=%08x CLKCR=%08x CCU838=%08x"
              "STAS=%08x\n",
          rd(SDMMC2_BASE + GCTRL), rd(SDMMC2_BASE + WIDTH),
          rd(SDMMC2_BASE + CLKCR), rd(CCU_BASE + SD2_CLK_CFG),
          rd(SDMMC2_BASE + STAS));
      g_read_lba = 0x00020000;
      u_read17("d1 ", 0);
      g_read_lba = 0x00020004;
      u_read17("d2 ", 0);

      printf("   [E] after reset + host 8-bit @26MHz -> read\n");
      wr(SDMMC2_BASE + WIDTH, 2);
      printf("   WIDTH=%08x\n", rd(SDMMC2_BASE + WIDTH));
      g_read_lba = 0x00020000;
      u_read17("e1 ", 0);

      printf("   [F] after reset + host 4-bit @26MHz -> read\n");
      wr(SDMMC2_BASE + WIDTH, 1);
      printf("   WIDTH=%08x\n", rd(SDMMC2_BASE + WIDTH));
      g_read_lba = 0x00020004;
      u_read17("f1 ", 0);

      printf("   [Z] final dump\n");
      dump_datapath("final");
      printf("   final STAS=%08x DMAC=%08x"
          "IDST=%08x\n", rd(SDMMC2_BASE + STAS),
             rd(SDMMC2_BASE + DMAC), rd(SDMMC2_BASE + IDST));
      return;
    }

  if (v == 38)
    {
      /* u38: find the ONE step that kills the data path.
       *
       * The card is left by U-Boot in a state where a probe CMD17 read
       * works.  Every step below starts from a known-good state and
       * changes one thing, so the first step whose read breaks is the
       * culprit.
       *
       *   [A]  control read: golden card, 8-bit host, 200MHz, AHB first
       *   [B]  CMD8 SEND_EXT_CSD -> EXT_CSD[179..201]: what bus width /
       *        timing is the card really using?
       *   [C]  host width sweep 1/4/8 bit with the card untouched
       *        (is WIDTH writable?  what does a width mismatch look like?)
       *   [D]  the driver's own pre-read procedure (blocksetup +
       *        recvsetup register writes) on the golden card
       *   [E]  the driver's CIU reset *without* the card HWRST -> read
       *   [F]  the driver's reset *with* card HWRST + card_init_wait(),
       *        then reads at 1/8/4 bit and EXT_CSD again
       *   [G]  real driver init (its own HWRST inside) -> read + EXT_CSD
       */

      uint32_t gctrl_gold;
      uint32_t width_gold;
      uint32_t clk_gold;
      uint32_t gate_gold;
      int rc;

      gctrl_gold = rd(SDMMC2_BASE + GCTRL);
      width_gold = rd(SDMMC2_BASE + WIDTH);
      clk_gold = rd(CCU_BASE + SD2_CLK_CFG);
      gate_gold = rd(CCU_BASE + SD_GATE_RESET);
      dump_datapath("golden");
      printf(
          "   captured GCTRL=%08x WIDTH=%08x SD2_CLK_CFG=%08x GATE=%08x"
              "STAS=%08x\n",
          gctrl_gold, width_gold, clk_gold, gate_gold,
              rd(SDMMC2_BASE + STAS));

      printf(
          "   [A] control: golden card, host 8-bit @200MHz, AHB before"
              "CMDR\n");
      wr(SDMMC2_BASE + GCTRL, gctrl_gold | 0x80000000u);
      wr(SDMMC2_BASE + WIDTH, width_gold);
      g_read_cmd = 17;
      g_read_lba = 0x00020000;
      u_read17("A1 ", 0);

      printf("   [B] CMD8 SEND_EXT_CSD with the same setup\n");
      g_read_cmd = 8;
      g_read_lba = 0;
      u_read17("B1 ", 0);
      g_read_cmd = 17;
      dump_extcsd("B1 ");

      printf("   [C] host width sweep, card untouched\n");
      wr(SDMMC2_BASE + WIDTH, 0);
      printf("   WIDTH<-0 readback=%08x\n", rd(SDMMC2_BASE + WIDTH));
      g_read_lba = 0x00020000;
      u_read17("C1 ", 0);
      wr(SDMMC2_BASE + WIDTH, 1);
      printf("   WIDTH<-1 readback=%08x\n", rd(SDMMC2_BASE + WIDTH));
      g_read_lba = 0x00020000;
      u_read17("C2 ", 0);
      wr(SDMMC2_BASE + WIDTH, 2);
      printf("   WIDTH<-2 readback=%08x\n", rd(SDMMC2_BASE + WIDTH));
      g_read_lba = 0x00020000;
      u_read17("C3 ", 0);

      printf("   [D] the driver's own pre-read procedure on the golden"
          "card\n");
      wr(SDMMC2_BASE + GCTRL, rd(SDMMC2_BASE + GCTRL) | 0x80000000u);
      wr(SDMMC2_BASE + WIDTH, width_gold);
      g_drv_style = 1;
      g_read_lba = 0x00020000;
      u_read17("D1 ", 0);
      g_drv_style = 0;
      dump_datapath("after-drv-procedure");

      printf("   [E] driver reset WITHOUT card HWRST (card stays"
          "selected)\n");
      drv_reset(0);
      set_ccu_clock(200000000u);
      clock_update();
      wr(SDMMC2_BASE + GCTRL, rd(SDMMC2_BASE + GCTRL) | 0x80000000u);
      wr(SDMMC2_BASE + WIDTH, width_gold);
      printf("   after E reset: GCTRL=%08x WIDTH=%08x CCU838=%08x"
          "STAS=%08x\n",
             rd(SDMMC2_BASE + GCTRL), rd(SDMMC2_BASE + WIDTH),
             rd(CCU_BASE + SD2_CLK_CFG), rd(SDMMC2_BASE + STAS));
      g_read_lba = 0x00020000;
      u_read17("E1 ", 0);
      g_read_lba = 0x00020004;
      u_read17("E2 ", 0);

      printf("   [F] driver reset WITH card HWRST + card_init_wait()\n");
      drv_reset(1);
      card_init_wait();
      set_ccu_clock(25000000u);
      clock_update();
      wr(SDMMC2_BASE + GCTRL, rd(SDMMC2_BASE + GCTRL) | 0x80000000u);
      wr(SDMMC2_BASE + WIDTH, 0);
      printf(
          "   after F init: GCTRL=%08x WIDTH=%08x CCU838=%08x STAS=%08x"
              "R1=%08x\n",
          rd(SDMMC2_BASE + GCTRL), rd(SDMMC2_BASE + WIDTH),
          rd(CCU_BASE + SD2_CLK_CFG), rd(SDMMC2_BASE + STAS),
          rd(SDMMC2_BASE + RESP0));
      g_read_lba = 0x00020000;
      u_read17("F1 ", 0);
      wr(SDMMC2_BASE + WIDTH, 2);
      printf("   WIDTH<-2 readback=%08x\n", rd(SDMMC2_BASE + WIDTH));
      g_read_lba = 0x00020000;
      u_read17("F2 ", 0);
      g_read_cmd = 8;
      g_read_lba = 0;
      u_read17("F3 ", 0);
      g_read_cmd = 17;
      dump_extcsd("F3 ");
      wr(SDMMC2_BASE + WIDTH, 1);
      printf("   WIDTH<-1 readback=%08x\n", rd(SDMMC2_BASE + WIDTH));
      g_read_lba = 0x00020000;
      u_read17("F4 ", 0);

      printf("   [G] real driver init (its own HWRST inside) then read\n");
      drv_reset(1);
      rc = yuzuki_h616_sdhci_initialize();
      printf("   driver-init returned %d\n", rc);
      dump_datapath("after-driver-init");
      printf(
          "   leftovers: GCTRL=%08x WIDTH=%08x IMASK=%08x CCU838=%08x"
              "STAS=%08x\n",
          rd(SDMMC2_BASE + GCTRL), rd(SDMMC2_BASE + WIDTH),
          rd(SDMMC2_BASE + IMASK), rd(CCU_BASE + SD2_CLK_CFG),
          rd(SDMMC2_BASE + STAS));
      wr(SDMMC2_BASE + WIDTH, 2);
      printf("   WIDTH<-2 readback=%08x\n", rd(SDMMC2_BASE + WIDTH));
      g_read_lba = 0x00020000;
      u_read17("G1 ", 0);
      g_read_cmd = 8;
      g_read_lba = 0;
      u_read17("G2 ", 0);
      g_read_cmd = 17;
      dump_extcsd("G2 ");
      wr(SDMMC2_BASE + WIDTH, 0);
      printf("   WIDTH<-0 readback=%08x\n", rd(SDMMC2_BASE + WIDTH));
      g_read_lba = 0x00020000;
      u_read17("G3 ", 0);

      printf("   [Z] final dump\n");
      dump_datapath("final");
      printf("   final STAS=%08x DMAC=%08x"
          "IDST=%08x\n", rd(SDMMC2_BASE + STAS),
             rd(SDMMC2_BASE + DMAC), rd(SDMMC2_BASE + IDST));
      return;
    }

  if (v == 39)
    {
      /* u39: is the *HS400 configuration* U-Boot leaves behind the reason
       * the
       * first data command after the driver's card reset transfers nothing?
       *
       * Forensics -- the golden state is exactly U-Boot's HS400 mode:
       *   dsbd (0x10c) bit31 = 1        (mmc_hs400_mode_onoff)
       *   csdc (0x54)  [3:0]  = 6
       *   GCTRL bit10 (DDR)   = 0
       *   card EXT_CSD[185] (HS_TIMING) = 3, EXT_CSD[196] (CARD_TYPE) = 0x57
       * CCU 0x838 = 0x81000005 -> 1.2GHz/6 = 200MHz, i.e. 100MHz card clock
       *   (mmc2 uses TIMING_MODE_4, which programs 2x the card clock)
       * and golden reads work.  The openvela driver resets the card back to
       * plain 1-bit SDR and never touches those registers, so the CIU keeps
       * sampling the HS400 strobe path -> DATA_TIMEOUT, CBCR=0, FIFO empty.
       * A failed data phase also wedges the CIU, so every arm below starts
       * with drv_reset(0) (CIU-only, harmless per u38[E]) and re-applies
       * clock / width / markers.
       *
       * [A]  golden read (card and controller both in U-Boot's HS400 state)
       *   [B]  driver init (card -> SDR 1-bit), HS400 markers kept -> read
       *   [C0] markers CLEARED, 400kHz, 1-bit -> read (most conservative)
       *   [C1] markers cleared, 25MHz, 1-bit  -> read  <-- the test
       *   [C2] ... same, LBA 0x20004   [C3] CMD8 -> EXT_CSD
       *   [D]  markers SET again -> read (pairing check)
       *   [E]  markers cleared again -> read (repeatability)
       *   [F]  markers cleared, 8-bit host -> read
       *   [G]  golden 8-bit/200MHz card with markers cleared (other side of
       *        the pairing: HS400 card, SDR controller)
       */

      printf("   [A] golden read, card+controller in U-Boot HS400 state\n");
      dump_datapath("golden");
      wr(SDMMC2_BASE + GCTRL, rd(SDMMC2_BASE + GCTRL) | 0x80000000u);
      g_read_cmd = 17;
      g_read_lba = 0x00020000;
      u_read17("A1 ", 0);

      printf(
          "   [B] driver init (card -> SDR 1-bit), HS400 markers still"
              "set\n");
      drv_reset(1);
      printf("   driver-init returned %d\n", yuzuki_h616_sdhci_initialize());
      u39_arm(1, 25000000u, 0);
      g_read_lba = 0x00020000;
      u_read17("B1 ", 0);

      printf("   [C0] markers cleared, 400kHz, 1-bit\n");
      u39_arm(0, 400000u, 0);
      g_read_lba = 0x00020000;
      u_read17("C0 ", 0);

      printf("   [C1] markers cleared, 25MHz, 1-bit (the test)\n");
      u39_arm(0, 25000000u, 0);
      g_read_lba = 0x00020000;
      u_read17("C1 ", 0);
      g_read_lba = 0x00020004;
      u_read17("C2 ", 0);
      g_read_cmd = 8;
      g_read_lba = 0;
      u_read17("C3 ", 0);
      g_read_cmd = 17;
      dump_extcsd("C3 ");

      printf("   [D] markers SET again (pairing check, 25MHz, 1-bit)\n");
      u39_arm(1, 25000000u, 0);
      g_read_lba = 0x00020000;
      u_read17("D1 ", 0);

      printf("   [E] markers cleared again (repeatability)\n");
      u39_arm(0, 25000000u, 0);
      g_read_lba = 0x00020000;
      u_read17("E1 ", 0);

      printf("   [F] markers cleared, 8-bit host @25MHz\n");
      u39_arm(0, 25000000u, 2);
      g_read_lba = 0x00020000;
      u_read17("F1 ", 0);

      printf("   [G] HS400 card + SDR controller (8-bit host, 200MHz)\n");
      wr(SDMMC2_BASE + DSD_REG, rd(SDMMC2_BASE + DSD_REG) & ~0x80000000u);
      wr(SDMMC2_BASE + CSDC_REG, rd(SDMMC2_BASE + CSDC_REG) & ~0xfu);
      wr(SDMMC2_BASE + GCTRL,
         (rd(SDMMC2_BASE + GCTRL) & ~(1u << 10)) | 0x80000000u);
      g_read_lba = 0x00020000;
      u_read17("G1 ", 0);

      printf("   [Z] final dump\n");
      dump_datapath("final");
      return;
    }

  if (v == 40)
    {
      /* u40: does the *driver* now read, with the HS400 markers cleared by
       * h616_reset() itself (the u39 fix)?
       *
       *   [A]  golden baseline read (card + controller in U-Boot's HS400
       *        state, markers set) -- control
       *   [B]  drv_reset(1) + yuzuki_h616_sdhci_initialize(): the patched
       *        driver clears DSBD bit31 / CSDC[3:0] / NTSR itself.  Read at
       *        25MHz / 1-bit with ONLY the clock and IMASK adjusted -- the
       *        controller state is otherwise exactly what the driver left.
       *        LBA 0x20000 and 0x20004 must come back byte-exact.
       *   [C]  set the HS400 markers back -> read must starve again (the
       *        control proving the markers are what [B] removed)
       *   [D]  clear them again -> read must work again
       */

      printf("   [A] golden baseline read (markers set)\n");
      dump_datapath("golden");
      wr(SDMMC2_BASE + GCTRL, rd(SDMMC2_BASE + GCTRL) | 0x80000000u);
      g_read_cmd = 17;
      g_read_lba = 0x00020000;
      u_read17("A1 ", 0);

      printf(
          "   [B] drv_reset(1) + driver init (patched h616_reset clears the"
              "HS400 markers)\n");
      drv_reset(1);
      printf("   driver-init returned %d\n", yuzuki_h616_sdhci_initialize());
      dump_datapath("after-driver-init");
      printf(
          "   markers now: DSD=%08x CSDC=%08x NTSR=%08x GCTRL=%08x"
              "WIDTH=%08x"
              "CCU838=%08x\n",
          rd(SDMMC2_BASE + DSD_REG), rd(SDMMC2_BASE + CSDC_REG),
          rd(SDMMC2_BASE + NTSR_REG), rd(SDMMC2_BASE + GCTRL),
          rd(SDMMC2_BASE + WIDTH), rd(CCU_BASE + SD2_CLK_CFG));
      wr(SDMMC2_BASE + IMASK, 0); /* silence the driver ISR only */
      set_ccu_clock(25000000u);
      clock_update();
      wr(SDMMC2_BASE + GCTRL, rd(SDMMC2_BASE + GCTRL) | 0x80000000u);
      wr(SDMMC2_BASE + WIDTH, 0);
      g_read_cmd = 17;
      g_read_lba = 0x00020000;
      u_read17("B1 ", 0);
      g_read_lba = 0x00020004;
      u_read17("B2 ", 0);
      g_read_cmd = 8;
      g_read_lba = 0;
      u_read17("B3 ", 0);
      g_read_cmd = 17;
      dump_extcsd("B3 ");

      printf("   [C] HS400 markers set back -> read\n");
      wr(SDMMC2_BASE + DSD_REG, rd(SDMMC2_BASE + DSD_REG) | 0x80000000u);
      wr(SDMMC2_BASE + CSDC_REG,
          (rd(SDMMC2_BASE + CSDC_REG) & ~0xfu) | 0x6u);
      printf("   arm C: DSD=%08x CSDC=%08x STAS=%08x\n",
             rd(SDMMC2_BASE + DSD_REG), rd(SDMMC2_BASE + CSDC_REG),
             rd(SDMMC2_BASE + STAS));
      g_read_lba = 0x00020000;
      u_read17("C1 ", 0);

      printf("   [D] markers cleared again -> read\n");
      u39_arm(0, 25000000u, 0);
      g_read_lba = 0x00020000;
      u_read17("D1 ", 0);

      printf("   [Z] final dump\n");
      dump_datapath("final");
      return;
    }

  /* --- card-state variants (14..19): the driver's failing read is the first
   * data command after *its own* card init, with the card freshly reset to
   * 1-bit; every controller register matches a working read except WIDTH,
   * IMASK and the preceding CMD13.  These variants reproduce the card side
   * and then read twice, because 10 and 12 showed a failed first data
   * command can recover by itself.
   *   14 = card_init, 1-bit host        15 = card_init, 8-bit host (control)
   *   16 = 14 + driver IMASK            17 = 14 + driver's CMD13
   *   18 = full driver replica (h616_reset at 400 kHz -> card init ->
   *        h616_blocksetup to 50 MHz -> 1-bit -> IMASK -> CMD13)
   *   19 = 18 + 100 ms settle before the read
   */

  if (v >= 14)
    {
      if (v == 18 || v == 19)
        {
          controller_reset_driverstyle(); /* CCU 400 kHz + reset + UPCLK */
        }

      card_init(); /* CMD0..CMD16 -> card is 1-bit */

      if (v == 18 || v == 19)
        {
          set_ccu_clock(50000000); /* h616_blocksetup() */
          clock_update();
        }

      wr(SDMMC2_BASE + WIDTH, (v == 15) ? 2 : 0);

      if (v == 16 || v == 18 || v == 19)
        {
          wr(SDMMC2_BASE + IMASK, 0x0a88); /* h616_configxfrints() */
        }
      else
        {
          wr(SDMMC2_BASE + IMASK, 0);
        }

      if (v == 17 || v == 18 || v == 19)
        {
          /* h616_dbg_card_state(): CMD13 SEND_STATUS, CARG = RCA 1 */

          send_cmd_ok(13, 0x00010000, CMDR_RESP_EXPECT | CMDR_CHECK_CRC);
        }

      if (v == 19)
        {
          usleep(100000); /* 100 ms settle */
        }

      wr(SDMMC2_BASE + TMOUT, 0xffffffff);
      wr(SDMMC2_BASE + 0x40, 0x300f00f0);
      wr(SDMMC2_BASE + GCTRL, rd(SDMMC2_BASE + GCTRL) | (1u << 31));
      wr(SDMMC2_BASE + GCTRL, rd(SDMMC2_BASE + GCTRL) & ~GCTRL_DMA_ENABLE);

      printf(
          "   state: IMASK=%08x WIDTH=%08x STAS=%08x R1=%08x CLKCR=%08x"
              "CCU838=%08x\n",
          rd(SDMMC2_BASE + IMASK), rd(SDMMC2_BASE + WIDTH),
          rd(SDMMC2_BASE + STAS), rd(SDMMC2_BASE + RESP0),
          rd(SDMMC2_BASE + CLKCR), rd(CCU_BASE + SD2_CLK_CFG));

      u_read17("read#1 ", 0);
      u_read17("read#2 ", 0);
      return;
    }

  wr(SDMMC2_BASE + IMASK, 0);
  wr(SDMMC2_BASE + TMOUT, 0xffffffff);
  wr(SDMMC2_BASE + 0x40, 0x300f00f0);

  switch (v)
  {
  case 1:
    wr(SDMMC2_BASE + WIDTH, 1);
    break;
  case 2:
    wr(SDMMC2_BASE + WIDTH, 0);
    break;
  case 8:
    wr(SDMMC2_BASE + WIDTH, 0);
    break; /* driver: 1-bit */
  case 13:
    wr(SDMMC2_BASE + WIDTH, 0);
    break; /* driver: 1-bit */
  case 3:
    card_init();
    wr(SDMMC2_BASE + WIDTH, 2);
    break;
  default:
    wr(SDMMC2_BASE + WIDTH, 2);
    break;
  }

  if (v != 4)
  {
    wr(SDMMC2_BASE + GCTRL, rd(SDMMC2_BASE + GCTRL) | (1u << 31));
  }

  wr(SDMMC2_BASE + GCTRL, rd(SDMMC2_BASE + GCTRL) & ~GCTRL_DMA_ENABLE);

  u_read17("", use_idmac);
}

/* ---------- sample/drive delay probe ----------
 * U-Boot's captured timing registers after a WORKING 512-byte read:
 *   drv_dl (0x140) = 0x00000000
 *   samp_dl(0x144) = 0x00001898
 *   ds_dl  (0x148) = 0x00000d8d
 * openvela never programs any of the three.  samp_dl is the INPUT sample
 * delay -- it affects the READ direction only, which is exactly the
 * direction
 * that fails (CMD24 write pushed BBCR=0x200; every read leaves CBCR=0).
 *   mmcdiag p <variant>
 *     0 dump only + CMD17
 *     1 write U-Boot's exact drv_dl/samp_dl/ds_dl, then CMD17
 *     2 samp_dl = 0x98 (SW_EN only), then CMD17
 *     3 samp_dl/2 then CMD17
 */

static void delay_probe(int v)
{
  uint32_t rintr = 0;
  volatile int t;
  int done = 0;

  printf(
      "p%d: before drv_dl=%08x samp_dl=%08x ds_dl=%08x DBGC=%08x"
          "NTSR=%08x\n",
      v, rd(SDMMC2_BASE + 0x140), rd(SDMMC2_BASE + 0x144),
      rd(SDMMC2_BASE + 0x148), rd(SDMMC2_BASE + 0x50),
      rd(SDMMC2_BASE + 0x5c));

  wr(SDMMC2_BASE + GCTRL,
     GCTRL_SOFT_RESET | GCTRL_FIFO_RESET | GCTRL_DMA_RESET);
  t = 100000;
  while ((rd(SDMMC2_BASE + GCTRL) &
          (GCTRL_SOFT_RESET | GCTRL_FIFO_RESET | GCTRL_DMA_RESET)) &&
         --t)
    ;
  wr(SDMMC2_BASE + GCTRL, GCTRL_INTERRUPT_ENABLE);
  set_ccu_clock(400000);
  clock_update();
  wr(SDMMC2_BASE + WIDTH, 0);
  wr(SDMMC2_BASE + TMOUT, 0xffffffff);
  wr(SDMMC2_BASE + 0x40, 0x300f00f0);
  wr(SDMMC2_BASE + IMASK, 0);
  wr(SDMMC2_BASE + RINTR, 0xffffffff);
  card_init();

  if (v == 1)
    {
      wr(SDMMC2_BASE + 0x140, 0x00000000);
      wr(SDMMC2_BASE + 0x144, 0x00001898);
      wr(SDMMC2_BASE + 0x148, 0x00000d8d);
    }
  else if (v == 2)
    {
      wr(SDMMC2_BASE + 0x144, 0x00000098);
    }
  else if (v == 3)
    {
      wr(SDMMC2_BASE + 0x144, 0x00001898);
      wr(SDMMC2_BASE + 0x148, 0x00000d8d);
    }

  printf("   after  drv_dl=%08x samp_dl=%08x ds_dl=%08x\n",
         rd(SDMMC2_BASE + 0x140), rd(SDMMC2_BASE + 0x144),
         rd(SDMMC2_BASE + 0x148));

  g_dmades[0] = DES0_CH | DES0_FD | DES0_LD | DES0_ER | DES0_OWN | DES0_DIC;
  g_dmades[1] = 512;
  g_dmades[2] = ((uint32_t)(uintptr_t)g_dmadat) >> 2;
  g_dmades[3] = 0;
  up_clean_dcache((uintptr_t)g_dmabuf, (uintptr_t)g_dmabuf + 64);
  wr(SDMMC2_BASE + GCTRL, rd(SDMMC2_BASE + GCTRL) | GCTRL_DMA_ENABLE);
  wr(SDMMC2_BASE + DMAC, DMAC_SOFT_RESET);
  t = 100000;
  while ((rd(SDMMC2_BASE + DMAC) & DMAC_SOFT_RESET) && --t)
    ;
  wr(SDMMC2_BASE + DMAC, DMAC_FIX_BURST | DMAC_IDMA_ON);
  wr(SDMMC2_BASE + IDIE, (1u << 1));
  wr(SDMMC2_BASE + DLBA, ((uint32_t)(uintptr_t)g_dmades) >> 2);

  wr(SDMMC2_BASE + BLKSZ, 512);
  wr(SDMMC2_BASE + BCNTR, 512);
  wr(SDMMC2_BASE + RINTR, 0xffffffff);
  wr(SDMMC2_BASE + CARG, g_read_lba);
  wr(SDMMC2_BASE + CMDR, CMDR_START | CMDR_RESP_EXPECT | CMDR_CHECK_CRC |
                             CMDR_DATA_EXPECT | CMDR_WAIT_PRE_OVER | 17);
  t = 6000000;
  while (t--)
    {
      rintr = rd(SDMMC2_BASE + RINTR);
      if (rintr & (1u << 3))
        {
          done = 1;
          break;
        }

      if (rintr & ((1u << 7) | (1u << 9) | (1u << 11) | (1u << 8) | (1u
          << 1) |
                   (1u << 6) | (1u << 15)))
        {
          done = -1;
          break;
        }
    }

  up_invalidate_dcache((uintptr_t)g_dmabuf, (uintptr_t)g_dmabuf + 64);
  printf("   CMD17 %s RINTR=%08x STAS=%08x CBCR=%08x IDST=%08x\n",
         done > 0 ? "DATA_OVER" : (done < 0 ? "ERROR" : "TIMEOUT"), rintr,
         rd(SDMMC2_BASE + STAS), rd(SDMMC2_BASE + 0x48),
         rd(SDMMC2_BASE + IDST));
  printf("   data:");
    {
      int i;
      for (i = 0; i < 4; i++)
        printf(" %08x", g_dmadat[i]);
    }

  printf("\n");
  wr(SDMMC2_BASE + RINTR, 0xffffffff);
}

/* ---------- cold-state probe ----------
 * RINTR=0x104 on CMD17 means bit8 RespTimeout|bit2 CmdDone: CMD17 gets NO
 * RESPONSE from the card, so the data phase is never even reached.  CMD13
 * after it shows state=5 (DATA) -- the card is internally busy.
 *
 * The U-Boot handoff may leave the card in transfer mode, so touching it
 * first gives a hard error.  Dump SMHC2 BEFORE any reset/clock write to see
 * what the frozen state actually is.
 *   mmcdiag c
 */

static void cold_probe(void)
{
  uint32_t rintr = 0;
  volatile int t;
  int done = 0;

  printf("c: COLD regs (no reset yet):\n");
  printf(
      "   GCTRL=%08x CLKCR=%08x TMOUT=%08x WIDTH=%08x BLKSZ=%08x"
          "BCNTR=%08x\n",
      rd(SDMMC2_BASE + 0x00), rd(SDMMC2_BASE + 0x04), rd(SDMMC2_BASE + 0x08),
      rd(SDMMC2_BASE + 0x0c), rd(SDMMC2_BASE + 0x10),
          rd(SDMMC2_BASE + 0x14));
  printf("   CMDR=%08x CARG=%08x IMASK=%08x RINTR=%08x STAS=%08x\n",
         rd(SDMMC2_BASE + 0x18), rd(SDMMC2_BASE + 0x1c),
         rd(SDMMC2_BASE + 0x30), rd(SDMMC2_BASE + 0x38),
         rd(SDMMC2_BASE + 0x3c));
  printf("   FTRGL=%08x CBCR=%08x BBCR=%08x DBGC=%08x A12A=%08x NTSR=%08x\n",
         rd(SDMMC2_BASE + 0x40), rd(SDMMC2_BASE + 0x48),
         rd(SDMMC2_BASE + 0x4c), rd(SDMMC2_BASE + 0x50),
         rd(SDMMC2_BASE + 0x58), rd(SDMMC2_BASE + 0x5c));
  printf("   DMAC=%08x DLBA=%08x IDST=%08x IDIE=%08x CHDA=%08x CCU=%08x\n",
         rd(SDMMC2_BASE + 0x80), rd(SDMMC2_BASE + 0x84),
         rd(SDMMC2_BASE + 0x88), rd(SDMMC2_BASE + 0x8c),
         rd(SDMMC2_BASE + 0x90), rd(CCU_BASE + SD2_CLK_CFG));
  printf("   pin(0x0300b048)=%08x  pin(0x0300b04c)=%08x\n", rd(0x0300b048),
         rd(0x0300b04c));

  /* now a minimal, NON-resetting CMD13 to see if the card answers at all */

  wr(SDMMC2_BASE + RINTR, 0xffffffff);
  wr(SDMMC2_BASE + IMASK, 0);
  wr(SDMMC2_BASE + CARG, 0x00010000);
  wr(SDMMC2_BASE + CMDR,
      CMDR_START | CMDR_RESP_EXPECT | CMDR_CHECK_CRC | 13);
  t = 4000000;
  while (t--)
    {
      rintr = rd(SDMMC2_BASE + RINTR);
      if (rintr & (1u << 3))
        {
          done = 1;
          break;
        }

      if (rintr & (1u << 8))
        {
          done = -2;
          break;
        }

      if (rintr & ((1u << 7) | (1u << 9) | (1u << 11) | (1u << 8) | (1u
          << 1) |
                   (1u << 6) | (1u << 15)))
        {
          done = -1;
          break;
        }
    }

  printf("   COLD CMD13 -> %s RINTR=%08x STAS=%08x RESP0=%08x\n",
         done > 0 ? "OK" : (done == -1 ? "ERROR" : "RESP_TIMEOUT"), rintr,
         rd(SDMMC2_BASE + STAS), rd(SDMMC2_BASE + 0x20));

  /* and a cold CMD17, no reset, no clock change, no descriptor setup */

  wr(SDMMC2_BASE + RINTR, 0xffffffff);
  wr(SDMMC2_BASE + CARG, g_read_lba);
  wr(SDMMC2_BASE + CMDR, CMDR_START | CMDR_RESP_EXPECT | CMDR_CHECK_CRC |
                             CMDR_DATA_EXPECT | CMDR_WAIT_PRE_OVER | 17);
  t = 4000000;
  done = 0;
  while (t--)
    {
      rintr = rd(SDMMC2_BASE + RINTR);
      if (rintr & (1u << 3))
        {
          done = 1;
          break;
        }

      if (rintr & (1u << 8))
        {
          done = -2;
          break;
        }

      if (rintr & ((1u << 7) | (1u << 9) | (1u << 11) | (1u << 8) | (1u
          << 1) |
                   (1u << 6) | (1u << 15)))
        {
          done = -1;
          break;
        }
    }

  printf("   COLD CMD17 -> %s RINTR=%08x STAS=%08x CBCR=%08x\n",
         done > 0 ? "DATA_OVER" : (done == -1 ? "ERROR" : "RESP_TIMEOUT"),
         rintr, rd(SDMMC2_BASE + STAS), rd(SDMMC2_BASE + 0x48));

  /* DECISIVE TEST: reset the CARD only (CMD0), with no controller-level
   * surgery at all, then retry.  If CMD17 starts working after CMD0, the
   * card
   * was left in transfer/DATA state by the previous owner (U-Boot) and our
   * init never recovers it.  If it still times out, the fault is physical
   * (clock/CMD line) and no software state matters.
   */

  wr(SDMMC2_BASE + RINTR, 0xffffffff);
  wr(SDMMC2_BASE + CARG, 0);
  wr(SDMMC2_BASE + CMDR, CMDR_START | CMDR_SEND_INIT_SEQ);
  t = 4000000;
  done = 0;
  while (t--)
    {
      rintr = rd(SDMMC2_BASE + RINTR);
      if (rintr & (1u << 3))
        {
          done = 1;
          break;
        }

      if (rintr & (1u << 8))
        {
          done = -2;
          break;
        }
    }

  printf("   CMD0 -> %s RINTR=%08x STAS=%08x\n",
         done > 0 ? "OK" : (done == -2 ? "RESP_TIMEOUT" : "ERROR"), rintr,
         rd(SDMMC2_BASE + STAS));

  wr(SDMMC2_BASE + RINTR, 0xffffffff);
  wr(SDMMC2_BASE + CARG, 0x00010000);
  wr(SDMMC2_BASE + CMDR,
      CMDR_START | CMDR_RESP_EXPECT | CMDR_CHECK_CRC | 13);
  t = 4000000;
  done = 0;
  while (t--)
    {
      rintr = rd(SDMMC2_BASE + RINTR);
      if (rintr & (1u << 3))
        {
          done = 1;
          break;
        }

      if (rintr & (1u << 8))
        {
          done = -2;
          break;
        }

      if (rintr & ((1u << 7) | (1u << 9) | (1u << 11) | (1u << 8) | (1u
          << 1) |
                   (1u << 6) | (1u << 15)))
        {
          done = -1;
          break;
        }
    }

  printf("   POST-CMD0 CMD13 -> %s RINTR=%08x STAS=%08x RESP0=%08x\n",
         done > 0 ? "OK" : (done == -1 ? "ERROR" : "RESP_TIMEOUT"), rintr,
         rd(SDMMC2_BASE + STAS), rd(SDMMC2_BASE + 0x20));

  wr(SDMMC2_BASE + RINTR, 0xffffffff);
  wr(SDMMC2_BASE + CARG, g_read_lba);
  wr(SDMMC2_BASE + CMDR, CMDR_START | CMDR_RESP_EXPECT | CMDR_CHECK_CRC |
                             CMDR_DATA_EXPECT | CMDR_WAIT_PRE_OVER | 17);
  t = 4000000;
  done = 0;
  while (t--)
    {
      rintr = rd(SDMMC2_BASE + RINTR);
      if (rintr & (1u << 3))
        {
          done = 1;
          break;
        }

      if (rintr & (1u << 8))
        {
          done = -2;
          break;
        }

      if (rintr & ((1u << 7) | (1u << 9) | (1u << 11) | (1u << 8) | (1u
          << 1) |
                   (1u << 6) | (1u << 15)))
        {
          done = -1;
          break;
        }
    }

  printf("   POST-CMD0 CMD17 -> %s RINTR=%08x STAS=%08x CBCR=%08x\n",
         done > 0 ? "DATA_OVER" : (done == -1 ? "ERROR" : "RESP_TIMEOUT"),
         rintr, rd(SDMMC2_BASE + STAS), rd(SDMMC2_BASE + 0x48));
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

int main(int argc, FAR char *argv[])
{
  /* DIAG: if invoked as "mmcdiag driver", call the real board driver
   * init (yuzuki_h616_sdhci_initialize) from the NSH task context and
   * report -- directly testing whether the driver path works when called
   * from a normal NSH command thread.
   */

  if (argc > 1 && argv[1][0] == 'd')
    {
      int r = yuzuki_h616_sdhci_initialize();
      printf("driver-init returned %d\n", r);
      return 0;
    }

  if (argc > 1 && argv[1][0] == 'c' && argv[1][1] == 0)
    {
      cold_probe();
      printf("=== c done ===\n");
      return 0;
    }

  if (argc > 2 && argv[1][0] == 'b')
    {
      /* mmcdiag b <lba> [<count>]: scan blocks through the CPU/AHB path */

      uint32_t lba = (uint32_t)strtoul(argv[2], NULL, 0);
      int count = (argc > 3) ? atoi(argv[3]) : 8;

      if (count < 1)
        {
          count = 1;
        }

      block_scan(lba, count);
      printf("=== b done ===\n");
      return 0;
    }

  if (argc > 2 && argv[1][0] == 'p')
    {
      int v;
      if (argv[2][0] == 'a')
        {
          for (v = 0; v < 4; v++)
            delay_probe(v);
        }
      else
        {
          delay_probe(atoi(argv[2]));
        }

      printf("=== p done ===\n");
      return 0;
    }

  if (argc > 1 && argv[1][0] == 's' && argv[1][1] == 0)
    {
      /* mmcdiag s: card CURRENT_STATE (CMD13) around a failed data read */

      s_probe();
      printf("=== s done ===\n");
      return 0;
    }

  if (argc > 2 && argv[1][0] == 'k')
    {
      /* mmcdiag k <variant>: DBGC/NTSR/GCTRL/clock/write batch */

      int v;
      if (argv[2][0] == 'a')
        {
          for (v = 0; v < 9; v++)
            batch_probe(v);
        }
      else
        {
          batch_probe(atoi(argv[2]));
        }

      printf("=== k done ===\n");
      return 0;
    }

  if (argc > 1 && argv[1][0] == 'g')
    {
      regdump_at_failure();
      printf("=== g done ===\n");
      return 0;
    }

  if (argc > 2 && argv[1][0] == 'u')
    {
      /* mmcdiag u <variant>: replay U-Boot's working read state exactly */

      int v;
      if (argv[2][0] == 'a')
        {
          for (v = 0; v < 41; v++)
            u_probe(v);
        }
      else
        {
          v = atoi(argv[2]);
          if (v < 0 || v > 40)
            {
              printf("u variant must be 0..40\n");
              return 1;
            }

          u_probe(v);
        }

      printf("=== u done ===\n");
      return 0;
    }

  if (argc > 1 && argv[1][0] == 'i')
    {
      /* DIAG: run the board driver init FIRST, then the raw mmcdiag
       * sequence -- if the raw sequence now fails, the driver init
       * corrupts the controller; if it still works, the controller is
       * fine after init and the problem is in the mmcsd layer.
       */

      int r = yuzuki_h616_sdhci_initialize();
      printf("driver-init returned %d, now raw...\n", r);
    }

  if (argc > 1 && argv[1][0] == 'n')
    {
      /* DIAG: driver init, then WITHOUT re-running controller_reset,
       * dump registers and issue a single raw CMD0 -- determines whether
       * the driver init leaves the controller in a state where commands
       * never execute.
       */

      int r = yuzuki_h616_sdhci_initialize();
      printf("driver-init returned %d, no-reset raw CMD0...\n", r);
      dump_regs("after-init");
      send_cmd(0, 0xf0f0f0f0, CMDR_SEND_INIT_SEQ);
      printf("CMD0(no-reset) done\n");
      dump_regs("after-cmd0-noreset");
      return 0;
    }

  if (argc > 1 && argv[1][0] == 'r')
    {
      /* DIAG: replicate the CURRENT h616_reset() exactly (CCU reset
       * pulse if STAS bit12/13 stuck + soft reset + TMOUT + FTRGL +
       * ID clock), then issue raw CMD0 -- determines whether the
       * h616_reset sequence itself leaves the controller command-ready.
       */

      printf("replicate h616_reset...\n");
      if ((rd(SDMMC2_BASE + STAS) & 0x6000) != 0)
        {
          printf("STAS stuck, CCU reset pulse\n");
          wr(CCU_BASE + 0x84c, rd(CCU_BASE + 0x84c) & ~(1 << 18));
          volatile int w;
          for (w = 0; w < 10000; w++)
            ;
          wr(CCU_BASE + 0x84c, rd(CCU_BASE + 0x84c) | (1 << 18) | (1 << 2));
          for (w = 0; w < 10000; w++)
            ;
          printf("after CCU reset STAS=%08x\n", rd(SDMMC2_BASE + STAS));
        }

      wr(SDMMC2_BASE + GCTRL,
         GCTRL_SOFT_RESET | GCTRL_FIFO_RESET | GCTRL_DMA_RESET);
      volatile int t = 100000;
      while ((rd(SDMMC2_BASE + GCTRL) &
              (GCTRL_SOFT_RESET | GCTRL_FIFO_RESET | GCTRL_DMA_RESET)) &&
             --t)
        ;
      wr(SDMMC2_BASE + GCTRL, GCTRL_INTERRUPT_ENABLE);
      wr(SDMMC2_BASE + RINTR, 0xffffffff);
      wr(SDMMC2_BASE + WIDTH, 1);
      wr(SDMMC2_BASE + TMOUT, 0xffffff40);
      wr(SDMMC2_BASE + 0x40, 0x20070008); /* FTRGL */
      set_ccu_clock(400000);
      clock_update();
      dump_regs("after-h616reset-replica");
      send_cmd(0, 0xf0f0f0f0, CMDR_SEND_INIT_SEQ);
      printf("CMD0(replica) done\n");
      dump_regs("after-cmd0-replica");
      return 0;
    }

  printf("=== H616 eMMC raw diag ===\n");

  /* 1. Reset controller + set ID clock */

  controller_reset();
  set_ccu_clock(400000);
  clock_update();
  printf("ID clock set (400KHz)\n");
  dump_regs("after-clk");

  /* 2. CMD0 pre-idle (arg=0xf0f0f0f0, with INIT_SEQ) */

  send_cmd(0, 0xf0f0f0f0, CMDR_SEND_INIT_SEQ);
  printf("CMD0(f0f0f0f0) done\n");
  dump_regs("after-cmd0a");

  usleep(100000);

  /* 3. CMD0 idle (arg=0) */

  send_cmd(0, 0x00000000, CMDR_SEND_INIT_SEQ);
  printf("CMD0(0) done\n");
  dump_regs("after-cmd0b");

  usleep(100000);

  /* 4. CMD1 with 3.2-3.4V + HCS (arg=0x40300000), no CRC (R3) */

  send_cmd(1, 0x40300000, CMDR_RESP_EXPECT);
  printf("CMD1(0x40300000) done\n");
  dump_regs("after-cmd1-hcs");

  usleep(50000);

  /* 5. CMD1 DRIVER-STYLE (with IMASK) */

  send_cmd_driverstyle(1, 0x40300000, CMDR_RESP_EXPECT);
  printf("CMD1 driver-style done\n");
  dump_regs("after-cmd1-driver");

  /* 6. CMD1 plain voltage (arg=0x00300000) */

  send_cmd(1, 0x00300000, CMDR_RESP_EXPECT);
  printf("CMD1(0x00300000) done\n");
  dump_regs("after-cmd1-volt");

  /* 7. Full driver-style: reset + FTRGL + CLKCR, then CMD0+CMD1 */

  printf("--- driver-style reset ---\n");
  controller_reset_driverstyle();
  dump_regs("after-drv-reset");
  send_cmd(0, 0xf0f0f0f0, CMDR_SEND_INIT_SEQ);
  usleep(100000);
  send_cmd(0, 0x00000000, CMDR_SEND_INIT_SEQ);
  usleep(100000);
  send_cmd(1, 0x40300000, CMDR_RESP_EXPECT);
  printf("driver-style CMD1 done\n");
  dump_regs("after-drv-cmd1");

  printf("=== diag done ===\n");
  return 0;
}
