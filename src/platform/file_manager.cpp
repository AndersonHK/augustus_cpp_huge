#include "file_manager.h"

#include "assets/assets.h"
#include "core/config.h"
#include "core/file.h"
#include "core/log.h"
#include "core/random.h"
#include "core/string.h"
#include "platform/android/android.h"
#include "platform/emscripten/emscripten.h"
#include "platform/file_manager_cache.h"
#include "platform/platform.h"
#include "platform/prefs.h"

#ifndef BUILDING_ASSET_PACKER
#include "SDL.h"
#else
#define SDL_VERSION_ATLEAST(x, y, z) 0
#endif

#include <algorithm>
#include <chrono>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <io.h>
#include <windows.h>
#else
#include <unistd.h>
#endif

#ifndef CUSTOM_ASSETS_DIR
#define CUSTOM_ASSETS_DIR nullptr
#endif

#ifdef __EMSCRIPTEN__
static int writing_to_file;
#endif

namespace
{
namespace fs = std::filesystem;

constexpr std::size_t ASSET_PATH_PREFIX_LENGTH = sizeof(ASSETS_DIRECTORY) - 1;
constexpr std::size_t MAX_ASSET_DIRS = 10;

struct PathDescriptor
{
    const char *subpath;
    bool requires_user_directory;
};

static const PathDescriptor k_path_descriptors[] = {
    { "", false },                                // PATH_LOCATION_ROOT
    { "config/", true },                          // PATH_LOCATION_CONFIG
    { "", false },                                // PATH_LOCATION_ASSET
    { "savegames/", true },                       // PATH_LOCATION_SAVEGAME
    { "scenarios/", true },                       // PATH_LOCATION_SCENARIO
    { "campaigns/", false },                      // PATH_LOCATION_CAMPAIGN
    { "screenshots/", true },                     // PATH_LOCATION_SCREENSHOT
    { "community/", false },                      // PATH_LOCATION_COMMUNITY
    { "editor/empires/", false },                 // PATH_LOCATION_EDITOR_CUSTOM_EMPIRES
    { "editor/messages/", false },                // PATH_LOCATION_EDITOR_CUSTOM_MESSAGES
    { "editor/events/", false },                  // PATH_LOCATION_EDITOR_CUSTOM_EVENTS
    { "editor/model/", false },                   // PATH_LOCATION_EDITOR_MODEL_DATA
    { "community/image", false }                  // PATH_LOCATION_COMMUNITY_IMAGE
};

#if defined(_WIN32) || defined(__SWITCH__) || defined(__APPLE__)
static const char *const k_asset_directories[MAX_ASSET_DIRS] = {
#ifdef _WIN32
    "***SDL_BASE_PATH***",
#endif
#ifdef __EMSCRIPTEN__
    "",
#endif
    ".",
#if defined(__SWITCH__)
    "romfs:",
#elif defined(__APPLE__)
    "***SDL_BASE_PATH***",
#endif
    CUSTOM_ASSETS_DIR
};
#else
static const char *const k_asset_directories[MAX_ASSET_DIRS] = {
    "***SDL_BASE_PATH***",
#ifdef __EMSCRIPTEN__
    "",
#endif
    ".",
    "***RELATIVE_APPIMG_PATH***",
    "***EXEC_PATH***",
    "***RELATIVE_EXEC_PATH***",
    "~/.local/share/augustus-game",
    "/usr/share/augustus-game",
    "/usr/local/share/augustus-game",
    "/opt/augustus-game",
    CUSTOM_ASSETS_DIR
};
#endif

static std::string to_utf8(const fs::path &path)
{
    return path.string();
}

static fs::path make_path(std::string_view path)
{
    return fs::path(std::string(path));
}

static bool starts_with(std::string_view text, std::string_view prefix)
{
    if (text.size() < prefix.size()) {
        return false;
    }
    return text.substr(0, prefix.size()) == prefix;
}

static std::string join_paths(std::string_view left, std::string_view right)
{
    if (left.empty()) {
        return std::string(right);
    }
    if (right.empty()) {
        return std::string(left);
    }
    if ((left.back() == '/' || left.back() == '\\') &&
        (right.front() == '/' || right.front() == '\\')) {
        return std::string(left) + std::string(right.substr(1));
    }
    if ((left.back() != '/' && left.back() != '\\') &&
        (right.front() != '/' && right.front() != '\\')) {
        return std::string(left) + "/" + std::string(right);
    }
    return std::string(left) + std::string(right);
}

static int ascii_lower_compare(std::string_view left, std::string_view right)
{
    const auto limit = std::min(left.size(), right.size());
    for (size_t i = 0; i < limit; ++i) {
        const int lhs = std::tolower(static_cast<unsigned char>(left[i]));
        const int rhs = std::tolower(static_cast<unsigned char>(right[i]));
        if (lhs != rhs) {
            return lhs - rhs;
        }
    }
    if (left.size() == right.size()) {
        return 0;
    }
    return left.size() < right.size() ? -1 : 1;
}

static int ascii_lower_ncompare(std::string_view left, std::string_view right, int limit)
{
    if (limit <= 0) {
        return 0;
    }
    const auto compare_length = std::min<size_t>(limit, std::max(left.size(), right.size()));
    for (size_t i = 0; i < compare_length; ++i) {
        const int lhs = i < left.size() ? std::tolower(static_cast<unsigned char>(left[i])) : 0;
        const int rhs = i < right.size() ? std::tolower(static_cast<unsigned char>(right[i])) : 0;
        if (lhs != rhs) {
            return lhs - rhs;
        }
    }
    return 0;
}

static long file_modified_time_seconds(const fs::path &path)
{
    std::error_code ec;
    const auto last_modified = fs::last_write_time(path, ec);
    if (ec) {
        return 0;
    }
    const auto epoch_time = std::chrono::time_point_cast<std::chrono::seconds>(
        last_modified - fs::file_time_type::clock::now() + std::chrono::system_clock::now());
    return static_cast<long>(std::chrono::duration_cast<std::chrono::seconds>(epoch_time.time_since_epoch()).count());
}

static void preserve_timestamps(std::string_view src, std::string_view dst)
{
    std::error_code src_ec;
    std::error_code dst_ec;
    const auto last = fs::last_write_time(make_path(src), src_ec);
    if (src_ec) {
        return;
    }
    fs::last_write_time(make_path(dst), last, dst_ec);
}

#if !defined(_WIN32) && !defined(__SWITCH__) && !defined(__APPLE__)
static bool resolve_exec_directory(std::string &buffer)
{
    char arg0_dir[FILE_NAME_MAX];
    ssize_t chars_read = readlink("/proc/self/exe", arg0_dir, FILE_NAME_MAX - 1);
    if (chars_read == -1) {
        chars_read = readlink("/proc/curproc/file", arg0_dir, FILE_NAME_MAX - 1);
        if (chars_read == -1) {
            chars_read = readlink("/proc/self/path/a.out", arg0_dir, FILE_NAME_MAX - 1);
            if (chars_read == -1) {
                return false;
            }
        }
    }
    arg0_dir[chars_read] = 0;
    buffer = to_utf8(fs::path(arg0_dir).parent_path());
    return true;
}
#endif

static bool write_base_path_to(std::string &destination)
{
#if !defined(BUILDING_ASSET_PACKER) && SDL_VERSION_ATLEAST(2, 0, 1)
    if (!platform_sdl_version_at_least(2, 0, 1)) {
        return false;
    }
    char *base_path = SDL_GetBasePath();
    if (!base_path) {
        return false;
    }
    destination = base_path;
    SDL_free(base_path);
    return true;
#else
    return false;
#endif
}

static std::string assets_directory;

static void set_assets_directory()
{
#ifdef __ANDROID__
    (void) write_base_path_to;
    return;
#else
    if (!assets_directory.empty()) {
        return;
    }

    for (const char *candidate : k_asset_directories) {
        if (!candidate) {
            continue;
        }

        std::string asset_path;
        bool candidate_ok = true;

        if (*candidate == '~') {
            const char *home_dir = getenv("HOME");
            if (!home_dir) {
                continue;
            }
            asset_path = home_dir;
            asset_path += candidate + 1;
        } else if (candidate == std::string{"***SDL_BASE_PATH***"}) {
            if (!write_base_path_to(asset_path)) {
                continue;
            }
        } else if (candidate == std::string{"***RELATIVE_APPIMG_PATH***"}) {
#if defined(_WIN32) || defined(__SWITCH__) || defined(__APPLE__)
            log_error("***RELATIVE_APPIMG_PATH*** is not available on your platform.", 0, 0);
            continue;
#else
            if (!write_base_path_to(asset_path)) {
                continue;
            }
            const auto base = asset_path.find("/bin");
            if (base == std::string::npos) {
                continue;
            }
            asset_path.resize(base);
            asset_path += "/share/augustus-game";
#endif
        } else if (candidate == std::string{"***EXEC_PATH***"}) {
#if defined(_WIN32) || defined(__SWITCH__) || defined(__APPLE__)
            log_error("***EXEC_PATH*** is not available on your platform.", 0, 0);
            continue;
#else
            if (!resolve_exec_directory(asset_path)) {
                continue;
            }
#endif
        } else if (candidate == std::string{"***RELATIVE_EXEC_PATH***"}) {
#if defined(_WIN32) || defined(__SWITCH__) || defined(__APPLE__)
            log_error("***RELATIVE_EXEC_PATH*** is not available on your platform.", 0, 0);
            continue;
#else
            if (!resolve_exec_directory(asset_path)) {
                continue;
            }
            asset_path += "/../share/augustus-game";
#endif
        } else {
            asset_path = candidate;
        }

        if (asset_path.empty()) {
            candidate_ok = false;
        }
        if (!candidate_ok) {
            continue;
        }

        if (!asset_path.empty() && asset_path.back() != '/') {
            asset_path += '/';
        }
        if (asset_path != "romfs:/") {
            asset_path += ASSETS_DIR_NAME;
        }

        log_info("Trying asset path at", asset_path.c_str(), 0);
        if (fs::is_directory(make_path(asset_path))) {
            assets_directory = asset_path;
            log_info("Asset path detected at", asset_path.c_str(), 0);
            return;
        }
    }
    assets_directory = ".";
#endif
}

static std::string resolve_asset_path(std::string_view path)
{
    if (starts_with(path, ASSETS_DIRECTORY)) {
        set_assets_directory();
        if (path.size() == ASSET_PATH_PREFIX_LENGTH) {
            return assets_directory;
        }
        return join_paths(assets_directory, path.substr(ASSET_PATH_PREFIX_LENGTH));
    }
    return std::string(path);
}

static std::string resolve_directory_for_listing(std::string_view dir)
{
    if (!dir.data() || !*dir.data() || dir == ".") {
        return ".";
    }
    if (starts_with(dir, ASSETS_DIRECTORY)) {
        return resolve_asset_path(dir);
    }
    return std::string(dir);
}

static int do_nothing(const char *name, long unused)
{
    (void) name;
    (void) unused;
    return LIST_MATCH;
}

static bool copy_directory_recursive(const fs::path &source, const fs::path &destination, bool overwrite_files, bool is_root)
{
    if (!fs::is_directory(source)) {
        return false;
    }

    std::error_code ec;
    if (fs::exists(destination, ec)) {
        if (!fs::is_directory(destination, ec)) {
            return false;
        }
        if (!overwrite_files && !is_root) {
            return true;
        }
    } else if (!fs::create_directories(destination, ec)) {
        return false;
    }
    if (ec) {
        return false;
    }

    bool success = true;
    for (fs::directory_iterator it(source, ec); it != fs::directory_iterator{} && !ec; it.increment(ec)) {
        const auto &entry = *it;
        const fs::file_status status = entry.symlink_status(ec);
        if (ec) {
            success = false;
            break;
        }

        const bool is_file = fs::is_regular_file(status) || fs::is_symlink(status);
        const bool is_directory = fs::is_directory(status);
        if (!is_file && !is_directory) {
            continue;
        }

        const fs::path destination_entry = destination / entry.path().filename();
        if (is_directory) {
            if (!copy_directory_recursive(entry.path(), destination_entry, overwrite_files, false)) {
                success = false;
            }
            continue;
        }

        if (!overwrite_files && fs::exists(destination_entry, ec)) {
            if (ec) {
                success = false;
            }
            continue;
        }

        if (!platform_file_manager_copy_file(to_utf8(entry.path()).c_str(), to_utf8(destination_entry).c_str())) {
            success = false;
        }
    }
    return success && !ec;
}

static bool remove_directory_recursive(const fs::path &path)
{
    std::error_code ec;
    if (!fs::is_directory(path, ec)) {
        return false;
    }

    bool success = true;
    for (fs::directory_iterator it(path, ec); it != fs::directory_iterator{} && !ec; it.increment(ec)) {
        const auto &entry = *it;
        const fs::file_status status = entry.symlink_status(ec);
        if (ec) {
            success = false;
            break;
        }

        const bool is_directory = fs::is_directory(status);
        const bool is_file = fs::is_regular_file(status) || fs::is_symlink(status);

        if (is_directory) {
            if (!remove_directory_recursive(entry.path())) {
                success = false;
                break;
            }
            continue;
        }
        if (is_file && !platform_file_manager_remove_file(to_utf8(entry.path()).c_str())) {
            success = false;
            break;
        }
    }
    if (!ec && success) {
        fs::remove(path, ec);
    }
    return success && !ec;
}
} // namespace

