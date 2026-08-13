#include "building/building.h"
#include "building/BuildingFoundation.h"
#include "building/building_type_registry_internal.h"
#include "building/industry.h"
#include "graphics/graphics.h"
#include "graphics/image.h"
#include "map/building.h"

#include "minimap.h"

#include "assets/assets.h"
#include "building/building_record.h"
#include "building/monument.h"
#include "city/view.h"
#include "core/calc.h"
#include "figure/formation.h"
#include "graphics/renderer.h"
#include "map/figure.h"
#include "map/grid.h"
#include "map/property.h"
#include "map/random.h"
#include "map/terrain.h"


#include <string.h>
#include <stdlib.h>
#include <vector>

enum {
    SAVE_MINIMAP_PREVIEW_MAGIC = 0x56504d4d,
    SAVE_MINIMAP_PREVIEW_VERSION = 1,
    SAVE_MINIMAP_PLACEHOLDER_WIDTH = 160,
    SAVE_MINIMAP_PLACEHOLDER_HEIGHT = 160
};

enum {
    FIGURE_COLOR_NONE = 0,
    FIGURE_COLOR_SOLDIER = 1,
    FIGURE_COLOR_SELECTED_SOLDIER = 2,
    FIGURE_COLOR_ENEMY = 3,
    FIGURE_COLOR_WOLF = 4,
    FIGURE_COLOR_TRADE_CARAVAN = 5,
    FIGURE_COLOR_TRADE_SHIP = 6,
};

typedef struct {
    color_t left;
    color_t right;
} tile_color;

typedef struct {
    color_t enemy;
    tile_color water[4];
    tile_color tree[4];
    tile_color rock[4];
    tile_color meadow[4];
    tile_color grass[8];
    tile_color road;
    tile_color highway;
} tile_color_climate_variants;

typedef struct {
    tile_color edges;
    tile_color center;
} building_tile_color;
static void get_viewport(int *x, int *y, int *width, int *height);
static unsigned int runtime_building_id_at(int grid_offset);
static int runtime_is_draw_tile(int grid_offset);
static int runtime_tile_size(int grid_offset);

static minimap_functions default_functions = {
    scenario_property_climate,
    nullptr,
    {map_grid_width, map_grid_height},
    {
        map_figure_foreach_until,
        map_terrain_get,
        runtime_building_id_at,
        runtime_is_draw_tile,
        runtime_tile_size,
        map_random_get
    },
    get_viewport
};

static unsigned int runtime_building_id_at(int grid_offset)
{
    return map_building_exists_at(grid_offset) ? map_building_at(grid_offset).id : 0;
}

static int runtime_is_draw_tile(int grid_offset)
{
    if (map_building_exists_at(grid_offset)) {
        Building &building = map_building_at(grid_offset);
        if (building.Foundation && building.Foundation->state().is_published() &&
            building.Foundation->contains_grid_offset(grid_offset)) {
            return 1;
        }
    }
    return map_property_is_draw_tile(grid_offset);
}

static int runtime_tile_size(int grid_offset)
{
    if (map_building_exists_at(grid_offset)) {
        Building &building = map_building_at(grid_offset);
        if (building.Foundation && building.Foundation->state().is_published() &&
            building.Foundation->contains_grid_offset(grid_offset)) {
            // Live foundations are rendered one exact bound cell at a time;
            // serialized scenario previews retain their square tile-size DTO.
            return 1;
        }
    }
    return map_property_legacy_multi_tile_size(grid_offset);
}

