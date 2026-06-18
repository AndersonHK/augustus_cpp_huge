#include "core/dir.h"

#include "core/config.h"
#include "core/file.h"
#include "core/string.h"
#include "platform/file_manager.h"

#include <algorithm>
#include <cstring>
#include <string>
#include <vector>

struct directory_entry {
    std::string name;
    unsigned int modified_time;
};

struct dir_state {
    dir_listing listing{};
    std::vector<directory_entry> entries;
    std::vector<dir_entry> sorted_files;
    std::string current_dir;
    std::string* case_corrected_output = nullptr;
    std::string resolved_path;
    std::string appended_path;
} data{};

static void clear_dir_listing(void)
{
    data.listing.files = nullptr;
    data.listing.num_files = 0;
    data.current_dir.clear();
    data.entries.clear();
    data.sorted_files.clear();
}

static const dir_listing *finalize_dir_listing(void)
{
    std::sort(data.entries.begin(), data.entries.end(),
        [](const directory_entry &a, const directory_entry &b) {
            return platform_file_manager_compare_filename(a.name.c_str(), b.name.c_str()) < 0;
        });

    data.sorted_files.resize(data.entries.size());
    for (size_t i = 0; i < data.entries.size(); i++) {
        data.sorted_files[i].name = data.entries[i].name.c_str();
        data.sorted_files[i].modified_time = data.entries[i].modified_time;
    }
    data.listing.files = data.sorted_files.empty() ? nullptr : data.sorted_files.data();
    data.listing.num_files = static_cast<int>(data.sorted_files.size());
    return &data.listing;
}

static int add_to_listing(const char *filename, long modified_time)
{
    data.entries.push_back({filename ? filename : "", static_cast<unsigned int>(modified_time)});
    return LIST_CONTINUE;
}

static int compare_case(const char *filename, long)
{
    if (!data.case_corrected_output) {
        return LIST_NO_MATCH;
    }
    if (platform_file_manager_compare_filename(filename, data.case_corrected_output->c_str()) == 0) {
        *data.case_corrected_output = filename;
        return LIST_MATCH;
    }
    return LIST_NO_MATCH;
}

static bool correct_case(const char *dir, std::string &filename, int type)
{
    data.case_corrected_output = &filename;
    return platform_file_manager_list_directory_contents(dir, type, 0, compare_case) == LIST_MATCH;
}

static const char *get_case_corrected_file(const char *dir, const char *filepath)
{
    if (!filepath) {
        return nullptr;
    }

    const char *filepath_to_check = filepath;
    const char *current_path_storage = data.resolved_path.empty() ? nullptr : data.resolved_path.data();
    std::string filepath_backup;
    if (current_path_storage && filepath_to_check >= current_path_storage
        && filepath_to_check < current_path_storage + data.resolved_path.size() + 1) {
        filepath_backup = filepath_to_check;
        filepath_to_check = filepath_backup.c_str();
    }

    data.resolved_path = dir && *dir ? dir : ".";
    if (data.resolved_path.empty() || (data.resolved_path.back() != '/' && data.resolved_path.back() != '\\')) {
        data.resolved_path.push_back('/');
    }
    const size_t path_offset = data.resolved_path.size();
    data.resolved_path += filepath_to_check;

    FILE *fp = file_open(data.resolved_path.c_str(), "rb");
    if (fp) {
        file_close(fp);
        return data.resolved_path.c_str() + (dir && *dir ? 0 : 2);
    }

    if (!platform_file_manager_should_case_correct_file()) {
        return nullptr;
    }

    data.resolved_path[path_offset - 1] = '\0';

    size_t segment_start = path_offset;
    while (true) {
        const size_t slash = data.resolved_path.find_first_of("/\\", segment_start);
        if (slash == std::string::npos) {
            break;
        }

        data.resolved_path[slash] = '\0';
        std::string segment = data.resolved_path.substr(segment_start, slash - segment_start);
        if (!correct_case(data.resolved_path.c_str(), segment, TYPE_DIR)) {
            return nullptr;
        }
        data.resolved_path[slash] = '/';
        data.resolved_path.replace(segment_start, slash - segment_start, segment);
        segment_start = slash + 1;
        if (segment_start < data.resolved_path.size() && data.resolved_path[segment_start] == '\\') {
            data.resolved_path.erase(segment_start, 1);
        }
        data.resolved_path[segment_start - 1] = '/';
    }

    std::string segment = data.resolved_path.substr(segment_start);
    if (!correct_case(data.resolved_path.c_str(), segment, TYPE_FILE)) {
        return nullptr;
    }
    data.resolved_path.replace(segment_start, std::string::npos, segment);
    data.resolved_path[path_offset - 1] = '/';

    return data.resolved_path.c_str() + (dir && *dir ? 0 : 2);
}

const dir_listing *dir_find_files_with_extension(const char *dir, const char *extension)
{
    clear_dir_listing();
    data.current_dir = dir ? dir : "";
    platform_file_manager_list_directory_contents(data.current_dir.empty() ? nullptr : data.current_dir.c_str(), TYPE_FILE, extension, add_to_listing);
    return finalize_dir_listing();
}

const dir_listing *dir_find_files_with_extension_at_location(int location, const char *extension)
{
    return dir_find_files_with_extension(platform_file_manager_get_directory_for_location(location, 0), extension);
}

const dir_listing *dir_find_all_subdirectories(const char *dir)
{
    clear_dir_listing();
    data.current_dir = dir ? dir : "";
    platform_file_manager_list_directory_contents(data.current_dir.empty() ? nullptr : data.current_dir.c_str(), TYPE_DIR, 0, add_to_listing);
    return finalize_dir_listing();
}

const dir_listing *dir_find_all_subdirectories_at_location(int location)
{
    return dir_find_all_subdirectories(platform_file_manager_get_directory_for_location(location, 0));
}

const dir_listing *dir_append_files_with_extension(const char *extension)
{
    platform_file_manager_list_directory_contents(data.current_dir.empty() ? nullptr : data.current_dir.c_str(), TYPE_FILE, extension, add_to_listing);
    return finalize_dir_listing();
}

const char *dir_get_file(const char *filepath, int localizable)
{
    if (!filepath) {
        return nullptr;
    }
    if (strncmp(ASSETS_DIRECTORY, filepath, sizeof(ASSETS_DIRECTORY) - 1) == 0) {
        return dir_get_file_at_location(filepath + sizeof(ASSETS_DIRECTORY), PATH_LOCATION_ASSET);
    }
    if (localizable != NOT_LOCALIZED) {
        const char *custom_dir = config_get_string(CONFIG_STRING_UI_LANGUAGE_DIR);
        if (*custom_dir) {
            const char *path = get_case_corrected_file(custom_dir, filepath);
            if (path) {
                return path;
            } else if (localizable == MUST_BE_LOCALIZED) {
                return nullptr;
            }
        }
    }

    return get_case_corrected_file(nullptr, filepath);
}

const char *dir_get_file_at_location(const char *filename, int location)
{
    return get_case_corrected_file(platform_file_manager_get_directory_for_location(location, 0), filename);
}

const char *dir_append_location(const char *filename, int location)
{
    data.appended_path = platform_file_manager_get_directory_for_location(location, 0);
    data.appended_path += filename;
    return data.appended_path.c_str();
}
