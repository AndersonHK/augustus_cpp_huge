#pragma once

#include <filesystem>
#include <string>

int platform_sdl_version_at_least(int major, int minor, int patch);
char *platform_get_logging_path(void);
char *platform_get_pref_path(void);
void exit_with_status(int status);

namespace platform {

std::string logging_path();
std::string pref_path();
std::filesystem::path logging_path_filesystem();
std::filesystem::path pref_path_filesystem();

}