static const tile_color_climate_variants CLIMATE_VARIANTS[3] = {
    // central
    {
        .enemy = COLOR_MINIMAP_ENEMY_CENTRAL,
        .water = {{0xff394a7b, 0xff31427b}, {0xff394a7b, 0xff314273}, {0xff313973, 0xff314273}, {0xff31427b, 0xff394a7b}},
        .tree = {{0xff6b8431, 0xff102108}, {0xff103908, 0xff737b29}, {0xff103108, 0xff526b21}, {0xff737b31, 0xff084a10}},
        .rock = {{0xff948484, 0xff635a4a}, {0xffa59c94, 0xffb5ada5}, {0xffb5ada5, 0xff8c8484}, {0xff635a4a, 0xffa59c94}},
        .meadow = {{0xffd6bd63, 0xff9c8c39}, {0xff948c39, 0xffd6bd63}, {0xffd6bd63, 0xff9c9439}, {0xff848431, 0xffada54a}},
        .grass = {
            {0xff6b8c31, 0xff6b7b29}, {0xff738431, 0xff6b7b29}, {0xff6b7329, 0xff7b8c39}, {0xff527b29, 0xff6b7321},
            {0xff6b8431, 0xff737b31}, {0xff6b7b31, 0xff737b29}, {0xff636b18, 0xff526b21}, {0xff737b31, 0xff737b29}
        },
        .road = {0xff736b63, 0xff4a3121},
        .highway = {0xffb3aba3, 0xff9a8171},
    },
    // northern
    {
        .enemy = COLOR_MINIMAP_ENEMY_NORTHERN,
        .water = {{0xff394a7b, 0xff31427b}, {0xff394a7b, 0xff314273}, {0xff313973, 0xff314273}, {0xff31427b, 0xff394a7b}},
        .tree = {{0xff527b31, 0xff082108}, {0xff083908, 0xff5a7329}, {0xff082908, 0xff316b21}, {0xff527b29, 0xff084a21}},
        .rock = {{0xff8c8484, 0xff5a5252}, {0xff9c9c94, 0xffa5a5a5}, {0xffa5a5a5, 0xff848484}, {0xff5a5252, 0xff9c9c94}},
        .meadow = {{0xff427318, 0xff8c9442}, {0xffb5ad4a, 0xff738c39}, {0xff8c8c39, 0xff6b7b29}, {0xff527331, 0xff5a8442}},
        .grass = {
            {0xff4a8431, 0xff4a7329}, {0xff527b29, 0xff4a7329}, {0xff526b29, 0xff5a8439}, {0xff397321, 0xff4a6b21},
            {0xff527b31, 0xff5a7331}, {0xff4a7329, 0xff5a7329}, {0xff4a6b18, 0xff316b21}, {0xff527b29, 0xff527329}
        },
        .road = {0xff736b63, 0xff4a3121},
        .highway = {0xffb3aba3, 0xff9a8171},
    },
    // desert
    {
        .enemy = COLOR_MINIMAP_ENEMY_DESERT,
        .water = {{0xff4a84c6, 0xff4a7bc6}, {0xff4a84c6, 0xff4a7bc6}, {0xff4a84c6, 0xff5284c6}, {0xff4a7bbd, 0xff4a7bc6}},
        .tree = {{0xffa59c7b, 0xff6b7b18}, {0xff214210, 0xffada573}, {0xff526b21, 0xffcec6a5}, {0xffa59c7b, 0xff316321}},
        .rock = {{0xffa59494, 0xff736352}, {0xffa59c94, 0xffb5ada5}, {0xffb5ada5, 0xff8c847b}, {0xff736352, 0xffbdada5}},
        .meadow = {{0xff739c31, 0xff9cbd52}, {0xff7bb529, 0xff63ad21}, {0xff9cbd52, 0xff8c944a}, {0xff7ba539, 0xff739c31}},
        .grass = {
            {0xffbdbd9c, 0xffb5b594}, {0xffc6bda5, 0xffbdbda5}, {0xffbdbd9c, 0xffc6c6ad}, {0xffd6cead, 0xffc6bd9c},
            {0xffa59c7b, 0xffbdb594}, {0xffcecead, 0xffb5ad94}, {0xffc6c6a5, 0xffdedebd}, {0xffcecead, 0xffd6d6b5}
        },
        .road = {0xff6b5a52, 0xff4a4239},
        .highway = {0xffb3aba3, 0xff9a8171},
    }
};

static struct {
    color_t soldier;
    color_t selected_soldier;
    color_t wolf;
    color_t trade_caravan;
    color_t trade_ship;
    tile_color wall;
    tile_color aqueduct;
    building_tile_color water_structure;
    building_tile_color house;
    building_tile_color building;
    building_tile_color monument;
    building_tile_color farm;
    building_tile_color industry;
    building_tile_color aesthetics;
    building_tile_color military;
} minimap_colors = {
    .soldier = COLOR_MINIMAP_SOLDIER,
    .selected_soldier = COLOR_MINIMAP_SELECTED_SOLDIER,
    .wolf = COLOR_MINIMAP_WOLF,
    .trade_caravan = COLOR_MINIMAP_TRADE_CARAVAN,
    .trade_ship = COLOR_MINIMAP_TRADE_SHIP,
    .wall = {0xffd6d3c6, 0xfff7f3de},
    .aqueduct = {0xff84baff, 0xff5282bd},
    .water_structure = {.edges = {0xff5282bd, 0xff5282bd}, .center = {0xff84baff, 0xff84baff} },
    .house = {.edges = {0xffffb28c, 0xffd65110}, .center = {0xffef824a, 0xffffa273} },
    .building = {.edges = {0xfffffbde, 0xffefd34a}, .center = {0xfffff3c6, 0xffffebb5} },
    .monument = {.edges = {0xfff5deff, 0xffb84aef}, .center = {0xffe9c6ff, 0xffdfb5ff} },
    .farm = {.edges = {0xff81ef4a, 0xffe8ffde}, .center = {0xffdcffc6, 0xffd5ffb5} },
    .industry = {.edges = {0xff6b2900, 0xffb6896d}, .center = {0xffb2602e, 0xff9d3c01} },
    .aesthetics = {.edges = {0xff019d7a, 0xffc4e1da}, .center = {0xff81d5c2, 0xff1ac6a0} },
    .military = {.edges = {0xff4e4e4e, 0xffb6b8b8}, .center = {0xff8c8c8c, 0xff6d6e6e} }
};

