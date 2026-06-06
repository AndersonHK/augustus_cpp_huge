#pragma once
#ifdef __cplusplus
extern "C" {
#endif


void sound_music_set_volume(int percentage);

void sound_music_play_intro(void);

void sound_music_play_editor(void);

void sound_music_update(int force);

void sound_music_pause(void);

void sound_music_resume(void);

void sound_music_stop(void);

void sound_music_next_track(void);

#ifdef __cplusplus
}
#endif
