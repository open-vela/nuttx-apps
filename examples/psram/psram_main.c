/****************************************************************************
 * apps/examples/psram/psram_main.c
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

#include <errno.h>
#include <malloc.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/****************************************************************************
 * External Functions
 ****************************************************************************/

/* Provided by arch/arm/src/bk7258/bk7258_psram.c */

extern int bk7258_psram_init(void);
extern uint32_t bk7258_psram_get_id(void);
extern uint32_t bk7258_psram_get_size(void);
extern int bk7258_psram_probe(void);
extern int bk7258_psram_test(uint32_t size_bytes);
extern int bk7258_psram_alias(void);
extern int bk7258_psram_width(void);

extern int bk7258_psram_heap_init(void);
extern void *bk7258_psram_malloc(size_t size);
extern void *bk7258_psram_memalign(size_t alignment,
                                    size_t size);
extern void bk7258_psram_free(void *ptr);
extern void bk7258_psram_meminfo(struct mallinfo *info);

/****************************************************************************
 * Private Data
 ****************************************************************************/

/* Track allocations so freeall can release them */

#define PSRAM_MAX_ALLOCS 64

static void    *g_allocs[PSRAM_MAX_ALLOCS];
static size_t   g_alloc_sizes[PSRAM_MAX_ALLOCS];
static int      g_nallocs;

/****************************************************************************
 * Private Functions
 ****************************************************************************/

/****************************************************************************
 * Name: elapsed_us
 *
 * Description:
 *   Microseconds between two CLOCK_MONOTONIC samples.
 *
 *   We deliberately use clock_gettime() rather than the OS-internal
 *   clock_systime_ticks(): the latter is documented as "should not be
 *   called from application code", and a raw tick delta silently bakes
 *   in the CONFIG_USEC_PER_TICK value (10 ms here), which is far too
 *   coarse to be trusted for these measurements.
 *
 ****************************************************************************/

static uint32_t elapsed_us(FAR const struct timespec *a,
                           FAR const struct timespec *b)
{
  int64_t us;

  us = (int64_t)(b->tv_sec - a->tv_sec) * 1000000LL +
       ((int64_t)b->tv_nsec - (int64_t)a->tv_nsec) / 1000LL;

  return (us > 0) ? (uint32_t)us : 0;
}

/****************************************************************************
 * Name: kbps_x10
 *
 * Description:
 *   Throughput in KB/s scaled by 10 (i.e. one decimal place).
 *   Returns 0 if the interval is too short to measure.
 *
 ****************************************************************************/

