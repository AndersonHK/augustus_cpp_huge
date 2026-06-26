#include "core/file.h"

#include "platform/file_manager.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <string_view>

static const size_t ASSET_PATH_PREFIX_LENGTH = sizeof(ASSETS_DIRECTORY) - 1;

static const char *find_last_extension(const char *filename)
{
    const std::string_view path{filename};
    const auto dot_position = path.find_last_of('.');
    if (dot_position == std::string_view::npos) {
        return nullptr;
    }
    return filename + dot_position;
}

static const char *find_last_path_separator(const char *filename)
{
    const std::string_view path{filename};
    const auto slash_position = path.find_last_of("/\\");
    if (slash_position == std::string_view::npos) {
        return nullptr;
    }
    return filename + slash_position;
}

static const char *asset_path_start(const char *filename)
{
    const std::string_view resolved_filename{filename};
    if (resolved_filename.compare(0, ASSET_PATH_PREFIX_LENGTH, ASSETS_DIRECTORY) == 0) {
        return filename + ASSET_PATH_PREFIX_LENGTH;
    }
    return filename;
}

FILE *file_open(const char *filename, const char *mode)
{
    if (!filename) {
        return 0;
    }
    const char *resolved_filename = asset_path_start(filename);
    return resolved_filename == filename ? platform_file_manager_open_file(filename, mode)
                                        : platform_file_manager_open_asset(resolved_filename, mode);
}

FILE *file_open_asset(const char *asset, const char *mode)
{
    return platform_file_manager_open_asset(asset, mode);
}

int file_close(FILE *stream)
{
    return platform_file_manager_close_file(stream);
}

int file_has_extension(const char *filename, const char *extension)
{
    if (!extension || !*extension) {
        return 1;
    }
    const char *dot_position = find_last_extension(filename);
    return dot_position ? platform_file_manager_compare_filename(dot_position + 1, extension) == 0 : 0;
}

void file_change_extension(char *filename, const char *new_extension)
{
    if (!new_extension || !*new_extension) {
        return;
    }
    char *dot_position = (char *) find_last_extension(filename);
    if (!dot_position) {
        return;
    }
    ++dot_position;
    snprintf(dot_position, strlen(dot_position) + 1, "%s", new_extension);
}

void file_append_extension(char *filename, const char *extension, size_t length)
{
    if (!extension || !*extension) {
        return;
    }
    const size_t actual_length = strlen(filename);
    const size_t extension_length = strlen(extension);
    if (actual_length + extension_length + 1 > length) {
        return;
    }
    filename[actual_length] = '.';
    memcpy(filename + actual_length + 1, extension, extension_length + 1);
}

void file_remove_extension(char *filename)
{
    char *dot_position = (char *) find_last_extension(filename);
    if (dot_position) {
        *dot_position = 0;
    }
}

const char *file_remove_path(const char *filename)
{
    const char *filename_without_directory = find_last_path_separator(filename);
    if (filename_without_directory) {
        return filename_without_directory + 1;
    }
    return filename;
}

int file_exists(const char *filename, int localizable)
{
    return NULL != dir_get_file(filename, localizable);
}

int file_remove(const char *filename)
{
    return platform_file_manager_remove_file(filename);
}
