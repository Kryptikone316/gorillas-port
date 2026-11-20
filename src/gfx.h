/* Small software drawing layer on RGB565 buffers (the 640x480 screen, the world
 * layer, sprites). Everything draws into the current target. */
#ifndef GFX_H
#define GFX_H

#include <stdint.h>

#define RGB(r, g, b) ((uint16_t)((((r) & 0xF8) << 8) | (((g) & 0xFC) << 3) | (((b) & 0xFF) >> 3)))
#define KEY RGB(255, 0, 255)            /* transparent colour in sprites */

typedef struct { int w, h; uint16_t* px; } Sprite;

void gfx_target(uint16_t* buf, int w, int h);   /* stride == w */
void gfx_rect(int x, int y, int w, int h, uint16_t c);
void gfx_frame(int x, int y, int w, int h, uint16_t c);
void gfx_dim(int x, int y, int w, int h);       /* darkens a rectangle (menus over the game) */
void gfx_ellipse(int cx, int cy, int rx, int ry, uint16_t c);   /* filled */
void gfx_line(int x0, int y0, int x1, int y1, uint16_t c);
void gfx_thick(int x0, int y0, int x1, int y1, int r, uint16_t c);   /* a line of filled discs */
void gfx_px(int x, int y, uint16_t c);
uint16_t gfx_get(int x, int y);
uint16_t gfx_mix(uint16_t a, uint16_t b, int t256);   /* a + (b - a) * t / 256 */

/* Text: font8x8 scaled by an integer. */
int gfx_text_w(const char* s, int scale);
void gfx_text(int x, int y, int scale, uint16_t c, const char* s);
void gfx_text_xy(int x, int y, int sx, int sy, uint16_t c, const char* s);
void gfx_shadows(int on);                       /* drop shadows under gfx_text_sh/_c on or off (plain text for the Q-BASIC theme) */
void gfx_text_sh(int x, int y, int scale, uint16_t c, uint16_t shadow, const char* s);
void gfx_text_c(int cx, int y, int scale, uint16_t c, uint16_t shadow, const char* s);

Sprite* spr_new(int w, int h);                  /* filled with KEY */
void spr_blit(const Sprite* s, int x, int y);   /* clipped, KEY skipped */
Sprite* spr_mirror(const Sprite* s);
void spr_outline(Sprite* s, uint16_t c);        /* 1 px outline round the opaque pixels */

#endif
