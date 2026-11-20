#include "gfx.h"
#include "font8x8.h"
#include <stdlib.h>
#include <string.h>

static uint16_t* g_buf;
static int g_w, g_h;

void gfx_target(uint16_t* buf, int w, int h) { g_buf = buf; g_w = w; g_h = h; }

void gfx_px(int x, int y, uint16_t c) {
    if ((unsigned)x < (unsigned)g_w && (unsigned)y < (unsigned)g_h) g_buf[y * g_w + x] = c;
}

uint16_t gfx_get(int x, int y) {
    if ((unsigned)x < (unsigned)g_w && (unsigned)y < (unsigned)g_h) return g_buf[y * g_w + x];
    return 0;
}

uint16_t gfx_mix(uint16_t a, uint16_t b, int t) {
    int ar = (a >> 11) & 31, ag = (a >> 5) & 63, ab = a & 31;
    int br = (b >> 11) & 31, bg = (b >> 5) & 63, bb = b & 31;
    int r = ar + (br - ar) * t / 256, g = ag + (bg - ag) * t / 256, bl = ab + (bb - ab) * t / 256;
    return (uint16_t)((r << 11) | (g << 5) | bl);
}

void gfx_rect(int x, int y, int w, int h, uint16_t c) {
    int x1 = x + w, y1 = y + h, i, j;
    if (x < 0) x = 0;
    if (y < 0) y = 0;
    if (x1 > g_w) x1 = g_w;
    if (y1 > g_h) y1 = g_h;
    if (x >= x1 || y >= y1) return;
    for (j = y; j < y1; j++) {
        uint16_t* p = g_buf + j * g_w;
        for (i = x; i < x1; i++) p[i] = c;
    }
}

void gfx_frame(int x, int y, int w, int h, uint16_t c) {
    gfx_rect(x, y, w, 1, c);
    gfx_rect(x, y + h - 1, w, 1, c);
    gfx_rect(x, y, 1, h, c);
    gfx_rect(x + w - 1, y, 1, h, c);
}

void gfx_dim(int x, int y, int w, int h) {
    int x1 = x + w, y1 = y + h, i, j;
    if (x < 0) x = 0;
    if (y < 0) y = 0;
    if (x1 > g_w) x1 = g_w;
    if (y1 > g_h) y1 = g_h;
    for (j = y; j < y1; j++) {
        uint16_t* p = g_buf + j * g_w;
        for (i = x; i < x1; i++) p[i] = (uint16_t)((p[i] >> 1) & 0x7BEF);   /* half brightness */
    }
}

void gfx_ellipse(int cx, int cy, int rx, int ry, uint16_t c) {
    int y;
    if (rx < 0 || ry < 0) return;
    for (y = -ry; y <= ry; y++) {
        /* half width at this row: rx * sqrt(1 - (y/ry)^2) */
        int hw, x0, x1;
        if (ry == 0) hw = rx;
        else {
            long num = (long)rx * rx * ((long)ry * ry - (long)y * y);
            long den = (long)ry * ry;
            long q = num / den, r = 0;
            while ((r + 1) * (r + 1) <= q) r++;   /* integer sqrt; q is small (rx <= ~100) */
            hw = (int)r;
        }
        x0 = cx - hw; x1 = cx + hw;
        gfx_rect(x0, cy + y, x1 - x0 + 1, 1, c);
    }
}

