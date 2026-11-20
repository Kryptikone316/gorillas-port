/* All art is drawn here, in code, from simple shapes: the gorillas, the banana and the sun. */
#include "art.h"
#include <math.h>
#include <string.h>

Sprite* spr_gor[NLOOKS][POSE_COUNT];
unsigned char gor_mask[GW * GH];
Sprite* spr_ban[4];
Sprite* spr_ban_qb[4];
Sprite* spr_sky[NSKY][2];

typedef struct { uint16_t fur, shade, belly, skin, dark; } Fur;
enum { ACC_NONE, ACC_SHADES, ACC_BANDANA, ACC_CROWN, ACC_PARTY };

static void limb(int x0, int y0, int x1, int y1, int r, uint16_t c) { gfx_thick(x0, y0, x1, y1, r, c); }

/* Right-hand side of the picture is in shadow: a darker fur tone on the far side of the body. */
static void shade_pass(Sprite* s, const Fur* f) {
    int i, j;
    for (j = 0; j < s->h; j++)
        for (i = 0; i < s->w; i++) {
            uint16_t* p = &s->px[j * s->w + i];
            if (*p == f->fur && (i - 20) * 2 + (j - 20) > 16) *p = f->shade;   /* lower right */
        }
}

static void accessory(Sprite* s, int acc) {
    int x, y;
    gfx_target(s->px, s->w, s->h);
    switch (acc) {
        case ACC_SHADES:
            gfx_rect(14, 11, 6, 4, RGB(18, 18, 30)); gfx_rect(21, 11, 6, 4, RGB(18, 18, 30)); gfx_rect(20, 12, 1, 1, RGB(18, 18, 30));
            gfx_px(15, 12, RGB(140, 170, 230)); gfx_px(22, 12, RGB(140, 170, 230));
            break;
        case ACC_BANDANA:       /* follows the head's outline, with a knot at the side */
            for (y = 7; y <= 9; y++) for (x = 10; x < 31; x++) if (s->px[y * s->w + x] != KEY) s->px[y * s->w + x] = RGB(204, 40, 44);
            gfx_rect(29, 8, 4, 2, RGB(204, 40, 44)); gfx_rect(31, 10, 2, 3, RGB(160, 24, 30)); gfx_rect(32, 6, 2, 2, RGB(160, 24, 30));
            break;
        case ACC_CROWN:
            gfx_rect(15, 4, 11, 3, RGB(255, 214, 40)); gfx_rect(15, 2, 2, 2, RGB(255, 214, 40)); gfx_rect(19, 1, 3, 3, RGB(255, 214, 40));
            gfx_rect(24, 2, 2, 2, RGB(255, 214, 40)); gfx_px(20, 5, RGB(220, 40, 60)); gfx_rect(15, 6, 11, 1, RGB(200, 150, 20));
            break;
        case ACC_PARTY:
            for (y = 1; y <= 8; y++) for (x = 20 - (y - 1); x <= 20 + (y - 1); x++) gfx_px(x, y, (((x + y) / 2) & 1) ? RGB(236, 80, 170) : RGB(70, 170, 240));
            gfx_rect(19, 0, 3, 2, RGB(255, 226, 60));
            break;
        default: break;
    }
}

