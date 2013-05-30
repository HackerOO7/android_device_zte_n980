/*
 * Copyright (C) 2007 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <stdbool.h>
#include <stdlib.h>
#include <unistd.h>

#include <fcntl.h>
#include <stdio.h>

#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/types.h>

#include <linux/fb.h>
#include <linux/kd.h>

#include <pixelflinger/pixelflinger.h>

#ifdef BOARD_USE_CUSTOM_RECOVERY_FONT
#include BOARD_USE_CUSTOM_RECOVERY_FONT
#else
#include "font_10x18.h"
#endif

#include "minui.h"

char PIXEL_SIZE;
char PIXEL_FORMAT;

#define NUM_BUFFERS 2

typedef struct {
    GGLSurface texture;
    unsigned cwidth;
    unsigned cheight;
    unsigned ascent;
} GRFont;

static GRFont *gr_font = 0;
static GGLContext *gr_context = 0;
static GGLSurface gr_font_texture;
static GGLSurface gr_framebuffer[NUM_BUFFERS];
static GGLSurface gr_mem_surface;
static unsigned gr_active_fb = 0;
static unsigned double_buffering = 0;

static int gr_fb_fd = -1;
static int gr_vt_fd = -1;

static struct fb_var_screeninfo vi;
static struct fb_fix_screeninfo fi;

inline size_t roundUpToPageSize(size_t x) {
    return (x + (PAGE_SIZE-1)) & ~(PAGE_SIZE-1);
}
static int get_framebuffer(GGLSurface *fb)
{
    int fd;
    void *bits;

    fd = open("/dev/graphics/fb0", O_RDWR);
    if (fd < 0) {
        perror("cannot open fb0");
        return -1;
    }

    if (ioctl(fd, FBIOGET_VSCREENINFO, &vi) < 0) {
        perror("failed to get fb0 info");
        close(fd);
        return -1;
    }
#if 0
    fprintf(stderr, "\tGetting VSCREENINFO\n");
    fprintf(stderr, "\t\txres=%u, yres=%u\n", vi.xres, vi.yres);
    fprintf(stderr, "\t\txres_virtual=%u, yres_virtual=%u\n", vi.xres_virtual, vi.yres_virtual);
    fprintf(stderr, "\t\txoffset=%u, yoffset=%u\n", vi.xoffset, vi.yoffset);
    fprintf(stderr, "\t\tbits_per_pixel=%u, grayscale=%u\n", vi.bits_per_pixel, vi.grayscale);
    fprintf(stderr, "\t\tred:\n\t\t\toffset=%u, length=%u, msb_right=%u\n", vi.red.offset, vi.red.length, vi.red.msb_right);
    fprintf(stderr, "\t\tgreen:\n\t\t\toffset=%u, length=%u, msb_right=%u\n", vi.green.offset, vi.green.length, vi.green.msb_right);
    fprintf(stderr, "\t\tblue:\n\t\t\toffset=%u, length=%u, msb_right=%u\n", vi.blue.offset, vi.blue.length, vi.blue.msb_right);
    fprintf(stderr, "\t\ttransp:\n\t\t\toffset=%u, length=%u, msb_right=%u\n", vi.transp.offset, vi.transp.length, vi.transp.msb_right);
    fprintf(stderr, "\t\tnonstd=%u, activate=%u\n", vi.nonstd, vi.activate);
    fprintf(stderr, "\t\theight=%u, width=%u\n", vi.height, vi.width);
    fprintf(stderr, "\t\taccel_flags=%u, pixclock=%u\n", vi.accel_flags, vi.pixclock);
    fprintf(stderr, "\t\tleft_margin=%u, right_margin=%u\n", vi.left_margin, vi.right_margin);
    fprintf(stderr, "\t\tupper_margin=%u, lower_margin=%u\n", vi.upper_margin, vi.lower_margin);
    fprintf(stderr, "\t\thsync_len=%u, vsync_len=%u\n", vi.hsync_len, vi.vsync_len);
    fprintf(stderr, "\t\tsync=%u, vmode=%u\n", vi.sync, vi.vmode);
    fprintf(stderr, "\t\trotate=%u, reserved={%u,%u,%u,%u,%u}\n", vi.rotate, vi.reserved[0], vi.reserved[1], vi.reserved[2], vi.reserved[3], vi.reserved[4]);
#endif

    switch (vi.bits_per_pixel) {
    case 16:
        PIXEL_SIZE = 2;
        PIXEL_FORMAT = GGL_PIXEL_FORMAT_RGB_565;
        break;
    case 24:
        PIXEL_SIZE = 3;
        PIXEL_FORMAT = GGL_PIXEL_FORMAT_RGB_888;
        break;
    case 32:
        PIXEL_SIZE = 4;
        if(vi.red.offset == 8)
            PIXEL_FORMAT = GGL_PIXEL_FORMAT_BGRA_8888;
        else if(vi.transp.length == 0)
            PIXEL_FORMAT = GGL_PIXEL_FORMAT_RGBX_8888;
        else
            PIXEL_FORMAT = GGL_PIXEL_FORMAT_RGBA_8888;
        break;
    default:
        perror("Unknown Pixel Format\n");
        close(fd);
        return -1;
    }

    if (ioctl(fd, FBIOGET_FSCREENINFO, &fi) < 0) {
        perror("failed to get fb0 info");
        close(fd);
        return -1;
    }

    //printf("fi.smem_len=%d, vi.yres=%d, fi.line_length=%d\n",fi.smem_len,vi.yres,fi.line_length);

    bits = mmap(0, fi.smem_len, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);//auto roundUpToPageSize
    if (bits == MAP_FAILED) {
        perror("failed to mmap framebuffer");
        close(fd);
        return -1;
    }

    fb->version = sizeof(*fb);
    fb->width = vi.xres;
    fb->height = vi.yres;
    fb->stride = fi.line_length/PIXEL_SIZE;
    fb->data = bits;
    fb->format = PIXEL_FORMAT;
    memset(fb->data, 0,roundUpToPageSize(vi.yres * fi.line_length));

    fb++;

    /* check if we can use double buffering */
    if (roundUpToPageSize(vi.yres * fi.line_length) * 2 > roundUpToPageSize(fi.smem_len))
        return fd;

    double_buffering = 1;

    fb->version = sizeof(*fb);
    fb->width = vi.xres;
    fb->height = vi.yres;
    fb->stride = fi.line_length/PIXEL_SIZE;
    fb->data = (void*) (((unsigned) bits) + roundUpToPageSize(vi.yres * fi.line_length));
    fb->format = PIXEL_FORMAT;
    memset(fb->data, 0, roundUpToPageSize(vi.yres * fi.line_length));

    return fd;
}

