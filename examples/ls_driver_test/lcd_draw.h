/****************************************************************************
 * apps/examples/ls_driver_test/lcd_draw.h
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

#ifndef __APPS_EXAMPLES_LS_DRIVER_TEST_LCD_DRAW_H
#define __APPS_EXAMPLES_LS_DRIVER_TEST_LCD_DRAW_H

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>

#include <stdbool.h>
#include <stdint.h>

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/* Pixel values are 0x00RRGGBB -- the layout U-Boot's blitter writes and
 * that NuttX calls FB_FMT_RGB32.
 */

#define LCD_RGB(r, g, b)  (((uint32_t)(r) << 16) | \
                           ((uint32_t)(g) << 8) | (uint32_t)(b))

/* Panel palette (deep background, high contrast -- reads well on camera) */

#define LCD_COLOR_BG      LCD_RGB(0x1a, 0x23, 0x32)
#define LCD_COLOR_TEXT    LCD_RGB(0xff, 0xff, 0xff)
#define LCD_COLOR_DIM     LCD_RGB(0x4a, 0x55, 0x68)
#define LCD_COLOR_DIV     LCD_RGB(0x2d, 0x37, 0x48)
#define LCD_COLOR_RED     LCD_RGB(0xff, 0x3b, 0x30)
#define LCD_COLOR_GREEN   LCD_RGB(0x34, 0xc7, 0x59)
#define LCD_COLOR_BLUE    LCD_RGB(0x0a, 0x84, 0xff)

/* Glyph metrics: F8X16 bitmap scaled up by an integer factor */

#define LCD_FONT_W        8
#define LCD_FONT_H        16

/* Ring thickness for an unfilled circle (the "LED off" indicator) */

#define LCD_RING_W        3

/****************************************************************************
 * Public Types
 ****************************************************************************/

struct lcd_s
{
  int fd;
  FAR uint32_t *fb;   /* mmap'd framebuffer, one 0x00RRGGBB per pixel */
  int xres;
  int yres;
  int stride;         /* bytes per scanline */
};

/****************************************************************************
 * Public Function Prototypes
 ****************************************************************************/

int lcd_open(FAR struct lcd_s *lcd);
void lcd_close(FAR struct lcd_s *lcd);

void lcd_putpixel(FAR struct lcd_s *lcd, int x, int y, uint32_t rgb);
void lcd_fill_rect(FAR struct lcd_s *lcd, int x, int y, int w, int h,
                   uint32_t rgb);
void lcd_clear(FAR struct lcd_s *lcd, uint32_t rgb);
void lcd_draw_hline(FAR struct lcd_s *lcd, int x, int y, int w,
                    uint32_t rgb);
void lcd_draw_vline(FAR struct lcd_s *lcd, int x, int y, int h,
                    uint32_t rgb);

void lcd_draw_circle(FAR struct lcd_s *lcd, int cx, int cy, int r,
                     uint32_t rgb, bool filled);

void lcd_draw_char(FAR struct lcd_s *lcd, int x, int y, char ch,
                   uint32_t rgb, int scale);
void lcd_draw_text(FAR struct lcd_s *lcd, int x, int y,
                   FAR const char *s, uint32_t rgb, int scale);

int lcd_text_width(FAR const char *s, int scale);
int lcd_font_height(int scale);

#endif /* __APPS_EXAMPLES_LS_DRIVER_TEST_LCD_DRAW_H */
