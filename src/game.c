/* Gorillas: menus, turn flow, drawing. */
#include "plat.h"
#include "gfx.h"
#include "art.h"
#include "world.h"
#include "bot.h"
#include "theme.h"
#include "osk.h"
#include "sound.h"
#include <math.h>
#include <strings.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ---------------------------------------------------------------- settings */
typedef struct {
    int mode;        /* 0 vs CPU, 1 two players, 2 CPU vs CPU */
    int diff;        /* 0..NLEVELS-1 */
    int rounds;      /* 1..15 */
    int grav;        /* index into k_grav */
    int windmode;    /* 0 off, 1 per round (classic), 2 every throw */
    int input;       /* 0 D-pad + keypad, 1 D-pad only, 2 keypad only */
    int guide, trail, vol, fps;
    int sky;         /* 0..NSKY-1 a sun or moon (moons = night sky), NSKY = random per round, NSKY+1 = Auto: the theme's own */
    int theme;       /* look of the city: TH_* in theme.h (cosmetic) */
    int look[2];     /* cosmetic gorilla for each side (index into art.c's looks) */
    char name[2][12];
} Cfg;

static Cfg C;
static Cfg g_saved;      /* the player's own settings while a Quick Game borrows C for its random ones */
static int g_quick;
static int qb(void) { return C.theme == TH_QBASIC; }         /* the purist theme: the original's look and feel throughout */
#define PANEL_COL (qb() ? RGB(0, 0, 0) : RGB(16, 22, 56))
#define FRAME_COL (qb() ? RGB(170, 170, 170) : COL_GOLD)
/* Surface gravity in m/s^2 (the Moon is no planet, but it earns its place). In order out from the Sun. */
static const struct { const char* name; double g; } k_grav[] = {
    { "Mercury", 3.7 }, { "Venus", 8.9 }, { "Earth", 9.8 }, { "Moon", 1.6 }, { "Mars", 3.7 },
    { "Jupiter", 24.8 }, { "Saturn", 10.4 }, { "Uranus", 8.9 }, { "Neptune", 11.2 }, { "Pluto", 0.6 } };
#define NGRAV 10

static int grav_find(const char* name) {
    int i;
    for (i = 0; i < NGRAV; i++) if (!strcasecmp(name, k_grav[i].name)) return i;
    return -1;
}

/* gorillas.txt holds the world's name; older files held a number (0 Earth, 1 Moon, 2 Mars, 3 Jupiter, 4 Pluto) */
static int grav_parse(const char* s) {
    static const char* const old[5] = { "Earth", "Moon", "Mars", "Jupiter", "Pluto" };
    int i;
    if (*s >= '0' && *s <= '9') { i = atoi(s); return grav_find(old[i < 0 ? 0 : (i > 4 ? 4 : i)]); }
    i = grav_find(s);
    return i >= 0 ? i : grav_find("Earth");
}
static const char* const k_mode[3] = { "Vs CPU", "2 Players", "CPU Vs CPU" };
static const char* const k_wind[3] = { "Off", "Per Round", "Every Throw" };
static const char* const k_input[3] = { "D-Pad + Keypad", "D-Pad Only", "Keypad Only" };

static const char* CFG_FILE = "gorillas.txt";

/* Each computer level has a look that suits it (cosmetic only); Gorilla 2 = Auto uses it. */
static const int k_bot_look[NLEVELS] = { 11 /* Cub: Party */, 3 /* Chimp: Jungle */, 2 /* Baboon: Cherry */, 8 /* Orangutan: Shades */, 10 /* Silverback: King */ };
static int resolve_look2(void) {
    if (C.look[1] >= 0) return C.look[1];
    return C.mode == 1 ? 1 : k_bot_look[C.diff];     /* two players: Silver; otherwise the opponent's own look */
}

static int clampi(int v, int lo, int hi) { return v < lo ? lo : (v > hi ? hi : v); }

static int theme_parse(const char* v) {            /* a name (as the file writes it) or a number */
    int i;
    for (i = 0; i < NTHEME; i++) if (!strcasecmp(v, theme_name(i))) return i;
    return clampi(atoi(v), 0, NTHEME - 1);
}

static int sky_parse(const char* v) {                /* a name (as the file writes it), or an old number: 0..5 as now, 6 was Random, 7 was Auto */
    int i;
    for (i = 0; i < NSKY + 2; i++) if (!strcasecmp(v, sky_name(i))) return i;
    i = atoi(v);
    return i <= 5 ? (i < 0 ? 0 : i) : (i == 6 ? NSKY : NSKY + 1);
}

static void cfg_defaults(void) {
    memset(&C, 0, sizeof C);
    C.mode = 0; C.diff = 2; C.rounds = 3; C.grav = grav_find("Earth"); C.windmode = 1; C.input = 0;
    C.guide = 1; C.trail = 0; C.vol = 7; C.fps = 50; C.look[0] = 0; C.look[1] = -1; C.sky = NSKY + 1; C.theme = TH_QBASIC;
    strcpy(C.name[0], "Player 1");
    strcpy(C.name[1], "Player 2");
}

static void cfg_load(void) {
    FILE* f = fopen(CFG_FILE, "r");
    char line[128];
    cfg_defaults();
    if (f) {
        while (fgets(line, sizeof line, f)) {
            char* eq = strchr(line, '=');
            char* nl;
            if (line[0] == '#' || !eq) continue;
            *eq = 0; eq++;
            if ((nl = strpbrk(eq, "\r\n"))) *nl = 0;
            if (!strcmp(line, "mode")) C.mode = atoi(eq);
            else if (!strcmp(line, "opponent")) C.diff = atoi(eq);
            else if (!strcmp(line, "rounds")) C.rounds = atoi(eq);
            else if (!strcmp(line, "gravity")) C.grav = grav_parse(eq);
            else if (!strcmp(line, "wind")) C.windmode = atoi(eq);
            else if (!strcmp(line, "input")) C.input = atoi(eq);
            else if (!strcmp(line, "guide")) C.guide = atoi(eq);
            else if (!strcmp(line, "trail")) C.trail = atoi(eq);
            else if (!strcmp(line, "volume")) C.vol = atoi(eq);
            else if (!strcmp(line, "fps")) C.fps = atoi(eq);
            else if (!strcmp(line, "sky")) C.sky = sky_parse(eq);
            else if (!strcmp(line, "theme")) C.theme = theme_parse(eq);
            else if (!strcmp(line, "gorilla1")) C.look[0] = atoi(eq);
            else if (!strcmp(line, "gorilla2")) C.look[1] = atoi(eq);
            else if (!strcmp(line, "name1")) { strncpy(C.name[0], eq, 10); C.name[0][10] = 0; }
            else if (!strcmp(line, "name2")) { strncpy(C.name[1], eq, 10); C.name[1][10] = 0; }
        }
        fclose(f);
    }
    if (getenv("GOR_MODE")) C.mode = atoi(getenv("GOR_MODE"));
    if (getenv("GOR_DIFF")) C.diff = atoi(getenv("GOR_DIFF"));
    if (getenv("GOR_ROUNDS")) C.rounds = atoi(getenv("GOR_ROUNDS"));
    if (getenv("GOR_GRAV")) C.grav = atoi(getenv("GOR_GRAV"));
    if (getenv("GOR_SKY")) C.sky = atoi(getenv("GOR_SKY"));
    if (getenv("GOR_THEME")) C.theme = atoi(getenv("GOR_THEME"));
    if (getenv("GOR_INPUT")) C.input = atoi(getenv("GOR_INPUT"));
    C.mode = clampi(C.mode, 0, 2); C.diff = clampi(C.diff, 0, NLEVELS - 1); C.rounds = clampi(C.rounds, 1, 15);
    C.grav = clampi(C.grav, 0, NGRAV - 1); C.windmode = clampi(C.windmode, 0, 2); C.input = clampi(C.input, 0, 2);
    C.guide = C.guide != 0; C.trail = C.trail != 0; C.vol = clampi(C.vol, 0, 10); C.fps = clampi(C.fps, 20, 100);
    C.sky = clampi(C.sky, 0, NSKY + 1); C.theme = clampi(C.theme, 0, NTHEME - 1);
    C.look[0] = clampi(C.look[0], 0, NLOOKS - 1); C.look[1] = clampi(C.look[1], -1, NLOOKS - 1);
    if (!C.name[0][0]) strcpy(C.name[0], "Player 1");
    if (!C.name[1][0]) strcpy(C.name[1], "Player 2");
}

/* A Quick Game's random settings are never written: the file always keeps the player's own (volume and aim guide, which the
 * pause menu can change, are the exception: they are preferences, not part of the random set). */
