#include "startup/startup_parser.h"

#include "assets/assets.h"
#include "building/building_type_registry.h"
#include "building/building_type_startup_bridge.h"
#include "building/properties.h"
#include "core/config.h"
#include "figure/figure_type_registry.h"
#include "figure/formation_type.h"
#include "figure/unit_type.h"
#include "game/defines.h"
#include "game/mod_manager.h"
#include "game/resource.h"
#include "graphics/image.h"
#include "graphics/renderer.h"
#include "scenario/property.h"
#include "translation/translation.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <filesystem>
#include <sstream>
#include <string>
#include <system_error>
#include <vector>

namespace startup_parser {
namespace {

struct StartupAtlas {
    image_atlas_data data = {};
    std::vector<std::vector<color_t>> pixel_pages;
    std::vector<color_t *> page_pointers;
    std::vector<int> widths;
    std::vector<int> heights;
};

std::array<StartupAtlas, ATLAS_MAX> g_startup_atlases;
graphics_renderer_interface g_startup_renderer = {};
int g_startup_renderer_installed = 0;
int g_next_startup_image_handle = 1;
constexpr int STARTUP_MAX_PACKED_IMAGE_SIZE = 64000;

bool has_case_insensitive_extension(const std::filesystem::path &path, const char *extension)
{
    std::string actual = path.extension().string();
    std::string expected = extension ? extension : "";
    std::transform(actual.begin(), actual.end(), actual.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    std::transform(expected.begin(), expected.end(), expected.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return actual == expected;
}

void reset_startup_atlas(StartupAtlas &atlas, atlas_type type)
{
    atlas.pixel_pages.clear();
    atlas.page_pointers.clear();
    atlas.widths.clear();
    atlas.heights.clear();
    atlas.data = {};
    atlas.data.type = type;
}

void startup_renderer_get_max_image_size(int *width, int *height)
{
    if (width) {
        *width = 4096;
    }
    if (height) {
        *height = 4096;
    }
}

const image_atlas_data *startup_renderer_prepare_image_atlas(
    atlas_type type, int num_images, int last_width, int last_height)
{
    if (type < ATLAS_FIRST || type >= ATLAS_MAX || num_images <= 0 || last_width <= 0 || last_height <= 0) {
        return nullptr;
    }

    StartupAtlas &atlas = g_startup_atlases[type];
    reset_startup_atlas(atlas, type);
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

int startup_renderer_create_image_atlas(const image_atlas_data *atlas_data, int delete_buffers)
{
    if (!atlas_data || atlas_data->type < ATLAS_FIRST || atlas_data->type >= ATLAS_MAX) {
        return 0;
    }
    if (delete_buffers) {
        reset_startup_atlas(g_startup_atlases[atlas_data->type], atlas_data->type);
    }
    return 1;
}

const image_atlas_data *startup_renderer_get_image_atlas(atlas_type type)
{
    if (type < ATLAS_FIRST || type >= ATLAS_MAX || g_startup_atlases[type].data.num_images <= 0) {
        return nullptr;
    }
    return &g_startup_atlases[type].data;
}

int startup_renderer_has_image_atlas(atlas_type type)
{
    return startup_renderer_get_image_atlas(type) ? 1 : 0;
}

void startup_renderer_free_image_atlas(atlas_type type)
{
    if (type >= ATLAS_FIRST && type < ATLAS_MAX) {
        reset_startup_atlas(g_startup_atlases[type], type);
    }
}

void startup_renderer_upload_image_resource(image *img, const color_t *pixels, int width, int height)
{
    (void) pixels;
    (void) width;
    (void) height;
    if (img) {
        img->resource_handle = g_next_startup_image_handle++;
    }
}

void startup_renderer_release_image_resource(image *img)
{
    if (img) {
        img->resource_handle = 0;
    }
}

void startup_renderer_load_unpacked_image(const image *img, const color_t *pixels)
{
    (void) img;
    (void) pixels;
}

void startup_renderer_free_unpacked_image(const image *img)
{
    (void) img;
}

int startup_renderer_should_pack_image(int width, int height)
{
    return width * height < STARTUP_MAX_PACKED_IMAGE_SIZE;
}

void install_startup_renderer_if_needed()
{
    if (graphics_renderer()) {
        return;
    }
    if (!g_startup_renderer_installed) {
        g_startup_renderer.get_max_image_size = startup_renderer_get_max_image_size;
        g_startup_renderer.prepare_image_atlas = startup_renderer_prepare_image_atlas;
        g_startup_renderer.create_image_atlas = startup_renderer_create_image_atlas;
        g_startup_renderer.get_image_atlas = startup_renderer_get_image_atlas;
        g_startup_renderer.has_image_atlas = startup_renderer_has_image_atlas;
        g_startup_renderer.free_image_atlas = startup_renderer_free_image_atlas;
        g_startup_renderer.upload_image_resource = startup_renderer_upload_image_resource;
        g_startup_renderer.release_image_resource = startup_renderer_release_image_resource;
        g_startup_renderer.load_unpacked_image = startup_renderer_load_unpacked_image;
        g_startup_renderer.free_unpacked_image = startup_renderer_free_unpacked_image;
        g_startup_renderer.should_pack_image = startup_renderer_should_pack_image;
        g_startup_renderer_installed = 1;
    }
    graphics_renderer_set_interface(&g_startup_renderer);
}

int count_xml_files(const std::filesystem::path &directory, std::string &failure)
{
    std::error_code error;
    if (!std::filesystem::is_directory(directory, error)) {
        failure = "Missing XML directory: " + directory.string();
        return -1;
    }

    int count = 0;
    for (const std::filesystem::directory_entry &entry : std::filesystem::directory_iterator(directory, error)) {
        if (error) {
            failure = "Unable to enumerate XML directory: " + directory.string();
            return -1;
        }
        if (entry.is_regular_file(error) && has_case_insensitive_extension(entry.path(), ".xml")) {
            ++count;
        }
    }
    return count;
}

void append_step(StartupParseResult &result, const char *label, int succeeded, const std::string &detail = std::string())
{
    StartupParseStep step;
    step.label = label ? label : "";
    step.succeeded = succeeded;
    step.detail = detail;
    result.steps.push_back(step);
}

int fail_step(StartupParseResult &result, const char *label, const std::string &message)
{
    append_step(result, label, 0, message);
    result.failure_step = label ? label : "";
    result.failure_message = message;
    return 0;
}

int run_step(StartupParseResult &result, const char *label, int (*step)(), const char *failure_reason = nullptr)
{
    if (step()) {
        append_step(result, label, 1);
        return 1;
    }

    const std::string detail = failure_reason && *failure_reason ? failure_reason : "";
    return fail_step(result, label, detail);
}

int load_resources(StartupParseResult &result)
{
    const std::filesystem::path resource_path = std::filesystem::path(mod_manager::mod_path()) / "Resources";
    std::string failure;
    const int expected_resources = count_xml_files(resource_path, failure);
    if (expected_resources <= 0) {
        if (failure.empty()) {
            failure = "No resource XML files found in: " + resource_path.string();
        }
        return fail_step(result, "Resources", failure);
    }

    resource_init();
    const int loaded_resources = resource_loaded_count();
    if (loaded_resources != expected_resources) {
        std::ostringstream detail;
        detail << "Loaded " << loaded_resources << " resources, expected "
            << expected_resources << " XML files from " << resource_path.string() << ".";
        return fail_step(result, "Resources", detail.str());
    }

    result.definitions.resource_definitions = loaded_resources;
    std::ostringstream detail;
    detail << loaded_resources << " definitions";
    append_step(result, "Resources", 1, detail.str());
    return 1;
}

std::string graphics_failure_detail()
{
    const char *asset_failure_reason = assets_get_failure_reason();
    if (asset_failure_reason && *asset_failure_reason) {
        return asset_failure_reason;
    }
    return "Failed to load startup graphics required by BuildingType validation.";
}

int prepare_building_type_graphics(StartupParseResult &result)
{
    install_startup_renderer_if_needed();
    if (!Image::load_climate(CLIMATE_CENTRAL, 0, 1, 1, 1)) {
        return fail_step(result, "BuildingType graphics preparation", graphics_failure_detail());
    }
    if (!Image::load_climate(CLIMATE_NORTHERN, 0, 1, 1, 1)) {
        return fail_step(result, "BuildingType graphics preparation", graphics_failure_detail());
    }
    if (!Image::load_climate(CLIMATE_DESERT, 0, 1, 1, 1)) {
        return fail_step(result, "BuildingType graphics preparation", graphics_failure_detail());
    }
    if (!Image::load_climate(CLIMATE_CENTRAL, 0, 1, 0, 0)) {
        return fail_step(result, "BuildingType graphics preparation", graphics_failure_detail());
    }

    result.definitions.building_type_graphics_prepared = 1;
    append_step(result, "BuildingType graphics preparation", 1, "climate graphics loaded");
    return 1;
}

} // namespace

StartupParseResult parse_startup_definitions(const StartupParseRequest &request)
{
    StartupParseResult result;

    config_load();

    if (request.validate_mod_layout && !building_type_startup_bridge_validate_mod()) {
        fail_step(
            result,
            "mod data",
            std::string("Selected mod data is missing. Expected BuildingType folder: ") +
                building_type_startup_bridge_get_building_type_path());
        return result;
    }

    if (request.load_localization && !run_step(result, "localization", []() { return lang_load(0); })) {
        return result;
    }

    model_reset();
    if (!load_resources(result)) {
        return result;
    }
    if (!run_step(result, "game defines", game_defines_load, game_defines_get_failure_reason())) {
        return result;
    }

    building_properties_init();
    figure_type_registry_reset();
    unit_type_registry_reset();
    formation_type_registry_reset();

    if (!run_step(result, "FigureType definitions", figure_type_registry_load, figure_type_registry_get_failure_reason())) {
        return result;
    }
    if (!run_step(result, "UnitType definitions", unit_type_registry_load, unit_type_registry_get_failure_reason())) {
        return result;
    }
    if (!run_step(result, "FormationType definitions", formation_type_registry_load, formation_type_registry_get_failure_reason())) {
        return result;
    }
    if (request.prepare_building_type_graphics && !prepare_building_type_graphics(result)) {
        return result;
    }
    if (!run_step(result, "BuildingType definitions", building_type_registry_load)) {
        return result;
    }
    if (!run_step(result, "FigureType building references", figure_type_registry_resolve_building_references,
        figure_type_registry_get_failure_reason())) {
        return result;
    }

    result.succeeded = 1;
    return result;
}

} // namespace startup_parser