int platform_file_manager_list_directory_contents(
    const char *dir, int type, const char *extension, int (*callback)(const char *, long))
{
    if (type == TYPE_NONE) {
        return LIST_ERROR;
    }

    const std::string current_dir = resolve_directory_for_listing(dir ? dir : "");

#ifdef __ANDROID__
    return android_get_directory_contents(current_dir.c_str(), type, extension, callback);
#elif defined(USE_FILE_CACHE)
    const dir_info *d = platform_file_manager_cache_get_dir_info(current_dir.c_str());
    if (!d) {
        return LIST_ERROR;
    }
    int match = LIST_NO_MATCH;
    for (file_info *f = d->first_file; f; f = f->next) {
        if (!(type & f->type)) {
            continue;
        }
        if (!platform_file_manager_cache_file_has_extension(f, extension)) {
            continue;
        }
        match = callback(f->name, f->modified_time);
        if (match == LIST_MATCH) {
            break;
        }
    }
    return match;
#else
    int match = LIST_NO_MATCH;
    std::error_code ec;
    for (fs::directory_iterator it(make_path(current_dir), ec); it != fs::directory_iterator{} && !ec; it.increment(ec)) {
        const auto &entry = *it;
        const auto status = entry.symlink_status(ec);
        if (ec) {
            return LIST_ERROR;
        }

        const bool is_file = fs::is_regular_file(status) || fs::is_symlink(status);
        const bool is_directory = fs::is_directory(status);
        if (!is_file && !is_directory) {
            continue;
        }
        if ((is_file && !(type & TYPE_FILE)) || (is_directory && !(type & TYPE_DIR))) {
            continue;
        }

        const std::string name = to_utf8(entry.path().filename());
        if (is_file && !file_has_extension(name.c_str(), extension)) {
            continue;
        }
        if (type & TYPE_DIR && !name.empty() && name[0] == '.') {
            continue;
        }

        const long modified = file_modified_time_seconds(entry.path());
        match = callback(name.c_str(), modified);
        if (match == LIST_MATCH) {
            break;
        }
    }
    if (ec) {
        return LIST_ERROR;
    }
    return match;
#endif
}