static void cfg_save(void) {
    Cfg e = C;
    FILE* f = fopen(CFG_FILE, "w");
    if (!f) return;
    if (g_quick) { e = g_saved; e.vol = C.vol; e.guide = C.guide; }
    fprintf(f, "# Gorillas settings (the menu reads and writes this file)\n");
    fprintf(f, "# mode: 0 vs CPU, 1 two players, 2 CPU vs CPU\nmode=%d\n", e.mode);
    fprintf(f, "# opponent: 0 Cub, 1 Chimp, 2 Baboon, 3 Orangutan, 4 Silverback\nopponent=%d\n", e.diff);
    fprintf(f, "rounds=%d\n# gravity: Mercury, Venus, Earth, Moon, Mars, Jupiter, Saturn, Uranus, Neptune or Pluto\ngravity=%s\n", e.rounds, k_grav[e.grav].name);
    fprintf(f, "# wind: 0 off, 1 per round (classic), 2 every throw\nwind=%d\n", e.windmode);
    fprintf(f, "# input: 0 D-pad + keypad, 1 D-pad only, 2 keypad only\ninput=%d\n", e.input);
    fprintf(f, "guide=%d\ntrail=%d\nvolume=%d\n# fps: frame rate cap (the game keeps its speed)\nfps=%d\n", e.guide, e.trail, e.vol, e.fps);
    fprintf(f, "# theme: Q-BASIC (the original's look and feel), Classic (this game's own first look), Uptown, Moonlit or Harbor; purely cosmetic\ntheme=%s\n", theme_name(e.theme));
    fprintf(f, "# sky: Smiley Sun, Cool Sun, Retro Sun, Crescent Moon, Full Moon, Sleepy Moon (a night sky), Q-BASIC Sun, Random (each round) or Auto (the theme's own); purely cosmetic\nsky=%s\n", sky_name(e.sky));
    fprintf(f, "# gorilla1, gorilla2: the look of each gorilla (0 Goldie, 1 Silver, 2 Cherry, 3 Jungle, 4 Blueberry, 5 Grape, 6 Snowy,\n# 7 Midnight, 8 Shades, 9 Bandit, 10 King, 11 Party); purely cosmetic. gorilla2=-1 is Auto: the opponent's own look by level\ngorilla1=%d\ngorilla2=%d\n", e.look[0], e.look[1]);
    fprintf(f, "name1=%s\nname2=%s\n", e.name[0], e.name[1]);
    fclose(f);
}

/* ---------------------------------------------------------------- input */
static unsigned g_down, g_prev, g_press, g_rep;
static int g_hold[14];

static void input_update(void) {
    int i;
    g_down = plat_buttons();
    g_press = g_down & ~g_prev;
    g_rep = g_press;
    for (i = 0; i < 14; i++) {
        if (g_down & (1u << i)) g_hold[i]++; else g_hold[i] = 0;
        if (i <= 3 && g_hold[i] > 14 && (g_hold[i] - 14) % 3 == 0) g_rep |= 1u << i;
    }
    g_prev = g_down;
}

/* ---------------------------------------------------------------- game state */
enum { ST_TITLE, ST_HELP, ST_PLAY, ST_OVER, ST_NAME };
enum { PH_START, PH_AIM, PH_KEYPAD, PH_THINK, PH_ARM, PH_FLY, PH_BOOM, PH_GBOOM, PH_DANCE, PH_RESULT };

static struct {
    int state, phase, timer;
    int human[2], level[2];
    char name[2][12];
    int score[2], round, rounds;
    int turn;                      /* who throws */
    int ang[2], vel[2];            /* the numbers each player has set */
    Shot shot;
    int shock;
    int boom_x, boom_y, boom_t;
    int hit_who, winner;
    int dance_t;
    Bot bot[2];
    int tgt_ang, tgt_vel, think_t;
    int kp_stage; Osk osk; int kp_msg;
    int paused, pause_sel;
    int arm_pose;
    int trail_n[2]; short trail[2][400][2];
    int help_page;
    int menu_sel, name_for, page;
    int look[2];                   /* the looks used in this match */
    int sky;                       /* the sun or moon of this round */
    int wind_note;
    char banner[48]; int banner_t;
    char sub[96];                  /* a second banner line (Quick Game says what it rolled) */
} G;

static int g_quit_code = -1;

static int is_human(int p) { return G.human[p]; }

static int roll_wind(void) {
    int w;
    if (C.windmode == 0) return 0;
    w = rnd(10) - 5;
    if (rnd(3) == 1) w += (w > 0) ? rnd(10) : -rnd(10);
    return w;
}

static void set_banner(const char* s, int frames) { snprintf(G.banner, sizeof G.banner, "%s", s); G.banner_t = frames; }

static void begin_turn(void) {
    int p = G.turn;
    G.shock = 0;
    G.shot.t = 0;
    G.arm_pose = POSE_DOWN;
    if (is_human(p)) {
        G.phase = PH_AIM;
        if (C.input == 2) {   /* keypad only: type the shot at once */
            osk_open(&G.osk, OSK_NUM, "Angle", "0 to 180 degrees (START = OK)", "", 6);
            G.kp_stage = 0; G.phase = PH_KEYPAD;
        }
    } else {
        G.phase = PH_THINK; G.think_t = 0; G.tgt_ang = -1; G.timer = 0;
        bot_begin(&G.bot[p], p, G.level[p], W.wind, W.grav);
    }
}

static int pick_sky(void) {
    static unsigned lcg = 2463534242u;                 /* not the game's random numbers: the sky never changes a game */
    if (qb()) return SKY_QB;                          /* the purist theme has its own sun and nothing else */
    if (C.sky < NSKY) return C.sky;
    if (C.sky > NSKY) return theme_default_sky(C.theme);
    lcg = lcg * 1664525u + 1013904223u + plat_ticks();
    return (int)((lcg >> 16) % NSKY);
}

static void new_round(void) {
    W.grav = k_grav[C.grav].g;
    G.sky = pick_sky();
    W.night = sky_is_night(G.sky);
    W.theme = C.theme;
    world_new_city();
    W.wind = roll_wind();
    G.trail_n[0] = G.trail_n[1] = 0;
    G.boom_t = 0;
    bot_new_round(&G.bot[0]); bot_new_round(&G.bot[1]);
    G.phase = PH_START; G.timer = 0;
    snprintf(G.banner, sizeof G.banner, "ROUND %d", G.round);
    G.banner_t = 60;
}

static void start_match(void) {
    int i;
    memset(&G.score, 0, sizeof G.score);
    G.state = ST_PLAY;
    G.sub[0] = 0;
    G.round = 1; G.rounds = C.rounds;
    G.paused = 0;
    for (i = 0; i < 2; i++) { G.ang[i] = 45; G.vel[i] = 50; }
    if (C.mode == 0) {
        G.human[0] = 1; G.human[1] = 0; G.level[1] = C.diff;
        snprintf(G.name[0], sizeof G.name[0], "%s", C.name[0]);
        snprintf(G.name[1], sizeof G.name[1], "%s", bot_name(C.diff));
    } else if (C.mode == 1) {
        G.human[0] = G.human[1] = 1;
        snprintf(G.name[0], sizeof G.name[0], "%s", C.name[0]);
        snprintf(G.name[1], sizeof G.name[1], "%s", C.name[1]);
    } else {
        int d0 = getenv("GOR_DIFF1") ? clampi(atoi(getenv("GOR_DIFF1")), 0, NLEVELS - 1) : C.diff;
        G.human[0] = G.human[1] = 0; G.level[0] = d0; G.level[1] = C.diff;
        snprintf(G.name[0], sizeof G.name[0], "%s", bot_name(d0));
        snprintf(G.name[1], sizeof G.name[1], "%s", bot_name(C.diff));
        if (d0 == C.diff) { snprintf(G.name[0], sizeof G.name[0], "%s", "Left"); snprintf(G.name[1], sizeof G.name[1], "%s", "Right"); }
    }
    G.look[0] = qb() ? LOOK_QB : C.look[0]; G.look[1] = qb() ? LOOK_QB : resolve_look2();
    snd_use_speaker(qb());
    gfx_shadows(!qb());
    if (!qb() && G.look[1] == G.look[0]) G.look[1] = (G.look[1] + 1) % NLOOKS;      /* two identical gorillas would be a muddle */
    G.turn = 0;
    new_round();
}

static void end_throw_miss(void) {
    int p = G.turn;
    if (!is_human(p)) bot_note_miss(&G.bot[p]);
    G.turn = 1 - p;
    if (C.windmode == 2) W.wind = roll_wind();
    begin_turn();
}

/* ---------------------------------------------------------------- drawing */
static uint16_t* fb;
#define COL_GOLD (qb() ? RGB(255, 255, 85) : RGB(255, 214, 60))
#define COL_WHITE RGB(255, 255, 255)
#define COL_BLACK RGB(0, 0, 0)

static void draw_wind_arrow(void) {
    int len = W.wind * 6, cx = SCR_W / 2, y = SCR_H - 7;
    if (!len) return;
    gfx_rect(len > 0 ? cx : cx + len, y - 1, len > 0 ? len : -len, 3, RGB(255, 80, 60));
    {
        int dir = len > 0 ? -1 : 1, tip = cx + len;
        gfx_line(tip, y, tip + dir * 6, y - 5, RGB(255, 80, 60));
        gfx_line(tip, y, tip + dir * 6, y + 5, RGB(255, 80, 60));
        gfx_line(tip, y - 1, tip + dir * 6, y - 6, RGB(255, 80, 60));
        gfx_line(tip, y + 1, tip + dir * 6, y + 6, RGB(255, 80, 60));
    }
}

/* The Q-BASIC theme's HUD: names in the top corners, the active player's prompts under the name, the score in a little box
 * over the city, and a single plain line in the black bar. */
