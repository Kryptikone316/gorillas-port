#include "world.h"
#include "gfx.h"
#include "art.h"
#include "theme.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

World W;

/* ---- random numbers: xorshift32 ---- */
static uint32_t g_rng = 2463534242u;
void rng_seed(uint32_t s) { g_rng = s ? s : 2463534242u; }
uint32_t rng_u32(void) {
    g_rng ^= g_rng << 13; g_rng ^= g_rng >> 17; g_rng ^= g_rng << 5;
    return g_rng;
}
int rnd(int n) { return (int)(rng_u32() % (uint32_t)n) + 1; }
double rnd_gauss(void) {
    double s = 0; int i;
    for (i = 0; i < 6; i++) s += (rng_u32() & 0xFFFF) / 65535.0;
    return (s - 3.0) * 1.414;     /* sum of 6 uniforms: variance 0.5 */
}

/* ---- sky ---- */
uint16_t sky_color(int y) {
    y = y < 0 ? 0 : (y >= SCR_H ? SCR_H - 1 : y);
    return theme_sky_color(W.theme, W.night, y);
}

void world_init(void) {
    W.pic = (uint16_t*)malloc((size_t)SCR_W * SCR_H * sizeof(uint16_t));
    W.sky = (uint16_t*)malloc((size_t)SCR_W * SCR_H * sizeof(uint16_t));
    W.solid = (uint8_t*)calloc((size_t)SCR_W * SCR_H, 1);
    theme_init();
    W.grav = 9.8;
}

/* The sky layer: the gradient; at night also the moon's glow and a scatter of stars. */
static uint32_t g_star_rng = 987654321u;       /* the stars' own generator: day or night, the game's random numbers are the same */
static int star_rnd(int n) { g_star_rng = g_star_rng * 1664525u + 1013904223u; return (int)((g_star_rng >> 8) % (uint32_t)n) + 1; }

static void build_sky(void) {
    int x, y, i;
    for (y = 0; y < SCR_H; y++) {
        uint16_t c = y < BASELINE ? sky_color(y) : RGB(14, 14, 36);       /* below the skyline: the theme paints the street */
        uint16_t* row = W.sky + y * SCR_W;
        for (x = 0; x < SCR_W; x++) row[x] = c;
    }
    if (!W.night) { theme_backdrop(); return; }
    /* moonlight: a soft pool of light round the moon, brightest next to it */
    for (y = 0; y < BASELINE; y++) {
        int dy = y - 38;
        if (dy < -150 || dy > 150) continue;
        for (x = SCR_W / 2 - 220; x < SCR_W / 2 + 220; x++) {
            long d2 = (long)(x - SCR_W / 2) * (x - SCR_W / 2) + (long)dy * dy;
            if (d2 < 200L * 200L) {
                int f = (int)(256 - d2 * 256 / (200L * 200L));       /* 256 at the centre .. 0 at the edge */
                f = f * f / 256;                                       /* softer fall-off */
                W.sky[y * SCR_W + x] = gfx_mix(W.sky[y * SCR_W + x], RGB(120, 150, 226), f * 80 / 256);
            }
        }
    }
    for (i = 0; i < 130; i++) {   /* stars: mostly single pixels, a few brighter crosses */
        int sx = star_rnd(SCR_W - 2), sy = star_rnd(BASELINE - 120) + 1;
        long dx = sx - SCR_W / 2, dy2 = sy - 38;
        uint16_t c = (i % 5 == 0) ? RGB(255, 244, 200) : ((i % 3 == 0) ? RGB(190, 200, 240) : RGB(130, 144, 200));
        if (dx * dx + dy2 * dy2 < 60L * 60L) continue;       /* keep the moon's own patch of sky clear */
        W.sky[sy * SCR_W + sx] = c;
        if (i % 11 == 0) { W.sky[sy * SCR_W + sx + 1] = c; W.sky[(sy + 1) * SCR_W + sx] = c; if (sx > 0) W.sky[sy * SCR_W + sx - 1] = c; if (sy > 0) W.sky[(sy - 1) * SCR_W + sx] = c; }
    }
    theme_backdrop();
}

