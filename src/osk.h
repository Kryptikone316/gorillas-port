/* On-screen keyboard for a pad with no keys: a numeric keypad (angle, velocity) and a small
 * alphanumeric board (names). Modelled on the Block Dude port's keyboard: D-pad moves,
 * A types the key, B deletes, START accepts, SELECT cancels; X shifts case on the letter board. */
#ifndef OSK_H
#define OSK_H

enum { OSK_NUM = 0, OSK_ALPHA = 1 };
enum { OSK_ACTIVE = 0, OSK_OK = 1, OSK_CANCEL = 2 };

typedef struct {
    int kind;
    char title[40];
    char hint[48];          /* second line: the range that is accepted */
    char buf[24];
    int len, maxlen;
    int cx, cy;
    int shift;              /* letter board: lower case */
    int result;
} Osk;

void osk_open(Osk* o, int kind, const char* title, const char* hint, const char* initial, int maxlen);
/* press = buttons newly pressed this frame (the D-pad also repeats while held). */
void osk_input(Osk* o, unsigned press);
void osk_draw(const Osk* o);

#endif