static void draw_hud_qb(void) {
    char buf[48];
    int p, w;
    const uint16_t white = RGB(255, 255, 255);
    snprintf(buf, sizeof buf, "%d>Score<%d", G.score[0], G.score[1]);
    w = (int)strlen(buf) * 8;
    gfx_rect(SCR_W / 2 - w / 2 - 3, BASELINE - 42, w + 6, 20, RGB(0, 0, 170));
    gfx_text_xy(SCR_W / 2 - w / 2, BASELINE - 40, 1, 2, white, buf);
    for (p = 0; p < 2; p++) {
        int active = (G.phase >= PH_AIM && G.phase <= PH_FLY && G.turn == p);
        int x = p == 0 ? 4 : SCR_W - 4 - (int)strlen(G.name[p]) * 8;
        gfx_text_xy(x, 2, 1, 2, white, G.name[p]);
        if (active) {
            int ax = p == 0 ? 4 : SCR_W - 4 - 13 * 8;
            snprintf(buf, sizeof buf, "Angle: %d", G.ang[p]);
            gfx_text_xy(ax, 20, 1, 2, white, buf);
            snprintf(buf, sizeof buf, "Velocity: %d", G.vel[p]);
            gfx_text_xy(ax, 38, 1, 2, white, buf);
            if (G.phase == PH_THINK) gfx_text_xy(ax, 56, 1, 2, RGB(170, 170, 170), "Thinking...");
        }
    }
    draw_wind_arrow();
    snprintf(buf, sizeof buf, "Round %d/%d   %s %.1f", G.round, G.rounds, k_grav[C.grav].name, k_grav[C.grav].g);
    gfx_text(8, SCR_H - 12, 1, RGB(170, 170, 170), buf);
    if (C.mode == 2 && G.phase != PH_KEYPAD) gfx_text(SCR_W - 8 - gfx_text_w("B = stop", 1), SCR_H - 12, 1, RGB(170, 170, 170), "B = stop");
}

static void draw_hud(void) {
    char buf[48];
    int p, x;
    if (qb()) { draw_hud_qb(); return; }
    snprintf(buf, sizeof buf, "%d > Score < %d", G.score[0], G.score[1]);
    gfx_text_c(SCR_W / 2, BASELINE + 5, 2, COL_WHITE, COL_BLACK, buf);
    draw_wind_arrow();
    snprintf(buf, sizeof buf, "Round %d/%d", G.round, G.rounds);
    gfx_text(8, BASELINE + 5, 1, RGB(170, 180, 220), buf);
    snprintf(buf, sizeof buf, "%s %.1f", k_grav[C.grav].name, k_grav[C.grav].g);
    gfx_text(8, BASELINE + 17, 1, RGB(170, 180, 220), buf);
    if (C.windmode) { snprintf(buf, sizeof buf, "Wind %d", W.wind); gfx_text(SCR_W - 8 - gfx_text_w(buf, 1), BASELINE + 5, 1, RGB(170, 180, 220), buf); }
    for (p = 0; p < 2; p++) {
        int active = (G.phase >= PH_AIM && G.phase <= PH_FLY && G.turn == p);
        x = p == 0 ? 8 : SCR_W - 8 - gfx_text_w(G.name[p], 2);
        gfx_text_sh(x, 6, 2, active ? COL_GOLD : COL_WHITE, COL_BLACK, G.name[p]);
        if (active) {
            int ax = p == 0 ? 8 : SCR_W - 8 - gfx_text_w("Velocity: 000", 2);
            snprintf(buf, sizeof buf, "Angle: %d", G.ang[p]);
            gfx_text_sh(ax, 28, 2, COL_WHITE, COL_BLACK, buf);
            snprintf(buf, sizeof buf, "Velocity: %d", G.vel[p]);
            gfx_text_sh(ax, 48, 2, COL_WHITE, COL_BLACK, buf);
            if (G.phase == PH_THINK) {
                const char* dots[4] = { "thinking", "thinking.", "thinking..", "thinking..." };
                gfx_text_sh(ax, 70, 1, RGB(200, 210, 255), COL_BLACK, dots[(plat_frame() / 12) & 3]);
            }
        }
    }
}

static void draw_sun(void) { spr_blit(spr_sky[G.sky][G.shock], SCR_W / 2 - SUN_W / 2, 6); }

static void draw_gorillas(void) {
    int p;
    for (p = 0; p < 2; p++) {
        int pose = POSE_DOWN;
        if (!W.alive[p]) continue;
        if (G.phase == PH_ARM && G.turn == p) pose = (p == 0) ? POSE_RIGHT_UP : POSE_LEFT_UP;
        if (G.phase == PH_DANCE && G.winner == p) pose = ((G.dance_t / 10) & 1) ? POSE_LEFT_UP : POSE_RIGHT_UP;
        spr_blit(spr_gor[G.look[p]][pose], W.gx[p], W.gy[p]);
    }
}

static void draw_guide(void) {
    int p = G.turn, hx, hy, i, len;
    double a, dx, dy, n;
    if (!C.guide || !is_human(p) || G.phase != PH_AIM) return;
    gorilla_hand(p, &hx, &hy);
    hy += 4;
    a = (p == 0 ? G.ang[p] : 180 - G.ang[p]) * 3.14159265 / 180.0;
    dx = cos(a); dy = -sin(a) * (double)SCR_H / 350.0;
    n = sqrt(dx * dx + dy * dy); dx /= n; dy /= n;
    len = 18 + G.vel[p] * 3 / 10;
    for (i = 14; i < len + 14; i += 7) {
        int px = hx + (int)(dx * i), py = hy + (int)(dy * i);
        gfx_rect(px - 1, py - 1, 3, 3, RGB(255, 236, 120));
    }
}

static void draw_trail(void) {
    int p, i;
    if (!C.trail) return;
    for (p = 0; p < 2; p++)
        for (i = 0; i < G.trail_n[p]; i++) gfx_rect(G.trail[p][i][0], G.trail[p][i][1], 2, 2, RGB(255, 255, 255));
}

static void draw_banana(void) {
    if (G.phase == PH_FLY) spr_blit((qb() ? spr_ban_qb : spr_ban)[G.shot.rot & 3], (int)G.shot.x - BAN_W / 2, (int)G.shot.y - BAN_W / 2);
}

static void fire(int cx, int cy, int rx, int ry, int f256) {
    int rr = rx * f256 / 256, ry2 = ry * f256 / 256, k;
    static const int jx[6] = { -5, 6, 0, -7, 7, 1 }, jy[6] = { -3, -5, 6, 5, 4, -7 };
    for (k = 0; k < 6; k++)   /* lumpy edge: a few overlapping blobs */
        gfx_ellipse(cx + jx[k] * rr / 24, cy + jy[k] * ry2 / 24, rr * 7 / 10 + 2, ry2 * 7 / 10 + 2, RGB(214, 60, 20));
    gfx_ellipse(cx, cy, rr + 1, ry2 + 1, RGB(255, 110, 20));
    for (k = 0; k < 3; k++) gfx_ellipse(cx + jx[k + 3] * rr / 40, cy + jy[k + 3] * ry2 / 40, rr * 8 / 10, ry2 * 8 / 10, RGB(255, 170, 30));
    gfx_ellipse(cx, cy, rr * 6 / 10, ry2 * 6 / 10, RGB(255, 226, 90));
    gfx_ellipse(cx, cy, rr * 3 / 10, ry2 * 3 / 10, RGB(255, 252, 210));
}

/* The original's explosions: flat crimson discs, a gorilla's alternating with its own peach. */
static void draw_booms_qb(void) {
    const uint16_t boom = RGB(255, 0, 85), peach = RGB(255, 170, 85);
    if (G.phase == PH_BOOM) {
        int t = G.boom_t, f = t <= 8 ? t * 32 : (16 - t) * 32;
        if (t < 16 && f > 0) gfx_ellipse(G.boom_x, G.boom_y, 10 * f / 256 + 1, 13 * f / 256 + 1, boom);
    } else if (G.phase == PH_GBOOM) {
        int t = G.boom_t, f = t <= 28 ? t * 256 / 28 : (60 - t) * 256 / 32, r, cx = W.gx[G.hit_who] + GW / 2, cy = W.gy[G.hit_who] + 22, k = 0;
        if (f < 0) f = 0;
        for (r = 50 * f / 256; r > 1; r -= 3, k++) gfx_ellipse(cx, cy, r, r * 5 / 4, (k & 1) ? peach : boom);
    }
}

static void draw_booms(void) {
    if (qb()) { draw_booms_qb(); return; }
    if (G.phase == PH_BOOM) {
        int t = G.boom_t, f = t <= 8 ? t * 32 : (16 - t) * 32;
        if (t < 16) fire(G.boom_x, G.boom_y, 16, 20, f > 0 ? f : 1);
    } else if (G.phase == PH_GBOOM) {
        int t = G.boom_t, k, f;
        int cx = W.gx[G.hit_who] + GW / 2, cy = W.gy[G.hit_who] + 22;
        f = t <= 28 ? t * 256 / 28 : (60 - t) * 256 / 32;
        if (f < 0) f = 0;
        fire(cx, cy, 44, 52, f);
        for (k = 0; k < 14; k++) {
            double a = k * 6.2831853 / 14 + t * 0.05;
            int r = t * 2 + (k & 3) * 6;
            gfx_rect(cx + (int)(cos(a) * r), cy + (int)(sin(a) * r * 0.8), 3, 3, (k & 1) ? RGB(255, 230, 80) : RGB(255, 120, 30));
        }
    }
}

static void draw_banner(void) {
    if (G.banner_t > 0) {
        gfx_text_c(SCR_W / 2, 100, 3, COL_WHITE, COL_BLACK, G.banner);
        if (G.sub[0] && G.round == 1) {      /* Quick Game: what it rolled, a line each */
            char line[96], *p = G.sub, *nl;
            int y = 136;
            snprintf(line, sizeof line, "%s", p);
            for (p = line; p && *p; p = nl ? nl + 1 : NULL, y += 24) {
                nl = strchr(p, '\n');
                if (nl) *nl = 0;
                gfx_text_c(SCR_W / 2, y, 2, COL_GOLD, COL_BLACK, p);
            }
        }
    }
}

