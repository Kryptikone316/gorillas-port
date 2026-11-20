/* The playfield: skyline, destructible terrain, gorilla positions, and the flight of a banana. */
#ifndef WORLD_H
#define WORLD_H

#include <stdint.h>
#include "plat.h"

#define BASELINE 450            /* y where the buildings end; the score strip is below */
#define MAXB 48

typedef struct { int x, w, top; } Building;

typedef struct {
    uint16_t* pic;              /* sky and buildings as drawn (SCR_W x SCR_H) */
    uint16_t* sky;              /* the sky alone (gradient, stars, moon glow): what a crater shows */
    int theme;                  /* cosmetic: the look of the city, see theme.h (set before world_new_city) */
    int night;                  /* cosmetic: a night sky with stars and moonlight (set before world_new_city) */
    uint8_t* solid;             /* 1 where a building is */
    Building b[MAXB];
    int nb;
    int gb[2];                  /* the building each gorilla stands on */
    int gx[2], gy[2];           /* top-left of each gorilla sprite */
    int alive[2];               /* a gorilla blown up is hidden */
    int wind;                   /* -14..15: pushes the banana; arrow length = 6 px per unit */
    double grav;                /* m/s^2, Earth 9.8 */
} World;

extern World W;

/* Random numbers (own generator so a seed repeats a game on every machine). */
void rng_seed(uint32_t s);
uint32_t rng_u32(void);
int rnd(int n);                 /* 1..n, like the original's FnRan */
double rnd_gauss(void);         /* roughly normal, sigma 1 */

void world_init(void);
void world_new_city(void);      /* new skyline, gorillas placed; wind and gravity are the caller's */
uint16_t sky_color(int y);       /* the plain gradient for the current day/night */
void world_carve(int cx, int cy, int rx, int ry);
void world_draw_background(uint16_t* dst);   /* copies the picture */

/* Flight of a banana. Time advances 0.1 per step, as in the original; positions are screen pixels. */
enum { SH_FLYING = 0, SH_BUILDING, SH_GORILLA, SH_OFFSCREEN };
typedef struct {
    int owner;                  /* thrower, 0 or 1 */
    double x0, y0, vx, vy, t;
    double x, y;
    int rot;
    int armed;                  /* may hit its own thrower (it has left his body) */
    int victim;                 /* SH_GORILLA: who was hit */
    int sun;                    /* passing through the sun */
    int steps;
} Shot;

/* angle: degrees measured toward the opponent (0 = flat, 90 = straight up). */
void shot_start(Shot* s, int owner, double angle, double velocity);
int shot_step(Shot* s, double wind, double grav);       /* one 0.1 time step */
int gorilla_at(int who, int x, int y);               /* is the pixel part of that gorilla */
void gorilla_hand(int owner, int* hx, int* hy);      /* where the banana leaves the hand */

#endif
