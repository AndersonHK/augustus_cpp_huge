#pragma once


#define TOTAL_GAME_SPEEDS 15
int game_speed_get_index(int speed);
int game_speed_get_speed(int index);
int game_speed_get_elapsed_ticks(void);