static void draw_gorilla(Sprite* s, int pose, const Fur* f, int acc) {
    gfx_target(s->px, s->w, s->h);
    /* legs and feet */
    gfx_ellipse(14, 40, 6, 5, f->fur);
    gfx_ellipse(26, 40, 6, 5, f->fur);
    gfx_ellipse(12, 44, 6, 2, f->dark);   /* feet: rows up to 46 */
    gfx_ellipse(28, 44, 6, 2, f->dark);
    /* torso and belly */
    gfx_ellipse(20, 29, 13, 12, f->fur);
    gfx_ellipse(20, 32, 8, 8, f->belly);
    /* arms: the raised one reaches up beside the head */
    if (pose == POSE_LEFT_UP) {
        limb(9, 22, 4, 8, 4, f->fur);
        gfx_ellipse(4, 5, 4, 4, f->dark);
    } else {
        limb(9, 24, 6, 38, 4, f->fur);
        gfx_ellipse(6, 40, 4, 3, f->dark);
    }
    if (pose == POSE_RIGHT_UP) {
        limb(31, 22, 36, 8, 4, f->fur);
        gfx_ellipse(36, 5, 4, 4, f->dark);
    } else {
        limb(31, 24, 34, 38, 4, f->fur);
        gfx_ellipse(34, 40, 4, 3, f->dark);
    }
    /* shoulders */
    gfx_ellipse(10, 23, 5, 5, f->fur);
    gfx_ellipse(30, 23, 5, 5, f->fur);
    /* head: crest, skull, face */
    gfx_ellipse(20, 6, 6, 5, f->shade);
    gfx_ellipse(20, 13, 9, 8, f->fur);
    gfx_ellipse(20, 15, 6, 5, f->skin);
    gfx_rect(14, 10, 13, 2, f->dark);              /* brow */
    gfx_rect(16, 12, 2, 2, f->dark);               /* eyes */
    gfx_rect(23, 12, 2, 2, f->dark);
    gfx_px(19, 15, f->dark); gfx_px(21, 15, f->dark);   /* nostrils */
    gfx_rect(17, 18, 7, 1, f->dark);               /* mouth */
    shade_pass(s, f);
    accessory(s, acc);
    spr_outline(s, RGB(30, 18, 14));
}

/* A crescent: inside the big circle, outside a smaller one shifted up. Opens upward; k turns it by k * 90 degrees. */
static Sprite* make_banana(int k) {
    Sprite* s = spr_new(BAN_W, BAN_W);
    int x, y;
    for (y = 0; y < BAN_W; y++)
        for (x = 0; x < BAN_W; x++) {
            double u = x - 6.0, v = y - 6.0, uu, vv;
            switch (k & 3) {   /* rotate the sample point */
                case 0: uu = u; vv = v; break;
                case 1: uu = v; vv = -u; break;
                case 2: uu = -u; vv = -v; break;
                default: uu = -v; vv = u; break;
            }
            {
                double d1 = sqrt(uu * uu + vv * vv);
                double d2 = sqrt(uu * uu + (vv + 3.2) * (vv + 3.2));
                if (d1 <= 5.9 && d2 >= 5.4) s->px[y * BAN_W + x] = (d1 > 5.0 || d2 < 6.4) ? RGB(214, 168, 20) : RGB(255, 228, 50);
            }
        }
    spr_outline(s, RGB(92, 60, 8));
    return s;
}

#define INK_SUN RGB(60, 36, 8)
#define INK_MOON RGB(60, 66, 108)

static void mouth(int c, int shocked, uint16_t ink) {
    if (shocked) gfx_ellipse(c, c + 7, 3, 4, ink);
    else {
        int x;   /* a smile: the lower arc of a circle */
        for (x = -7; x <= 7; x++) {
            int y = (int)(sqrt(64.0 - x * x) * 0.6) - 2;
            gfx_rect(c + x, c + 5 + (y > 0 ? y : 0), 1, 2, ink);
        }
    }
}