void gfx_line(int x0, int y0, int x1, int y1, uint16_t c) {
    int dx = abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
    int dy = -abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
    int err = dx + dy;
    for (;;) {
        int e2;
        gfx_px(x0, y0, c);
        if (x0 == x1 && y0 == y1) break;
        e2 = 2 * err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
}

void gfx_thick(int x0, int y0, int x1, int y1, int r, uint16_t c) {
    int dx = abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
    int dy = -abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
    int err = dx + dy;
    for (;;) {
        int e2;
        gfx_ellipse(x0, y0, r, r, c);
        if (x0 == x1 && y0 == y1) break;
        e2 = 2 * err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
}

int gfx_text_w(const char* s, int scale) { return (int)strlen(s) * 8 * scale; }

void gfx_text(int x, int y, int scale, uint16_t c, const char* s) {
    for (; *s; s++, x += 8 * scale) {
        const unsigned char* g;
        int gy, gx;
        if ((unsigned char)*s >= 128) continue;
        g = font8x8_basic[(unsigned char)*s];
        for (gy = 0; gy < 8; gy++) {
            unsigned char row = g[gy];
            if (!row) continue;
            for (gx = 0; gx < 8; gx++)
                if (row & (1 << gx)) gfx_rect(x + gx * scale, y + gy * scale, scale, scale, c);
        }
    }
}

/* Text with separate horizontal and vertical scales (8x16 characters for the Q-BASIC theme: sx 1, sy 2). */
void gfx_text_xy(int x, int y, int sx, int sy, uint16_t c, const char* s) {
    for (; *s; s++, x += 8 * sx) {
        const unsigned char* g;
        int gy, gx;
        if ((unsigned char)*s >= 128) continue;
        g = font8x8_basic[(unsigned char)*s];
        for (gy = 0; gy < 8; gy++) {
            unsigned char row = g[gy];
            if (!row) continue;
            for (gx = 0; gx < 8; gx++)
                if (row & (1 << gx)) gfx_rect(x + gx * sx, y + gy * sy, sx, sy, c);
        }
    }
}

static int g_shadow = 1;
void gfx_shadows(int on) { g_shadow = on; }

void gfx_text_sh(int x, int y, int scale, uint16_t c, uint16_t shadow, const char* s) {
    if (g_shadow) gfx_text(x + scale, y + scale, scale, shadow, s);
    gfx_text(x, y, scale, c, s);
}

void gfx_text_c(int cx, int y, int scale, uint16_t c, uint16_t shadow, const char* s) {
    gfx_text_sh(cx - gfx_text_w(s, scale) / 2, y, scale, c, shadow, s);
}

Sprite* spr_new(int w, int h) {
    Sprite* s = (Sprite*)malloc(sizeof(Sprite));
    int i;
    s->w = w; s->h = h;
    s->px = (uint16_t*)malloc((size_t)w * h * sizeof(uint16_t));
    for (i = 0; i < w * h; i++) s->px[i] = KEY;
    return s;
}

void spr_blit(const Sprite* s, int x, int y) {
    int j, i;
    for (j = 0; j < s->h; j++) {
        int yy = y + j;
        const uint16_t* src;
        uint16_t* dst;
        if ((unsigned)yy >= (unsigned)g_h) continue;
        src = s->px + j * s->w;
        dst = g_buf + yy * g_w;
        for (i = 0; i < s->w; i++) {
            int xx = x + i;
            if ((unsigned)xx < (unsigned)g_w && src[i] != KEY) dst[xx] = src[i];
        }
    }
}

Sprite* spr_mirror(const Sprite* s) {
    Sprite* m = spr_new(s->w, s->h);
    int j, i;
    for (j = 0; j < s->h; j++)
        for (i = 0; i < s->w; i++) m->px[j * s->w + i] = s->px[j * s->w + (s->w - 1 - i)];
    return m;
}

void spr_outline(Sprite* s, uint16_t c) {
    int w = s->w, h = s->h, i, j;
    uint16_t* out = (uint16_t*)malloc((size_t)w * h * sizeof(uint16_t));
    memcpy(out, s->px, (size_t)w * h * sizeof(uint16_t));
    for (j = 0; j < h; j++)
        for (i = 0; i < w; i++) {
            if (s->px[j * w + i] != KEY) continue;
            if ((i > 0 && s->px[j * w + i - 1] != KEY) || (i < w - 1 && s->px[j * w + i + 1] != KEY) ||
                (j > 0 && s->px[(j - 1) * w + i] != KEY) || (j < h - 1 && s->px[(j + 1) * w + i] != KEY))
                out[j * w + i] = c;
        }
    memcpy(s->px, out, (size_t)w * h * sizeof(uint16_t));
    free(out);
}
