#ifndef ART_H
#define ART_H
#include "gfx.h"

#define GW 40            /* gorilla sprite size */
#define GH 48

enum { POSE_DOWN = 0, POSE_LEFT_UP = 1, POSE_RIGHT_UP = 2, POSE_COUNT = 3 };

#define NLOOKS 13
#define LOOK_QB 12          /* the Q-BASIC gorilla: our own drawing in the original's flat style */
extern Sprite* spr_gor[NLOOKS][POSE_COUNT];    /* [look][pose]: cosmetic gorillas, the hit area is always look 0's */
const char* look_name(int look);
extern unsigned char gor_mask[GW * GH];    /* opaque pixels of look 0's arms-down pose (hit test, the same for every look) */
extern Sprite* spr_ban[4];                 /* banana, four rotations */
extern Sprite* spr_ban_qb[4];              /* the Q-BASIC theme's flat yellow banana */
#define NSKY 7          /* 0..2 suns, 3..5 moons (a moon makes the sky a night sky), 6 the Q-BASIC sun; NSKY itself = Random in the menu */
#define SKY_QB 6
extern Sprite* spr_sky[NSKY][2];           /* [style][0 = calm, 1 = shocked] */
const char* sky_name(int style);           /* 0..NSKY (NSKY = "Random") */
#define sky_is_night(s) ((s) >= 3 && (s) <= 5)
#define SUN_W 64
#define BAN_W 13

void art_init(void);

#endif