static void get_memory_surface(GGLSurface* ms) {
  ms->version = sizeof(*ms);
  ms->width = vi.xres;
  ms->height = vi.yres;
  ms->stride = fi.line_length/PIXEL_SIZE;
  ms->data = malloc(fi.line_length * vi.yres);
  ms->format = PIXEL_FORMAT;
}

static void set_active_framebuffer(unsigned n)
{
    if (n > 1 || !double_buffering) return;
    vi.yres_virtual = vi.yres * NUM_BUFFERS;
    vi.yoffset = n * vi.yres;
    vi.bits_per_pixel = PIXEL_SIZE * 8;
    if (ioctl(gr_fb_fd, FBIOPUT_VSCREENINFO, &vi) < 0) {
        perror("active fb swap failed");
    }
}

void gr_flip(void)
{
    GGLContext *gl = gr_context;

    /* swap front and back buffers */
    if (double_buffering)
        gr_active_fb = (gr_active_fb + 1) & 1;

    /* copy data from the in-memory surface to the buffer we're about
     * to make active. */
    memcpy(gr_framebuffer[gr_active_fb].data, gr_mem_surface.data,
           fi.line_length * vi.yres);

    /* inform the display driver */
    set_active_framebuffer(gr_active_fb);
}

void gr_color(unsigned char r, unsigned char g, unsigned char b, unsigned char a)
{
    GGLContext *gl = gr_context;
    GGLint color[4];
    color[0] = ((r << 8) | r) + 1;
    color[1] = ((g << 8) | g) + 1;
    color[2] = ((b << 8) | b) + 1;
    color[3] = ((a << 8) | a) + 1;
    gl->color4xv(gl, color);
}

/**
int getGBCharID(unsigned c1, unsigned c2)
{
    if (c1 >= 0xB0 && c1 <=0xF7 && c2>=0xA1 && c2<=0xFE)
    {
        return (c1-0xB0)*94+c2-0xA1;
    }
    return -1;
}

int getUNICharID(unsigned short unicode)
{
    int i;
    for (i = 0; i < UNICODE_NUM; i++) {
        if (unicode == unicodemap[i]) return i;
    }
    return -1;
}
**/

int gr_measure(const char *s)
{
    int length = 0;
    int cols = 0;
    char *ptr;
    int text_max_x = gr_fb_width();
    for (ptr = s; (*ptr != '\0') && (*ptr != '\n'); ptr++,cols++) {
        if (((*ptr) & 0xF0) == 0xE0) {
            if (length + gr_font->cheight >= text_max_x) {
                break;
            }
            length += gr_font->cheight;
            ptr+=2;
        } else {
            if (length + gr_font->cwidth >= text_max_x) {
                break;
            }
            length += gr_font->cwidth;
        }
    }
    return length;
//    return gr_font->cwidth * strlen(s);
}