static struct {
    struct {
        int x;
        int y;
        int width;
        int height;
        int width_tiles;
        int height_tiles;
    } screen;
    struct {
        int x;
        int y;
        int width;
        int height;
        int offset_x;
        int offset_y;
        float scale;
        float max_scale;
    } minimap;
    struct {
        int stride;
        color_t *buffer;
    } cache;
    const minimap_functions *functions;
    struct {
        int x;
        int y;
        int grid_offset;
    } mouse;
    int refresh_requested;
    struct {
        int x;
        int y;
        int width;
        int height;
    } viewport;
} data;

typedef struct {
    const minimap_functions *functions;
    int x;
    int y;
    int width;
    int height;
    int stride;
    color_t *buffer;
    const tile_color_climate_variants *climate;
} minimap_render_context;

static struct {
    int width;
    int height;
    int has_image;
} preview;

static minimap_render_context *active_render_context;

struct ActiveMinimapRenderContext {
    explicit ActiveMinimapRenderContext(minimap_render_context *context)
        : previous(active_render_context)
    {
        active_render_context = context;
    }

    ~ActiveMinimapRenderContext()
    {
        active_render_context = previous;
    }

    minimap_render_context *previous;
};

static minimap_render_context make_live_render_context(void)
{
    const minimap_functions *functions = data.functions ? data.functions : &default_functions;
    minimap_render_context context = {
        functions,
        data.minimap.x,
        data.minimap.y,
        data.minimap.width,
        data.minimap.height,
        data.cache.stride,
        data.cache.buffer,
        &CLIMATE_VARIANTS[functions->climate()]
    };
    return context;
}

static void get_viewport(int *x, int *y, int *width, int *height)
{
    city_view_get_camera(x, y);
    city_view_get_viewport_size_tiles(width, height);
}

void widget_minimap_invalidate(void)
{
    data.refresh_requested = 1;
}

void widget_minimap_restore_default_functions(void)
{
    data.functions = &default_functions;
    widget_minimap_invalidate();
}

static void foreach_map_tile(map_callback *callback)
{
    if (!active_render_context) {
        return;
    }
    city_view_foreach_minimap_tile(0, 0, active_render_context->x, active_render_context->y,
        active_render_context->width, active_render_context->height, callback);
}

static void setup_minimap(int x_offset, int y_offset, int width, int height)
{
    data.screen.x = x_offset;
    data.screen.y = y_offset;
    data.screen.width = width;
    data.screen.height = height;
    data.screen.width_tiles = width / 2;
    data.screen.height_tiles = height;

    float max_scale_width = data.minimap.width * 2 / (float) data.screen.width;
    float max_scale_height = data.minimap.height / (float) data.screen.height;

    data.minimap.max_scale = max_scale_width > max_scale_height ? max_scale_width : max_scale_height;
}