static void draw_play(void) {
    memcpy(fb, W.pic, (size_t)SCR_W * SCR_H * sizeof(uint16_t));
    gfx_target(fb, SCR_W, SCR_H);
    draw_sun();
    draw_gorillas();
    draw_trail();
    draw_guide();
    draw_booms();
    draw_banana();
    draw_hud();
    draw_banner();
    if (G.phase == PH_AIM && is_human(G.turn)) {
        const char* h = C.input == 2 ? "A=Enter shot" : (C.input == 1 ? "Up/Dn angle  L/R power  A=Throw" : "Up/Dn angle  L/R power  A=Throw  X=Keypad");
        gfx_text_c(SCR_W / 2, BASELINE - 14, 1, COL_WHITE, COL_BLACK, h);
    }
    if (G.phase == PH_KEYPAD) osk_draw(&G.osk);
    if (G.kp_msg > 0 && G.phase == PH_KEYPAD) gfx_text_c(SCR_W / 2, SCR_H / 2 + 150, 2, RGB(255, 120, 100), COL_BLACK, "Out of range");
    if (!qb() && C.mode == 2 && G.phase != PH_KEYPAD) gfx_text(SCR_W - 8 - gfx_text_w("B = stop", 1), BASELINE + 17, 1, RGB(200, 210, 255), "B = stop");
}

/* ---------------------------------------------------------------- pause menu */
static const char* const k_pause[4] = { "Resume", "Volume", "Aim guide", "Quit to title" };

static void draw_pause(void) {
    int i, x = SCR_W / 2 - 150, y = 130;
    char b[40];
    gfx_dim(0, 0, SCR_W, SCR_H);
    gfx_rect(x, y, 300, 190, PANEL_COL);
    gfx_frame(x, y, 300, 190, FRAME_COL);
    gfx_text_c(SCR_W / 2, y + 12, 3, COL_WHITE, COL_BLACK, "PAUSED");
    for (i = 0; i < 4; i++) {
        int sel = (i == G.pause_sel);
        if (i == 1) snprintf(b, sizeof b, "Volume  < %d >", C.vol);
        else if (i == 2) snprintf(b, sizeof b, "Aim guide  < %s >", C.guide ? "On" : "Off");
        else snprintf(b, sizeof b, "%s", k_pause[i]);
        gfx_text_c(SCR_W / 2, y + 60 + i * 28, 2, sel ? COL_GOLD : COL_WHITE, COL_BLACK, b);
    }
}

/* ---------------------------------------------------------------- turn logic */
static void adjust(int* v, int d, int lo, int hi) { *v = clampi(*v + d, lo, hi); }

static void aim_input(void) {
    int p = G.turn;
    static const int ids[4] = { 2, 3, 0, 1 };   /* UP, DOWN, LEFT, RIGHT -> hold slots */
    int i;
    for (i = 0; i < 4; i++) {
        int h = G.paused ? 0 : g_hold[ids[i]];
        int fire_now = (h == 1) || (h > 12 && (h % 2) == 0);
        int step = h > 70 ? 5 : 1;
        if (!fire_now) continue;
        if (i == 0) adjust(&G.ang[p], step, 0, 180);
        if (i == 1) adjust(&G.ang[p], -step, 0, 180);
        if (i == 2) adjust(&G.vel[p], -step, 1, 200);
        if (i == 3) adjust(&G.vel[p], step, 1, 200);
        snd_play(SND_TICK);
    }
    if (g_press & B_B) { C.guide = !C.guide; snd_play(SND_BLIP); }
    if ((g_press & B_X) && C.input != 1) {
        osk_open(&G.osk, OSK_NUM, "Angle", "0 to 180 degrees (START = OK)", "", 6);
        G.kp_stage = 0; G.phase = PH_KEYPAD;
        return;
    }
    if (g_press & B_A) {
        if (C.input == 2) {
            osk_open(&G.osk, OSK_NUM, "Angle", "0 to 180 degrees (START = OK)", "", 6);
            G.kp_stage = 0; G.phase = PH_KEYPAD;
        } else { G.phase = PH_ARM; G.timer = 0; G.arm_pose = 1; snd_play(SND_THROW); }
    }
}

static void keypad_input(void) {
    int p = G.turn;
    if (G.kp_msg > 0) G.kp_msg--;
    osk_input(&G.osk, g_rep);
    if (G.osk.result == OSK_CANCEL) { G.phase = PH_AIM; return; }
    if (G.osk.result == OSK_OK) {
        double v = G.osk.buf[0] ? atof(G.osk.buf) : (G.kp_stage == 0 ? G.ang[p] : G.vel[p]);
        int lo = G.kp_stage == 0 ? 0 : 1, hi = G.kp_stage == 0 ? 180 : 200;
        if (v < lo || v > hi) {   /* the original also refused values out of range */
            osk_open(&G.osk, OSK_NUM, G.kp_stage == 0 ? "Angle" : "Velocity", G.kp_stage == 0 ? "0 to 180 degrees (START = OK)" : "1 to 200 (START = OK)", "", 6);
            G.kp_msg = 70; snd_play(SND_LOSE);
            return;
        }
        if (G.kp_stage == 0) {
            G.ang[p] = (int)(v + 0.5);
            G.kp_stage = 1;
            osk_open(&G.osk, OSK_NUM, "Velocity", "1 to 200 (START = OK)", "", 6);
            snd_play(SND_SELECT);
        } else {
            G.vel[p] = (int)(v + 0.5);
            G.phase = PH_ARM; G.timer = 0; G.arm_pose = 1; snd_play(SND_THROW);
        }
    }
}

static void think(void) {
    int p = G.turn;
    Bot* b = &G.bot[p];
    G.think_t++;
    if (G.tgt_ang < 0) {
        int done;
        if (plat_headless()) done = bot_work(b, 220);          /* tests: a fixed amount per frame, so runs repeat exactly */
        else {      /* the device: about 6 ms of searching a frame, whatever the CPU, so the picture never stutters while it thinks */
            unsigned t0 = plat_ticks();
            do { done = bot_work(b, 16); } while (!done && plat_ticks() - t0 < 6);
        }
        if (done && G.think_t > 24) { G.tgt_ang = b->angle; G.tgt_vel = b->vel; }
        return;
    }
    /* the numbers "type themselves" into the fields */
    {
        int da = G.tgt_ang - G.ang[p], dv = G.tgt_vel - G.vel[p];
        if (da || dv) {
            int sa = abs(da) > 40 ? 3 : (abs(da) > 12 ? 2 : 1), sv = abs(dv) > 60 ? 4 : (abs(dv) > 20 ? 2 : 1);
            G.ang[p] += da > 0 ? (da < sa ? da : sa) : (-da < sa ? da : -sa);
            G.vel[p] += dv > 0 ? (dv < sv ? dv : sv) : (-dv < sv ? dv : -sv);
            if (!(G.think_t & 3)) snd_play(SND_TICK);
        } else {
            if (++G.timer > 18) { G.phase = PH_ARM; G.timer = 0; G.arm_pose = 1; snd_play(SND_THROW); }
        }
    }
}

static void start_hit(int victim) {
    int thrower = G.turn;
    G.hit_who = victim;
    G.winner = (victim == thrower) ? 1 - thrower : thrower;
    G.score[G.winner]++;
    G.phase = PH_GBOOM; G.boom_t = 0;
    snd_play(SND_BIGBOOM);
}

static void trail_add(void) {
    int p = G.turn;
    if (G.trail_n[p] < 400 && (G.shot.steps & 1) == 0) {
        G.trail[p][G.trail_n[p]][0] = (short)G.shot.x;
        G.trail[p][G.trail_n[p]][1] = (short)G.shot.y;
        G.trail_n[p]++;
    }
}

static void update_play(void) {
    int p = G.turn;
    if (G.banner_t > 0) G.banner_t--;
    switch (G.phase) {
        case PH_START:
            if (++G.timer > 50) begin_turn();
            break;
        case PH_AIM:
            if (is_human(p)) aim_input();
            break;
        case PH_KEYPAD: keypad_input(); break;
        case PH_THINK: think(); break;
        case PH_ARM:
            if (++G.timer >= 8) {
                shot_start(&G.shot, p, G.ang[p], G.vel[p]);
                G.trail_n[p] = 0;
                G.phase = PH_FLY;
            }
            break;
        case PH_FLY: {
            int r, was = G.shock, n = 1, k;
            /* high above the top edge nothing can be seen: let the banana fly on faster until it is back */
            if (G.shot.y < -200) n = 12; else if (G.shot.y < -40) n = 5;
            for (k = 0, r = SH_FLYING; k < n && r == SH_FLYING; k++) r = shot_step(&G.shot, W.wind, W.grav);
            if (G.shot.sun) G.shock = 1;
            if (!was && G.shock) snd_play(SND_OOH);
            trail_add();
            if (r == SH_BUILDING) {
                G.boom_x = (int)G.shot.x + 3; G.boom_y = (int)G.shot.y + 3; G.boom_t = 0; G.phase = PH_BOOM;
                snd_play(SND_BOOM);
            } else if (r == SH_GORILLA) {
                start_hit(G.shot.victim);
            } else if (r == SH_OFFSCREEN) {
                end_throw_miss();
            }
            break;
        }
        case PH_BOOM:
            if (G.boom_t == 8) world_carve(G.boom_x, G.boom_y, 10, 13);
            if (++G.boom_t >= 16) end_throw_miss();
            break;
        case PH_GBOOM:
            if (G.boom_t == 20) {
                W.alive[G.hit_who] = 0;
                world_carve(W.gx[G.hit_who] + GW / 2, W.gy[G.hit_who] + GH - 4, 24, 20);
            }
            if (++G.boom_t >= 60) { G.phase = PH_DANCE; G.dance_t = 0; snd_play(SND_DANCE); }
            break;
        case PH_DANCE:
            if (++G.dance_t >= 80) {
                char b[48];
                snprintf(b, sizeof b, "%s scores!", G.name[G.winner]);
                set_banner(b, 70);
                G.phase = PH_RESULT; G.timer = 0;
            }
            break;
        case PH_RESULT:
            if (++G.timer >= 70) {
                int played = G.round, tie = (G.score[0] == G.score[1]);
                if (played >= G.rounds && !tie) { G.state = ST_OVER; G.timer = 0; snd_play(G.human[G.winner] || C.mode == 2 ? SND_WIN : SND_LOSE); break; }
                if (played >= G.rounds && tie) G.rounds++;
                G.round++;
                G.turn = 1 - G.turn;      /* throws keep alternating across rounds */
                new_round();
            }
            break;
    }
}

