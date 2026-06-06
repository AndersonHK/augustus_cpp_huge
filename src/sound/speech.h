#pragma once
#ifdef __cplusplus
extern "C" {
#endif


void sound_speech_set_volume(int percentage);

void sound_speech_play_file(const char *filename);

void sound_speech_stop(void);

#ifdef __cplusplus
}
#endif