/* 0 Smiley Sun, 1 Cool Sun (sunglasses): a disc with rays */
static Sprite* make_sun(int cool, int shocked) {
    Sprite* s = spr_new(SUN_W, SUN_W);
    int i, c = SUN_W / 2;
    gfx_target(s->px, SUN_W, SUN_W);
    for (i = 0; i < 12; i++) {
        double a = i * 3.14159265 / 6;
        double r0 = 16, r1 = (i & 1) ? 26 : 30;
        gfx_thick(c + (int)(cos(a) * r0), c + (int)(sin(a) * r0), c + (int)(cos(a) * r1), c + (int)(sin(a) * r1), 1, RGB(255, 200, 40));
    }
    gfx_ellipse(c, c, 16, 16, RGB(255, 214, 54));
    gfx_ellipse(c - 5, c - 6, 6, 5, RGB(255, 240, 150));          /* highlight */
    if (cool) {
        gfx_rect(c - 12, c - 7, 10, 7, RGB(24, 22, 36)); gfx_rect(c + 2, c - 7, 10, 7, RGB(24, 22, 36)); gfx_rect(c - 2, c - 6, 4, 2, RGB(24, 22, 36));
        gfx_rect(c - 10, c - 6, 3, 1, RGB(150, 180, 240)); gfx_rect(c + 4, c - 6, 3, 1, RGB(150, 180, 240));
    } else {
        gfx_rect(c - 8, c - 5, 3, 4, INK_SUN);
        gfx_rect(c + 5, c - 5, 3, 4, INK_SUN);
        if (shocked) { gfx_rect(c - 10, c - 9, 7, 1, INK_SUN); gfx_rect(c + 4, c - 9, 7, 1, INK_SUN); }   /* raised brows */
    }
    mouth(c, shocked, INK_SUN);
    spr_outline(s, RGB(150, 90, 0));
    return s;
}

/* 2 Retro Sun: a flat disc in sunset bands, with the lower half cut by gaps */
static Sprite* make_retro(int shocked) {
    Sprite* s = spr_new(SUN_W, SUN_W);
    int x, y, c = SUN_W / 2;
    gfx_target(s->px, SUN_W, SUN_W);
    for (y = c - 22; y <= c + 22; y++)
        for (x = c - 22; x <= c + 22; x++) {
            long d2 = (long)(x - c) * (x - c) + (long)(y - c) * (y - c);
            int v = y - c;
            if (d2 > 21L * 21L) continue;
            if (v > 3 && ((v - 3) % 6) < (v - 3) / 6 + 1) continue;           /* horizontal gaps that widen toward the bottom */
            gfx_px(x, y, gfx_mix(RGB(255, 236, 80), RGB(255, 70, 110), (v + 22) * 256 / 44));
        }
    gfx_rect(c - 9, c - 6, 3, 3, RGB(90, 20, 70)); gfx_rect(c + 6, c - 6, 3, 3, RGB(90, 20, 70));
    if (shocked) gfx_ellipse(c, c + 1, 2, 3, RGB(90, 20, 70));
    else for (x = -5; x <= 5; x++) gfx_px(c + x, c + 5 - (x * x) / 8, RGB(90, 20, 70));
    spr_outline(s, RGB(150, 30, 80));
    return s;
}

