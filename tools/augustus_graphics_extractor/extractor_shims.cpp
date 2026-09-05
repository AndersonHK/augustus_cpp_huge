#include "core/crash_context.h"
#include "assets/graphics_extractor_shims.h"
#include "core/dir.h"
#include "core/file.h"
#include "core/log.h"
#include "assets/assets.h"
#include "building/building_type.h"
#include "game/mod_manager.h"
#include "graphics/font.h"
#include "graphics/renderer.h"
#include "graphics/runtime_overlay_images.h"
#include "platform/file_manager.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <array>
#include <string>
#include <vector>

typedef struct building building;

namespace {

class ListingStorage {
public:
    dir_listing listing = {};
    std::vector<dir_entry> entries;
    std::vector<std::string> names;
};

ListingStorage g_listing;
int g_debug_enabled = 0;
std::filesystem::path g_game_root = std::filesystem::current_path();
std::string g_augustus_graphics_path = "Mods/Augustus/Graphics/";
std::string g_julius_graphics_path = "Mods/Julius/Graphics/";

class HarnessAtlas {
public:
    image_atlas_data data = {};
    std::vector<std::vector<color_t>> pixel_pages;
    std::vector<color_t *> page_pointers;
    std::vector<int> widths;
    std::vector<int> heights;
};

std::array<HarnessAtlas, ATLAS_MAX> g_atlases;

std::string normalize_slashes(std::string value)
{
    for (char &ch : value) {
        if (ch == '\\') {
            ch = '/';
        }
    }
    return value;
}

std::string asset_root()
{
    return (g_game_root / "assets").string();
}

bool has_extension(const std::filesystem::path &path, const char *extension)
{
    if (!extension || !*extension) {
        return true;
    }

    std::string actual = path.extension().string();
    if (!actual.empty() && actual.front() == '.') {
        actual.erase(actual.begin());
    }
    std::string expected = extension;
    std::transform(actual.begin(), actual.end(), actual.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    std::transform(expected.begin(), expected.end(), expected.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return actual == expected;
}

std::filesystem::path resolve_asset_marker_path(const char *path)
{
    if (!path) {
        return {};
    }
    std::string value = normalize_slashes(path);
    constexpr const char *marker = ASSETS_DIRECTORY;
    const std::string normalized_marker = normalize_slashes(marker);
    if (value.rfind(normalized_marker, 0) == 0) {
        value.erase(0, normalized_marker.size());
        while (!value.empty() && value.front() == '/') {
            value.erase(value.begin());
        }
        return std::filesystem::path(asset_root()) / value;
    }
    std::filesystem::path resolved(path);
    if (resolved.is_relative()) {
        resolved = g_game_root / resolved;
    }
    return resolved;
}

long modified_time_for(const std::filesystem::directory_entry &entry)
{
    std::error_code error;
    const auto value = entry.last_write_time(error);
    if (error) {
        return 0;
    }
    return static_cast<long>(value.time_since_epoch().count());
}

void log_line(const char *label, const char *msg, const char *param_str, int param_int)
{
    std::ostream &stream = std::strcmp(label, "Error") == 0 ? std::cerr : std::cout;
    stream << label << ": " << (msg ? msg : "");
    if (param_str) {
        stream << "  " << param_str;
    }
    if (param_int) {
        stream << "  " << param_int;
    }
    stream << "\n";
}

std::string with_trailing_separator(const std::string &path)
{
    if (path.empty() || path.back() == '\\' || path.back() == '/') {
        return path;
    }
    return path + std::string(1, std::filesystem::path::preferred_separator);
}

void reset_atlas(HarnessAtlas &atlas, atlas_type type)
{
    atlas.pixel_pages.clear();
    atlas.page_pointers.clear();
    atlas.widths.clear();
    atlas.heights.clear();
    atlas.data = {};
    atlas.data.type = type;
}

void renderer_get_max_image_size(int *width, int *height)
{
    if (width) {
        *width = 4096;
    }
    if (height) {
        *height = 4096;
    }
}

const image_atlas_data *renderer_prepare_image_atlas(atlas_type type, int num_images, int last_width, int last_height)
{
    if (type < ATLAS_FIRST || type >= ATLAS_MAX || num_images <= 0 || last_width <= 0 || last_height <= 0) {
        return nullptr;
    }

    HarnessAtlas &atlas = g_atlases[type];
    reset_atlas(atlas, type);
    atlas.pixel_pages.resize(static_cast<size_t>(num_images));
    atlas.page_pointers.resize(static_cast<size_t>(num_images));
    atlas.widths.resize(static_cast<size_t>(num_images));
    atlas.heights.resize(static_cast<size_t>(num_images));

    for (int index = 0; index < num_images; ++index) {
        const int width = index == num_images - 1 ? last_width : 4096;
        const int height = index == num_images - 1 ? last_height : 4096;
        atlas.widths[static_cast<size_t>(index)] = width;
        atlas.heights[static_cast<size_t>(index)] = height;
        atlas.pixel_pages[static_cast<size_t>(index)].assign(static_cast<size_t>(width) * height, 0);
        atlas.page_pointers[static_cast<size_t>(index)] = atlas.pixel_pages[static_cast<size_t>(index)].data();
    }

    atlas.data.type = type;
    atlas.data.num_images = num_images;
    atlas.data.buffers = atlas.page_pointers.data();
    atlas.data.image_widths = atlas.widths.data();
    atlas.data.image_heights = atlas.heights.data();
    return &atlas.data;
}

int renderer_create_image_atlas(const image_atlas_data *atlas_data, int delete_buffers)
{
    if (!atlas_data || atlas_data->type < ATLAS_FIRST || atlas_data->type >= ATLAS_MAX) {
        return 0;
    }
    if (delete_buffers) {
        reset_atlas(g_atlases[atlas_data->type], atlas_data->type);
    }
    return 1;
}

const image_atlas_data *renderer_get_image_atlas(atlas_type type)
{
    if (type < ATLAS_FIRST || type >= ATLAS_MAX || g_atlases[type].data.num_images <= 0) {
        return nullptr;
    }
    return &g_atlases[type].data;
}

int renderer_has_image_atlas(atlas_type type)
{
    return renderer_get_image_atlas(type) ? 1 : 0;
}

void renderer_free_image_atlas(atlas_type type)
{
    if (type >= ATLAS_FIRST && type < ATLAS_MAX) {
        reset_atlas(g_atlases[type], type);
    }
}

void renderer_upload_image_resource(image *img, const color_t *pixels, int width, int height)
{
    static int next_handle = 1;
    (void) pixels;
    (void) width;
    (void) height;
    if (img) {
        img->resource_handle = next_handle++;
    }
}

void renderer_release_image_resource(image *img)
{
    if (img) {
        img->resource_handle = 0;
    }
}

int renderer_should_pack_image(int width, int height)
{
    (void) width;
    (void) height;
    return 1;
}

graphics_renderer_interface g_renderer = {};
image g_stub_image = {};
font_definition g_stub_font = {};

} // namespace

void augustus_graphics_extractor_shims_set_game_root(const char *path)
{
    if (path && *path) {
        g_game_root = std::filesystem::absolute(std::filesystem::path(path));
    }
}

void augustus_graphics_extractor_shims_set_augustus_graphics_path(const char *path)
{
    if (path && *path) {
        g_augustus_graphics_path = with_trailing_separator(std::filesystem::absolute(std::filesystem::path(path)).string());
    }
}

void augustus_graphics_extractor_shims_set_julius_graphics_path(const char *path)
{
    if (path && *path) {
        g_julius_graphics_path = with_trailing_separator(std::filesystem::absolute(std::filesystem::path(path)).string());
    }
}

void augustus_graphics_extractor_shims_install_renderer(void)
{
    g_renderer.get_max_image_size = renderer_get_max_image_size;
    g_renderer.prepare_image_atlas = renderer_prepare_image_atlas;
    g_renderer.create_image_atlas = renderer_create_image_atlas;
    g_renderer.get_image_atlas = renderer_get_image_atlas;
    g_renderer.has_image_atlas = renderer_has_image_atlas;
    g_renderer.free_image_atlas = renderer_free_image_atlas;
    g_renderer.upload_image_resource = renderer_upload_image_resource;
    g_renderer.release_image_resource = renderer_release_image_resource;
    g_renderer.should_pack_image = renderer_should_pack_image;
    graphics_renderer_set_interface(&g_renderer);
}

int assets_init(int force_reload, color_t **main_images, int *main_image_widths)
{
    (void) force_reload;
    (void) main_images;
    (void) main_image_widths;
    return 1;
}

int assets_get_image_id(const char *assetlist_name, const char *image_name)
{
    (void) assetlist_name;
    (void) image_name;
    return 0;
}

const image *assets_get_image(int image_id)
{
    (void) image_id;
    return &g_stub_image;
}

const image *assets_get_font_image(int letter_id)
{
    (void) letter_id;
    return &g_stub_image;
}

const font_definition *font_definition_for(font_t font)
{
    (void) font;
    return &g_stub_font;
}

void assets_load_unpacked_asset(int image_id)
{
    (void) image_id;
}

int screen_width(void)
{
    return 640;
}

int screen_height(void)
{
    return 480;
}

int runtime_overlay_images_init_or_reload(void)
{
    return 1;
}

void runtime_overlay_images_reset(void)
{
}

const Image *runtime_footprint_overlay_image()
{
    return nullptr;
}

building *building_first_of_type(building_type type)
{
    (void) type;
    return nullptr;
}

building_type building_type_registry_runtime_id_from_text(const char *text_id)
{
    (void) text_id;
    return BUILDING_NONE;
}

unsigned int map_image_at(int grid_offset)
{
    (void) grid_offset;
    return 0;
}

void map_image_set(int grid_offset, int image_id)
{
    (void) grid_offset;
    (void) image_id;
}

void map_building_tiles_add(unsigned int building_id, int x, int y, int size, int image_id, int terrain)
{
    (void) building_id;
    (void) x;
    (void) y;
    (void) size;
    (void) image_id;
    (void) terrain;
}

void log_info(const char *msg, const char *param_str, int param_int)
{
    log_line("Info", msg, param_str, param_int);
}

void log_set_debug_enabled(bool enabled)
{
    g_debug_enabled = enabled ? 1 : 0;
}

bool log_is_debug_enabled()
{
    return g_debug_enabled != 0;
}

void log_warning(const char *msg, const char *param_str, int param_int)
{
    log_line("Warning", msg, param_str, param_int);
}

void log_error(const char *msg, const char *param_str, int param_int)
{
    log_line("Error", msg, param_str, param_int);
}

void log_repeated_messages()
{
}

FILE *file_open(const char *filename, const char *mode)
{
    if (!filename) {
        return nullptr;
    }
    const std::filesystem::path path = resolve_asset_marker_path(filename);
    return std::fopen(path.string().c_str(), mode);
}

FILE *file_open_asset(const char *asset, const char *mode)
{
    const std::filesystem::path path = std::filesystem::path(asset_root()) / (asset ? asset : "");
    return std::fopen(path.string().c_str(), mode);
}

int file_close(FILE *stream)
{
    return std::fclose(stream) == 0;
}

int file_has_extension(const char *filename, const char *extension)
{
    return has_extension(std::filesystem::path(filename ? filename : ""), extension) ? 1 : 0;
}

void file_change_extension(char *filename, const char *new_extension)
{
    if (!filename || !new_extension || !*new_extension) {
        return;
    }
    char *dot = std::strrchr(filename, '.');
    if (dot) {
        std::snprintf(dot + 1, std::strlen(dot + 1) + 1, "%s", new_extension);
    }
}

void file_append_extension(char *filename, const char *extension, size_t length)
{
    if (!filename || !extension || !*extension) {
        return;
    }
    const size_t current_length = std::strlen(filename);
    if (current_length + std::strlen(extension) + 2 > length) {
        return;
    }
    std::snprintf(filename + current_length, length - current_length, ".%s", extension);
}

void file_remove_extension(char *filename)
{
    if (!filename) {
        return;
    }
    char *dot = std::strrchr(filename, '.');
    if (dot) {
        *dot = '\0';
    }
}

const char *file_remove_path(const char *filename)
{
    if (!filename) {
        return "";
    }
    const char *slash = std::strrchr(filename, '/');
    const char *backslash = std::strrchr(filename, '\\');
    const char *separator = std::max(slash ? slash : filename, backslash ? backslash : filename);
    return separator == filename ? filename : separator + 1;
}

int file_exists(const char *filename, int localizable)
{
    (void) localizable;
    return std::filesystem::exists(resolve_asset_marker_path(filename)) ? 1 : 0;
}

int file_remove(const char *filename)
{
    std::error_code error;
    return std::filesystem::remove(resolve_asset_marker_path(filename), error) ? 1 : 0;
}

const dir_listing *dir_find_files_with_extension(const char *dir, const char *extension)
{
    g_listing.entries.clear();
    g_listing.names.clear();

    std::vector<std::pair<std::string, long>> matches;
    std::error_code error;
    const std::filesystem::path directory = resolve_asset_marker_path(dir && *dir ? dir : ".");
    for (const std::filesystem::directory_entry &entry : std::filesystem::directory_iterator(directory, error)) {
        if (error) {
            break;
        }
        if (!entry.is_regular_file() || !has_extension(entry.path(), extension)) {
            continue;
        }
        matches.emplace_back(entry.path().filename().string(), modified_time_for(entry));
    }

    std::sort(matches.begin(), matches.end(), [](const auto &left, const auto &right) {
        std::string left_name = left.first;
        std::string right_name = right.first;
        std::transform(left_name.begin(), left_name.end(), left_name.begin(), [](unsigned char ch) {
            return static_cast<char>(std::tolower(ch));
        });
        std::transform(right_name.begin(), right_name.end(), right_name.begin(), [](unsigned char ch) {
            return static_cast<char>(std::tolower(ch));
        });
        return left_name < right_name;
    });

    g_listing.entries.resize(matches.size());
    g_listing.names.reserve(matches.size());
    for (size_t index = 0; index < matches.size(); ++index) {
        g_listing.names.push_back(matches[index].first);
        g_listing.entries[index].name = const_cast<char *>(g_listing.names[index].c_str());
        g_listing.entries[index].modified_time = static_cast<unsigned int>(matches[index].second);
    }
    g_listing.listing.files = g_listing.entries.empty() ? nullptr : g_listing.entries.data();
    g_listing.listing.num_files = static_cast<int>(g_listing.entries.size());
    return &g_listing.listing;
}

const dir_listing *dir_find_files_with_extension_at_location(int location, const char *extension)
{
    const std::string directory = platform_file_manager_get_directory_for_location(location);
    return dir_find_files_with_extension(directory.c_str(), extension);
}

const dir_listing *dir_append_files_with_extension(const char *extension)
{
    return dir_find_files_with_extension(".", extension);
}

const dir_listing *dir_find_all_subdirectories(const char *dir)
{
    return dir_find_files_with_extension(dir, nullptr);
}

const dir_listing *dir_find_all_subdirectories_at_location(int location)
{
    const std::string directory = platform_file_manager_get_directory_for_location(location);
    return dir_find_all_subdirectories(directory.c_str());
}

const char *dir_get_file(const char *filepath, int localizable)
{
    (void) localizable;
    static std::string resolved;
    const std::filesystem::path path = resolve_asset_marker_path(filepath);
    if (!std::filesystem::exists(path)) {
        return nullptr;
    }
    resolved = path.string();
    return resolved.c_str();
}

const char *dir_get_file_at_location(const char *filepath, int location)
{
    static std::string resolved;
    resolved = (std::filesystem::path(platform_file_manager_get_directory_for_location(location)) /
        (filepath ? filepath : "")).string();
    return std::filesystem::exists(resolved) ? resolved.c_str() : nullptr;
}

const char *dir_append_location(const char *filename, int location)
{
    static std::string resolved;
    resolved = (std::filesystem::path(platform_file_manager_get_directory_for_location(location)) /
        (filename ? filename : "")).string();
    return resolved.c_str();
}

int platform_file_manager_set_base_path(const char *path)
{
    if (!path || !*path) {
        return 0;
    }
    std::error_code error;
    std::filesystem::current_path(path, error);
    return error ? 0 : 1;
}

std::string platform_file_manager_get_directory_for_location(int location, const char *user_directory)
{
    (void) user_directory;
    if (location == PATH_LOCATION_ASSET) {
        return asset_root();
    }
    return std::filesystem::current_path().string();
}

int platform_file_manager_is_directory_writeable(const char *directory)
{
    std::error_code error;
    const std::filesystem::path path = directory && *directory ? directory : ".";
    std::filesystem::create_directories(path, error);
    return error ? 0 : 1;
}

int platform_file_manager_list_directory_contents(
    const char *dir,
    int type,
    const char *extension,
    int (*callback)(const char *, long))
{
    if (!callback || type == TYPE_NONE) {
        return LIST_ERROR;
    }

    std::error_code error;
    const std::filesystem::path directory = resolve_asset_marker_path(dir && *dir ? dir : ".");
    int match = LIST_NO_MATCH;
    for (const std::filesystem::directory_entry &entry : std::filesystem::directory_iterator(directory, error)) {
        if (error) {
            return LIST_ERROR;
        }
        const bool is_dir = entry.is_directory();
        const bool is_file = entry.is_regular_file();
        if ((is_dir && !(type & TYPE_DIR)) || (is_file && !(type & TYPE_FILE)) || (!is_dir && !is_file)) {
            continue;
        }
        if (is_file && !has_extension(entry.path(), extension)) {
            continue;
        }
        match = callback(entry.path().filename().string().c_str(), modified_time_for(entry));
        if (match == LIST_MATCH) {
            break;
        }
    }
    return match;
}

int platform_file_manager_should_case_correct_file(void)
{
    return 0;
}

int platform_file_manager_filename_contains(const char *filename, const char *expression)
{
    if (!filename || !expression) {
        return 0;
    }
    return std::string(filename).find(expression) != std::string::npos ? 1 : 0;
}

int platform_file_manager_compare_filename(const char *a, const char *b)
{
    std::string left = a ? a : "";
    std::string right = b ? b : "";
    std::transform(left.begin(), left.end(), left.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    std::transform(right.begin(), right.end(), right.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return left < right ? -1 : (left > right ? 1 : 0);
}

int platform_file_manager_compare_filename_prefix(const char *filename, const char *prefix, int prefix_len)
{
    std::string left = filename ? filename : "";
    std::string right = prefix ? prefix : "";
    if (prefix_len >= 0 && prefix_len < static_cast<int>(right.size())) {
        right.resize(static_cast<size_t>(prefix_len));
    }
    if (left.size() > right.size()) {
        left.resize(right.size());
    }
    return platform_file_manager_compare_filename(left.c_str(), right.c_str());
}

FILE *platform_file_manager_open_file(const char *filename, const char *mode)
{
    return std::fopen(resolve_asset_marker_path(filename).string().c_str(), mode);
}

FILE *platform_file_manager_open_asset(const char *asset, const char *mode)
{
    return file_open_asset(asset, mode);
}

int platform_file_manager_close_file(FILE *stream)
{
    return std::fclose(stream) == 0;
}

int platform_file_manager_remove_file(const char *filename)
{
    return file_remove(filename);
}

int platform_file_manager_create_directory(const char *name, const char *location, int overwrite)
{
    (void) overwrite;
    std::error_code error;
    std::filesystem::path path = name ? name : "";
    if (location && *location && path.is_relative()) {
        path = std::filesystem::path(location) / path;
    }
    std::filesystem::create_directories(path, error);
    return error ? 0 : 1;
}

int platform_file_manager_copy_file(const char *src, const char *dst)
{
    std::error_code error;
    std::filesystem::copy_file(resolve_asset_marker_path(src), resolve_asset_marker_path(dst),
        std::filesystem::copy_options::overwrite_existing, error);
    return error ? 0 : 1;
}

int platform_file_manager_copy_directory(const char *src, const char *dst, int overwrite_files)
{
    std::error_code error;
    const auto options = overwrite_files ? std::filesystem::copy_options::overwrite_existing :
        std::filesystem::copy_options::none;
    std::filesystem::copy(resolve_asset_marker_path(src), resolve_asset_marker_path(dst),
        options | std::filesystem::copy_options::recursive, error);
    return error ? 0 : 1;
}

int platform_file_manager_remove_directory(const char *path)
{
    std::error_code error;
    std::filesystem::remove_all(resolve_asset_marker_path(path), error);
    return error ? 0 : 1;
}

namespace mod_manager {

void set_mod_name(std::string_view mod_name)
{
    (void) mod_name;
}

bool load_mod_list()
{
    return true;
}

const std::string &failure_reason()
{
    static const std::string value;
    return value;
}

const std::string &mod_name()
{
    static const std::string value = "Augustus";
    return value;
}

const std::string &mod_path()
{
    static const std::string value = "Mods/Augustus/";
    return value;
}

const std::string &graphics_path()
{
    return g_augustus_graphics_path;
}

const std::string &augustus_graphics_path()
{
    return g_augustus_graphics_path;
}

const std::string &julius_graphics_path()
{
    return g_julius_graphics_path;
}

const std::vector<std::string> &mod_names()
{
    static const std::vector<std::string> values = { "Julius", "Augustus" };
    return values;
}

const std::vector<std::string> &mod_paths()
{
    static const std::vector<std::string> values = { "Mods/Julius/", "Mods/Augustus/" };
    return values;
}

const std::vector<std::string> &graphics_paths()
{
    static std::vector<std::string> values;
    values = { g_julius_graphics_path, g_augustus_graphics_path };
    return values;
}

bool validate_mod_path()
{
    return true;
}

bool validate_graphics_path()
{
    return true;
}

}

CrashContextScope::CrashContextScope(
    const char *stage,
    const char *context,
    crash_context_log_callback callback,
    void *userdata)
{
    (void) stage;
    (void) context;
    (void) callback;
    (void) userdata;
}

CrashContextScope::~CrashContextScope()
{
}

void CrashContextScope::set_context(const char *context)
{
    (void) context;
}

void CrashContextScope::set_callback(crash_context_log_callback callback, void *userdata)
{
    (void) callback;
    (void) userdata;
}

void crash_context_clear(void)
{
}

int crash_context_push_scope(
    const char *stage,
    const char *context,
    crash_context_log_callback callback,
    void *userdata)
{
    (void) stage;
    (void) context;
    (void) callback;
    (void) userdata;
    return 1;
}

void crash_context_pop_scope(int token)
{
    (void) token;
}

void crash_context_update_scope_context(int token, const char *context)
{
    (void) token;
    (void) context;
}

void crash_context_update_scope_callback(int token, crash_context_log_callback callback, void *userdata)
{
    (void) token;
    (void) callback;
    (void) userdata;
}

void crash_context_set_stage(const char *stage, const char *context)
{
    (void) stage;
    (void) context;
}

void crash_context_log_current(void)
{
}

void error_context_log_current(void)
{
}

void error_context_flush_report_counts(void)
{
}

void error_context_report_info(const char *message, const char *detail)
{
    log_info(message, detail, 0);
}

void error_context_report_warning(const char *message, const char *detail)
{
    log_warning(message, detail, 0);
}

void error_context_report_error(const char *message, const char *detail)
{
    log_error(message, detail, 0);
}

void error_context_report_fatal_error_dialog(const char *title, const char *message, const char *detail)
{
    log_error(title ? title : message, detail, 0);
}

void crash_context_report_error(const char *message, const char *detail)
{
    log_error(message, detail, 0);
}

void crash_context_report_error_dialog(const char *title, const char *message, const char *detail)
{
    log_error(title ? title : message, detail, 0);
}
