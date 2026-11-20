#include "theme.h"
#include "world.h"
#include "gfx.h"
#include <stdlib.h>
#include <string.h>

/* The themes' own random numbers: the game's generator is never touched. */
static uint32_t g_c = 1357911u;
static int crnd(int n) { g_c = g_c * 1664525u + 1013904223u; return (int)((g_c >> 8) % (uint32_t)n) + 1; }

#define MIXC(a, b, t) gfx_mix((a), (b), (t))
#define BLACK RGB(0, 0, 0)
#define WHITE RGB(255, 255, 255)

static const char* const k_name[NTHEME] = { "Q-BASIC", "Classic", "Uptown", "Moonlit", "Harbor" };
const char* theme_name(int t) { return (t >= 0 && t < NTHEME) ? k_name[t] : "?"; }
int theme_default_sky(int t) { return t == TH_QBASIC ? 6 : (t == TH_MOONLIT ? 4 : (t == TH_HARBOR ? 3 : 0)); }   /* Q-BASIC sun, smiley sun (Classic, Uptown), full moon, crescent */

uint16_t theme_sky_color(int th, int night, int y) {
    int t = y * 256 / BASELINE;
    if (t < 0) t = 0;
    if (t > 256) t = 256;
    if (th == TH_QBASIC) return RGB(0, 0, 170);       /* always the original's EGA blue, night or not */
    if (night) {
        if (th == TH_MOONLIT) return MIXC(RGB(2, 4, 24), RGB(26, 40, 94), t);
        if (th == TH_HARBOR) return MIXC(RGB(4, 8, 34), RGB(44, 70, 126), t);
        return MIXC(RGB(4, 6, 36), RGB(34, 50, 116), t);
    }
    if (th == TH_CLASSIC) return MIXC(RGB(10, 16, 104), RGB(86, 138, 224), t);       /* the port's original gradient */
    return MIXC(RGB(0, 22, 176), RGB(74, 134, 240), t);
}

/* ---- what a city remembers, so craters can take roof ornaments with them and keep the street furniture ---- */
typedef struct { int x, w, top, pal, tex, style, awning; } Bld;
static Bld g_bl[MAXB];
static int g_nbl;
typedef struct { int x0, y0, x1, y1, base, alive; } Orn;
#define MAXORN 64
static Orn g_orn[MAXORN];
static int g_norn;
typedef struct { Sprite* s; int x, y; } Prop;
static Prop g_prop[16];
static int g_nprop;
static uint8_t* g_deco;                 /* per pixel: ornament number + 1 (0 = none) */

static Sprite *s_tree, *s_lamp, *s_lamp_lit, *s_stall_r, *s_stall_g, *s_barrel, *s_crate, *s_bollard;

/* ---- small helpers ---- */
static uint16_t tint(uint16_t c) { return W.night ? MIXC(c, RGB(12, 18, 66), W.theme == TH_MOONLIT ? 118 : 96) : c; }
static void rect_a(int x, int y, int w, int h, uint16_t c, int t) {         /* a rectangle blended over what is there */
    int i, j;
    for (j = y; j < y + h; j++)
        for (i = x; i < x + w; i++)
            if (i >= 0 && i < SCR_W && j >= 0 && j < SCR_H) gfx_px(i, j, MIXC(gfx_get(i, j), c, t));
}
static void blob(int cx, int cy, int rx, int ry, uint16_t col, int amax) {   /* a soft ellipse into W.sky */
    int x, y;
    for (y = cy - ry; y <= cy + ry; y++) {
        if (y < 0 || y >= BASELINE) continue;
        for (x = cx - rx; x <= cx + rx; x++) {
            long nx, ny, d;
            if (x < 0 || x >= SCR_W) continue;
            nx = (long)(x - cx) * 256 / rx; ny = (long)(y - cy) * 256 / ry;
            d = nx * nx + ny * ny;
            if (d >= 65536) continue;
            W.sky[y * SCR_W + x] = MIXC(W.sky[y * SCR_W + x], col, (int)((65536 - d) * amax / 65536));
        }
    }
}
static void tri(int cx, int base, int hw, int h, uint16_t c) {                /* a gable/spire: apex above base */
    int k;
    for (k = 0; k < h; k++) { int ww = hw * k / h; gfx_rect(cx - ww, base - h + k, ww * 2 + 1, 1, c); }
}

/* ---- sprites for the street ---- */
static Sprite* mk(int w, int h) { Sprite* s = spr_new(w, h); gfx_target(s->px, w, h); return s; }

