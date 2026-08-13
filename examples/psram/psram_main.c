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
      printf("  test [mb]       address-in-address test (destructive)\n");
      printf("  alias           address alias detection\n");
      printf("  width           8/16/32-bit access width test\n");
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
  else
    {
      printf("psram: unknown command '%s'\n", argv[1]);
      return -EINVAL;
    }

  return OK;
}