int platform_file_manager_should_case_correct_file(void)
{
#if defined(_WIN32) || defined(__ANDROID__)
    return 0;
#else
    return 1;
#endif
}

int platform_file_manager_filename_contains(const char *filename, const char *expression)
{
    const bool has_filename = filename && *filename;
    const bool has_expression = expression && *expression;
    if (!has_filename) {
        return !has_expression;
    }
    if (!has_expression) {
        return 1;
    }
    const std::string_view filename_view{filename};
    const std::string_view expression_view{expression};
    if (filename_view.size() < expression_view.size()) {
        return 0;
    }
    const size_t limit = filename_view.size() - expression_view.size();
    for (size_t i = 0; i <= limit; i++) {
        if (platform_file_manager_compare_filename_prefix(filename + i, expression, static_cast<int>(expression_view.size())) == 0) {
            return 1;
        }
    }
    return 0;
}

int platform_file_manager_compare_filename(const char *a, const char *b)
{
    return ascii_lower_compare(a ? std::string_view(a) : std::string_view(),
                              b ? std::string_view(b) : std::string_view());
}

int platform_file_manager_compare_filename_prefix(const char *filename, const char *prefix, int prefix_len)
{
    return ascii_lower_ncompare(filename ? std::string_view(filename) : std::string_view(),
                               prefix ? std::string_view(prefix) : std::string_view(),
                               prefix_len);
}

