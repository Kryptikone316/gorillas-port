#include "sound.h"
#include "plat.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define RATE 48000

static short* g_pcm[SND_COUNT];
static short* g_spk[SND_COUNT];          /* the PC-speaker set */
static int g_spk_frames[SND_COUNT], g_use_spk;
static int g_frames[SND_COUNT];
static int g_vol = 7;

enum { W_SQUARE, W_TRI, W_SINE, W_NOISE, W_PULSE };

typedef struct { float* v; int n; } Buf;

static Buf buf_new(float secs) {
    Buf b;
    b.n = (int)(secs * RATE);
    b.v = (float*)calloc((size_t)b.n, sizeof(float));
    return b;
}

static unsigned g_noise = 12345u;
static float noise(void) {
    g_noise = g_noise * 1664525u + 1013904223u;
    return ((g_noise >> 8) & 0xFFFF) / 32768.0f - 1.0f;
}

/* One tone from t0 for dur seconds, gliding f0 -> f1, with a short attack and an exponential-ish release. */
static void tone(Buf* b, float t0, float dur, float f0, float f1, int wave, float vol) {
    int s0 = (int)(t0 * RATE), n = (int)(dur * RATE), i;
    double ph = 0;
    float lp = 0;
    for (i = 0; i < n && s0 + i < b->n; i++) {
        float k = (float)i / n, f = f0 + (f1 - f0) * k, v;
        float env = (i < 200 ? i / 200.0f : 1.0f) * (1.0f - k) * (1.0f - k * 0.3f);
        ph += f / RATE;
        ph -= floor(ph);
        switch (wave) {
            case W_SQUARE: v = ph < 0.5 ? 1.0f : -1.0f; break;
            case W_PULSE: v = ph < 0.25 ? 1.0f : -1.0f; break;
            case W_TRI: v = (float)(ph < 0.5 ? 4 * ph - 1 : 3 - 4 * ph); break;
            case W_SINE: v = (float)sin(ph * 6.2831853); break;
            default: lp += (noise() - lp) * (f / RATE * 6.0f > 1 ? 1 : f / RATE * 6.0f); v = lp * 3.0f; break;   /* f = noise cut-off */
        }
        b->v[s0 + i] += v * env * vol;
    }
}

/* Noise whose cut-off sweeps f0 -> f1 (a whoosh, a rumble). */
static void rumble(Buf* b, float t0, float dur, float f0, float f1, float vol) { tone(b, t0, dur, f0, f1, W_NOISE, vol); }

/* Every sound is normalised to its own peak (UI ticks lower, the rest high): a low digital level costs quality on the device. */
static void finish(int id, Buf* b) {
    int i;
    float m = 1e-6f, gain, target = (id == SND_TICK || id == SND_BLIP) ? 0.45f : 0.85f;
    short* out = (short*)malloc((size_t)b->n * 2 * sizeof(short));
    for (i = 0; i < b->n; i++) if (fabsf(b->v[i]) > m) m = fabsf(b->v[i]);
    gain = target / m;
    for (i = 0; i < b->n; i++) {
        float x = b->v[i] * gain;
        if (x > 1) x = 1;
        if (x < -1) x = -1;
        out[i * 2] = out[i * 2 + 1] = (short)(x * 32000);
    }
    g_pcm[id] = out;
    g_frames[id] = b->n;
    free(b->v);
    plat_snd_set(id, out, b->n);
}

static float note(int semis_from_a4) { return 440.0f * powf(2.0f, semis_from_a4 / 12.0f); }