/* ---------------------------------------------------------------- screens */
/* The body a Sky setting shows where no round has picked one: Random shows the first sun, Auto the theme's own. */
static int sky_shown(int style) { return qb() ? SKY_QB : (style == NSKY ? 0 : (style > NSKY ? theme_default_sky(C.theme) : style)); }

/* The original's border of asterisks round a text screen, steady: nothing in the themes flashes or animates. */
static void border(void) {
    int i;
    const uint16_t c = RGB(255, 255, 85);
    for (i = 0; i < 80; i += 2) { gfx_text_xy(i * 8, 0, 1, 2, c, "*"); gfx_text_xy(i * 8, SCR_H - 16, 1, 2, c, "*"); }
    for (i = 2; i < 29; i += 2) { gfx_text_xy(0, i * 16, 1, 2, c, "*"); gfx_text_xy(SCR_W - 8, i * 16, 1, 2, c, "*"); }
}

static void draw_title_scene(int t) {
    int p;
    if (qb()) {                       /* a text screen: black, the steady border, a gorilla each side of the heading */
        gfx_target(fb, SCR_W, SCR_H);
        gfx_rect(0, 0, SCR_W, SCR_H, RGB(0, 0, 0));
        border();
        for (p = 0; p < 2; p++) spr_blit(spr_gor[LOOK_QB][p == 0 ? POSE_RIGHT_UP : POSE_LEFT_UP], p == 0 ? 28 : SCR_W - 28 - GW, 22);
        return;
    }
    memcpy(fb, W.pic, (size_t)SCR_W * SCR_H * sizeof(uint16_t));
    gfx_target(fb, SCR_W, SCR_H);
    spr_blit(spr_sky[sky_shown(C.sky)][0], SCR_W - 84, 4);
    for (p = 0; p < 2; p++) spr_blit(spr_gor[p == 0 ? C.look[0] : resolve_look2()][p == 0 ? POSE_RIGHT_UP : POSE_LEFT_UP], W.gx[p], W.gy[p]);      /* still: they face each other, arms up */
}

/* The title screen's own city (its sky shows the Sky setting; a new one each time Sky changes). */
static void title_city(void) {
    int p, i;
    W.night = sky_is_night(sky_shown(C.sky));
    W.theme = C.theme;
    snd_use_speaker(qb());
    gfx_shadows(!qb());
    world_new_city();
    W.wind = 0;
    for (p = 0; p < 2; p++) {        /* the gorillas stand left and right of the menu panel */
        int cx = p == 0 ? 72 : SCR_W - 72;
        for (i = 0; i < W.nb; i++)
            if (cx >= W.b[i].x && cx < W.b[i].x + W.b[i].w) { W.gx[p] = cx - GW / 2; W.gy[p] = W.b[i].top - (GH - 1); }
    }
}

enum { PG_MAIN, PG_CONFIG, PG_AV };
enum { R_CONFIG, R_AV, R_PLAY, R_QUICK, R_HELP, R_EXIT,                       /* main page */
       R_MODE, R_OPP, R_ROUNDS, R_GRAV, R_WIND,                     /* Configuration */
       R_INPUT, R_GUIDE, R_TRAIL, R_N1, R_N2, R_G1, R_G2, R_THEME, R_SKY, R_VOL,    /* Audio/Visual */
       R_BACK, R_COUNT };

static const int k_page_rows[3][14] = {
    { R_CONFIG, R_AV, R_PLAY, R_QUICK, R_HELP, R_EXIT, -1 },
    { R_MODE, R_OPP, R_ROUNDS, R_GRAV, R_WIND, R_BACK, -1 },
    { R_INPUT, R_GUIDE, R_TRAIL, R_N1, R_N2, R_G1, R_G2, R_THEME, R_SKY, R_VOL, R_BACK, -1 },
};
static const char* const k_page_title[3] = { "", "Configuration", "Audio/Visual" };

static int row_visible(int r) {
    if (r == R_OPP) return C.mode != 1;
    if (r == R_N1) return C.mode != 2;
    if (r == R_N2) return C.mode == 1;
    if (r == R_G1 || r == R_G2 || r == R_SKY) return !qb();      /* the purist theme has its own gorillas and sun */
    return 1;
}

static const char* row_label(int r) {
    switch (r) {
        case R_CONFIG: return "Configuration";
        case R_AV: return "Audio/Visual";
        case R_PLAY: return "Play!";
        case R_QUICK: return "Quick Game";
        case R_HELP: return "How To Play";
        case R_EXIT: return "Exit Game";
        case R_MODE: return "Mode";
        case R_OPP: return "Opponent";
        case R_ROUNDS: return "Rounds";
        case R_GRAV: return "Gravity";
        case R_WIND: return "Wind";
        case R_INPUT: return "Input";
        case R_GUIDE: return "Aim Guide";
        case R_TRAIL: return "Last-Shot Trail";
        case R_N1: return "Player 1 Name";
        case R_N2: return "Player 2 Name";
        case R_G1: return "Gorilla 1";
        case R_G2: return "Gorilla 2";
        case R_THEME: return "Theme";
        case R_SKY: return "Sky";
        case R_VOL: return "Volume";
        default: return "Back";
    }
}

static int row_has_value(int r) { return r >= R_MODE && r != R_BACK; }

static void menu_value(int r, char* out, size_t n) {
    switch (r) {
        case R_MODE: snprintf(out, n, "%s", k_mode[C.mode]); break;
        case R_OPP: snprintf(out, n, "%s", bot_name(C.diff)); break;
        case R_ROUNDS: snprintf(out, n, "%d", C.rounds); break;
        case R_GRAV: snprintf(out, n, "%s %.1f", k_grav[C.grav].name, k_grav[C.grav].g); break;
        case R_WIND: snprintf(out, n, "%s", k_wind[C.windmode]); break;
        case R_INPUT: snprintf(out, n, "%s", k_input[C.input]); break;
        case R_GUIDE: snprintf(out, n, "%s", C.guide ? "On" : "Off"); break;
        case R_TRAIL: snprintf(out, n, "%s", C.trail ? "On" : "Off"); break;
        case R_N1: snprintf(out, n, "%s", C.name[0]); break;
        case R_N2: snprintf(out, n, "%s", C.name[1]); break;
        case R_G1: snprintf(out, n, "%s", look_name(C.look[0])); break;
        case R_G2: if (C.look[1] < 0) snprintf(out, n, "Auto"); else snprintf(out, n, "%s", look_name(C.look[1])); break;
        case R_THEME: snprintf(out, n, "%s", theme_name(C.theme)); break;
        case R_SKY: snprintf(out, n, "%s", sky_name(C.sky)); break;
        case R_VOL: snprintf(out, n, "%d", C.vol); break;
        default: out[0] = 0;
    }
}

static void menu_change(int r, int d) {
    switch (r) {
        case R_MODE: C.mode = (C.mode + d + 3) % 3; break;
        case R_OPP: C.diff = (C.diff + d + NLEVELS) % NLEVELS; break;
        case R_ROUNDS: C.rounds = clampi(C.rounds + d, 1, 15); break;
        case R_GRAV: C.grav = (C.grav + d + NGRAV) % NGRAV; break;
        case R_WIND: C.windmode = (C.windmode + d + 3) % 3; break;
        case R_INPUT: C.input = (C.input + d + 3) % 3; break;
        case R_GUIDE: C.guide = !C.guide; break;
        case R_TRAIL: C.trail = !C.trail; break;
        case R_G1: C.look[0] = (C.look[0] + d + NLOOKS) % NLOOKS; break;
        case R_G2: C.look[1] = (C.look[1] + 1 + d + NLOOKS + 1) % (NLOOKS + 1) - 1; break;     /* -1 (Auto), 0 .. NLOOKS-1 */
        case R_THEME: C.theme = (C.theme + d + NTHEME) % NTHEME; title_city(); break;
        case R_SKY: C.sky = (C.sky + d + NSKY + 2) % (NSKY + 2); title_city(); break;
        case R_VOL: C.vol = clampi(C.vol + d, 0, 10); snd_set_volume(C.vol); break;
        default: return;
    }
    snd_play(SND_TICK);
}

