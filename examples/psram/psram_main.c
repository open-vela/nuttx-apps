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

/* Temporary buffer for SRAM verification */

static char g_sram_verify[16];

/****************************************************************************
 * Public Functions
 ****************************************************************************/

int main(int argc, FAR char *argv[])
{
  if (argc < 2)
    {
      printf("usage: psram <command>\n");
      printf("  id              read chip ID and size\n");
      printf("  probe           init + ID + single-word data test\n");
      printf("  test [mb]       address-in-address test "
             "(destructive)\n");
      printf("  alias           address alias detection\n");
      printf("  width           8/16/32-bit access width test\n");
      printf("  heap            show PSRAM heap status\n");
      printf("  alloc <KB>      alloc from PSRAM heap\n");
      printf("  freeall         free all alloc'd blocks\n");
      return -EINVAL;
    }

  if (strcmp(argv[1], "id") == 0)
    {
      int ret = bk7258_psram_init();

      if (ret < 0)
        {
          return ret;
        }

      printf("psram: chip ID = 0x%04x\n", bk7258_psram_get_id());
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
      int ret;

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
      volatile uint8_t *p;
      int ret;
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

      /* Prove that static data lives in SRAM, not PSRAM */

      printf("  verify: static @ %p "
             "(SRAM %s)\n",
             g_sram_verify,
             ((uintptr_t)g_sram_verify >= 0x28000000 &&
              (uintptr_t)g_sram_verify < 0x29000000)
             ? "OK" : "UNEXPECTED");
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