static void make_props(void) {
    uint16_t dk = RGB(36, 36, 46);
    int i, k;
    s_tree = mk(34, 44);
    gfx_rect(15, 28, 4, 16, RGB(84, 56, 34)); gfx_rect(17, 28, 2, 16, RGB(60, 40, 26));
    gfx_ellipse(17, 15, 13, 12, RGB(30, 92, 40)); gfx_ellipse(9, 21, 8, 8, RGB(30, 92, 40)); gfx_ellipse(25, 21, 8, 8, RGB(30, 92, 40));
    gfx_ellipse(14, 12, 9, 7, RGB(46, 128, 52)); gfx_ellipse(22, 18, 6, 5, RGB(46, 128, 52)); gfx_ellipse(9, 20, 5, 4, RGB(46, 128, 52));
    gfx_ellipse(12, 9, 4, 3, RGB(90, 170, 80)); gfx_ellipse(8, 18, 2, 2, RGB(90, 170, 80));
    spr_outline(s_tree, RGB(14, 44, 22));
    for (k = 0; k < 2; k++) {                                   /* lamp post: dull by day, a lit lantern with a dithered glow by night */
        Sprite* s = mk(28, 46);
        if (k) {
            int dx, dy;
            for (dy = -8; dy <= 8; dy++)
                for (dx = -9; dx <= 9; dx++)
                    if (dx * dx * 100 / 81 + dy * dy * 100 / 64 <= 100 && ((dx + dy) & 1) == 0) gfx_px(14 + dx, 8 + dy, RGB(255, 214, 120));
        }
        gfx_rect(13, 14, 2, 32, dk); gfx_rect(11, 42, 6, 4, dk); gfx_rect(11, 13, 6, 2, dk);
        gfx_rect(11, 3, 6, 9, dk); gfx_rect(12, 4, 4, 7, k ? RGB(255, 236, 150) : RGB(190, 194, 206)); gfx_rect(10, 1, 8, 3, dk);
        if (k) s_lamp_lit = s; else s_lamp = s;
    }
    for (k = 0; k < 2; k++) {                                   /* market stall: striped awning over a table of goods */
        Sprite* s = mk(32, 30);
        uint16_t a = k ? RGB(40, 140, 70) : RGB(196, 44, 44);
        for (i = 0; i < 8; i++) gfx_rect(1 + i * 4, 0, 4, 8, (i & 1) ? RGB(236, 236, 230) : a);
        gfx_rect(0, 8, 32, 1, MIXC(a, BLACK, 90));
        gfx_rect(2, 9, 2, 21, RGB(96, 64, 36)); gfx_rect(28, 9, 2, 21, RGB(96, 64, 36));
        gfx_rect(4, 20, 24, 5, RGB(130, 88, 48)); gfx_rect(4, 25, 24, 5, RGB(90, 60, 34));
        for (i = 0; i < 5; i++) gfx_rect(6 + i * 4, 17, 3, 3, i % 3 == 0 ? RGB(210, 50, 40) : (i % 3 == 1 ? RGB(240, 200, 50) : RGB(80, 170, 60)));
        if (k) s_stall_g = s; else s_stall_r = s;
    }
    s_barrel = mk(12, 14);
    gfx_rect(1, 0, 10, 14, RGB(124, 80, 42)); gfx_rect(0, 3, 12, 8, RGB(124, 80, 42));
    gfx_rect(0, 3, 12, 1, RGB(50, 40, 40)); gfx_rect(0, 10, 12, 1, RGB(50, 40, 40)); gfx_rect(3, 0, 1, 14, RGB(92, 58, 30));
    s_crate = mk(15, 14);
    gfx_rect(0, 0, 15, 14, RGB(150, 110, 60)); gfx_frame(0, 0, 15, 14, RGB(88, 60, 32));
    gfx_line(1, 1, 13, 12, RGB(100, 70, 38)); gfx_line(13, 1, 1, 12, RGB(100, 70, 38));
    s_bollard = mk(8, 12);
    gfx_rect(1, 2, 6, 10, RGB(52, 54, 62)); gfx_rect(0, 0, 8, 3, RGB(86, 88, 100)); gfx_rect(5, 3, 2, 9, RGB(36, 38, 46));
}

void theme_init(void) {
    g_deco = (uint8_t*)calloc((size_t)SCR_W * SCR_H, 1);
    make_props();
}

void theme_begin(void) {
    g_nbl = 0; g_norn = 0; g_nprop = 0;
    memset(g_deco, 0, (size_t)SCR_W * SCR_H);
}