static void goto_page(int pg, int sel) {
    G.page = pg;
    G.menu_sel = sel >= 0 ? sel : k_page_rows[pg][0];
}

static void next_visible(int d) {
    const int* rows = k_page_rows[G.page];
    int n = 0, i = 0, k;
    while (rows[n] >= 0) n++;
    for (k = 0; k < n; k++) if (rows[k] == G.menu_sel) i = k;
    do { i = (i + d + n) % n; } while (!row_visible(rows[i]));
    G.menu_sel = rows[i];
}

/* Quick Game: a random opponent, rounds, gravity, wind, sky and gorilla for you, a straight match against the computer. Your own
 * settings (kept in g_saved) come back when the match is left; input method, aim guide, volume and names are preferences and stay. */
static void quick_game(void) {
    static const int rounds[3] = { 1, 3, 5 };
    char sub[96];
    g_saved = C;
    g_quick = 1;
    C.mode = 0;
    C.diff = rnd(NLEVELS) - 1;
    C.rounds = rounds[rnd(3) - 1];
    C.grav = rnd(NGRAV) - 1;
    C.windmode = rnd(3) - 1;
    C.theme = rnd(NTHEME) - 1;
    C.sky = NSKY + 1;                     /* Auto: the theme's own sun or moon */
    do { C.look[0] = rnd(NLOOKS) - 1; } while (C.look[0] == k_bot_look[C.diff]);     /* never the opponent's own look */
    C.look[1] = -1;                       /* the opponent wears its own look */
    start_match();
    snprintf(sub, sizeof sub, "Quick Game: %s\n%s gravity, %s\n%d round%s", bot_name(C.diff), k_grav[C.grav].name,
             C.windmode == 0 ? "no wind" : (C.windmode == 1 ? "classic wind" : "gusty wind"), C.rounds, C.rounds == 1 ? "" : "s");
    snprintf(G.sub, sizeof G.sub, "%s", sub);
    G.banner_t = 110;
}

static void update_title(void) {
    int r = G.menu_sel;
    if (g_rep & B_UP) { next_visible(-1); snd_play(SND_TICK); }
    if (g_rep & B_DOWN) { next_visible(1); snd_play(SND_TICK); }
    if (g_rep & B_LEFT) menu_change(r, -1);
    if (g_rep & B_RIGHT) menu_change(r, 1);
    if ((g_press & B_B) && G.page != PG_MAIN) { goto_page(PG_MAIN, G.page == PG_CONFIG ? R_CONFIG : R_AV); snd_play(SND_TICK); cfg_save(); }
    if (g_press & B_A) {
        if (r == R_PLAY) { snd_play(SND_SELECT); cfg_save(); start_match(); }
        else if (r == R_QUICK) { snd_play(SND_SELECT); quick_game(); }
        else if (r == R_CONFIG) { goto_page(PG_CONFIG, -1); snd_play(SND_SELECT); }
        else if (r == R_AV) { goto_page(PG_AV, -1); snd_play(SND_SELECT); }
        else if (r == R_BACK) { goto_page(PG_MAIN, G.page == PG_CONFIG ? R_CONFIG : R_AV); snd_play(SND_SELECT); cfg_save(); }
        else if (r == R_HELP) { G.state = ST_HELP; G.help_page = 0; snd_play(SND_SELECT); }
        else if (r == R_EXIT) { cfg_save(); g_quit_code = 0; }
        else if (r == R_N1 || r == R_N2) {
            G.name_for = r == R_N1 ? 0 : 1;
            osk_open(&G.osk, OSK_ALPHA, r == R_N1 ? "Player 1 Name" : "Player 2 Name", "Up to 10 characters", C.name[G.name_for], 10);
            G.state = ST_NAME; snd_play(SND_SELECT);
        } else menu_change(r, 1);
    }
    if (g_press & B_START) { cfg_save(); start_match(); snd_play(SND_SELECT); }
    if ((g_down & B_SELECT) && (g_down & B_START)) { cfg_save(); g_quit_code = 0; }
}

static void blit2x(const Sprite* s, int x, int y) {
    int i, j;
    for (j = 0; j < s->h; j++)
        for (i = 0; i < s->w; i++) if (s->px[j * s->w + i] != KEY) gfx_rect(x + i * 2, y + j * 2, 2, 2, s->px[j * s->w + i]);
}

/* A card beside the panel showing the gorilla the highlighted row is about to give you. */
static void draw_look_card(int side, int look, const char* caption) {
    int x = side == 0 ? 8 : SCR_W - 8 - 104, y = 150;
    gfx_rect(x, y, 104, 140, RGB(16, 22, 56));
    gfx_frame(x, y, 104, 140, COL_GOLD);
    blit2x(spr_gor[look][POSE_DOWN], x + 12, y + 8);
    gfx_text_c(x + 52, y + 112, 1, COL_WHITE, COL_BLACK, caption ? caption : look_name(look));
    if (caption) gfx_text_c(x + 52, y + 124, 1, RGB(200, 210, 255), COL_BLACK, look_name(look));
}

/* A card for the Sky row: the body on a patch of its own sky. */
static void draw_sky_card(int style) {
    int x = SCR_W - 8 - 104, y = 150, shown = sky_shown(style);
    const char* nm = sky_name(style);
    const char* sp = strchr(nm, ' ');
    gfx_rect(x, y, 104, 140, sky_is_night(shown) ? RGB(8, 12, 56) : RGB(70, 120, 210));
    gfx_frame(x, y, 104, 140, COL_GOLD);
    spr_blit(spr_sky[shown][0], x + 20, y + 14);
    if (sp) {
        char a[16];
        snprintf(a, sizeof a, "%.*s", (int)(sp - nm), nm);
        gfx_text_c(x + 52, y + 96, 1, COL_WHITE, COL_BLACK, a);
        gfx_text_c(x + 52, y + 108, 1, COL_WHITE, COL_BLACK, sp + 1);
    } else {
        gfx_text_c(x + 52, y + 96, 1, COL_WHITE, COL_BLACK, nm);
        gfx_text_c(x + 52, y + 108, 1, COL_WHITE, COL_BLACK, style == NSKY ? "each round" : "from theme");
    }
}

static const char* menu_hint(void);

static const char* menu_hint(void) {
    const char* hint = "";
        switch (G.menu_sel) {
            case R_QUICK: hint = "A random opponent, gravity, wind and theme, then straight in."; break;
            case R_CONFIG: hint = "Opponent, rounds, gravity and wind."; break;
            case R_AV: hint = "Input, aiming aids, names, gorillas and sound."; break;
            case R_OPP: hint = bot_tagline(C.diff); break;
            case R_MODE: hint = C.mode == 0 ? "You (left) against a computer gorilla." : (C.mode == 1 ? "Two people, one pad: pass it over." : "Watch two computer gorillas play."); break;
            case R_ROUNDS: hint = "Most rounds won takes the match."; break;
            case R_GRAV: hint = "Surface gravity in m/s2: more pulls bananas down faster."; break;
            case R_WIND: hint = "Classic: a new wind each round."; break;
            case R_INPUT: hint = "X opens a number keypad when allowed."; break;
            case R_TRAIL: hint = "Shows where your last throws flew."; break;
            case R_GUIDE: hint = "A dotted line shows your aim."; break;
            case R_N1: case R_N2: hint = "A opens the on-screen keyboard."; break;
            case R_THEME: hint = C.theme == TH_QBASIC ? "The original's look and feel, sun, gorillas and sounds." : (C.theme == TH_CLASSIC ? "This game's own first look. Cosmetic only." : "How the city looks. Cosmetic only: changes nothing in play."); break;
            case R_SKY: hint = "A sun or moon; moons make it night. Auto: the theme's own."; break;
            case R_G1: hint = C.mode == 2 ? "The left gorilla. Cosmetic only." : "Your gorilla. Cosmetic only: changes nothing in play."; break;
            case R_G2: hint = C.look[1] < 0 ? (C.mode == 1 ? "Auto: Silver for player 2." : "Auto: each opponent has its own look.") : "Cosmetic only: changes nothing in play."; break;
            default: break;
        }
    return hint;
}

/* The Q-BASIC theme's menu: plain text on a text screen, the way the original asked its questions. */
static void draw_title_qb(void) {
    const int* rows = k_page_rows[G.page];
    int i, y, sp;
    char val[40], line[64];
    const uint16_t dim = RGB(170, 170, 170), hi = RGB(255, 255, 255);
    gfx_text_xy((SCR_W - 29 * 16) / 2, 36, 2, 2, hi, "Q B A S I C   G O R I L L A S");
    gfx_text_xy((SCR_W - 40 * 8) / 2, 80, 1, 2, dim, "A new game in the spirit of the original");
    if (G.page == PG_MAIN) { sp = 44; y = 150; }
    else { sp = 30; y = 136; gfx_text_xy((SCR_W - (int)strlen(k_page_title[G.page]) * 16) / 2, 108, 2, 2, RGB(255, 255, 85), k_page_title[G.page]); }
    for (i = 0; rows[i] >= 0; i++) {
        int r = rows[i], sel = (r == G.menu_sel);
        if (!row_visible(r)) continue;
        if (!row_has_value(r)) {
            const char* l = (r == R_PLAY && sel) ? "> Play! <" : row_label(r);
            gfx_text_xy((SCR_W - (int)strlen(l) * 16) / 2, y, 2, 2, sel ? hi : dim, l);
        } else {
            int arrows = (r != R_N1 && r != R_N2);
            menu_value(r, val, sizeof val);
            gfx_text_xy(110, y, 2, 2, sel ? hi : dim, row_label(r));
            snprintf(line, sizeof line, (sel && arrows) ? "< %s >" : "%s", val);
            gfx_text_xy(SCR_W - 110 - (int)strlen(line) * 16, y, 2, 2, sel ? hi : dim, line);
        }
        y += sp;
    }
    gfx_text_xy((SCR_W - (int)strlen(menu_hint()) * 8) / 2, 412, 1, 2, RGB(85, 255, 255), menu_hint());
    gfx_text_xy((SCR_W - 60 * 8) / 2, 436, 1, 2, dim, G.page == PG_MAIN ? "Up/Down: choose   A: select   START: play" : "Up/Down: choose   Left/Right: change   A: select   B: back");
}

