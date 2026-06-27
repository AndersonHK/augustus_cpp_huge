#include "assets/assets.h"
#include "building/menu.h"
#include "building/monument.h"
#include "core/image.h"
#include "figure/figure.h"
#include "figure/image.h"
#include "graphics/font.h"
#include "graphics/screen.h"
#include "scenario/allowed_building.h"
#include "scenario/map.h"

namespace {

image g_dummy_image = {};
font_definition g_dummy_font = {};

} // namespace

void building_menu_invalidate_catalog(void)
{
}

void building_monument_reset_runtime_bridge(void)
{
}

int building_is_house(building_type type)
{
    (void) type;
    return 0;
}

int scenario_map_has_fishing_points(void)
{
    return 0;
}

int scenario_allowed_building(building_type type)
{
    (void) type;
    return 0;
}

int screen_width(void)
{
    return 640;
}

int screen_height(void)
{
    return 480;
}

void platform_screen_show_error_message_box(const char *title, const char *message)
{
    (void) title;
    (void) message;
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

unsigned int Figure::id() const
{
    return 0;
}

int figure_image_normalize_direction(int direction)
{
    direction %= 8;
    return direction < 0 ? direction + 8 : direction;
}

int figure_image_corpse_offset(Figure *f)
{
    (void) f;
    return 0;
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