static uint32_t kbps_x10(size_t nbytes, uint32_t us)
{
  if (us == 0)
    {
      return 0;
    }

  return (uint32_t)((uint64_t)nbytes * 10 * 1000000ull /
                    (uint64_t)us / 1024ull);
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

int main(int argc, FAR char *argv[])
{
  int ret;

  if (argc < 2)
    {
      printf("usage: psram <command>\n");
      printf("  id              read chip ID and size\n");
      printf("  probe           init + ID + single-word test\n");
      printf("  test [mb]       address-in-address test "
             "(destructive)\n");
      printf("  alias           address alias detection\n");
      printf("  width           8/16/32-bit access test\n");
      printf("  heap            show PSRAM heap status\n");
      printf("  alloc <KB>      alloc from PSRAM heap\n");
      printf("  align <A> <KB>  memalign alloc (A=32/64)\n");
      printf("  fbtest          camera framebuffer test\n");
      printf("  freeall         free all alloc'd blocks\n");
      return -EINVAL;
    }

  if (strcmp(argv[1], "id") == 0)
    {
      ret = bk7258_psram_init();

      if (ret < 0)
        {
          return ret;
        }

      printf("psram: chip ID = 0x%04lx\n",
             (unsigned long)bk7258_psram_get_id());
      printf("psram: size    = %lu KB (%lu MB)\n",
             (unsigned long)(bk7258_psram_get_size() / 1024),
             (unsigned long)(bk7258_psram_get_size() /
                             (1024 * 1024)));
    }
  else if (strcmp(argv[1], "probe") == 0)
    {
      return bk7258_psram_probe();
    }
  else if (strcmp(argv[1], "test") == 0)
    {
      uint32_t mb = 0;

      if (argc >= 3)
        {
          mb = (uint32_t)strtoul(argv[2], NULL, 0);
        }

      return bk7258_psram_test(mb * 1024 * 1024);
    }
  else if (strcmp(argv[1], "alias") == 0)
    {
      return bk7258_psram_alias();
    }
  else if (strcmp(argv[1], "width") == 0)
    {
      return bk7258_psram_width();
    }
  else if (strcmp(argv[1], "heap") == 0)
    {
      struct mallinfo info;

      ret = bk7258_psram_heap_init();
      if (ret < 0)
        {
          return ret;
        }

      bk7258_psram_meminfo(&info);
      printf("PSRAM heap status:\n");
      printf("  arena (total) : %u bytes (%u KB)\n",
             info.arena, info.arena / 1024);
      printf("  allocated     : %u bytes (%u KB)\n",
             info.uordblks, info.uordblks / 1024);
      printf("  free          : %u bytes (%u KB)\n",
             info.fordblks, info.fordblks / 1024);
      printf("  largest free  : %u bytes (%u KB)\n",
             info.mxordblk, info.mxordblk / 1024);
      printf("  alloc chunks  : %u\n", info.aordblks);
      printf("  free chunks   : %u\n", info.ordblks);
    }
  else if (strcmp(argv[1], "alloc") == 0)
    {
      size_t kb;
      void *ptr;
      void *sram;
      volatile uint8_t *p;
      int i;

      if (argc < 3)
        {
          printf("usage: psram alloc <KB>\n");
          return -EINVAL;
        }

      ret = bk7258_psram_heap_init();
      if (ret < 0)
        {
          return ret;
        }

      if (g_nallocs >= PSRAM_MAX_ALLOCS)
        {
          printf("alloc: slot full (max %d), "
                 "run freeall first\n", PSRAM_MAX_ALLOCS);
          return -ENOMEM;
        }

      kb = (size_t)strtoul(argv[2], NULL, 0);
      ptr = bk7258_psram_malloc(kb * 1024);
      if (ptr == NULL)
        {
          printf("alloc: %zu KB failed (out of memory)\n", kb);
          return -ENOMEM;
        }

      /* Verify address is in PSRAM window */

      if ((uintptr_t)ptr < 0x60000000 ||
          (uintptr_t)ptr >= 0x61000000)
        {
          printf("alloc: ERROR — ptr %p outside "
                 "PSRAM window!\n", ptr);
          bk7258_psram_free(ptr);
          return -EFAULT;
        }

      /* Quick read/write verify */

      p = (volatile uint8_t *)ptr;
      for (i = 0; i < 64 && i < (int)(kb * 1024); i++)
        {
          p[i] = (uint8_t)(i & 0xff);
        }

      for (i = 0; i < 64 && i < (int)(kb * 1024); i++)
        {
          if (p[i] != (uint8_t)(i & 0xff))
            {
              printf("alloc: VERIFY FAIL @ %p+%d "
                     "(expected 0x%02x, got 0x%02x)\n",
                     ptr, i, (unsigned)(i & 0xff),
                     (unsigned)p[i]);
              bk7258_psram_free(ptr);
              return -EIO;
            }
        }

      g_allocs[g_nallocs] = ptr;
      g_alloc_sizes[g_nallocs] = kb * 1024;
      g_nallocs++;

      printf("alloc: %zu KB @ %p (OK, slot %d)\n",
             kb, ptr, g_nallocs - 1);

      /* Prove that ordinary malloc still returns SRAM */

      sram = malloc(1024);
      if (sram != NULL)
        {
          printf("  verify: malloc(1024)=%p %s\n",
                 sram,
                 ((uintptr_t)sram >= 0x60000000 &&
                  (uintptr_t)sram < 0x61000000)
                 ? "POLLUTED!" : "(SRAM OK)");
          free(sram);
        }
    }
  else if (strcmp(argv[1], "align") == 0)
    {
      size_t align;
      size_t kb;
      void *ptr;

      if (argc < 4)
        {
          printf("usage: psram align <alignment> <KB>\n");
          return -EINVAL;
        }

      ret = bk7258_psram_heap_init();
      if (ret < 0)
        {
          return ret;
        }

      if (g_nallocs >= PSRAM_MAX_ALLOCS)
        {
          printf("align: slot full, run freeall first\n");
          return -ENOMEM;
        }

      align = (size_t)strtoul(argv[2], NULL, 0);
      kb = (size_t)strtoul(argv[3], NULL, 0);

      /* Validate before use: the mod checks below would divide by
       * zero on align == 0, and a non-power-of-two alignment can
       * never be satisfied.
       */

      if (align == 0 || (align & (align - 1)) != 0)
        {
          printf("align: alignment must be a non-zero power "
                 "of two (got %zu)\n", align);
          return -EINVAL;
        }

      if (kb == 0)
        {
          printf("align: size must be >= 1 KB\n");
          return -EINVAL;
        }

      ptr = bk7258_psram_memalign(align, kb * 1024);
      if (ptr == NULL)
        {
          printf("align: memalign(%zu, %zu KB) failed\n",
                 align, kb);
          return -ENOMEM;
        }

      printf("align: %zu KB @ %p (align %zu, "
             "mod=%zu)\n",
             kb, ptr, align,
             (size_t)((uintptr_t)ptr % align));

      if ((uintptr_t)ptr % align != 0)
        {
          printf("  ERROR: alignment not met!\n");
          bk7258_psram_free(ptr);
          return -EIO;
        }

      g_allocs[g_nallocs] = ptr;
      g_alloc_sizes[g_nallocs] = kb * 1024;
      g_nallocs++;
    }
  else if (strcmp(argv[1], "fbtest") == 0)
    {
      /* Camera framebuffer validation: alloc 614 KB at
       * 32 and 64 byte alignment, verify, read/write test,
       * measure bandwidth, free all.
       */

      size_t fb_kb = 614;
      int a;

      ret = bk7258_psram_heap_init();
      if (ret < 0)
        {
          return ret;
        }

      printf("fbtest: camera buffer %zu KB\n", fb_kb);

      for (a = 0; a < 2; a++)
        {
          size_t al = (a == 0) ? 32 : 64;
          void *p;
          volatile uint8_t *v;
          struct timespec t0;
          struct timespec t1;
          uint32_t wr_us;
          uint32_t rd_us;
          uint32_t wr_x10;
          uint32_t rd_x10;
          size_t i;
          size_t nbytes = fb_kb * 1024;

          if (g_nallocs >= PSRAM_MAX_ALLOCS)
            {
              printf("  slot full, run freeall\n");
              return -ENOMEM;
            }

          p = bk7258_psram_memalign(al, nbytes);
          if (p == NULL)
            {
              printf("  align %zu: alloc FAILED\n", al);
              return -ENOMEM;
            }

          printf("  align %zu: %p (mod=%zu) ",
                 al, p, (size_t)((uintptr_t)p % al));

          if ((uintptr_t)p % al != 0)
            {
              printf("FAIL\n");
              bk7258_psram_free(p);
              return -EIO;
            }

          g_allocs[g_nallocs] = p;
          g_alloc_sizes[g_nallocs] = nbytes;
          g_nallocs++;

          /* Write pattern */

          v = (volatile uint8_t *)p;
          clock_gettime(CLOCK_MONOTONIC, &t0);
          for (i = 0; i < nbytes; i++)
            {
              v[i] = (uint8_t)(i & 0xff);
            }

          clock_gettime(CLOCK_MONOTONIC, &t1);
          wr_us = elapsed_us(&t0, &t1);

          /* Read verify */

          clock_gettime(CLOCK_MONOTONIC, &t0);
          for (i = 0; i < nbytes; i++)
            {
              if (v[i] != (uint8_t)(i & 0xff))
                {
                  printf("FAIL @ +%zu\n", i);
                  return -EIO;
                }
            }

          clock_gettime(CLOCK_MONOTONIC, &t1);
          rd_us = elapsed_us(&t0, &t1);

          /* Throughput in KB/s x10 (one decimal place). */

          wr_x10 = kbps_x10(nbytes, wr_us);
          rd_x10 = kbps_x10(nbytes, rd_us);

          printf("OK, %lu.%lu / %lu.%lu KB/s (wr/rd), "
                 "%lu / %lu ms\n",
                 (unsigned long)(wr_x10 / 10),
                 (unsigned long)(wr_x10 % 10),
                 (unsigned long)(rd_x10 / 10),
                 (unsigned long)(rd_x10 % 10),
                 (unsigned long)(wr_us / 1000),
                 (unsigned long)(rd_us / 1000));
        }

      printf("fbtest: all passed, %d blocks allocated\n",
             g_nallocs);
    }
  else if (strcmp(argv[1], "freeall") == 0)
    {
      int i;

      if (g_nallocs == 0)
        {
          printf("freeall: nothing to free\n");
          return OK;
        }

      for (i = 0; i < g_nallocs; i++)
        {
          bk7258_psram_free(g_allocs[i]);
        }

      printf("freeall: released %d block(s)\n", g_nallocs);
      g_nallocs = 0;
    }
  else
    {
      printf("psram: unknown command '%s'\n", argv[1]);
      return -EINVAL;
    }

  return OK;
}