/* A card for the Theme row: a small copy of the city just built for the title screen. */
static void draw_theme_card(void) {
    int x = SCR_W - 8 - 104, y = 150, i, j;
    for (j = 0; j < 116; j++)
        for (i = 0; i < 104; i++) fb[(y + 2 + j) * SCR_W + x + i] = W.pic[(214 + j * 2) * SCR_W + 216 + i * 2];
    gfx_rect(x, y, 104, 2, RGB(20, 24, 56)); gfx_rect(x, y + 118, 104, 22, RGB(20, 24, 56));
    gfx_frame(x, y, 104, 140, COL_GOLD);
    gfx_text_c(x + 52, y + 124, 1, COL_WHITE, COL_BLACK, theme_name(C.theme));
}

static void draw_title(int t) {
    const int* rows = k_page_rows[G.page];
    int i, y, sp, px = 114, pw = 412, py = 94, ph;
    char val[40], line[64];
    draw_title_scene(t);
    if (qb()) { draw_title_qb(); return; }
    gfx_text_c(SCR_W / 2, 14, 5, COL_GOLD, RGB(120, 40, 0), "GORILLAS");
    gfx_text_c(SCR_W / 2, 62, 1, RGB(255, 255, 255), COL_BLACK, "An SDL homage to the QBasic classic");
    if (G.page == PG_MAIN) { sp = 38; y = 120; ph = 6 * sp + 36; }
    else { int n = 0; sp = 26; y = 138; for (i = 0; rows[i] >= 0; i++) if (row_visible(rows[i])) n++; ph = 26 * n + 48; }
    gfx_dim(px, py, pw, ph); gfx_dim(px, py, pw, ph);               /* a busy city behind it: dark enough to read */
    gfx_frame(px, py, pw, ph, RGB(120, 130, 190));
    if (G.page != PG_MAIN) gfx_text_c(SCR_W / 2, py + 14, 2, COL_GOLD, COL_BLACK, k_page_title[G.page]);
    for (i = 0; rows[i] >= 0; i++) {
        int r = rows[i], sel = (r == G.menu_sel);
        if (!row_visible(r)) continue;
        if (!row_has_value(r)) {   /* a button: centred text; Play keeps its "> Play! <" look */
            gfx_text_c(SCR_W / 2, y, 2, sel ? COL_GOLD : COL_WHITE, COL_BLACK, (r == R_PLAY && sel) ? "> Play! <" : row_label(r));
        } else {
            int arrows = (r != R_N1 && r != R_N2);
            menu_value(r, val, sizeof val);
            gfx_text_sh(px + 16, y, 2, sel ? COL_GOLD : RGB(190, 200, 230), COL_BLACK, row_label(r));
            snprintf(line, sizeof line, (sel && arrows) ? "< %s >" : "%s", val);
            gfx_text_sh(px + pw - 16 - gfx_text_w(line, 2), y, 2, sel ? COL_GOLD : COL_WHITE, COL_BLACK, line);
        }
        y += sp;
    }
    if (G.menu_sel == R_G1) draw_look_card(0, C.look[0], NULL);
    if (G.menu_sel == R_G2) draw_look_card(1, resolve_look2(), C.look[1] < 0 ? "Auto" : NULL);
    if (G.menu_sel == R_OPP) draw_look_card(1, resolve_look2(), bot_name(C.diff));
    if (G.menu_sel == R_SKY) draw_sky_card(C.sky);
    if (G.menu_sel == R_THEME) draw_theme_card();
    gfx_text_c(SCR_W / 2, 436, 1, RGB(200, 210, 255), COL_BLACK, menu_hint());
    gfx_text_c(SCR_W / 2, 454, 1, RGB(160, 170, 210), COL_BLACK,
               G.page == PG_MAIN ? "Up/Down: Choose   A: Select   START: Play" : "Up/Down: Choose   Left/Right: Change   A: Select   B: Back");
}

static const char* const k_help1[] = {
    "Hit the other gorilla with an", "exploding banana. Set the Angle and", "the Velocity (power) of your throw,",
    "and allow for the Wind (the arrow", "at the bottom), the Gravity and", "the skyline. Bananas blow holes in",
    "buildings.", "", "A hit scores a point. A throw that", "lands on your own gorilla scores", "for your opponent.",
    "", "The sun looks shocked when a", "banana flies through it.", NULL };
static const char* const k_help2[] = {
    "Up / Down      Angle", "Left / Right   Velocity", "(hold a direction to go faster)", "A              Throw", "X              Number keypad",
    "B              Aim guide on / off", "START          Pause", "SELECT+START  Back to the title", "", "On the keypad:",
    "D-pad moves, A types, B deletes,", "START accepts, SELECT cancels.", NULL };

static void draw_help(void) {
    const char* const* t = G.help_page == 0 ? k_help1 : k_help2;
    int i, y = 66;
    draw_title_scene(0);
    gfx_dim(0, 0, SCR_W, SCR_H);
    gfx_rect(16, 16, 608, 448, PANEL_COL);
    gfx_frame(16, 16, 608, 448, FRAME_COL);
    gfx_text_c(SCR_W / 2, 34, 3, COL_WHITE, COL_BLACK, G.help_page == 0 ? "HOW TO PLAY" : "CONTROLS");
    for (i = 0; t[i]; i++, y += 24) gfx_text(40, y, 2, RGB(230, 236, 255), t[i]);
    gfx_text_c(SCR_W / 2, 430, 1, RGB(170, 180, 220), COL_BLACK, "Left/Right: page    B: back");
}

static void draw_over(void) {
    char b[64];
    memcpy(fb, W.pic, (size_t)SCR_W * SCR_H * sizeof(uint16_t));
    gfx_target(fb, SCR_W, SCR_H);
    gfx_dim(0, 0, SCR_W, SCR_H);
    gfx_rect(100, 110, 440, 250, PANEL_COL);       /* wide enough for the longest "<name> wins the match!" */
    gfx_frame(100, 110, 440, 250, FRAME_COL);
    gfx_text_c(SCR_W / 2, 126, 3, COL_GOLD, RGB(120, 40, 0), "GAME OVER");
    gfx_text_c(SCR_W / 2, 176, 2, COL_WHITE, COL_BLACK, "Score");
    snprintf(b, sizeof b, "%s", G.name[0]); gfx_text(150, 210, 2, COL_WHITE, b);
    snprintf(b, sizeof b, "%d", G.score[0]); gfx_text(490 - gfx_text_w(b, 2), 210, 2, COL_WHITE, b);
    snprintf(b, sizeof b, "%s", G.name[1]); gfx_text(150, 236, 2, COL_WHITE, b);
    snprintf(b, sizeof b, "%d", G.score[1]); gfx_text(490 - gfx_text_w(b, 2), 236, 2, COL_WHITE, b);
    snprintf(b, sizeof b, "%s wins the match!", G.name[G.score[0] > G.score[1] ? 0 : 1]);
    if (gfx_text_w(b, 2) > 420) snprintf(b, sizeof b, "%s wins!", G.name[G.score[0] > G.score[1] ? 0 : 1]);
    gfx_text_c(SCR_W / 2, 286, 2, COL_GOLD, COL_BLACK, b);
    gfx_text_c(SCR_W / 2, 330, 1, RGB(170, 180, 220), COL_BLACK, "Press A");
}

/* ---------------------------------------------------------------- main frame */
static void to_title(void) {
    if (g_quick) {        /* leaving a Quick Game: the player's own settings come back */
        int vol = C.vol, guide = C.guide;
        C = g_saved; C.vol = vol; C.guide = guide;
        g_quick = 0;
    }
    G.state = ST_TITLE;
    G.paused = 0;
    G.page = PG_MAIN; G.menu_sel = R_PLAY;
    title_city();
    snd_play(SND_TITLE);
}

