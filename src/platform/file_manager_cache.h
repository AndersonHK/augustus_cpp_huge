#pragma once

#if defined(__vita__) || defined(__SWITCH__)
#define USE_FILE_CACHE

#include "core/file.h"

struct file_info {
    char name[FILE_NAME_MAX];
    const char *extension;
    int type;
    unsigned int modified_time;
    file_info *next;
};

struct dir_info {
    char name[FILE_NAME_MAX];
    file_info *first_file;
    dir_info *next;
};

const dir_info *platform_file_manager_cache_get_dir_info(const char *dir);
int platform_file_manager_cache_file_has_extension(const file_info *f, const char *extension);
void platform_file_manager_cache_update_file_info(const char *filename);
void platform_file_manager_cache_delete_file_info(const char *filename);
void platform_file_manager_cache_invalidate(void);

#endif