static void position_minimap(int x_offset, int y_offset, int width, int height)
{
    setup_minimap(x_offset, y_offset, width, height);

    data.functions->viewport(&data.viewport.x, &data.viewport.y, &data.viewport.width, &data.viewport.height);

    float scale_width = data.viewport.width / (float) data.screen.width_tiles;
    float scale_height = data.viewport.height / (float) data.screen.height_tiles;

    data.minimap.scale = scale_width > scale_height ? scale_width : scale_height;

    if (data.minimap.scale > data.minimap.max_scale) {
        data.minimap.scale = data.minimap.max_scale;
    }

    if (data.minimap.scale < 1.0f) {
        data.minimap.scale = SCALE_NONE;
    }

    int adjusted_camera_x = (data.viewport.x - data.minimap.x) * 2;
    int adjusted_camera_y = data.viewport.y - data.minimap.y;
    int minimap_width_pixels = (int) ((data.minimap.width * 2) / data.minimap.scale);
    int minimap_height_pixels = (int) (data.minimap.height / data.minimap.scale);

    data.minimap.offset_x = (minimap_width_pixels - data.screen.width) / 2;
    data.minimap.offset_y = (minimap_height_pixels - data.screen.height) / 2;

    if (minimap_width_pixels > data.screen.width) {
        if (data.minimap.offset_x > adjusted_camera_x) {
            data.minimap.offset_x = adjusted_camera_x;
            if (data.minimap.offset_x < 0) {
                data.minimap.offset_x = 0;
            }
        } else if ((adjusted_camera_x + data.viewport.width * 2) / data.minimap.scale >
            data.minimap.offset_x + data.screen.width) {
            data.minimap.offset_x = (int) ((adjusted_camera_x + data.viewport.width * 2) / data.minimap.scale -
                data.screen.width);
        }
    }
    if (minimap_height_pixels > data.screen.height) {
        if (data.minimap.offset_y > adjusted_camera_y) {
            data.minimap.offset_y = adjusted_camera_y;
            if (data.minimap.offset_y < 0) {
                data.minimap.offset_y = 0;
            }
        } else if ((adjusted_camera_y + data.viewport.height) / data.minimap.scale >
            data.minimap.offset_y + data.screen.height) {
            data.minimap.offset_y = (int) ((adjusted_camera_y + data.viewport.height) / data.minimap.scale -
                data.screen.height);
        }
    }
}

static int has_figure_color(Figure *f)
{
    int type = f->type;
    if (f->is_legion()) {
        return formation_get_selected() == f->formation_id ?
            FIGURE_COLOR_SELECTED_SOLDIER : FIGURE_COLOR_SOLDIER;
    }
    if (f->is_enemy()) {
        return FIGURE_COLOR_ENEMY;
    }
    if (f->type == FIGURE_INDIGENOUS_NATIVE &&
        f->action_state == FIGURE_ACTION_159_NATIVE_ATTACKING) {
        return FIGURE_COLOR_ENEMY;
    }
    if (type == FIGURE_WOLF) {
        return FIGURE_COLOR_WOLF;
    }

    if (type == FIGURE_TRADE_CARAVAN || type == FIGURE_NATIVE_TRADER) {
        return FIGURE_COLOR_TRADE_CARAVAN;
    }
    if (type == FIGURE_TRADE_SHIP) {
        return FIGURE_COLOR_TRADE_SHIP;
    }

    return FIGURE_COLOR_NONE;
}

static inline void draw_pixel(int x, int y, color_t color)
{
    if (x < 0 || y < 0 || x >= active_render_context->width * 2 || y >= active_render_context->height) {
        return;
    }
    active_render_context->buffer[y * active_render_context->stride + x] = color;
}

static inline void draw_tile(int x_offset, int y_offset, const tile_color *colors)
{
    draw_pixel(x_offset, y_offset, colors->left);
    draw_pixel(x_offset + 1, y_offset, colors->right);
}

static int draw_figure(int x_view, int y_view, int grid_offset)
{
    const minimap_functions *functions = active_render_context->functions;
    if (!functions->offset.figure) {
        return 0;
    }
    int color_type = functions->offset.figure(grid_offset, has_figure_color);
    if (color_type == FIGURE_COLOR_NONE) {
        return 0;
    }
    color_t color = minimap_colors.wolf;
    if (color_type == FIGURE_COLOR_SOLDIER) {
        color = minimap_colors.soldier;
    } else if (color_type == FIGURE_COLOR_SELECTED_SOLDIER) {
        color = minimap_colors.selected_soldier;
    } else if (color_type == FIGURE_COLOR_ENEMY) {
        color = active_render_context->climate->enemy;
    } else if (color_type == FIGURE_COLOR_TRADE_CARAVAN) {
        color = minimap_colors.trade_caravan;
    } else if (color_type == FIGURE_COLOR_TRADE_SHIP) {
        color = minimap_colors.trade_ship;
    }

    draw_pixel(x_view, y_view, color);
    draw_pixel(x_view + 1, y_view, color);
    return 1;
}

static int building_is_industry(const building_type_registry_impl::BuildingType *type)
{
    return building_is_raw_resource_producer(type) || building_is_workshop(type) ||
        type->attr_is("wharf");
}

static int building_is_military(building_type type)
{
    return building_is_fort(type) || building_type_registry_impl::type_attr_is_any(type,
        {"fort_ground", "barracks", "military_academy", "mess_hall", "tower", "watchtower", "gatehouse", "palisade_gate"});
}