/* Test hook GOR_BOXART=1: the scene for the box art (city, both gorillas, sun, a banana in flight); =2: the title band. */
static void draw_boxart(int which) {
    int i;
    if (which == 4) {      /* the Game Over panel with the longest likely names (layout check) */
        snprintf(G.name[0], sizeof G.name[0], "Player 1"); snprintf(G.name[1], sizeof G.name[1], "Silverback");
        G.score[0] = 0; G.score[1] = 3;
        draw_over();
        return;
    }
    memcpy(fb, W.pic, (size_t)SCR_W * SCR_H * sizeof(uint16_t));
    gfx_target(fb, SCR_W, SCR_H);
    if (which == 3) {      /* every sky body, calm and shocked, on a day and a night patch */
        int s, k;
        for (s = 0; s < NSKY; s++)
            for (k = 0; k < 2; k++) {
                int bx = 20 + (s % 3) * 205 + k * 100, by = 20 + (s / 3) * 230;
                gfx_rect(bx - 10, by - 10, 90, 90, sky_is_night(s) ? RGB(8, 12, 56) : RGB(70, 120, 210));
                spr_blit(spr_sky[s][k], bx, by);
            }
        return;
    }
    if (which == 2) {
        for (i = 0; i < SCR_H; i++) gfx_rect(0, i, SCR_W, 1, sky_color(0));
        gfx_text_c(SCR_W / 2, 150, 8, COL_GOLD, RGB(120, 40, 0), "GORILLAS");
        gfx_text_c(SCR_W / 2, 262, 3, COL_WHITE, COL_BLACK, "Throw. Aim. Boom.");
        return;
    }
    spr_blit(spr_sky[0][0], SCR_W - 110, 14);
    spr_blit(spr_gor[0][POSE_RIGHT_UP], W.gx[0], W.gy[0]);
    spr_blit(spr_gor[1][POSE_DOWN], W.gx[1], W.gy[1]);
    for (i = 1; i < 8; i++) {      /* the banana's path, dotted, and the banana itself */
        double tt = i / 8.0;
        int bx = W.gx[0] + 36 + (int)(tt * (W.gx[1] - W.gx[0] - 20)), by = W.gy[0] - 4 - (int)(sin(tt * 3.14159) * 170);
        if (i == 5) {   /* the banana at 3x so it reads on the small art */
            int x, y;
            for (y = 0; y < BAN_W; y++) for (x = 0; x < BAN_W; x++) if (spr_ban[1]->px[y * BAN_W + x] != KEY) gfx_rect(bx - 19 + x * 3, by - 19 + y * 3, 3, 3, spr_ban[1]->px[y * BAN_W + x]);
        } else if (i < 5) gfx_rect(bx - 2, by - 2, 4, 4, RGB(255, 236, 120));
    }
}

static void frame(void) {
    static int t;
    t++;
    input_update();
    gfx_target(fb, SCR_W, SCR_H);
    if (getenv("GOR_BOXART")) { draw_boxart(atoi(getenv("GOR_BOXART"))); return; }
    switch (G.state) {
        case ST_TITLE:
            update_title();
            draw_title(t);
            break;
        case ST_NAME:
            osk_input(&G.osk, g_rep);
            draw_title(t);
            osk_draw(&G.osk);
            if (G.osk.result == OSK_OK && G.osk.buf[0]) { strncpy(C.name[G.name_for], G.osk.buf, 10); C.name[G.name_for][10] = 0; }
            if (G.osk.result != OSK_ACTIVE) { G.state = ST_TITLE; snd_play(SND_SELECT); }
            break;
        case ST_HELP:
            if (g_press & (B_LEFT | B_RIGHT)) { G.help_page = !G.help_page; snd_play(SND_TICK); }
            if (g_press & (B_B | B_A | B_START)) { G.state = ST_TITLE; snd_play(SND_TICK); }
            draw_help();
            break;
        case ST_PLAY:
            if ((g_down & B_SELECT) && (g_down & B_START)) { to_title(); break; }
            if (C.mode == 2 && (g_press & B_B)) { to_title(); break; }
            if (G.paused) {
                if (g_rep & B_UP) { G.pause_sel = (G.pause_sel + 3) % 4; snd_play(SND_TICK); }
                if (g_rep & B_DOWN) { G.pause_sel = (G.pause_sel + 1) % 4; snd_play(SND_TICK); }
                if (G.pause_sel == 1 && (g_rep & (B_LEFT | B_RIGHT))) { C.vol = clampi(C.vol + ((g_rep & B_RIGHT) ? 1 : -1), 0, 10); snd_set_volume(C.vol); snd_play(SND_TICK); }
                if (G.pause_sel == 2 && (g_rep & (B_LEFT | B_RIGHT | B_A))) { C.guide = !C.guide; snd_play(SND_TICK); }
                if ((g_press & B_A) && G.pause_sel == 0) G.paused = 0;
                if ((g_press & B_A) && G.pause_sel == 3) { cfg_save(); to_title(); break; }
                if (g_press & (B_START | B_B)) G.paused = 0;
            } else {
                if ((g_press & B_START) && G.phase != PH_KEYPAD && C.mode != 2) { G.paused = 1; G.pause_sel = 0; }
                else update_play();
            }
            draw_play();
            if (G.paused) draw_pause();
            break;
        case ST_OVER:
            if (++G.timer > 30 && (g_press & (B_A | B_START | B_B))) { cfg_save(); to_title(); break; }
            if (C.mode == 2 && G.timer > 300) { to_title(); break; }
            draw_over();
            break;
    }
}

/* Off-device bot test: GOR_BOTTEST=<rounds> plays CPU rounds (no drawing) and prints how it went. */
/* Test hook GOR_CITYTEST=<n>: n cities from the seed with the configured theme; prints a hash of the terrain mask, the gorilla
 * spots and the wind (identical in every theme: a theme is cosmetic) and the time each city took to draw. */
static void city_test(int n) {
    int k;
    for (k = 0; k < n; k++) {
        uint32_t h = 2166136261u;
        size_t i;
        unsigned t0;
        W.theme = C.theme; W.night = sky_is_night(sky_shown(C.sky));
        t0 = plat_ticks();
        world_new_city();
        t0 = plat_ticks() - t0;
        W.wind = roll_wind();
        for (i = 0; i < (size_t)SCR_W * SCR_H; i++) h = (h ^ W.solid[i]) * 16777619u;
        h = (h ^ (uint32_t)W.gx[0]) * 16777619u; h = (h ^ (uint32_t)W.gy[0]) * 16777619u;
        h = (h ^ (uint32_t)W.gx[1]) * 16777619u; h = (h ^ (uint32_t)W.gy[1]) * 16777619u;
        h = (h ^ (uint32_t)(W.wind + 100)) * 16777619u;
        printf("city %d: %08x (%u ms)\n", k, h, t0);
    }
}

static void bot_test(int rounds) {
    int r, wins[2] = { 0, 0 }, hits_first[2] = { 0, 0 }, totthrows = 0, timeouts = 0;
    int d0 = getenv("GOR_DIFF1") ? atoi(getenv("GOR_DIFF1")) : C.diff, d1 = C.diff;
    Bot bot[2];
    W.grav = k_grav[C.grav].g;
    memset(bot, 0, sizeof bot);
    for (r = 0; r < rounds; r++) {
        int turn = r & 1, n = 0, done = 0;
        world_new_city();
        W.wind = roll_wind();
        bot_new_round(&bot[0]); bot_new_round(&bot[1]);
        while (!done && n < 30) {
            int lvl = turn == 0 ? d0 : d1, res;
            Shot s;
            bot_begin(&bot[turn], turn, lvl, W.wind, W.grav);
            while (!bot_work(&bot[turn], 100000)) {}
            shot_start(&s, turn, bot[turn].angle, bot[turn].vel);
            do { res = shot_step(&s, W.wind, W.grav); } while (res == SH_FLYING);
            n++;
            if (getenv("GOR_BOTDEBUG")) printf("  r%d t%d P%d wind %d: hits %d -> a%d v%d res %d end(%.0f,%.0f) target(%d,%d) t=%.1f\n", r, n, turn, W.wind, bot[turn].nhits, bot[turn].angle, bot[turn].vel, res, s.x, s.y, W.gx[1 - turn] + 20, W.gy[1 - turn] + 24, s.t);
            if (res == SH_GORILLA) {
                int winner = s.victim == turn ? 1 - turn : turn;
                wins[winner]++;
                if (n <= 2) hits_first[winner]++;
                done = 1;
            } else {
                if (res == SH_BUILDING) world_carve((int)s.x + 3, (int)s.y + 3, 10, 13);
                bot_note_miss(&bot[turn]);
                turn = 1 - turn;
                if (C.windmode == 2) W.wind = roll_wind();
            }
        }
        if (!done) timeouts++;
        totthrows += n;
    }
    printf("bot test: %s (left) vs %s (right), %d rounds, gravity %s, wind mode %d\n", bot_name(d0), bot_name(d1), rounds, k_grav[C.grav].name, C.windmode);
    printf("  wins: left %d, right %d, no result in 30 throws: %d; average throws per round %.2f\n", wins[0], wins[1], timeouts, (double)totthrows / rounds);
}

int main(int argc, char** argv) {
    uint32_t seed;
    (void)argc; (void)argv;
    cfg_load();
    seed = getenv("GOR_SEED") ? (uint32_t)atoi(getenv("GOR_SEED")) : (uint32_t)(plat_ticks() * 2654435761u + 12345u);
    rng_seed(seed);
    art_init();
    world_init();
    if (getenv("GOR_CITYTEST")) { city_test(atoi(getenv("GOR_CITYTEST"))); return 0; }
    if (getenv("GOR_BOTTEST")) { bot_test(atoi(getenv("GOR_BOTTEST"))); return 0; }
    if (plat_init() != 0) return 1;
    fb = plat_fb();
    plat_set_fps(C.fps);
    snd_init();
    snd_set_volume(C.vol);
    if (getenv("GOR_WAVS")) { snd_dump_wavs(getenv("GOR_WAVS")); }
    to_title();
    if (getenv("GOR_AUTOSTART")) start_match();
    if (getenv("GOR_QUICK")) quick_game();
    for (;;) {
        frame();
        plat_present();
        if (g_quit_code >= 0) break;
    }
    plat_quit(g_quit_code);
}
