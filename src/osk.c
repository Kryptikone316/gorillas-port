#include "osk.h"
#include "gfx.h"
#include "plat.h"
#include "sound.h"
#include <string.h>
#include <ctype.h>
#include <stdio.h>

/* Rows of keys. '<' is Delete, '!' is OK; a row made only of those is a row of wide keys. */
static const char* const k_num[] = { "789", "456", "123", "0.<", "!" };
static const char* const k_alpha[] = { "ABCDEFGHIJ", "KLMNOPQRST", "UVWXYZ0123", "456789-_. ", "<!" };

static const char* const* rows(const Osk* o, int* n) {
    if (o->kind == OSK_NUM) { *n = 5; return k_num; }
    *n = 5; return k_alpha;
}

static int is_special_row(const char* r) {
    for (; *r; r++) if (*r != '<' && *r != '!') return 0;
    return 1;
}

void osk_open(Osk* o, int kind, const char* title, const char* hint, const char* initial, int maxlen) {
    memset(o, 0, sizeof *o);
    o->kind = kind;
    strncpy(o->title, title, sizeof o->title - 1);
    strncpy(o->hint, hint ? hint : "", sizeof o->hint - 1);
    o->maxlen = maxlen > 20 ? 20 : maxlen;
    if (initial) { strncpy(o->buf, initial, (size_t)o->maxlen); o->len = (int)strlen(o->buf); }
    if (kind == OSK_NUM) { o->cx = 0; o->cy = 3; }       /* starts on the 0 key */
    o->result = OSK_ACTIVE;
}

static void clamp_cursor(Osk* o) {
    int n, w;
    const char* const* r = rows(o, &n);
    w = (int)strlen(r[o->cy]);
    if (o->cx >= w) o->cx = w - 1;
    if (o->cx < 0) o->cx = 0;
}

static void type_char(Osk* o, char ch) {
    if (o->kind == OSK_NUM && ch == '.' && strchr(o->buf, '.')) { snd_play(SND_BLIP); return; }
    if (o->len >= o->maxlen) { snd_play(SND_BLIP); return; }
    o->buf[o->len++] = ch;
    o->buf[o->len] = 0;
    snd_play(SND_BLIP);
}

void osk_input(Osk* o, unsigned press) {
    int n;
    const char* const* r = rows(o, &n);
    if (o->result != OSK_ACTIVE) return;
    if (press & B_LEFT) { int w = (int)strlen(r[o->cy]); o->cx = (o->cx + w - 1) % w; }
    if (press & B_RIGHT) { int w = (int)strlen(r[o->cy]); o->cx = (o->cx + 1) % w; }
    if (press & B_UP) { o->cy = (o->cy + n - 1) % n; clamp_cursor(o); }
    if (press & B_DOWN) { o->cy = (o->cy + 1) % n; clamp_cursor(o); }
    if (press & (B_UP | B_DOWN | B_LEFT | B_RIGHT)) snd_play(SND_TICK);
    if ((press & B_X) && o->kind == OSK_ALPHA) o->shift = !o->shift;
    if (press & B_A) {
        char k = r[o->cy][o->cx];
        if (k == '!') o->result = OSK_OK;
        else if (k == '<') { if (o->len > 0) o->buf[--o->len] = 0; snd_play(SND_BLIP); }
        else {
            if (o->shift && isalpha((unsigned char)k)) k = (char)tolower((unsigned char)k);
            type_char(o, k);
        }
    }
    if ((press & B_B) && o->len > 0) { o->buf[--o->len] = 0; snd_play(SND_BLIP); }
    if (press & B_START) o->result = OSK_OK;
    if (press & B_SELECT) o->result = OSK_CANCEL;
}

void osk_draw(const Osk* o) {
    int n, y, x, pw, ph, px, py, cw, ch, gap = 4, top;
    char shown[32];
    const char* const* r = rows(o, &n);
    cw = o->kind == OSK_NUM ? 64 : 36;
    ch = o->kind == OSK_NUM ? 40 : 34;
    pw = (o->kind == OSK_NUM ? 3 : 10) * (cw + gap) + gap + 24;
    if (pw < 350) pw = 350;
    ph = 118 + n * (ch + gap) + 40;
    px = (SCR_W - pw) / 2;
    py = (SCR_H - ph) / 2;
    gfx_dim(0, 0, SCR_W, SCR_H);
    gfx_rect(px, py, pw, ph, RGB(16, 22, 56));
    gfx_frame(px, py, pw, ph, RGB(255, 214, 60));
    gfx_frame(px + 1, py + 1, pw - 2, ph - 2, RGB(255, 214, 60));
    gfx_text_c(SCR_W / 2, py + 12, 2, RGB(255, 255, 255), RGB(0, 0, 0), o->title);
    if (o->hint[0]) gfx_text_c(SCR_W / 2, py + 36, 1, RGB(160, 190, 255), RGB(0, 0, 0), o->hint);
    /* the typed text in a field with a blinking cursor */
    gfx_rect(px + 16, py + 54, pw - 32, 36, RGB(0, 0, 0));
    gfx_frame(px + 16, py + 54, pw - 32, 36, RGB(120, 130, 170));
    snprintf(shown, sizeof shown, "%s%s", o->buf, ((plat_frame() / 20) & 1) ? "_" : " ");
    gfx_text(px + 26, py + 62, 3, RGB(255, 236, 120), shown);
    top = py + 104;
    for (y = 0; y < n; y++) {
        int w = (int)strlen(r[y]), special = is_special_row(r[y]);
        int cols = o->kind == OSK_NUM ? 3 : 10;
        int kw = special ? (cols * (cw + gap) - gap) / w - (w > 1 ? gap / 2 : 0) : cw;
        int rowx = px + (pw - (cols * (cw + gap) - gap)) / 2;
        for (x = 0; x < w; x++) {
            char k = r[y][x];
            char label[8];
            int kx = rowx + x * (kw + gap), ky = top + y * (ch + gap);
            int sel = (x == o->cx && y == o->cy);
            if (k == '<') strcpy(label, "Del");
            else if (k == '!') strcpy(label, "OK");
            else { label[0] = (o->shift && isalpha((unsigned char)k)) ? (char)tolower((unsigned char)k) : k; label[1] = 0; if (k == ' ') strcpy(label, "_"); }
            gfx_rect(kx, ky, kw, ch, sel ? RGB(255, 214, 60) : RGB(44, 54, 112));
            gfx_frame(kx, ky, kw, ch, sel ? RGB(255, 255, 255) : RGB(90, 104, 170));
            gfx_text(kx + (kw - gfx_text_w(label, 2)) / 2, ky + (ch - 16) / 2, 2, sel ? RGB(20, 20, 40) : RGB(255, 255, 255), label);
        }
    }
    gfx_text_c(SCR_W / 2, py + ph - 30, 1, RGB(190, 200, 230), RGB(0, 0, 0),
               o->kind == OSK_ALPHA ? "A=Type  B=Delete  X=Case  START=OK  SELECT=Cancel" : "A=Type  B=Delete  START=OK  SELECT=Cancel");
}