static int building_is_aesthetic(building_type type)
{
    return building_type_registry_impl::type_attr_is_any(type,
        {"small_pond", "large_pond", "pine_tree", "fir_tree", "oak_tree", "elm_tree", "fig_tree", "plum_tree",
            "palm_tree", "date_tree", "pine_path", "fir_path", "oak_path", "elm_path", "fig_path", "plum_path",
            "palm_path", "date_path", "pavilion", "goddess_statue", "senator_statue", "obelisk", "triumphal_arch", "horse_statue",
            "dolphin_fountain", "hedge_dark", "hedge_light", "looped_garden_wall", "legion_statue",
            "decorative_column", "colonnade", "garden_path", "roofed_garden_wall", "roofed_garden_wall_gate",
            "hedge_gate_dark", "hedge_gate_light", "gardens"});
}

static int building_is_water_structure(const building_type_registry_impl::BuildingType &type)
{
    return type.is_well() || type.is_fountain() || type.attr_is("reservoir");
}

static void draw_building(int x_offset, int y_offset, int grid_offset)
{
    const minimap_functions *functions = active_render_context->functions;
    if (!functions->offset.is_draw_tile(grid_offset)) {
        return;
    }

    const building_tile_color *colors = &minimap_colors.building;
    int size = functions->offset.tile_size(grid_offset);

    if (functions->offset.building_id) {
        Building *runtime_building = nullptr;
        const int building_id = functions->offset.building_id(grid_offset);
        building *b = nullptr;
        if (functions->building) {
            b = building_id ? functions->building(building_id) : nullptr;
        } else {
            runtime_building = map_building_exists_at(grid_offset) ? &map_building_at(grid_offset) : nullptr;
            if (runtime_building) {
                b = const_cast<building *>(runtime_building->record());
            }
        }

        // Palisades are drawn like walls
        if (b && building_type_registry_impl::type_attr_is(b->type, "palisade")) {
            draw_tile(x_offset, y_offset, &minimap_colors.wall);
            return;
        }

        const building_type_registry_impl::BuildingType *type_definition = nullptr;
        if (runtime_building) {
            type_definition = runtime_building->type;
        } else if (b) {
            type_definition = building_type_registry_impl::definition_for_type(b->type);
        }
        if (b && building_type_registry_impl::type_has_housing(b->type)) {
            colors = &minimap_colors.house;
        } else if (type_definition && building_is_water_structure(*type_definition)) {
            colors = &minimap_colors.water_structure;
        } else if (b && building_monument_is_monument(b)) {
            colors = &minimap_colors.monument;
        } else if (b && type_definition && type_definition->is_farm()) {
            colors = &minimap_colors.farm;
        } else if (b && type_definition && building_is_industry(type_definition)) {
            colors = &minimap_colors.industry;
        } else if (b && building_is_military(b->type)) {
            colors = &minimap_colors.military;
        } else if (b && building_is_aesthetic(b->type)) {
            colors = &minimap_colors.aesthetics;
        }
    }
    if (size == 1) {
        // The 1x1 house image is inverted for some reason
        if (colors == &minimap_colors.house) {
            draw_pixel(x_offset, y_offset, colors->center.right);
            draw_pixel(x_offset + 1, y_offset, colors->center.left);
        } else {
            draw_tile(x_offset, y_offset, &colors->edges);
        }
        return;
    }
    int width = size * 2;
    int height = width - 1;
    y_offset -= size - 1;
    int start_y = y_offset < 0 ? -y_offset : 0;
    int end_y = height / 2 + 1;

    for (int y = start_y; y < end_y; y++) {
        int x_start = height / 2 - y;
        int x_end = width - x_start - 1;
        draw_pixel(x_start + x_offset, y + y_offset, colors->edges.left);
        draw_pixel(x_end + x_offset, y + y_offset, colors->edges.right);
        if (x_start + x_offset < 0) {
            x_start = -x_offset - 1;
        }
        for (int x = x_start; x < x_end - 1; x++) {
            draw_pixel(x + x_offset + 1, y + y_offset, ((size + x + y) & 1) ?
                colors->center.left : colors->center.right);
        }
    }
    y_offset += height / 2 + 1;
    start_y = y_offset < 0 ? -y_offset : 0;
    end_y = height / 2;

    for (int y = start_y; y < end_y; y++) {
        int x_start = y + 1;
        int x_end = width - x_start - 1;
        draw_pixel(x_start + x_offset, y + y_offset, colors->edges.left);
        draw_pixel(x_end + x_offset, y + y_offset, colors->edges.right);
        if (x_start + x_offset < 0) {
            x_start = -x_offset - 1;
        }
        for (int x = x_start; x < x_end - 1; x++) {
            draw_pixel(x + x_offset + 1, y + y_offset, ((x + y) & 1) ?
                colors->center.left : colors->center.right);
        }
    }
}