int platform_file_manager_set_base_path(const char *path)
{
    if (!path) {
        log_error("set_base_path: path was not set. Augustus will probably crash.", 0, 0);
        return 0;
    }
#ifdef __ANDROID__
    return android_set_base_path(path);
#else
    std::error_code ec;
    fs::current_path(make_path(path), ec);
    if (ec) {
        return 0;
    }
#ifdef USE_FILE_CACHE
    platform_file_manager_cache_invalidate();
#endif
    return 1;
#endif
}

std::string platform_file_manager_get_directory_for_location(int location, const char *user_directory)
{
    if (!user_directory) {
        user_directory = pref_user_dir();
    }

    if (location < 0 || location >= PATH_LOCATION_MAX) {
        return {};
    }

    std::string full_path;
    if (location == PATH_LOCATION_ASSET) {
        set_assets_directory();
        full_path = assets_directory;
    } else {
        if (k_path_descriptors[location].requires_user_directory && !*user_directory) {
            return {};
        } else {
            full_path = join_paths(user_directory, k_path_descriptors[location].subpath);
        }
    }

    if (full_path.size() >= FILE_NAME_MAX) {
        log_error("Path ID too long for location: ", 0, location);
    }
    return full_path;
}

int platform_file_manager_is_directory_writeable(const char *directory)
{
    if (!directory || !*directory) {
        directory = ".";
    }
    int attempt = 0;
    std::string test_file_path;
    do {
        test_file_path = join_paths(directory, "test_");
        test_file_path += std::to_string(random_from_stdlib()) + ".txt";
        attempt++;
    } while (file_exists(test_file_path.c_str(), NOT_LOCALIZED) && attempt < 5);

    FILE *fp = file_open(test_file_path.c_str(), "w");
    if (!fp) {
        return 0;
    }
    fclose(fp);
    if (!file_remove(test_file_path.c_str())) {
        return 0;
    }
    return 1;
}