/* ---- the backdrop ---- */
static void silhouette(uint16_t col, int hmin, int hmax, int gothic, uint16_t lit) {
    int x = -6;
    while (x < SCR_W) {
        int w = 14 + crnd(30), h = hmin + crnd(hmax - hmin), top = BASELINE - h, gx, gy;
        gfx_rect(x, top, w, h, col);
        if (crnd(3) == 1) gfx_rect(x + w / 4, top - 10 - crnd(14), w / 2, 30, col);                       /* a stepped crown */
        if (gothic && crnd(5) <= 3) tri(x + w / 2, top, w / 2 + 1, 10 + crnd(16), col);                     /* a pointed roof */
        if (crnd(4) == 1) gfx_rect(x + w / 2, top - 20 - crnd(16), 2, 40, col);                           /* a spire or an aerial */
        if (lit)
            for (gy = top + 8; gy < BASELINE - 6; gy += 10)
                for (gx = x + 3; gx < x + w - 4; gx += 6)
                    if (crnd(9) == 1) gfx_rect(gx, gy, 2, 3, lit);
        x += w + crnd(3) - 1;
    }
}

static void strip(void) {
    int y0 = BASELINE, i, h = SCR_H - BASELINE;
    uint16_t walk, road;
    if (W.theme == TH_CLASSIC) {
        gfx_rect(0, y0, SCR_W, h, RGB(14, 14, 36)); gfx_rect(0, y0, SCR_W, 1, RGB(60, 60, 110));
        return;
    }
    if (W.theme == TH_QBASIC) {
        gfx_rect(0, y0, SCR_W, h, RGB(0, 0, 170)); gfx_rect(0, SCR_H - 16, SCR_W, 16, RGB(0, 0, 0));    /* sky colour, then the black bar */
        return;
    }
    if (W.theme == TH_HARBOR) {                                  /* a stone quay and the dark water below it */
        gfx_rect(0, y0, SCR_W, 8, RGB(64, 62, 74));
        for (i = 0; i < SCR_W; i += 22) gfx_rect(i + ((i / 22) & 1) * 9, y0 + 2, 1, 6, RGB(40, 40, 50));
        gfx_rect(0, y0, SCR_W, 2, RGB(110, 108, 122)); gfx_rect(0, y0 + 8, SCR_W, 1, RGB(28, 28, 38));
        for (i = y0 + 9; i < SCR_H; i++) gfx_rect(0, i, SCR_W, 1, MIXC(RGB(14, 30, 70), RGB(6, 12, 34), (i - y0 - 9) * 256 / (h - 9)));
        for (i = 0; i < 70; i++) { int sx = crnd(SCR_W - 4), sy = crnd(h - 13); gfx_rect(sx, y0 + 11 + sy, 3, 1, RGB(48, 76, 130)); }   /* (one call per random number: argument order is the compiler's) */
        return;
    }
    walk = W.night ? RGB(50, 52, 68) : RGB(80, 82, 100);
    road = W.night ? RGB(18, 20, 30) : RGB(30, 32, 44);
    gfx_rect(0, y0, SCR_W, 10, walk);
    for (i = 0; i < SCR_W; i += 26) gfx_rect(i, y0, 1, 10, MIXC(walk, BLACK, 80));
    gfx_rect(0, y0, SCR_W, 1, MIXC(walk, WHITE, 60)); gfx_rect(0, y0 + 10, SCR_W, 2, MIXC(walk, WHITE, 40));
    gfx_rect(0, y0 + 12, SCR_W, h - 12, road);
    for (i = 8; i < SCR_W; i += 48) gfx_rect(i, y0 + 21, 24, 3, W.night ? RGB(150, 118, 30) : RGB(200, 160, 30));
}

