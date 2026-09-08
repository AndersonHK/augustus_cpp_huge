#pragma once

// Bootstrap UI uses authored files directly: extracted image and font registries
// are deliberately unavailable while this screen is preparing them.
void platform_loading_screen_begin();
void platform_loading_screen_end();
bool platform_loading_screen_cancelled();
void platform_loading_screen_validate(const char *output_directory);
