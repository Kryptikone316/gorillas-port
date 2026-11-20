/* Computer opponents. A bot works out shots by simulating them with the game's own physics,
 * then plays the best one with a level-dependent error: the weaker ones ignore the wind and
 * throw wildly; the stronger ones read the wind, aim true and zero in after a miss. */
#ifndef BOT_H
#define BOT_H

#define NLEVELS 5

typedef struct {
    int me, level;
    double wind_seen, grav;
    int ia, iv, started;
    int done;                   /* the answer is final (asking again does not draw new error) */
    int phase, refine, ca, cv, ra, rv, rA, rV;     /* phase 1: fine sweeps round the closest miss when the grid found no hit */
    int nhits, seen;
    int hit_a[96], hit_v[96];
    double hit_t[96];           /* flight time of each hit: the quick ones are the sure ones */
    double best_d;
    int best_a, best_v;
    int misses;                 /* this round: the bot gets more accurate with each */
    int angle, vel;             /* the answer */
} Bot;

const char* bot_name(int level);
const char* bot_tagline(int level);
void bot_new_round(Bot* b);
void bot_begin(Bot* b, int me, int level, int wind, double grav);
int bot_work(Bot* b, int budget);       /* runs up to budget simulated shots; 1 = the answer is ready */
void bot_note_miss(Bot* b);

#endif