void theme_backdrop(void) {
    int i, k;
    gfx_target(W.sky, SCR_W, SCR_H);
    if (W.theme == TH_UPTOWN) {
        silhouette(RGB(3, 5, 20), 140, 330, 0, 0);
    } else if (W.theme == TH_MOONLIT || W.theme == TH_HARBOR) {
        if (W.night) {                                       /* cloud banks lit from below by the city */
            for (k = 0; k < 10; k++) {
                int cx = crnd(SCR_W) - 1, cy = 50 + crnd(190);
                for (i = 0; i < 5; i++) {
                    int jx = crnd(20), jy = crnd(14), rx = crnd(40), ry = crnd(10), am = crnd(50);
                    blob(cx + (i - 2) * 38 + jx, cy + jy - 7, 50 + rx, 12 + ry, RGB(64, 82, 138), 60 + am);
                }
            }
        }
        if (W.theme == TH_MOONLIT) {
            silhouette(RGB(8, 12, 34), 150, 340, 1, RGB(170, 126, 56));
            for (i = 0; i < 6; i++) rect_a(0, BASELINE - 100 + i * 17, SCR_W, 17, RGB(60, 80, 140), 12 + i * 8);   /* mist at the foot of the skyline */
            for (i = 120; i < 330; i++) {                    /* a searchlight beam climbing to the clouds */
                int t = (i - 120) * 256 / 210, cx = 470 + (t * 160) / 256, hw = 42 - 38 * t / 256, x;
                for (x = cx - hw; x <= cx + hw; x++) {
                    int a = (256 - abs(x - cx) * 256 / hw) * (70 - 38 * t / 256) / 256;
                    if (x >= 0 && x < SCR_W) W.sky[i * SCR_W + x] = MIXC(W.sky[i * SCR_W + x], RGB(196, 212, 255), a);
                }
            }
            blob(470, 118, 46, 16, RGB(210, 224, 255), 170);
        } else {
            int x;
            silhouette(RGB(8, 12, 30), 70, 190, 0, RGB(150, 112, 50));
            for (x = 440; x < SCR_W; x++) {                  /* the headland and its lighthouse */
                int d = x - 560, hh = 150 - d * d / 90;
                if (hh > 0) gfx_rect(x, BASELINE - hh, 1, hh, RGB(6, 10, 22));
            }
            for (i = 0; i < 6; i++) gfx_rect(556, BASELINE - 150 - 54 + i * 9, 13, 9, (i & 1) ? RGB(214, 214, 220) : RGB(190, 50, 50));
            gfx_rect(555, BASELINE - 150 - 62, 15, 8, RGB(255, 236, 150)); gfx_rect(553, BASELINE - 150 - 65, 19, 3, RGB(20, 24, 40));
            tri(562, BASELINE - 150 - 65, 8, 9, RGB(20, 24, 40));
            for (i = 0; i < 360; i++) {                      /* its beam, sweeping out over the bay */
                int t = i * 256 / 360, x = 553 - i, y = BASELINE - 150 - 58 - (i / 8), hw = 2 + i / 14, yy;
                for (yy = y - hw; yy <= y + hw; yy++)
                    if (x >= 0 && yy >= 0 && yy < BASELINE) W.sky[yy * SCR_W + x] = MIXC(W.sky[yy * SCR_W + x], RGB(255, 240, 180), (256 - abs(yy - y) * 256 / hw) * (62 - 34 * t / 256) / 256);
            }
            for (k = 0; k < 2; k++) {                        /* masts of ships moored behind the street */
                int mx = k ? 410 : 96;
                gfx_rect(mx, BASELINE - 180, 2, 180, RGB(8, 10, 24));
                gfx_rect(mx - 22, BASELINE - 150, 46, 1, RGB(8, 10, 24)); gfx_rect(mx - 16, BASELINE - 112, 34, 1, RGB(8, 10, 24));
                gfx_line(mx + 1, BASELINE - 180, mx - 24, BASELINE - 100, RGB(8, 10, 24));
                gfx_line(mx + 1, BASELINE - 180, mx + 26, BASELINE - 100, RGB(8, 10, 24));
            }
            for (i = 0; i < 5; i++) rect_a(0, BASELINE - 60 + i * 12, SCR_W, 12, RGB(60, 80, 140), 10 + i * 8);
        }
    }
    strip();
    gfx_target(W.pic, SCR_W, SCR_H);
}

/* ---- buildings ---- */
static const struct { int r, g, b; } k_wall[7] = {
    { 214, 170, 118 }, { 170, 70, 50 }, { 140, 142, 152 }, { 206, 150, 46 }, { 58, 104, 190 }, { 120, 134, 104 }, { 196, 88, 48 } };
static const struct { int r, g, b; } k_plank[5] = { { 70, 112, 114 }, { 142, 62, 48 }, { 172, 150, 110 }, { 92, 100, 124 }, { 60, 88, 72 } };