void snd_init(void) {
    Buf b;
    int i;
    /* menu tick and key blip */
    b = buf_new(0.03f); tone(&b, 0, 0.03f, 1400, 1200, W_SQUARE, 0.35f); finish(SND_TICK, &b);
    b = buf_new(0.07f); tone(&b, 0, 0.07f, 880, 880, W_PULSE, 0.5f); finish(SND_BLIP, &b);
    /* select: two quick notes up */
    b = buf_new(0.22f); tone(&b, 0, 0.09f, 660, 660, W_SQUARE, 0.45f); tone(&b, 0.09f, 0.13f, 990, 990, W_SQUARE, 0.45f); finish(SND_SELECT, &b);
    /* throw: a whoosh with a short rising chirp */
    b = buf_new(0.30f); rumble(&b, 0, 0.30f, 1800, 500, 0.5f); tone(&b, 0, 0.22f, 300, 760, W_TRI, 0.25f); finish(SND_THROW, &b);
    /* crater: noise burst with a falling thump */
    b = buf_new(0.55f); rumble(&b, 0, 0.55f, 900, 120, 1.2f); tone(&b, 0, 0.35f, 110, 38, W_SINE, 1.0f); finish(SND_BOOM, &b);
    /* gorilla hit: bigger and longer, with a falling wail */
    b = buf_new(1.30f); rumble(&b, 0, 1.30f, 1200, 80, 1.3f); tone(&b, 0, 0.9f, 140, 30, W_SINE, 1.1f);
    tone(&b, 0.05f, 0.9f, 520, 70, W_SQUARE, 0.22f); finish(SND_BIGBOOM, &b);
    /* the sun's "ooh": a wobbling sine that rises and settles */
    b = buf_new(0.45f);
    for (i = 0; i < 9; i++) tone(&b, i * 0.045f, 0.06f, 520 + i * 28 + (i & 1) * 18, 540 + i * 28, W_SINE, 0.45f);
    finish(SND_OOH, &b);
    /* title jingle: a bouncy rising figure and a closing chord (an original tune) */
    b = buf_new(2.6f);
    {
        static const int mel[] = { -9, -5, -2, 3, -2, 3, 7, 3, 7, 10, 7, 3 };
        static const float len[] = { .14f, .14f, .14f, .28f, .14f, .14f, .28f, .14f, .14f, .28f, .14f, .42f };
        float t = 0;
        for (i = 0; i < 12; i++) {
            tone(&b, t, len[i] * 1.2f, note(mel[i]), note(mel[i]), W_PULSE, 0.4f);
            tone(&b, t, len[i] * 1.2f, note(mel[i] - 12), note(mel[i] - 12), W_TRI, 0.3f);
            t += len[i];
        }
        tone(&b, t, 0.9f, note(3), note(3), W_PULSE, 0.4f); tone(&b, t, 0.9f, note(7), note(7), W_PULSE, 0.3f);
        tone(&b, t, 0.9f, note(10), note(10), W_PULSE, 0.3f); tone(&b, t, 0.9f, note(-9), note(-9), W_TRI, 0.4f);
    }
    finish(SND_TITLE, &b);
    /* victory dance: eight steps, one per arm change (0.2 s each) */
    b = buf_new(1.7f);
    {
        static const int mel[] = { 3, 7, 10, 15, 10, 7, 12, 15 };
        for (i = 0; i < 8; i++) {
            tone(&b, i * 0.2f, 0.16f, note(mel[i]), note(mel[i]), W_PULSE, 0.45f);
            tone(&b, i * 0.2f, 0.16f, note(mel[i] - 12), note(mel[i] - 12), W_TRI, 0.25f);
        }
    }
    finish(SND_DANCE, &b);
    /* match won: fanfare */
    b = buf_new(2.4f);
    {
        static const int mel[] = { 3, 3, 3, 7, 10, 7, 10, 15 };
        static const float len[] = { .15f, .15f, .15f, .3f, .3f, .15f, .3f, .9f };
        float t = 0;
        for (i = 0; i < 8; i++) {
            tone(&b, t, len[i] * 1.1f, note(mel[i]), note(mel[i]), W_SQUARE, 0.35f);
            tone(&b, t, len[i] * 1.1f, note(mel[i] - 12), note(mel[i] - 12), W_TRI, 0.3f);
            t += len[i];
        }
    }
    finish(SND_WIN, &b);
    /* lost: a sagging wah-wah */
    b = buf_new(1.2f);
    tone(&b, 0, 0.3f, note(3), note(1), W_TRI, 0.6f); tone(&b, 0.3f, 0.3f, note(0), note(-2), W_TRI, 0.6f);
    tone(&b, 0.6f, 0.6f, note(-4), note(-12), W_TRI, 0.6f);
    finish(SND_LOSE, &b);
}

