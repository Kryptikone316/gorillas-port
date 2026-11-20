/* City themes: purely cosmetic. A theme paints the backdrop behind the buildings, the buildings themselves, the roof
 * ornaments and the street. It never touches the terrain mask or the game's random numbers, so a seed plays out
 * the same in every theme. Everything is drawn in code (own art; the concept pictures are design reference only). */
#ifndef THEME_H
#define THEME_H

#include <stdint.h>

#define NTHEME 5          /* Q-BASIC first: the purist default; Classic is the port's own original look */
enum { TH_QBASIC, TH_CLASSIC, TH_UPTOWN, TH_MOONLIT, TH_HARBOR };

const char* theme_name(int t);
int theme_default_sky(int t);                 /* the sun or moon "Auto" picks for a theme */
uint16_t theme_sky_color(int theme, int night, int y);

void theme_init(void);                        /* once: street props (sprites) */
void theme_begin(void);                       /* a new city starts */
void theme_backdrop(void);                    /* behind the buildings: into W.sky, strip included */
void theme_building(int x, int w, int top, int color);   /* into W.pic */
void theme_finish(void);                      /* roof ornaments and street props (needs W.b, W.gb) */
void theme_carve(int cx, int cy, int rx, int ry);        /* after a crater: drop ornaments that lost their roof */

#endif