static void brick_fill(int x, int y, int w, int h, uint16_t base, int seed) {
    uint16_t mort = MIXC(base, BLACK, 70), hi = MIXC(base, WHITE, 30), lo = MIXC(base, BLACK, 26);
    int i, j;
    for (j = 0; j < h; j++) {
        int row = j / 4;
        for (i = 0; i < w; i++) {
            int bx = i + ((row & 1) ? 4 : 0);
            uint16_t c;
            if (j % 4 == 3 || bx % 8 == 7) c = mort;
            else { int hs = ((bx / 8) * 7 + row * 13 + seed) & 7; c = hs == 0 ? hi : (hs == 1 ? lo : base); }
            gfx_px(x + i, y + j, c);
        }
    }
}
static void plank_fill(int x, int y, int w, int h, uint16_t base, int seed) {
    uint16_t line = MIXC(base, BLACK, 90), hi = MIXC(base, WHITE, 22);
    int i, j;
    for (j = 0; j < h; j++) {
        int row = j / 3;
        for (i = 0; i < w; i++) {
            uint16_t c = base;
            if (j % 3 == 2) c = line;
            else if (((i + row * 11 + seed) % 23) == 0) c = line;
            else if (j % 3 == 0 && ((i + row * 5) & 7) < 4) c = hi;
            gfx_px(x + i, y + j, c);
        }
    }
}

static void window(int x, int y, int w, int h, int lit, int arch, uint16_t wall, int sill) {
    uint16_t frame = tint(RGB(30, 24, 28)), top, bot;
    int j;
    if (W.night && lit) {                                         /* a pool of window light on the wall */
        uint16_t gl = MIXC(wall, RGB(255, 190, 80), W.theme == TH_MOONLIT ? 70 : 46);
        gfx_rect(x - 2, y - 2, w + 4, h + 4, gl);
    }
    gfx_rect(x - 1, y - 1, w + 2, h + 2, frame);
    if (lit) { top = RGB(255, 236, 120); bot = RGB(250, 190, 64); } else { top = tint(RGB(40, 48, 78)); bot = tint(RGB(20, 26, 46)); }
    for (j = 0; j < h; j++) gfx_rect(x, y + j, w, 1, MIXC(top, bot, j * 256 / h));
    gfx_rect(x + w / 2, y, 1, h, frame); gfx_rect(x, y + h / 2, w, 1, frame);
    if (arch) {                                                   /* round the top corners */
        gfx_px(x - 1, y - 1, wall); gfx_px(x + w, y - 1, wall); gfx_px(x, y, frame); gfx_px(x + w - 1, y, frame);
    }
    if (sill == 1) gfx_rect(x - 2, y + h + 1, w + 4, 2, MIXC(wall, WHITE, 90));
    if (sill == 2) { gfx_rect(x - 2, y + h - 3, w + 4, 1, frame); for (j = -1; j < w + 2; j += 2) gfx_rect(x + j, y + h - 3, 1, 4, frame); }
}

/* Classic: the port's own first look: outlined, bevelled blocks in teal, brick and slate, with lit windows. */
static void classic_building(int x, int w, int top, int color) {
    static const struct { int r, g, b; } col[3] = { { 172, 56, 52 }, { 40, 150, 156 }, { 130, 134, 154 } };
    int wx, wy;
    uint16_t base = RGB(col[color].r, col[color].g, col[color].b), edge = RGB(12, 12, 30);
    if (W.night) base = MIXC(base, RGB(8, 12, 56), 72);            /* a cool, moonlit tone; the windows stay lit */
    gfx_rect(x - 1, top - 1, w + 2, BASELINE - top + 1, edge);
    gfx_rect(x, top, w, BASELINE - top, base);
    gfx_rect(x, top, w, 3, MIXC(base, WHITE, 70));                  /* roof edge */
    gfx_rect(x + w - 3, top + 3, 3, BASELINE - top - 3, MIXC(base, BLACK, 80));      /* shaded side */
    for (wx = x + 5; wx + 6 < x + w - 4; wx += 12)
        for (wy = top + 10; wy + 10 < BASELINE - 4; wy += 20)
            gfx_rect(wx, wy, 6, 10, crnd(4) == 1 ? RGB(46, 48, 74) : RGB(255, 226, 96));
}

static void qbasic_building(int x, int w, int top, int color) {
    static const uint16_t col[3] = { RGB(170, 0, 0), RGB(0, 170, 170), RGB(170, 170, 170) };
    int wx, wy;
    gfx_rect(x, top, w, BASELINE - top, col[color]);
    for (wx = x + 3; wx + 4 < x + w - 1; wx += 10)         /* the original's grid: 4 x 7 windows (9 here: the screen is taller) every 10 x 15 */
        for (wy = top + 4; wy + 9 < BASELINE - 3; wy += 20)
            gfx_rect(wx, wy, 4, 9, crnd(4) == 1 ? RGB(85, 85, 85) : RGB(255, 255, 85));
}

