extern "C" {
#include "file.h"

#include "core/file.h"
}

#include "miniz/miniz.h"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <string>

#define CAMPAIGNS_PREFIX_SIZE sizeof(CAMPAIGNS_DIRECTORY)

static struct {
    int is_folder;
    char file_name[FILE_NAME_MAX];
    int file_name_offset;
    struct {
        FILE *stream;
        mz_zip_archive archive;
        int is_open;
    } zip;
} data;

static std::string normalized_zip_entry_name(const char *filename)
{
    std::string entry_name = filename ? filename : "";
    for (char &ch : entry_name) {
        if (ch == '\\') {
            ch = '/';
        }
    }
    return entry_name;
}

int campaign_file_exists(const char *filename)
{
    if (data.is_folder) {
        snprintf(&data.file_name[data.file_name_offset], FILE_NAME_MAX - data.file_name_offset, "/%s", filename);
        return dir_get_file_at_location(data.file_name, PATH_LOCATION_CAMPAIGN) != 0;
    }
    int close_at_end = !data.zip.is_open;
    if (!campaign_file_open_zip()) {
        return 0;
    }
    std::string entry_name = normalized_zip_entry_name(filename);
    int has_file = mz_zip_reader_locate_file(&data.zip.archive, entry_name.c_str(), nullptr, 0) >= 0;
    if (close_at_end) {
        campaign_file_close_zip();
    }
    return has_file;
}

static void *load_file_from_folder(const char *file, size_t *length)
{
    *length = 0;
    snprintf(&data.file_name[data.file_name_offset], FILE_NAME_MAX - data.file_name_offset, "/%s", file);
    const char *filename = dir_get_file_at_location(data.file_name, PATH_LOCATION_CAMPAIGN);
    if (!filename) {
        return 0;
    }
    FILE *campaign_file = file_open(filename, "rb");
    if (!campaign_file) {
        return 0;
    }
    fseek(campaign_file, 0, SEEK_END);
    *length = ftell(campaign_file);
    fseek(campaign_file, 0, SEEK_SET);
    uint8_t *buffer = static_cast<uint8_t *>(malloc(sizeof(char) * *length));
    if (!buffer) {
        file_close(campaign_file);
        return 0;
    }
    size_t result = fread(buffer, 1, *length, campaign_file);
    file_close(campaign_file);
    if (result != *length) {
        free(buffer);
        return 0;
    }
    return buffer;
}

static void *load_file_from_zip(const char *file, size_t *length)
{
    *length = 0;
    int close_at_end = !data.zip.is_open;
    if (!campaign_file_open_zip()) {
        return 0;
    }

    std::string entry_name = normalized_zip_entry_name(file);
    int file_index = mz_zip_reader_locate_file(&data.zip.archive, entry_name.c_str(), nullptr, 0);
    if (file_index < 0) {
        if (close_at_end) {
            campaign_file_close_zip();
        }
        return 0;
    }

    mz_zip_archive_file_stat file_stat = {};
    if (!mz_zip_reader_file_stat(&data.zip.archive, static_cast<mz_uint>(file_index), &file_stat) ||
        file_stat.m_is_directory ||
        file_stat.m_uncomp_size > static_cast<mz_uint64>(std::numeric_limits<size_t>::max())) {
        if (close_at_end) {
            campaign_file_close_zip();
        }
        return 0;
    }

    *length = static_cast<size_t>(file_stat.m_uncomp_size);
    uint8_t *buffer = static_cast<uint8_t *>(malloc(*length ? *length : 1));
    if (!buffer) {
        *length = 0;
        if (close_at_end) {
            campaign_file_close_zip();
        }
        return 0;
    }

    int result = mz_zip_reader_extract_to_mem(&data.zip.archive, static_cast<mz_uint>(file_index), buffer, *length, 0);
    if (close_at_end) {
        campaign_file_close_zip();
    }

    if (!result) {
        *length = 0;
        free(buffer);
        return 0;
    }
    return buffer;
}

void *campaign_file_load(const char *file, size_t *length)
{
    return data.is_folder ? load_file_from_folder(file, length) : load_file_from_zip(file, length);
}

void campaign_file_set_path(const char *path)
{
    campaign_file_close_zip();
    if (path && path[0]) {
        data.is_folder = !file_has_extension(path, "campaign");
        data.file_name_offset = snprintf(data.file_name, FILE_NAME_MAX, "%s", path);
    } else {
        data.file_name[0] = 0;
        data.file_name_offset = 0;
        data.is_folder = 0;
    }
}

const char *campaign_file_remove_prefix(const char *path)
{
    if (!data.file_name[0]) {
        return 0;
    }
    if (strncmp(path, CAMPAIGNS_DIRECTORY "/", CAMPAIGNS_PREFIX_SIZE) != 0) {
        return 0;
    }
    path += CAMPAIGNS_PREFIX_SIZE;

    return path;
}

int campaign_file_is_zip(void)
{
    return !data.is_folder;
}

int campaign_file_open_zip(void)
{
    if (data.is_folder) {
        return 1;
    }
    if (!*data.file_name) {
        return 0;
    }
    if (!data.zip.stream) {
        const char *filename = dir_get_file_at_location(data.file_name, PATH_LOCATION_CAMPAIGN);
        if (!filename) {
            return 0;
        }
        data.zip.stream = file_open(filename, "rb");
        if (!data.zip.stream) {
            return 0;
        }
    }
    if (!data.zip.is_open) {
        std::memset(&data.zip.archive, 0, sizeof(data.zip.archive));
        if (!mz_zip_reader_init_cfile(&data.zip.archive, data.zip.stream, 0, 0)) {
            campaign_file_close_zip();
            return 0;
        }
        data.zip.is_open = 1;
    }
    return 1;
}

void campaign_file_close_zip(void)
{
    if (data.zip.is_open) {
        mz_zip_reader_end(&data.zip.archive);
        std::memset(&data.zip.archive, 0, sizeof(data.zip.archive));
        data.zip.is_open = 0;
    }
    if (data.zip.stream) {
        file_close(data.zip.stream);
        data.zip.stream = 0;
    }
}
