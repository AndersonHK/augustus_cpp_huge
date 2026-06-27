#include "core/time.h"

#include <cstdint>
#include <cstdlib>

uint64_t system_get_ticks(void)
{
    return time_get_millis();
}

void system_resize(int width, int height)
{
    (void) width;
    (void) height;
}

void system_center(void)
{
}

void system_set_fullscreen(int fullscreen)
{
    (void) fullscreen;
}

int system_supports_select_folder_dialog(void)
{
    return 0;
}

const char *system_show_select_folder_dialog(const char *title, const char *default_path)
{
    (void) title;
    (void) default_path;
    return nullptr;
}

void system_exit(void)
{
    std::exit(0);
}
