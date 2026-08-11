/* audio.h -- APU driver interface. */
#ifndef AUDIO_H
#define AUDIO_H

enum {
    SFX_NONE,
    SFX_SWIPE,
    SFX_JUMP,
    SFX_PICKUP,
    SFX_HURT,
    SFX_HIT,
    SFX_ENEMY_DIE,
    SFX_THROW,
    SFX_SPLAT,
    SFX_DOOR,
    SFX_SLAM,
    SFX_BOSS_HURT
};

enum {
    SONG_NONE,
    SONG_TITLE,
    SONG_CASTLE,
    SONG_BOSS,
    SONG_VICTORY,
    SONG_DEATH
};

void audio_init(void);
void audio_frame(void);         /* call once per frame */
void audio_sfx(unsigned char id);
void audio_song(unsigned char id);

#endif