void gr_font_size(int *x, int *y)
{
    *x = gr_font->cwidth;
    *y = gr_font->cheight;
}

/**
int gr_text(int x, int y, const char *s)
{
    GGLContext *gl = gr_context;
    GRFont *font = gr_font;
    unsigned off;
    unsigned off2;
    unsigned off3;
    int id;
    unsigned short unicode;

    y -= font->ascent;

    gl->bindTexture(gl, &font->texture);
    gl->texEnvi(gl, GGL_TEXTURE_ENV, GGL_TEXTURE_ENV_MODE, GGL_REPLACE);
    gl->texGeni(gl, GGL_S, GGL_TEXTURE_GEN_MODE, GGL_ONE_TO_ONE);
    gl->texGeni(gl, GGL_T, GGL_TEXTURE_GEN_MODE, GGL_ONE_TO_ONE);
    gl->enable(gl, GGL_TEXTURE_2D);

    while((off = *s++)) {
        if (off < 0x80)
        {
            off -= 32;
            if (off < 96) {
            if ((x + font->cwidth) >= gr_fb_width()) return x;
                gl->texCoord2i(gl, (off * font->cheight) - x, 0 - y);
                gl->recti(gl, x, y, x + font->cwidth, y + font->cheight);
            }
            x += font->cwidth;
        }
        else
        {
            if ((off & 0xF0) == 0xE0)
            {
                off2 = *s++;
                off3 = *s++;
                unicode = (off & 0x1F) << 12;
                unicode |= (off2 & 0x3F) << 6;
                unicode |= (off3 & 0x3F);
                id = getUNICharID(unicode);
                //LOGI("%X %X %X  %X  %d", off, off2, off3, unicode, id);
                if (id >= 0) {
                    if ((x + font->cheight) >= gr_fb_width()) return x;
                    gl->texCoord2i(gl, ((id % 96) * font->cheight) - x, (id / 96 + 1) * font->cheight - y);
                    gl->recti(gl, x, y, x + font->cheight, y + font->cheight);
                    x += font->cheight;
                } else {
                    x += font->cheight;
                }
            } else {
                x += font->cwidth;
            }
        }
    }

    return x;
}
**/

int gr_text(int x, int y, const char *s)
{
    GGLContext *gl = gr_context;
    GRFont *font = gr_font;
    unsigned off;

    y -= font->ascent;

    gl->bindTexture(gl, &font->texture);
    gl->texEnvi(gl, GGL_TEXTURE_ENV, GGL_TEXTURE_ENV_MODE, GGL_REPLACE);
    gl->texGeni(gl, GGL_S, GGL_TEXTURE_GEN_MODE, GGL_ONE_TO_ONE);
    gl->texGeni(gl, GGL_T, GGL_TEXTURE_GEN_MODE, GGL_ONE_TO_ONE);
    gl->enable(gl, GGL_TEXTURE_2D);

    while((off = *s++)) {
        off -= 32;
        if (off < 96) {
            gl->texCoord2i(gl, (off * font->cwidth) - x, 0 - y);
            gl->recti(gl, x, y, x + font->cwidth, y + font->cheight);
        }
        x += font->cwidth;
    }

    return x;
}

void gr_texticon(int x, int y, gr_surface icon) {
    if (gr_context == NULL || icon == NULL) {
        return;
    }
    GGLContext* gl = gr_context;

    gl->bindTexture(gl, (GGLSurface*) icon);
    gl->texEnvi(gl, GGL_TEXTURE_ENV, GGL_TEXTURE_ENV_MODE, GGL_REPLACE);
    gl->texGeni(gl, GGL_S, GGL_TEXTURE_GEN_MODE, GGL_ONE_TO_ONE);
    gl->texGeni(gl, GGL_T, GGL_TEXTURE_GEN_MODE, GGL_ONE_TO_ONE);
    gl->enable(gl, GGL_TEXTURE_2D);

    int w = gr_get_width(icon);
    int h = gr_get_height(icon);

    gl->texCoord2i(gl, -x, -y);
    gl->recti(gl, x, y, x+gr_get_width(icon), y+gr_get_height(icon));
}

void gr_fill(int x, int y, int w, int h)
{
    GGLContext *gl = gr_context;
    gl->disable(gl, GGL_TEXTURE_2D);
    gl->recti(gl, x, y, w, h);
}