/* 3 Crescent Moon, 4 Full Moon, 5 Sleepy Moon (nightcap) */
static Sprite* make_moon(int kind, int shocked) {
    Sprite* s = spr_new(SUN_W, SUN_W);
    int x, y, c = SUN_W / 2;
    uint16_t body = RGB(240, 236, 200), shade = RGB(206, 204, 166), crater = RGB(214, 212, 178);
    gfx_target(s->px, SUN_W, SUN_W);
    if (kind == 3) {            /* the disc less a bite taken out of the right */
        for (y = 0; y < SUN_W; y++)
            for (x = 0; x < SUN_W; x++) {
                long a2 = (long)(x - (c - 3)) * (x - (c - 3)) + (long)(y - c) * (y - c);
                long b2 = (long)(x - (c + 8)) * (x - (c + 8)) + (long)(y - (c - 4)) * (y - (c - 4));
                if (a2 <= 22L * 22L && b2 > 18L * 18L) gfx_px(x, y, (a2 > 19L * 19L || b2 < 21L * 21L) ? shade : body);
            }
        if (shocked) { gfx_rect(c - 16, c - 4, 4, 4, RGB(255, 255, 255)); gfx_rect(c - 15, c - 3, 2, 2, INK_MOON); gfx_ellipse(c - 13, c + 8, 2, 3, INK_MOON); }
        else {      /* a sleepy closed eye and a small smile on the thick part */
            gfx_rect(c - 17, c - 3, 6, 1, INK_MOON); gfx_px(c - 17, c - 2, INK_MOON); gfx_px(c - 12, c - 2, INK_MOON);
            gfx_px(c - 16, c + 7, INK_MOON); gfx_rect(c - 15, c + 8, 3, 1, INK_MOON); gfx_px(c - 12, c + 7, INK_MOON);
        }
        gfx_rect(c - 16, c + 2, 3, 2, RGB(236, 170, 160));       /* a blush */
    } else {
        gfx_ellipse(c, c, 19, 19, shade);
        gfx_ellipse(c - 1, c - 1, 18, 18, body);
        if (kind == 4) {        /* craters */
            gfx_ellipse(c - 8, c - 10, 4, 3, crater); gfx_ellipse(c + 10, c - 3, 5, 5, crater);
            gfx_ellipse(c - 4, c + 12, 3, 3, crater); gfx_ellipse(c + 6, c + 12, 2, 2, crater); gfx_ellipse(c - 13, c + 2, 2, 2, crater);
        }
        if (shocked) {
            gfx_ellipse(c - 7, c - 3, 3, 3, RGB(255, 255, 255)); gfx_ellipse(c + 7, c - 3, 3, 3, RGB(255, 255, 255));
            gfx_rect(c - 8, c - 4, 2, 2, INK_MOON); gfx_rect(c + 6, c - 4, 2, 2, INK_MOON);
            gfx_ellipse(c, c + 8, 3, 4, INK_MOON);
        } else if (kind == 5) { /* eyes closed */
            gfx_rect(c - 10, c - 2, 6, 1, INK_MOON); gfx_rect(c + 4, c - 2, 6, 1, INK_MOON);
            gfx_rect(c - 3, c + 8, 6, 1, INK_MOON);
        } else {
            gfx_rect(c - 8, c - 4, 3, 3, INK_MOON); gfx_rect(c + 5, c - 4, 3, 3, INK_MOON);
            for (x = -6; x <= 6; x++) gfx_px(c + x, c + 9 - (x * x) / 9, INK_MOON);
        }
        if (kind == 5) {        /* nightcap: a band over the brow and a floppy tip with a pom-pom */
            gfx_rect(c - 17, c - 15, 34, 5, RGB(60, 90, 190));
            gfx_thick(c - 6, c - 17, c + 6, c - 26, 4, RGB(70, 104, 210));
            gfx_thick(c + 6, c - 26, c + 17, c - 20, 3, RGB(70, 104, 210));
            gfx_ellipse(c + 19, c - 17, 3, 3, RGB(255, 255, 255));
        }
    }
    spr_outline(s, RGB(120, 120, 170));
    return s;
}

/* ---- the Q-BASIC look: flat EGA colours, our own drawing (nothing copied from the original's pictures) ---- */
#define QB_PEACH RGB(255, 170, 85)
#define QB_BLUE RGB(0, 0, 170)
#define QB_YELLOW RGB(255, 255, 85)

/* The gorilla is drawn on a 32 x 29 grid of "original" pixels and stretched into the 40 x 48 sprite (the original's pixels were tall). */
static void lrect(Sprite* s, int x0, int y0, int x1, int y1, uint16_t c) {
    int ya = 8 + y0 * 40 / 29, yb = 8 + (y1 + 1) * 40 / 29, y, x;
    for (y = ya; y < yb && y < s->h; y++)
        for (x = 4 + x0; x <= 4 + x1 && x < s->w; x++) s->px[y * s->w + x] = c;
}