static void draw_minimap_tile(int x_view, int y_view, int grid_offset)
{
    if (grid_offset < 0) {
        return;
    }

    if (draw_figure(x_view, y_view, grid_offset)) {
        return;
    }
    const minimap_functions *functions = active_render_context->functions;
    const tile_color_climate_variants *climate = active_render_context->climate;
    int terrain = functions->offset.terrain(grid_offset);

    if (terrain & TERRAIN_BUILDING && !(terrain & (TERRAIN_AQUEDUCT | TERRAIN_WALL))) {
        draw_building(x_view, y_view, grid_offset);
        return;
    }
    int rand = functions->offset.random(grid_offset);
    const tile_color *colors;
    if (terrain & TERRAIN_AQUEDUCT) {
        colors = &minimap_colors.aqueduct;
    } else if (terrain & TERRAIN_ROAD) {
        colors = &climate->road;
    } else if (terrain & TERRAIN_HIGHWAY) {
        colors = &climate->highway;
    } else if (terrain & TERRAIN_WATER) {
        colors = &climate->water[rand & 3];
    } else if (terrain & (TERRAIN_SHRUB | TERRAIN_TREE)) {
        colors = &climate->tree[rand & 3];
    } else if (terrain & (TERRAIN_ROCK | TERRAIN_ELEVATION)) {
        colors = &climate->rock[rand & 3];
    } else if (terrain & TERRAIN_WALL) {
        colors = &minimap_colors.wall;
    } else if (terrain & TERRAIN_MEADOW) {
        colors = &climate->meadow[rand & 3];
    } else if (terrain & TERRAIN_GARDEN) {
        colors = &minimap_colors.aesthetics.edges;
    } else {
        colors = &climate->grass[rand & 7];
    }
    draw_tile(x_view, y_view, colors);
}

static void draw_viewport_rectangle(void)
{
    int x_offset = (int) ((2 * (data.viewport.x - data.minimap.x) - 2 / 30) / data.minimap.scale);
    x_offset += data.screen.x - data.minimap.offset_x;
    if (x_offset < data.screen.x) {
        x_offset = data.screen.x;
    }
    if (x_offset + 2 * data.viewport.width + 4 > data.screen.x + data.screen.width) {
        x_offset -= 2;
    }
    int y_offset = (int) ((data.viewport.y - data.minimap.y + 2) / data.minimap.scale);
    y_offset += data.screen.y - data.minimap.offset_y;
    graphics_draw_rect(x_offset, y_offset,
        (int) ((data.viewport.width * 2) / data.minimap.scale + 4),
        (int) (data.viewport.height / data.minimap.scale - 4),
        COLOR_MINIMAP_VIEWPORT);
}

static void prepare_minimap_cache(void)
{
    if (data.functions->map.width() != data.minimap.width || data.functions->map.height() * 2 != data.minimap.height ||
        !graphics_renderer()->has_custom_image(CUSTOM_IMAGE_MINIMAP)) {
        data.minimap.width = data.functions->map.width();
        data.minimap.height = data.functions->map.height() * 2;
        data.minimap.x = (VIEW_X_MAX - data.minimap.width) / 2;
        data.minimap.y = (VIEW_Y_MAX - data.minimap.height) / 2;

        graphics_renderer()->create_custom_image(CUSTOM_IMAGE_MINIMAP, data.minimap.width * 2, data.minimap.height, 0);
    }
    data.cache.buffer = graphics_renderer()->get_custom_image_buffer(CUSTOM_IMAGE_MINIMAP, &data.cache.stride);
}

static void clear_minimap(minimap_render_context &context)
{
    memset(context.buffer, 0, sizeof(color_t) * context.height * context.stride);
}

static void render_minimap_to_context(minimap_render_context &context)
{
    if (!context.buffer || !context.functions) {
        return;
    }

    ActiveMinimapRenderContext active(&context);
    clear_minimap(context);
    foreach_map_tile(draw_minimap_tile);
}

void widget_minimap_update(const minimap_functions *functions)
{
    data.functions = functions ? functions : &default_functions;
    prepare_minimap_cache();
    if (!data.cache.buffer) {
        return;
    }
    minimap_render_context context = make_live_render_context();
    render_minimap_to_context(context);
    graphics_renderer()->update_custom_image(CUSTOM_IMAGE_MINIMAP);
}

