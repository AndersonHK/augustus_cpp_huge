#pragma once

#include "SDL_platform.h"

#ifdef __IPHONEOS__

const char *ios_show_c3_path_dialog(int again);
void c3_path_chosen(char *new_path);
const char *ios_get_base_path(void);

#endif // __IPHONEOS__