static void draw_qb_gorilla(Sprite* s, int pose) {
    const uint16_t P = QB_PEACH, B = QB_BLUE;
    lrect(s, 11, 1, 20, 9, P); lrect(s, 10, 3, 10, 6, P); lrect(s, 21, 3, 21, 6, P);           /* big head, no neck, ears */
    lrect(s, 12, 3, 19, 3, B); lrect(s, 13, 5, 14, 5, B); lrect(s, 17, 5, 18, 5, B);            /* heavy brow, deep-set eyes */
    lrect(s, 13, 8, 18, 8, B);                                                                  /* wide mouth */
    lrect(s, 9, 9, 22, 9, P); lrect(s, 5, 10, 26, 11, P); lrect(s, 2, 12, 29, 14, P);           /* sloping shoulders, very wide */
    lrect(s, 8, 15, 23, 21, P);                                                                 /* barrel chest */
    lrect(s, 9, 16, 12, 16, B); lrect(s, 19, 16, 22, 16, B);                                    /* chest lines */
    lrect(s, 9, 22, 22, 22, P);                                                                 /* hips */
    lrect(s, 5, 23, 12, 25, P); lrect(s, 19, 23, 26, 25, P);                                    /* short, thick legs */
    lrect(s, 3, 26, 12, 28, P); lrect(s, 19, 26, 28, 28, P);                                    /* big feet */
    if (pose == POSE_LEFT_UP) { lrect(s, 1, 9, 5, 11, P); lrect(s, 1, 1, 5, 9, P); lrect(s, 0, 0, 6, 3, P); }
    else { lrect(s, 1, 15, 6, 21, P); lrect(s, 1, 22, 7, 25, P); }                              /* long arm down, fist low by the hip */
    if (pose == POSE_RIGHT_UP) { lrect(s, 26, 9, 30, 11, P); lrect(s, 26, 1, 30, 9, P); lrect(s, 25, 0, 31, 3, P); }
    else { lrect(s, 25, 15, 30, 21, P); lrect(s, 24, 22, 30, 25, P); }
}

/* The Q-BASIC sun: a flat yellow disc with sixteen thin rays, crossed eyes and a smile (an O when a banana is near). */
static Sprite* make_qb_sun(int shocked) {
    Sprite* s = spr_new(SUN_W, SUN_W);
    int i, c = SUN_W / 2, x;
    gfx_target(s->px, SUN_W, SUN_W);
    for (i = 0; i < 16; i++) {
        double a = i * 3.14159265 / 8, r1 = (i & 1) ? 23 : 30;
        gfx_line(c + (int)(cos(a) * 15), c + (int)(sin(a) * 15), c + (int)(cos(a) * r1), c + (int)(sin(a) * r1), QB_YELLOW);
    }
    gfx_ellipse(c, c, 12, 12, QB_YELLOW);
    for (i = -1; i <= 1; i++) { gfx_px(c - 6 + i, c - 3, QB_BLUE); gfx_px(c + 6 + i, c - 3, QB_BLUE); gfx_px(c - 6, c - 3 + i, QB_BLUE); gfx_px(c + 6, c - 3 + i, QB_BLUE); }
    if (shocked) gfx_ellipse(c, c + 6, 3, 4, QB_BLUE);
    else for (x = -6; x <= 6; x++) gfx_px(c + x, c + 9 - (x * x) / 14, QB_BLUE);
    return s;
}

/* A flat yellow crescent, no outline or shading. */
static Sprite* make_banana_qb(int k) {
    Sprite* s = spr_new(BAN_W, BAN_W);
    int x, y;
    for (y = 0; y < BAN_W; y++)
        for (x = 0; x < BAN_W; x++) {
            double u = x - 6.0, v = y - 6.0, uu, vv;
            switch (k & 3) { case 0: uu = u; vv = v; break; case 1: uu = v; vv = -u; break; case 2: uu = -u; vv = -v; break; default: uu = -v; vv = u; break; }
            if (sqrt(uu * uu + vv * vv) <= 5.9 && sqrt(uu * uu + (vv + 3.2) * (vv + 3.2)) >= 5.0) s->px[y * BAN_W + x] = QB_YELLOW;
        }
    return s;
}