void widget_minimap_save_preview(buffer *buf)
{
    if (!buf) {
        return;
    }

    const int minimap_width = default_functions.map.width();
    const int minimap_height = default_functions.map.height() * 2;
    const int width = minimap_width * 2;
    const int height = minimap_height;
    if (width <= 0 || height <= 0) {
        buffer_init_dynamic(buf, 0);
        return;
    }

    std::vector<color_t> pixels(static_cast<size_t>(width) * height);
    minimap_render_context context = {
        &default_functions,
        (VIEW_X_MAX - minimap_width) / 2,
        (VIEW_Y_MAX - minimap_height) / 2,
        minimap_width,
        minimap_height,
        width,
        pixels.data(),
        &CLIMATE_VARIANTS[default_functions.climate()]
    };
    render_minimap_to_context(context);

    const size_t row_size = sizeof(color_t) * width;
    const size_t payload_size = 4 * sizeof(uint32_t) + row_size * height;
    buffer_init_dynamic(buf, payload_size);
    buffer_write_u32(buf, SAVE_MINIMAP_PREVIEW_MAGIC);
    buffer_write_u32(buf, SAVE_MINIMAP_PREVIEW_VERSION);
    buffer_write_u32(buf, static_cast<uint32_t>(width));
    buffer_write_u32(buf, static_cast<uint32_t>(height));
    for (int y = 0; y < height; y++) {
        buffer_write_raw(buf, &pixels[static_cast<size_t>(y) * width], row_size);
    }
}

int widget_minimap_load_saved_preview(buffer *buf)
{
    if (!buf || buf->size < sizeof(uint32_t)) {
        return 0;
    }

    buffer payload = *buf;
    const size_t payload_size = buffer_load_dynamic(&payload);
    if (payload_size < 4 * sizeof(uint32_t)) {
        return 0;
    }
    if (buffer_read_u32(&payload) != SAVE_MINIMAP_PREVIEW_MAGIC) {
        return 0;
    }
    if (buffer_read_u32(&payload) != SAVE_MINIMAP_PREVIEW_VERSION) {
        return 0;
    }

    const int width = static_cast<int>(buffer_read_u32(&payload));
    const int height = static_cast<int>(buffer_read_u32(&payload));
    if (width <= 0 || height <= 0 || width > GRID_SIZE * 2 || height > GRID_SIZE * 2) {
        return 0;
    }
    const size_t row_size = sizeof(color_t) * width;
    if (payload_size != 4 * sizeof(uint32_t) + row_size * height) {
        return 0;
    }

    graphics_renderer()->create_custom_image(CUSTOM_IMAGE_MINIMAP_PREVIEW, width, height, 0);
    int stride = 0;
    color_t *preview_buffer = graphics_renderer()->get_custom_image_buffer(CUSTOM_IMAGE_MINIMAP_PREVIEW, &stride);
    if (!preview_buffer) {
        return 0;
    }

    for (int y = 0; y < height; y++) {
        buffer_read_raw(&payload, &preview_buffer[y * stride], row_size);
    }
    if (payload.overflow) {
        preview.has_image = 0;
        return 0;
    }
    preview.width = width;
    preview.height = height;
    preview.has_image = 1;
    graphics_renderer()->update_custom_image(CUSTOM_IMAGE_MINIMAP_PREVIEW);
    return 1;
}

void widget_minimap_show_placeholder_preview(void)
{
    graphics_renderer()->create_custom_image(
        CUSTOM_IMAGE_MINIMAP_PREVIEW,
        SAVE_MINIMAP_PLACEHOLDER_WIDTH,
        SAVE_MINIMAP_PLACEHOLDER_HEIGHT,
        0);
    int stride = 0;
    color_t *preview_buffer = graphics_renderer()->get_custom_image_buffer(CUSTOM_IMAGE_MINIMAP_PREVIEW, &stride);
    if (!preview_buffer) {
        return;
    }

    for (int y = 0; y < SAVE_MINIMAP_PLACEHOLDER_HEIGHT; y++) {
        for (int x = 0; x < SAVE_MINIMAP_PLACEHOLDER_WIDTH; x++) {
            const int border = x < 2 || y < 2 ||
                x >= SAVE_MINIMAP_PLACEHOLDER_WIDTH - 2 ||
                y >= SAVE_MINIMAP_PLACEHOLDER_HEIGHT - 2;
            const int stripe = ((x + y) / 16) & 1;
            preview_buffer[y * stride + x] = border ? COLOR_MINIMAP_LIGHT :
                (stripe ? COLOR_MINIMAP_DARK : COLOR_BLACK);
        }
    }
    preview.width = SAVE_MINIMAP_PLACEHOLDER_WIDTH;
    preview.height = SAVE_MINIMAP_PLACEHOLDER_HEIGHT;
    preview.has_image = 1;
    graphics_renderer()->update_custom_image(CUSTOM_IMAGE_MINIMAP_PREVIEW);
}