#if defined(__ANDROID__)

FILE *platform_file_manager_open_file(const char *filename, const char *mode)
{
    int fd = android_get_file_descriptor(filename, mode);
    if (!fd) {
        return NULL;
    }
    return fdopen(fd, mode);
}

FILE *platform_file_manager_open_asset(const char *asset, const char *mode)
{
    return (FILE *)android_open_asset(asset, mode);
}

int platform_file_manager_remove_file(const char *filename)
{
    return android_remove_file(filename);
}

#else

FILE *platform_file_manager_open_file(const char *filename, const char *mode)
{
#ifdef USE_FILE_CACHE
    if (strchr(mode, 'w')) {
        platform_file_manager_cache_update_file_info(filename);
    }
#endif

#if defined(__EMSCRIPTEN__)
    writing_to_file = strchr(mode, 'w') != nullptr;
#endif

#ifdef _WIN32
    const std::wstring path = make_path(filename).wstring();
    const std::wstring file_mode = make_path(mode).wstring();
    return _wfopen(path.c_str(), file_mode.c_str());
#else
    return std::fopen(to_utf8(make_path(filename)).c_str(), mode);
#endif
}

int platform_file_manager_remove_file(const char *filename)
{
#ifdef USE_FILE_CACHE
    platform_file_manager_cache_delete_file_info(filename);
#endif
#ifdef _WIN32
    const std::wstring path = make_path(filename).wstring();
    const int result = _wremove(path.c_str()) == 0 ? 1 : 0;
#else
    const int result = fs::remove(make_path(filename)) ? 1 : 0;
#endif
#if defined(__EMSCRIPTEN__)
    if (result == 1) {
        EM_ASM(
            Module.syncFS();
        );
    }
#endif
    return result;
}

FILE *platform_file_manager_open_asset(const char *asset, const char *mode)
{
    const char *cased_asset_path = dir_get_file_at_location(asset, PATH_LOCATION_ASSET);
    return cased_asset_path ? platform_file_manager_open_file(cased_asset_path, mode) : 0;
}
#endif

int platform_file_manager_close_file(FILE *stream)
{
    int result = fclose(stream);
#ifdef __EMSCRIPTEN__
    if (writing_to_file) {
        writing_to_file = 0;
        EM_ASM(
            Module.syncFS();
        );
    }
#endif
    return result == 0;
}