static void modern_building(Bld* b, int x, int w, int top, int color) {
    static const int map[3][3] = { { 1, 6, 3 }, { 4, 5, 0 }, { 2, 0, 3 } };
    int i, j, cols, gx0, wy, lit_pc = W.night ? 82 : 70, seed = crnd(50);
    uint16_t base, edge = tint(RGB(10, 10, 24)), wall;
    b->pal = map[color][crnd(3) - 1];
    b->tex = (W.theme == TH_HARBOR && crnd(2) == 1);
    b->style = crnd(3) - 1;
    b->awning = (w >= 46 && crnd(3) == 1) ? crnd(2) : 0;
    if (b->tex) { int p = crnd(5) - 1; base = RGB(k_plank[p].r, k_plank[p].g, k_plank[p].b); }
    else base = RGB(k_wall[b->pal].r, k_wall[b->pal].g, k_wall[b->pal].b);
    base = tint(base);
    wall = base;
    gfx_rect(x - 1, top - 1, w + 2, BASELINE - top + 1, edge);
    if (b->tex) plank_fill(x, top, w, BASELINE - top, base, seed); else brick_fill(x, top, w, BASELINE - top, base, seed);
    rect_a(x + w - 4, top + 5, 4, BASELINE - top - 5, BLACK, 70);                      /* the shaded side */
    gfx_rect(x, top, w, 3, MIXC(base, WHITE, 80));                                       /* cornice */
    gfx_rect(x, top + 3, w, 2, MIXC(base, BLACK, 100));
    for (i = x + 2; i < x + w - 3; i += 4) gfx_rect(i, top + 5, 2, 2, MIXC(base, BLACK, 90));
    cols = (w - 4) / 12; if (cols < 1) cols = 1;
    gx0 = x + (w - cols * 12) / 2 + 2;
    for (wy = top + 11; wy + 11 < BASELINE - 38; wy += 20)
        for (j = 0; j < cols; j++)
            window(gx0 + j * 12, wy, 7, 11, crnd(100) <= lit_pc, b->style == 1, wall, b->style == 2 ? 2 : 1);
    gfx_rect(x, BASELINE - 8, w, 8, MIXC(tint(RGB(110, 108, 120)), base, 60));        /* the stone course at street level */
    for (i = x + 3; i < x + w - 3; i += 9) gfx_rect(i, BASELINE - 8, 1, 8, MIXC(base, BLACK, 100));
    if (b->awning) {                                                                      /* a shop: striped awning over a lit window */
        int aw = w - 12 > 34 ? 34 : w - 12, ax = x + (w - aw) / 2;
        uint16_t a = b->awning == 1 ? RGB(196, 44, 44) : RGB(40, 140, 70);
        gfx_rect(ax + 1, BASELINE - 24, aw - 2, 14, MIXC(RGB(255, 210, 100), base, 40));
        for (i = 0; i < aw; i += 4) gfx_rect(ax + i, BASELINE - 32, 4, 8, ((i / 4) & 1) ? tint(RGB(236, 236, 230)) : tint(a));
        gfx_rect(ax, BASELINE - 24, aw, 1, MIXC(a, BLACK, 100));
    } else {                                                                              /* a door under an arch */
        int dx = x + w / 2 - 5;
        gfx_rect(dx - 1, BASELINE - 22, 12, 22, MIXC(base, BLACK, 110));
        gfx_rect(dx, BASELINE - 20, 10, 20, tint(RGB(76, 50, 32)));
        gfx_rect(dx + 1, BASELINE - 19, 8, 5, W.night ? RGB(255, 214, 110) : RGB(150, 170, 200));
        gfx_rect(dx + 7, BASELINE - 9, 1, 2, RGB(230, 200, 90));
    }
}

void theme_building(int x, int w, int top, int color) {
    Bld* b = &g_bl[g_nbl < MAXB ? g_nbl : MAXB - 1];
    b->x = x; b->w = w; b->top = top;
    g_nbl++;
    gfx_target(W.pic, SCR_W, SCR_H);
    if (W.theme == TH_QBASIC) qbasic_building(x, w, top, color);
    else if (W.theme == TH_CLASSIC) classic_building(x, w, top, color);
    else modern_building(b, x, w, top, color);
}