static void draw_building(int x, int w, int top, int color) {
    int i, j;
    theme_building(x, w, top, color);          /* the picture; the terrain mask below is the same in every theme */
    for (j = top - 1; j < BASELINE; j++)
        for (i = x; i < x + w; i++) W.solid[j * SCR_W + i] = 1;
    for (i = x - 1; i <= x + w; i++) if (top - 1 >= 0) W.solid[(top - 1) * SCR_W + i] = 1;
}

static void place_gorillas(void) {
    int i;
    for (i = 0; i < 2; i++) {
        int n = i == 0 ? rnd(2) : W.nb - 1 - (rnd(2) - 1);     /* second or third building from each edge (0-based 1..2 / nb-2..nb-3) */
        Building* b;
        if (i == 0) n = rnd(2);                                 /* 1 or 2 */
        else n = W.nb - 1 - rnd(2);                             /* nb-2 or nb-3 */
        if (n < 0) n = 0;
        if (n >= W.nb) n = W.nb - 1;
        b = &W.b[n];
        W.gb[i] = n;
        W.gx[i] = b->x + b->w / 2 - GW / 2;
        W.gy[i] = b->top - (GH - 1);
        W.alive[i] = 1;
    }
}

void world_new_city(void) {
    const double sy = (double)SCR_H / 350.0;
    int x = 2, slope = rnd(6), newht, i;
    theme_begin();
    build_sky();
    memcpy(W.pic, W.sky, (size_t)SCR_W * SCR_H * sizeof(uint16_t));
    gfx_target(W.pic, SCR_W, SCR_H);
    memset(W.solid, 0, (size_t)SCR_W * SCR_H);
    switch (slope) {                         /* the trend of the skyline: up, down, hill, hill, valley, flat */
        case 1: newht = 15; break;
        case 2: newht = 130; break;
        case 3: case 4: newht = 15; break;
        case 5: newht = 130; break;
        default: newht = 60; break;
    }
    W.nb = 0;
    do {
        const int inc = 10;
        int bw, bh, top;
        switch (slope) {
            case 1: newht += inc; break;
            case 2: newht -= inc; break;
            case 3: case 4: newht += (x > SCR_W / 2) ? -2 * inc : 2 * inc; break;
            case 5: newht += (x > SCR_W / 2) ? 2 * inc : -2 * inc; break;
            default: break;
        }
        bw = rnd(37) + 37;
        if (x + bw > SCR_W - 2) bw = SCR_W - x - 2;
        bh = rnd(120) + newht;
        if (bh < inc) bh = inc;
        top = BASELINE - (int)(bh * sy);
        if (top < 150) top = 150 + rnd(20);               /* never into the sun and the score */
        if (top > BASELINE - 34) top = BASELINE - 34;
        if (bw < 8) break;
        W.b[W.nb].x = x; W.b[W.nb].w = bw; W.b[W.nb].top = top;
        draw_building(x, bw, top, rnd(3) - 1);
        W.nb++;
        x += bw + 2;
    } while (x < SCR_W - 10 && W.nb < MAXB);
    for (i = 0; i < 2; i++) W.alive[i] = 1;
    place_gorillas();
    theme_finish();
}

void world_carve(int cx, int cy, int rx, int ry) {
    int y, x;
    for (y = cy - ry; y <= cy + ry; y++) {
        long dy = y - cy;
        if (y < 0 || y >= BASELINE) continue;
        for (x = cx - rx; x <= cx + rx; x++) {
            long dx = x - cx;
            if (x < 0 || x >= SCR_W) continue;
            if (dx * dx * (long)ry * ry + dy * dy * (long)rx * rx <= (long)rx * rx * ry * ry) {
                W.pic[y * SCR_W + x] = W.sky[y * SCR_W + x];
                W.solid[y * SCR_W + x] = 0;
            }
        }
    }
    theme_carve(cx, cy, rx, ry);
}

void world_draw_background(uint16_t* dst) { memcpy(dst, W.pic, (size_t)SCR_W * SCR_H * sizeof(uint16_t)); }

/* ---- the banana ---- */
int gorilla_at(int who, int x, int y) {
    int lx = x - W.gx[who], ly = y - W.gy[who];
    if (!W.alive[who]) return 0;
    if (lx < 0 || ly < 0 || lx >= GW || ly >= GH) return 0;
    return gor_mask[ly * GW + lx];
}