int platform_file_manager_flush_file(FILE *stream)
{
    if (!stream || fflush(stream) != 0) {
        return 0;
    }
#if defined(_WIN32)
    return _commit(_fileno(stream)) == 0;
#elif defined(__EMSCRIPTEN__)
    return 1;
#else
    return fsync(fileno(stream)) == 0;
#endif
}

int platform_file_manager_create_directory(const char *name, const char *location, int overwrite)
{
    std::string local_name = name ? name : "";
    if (local_name.empty()) {
        return 0;
    }

    std::string current_path = location ? std::string(location) : "";
    if (location) {
        const size_t location_length = std::char_traits<char>::length(location);
        if (local_name.rfind(location, 0) == 0) {
            local_name = local_name.substr(location_length);
        }
    }

    if (!local_name.empty() && (local_name[0] == '/' || local_name[0] == '\\')) {
        local_name.erase(0, 1);
    }

    bool exists = false;
    fs::path base_path = current_path.empty() ? fs::path() : make_path(current_path);

    size_t start = 0;
    while (start < local_name.size()) {
        while (start < local_name.size() && (local_name[start] == '/' || local_name[start] == '\\')) {
            ++start;
        }
        if (start >= local_name.size()) {
            break;
        }
        size_t token_end = start;
        while (token_end < local_name.size() && local_name[token_end] != '/' && local_name[token_end] != '\\') {
            ++token_end;
        }
        base_path /= make_path(local_name.substr(start, token_end - start));
        start = token_end;

#ifdef __ANDROID__
        const std::string path_token = to_utf8(base_path);
        const int result = android_create_directory(path_token.c_str());
        if (result != 1) {
            if (result == 0) {
                return 0;
            } else if (!overwrite) {
                exists = true;
            }
        }
#else
        std::error_code ec;
        const bool target_exists = fs::exists(base_path, ec);
        if (ec) {
            return 0;
        }
        if (target_exists) {
            if (!fs::is_directory(base_path, ec) || ec) {
                return 0;
            }
            if (!overwrite) {
                exists = true;
            }
            continue;
        }
        if (!fs::create_directory(base_path, ec)) {
            return 0;
        }
#endif
    }
    return overwrite || !exists;
}

int platform_file_manager_copy_file(const char *src, const char *dst)
{
    std::ifstream source(make_path(src), std::ios::binary);
    if (!source.is_open()) {
        return 0;
    }
    std::ofstream destination(make_path(dst), std::ios::binary);
    if (!destination.is_open()) {
        return 0;
    }
    destination << source.rdbuf();
    if (!destination.good()) {
        return 0;
    }

    preserve_timestamps(src, dst);
    return 1;
}

int platform_file_manager_replace_file(const char *src, const char *dst)
{
    if (!src || !*src || !dst || !*dst) {
        return 0;
    }
    int result = 0;
#if defined(_WIN32)
    const std::wstring source_path = make_path(src).wstring();
    const std::wstring destination_path = make_path(dst).wstring();
    result = MoveFileExW(source_path.c_str(), destination_path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) ? 1 : 0;
#else
    std::error_code ec;
    fs::rename(make_path(src), make_path(dst), ec);
    if (ec) {
        return 0;
    }
#if defined(__EMSCRIPTEN__)
    EM_ASM(Module.syncFS(););
#endif
    result = 1;
#endif
#ifdef USE_FILE_CACHE
    if (result) {
        platform_file_manager_cache_delete_file_info(src);
        platform_file_manager_cache_update_file_info(dst);
    }
#endif
    return result;
}

int platform_file_manager_copy_directory(const char *src, const char *dst, int overwrite_files)
{
    if (!platform_file_manager_list_directory_contents(src, TYPE_DIR, 0, do_nothing)) {
        return 0;
    }

    const fs::path source_path = make_path(resolve_asset_path(src ? src : ""));
    const fs::path destination_path = make_path(resolve_asset_path(dst ? dst : ""));
    return copy_directory_recursive(source_path, destination_path, overwrite_files, true);
}

int platform_file_manager_remove_directory(const char *path)
{
    const fs::path target_path = make_path(resolve_asset_path(path ? path : ""));
    return remove_directory_recursive(target_path);
}