/* ---- roof ornaments and street furniture ---- */
static void orn_done(int x0, int y0, int x1, int y1, int base) {
    Orn* o;
    int i, j;
    if (g_norn >= MAXORN) return;
    o = &g_orn[g_norn];
    o->x0 = x0; o->y0 = y0; o->x1 = x1; o->y1 = y1; o->base = base; o->alive = 1;
    for (j = y0; j <= y1; j++)
        for (i = x0; i <= x1; i++)
            if (i >= 0 && i < SCR_W && j >= 0 && j < BASELINE && W.pic[j * SCR_W + i] != W.sky[j * SCR_W + i]) g_deco[j * SCR_W + i] = (uint8_t)(g_norn + 1);
    g_norn++;
}

static void ornament(const Bld* b) {
    int type = crnd(4), x, top = b->top;
    uint16_t dk = tint(RGB(40, 40, 52));
    if (b->w < 28) type = 3;
    x = b->x + 6 + crnd(b->w - 24 > 1 ? b->w - 24 : 1) - 1;
    gfx_target(W.pic, SCR_W, SCR_H);
    if (type == 1) {                                                        /* water tower */
        uint16_t wood = tint(RGB(124, 84, 48)), band = tint(RGB(60, 42, 30));
        int k;
        gfx_rect(x + 2, top - 13, 2, 13, dk); gfx_rect(x + 14, top - 13, 2, 13, dk);
        gfx_line(x + 3, top - 13, x + 14, top - 1, dk); gfx_line(x + 14, top - 13, x + 3, top - 1, dk);
        gfx_rect(x, top - 26, 18, 13, wood);
        for (k = 0; k < 3; k++) gfx_rect(x, top - 24 + k * 5, 18, 1, band);
        gfx_rect(x + 5, top - 26, 1, 13, band); gfx_rect(x + 12, top - 26, 1, 13, band);
        for (k = 0; k < 6; k++) gfx_rect(x + 9 - 2 - k * 2 + 1, top - 32 + k, 4 + k * 4 - 2, 1, tint(RGB(96, 58, 40)));
        orn_done(x, top - 33, x + 17, top - 1, top);
    } else if (type == 2) {                                                 /* chimney stacks */
        uint16_t br = tint(RGB(150, 70, 56));
        gfx_rect(x, top - 14, 5, 14, br); gfx_rect(x - 1, top - 17, 7, 3, MIXC(br, BLACK, 70));
        gfx_rect(x + 9, top - 9, 5, 9, br); gfx_rect(x + 8, top - 12, 7, 3, MIXC(br, BLACK, 70));
        gfx_rect(x + 1, top - 14, 1, 14, MIXC(br, WHITE, 40));
        orn_done(x - 1, top - 17, x + 14, top - 1, top);
    } else if (type == 3) {                                                 /* aerial with a beacon */
        gfx_rect(x + 4, top - 28, 1, 28, dk); gfx_rect(x, top - 22, 9, 1, dk); gfx_rect(x + 1, top - 16, 7, 1, dk);
        gfx_rect(x + 3, top - 31, 3, 3, W.night ? RGB(255, 70, 60) : RGB(200, 50, 50));
        orn_done(x, top - 31, x + 8, top - 1, top);
    } else {                                                                /* a small spire */
        int cx = b->x + b->w / 2;
        tri(cx, top, 9, 18, MIXC(tint(RGB(k_wall[b->pal].r, k_wall[b->pal].g, k_wall[b->pal].b)), tint(RGB(70, 90, 130)), 110));
        gfx_rect(cx, top - 28, 1, 10, dk);
        orn_done(cx - 9, top - 28, cx + 9, top - 1, top);
    }
}

static void put_prop(Sprite* s, int x, int y) {
    int i, j;
    if (g_nprop >= 16) return;
    g_prop[g_nprop].s = s; g_prop[g_nprop].x = x; g_prop[g_nprop].y = y; g_nprop++;
    for (j = 0; j < s->h; j++)
        for (i = 0; i < s->w; i++) {
            uint16_t c = s->px[j * s->w + i];
            int px = x + i, py = y + j;
            if (c != KEY && px >= 0 && px < SCR_W && py >= 0 && py < SCR_H) W.pic[py * SCR_W + px] = c;
        }
}