void gorilla_hand(int owner, int* hx, int* hy) {
    /* the raised arm: player 1 throws with the right arm, player 2 with the left; the banana starts just above the fist */
    *hx = W.gx[owner] + (owner == 0 ? 36 : 4);
    *hy = W.gy[owner] - 4;
}

void shot_start(Shot* s, int owner, double angle, double velocity) {
    double a = (owner == 0 ? angle : 180.0 - angle) * 3.14159265358979 / 180.0;
    int hx, hy;
    memset(s, 0, sizeof *s);
    gorilla_hand(owner, &hx, &hy);
    s->owner = owner;
    s->x0 = hx; s->y0 = hy;
    s->vx = cos(a) * velocity;
    s->vy = sin(a) * velocity;
    s->x = hx; s->y = hy;
    s->victim = -1;
}

static int probe(Shot* s, int px, int py, int* victim) {
    int o = s->owner, other = 1 - o;
    if (py < 0) return 0;
    if (gorilla_at(other, px, py)) { *victim = other; return 1; }
    if (s->armed && gorilla_at(o, px, py)) { *victim = o; return 1; }
    if (px >= 0 && px < SCR_W && py < BASELINE && W.solid[py * SCR_W + px]) { *victim = -1; return 2; }
    return 0;
}

int shot_step(Shot* s, double wind, double grav) {
    const double sy = (double)SCR_H / 350.0;
    int sub;
    s->steps++;
    for (sub = 0; sub < 4; sub++) {
        static const int off[5][2] = { { 0, 0 }, { 3, 0 }, { -3, 0 }, { 0, 3 }, { 0, -3 } };
        int k, r = 0, victim = -1, px, py;
        s->t += 0.025;
        s->x = s->x0 + s->vx * s->t + 0.5 * (wind / 5.0) * s->t * s->t;
        s->y = s->y0 + (-(s->vy * s->t) + 0.5 * grav * s->t * s->t) * sy;
        s->rot = ((int)(s->t * 10)) & 3;
        if (s->x >= SCR_W - 10 || s->x <= 3 || s->y >= SCR_H - 3 || s->t > 400) return SH_OFFSCREEN;
#ifndef SHOT_NO_SKIP
        /* High above the top edge nothing can happen. Jump to just before it comes back down (on the same time grid, so the
         * result is exactly what stepping would give), but only when it stays inside the side limits all the way: a parabola's
         * extremes are its ends and its vertex, so those are checked. Low gravity lobs otherwise cost thousands of steps. */
        if (s->y < -40.0 && grav > 1e-6) {
            double A = 0.5 * grav * sy, B = -s->vy * sy, Cc = s->y0 + 40.0, disc = B * B - 4 * A * Cc;
            if (disc > 0) {
                double tc = (-B + sqrt(disc)) / (2 * A), tn = floor(tc / 0.025) * 0.025 - 0.075;
                if (tn > s->t + 0.1) {
                    double a2 = 0.5 * (wind / 5.0), xn = s->x0 + s->vx * tn + a2 * tn * tn;
                    int ok = (xn < SCR_W - 10 && xn > 3);
                    if (ok && a2 != 0) {
                        double tv = -s->vx / (2 * a2), xv = s->x0 + s->vx * tv + a2 * tv * tv;
                        if (tv > s->t && tv < tn && !(xv < SCR_W - 10 && xv > 3)) ok = 0;
                    }
                    if (ok) { s->t = tn; s->armed = 1; s->sun = 0; continue; }
                }
            }
        }
#endif
        px = (int)s->x; py = (int)s->y;
        /* it arms itself once it is clear of its thrower, so a flat throw does not blow up in his own hand */
        if (!s->armed && !gorilla_at(s->owner, px, py) && !gorilla_at(s->owner, px, py + 4) && s->t > 0.04) s->armed = 1;
        for (k = 0; k < 5 && !r; k++) r = probe(s, px + off[k][0], py + off[k][1], &victim);
        if (r == 1) { s->victim = victim; return SH_GORILLA; }
        if (r == 2) return SH_BUILDING;
        s->sun = (py >= 0 && py < 56 && abs(px - SCR_W / 2) < 26);
    }
    return SH_FLYING;
}
