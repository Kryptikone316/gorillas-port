/* Prints the outcome of many random flights. Build once as is and once with -DSHOT_NO_SKIP (the plain stepping) and diff the
 * output: the flight skipping in shot_step must change nothing but the time it takes. */
#include "world.h"
#include "art.h"
#include <stdio.h>
#include <stdlib.h>

int main(void) {
    static const double g[10] = { 3.7, 8.9, 9.8, 1.6, 3.7, 24.8, 10.4, 8.9, 11.2, 0.6 };
    int i, city;
    long total_steps = 0;
    art_init(); world_init(); rng_seed(4242);
    for (city = 0; city < 20; city++) {
        world_new_city();
        for (i = 0; i < 1500; i++) {
            Shot s;
            int owner = rnd(2) - 1, wind = rnd(30) - 15, r;
            double ang = rnd(181) - 1, vel = rnd(200), gr = g[rnd(10) - 1];
            shot_start(&s, owner, ang, vel);
            do { r = shot_step(&s, wind, gr); } while (r == SH_FLYING);
            total_steps += s.steps;
            printf("%d %.5f %.5f %.5f %d\n", r, s.x, s.y, s.t, s.victim);
        }
    }
    fprintf(stderr, "calls to shot_step: %ld\n", total_steps);
    return 0;
}
