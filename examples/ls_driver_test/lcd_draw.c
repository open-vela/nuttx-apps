/****************************************************************************
 * apps/examples/ls_driver_test/lcd_draw.c
 *
 * Minimal drawing layer over /dev/fb0 for the LS2K0300 RGB panel.
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Licensed to the Apache Software Foundation (ASF) under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.
 * The ASF licenses this file to you under the Apache License, Version 2.0
 * (the "License"); you may not use this file except in compliance with the
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
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <syslog.h>

#include <nuttx/video/fb.h>

#include "lcd_draw.h"
#include "oled_fonts.h"

/****************************************************************************
 * Public Functions
 ****************************************************************************/

int lcd_open(FAR struct lcd_s *lcd)
{
  struct fb_videoinfo_s vinfo;
  struct fb_planeinfo_s pinfo;

  memset(lcd, 0, sizeof(*lcd));
  lcd->fd = -1;

  lcd->fd = open("/dev/fb0", O_RDWR);
  if (lcd->fd < 0)
    {
      int err = errno;
      syslog(LOG_ERR, "lcd: open /dev/fb0 failed: %d\n", err);
      return -err;
    }

  if (ioctl(lcd->fd, FBIOGET_VIDEOINFO, (unsigned long)&vinfo) < 0 ||
      ioctl(lcd->fd, FBIOGET_PLANEINFO, (unsigned long)&pinfo) < 0)
    {
      int err = errno;
      syslog(LOG_ERR, "lcd: FBIOGET_* failed: %d\n", err);
      close(lcd->fd);
      lcd->fd = -1;
      return -err;
    }

  lcd->xres   = vinfo.xres;
  lcd->yres   = vinfo.yres;
  lcd->stride = pinfo.stride;

  /* mmap is the supported way in: in the FLAT build it hands back the
   * kernel pointer directly, and in the KERNEL build it performs the
   * address mapping a raw pinfo.fbmem would lack.
   */

  lcd->fb = mmap(NULL, pinfo.fblen, PROT_READ | PROT_WRITE,
                 MAP_SHARED | MAP_FILE, lcd->fd, 0);
  if (lcd->fb == MAP_FAILED)
    {
      int err = errno;
      syslog(LOG_ERR, "lcd: mmap failed: %d\n", err);
      close(lcd->fd);
      lcd->fd = -1;
      return -err;
    }

  syslog(LOG_INFO,
         "lcd: /dev/fb0 %dx%d stride=%d bpp=%d fb=%p\n",
         lcd->xres, lcd->yres, lcd->stride, pinfo.bpp, lcd->fb);
  return OK;
}

void lcd_close(FAR struct lcd_s *lcd)
{
  if (lcd->fb != NULL && lcd->fb != MAP_FAILED)
    {
      munmap(lcd->fb, (size_t)lcd->stride * lcd->yres);
    }

  if (lcd->fd >= 0)
    {
      close(lcd->fd);
    }

  lcd->fb = NULL;
  lcd->fd = -1;
}

void lcd_putpixel(FAR struct lcd_s *lcd, int x, int y, uint32_t rgb)
{
  if (x < 0 || y < 0 || x >= lcd->xres || y >= lcd->yres)
    {
      return;
    }

  lcd->fb[(y * (lcd->stride >> 2)) + x] = rgb;
}

void lcd_fill_rect(FAR struct lcd_s *lcd, int x, int y, int w, int h,
                   uint32_t rgb)
{
  int x0 = x > 0 ? x : 0;
  int y0 = y > 0 ? y : 0;
  int x1 = x + w < lcd->xres ? x + w : lcd->xres;
  int y1 = y + h < lcd->yres ? y + h : lcd->yres;
  int px;
  int py;

  for (py = y0; py < y1; py++)
    {
      FAR uint32_t *line = lcd->fb + (py * (lcd->stride >> 2));

      for (px = x0; px < x1; px++)
        {
          line[px] = rgb;
        }
    }
}

void lcd_clear(FAR struct lcd_s *lcd, uint32_t rgb)
{
  lcd_fill_rect(lcd, 0, 0, lcd->xres, lcd->yres, rgb);
}

void lcd_draw_hline(FAR struct lcd_s *lcd, int x, int y, int w,
                    uint32_t rgb)
{
  lcd_fill_rect(lcd, x, y, w, 1, rgb);
}

void lcd_draw_vline(FAR struct lcd_s *lcd, int x, int y, int h,
                    uint32_t rgb)
{
  lcd_fill_rect(lcd, x, y, 1, h, rgb);
}

void lcd_draw_circle(FAR struct lcd_s *lcd, int cx, int cy, int r,
                     uint32_t rgb, bool filled)
{
  /* Bounding-box scan with an integer distance test.  r is at most a few
   * dozen pixels, so the cost is trivial and there is no sqrt to get
   * wrong.  An unfilled circle is drawn as a ring of thickness
   * LCD_RING_W.
   */

  int inner = filled ? 0 : (r - LCD_RING_W);
  long inner2 = (long)inner * inner;
  long outer2 = (long)r * r;
  int x;
  int y;

  if (inner < 0)
    {
      inner2 = 0;
    }

  for (y = -r; y <= r; y++)
    {
      for (x = -r; x <= r; x++)
        {
          long d2 = (long)x * x + (long)y * y;

          if (d2 <= outer2 && d2 > inner2)
            {
              lcd_putpixel(lcd, cx + x, cy + y, rgb);
            }
        }
    }
}

void lcd_draw_char(FAR struct lcd_s *lcd, int x, int y, char ch,
                   uint32_t rgb, int scale)
{
  FAR const unsigned char *glyph;
  int col;
  int row;

  if (ch < ' ' || ch > '~')
    {
      ch = ' ';
    }

  /* F8X16 layout: 16 bytes per glyph, the first 8 are the top block and
   * the next 8 the bottom block, byte index = column, bit index = row
   * with bit0 at the top of the block.
   */

  glyph = &F8X16[((unsigned char)(ch - ' ')) * 16];

  for (col = 0; col < LCD_FONT_W; col++)
    {
      for (row = 0; row < LCD_FONT_H; row++)
        {
          unsigned char bits = (row < 8) ? glyph[col] : glyph[8 + col];

          if ((bits >> (row & 7)) & 1)
            {
              lcd_fill_rect(lcd,
                            x + col * scale, y + row * scale,
                            scale, scale, rgb);
            }
        }
    }
}

void lcd_draw_text(FAR struct lcd_s *lcd, int x, int y,
                   FAR const char *s, uint32_t rgb, int scale)
{
  if (s == NULL)
    {
      return;
    }

  while (*s != '\0')
    {
      lcd_draw_char(lcd, x, y, *s, rgb, scale);
      x += LCD_FONT_W * scale;
      s++;
    }
}

int lcd_text_width(FAR const char *s, int scale)
{
  return (s == NULL) ? 0 : (int)strlen(s) * LCD_FONT_W * scale;
}

int lcd_font_height(int scale)
{
  return LCD_FONT_H * scale;
}