void theme_finish(void) {
    int i, tries;
    if (W.theme <= TH_CLASSIC) return;
    gfx_target(W.pic, SCR_W, SCR_H);
    for (i = 0; i < g_nbl && i < MAXB; i++)
        if (i != W.gb[0] && i != W.gb[1] && crnd(100) <= 48) ornament(&g_bl[i]);
    for (tries = 0; tries < 40 && g_nprop < 8; tries++) {
        int x = 6 + crnd(SCR_W - 52) - 1, k, ok = 1, r = crnd(100);
        Sprite* s;
        for (k = 0; k < g_nprop; k++) if (abs(g_prop[k].x - x) < 54) ok = 0;
        if (!ok) continue;
        if (W.theme == TH_HARBOR) s = r <= 28 ? (W.night ? s_lamp_lit : s_lamp) : (r <= 52 ? s_barrel : (r <= 74 ? s_crate : s_bollard));
        else if (W.theme == TH_MOONLIT) s = r <= 45 ? s_lamp_lit : (r <= 75 ? (crnd(2) == 1 ? s_stall_r : s_stall_g) : s_tree);
        else s = r <= 38 ? s_tree : (r <= 68 ? (W.night ? s_lamp_lit : s_lamp) : (crnd(2) == 1 ? s_stall_r : s_stall_g));
        put_prop(s, x, BASELINE + 8 - s->h);
        if (s == s_lamp_lit) rect_a(x - 6, BASELINE + 1, 40, 10, RGB(255, 196, 96), 30);              /* a pool of lamplight on the pavement */
    }
    if (W.theme == TH_HARBOR) {                                           /* the city's lights, and the moon, on the water */
        int k;
        gfx_target(W.pic, SCR_W, SCR_H);
        for (i = 0; i < g_nbl && i < MAXB; i++)
            for (k = 0; k < 2; k++) {
                int x = g_bl[i].x + 4 + crnd(g_bl[i].w > 10 ? g_bl[i].w - 8 : 1) - 1, y;
                for (y = BASELINE + 10; y < SCR_H - 1; y += 3) {
                    int xx = x + ((y / 3) & 1) - (k & 1);
                    gfx_px(xx, y, MIXC(gfx_get(xx, y), RGB(255, 190, 80), 120)); gfx_px(xx, y + 1, MIXC(gfx_get(xx, y + 1), RGB(255, 190, 80), 70));
                }
            }
        for (i = 0; i < 9; i++) rect_a(SCR_W / 2 - 18 + (i & 1) * 4 - i, BASELINE + 11 + i * 2, 20 + i * 2, 1, RGB(210, 226, 255), 90 - i * 6);
    }
}

void theme_carve(int cx, int cy, int rx, int ry) {
    int i, j, k;
    for (j = cy - ry; j <= cy + ry; j++) {                  /* what the crater cut away is no longer an ornament's */
        long dy = j - cy;
        if (j < 0 || j >= BASELINE) continue;
        for (i = cx - rx; i <= cx + rx; i++) {
            long dx = i - cx;
            if (i >= 0 && i < SCR_W && dx * dx * (long)ry * ry + dy * dy * (long)rx * rx <= (long)rx * rx * ry * ry) g_deco[j * SCR_W + i] = 0;
        }
    }
    for (k = 0; k < g_norn; k++) {
        Orn* o = &g_orn[k];
        int broken = 0;
        if (!o->alive || o->x1 < cx - rx - 1 || o->x0 > cx + rx + 1 || o->y1 < cy - ry - 1 || o->y0 > cy + ry + 1) continue;
        for (i = o->x0; i <= o->x1; i++) if (i >= 0 && i < SCR_W && !W.solid[o->base * SCR_W + i]) { broken = 1; break; }
        if (!broken) {                                       /* only part of it hit: whatever is left of it stands */
            continue;
        }
        o->alive = 0;
        for (j = o->y0; j <= o->y1; j++)
            for (i = o->x0; i <= o->x1; i++)
                if (i >= 0 && i < SCR_W && j >= 0 && j < BASELINE && g_deco[j * SCR_W + i] == k + 1) { W.pic[j * SCR_W + i] = W.sky[j * SCR_W + i]; g_deco[j * SCR_W + i] = 0; }
    }
    for (k = 0; k < g_nprop; k++) {                          /* street furniture stands in front of the wall: put it back */
        const Prop* p = &g_prop[k];
        if (p->x + p->s->w < cx - rx || p->x > cx + rx || p->y + p->s->h < cy - ry || p->y > cy + ry) continue;
        for (j = 0; j < p->s->h; j++)
            for (i = 0; i < p->s->w; i++) {
                uint16_t c = p->s->px[j * p->s->w + i];
                int px = p->x + i, py = p->y + j;
                if (c != KEY && px >= 0 && px < SCR_W && py >= 0 && py < BASELINE) W.pic[py * SCR_W + px] = c;
            }
    }
}