/* ---- the PC-speaker set (Q-BASIC theme): one square-wave voice, no envelopes, our own little tunes ---- */
static void beep(Buf* b, float t0, float dur, float f0, float f1) {          /* a square wave gliding f0 -> f1; f0 = 0 is a rest */
    int s0 = (int)(t0 * RATE), n = (int)(dur * RATE), i;
    double ph = 0;
    if (f0 <= 0) return;
    for (i = 0; i < n && s0 + i < b->n; i++) {
        float k = (float)i / n, f = f0 + (f1 - f0) * k, edge = i < 48 ? i / 48.0f : (n - i < 48 ? (n - i) / 48.0f : 1.0f);
        ph += f / RATE; ph -= floor(ph);
        b->v[s0 + i] += (ph < 0.5 ? 1.0f : -1.0f) * edge * 0.6f;
    }
}

static void spk_finish(int id, Buf* b) {
    int i;
    short* out = (short*)malloc((size_t)b->n * 2 * sizeof(short));
    float target = (id == SND_TICK || id == SND_BLIP) ? 0.35f : 0.6f;       /* squares are loud: lower than the normal set */
    for (i = 0; i < b->n; i++) {
        float x = b->v[i] * target;
        out[i * 2] = out[i * 2 + 1] = (short)(x * 32000);
    }
    g_spk[id] = out; g_spk_frames[id] = b->n;
    free(b->v);
}

static void speaker_init(void) {
    Buf b;
    int i;
    float f;
    b = buf_new(0.02f); beep(&b, 0, 0.02f, 1000, 1000); spk_finish(SND_TICK, &b);
    b = buf_new(0.04f); beep(&b, 0, 0.04f, 1500, 1500); spk_finish(SND_BLIP, &b);
    b = buf_new(0.12f); beep(&b, 0, 0.05f, 880, 880); beep(&b, 0.06f, 0.06f, 1320, 1320); spk_finish(SND_SELECT, &b);
    b = buf_new(0.30f);                                                  /* throw: a quick rising run of stepped notes */
    for (i = 0; i < 10; i++) beep(&b, i * 0.03f, 0.028f, 330.0f * powf(1.12f, (float)i), 330.0f * powf(1.12f, (float)i));
    spk_finish(SND_THROW, &b);
    b = buf_new(0.60f);                                                  /* a hit on a building: stuttering low notes falling away */
    for (i = 0; i < 24; i++) { f = 180.0f - i * 6.0f + ((i * 7) % 5) * 9.0f; beep(&b, i * 0.025f, 0.022f, f, f); }
    spk_finish(SND_BOOM, &b);
    b = buf_new(1.40f);                                                  /* a gorilla: a long falling growl, then a sag */
    for (i = 0; i < 40; i++) { f = 220.0f - i * 4.0f + ((i * 11) % 7) * 14.0f; beep(&b, i * 0.02f, 0.018f, f, f); }
    for (i = 0; i < 6; i++) beep(&b, 0.85f + i * 0.09f, 0.08f, 140.0f - i * 16.0f, 130.0f - i * 16.0f);
    spk_finish(SND_BIGBOOM, &b);
    b = buf_new(0.30f);                                                  /* the sun is startled */
    beep(&b, 0, 0.12f, 600, 900); beep(&b, 0.14f, 0.14f, 900, 700); spk_finish(SND_OOH, &b);
    b = buf_new(2.4f);                                                   /* title: a short bouncing tune in the PC-speaker style */
    {
        static const float mel[] = { 262, 330, 392, 523, 392, 523, 659, 523, 392, 330, 392, 262 };
        static const float len[] = { .14f, .14f, .14f, .28f, .14f, .14f, .28f, .14f, .14f, .14f, .14f, .56f };
        float t = 0;
        for (i = 0; i < 12; i++) { beep(&b, t, len[i] * 0.9f, mel[i], mel[i]); t += len[i]; }
        t += 0.1f;
        beep(&b, t, 0.5f, 523, 523);
    }
    spk_finish(SND_TITLE, &b);
    b = buf_new(1.7f);                                                   /* victory dance: eight steps, one per arm change */
    {
        static const float mel[] = { 392, 494, 587, 784, 587, 494, 659, 784 };
        for (i = 0; i < 8; i++) beep(&b, i * 0.2f, 0.16f, mel[i], mel[i]);
    }
    spk_finish(SND_DANCE, &b);
    b = buf_new(2.0f);                                                   /* match won */
    {
        static const float mel[] = { 392, 392, 392, 523, 659, 523, 659, 784 };
        static const float len[] = { .15f, .15f, .15f, .3f, .3f, .15f, .3f, .7f };
        float t = 0;
        for (i = 0; i < 8; i++) { beep(&b, t, len[i] * 0.92f, mel[i], mel[i]); t += len[i]; }
    }
    spk_finish(SND_WIN, &b);
    b = buf_new(1.2f);                                                   /* lost: four falling notes */
    beep(&b, 0, 0.25f, 330, 300); beep(&b, 0.3f, 0.25f, 262, 240); beep(&b, 0.6f, 0.25f, 196, 180); beep(&b, 0.9f, 0.3f, 131, 100);
    spk_finish(SND_LOSE, &b);
}