void widget_minimap_draw_preview(int x_offset, int y_offset, int width, int height)
{
    if (!preview.has_image || !graphics_renderer()->has_custom_image(CUSTOM_IMAGE_MINIMAP_PREVIEW)) {
        return;
    }

    const int preview_width = preview.width;
    const int preview_height = preview.height;
    if (preview_width <= 0 || preview_height <= 0) {
        return;
    }

    graphics_set_clip_rectangle(x_offset, y_offset, width, height);
    graphics_fill_rect(x_offset, y_offset, width, height, COLOR_BLACK);
    const float scale_x = preview_width / static_cast<float>(width);
    const float scale_y = preview_height / static_cast<float>(height);
    float scale = scale_x > scale_y ? scale_x : scale_y;
    if (scale < SCALE_NONE) {
        scale = SCALE_NONE;
    }
    const int drawn_width = static_cast<int>(preview_width / scale);
    const int drawn_height = static_cast<int>(preview_height / scale);
    const int draw_x = x_offset + (width - drawn_width) / 2;
    const int draw_y = y_offset + (height - drawn_height) / 2;
    graphics_renderer()->draw_custom_image(
        CUSTOM_IMAGE_MINIMAP_PREVIEW,
        static_cast<int>(draw_x * scale),
        static_cast<int>(draw_y * scale),
        scale,
        1);
    graphics_reset_clip_rectangle();
}

void widget_minimap_draw(int x_offset, int y_offset, int width, int height)
{
    if (!data.cache.buffer) {
        return;
    }
    position_minimap(x_offset, y_offset, width, height);
    graphics_renderer()->draw_custom_image(CUSTOM_IMAGE_MINIMAP,
        (int) ((data.screen.x - data.minimap.offset_x) * data.minimap.scale),
        (int) ((data.screen.y - data.minimap.offset_y) * data.minimap.scale), data.minimap.scale, 0);
}

void widget_minimap_draw_decorated(int x_offset, int y_offset, int width, int height)
{
    graphics_set_clip_rectangle(x_offset, y_offset, width, height);
    graphics_fill_rect(x_offset, y_offset, width, height, COLOR_BLACK);
    if (data.refresh_requested) {
        widget_minimap_update(0);
        data.refresh_requested = 0;
    }
    widget_minimap_draw(x_offset, y_offset, width, height);
    draw_viewport_rectangle();
    graphics_reset_clip_rectangle();

    graphics_draw_line(x_offset - 1, x_offset - 1 + width, y_offset - 1, y_offset - 1, COLOR_MINIMAP_DARK);
    graphics_draw_line(x_offset - 1, x_offset - 1, y_offset, y_offset + height, COLOR_MINIMAP_DARK);
    graphics_draw_line(x_offset - 1 + width, x_offset - 1 + width, y_offset,
        y_offset + height, COLOR_MINIMAP_LIGHT);
}

static void update_mouse_grid_offset(int x_view, int y_view, int grid_offset)
{
    if (data.mouse.y == y_view && (data.mouse.x == x_view || data.mouse.x == x_view + 1)) {
        data.mouse.grid_offset = grid_offset < 0 ? 0 : grid_offset;
    }
}

static int get_mouse_grid_offset(const mouse *m)
{
    data.mouse.x = (int) ((m->x - data.screen.x) * data.minimap.scale + data.minimap.offset_x);
    data.mouse.y = (int) ((m->y - data.screen.y) * data.minimap.scale + data.minimap.offset_y);
    data.mouse.grid_offset = 0;
    minimap_render_context context = make_live_render_context();
    ActiveMinimapRenderContext active(&context);
    foreach_map_tile(update_mouse_grid_offset);
    return data.mouse.grid_offset;
}

static int is_in_minimap(const mouse *m)
{
    if (m->x >= data.screen.x && m->x < data.screen.x + data.screen.width &&
        m->y >= data.screen.y && m->y < data.screen.y + data.screen.height) {
        return 1;
    }
    return 0;
}

int widget_minimap_handle_mouse(const mouse *m)
{
    if ((m->left.went_down || m->right.went_down) && is_in_minimap(m)) {
        int grid_offset = get_mouse_grid_offset(m);
        if (grid_offset > 0) {
            city_view_go_to_grid_offset(grid_offset);
            return 1;
        }
    }
    return 0;
}
