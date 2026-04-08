/****************************************************************************
 * apps/examples/camtest/camtest_main.c
 *
 * SPDX-License-Identifier: Apache-2.0
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

#include <fcntl.h>
#include <nuttx/config.h>
#include <nuttx/video/fb.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>

extern int esp32s3_cam_capture(uint8_t** fb, uint32_t* size);

/****************************************************************************
 * Private Functions
 ****************************************************************************/

/* Video recording mode: binary dump frames over USB CDC.
 * Usage: camtest video <frames>
 * PC side: use cam_record.py to receive and convert to mp4.
 *
 * Protocol:
 *   "RAWVIDEO:START:<frames>:<width>:<height>:<framesize>\n"
 *   [frame 0 raw binary: framesize bytes]
 *   "F\n"   (frame delimiter)
 *   [frame 1 raw binary: framesize bytes]
 *   "F\n"
 *   ...
 *   "RAWVIDEO:END\n"
 */

static int do_video(int frames, int fb_fd,
    struct fb_videoinfo_s* vinfo,
    struct fb_planeinfo_s* pinfo,
    uint8_t* lcd_fb)
{
    uint8_t* cam_fb = NULL;
    uint32_t cam_size = 0;
    struct fb_area_s area;
    int ret;
    int captured = 0;

    area.x = 0;
    area.y = 0;
    area.w = vinfo->xres;
    area.h = vinfo->yres;

    ret = esp32s3_cam_capture(&cam_fb, &cam_size);
    if (ret < 0) {
        printf("capture fail\n");
        return -1;
    }

    printf("RAWVIDEO:START:%d:%u:%u:%lu\n",
        frames, vinfo->xres, vinfo->yres,
        (unsigned long)cam_size);
    fflush(stdout);
    usleep(100000);

    for (int i = 0; i < frames; i++) {
        if (i > 0) {
            ret = esp32s3_cam_capture(&cam_fb, &cam_size);
            if (ret < 0) {
                break;
            }
        }

        write(STDOUT_FILENO, cam_fb, cam_size);
        write(STDOUT_FILENO, "F\n", 2);

        uint32_t sz = cam_size < pinfo->fblen ? cam_size : pinfo->fblen;
        memcpy(lcd_fb, cam_fb, sz);
        ioctl(fb_fd, FBIO_UPDATE, (unsigned long)((uintptr_t)&area));

        captured++;
    }

    usleep(50000);
    printf("RAWVIDEO:END:%d\n", captured);
    fflush(stdout);

    return captured;
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

int main(int argc, char* argv[])
{
    uint8_t* cam_fb = NULL;
    uint32_t cam_size = 0;
    int fd;
    int ret;
    int frames = 5;
    int video_mode = 0;
    int recv_mode = 0;
    struct fb_videoinfo_s vinfo;
    struct fb_planeinfo_s pinfo;
    struct fb_area_s area;

    if (argc > 1 && strcmp(argv[1], "video") == 0) {
        video_mode = 1;
        frames = (argc > 2) ? atoi(argv[2]) : 75;
    } else if (argc > 1 && strcmp(argv[1], "recv") == 0) {
        recv_mode = 1;
    } else if (argc > 1) {
        frames = atoi(argv[1]);
    }

    fd = open("/dev/fb0", O_RDWR);
    if (fd < 0) {
        printf("open fb0 fail\n");
        return -1;
    }

    ioctl(fd, FBIOGET_VIDEOINFO, (unsigned long)((uintptr_t)&vinfo));
    ioctl(fd, FBIOGET_PLANEINFO, (unsigned long)((uintptr_t)&pinfo));

    uint8_t* lcd_fb = mmap(NULL, pinfo.fblen, PROT_READ | PROT_WRITE,
        MAP_SHARED | MAP_FILE, fd, 0);
    if (lcd_fb == MAP_FAILED) {
        close(fd);
        return -1;
    }

    area.x = 0;
    area.y = 0;
    area.w = vinfo.xres;
    area.h = vinfo.yres;

    printf("camtest: %ux%u stride=%u fblen=%u bpp=%u\n",
        vinfo.xres, vinfo.yres, pinfo.stride, pinfo.fblen, pinfo.bpp);

    /* Video recording mode */

    if (video_mode) {
        int n = do_video(frames, fd, &vinfo, &pinfo, lcd_fb);
        printf("camtest: recorded %d frames\n", n);
        munmap(lcd_fb, pinfo.fblen);
        close(fd);
        return 0;
    }

    if (recv_mode) {
        printf("RECV:READY:%u:%u:%u\n", vinfo.xres, vinfo.yres, pinfo.fblen);
        fflush(stdout);

        char line[1024];
        uint32_t off = 0;
        while (off < pinfo.fblen && fgets(line, sizeof(line), stdin)) {
            if (strncmp(line, "RECV:END", 8) == 0)
                break;
            for (int j = 0;
                 line[j] && line[j + 1] && off < pinfo.fblen;
                 j += 2) {
                if (line[j] == '\n' || line[j] == '\r')
                    break;
                char h[3];
                h[0] = line[j];
                h[1] = line[j + 1];
                h[2] = 0;
                lcd_fb[off++] = (uint8_t)strtoul(h, NULL, 16);
            }
        }

        printf("RECV:DONE:%lu\n", (unsigned long)off);
        ioctl(fd, FBIO_UPDATE, (unsigned long)((uintptr_t)&area));
        sleep(30);
        munmap(lcd_fb, pinfo.fblen);
        close(fd);
        return 0;
    }

    /* Color diagnostic mode: 0=pure colors, -1=gradient */

    if (frames == 0 || frames == -1) {
        uint16_t* p16 = (uint16_t*)lcd_fb;
        uint32_t w = vinfo.xres;
        uint32_t h = vinfo.yres;

        if (frames == -1) {
            printf("Filling: RGB gradient %ux%u\n", w, h);
            for (uint32_t y = 0; y < h; y++) {
                for (uint32_t x = 0; x < w; x++) {
                    uint8_t r = (x * 255) / w;
                    uint8_t g = (y * 255) / h;
                    uint8_t b = 128;
                    uint16_t px = ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3);
                    p16[y * w + x] = px;
                }
            }
        } else {
            uint32_t npix = w * h;
            uint32_t quarter = npix / 4;

            printf("Filling: RED|GREEN|BLUE|GRAY (standard LE RGB565)\n");

            for (uint32_t i = 0; i < quarter; i++)
                p16[i] = 0xf800;
            for (uint32_t i = quarter; i < quarter * 2; i++)
                p16[i] = 0x07e0;
            for (uint32_t i = quarter * 2; i < quarter * 3; i++)
                p16[i] = 0x001f;
            for (uint32_t i = quarter * 3; i < npix; i++)
                p16[i] = 0x8410;
        }

        ioctl(fd, FBIO_UPDATE, (unsigned long)((uintptr_t)&area));
        printf("Should show %s\n", frames == -1 ? "gradient: left=dark->red, top=dark->green, blue=constant" : "RED|GREEN|BLUE|GRAY");
        sleep(10);
        munmap(lcd_fb, pinfo.fblen);
        close(fd);
        return 0;
    }

    /* Normal capture + display + hex dump mode */

    for (int i = 0; i < frames; i++) {
        ret = esp32s3_cam_capture(&cam_fb, &cam_size);
        if (ret < 0) {
            printf("capture fail\n");
            break;
        }

        uint32_t cam_w = 320;
        uint32_t cam_stride = cam_w * (pinfo.bpp / 8);
        uint32_t lcd_stride = pinfo.stride;
        uint32_t copy_w = lcd_stride < cam_stride ? lcd_stride : cam_stride;
        uint32_t copy_h = pinfo.fblen / lcd_stride;
        uint32_t cam_h = cam_size / cam_stride;
        if (copy_h > cam_h)
            copy_h = cam_h;

        for (uint32_t row = 0; row < copy_h; row++) {
            memcpy(lcd_fb + row * lcd_stride,
                cam_fb + row * cam_stride,
                copy_w);
        }

        ioctl(fd, FBIO_UPDATE, (unsigned long)((uintptr_t)&area));

        if (i % 10 == 0) {
            printf("frame %d/%d size=%lu\n",
                i + 1, frames, (unsigned long)cam_size);
        }
    }

    /* Hex dump last frame */

    if (cam_fb && cam_size > 0) {
        printf("RAWDUMP:START:%lu\n", (unsigned long)cam_size);
        for (uint32_t i = 0; i < cam_size; i++) {
            printf("%02x", cam_fb[i]);
            if ((i & 63) == 63) {
                printf("\n");
            }
        }

        printf("\nRAWDUMP:END\n");
    }

    sleep(5);
    munmap(lcd_fb, pinfo.fblen);
    close(fd);
    return 0;
}
