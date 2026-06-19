#pragma once

#include "core/dir.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include <string>
#include <string_view>

/**
 * @file
 * File-related functions.
 *
 * Methods related to extensions:
 * @li The extension starts from the first dot, double extensions are not supported
 * @li Extension parameters are expected to be 3 chars, without leading dot
 */

#define FILE_NAME_MAX 300

/**
 * Wrapper for fopen converting filename to path in current working directory
 * @param filename Filename
 * @param mode Mode to open the file (e.g. "wb").
 * @return FILE
 */
FILE *file_open(const char *filename, const char *mode);

/**
 * Wrapper for fopen converting filename to path in asset directory
 * @param asset Asset filename
 * @param mode Mode to open the asset file (e.g. "wb").
 * @return FILE
 */
FILE *file_open_asset(const char *asset, const char *mode);

/**
 * Wrapper to fclose
 * @return See fclose (If the stream is successfully closed, a zero value is returned.
 *         On failure, EOF is returned.)
 */
int file_close(FILE *stream);

/**
 * Checks whether the file has the given extension
 * @param filename Filename to check
 * @param extension Extension
 * @return boolean true if the file has the given extension, false otherwise
 */
int file_has_extension(const char *filename, const char *extension);

/**
 * Replaces the current extension by the given new extension.
 * Filename is unchanged if there was no extension.
 * @param[in,out] filename Filename to change
 * @param new_extension New extension
 */
void file_change_extension(char *filename, const char *new_extension);

/**
 * Appends the extension to the file
 * @param[in,out] filename Filename to change
 * @param extension Extension to append
 * @param length Length of the filename buffer
 */
void file_append_extension(char *filename, const char *extension, size_t length);

/**
 * Removes the extension from the file
 * @param[in,out] filename Filename to change
 */
void file_remove_extension(char *filename);

/**
 * Removes the path from the filename
 * @param filename Filename to change
 * @return Filename without path
 */
const char *file_remove_path(const char *filename);

/**
 * Check if file exists
 * @param filename Filename to check
 * @param localizable Whether the file may be localized (see core/dir.h)
 * @return boolean true if the file exists, false otherwise
 */
int file_exists(const char *filename, int localizable);

/**
 * Remove a file
 * @param filename Filename to remove
 * @return boolean true if the file removal was successful, false otherwise
 */
int file_remove(const char *filename);

inline FILE *file_open(std::string_view filename, std::string_view mode)
{
    const std::string filename_s(filename);
    const std::string mode_s(mode);
    return file_open(filename_s.c_str(), mode_s.c_str());
}

inline FILE *file_open_asset(std::string_view asset, std::string_view mode)
{
    const std::string asset_s(asset);
    const std::string mode_s(mode);
    return file_open_asset(asset_s.c_str(), mode_s.c_str());
}

inline bool file_has_extension(std::string_view filename, std::string_view extension)
{
    const std::string filename_s(filename);
    const std::string extension_s(extension);
    return file_has_extension(filename_s.c_str(), extension_s.c_str()) != 0;
}

inline void file_change_extension(std::string &filename, std::string_view new_extension)
{
    if (new_extension.empty()) {
        return;
    }
    const auto current_ext = filename.find_last_of('.');
    if (current_ext == std::string::npos) {
        return;
    }
    filename.resize(current_ext + 1);
    filename.append(new_extension);
}

inline void file_append_extension(std::string &filename, std::string_view extension)
{
    if (extension.empty()) {
        return;
    }
    filename.push_back('.');
    filename.append(extension);
}

inline void file_remove_extension(std::string &filename)
{
    const auto pos = filename.find_last_of('.');
    if (pos != std::string::npos) {
        filename.resize(pos);
    }
}

inline std::string_view file_remove_path(std::string_view filename)
{
    const auto slash = filename.find_last_of("/\\");
    if (slash == std::string_view::npos) {
        return filename;
    }
    return filename.substr(slash + 1);
}

inline bool file_exists(std::string_view filename, bool localizable = false)
{
    const std::string filename_s(filename);
    return file_exists(filename_s.c_str(), localizable ? 1 : 0) != 0;
}

inline bool file_remove(std::string_view filename)
{
    const std::string filename_s(filename);
    return file_remove(filename_s.c_str()) != 0;
}
