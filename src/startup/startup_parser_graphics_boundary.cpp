#ifdef STARTUP_PARSER_TEST

#include "assets/assets.h"
#include "core/image.h"
#include "graphics/font.h"
#include "graphics/renderer.h"
#include "graphics/screen.h"
#include "graphics/runtime_overlay_images.h"

#include <array>
#include <cstdint>
#include <vector>

namespace {

struct StartupAtlas {
    image_atlas_data data = {};
    std::vector<std::vector<color_t>> pixel_pages;
    std::vector<color_t *> page_pointers;
    std::vector<int> widths;
    std::vector<int> heights;
};

image g_dummy_image = {};
font_definition g_dummy_font = {};
std::array<StartupAtlas, ATLAS_MAX> g_startup_atlases;
graphics_renderer_interface g_startup_renderer = {};
int g_startup_renderer_installed = 0;
int g_next_startup_image_handle = 1;
std::vector<std::uint64_t> g_startup_image_fingerprints(1, 0);
constexpr int STARTUP_MAX_PACKED_IMAGE_SIZE = 64000;

std::uint64_t image_fingerprint(const color_t *pixels, int width, int height)
{
    constexpr std::uint64_t FNV_OFFSET = 14695981039346656037ull;
    constexpr std::uint64_t FNV_PRIME = 1099511628211ull;
    std::uint64_t fingerprint = FNV_OFFSET;
    const std::size_t byte_count = static_cast<std::size_t>(width) * height * sizeof(color_t);
    const auto *bytes = reinterpret_cast<const unsigned char *>(pixels);
    for (std::size_t index = 0; index < byte_count; ++index) {
        fingerprint = (fingerprint ^ bytes[index]) * FNV_PRIME;
    }
    fingerprint = (fingerprint ^ static_cast<std::uint64_t>(width)) * FNV_PRIME;
    return (fingerprint ^ static_cast<std::uint64_t>(height)) * FNV_PRIME;
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
    if (!img || !pixels || width <= 0 || height <= 0) return;
    img->resource_handle = g_next_startup_image_handle++;
    if (g_startup_image_fingerprints.size() <= static_cast<std::size_t>(img->resource_handle)) {
        g_startup_image_fingerprints.resize(static_cast<std::size_t>(img->resource_handle) + 1, 0);
    }
    g_startup_image_fingerprints[static_cast<std::size_t>(img->resource_handle)] = image_fingerprint(pixels, width, height);
}

void startup_renderer_release_image_resource(image *img)
{
    if (img) {
        if (img->resource_handle > 0 && static_cast<std::size_t>(img->resource_handle) < g_startup_image_fingerprints.size()) {
            g_startup_image_fingerprints[static_cast<std::size_t>(img->resource_handle)] = 0;
        }
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

} // namespace

namespace startup_parser {

std::uint64_t image_resource_fingerprint(image_handle handle)
{
    return handle > 0 && static_cast<std::size_t>(handle) < g_startup_image_fingerprints.size() ?
        g_startup_image_fingerprints[static_cast<std::size_t>(handle)] : 0;
}

void install_graphics_validation_renderer()
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

} // namespace startup_parser

int screen_width(void)
{
    return 640;
}

int screen_height(void)
{
    return 480;
}

void assets_load_unpacked_asset(int image_id)
{
    (void) image_id;
}

int assets_lookup_image_id(asset_id id)
{
    (void) id;
    return 0;
}

int image_is_external(const image *img)
{
    (void) img;
    return 0;
}

const font_definition *font_definition_for(font_t font)
{
    g_dummy_font.font = font;
    g_dummy_font.line_height = 1;
    g_dummy_font.metric_scale_percentage = 100;
    return &g_dummy_font;
}

int image_load_climate(int climate_id, int is_editor, int force_reload, int keep_atlas_buffers, int extract_legacy_graphics)
{
    (void) climate_id;
    (void) is_editor;
    (void) force_reload;
    (void) keep_atlas_buffers;
    (void) extract_legacy_graphics;
    return 0;
}

int image_load_fonts(encoding_type encoding)
{
    (void) encoding;
    return 0;
}

int image_load_enemy(int enemy_id)
{
    (void) enemy_id;
    return 0;
}

void image_load_external_data(const image *img)
{
    (void) img;
}

int image_get_external_dimensions(const image *img, int *width, int *height)
{
    if (!img || !width || !height) {
        return 0;
    }
    *width = img->width;
    *height = img->height;
    return *width > 0 && *height > 0;
}

void image_crop(image *img, const color_t *pixels)
{
    (void) img;
    (void) pixels;
}

int image_group(int group)
{
    (void) group;
    return 0;
}

const image *image_get(int id)
{
    (void) id;
    return &g_dummy_image;
}

const image *image_letter(int letter_id)
{
    (void) letter_id;
    return &g_dummy_image;
}

const image *image_get_enemy(int id)
{
    (void) id;
    return &g_dummy_image;
}

const Image *runtime_footprint_overlay_image()
{
    return nullptr;
}

#endif // STARTUP_PARSER_TEST
