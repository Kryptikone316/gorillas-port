#include "bot.h"
#include "world.h"
#include "art.h"
#include <math.h>
#include <string.h>

static const struct { const char* name; const char* tag; double wind_know, ang_sd, vel_sd, learn; } L[NLEVELS] = {
    { "Cub",        "Wild throws. Ignores the wind.",      0.0, 18.0, 0.25, 1.00 },
    { "Chimp",      "Gets the idea. Mostly.",              0.3,  7.0, 0.12, 0.85 },
    { "Baboon",     "Reads the wind. Sometimes.",          0.7,  3.5, 0.06, 0.80 },
    { "Orangutan",  "Calm, careful, accurate.",            1.0,  2.4, 0.045, 0.70 },
    { "Silverback", "Rarely misses. Be afraid.",           1.0,  1.0, 0.022, 0.60 },
};

const char* bot_name(int level) { return L[level < 0 ? 0 : (level >= NLEVELS ? NLEVELS - 1 : level)].name; }
const char* bot_tagline(int level) { return L[level < 0 ? 0 : (level >= NLEVELS ? NLEVELS - 1 : level)].tag; }

void bot_new_round(Bot* b) { b->misses = 0; }
void bot_note_miss(Bot* b) { b->misses++; }

/* Flies a shot to its end. Returns 1 if it hits the opponent, 0 if not (self hits count as bad: -1).
 * *dist = how far from the opponent's middle it ended. */
static int sim(int me, double ang, double vel, double wind, double grav, double* dist, double* flight) {
    Shot s;
    int r, n = 0, other = 1 - me;
    double tx = W.gx[other] + (GW / 2), ty = W.gy[other] + (GH / 2);
    shot_start(&s, me, ang, vel);
    do { r = shot_step(&s, wind, grav); } while (r == SH_FLYING && ++n < 2000);
    *dist = hypot(s.x - tx, s.y - ty);
    *flight = s.t;
    if (r == SH_GORILLA) return s.victim == other ? 1 : -1;
    if (r == SH_FLYING) *dist = 1e9;
    return 0;
}

void bot_begin(Bot* b, int me, int level, int wind, double grav) {
    int misses = b->misses;
    memset(b, 0, sizeof *b);
    b->me = me; b->level = level; b->grav = grav;
    b->wind_seen = wind * L[level].wind_know;
    b->best_d = 1e18;
    b->misses = misses;
    b->ia = 4;
    b->iv = 4;
}

static void finish(Bot* b) {
    int a, v;
    double scale = pow(L[b->level].learn, b->misses);
    if (scale < 0.25) scale = 0.25;
    if (b->nhits > 0) {
        /* any hit that is not much slower than the quickest: a random one of those, for some variety */
        int k, cand[96], nc = 0;
        double tmin = 1e18;
        for (k = 0; k < b->nhits; k++) if (b->hit_t[k] < tmin) tmin = b->hit_t[k];
        for (k = 0; k < b->nhits; k++) if (b->hit_t[k] <= tmin * 1.5 + 0.5) cand[nc++] = k;
        k = cand[(int)(rng_u32() % (uint32_t)nc)];
        a = b->hit_a[k]; v = b->hit_v[k];
    } else { a = b->best_a; v = b->best_v; }
    /* the human error: a bit off in angle and in strength */
    {
        double fa = a + rnd_gauss() * L[b->level].ang_sd * scale;
        double fv = v * (1.0 + rnd_gauss() * L[b->level].vel_sd * scale);
        a = (int)floor(fa + 0.5); v = (int)floor(fv + 0.5);
    }
    if (a < 0) a = 0;
    if (a > 180) a = 180;
    if (v < 1) v = 1;
    if (v > 200) v = 200;
    b->angle = a; b->vel = v;
}

/* Keeps the 96 quickest hits: a long flight is at the wind's mercy (its push grows with the square of the time), so a quick
 * throw is the safer one on any world, and on a low-gravity world the only sensible one. */
static void record(Bot* b, int a, int v, int r, double d, double tt) {
    if (r == 1) {
        b->seen++;
        if (b->nhits < 96) { b->hit_a[b->nhits] = a; b->hit_v[b->nhits] = v; b->hit_t[b->nhits] = tt; b->nhits++; }
        else {
            int k, worst = 0;
            for (k = 1; k < 96; k++) if (b->hit_t[k] > b->hit_t[worst]) worst = k;
            if (tt < b->hit_t[worst]) { b->hit_a[worst] = a; b->hit_v[worst] = v; b->hit_t[worst] = tt; }
        }
    } else if (r == 0 && d < b->best_d) { b->best_d = d; b->best_a = a; b->best_v = v; }
}

/* A fine sweep (step 1) round the closest miss so far. */
static void start_sweep(Bot* b) {
    b->ca = b->best_a; b->cv = b->best_v;
    b->rA = 4; b->rV = b->cv * 8 / 100 < 3 ? 3 : b->cv * 8 / 100;
    b->ra = b->ca - b->rA; b->rv = b->cv - b->rV;
}

int bot_work(Bot* b, int budget) {
    if (b->done) return 1;
    while (budget-- > 0) {
        double d, tt;
        int r;
        if (b->phase == 0) {
            if (b->ia > 176) {
                if (b->nhits > 0 || b->best_d > 1e17) { finish(b); b->done = 1; return 1; }
                b->phase = 1; b->refine = 0; start_sweep(b);     /* the grid stepped over every hit: look closer */
                continue;
            }
            r = sim(b->me, b->ia, b->iv, b->wind_seen, b->grav, &d, &tt);
            record(b, b->ia, b->iv, r, d, tt);
            b->iv += b->iv < 30 ? 2 : 3;          /* finer at the slow end: low-gravity worlds need gentle throws */
            if (b->iv > 198) { b->iv = 4; b->ia += (b->ia < 24 || b->ia >= 156) ? 1 : 2; }     /* range changes fastest with the angle at the flat ends */
        } else {
            int a = b->ra, v = b->rv;
            if (b->ra > b->ca + b->rA) {          /* this sweep is done */
                if (b->nhits > 0 || ++b->refine >= 3) { finish(b); b->done = 1; return 1; }
                start_sweep(b);
                continue;
            }
            if (a >= 0 && a <= 180 && v >= 1 && v <= 200) {
                r = sim(b->me, a, v, b->wind_seen, b->grav, &d, &tt);
                record(b, a, v, r, d, tt);
            }
            if (++b->rv > b->cv + b->rV) { b->rv = b->cv - b->rV; b->ra++; }
        }
    }
    return 0;
}