void snd_use_speaker(int on) {
    int i;
    on = on != 0;
    if (on == g_use_spk) return;
    if (!g_spk[0]) speaker_init();
    g_use_spk = on;
    for (i = 0; i < SND_COUNT; i++) {
        if (on) plat_snd_set(i, g_spk[i], g_spk_frames[i]);
        else plat_snd_set(i, g_pcm[i], g_frames[i]);
    }
}

void snd_set_volume(int v) { g_vol = v < 0 ? 0 : (v > 10 ? 10 : v); }
int snd_get_volume(void) { return g_vol; }

void snd_play(int id) {
    float g;
    if (id < 0 || id >= SND_COUNT || g_vol <= 0 || !g_pcm[id]) return;
    g = g_vol / 10.0f;
    plat_snd_play(id, g * g);
}

static void w32(FILE* f, unsigned v) { fputc(v & 255, f); fputc((v >> 8) & 255, f); fputc((v >> 16) & 255, f); fputc((v >> 24) & 255, f); }
static void w16(FILE* f, unsigned v) { fputc(v & 255, f); fputc((v >> 8) & 255, f); }

static void dump_set(const char* dir, const char* prefix, short* const* pcm, const int* frames) {
    static const char* const names[SND_COUNT] = { "tick", "blip", "select", "throw", "boom", "bigboom", "ooh", "title", "dance", "win", "lose" };
    int i;
    for (i = 0; i < SND_COUNT; i++) {
        char path[256];
        FILE* f;
        snprintf(path, sizeof path, "%s/%s%s.wav", dir, prefix, names[i]);
        f = fopen(path, "wb");
        if (!f) continue;
        fwrite("RIFF", 1, 4, f); w32(f, 36 + (unsigned)frames[i] * 4); fwrite("WAVEfmt ", 1, 8, f);
        w32(f, 16); w16(f, 1); w16(f, 2); w32(f, RATE); w32(f, RATE * 4); w16(f, 4); w16(f, 16);
        fwrite("data", 1, 4, f); w32(f, (unsigned)frames[i] * 4);
        fwrite(pcm[i], 4, (size_t)frames[i], f);
        fclose(f);
    }
}

void snd_dump_wavs(const char* dir) {
    if (!g_spk[0]) speaker_init();
    dump_set(dir, "", g_pcm, g_frames);
    dump_set(dir, "spk_", g_spk, g_spk_frames);
}
