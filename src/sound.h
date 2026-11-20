/* Sounds, all synthesized at start-up (no sound files): square/triangle tones and filtered noise. */
#ifndef SOUND_H
#define SOUND_H

enum {
    SND_TICK, SND_BLIP, SND_SELECT, SND_THROW, SND_BOOM, SND_BIGBOOM, SND_OOH,
    SND_TITLE, SND_DANCE, SND_WIN, SND_LOSE, SND_COUNT
};

void snd_init(void);
void snd_play(int id);
void snd_use_speaker(int on);    /* 1: the Q-BASIC theme's PC-speaker-style sounds (square-wave beeps), 0: the normal set */
void snd_set_volume(int v);      /* 0..10 */
int snd_get_volume(void);
void snd_dump_wavs(const char* dir);   /* test hook: writes each sound as a .wav */

#endif