void gr_blit(gr_surface source, int sx, int sy, int w, int h, int dx, int dy) {
    if (gr_context == NULL || source == NULL) {
        return;
    }
    GGLContext *gl = gr_context;

    gl->bindTexture(gl, (GGLSurface*) source);
    gl->texEnvi(gl, GGL_TEXTURE_ENV, GGL_TEXTURE_ENV_MODE, GGL_REPLACE);
    gl->texGeni(gl, GGL_S, GGL_TEXTURE_GEN_MODE, GGL_ONE_TO_ONE);
    gl->texGeni(gl, GGL_T, GGL_TEXTURE_GEN_MODE, GGL_ONE_TO_ONE);
    gl->enable(gl, GGL_TEXTURE_2D);
    gl->texCoord2i(gl, sx - dx, sy - dy);
    gl->recti(gl, dx, dy, dx + w, dy + h);
}

unsigned int gr_get_width(gr_surface surface) {
    if (surface == NULL) {
        return 0;
    }
    return ((GGLSurface*) surface)->width;
}

unsigned int gr_get_height(gr_surface surface) {
    if (surface == NULL) {
        return 0;
    }
    return ((GGLSurface*) surface)->height;
}

static void gr_init_font(void)
{
    GGLSurface *ftex;
    unsigned char *bits, *rle;
    unsigned char *in, data;

    gr_font = calloc(sizeof(*gr_font), 1);
    ftex = &gr_font->texture;

    bits = malloc(font.width * font.height);

    ftex->version = sizeof(*ftex);
    ftex->width = font.width;
    ftex->height = font.height;
    ftex->stride = font.width;
    ftex->data = (void*) bits;
    ftex->format = GGL_PIXEL_FORMAT_A_8;

    in = font.rundata;
    while((data = *in++)) {
        memset(bits, (data & 0x80) ? 255 : 0, data & 0x7f);
        bits += (data & 0x7f);
    }

    gr_font->cwidth = font.cwidth;
    gr_font->cheight = font.cheight;
    gr_font->ascent = font.cheight - 2;
}

int gr_init(void)
{
    gglInit(&gr_context);
    GGLContext *gl = gr_context;

    gr_init_font();
    gr_vt_fd = open("/dev/tty0", O_RDWR | O_SYNC);
    if (gr_vt_fd < 0) {
        // This is non-fatal; post-Cupcake kernels don't have tty0.
        perror("can't open /dev/tty0");
    } else if (ioctl(gr_vt_fd, KDSETMODE, (void*) KD_GRAPHICS)) {
        // However, if we do open tty0, we expect the ioctl to work.
        perror("failed KDSETMODE to KD_GRAPHICS on tty0");
        gr_exit();
        return -1;
    }

    gr_fb_fd = get_framebuffer(gr_framebuffer);
    if (gr_fb_fd < 0) {
        gr_exit();
        return -1;
    }

    get_memory_surface(&gr_mem_surface);

    fprintf(stderr, "framebuffer: fd %d (%d x %d)\n",
            gr_fb_fd, gr_framebuffer[0].width, gr_framebuffer[0].height);

        /* start with 0 as front (displayed) and 1 as back (drawing) */
    gr_active_fb = 0;
    set_active_framebuffer(0);
    gl->colorBuffer(gl, &gr_mem_surface);

    gl->activeTexture(gl, 0);
    gl->enable(gl, GGL_BLEND);
    gl->blendFunc(gl, GGL_SRC_ALPHA, GGL_ONE_MINUS_SRC_ALPHA);

    gr_fb_blank(false);

    return 0;
}

void gr_exit(void)
{
    close(gr_fb_fd);
    gr_fb_fd = -1;

    free(gr_mem_surface.data);

    ioctl(gr_vt_fd, KDSETMODE, (void*) KD_TEXT);
    close(gr_vt_fd);
    gr_vt_fd = -1;
}

int gr_fb_width(void)
{
    return gr_framebuffer[0].width;
}

int gr_fb_height(void)
{
    return gr_framebuffer[0].height;
}

//    Add for custom virtualkeys
/**
int gr_fb_height_wo_vk(void)
{
    return gr_framebuffer[0].height - board_touch_button_height;
}
**/

gr_pixel *gr_fb_data(void)
{
    return (unsigned short *) gr_mem_surface.data;
}

void gr_fb_blank(bool blank)
{
    int ret;

    ret = ioctl(gr_fb_fd, FBIOBLANK, blank ? FB_BLANK_POWERDOWN : FB_BLANK_UNBLANK);
    if (ret < 0)
        perror("ioctl(): blank");
}