static const struct { const char* name; Fur fur; int acc; } k_look[NLOOKS] = {
    { "Goldie",    { RGB(214, 140, 52), RGB(170, 100, 36), RGB(246, 200, 120), RGB(250, 214, 160), RGB(70, 38, 20) }, ACC_NONE },
    { "Silver",    { RGB(96, 102, 130), RGB(66, 70, 96), RGB(150, 158, 186), RGB(204, 208, 224), RGB(26, 28, 44) }, ACC_NONE },
    { "Cherry",    { RGB(190, 74, 60), RGB(146, 50, 44), RGB(240, 156, 130), RGB(250, 206, 180), RGB(72, 22, 20) }, ACC_NONE },
    { "Jungle",    { RGB(78, 150, 84), RGB(52, 112, 62), RGB(164, 214, 150), RGB(226, 232, 186), RGB(24, 56, 28) }, ACC_NONE },
    { "Blueberry", { RGB(72, 112, 204), RGB(48, 80, 160), RGB(150, 182, 240), RGB(214, 226, 250), RGB(20, 32, 78) }, ACC_NONE },
    { "Grape",     { RGB(138, 86, 178), RGB(100, 58, 138), RGB(206, 168, 234), RGB(238, 214, 246), RGB(46, 24, 66) }, ACC_NONE },
    { "Snowy",     { RGB(232, 232, 242), RGB(186, 190, 214), RGB(255, 255, 255), RGB(252, 226, 220), RGB(90, 90, 120) }, ACC_NONE },
    { "Midnight",  { RGB(54, 52, 66), RGB(34, 32, 46), RGB(98, 94, 118), RGB(196, 180, 170), RGB(10, 10, 16) }, ACC_NONE },
    { "Shades",    { RGB(214, 140, 52), RGB(170, 100, 36), RGB(246, 200, 120), RGB(250, 214, 160), RGB(70, 38, 20) }, ACC_SHADES },
    { "Bandit",    { RGB(54, 52, 66), RGB(34, 32, 46), RGB(98, 94, 118), RGB(196, 180, 170), RGB(10, 10, 16) }, ACC_BANDANA },
    { "King",      { RGB(96, 102, 130), RGB(66, 70, 96), RGB(150, 158, 186), RGB(204, 208, 224), RGB(26, 28, 44) }, ACC_CROWN },
    { "Party",     { RGB(232, 232, 242), RGB(186, 190, 214), RGB(255, 255, 255), RGB(252, 226, 220), RGB(90, 90, 120) }, ACC_PARTY },
    { "Q-BASIC",   { QB_PEACH, QB_PEACH, QB_PEACH, QB_PEACH, QB_BLUE }, ACC_NONE },
};

static const char* const k_sky_name[NSKY + 2] = { "Smiley Sun", "Cool Sun", "Retro Sun", "Crescent Moon", "Full Moon", "Sleepy Moon", "Q-BASIC Sun", "Random", "Auto" };
const char* sky_name(int style) { return k_sky_name[style < 0 ? 0 : (style > NSKY + 1 ? NSKY + 1 : style)]; }

const char* look_name(int look) { return k_look[look < 0 ? 0 : (look >= NLOOKS ? NLOOKS - 1 : look)].name; }

void art_init(void) {
    int p, k, i;
    for (p = 0; p < NLOOKS; p++)
        for (k = 0; k < POSE_COUNT; k++) {
            spr_gor[p][k] = spr_new(GW, GH);
            if (p == LOOK_QB) draw_qb_gorilla(spr_gor[p][k], k);
            else draw_gorilla(spr_gor[p][k], k, &k_look[p].fur, k_look[p].acc);
        }
    for (i = 0; i < GW * GH; i++) gor_mask[i] = spr_gor[0][POSE_DOWN]->px[i] != KEY;
    for (k = 0; k < 4; k++) { spr_ban[k] = make_banana(k); spr_ban_qb[k] = make_banana_qb(k); }
    for (k = 0; k < 2; k++) {
        spr_sky[0][k] = make_sun(0, k); spr_sky[1][k] = make_sun(1, k); spr_sky[2][k] = make_retro(k);
        spr_sky[3][k] = make_moon(3, k); spr_sky[4][k] = make_moon(4, k); spr_sky[5][k] = make_moon(5, k);
        spr_sky[SKY_QB][k] = make_qb_sun(k);
    }
}
