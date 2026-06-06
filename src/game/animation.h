#pragma once
#ifdef __cplusplus
extern "C" {
#endif


void game_animation_init(void);

void game_animation_update(void);

int game_animation_should_advance(int speed);

#ifdef __cplusplus
}
#endif
