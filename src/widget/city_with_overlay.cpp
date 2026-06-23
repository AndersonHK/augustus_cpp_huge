#include "building/building_type.h"
#include "building/construction.h"
#include "building/construction_clear.h"
#include "building/industry.h"
#include "building/storage.h"
#include "figure/roamer_preview.h"
#include "game/performance_tracker.h"
#include "game/state.h"
#include "graphics/graphics.h"
#include "graphics/image.h"
#include "graphics/weather.h"
#include "map/bridge.h"
#include "map/building.h"
#include "map/image.h"
#include "map/image_context.h"
#include "map/tiles.h"
#include "widget/city_bridge.h"
#include "widget/city_building_ghost.h"
#include "widget/city_draw_highway.h"
#include "widget/city_figure.h"
#include "widget/city_overlay_education.h"
#include "widget/city_overlay_entertainment.h"
#include "widget/city_overlay_health.h"
#include "widget/city_overlay_housing.h"
#include "widget/city_overlay_other.h"
#include "widget/city_overlay_risks.h"
#include "widget/city_without_overlay.h"

#include "city_with_overlay.h"

#include "building/animations.h"
#include "building/building.h"
#include "building/building_record.h"
#include "building/building_type_registry_internal.h"
#include "city/view_render.h"
#include "widget/city_draw.h"

#include "assets/assets.h"
#include "building/granary.h"
#include "building/properties.h"
#include "building/building_type_api.h"
#include "city/view.h"
#include "core/config.h"
#include "core/log.h"
#include "game/resource.h"
#include "game/resource_graphics.h"
#include "graphics/renderer.h"
#include "graphics/window.h"
#include "map/figure.h"
#include "map/grid.h"
#include "map/property.h"
#include "map/random.h"
#include "map/terrain.h"
#include "widget/city_overlay.h"

#include <cstring>

static const city_overlay *overlay = 0;
static float scale = SCALE_NONE;
static unsigned int city_roamer_preview_selected_building_id = ((unsigned int) -1); //NO_POSITION default

#define SELECTED_BUILDING_COLOR_MASK COLOR_MASK_SKY_BLUE
#define OFFSET(x,y) (x + GRID_SIZE * y)

static int building_matches(const Building &building, const char *text_id)
{
    const building_type_registry_impl::BuildingType *definition = building.type;
    return definition && std::strcmp(definition->attr(), text_id) == 0;
}

static const int ADJACENT_OFFSETS[2][4][7] = {
    {
        {OFFSET(-1, 0), OFFSET(-1, -1), OFFSET(-1, -2), OFFSET(0, -2), OFFSET(1, -2)},
        {OFFSET(0, -1), OFFSET(1, -1), OFFSET(2, -1), OFFSET(2, 0), OFFSET(2, 1)},
        {OFFSET(1, 0), OFFSET(1, 1), OFFSET(1, 2), OFFSET(0, 2), OFFSET(-1, 2)},
        {OFFSET(0, 1), OFFSET(-1, 1), OFFSET(-2, 1), OFFSET(-2, 0), OFFSET(-2, -1)}
    },
    {
        {OFFSET(-1, 0), OFFSET(-1, -1), OFFSET(-1, -2), OFFSET(-1, -3), OFFSET(0, -3),  OFFSET(1, -3), OFFSET(2, -3)},
        {OFFSET(0, -1), OFFSET(1, -1), OFFSET(2, -1), OFFSET(3, -1), OFFSET(3, 0),  OFFSET(3, 1), OFFSET(3, 2)},
        {OFFSET(1, 0), OFFSET(1, 1), OFFSET(1, 2), OFFSET(1, 3), OFFSET(0, 3),  OFFSET(-1, 3), OFFSET(-2, 3)},
        {OFFSET(0, 1), OFFSET(-1, 1), OFFSET(-2, 1), OFFSET(-3, 1), OFFSET(-3, 0),  OFFSET(-3, -1), OFFSET(-3, -2)}
    }
};

static const city_overlay *get_city_overlay(void)
{
    switch (game_state_overlay()) {
        case OVERLAY_FIRE:
            return city_overlay_for_fire();
        case OVERLAY_CRIME:
            return city_overlay_for_crime();
        case OVERLAY_DAMAGE:
            return city_overlay_for_damage();
        case OVERLAY_PROBLEMS:
            return city_overlay_for_problems();
        case OVERLAY_NATIVE:
            return city_overlay_for_native();
        case OVERLAY_ENTERTAINMENT:
            return city_overlay_for_entertainment();
        case OVERLAY_THEATER:
            return city_overlay_for_theater();
        case OVERLAY_AMPHITHEATER:
            return city_overlay_for_amphitheater();
        case OVERLAY_ARENA:
            return city_overlay_for_arena();
        case OVERLAY_COLOSSEUM:
            return city_overlay_for_colosseum();
        case OVERLAY_HIPPODROME:
            return city_overlay_for_hippodrome();
        case OVERLAY_TAVERN:
            return city_overlay_for_tavern();
        case OVERLAY_EDUCATION:
            return city_overlay_for_education();
        case OVERLAY_SCHOOL:
            return city_overlay_for_school();
        case OVERLAY_LIBRARY:
            return city_overlay_for_library();
        case OVERLAY_ACADEMY:
            return city_overlay_for_academy();
        case OVERLAY_HEALTH:
            return city_overlay_for_health();
        case OVERLAY_BARBER:
            return city_overlay_for_barber();
        case OVERLAY_BATHHOUSE:
            return city_overlay_for_bathhouse();
        case OVERLAY_CLINIC:
            return city_overlay_for_clinic();
        case OVERLAY_HOSPITAL:
            return city_overlay_for_hospital();
        case OVERLAY_SICKNESS:
            return city_overlay_for_sickness();
        case OVERLAY_RELIGION:
            return city_overlay_for_religion();
        case OVERLAY_TAX_INCOME:
            return city_overlay_for_tax_income();
        case OVERLAY_EFFICIENCY:
            return city_overlay_for_efficiency();
        case OVERLAY_FOOD_STOCKS:
            return city_overlay_for_food_stocks();
        case OVERLAY_WATER:
            return city_overlay_for_water();
        case OVERLAY_SENTIMENT:
            return city_overlay_for_sentiment();
        case OVERLAY_DESIRABILITY:
            return city_overlay_for_desirability();
        case OVERLAY_ROADS:
            return city_overlay_for_roads();
        case OVERLAY_LEVY:
            return city_overlay_for_levy();
        case OVERLAY_EMPLOYMENT:
            return city_overlay_for_employment();
        case OVERLAY_MOTHBALL:
            return city_overlay_for_mothball();
        case OVERLAY_ENEMY:
            return city_overlay_for_enemy();
        case OVERLAY_LOGISTICS:
            return city_overlay_for_logistics();
        case OVERLAY_STORAGES:
            return city_overlay_for_storages();
        case OVERLAY_HOUSE_SMALL_TENT:
            return city_overlay_for_small_tent();
        case OVERLAY_HOUSE_LARGE_TENT:
            return city_overlay_for_large_tent();
        case OVERLAY_HOUSE_SMALL_SHACK:
            return city_overlay_for_small_shack();
        case OVERLAY_HOUSE_LARGE_SHACK:
            return city_overlay_for_large_shack();
        case OVERLAY_HOUSE_SMALL_HOVEL:
            return city_overlay_for_small_hovel();
        case OVERLAY_HOUSE_LARGE_HOVEL:
            return city_overlay_for_large_hovel();
        case OVERLAY_HOUSE_SMALL_CASA:
            return city_overlay_for_small_casa();
        case OVERLAY_HOUSE_LARGE_CASA:
            return city_overlay_for_large_casa();
        case OVERLAY_HOUSE_SMALL_INSULA:
            return city_overlay_for_small_insula();
        case OVERLAY_HOUSE_MEDIUM_INSULA:
            return city_overlay_for_medium_insula();
        case OVERLAY_HOUSE_LARGE_INSULA:
            return city_overlay_for_large_insula();
        case OVERLAY_HOUSE_GRAND_INSULA:
            return city_overlay_for_grand_insula();
        case OVERLAY_HOUSE_SMALL_VILLA:
            return city_overlay_for_small_villa();
        case OVERLAY_HOUSE_MEDIUM_VILLA:
            return city_overlay_for_medium_villa();
        case OVERLAY_HOUSE_LARGE_VILLA:
            return city_overlay_for_large_villa();
        case OVERLAY_HOUSE_GRAND_VILLA:
            return city_overlay_for_grand_villa();
        case OVERLAY_HOUSE_SMALL_PALACE:
            return city_overlay_for_small_palace();
        case OVERLAY_HOUSE_MEDIUM_PALACE:
            return city_overlay_for_medium_palace();
        case OVERLAY_HOUSE_LARGE_PALACE:
            return city_overlay_for_large_palace();
        case OVERLAY_HOUSE_LUXURY_PALACE:
            return city_overlay_for_luxury_palace();
        case OVERLAY_HOUSING_GROUPS_TENTS:
            return city_overlay_for_housing_groups_tents();
        case OVERLAY_HOUSING_GROUPS_SHACKS:
            return city_overlay_for_housing_groups_shacks();
        case OVERLAY_HOUSING_GROUPS_HOVELS:
            return city_overlay_for_housing_groups_hovels();
        case OVERLAY_HOUSING_GROUPS_CASAE:
            return city_overlay_for_housing_groups_casae();
        case OVERLAY_HOUSING_GROUPS_INSULAE:
            return city_overlay_for_housing_groups_insulae();
        case OVERLAY_HOUSING_GROUPS_VILLAS:
            return city_overlay_for_housing_groups_villas();
        case OVERLAY_HOUSING_GROUPS_PALACES:
            return city_overlay_for_housing_groups_palaces();
        default:
            return 0;
    }
}

static int select_city_overlay(void)
{
    if (!overlay || overlay->type != game_state_overlay()) {
        overlay = get_city_overlay();
    }
    return overlay != 0;
}

void city_with_overlay_update(void)
{
    select_city_overlay();
}

static color_t get_building_color_mask(const Building &building)
{
    color_t color_mask = COLOR_MASK_NONE;
    const building_type type = building.type ? building.type->type() : BUILDING_NONE;
    const model_building *model = model_get_building(type);
    int labor_needed = model->laborers;
    if (!labor_needed && !building_matches(building, "warehouse_space")) { // account for warehouse case
        color_mask = COLOR_MASK_NONE;
    } else {
        if (building_matches(building, "latrines") || building_matches(building, "fountain")) {
            color_mask = COLOR_MASK_NONE;
        } else {
            color_mask = SELECTED_BUILDING_COLOR_MASK;
        }
    }
    return color_mask;
}

static int is_building_selected(const Building &building)
{
    if (!config_get(CONFIG_UI_HIGHLIGHT_SELECTED_BUILDING)) { // if option not selected in config, abandon
        return 0;
    }
    Building main_building = building.main();
    if (building.id() == city_roamer_preview_selected_building_id || main_building.id() == city_roamer_preview_selected_building_id) {
        return 1;
    } else {
        return 0;
    }

}

static int is_drawable_farmhouse(int grid_offset, int map_orientation)
{
    if (!map_property_is_draw_tile(grid_offset)) {
        return 0;
    }
    int xy = map_property_multi_tile_xy(grid_offset);
    if (map_orientation == DIR_0_TOP && xy == EDGE_X0Y1) {
        return 1;
    }
    if (map_orientation == DIR_2_RIGHT && xy == EDGE_X0Y0) {
        return 1;
    }
    if (map_orientation == DIR_4_BOTTOM && xy == EDGE_X1Y0) {
        return 1;
    }
    if (map_orientation == DIR_2_RIGHT && xy == EDGE_X1Y1) {
        return 1;
    }
    return 0;
}

static int is_drawable_farm_corner(int grid_offset)
{
    if (!map_property_is_draw_tile(grid_offset)) {
        return 0;
    }

    int map_orientation = city_view_orientation();
    int xy = map_property_multi_tile_xy(grid_offset);
    if (map_orientation == DIR_0_TOP && xy == EDGE_X0Y2) {
        return 1;
    } else if (map_orientation == DIR_2_RIGHT && xy == EDGE_X0Y0) {
        return 1;
    } else if (map_orientation == DIR_4_BOTTOM && xy == EDGE_X2Y0) {
        return 1;
    } else if (map_orientation == DIR_6_LEFT && xy == EDGE_X2Y2) {
        return 1;
    }
    return 0;
}

static int draw_building_as_deleted(const Building &building)
{
    Building main_building = building.main();
    return main_building.id() && (main_building.is_deleted() || map_property_is_deleted(main_building.grid_offset()));
}

static int is_multi_tile_terrain(int grid_offset)
{
    return !map_building_at(grid_offset) && map_property_multi_tile_size(grid_offset) > 1;
}

static int has_adjacent_deletion(int grid_offset)
{
    int size = map_property_multi_tile_size(grid_offset);
    int total_adjacent_offsets = size * 2 + 1;
    const int *adjacent_offset = ADJACENT_OFFSETS[size - 2][city_view_orientation() / 2];
    for (int i = 0; i < total_adjacent_offsets; ++i) {
        if (map_property_is_deleted(grid_offset + adjacent_offset[i]) ||
            draw_building_as_deleted(Building(building_get(map_building_at(grid_offset + adjacent_offset[i]))))) {
            return 1;
        }
    }
    return 0;
}

static void draw_flattened_building_footprint(const Building &building, int x, int y, int image_offset, color_t color_mask)
{
    int image_base = Image::group(GROUP_TERRAIN_OVERLAY) + image_offset;
    if (building.has_house_size()) {
        image_base += 4;
    }
    if (map_is_bridge(building.grid_offset())) {//dont draw bridges
        return;
    }
    if (building.size() == 1) {
        Image::from_id(image_base).draw_isometric_footprint_from_draw_tile(x, y, color_mask, scale);
    } else if (building.size() == 2) {
        const int x_tile_offset[] = { 30, 0, 60, 30 };
        const int y_tile_offset[] = { -15, 0, 0, 15 };
        for (int i = 0; i < 4; i++) {
            Image::from_id(image_base + i).draw_isometric_footprint_from_draw_tile(x + x_tile_offset[i], y + y_tile_offset[i], color_mask, scale);
        }
    } else if (building.size() == 3) {
        const int image_tile_offset[] = { 0, 1, 2, 1, 3, 2, 3, 3, 3 };
        const int x_tile_offset[] = { 60, 30, 90, 0, 60, 120, 30, 90, 60 };
        const int y_tile_offset[] = { -30, -15, -15, 0, 0, 0, 15, 15, 30 };
        for (int i = 0; i < 9; i++) {
            Image::from_id(image_base + image_tile_offset[i]).draw_isometric_footprint_from_draw_tile(x + x_tile_offset[i], y + y_tile_offset[i], color_mask, scale);
        }
    } else if (building.size() == 4) {
        const int image_tile_offset[] = { 0, 1, 2, 1, 3, 2, 1, 3, 3, 2, 3, 3, 3, 3, 3, 3 };
        const int x_tile_offset[] = {
            90,
            60, 120,
            30, 90, 150,
            0, 60, 120, 180,
            30, 90, 150,
            60, 120,
            90
        };
        const int y_tile_offset[] = {
            -45,
            -30, -30,
            -15, -15, -15,
            0, 0, 0, 0,
            15, 15, 15,
            30, 30,
            45
        };
        for (int i = 0; i < 16; i++) {
            Image::from_id(image_base + image_tile_offset[i]).draw_isometric_footprint_from_draw_tile(x + x_tile_offset[i], y + y_tile_offset[i], color_mask, scale);
        }
    } else if (building.size() == 5) {
        const int image_tile_offset[] = { 0, 1, 2, 1, 3, 2, 1, 3, 3, 2, 1, 3, 3, 3, 2, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3 };
        const int x_tile_offset[] = {
            120,
            90, 150,
            60, 120, 180,
            30, 90, 150, 210,
            0, 60, 120, 180, 240,
            30, 90, 150, 210,
            60, 120, 180,
            90, 150,
            120
        };
        const int y_tile_offset[] = {
            -60,
            -45, -45,
            -30, -30, -30,
            -15, -15, -15, -15,
            0, 0, 0, 0, 0,
            15, 15, 15, 15,
            30, 30, 30,
            45, 45,
            60
        };
        for (int i = 0; i < 25; i++) {
            Image::from_id(image_base + image_tile_offset[i]).draw_isometric_footprint_from_draw_tile(x + x_tile_offset[i], y + y_tile_offset[i], color_mask, scale);
        }
    } else if (building.size() == 7) {
        const int image_tile_offset[] = { 0, 1, 2, 1, 3, 2, 1, 3, 3, 2, 1, 3, 3, 3, 2, 1, 3, 3, 3, 3, 2, 1,
            3, 3, 3, 3, 3, 2, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3 };
        const int x_tile_offset[] = {
            180,
            150, 210,
            120, 180, 240,
            90, 150, 210, 270,
            60, 120, 180, 240, 300,
            30, 90, 150, 210, 270, 330,
            0, 60, 120, 180, 240, 300, 360,
            30, 90, 150, 210, 270, 330,
            60, 120, 180, 240, 300,
            90, 150, 210, 270,
            120, 180, 240,
            150, 210,
            180,
        };
        const int y_tile_offset[] = {
            -90,
            -75,-75,
            -60,-60,-60,
            -45, -45,-45,-45,
            -30, -30, -30,-30,-30,
            -15, -15, -15, -15,-15,-15,
            0, 0, 0, 0, 0, 0, 0,
            15, 15, 15, 15, 15, 15,
            30, 30, 30, 30, 30,
            45, 45, 45, 45,
            60, 60, 60,
            75, 75,
            90,
        };
        for (int i = 0; i < 49; i++) {
            Image::from_id(image_base + image_tile_offset[i]).draw_isometric_footprint_from_draw_tile(x + x_tile_offset[i], y + y_tile_offset[i], color_mask, scale);
        }
    }
}

void city_with_overlay_draw_building_footprint(int x, int y, int grid_offset, int image_offset)
{
    int building_id = map_building_at(grid_offset);
    if (!building_id) {
        return;
    }
    Building building(building_get(building_id));
    ::building *overlay_building = building_get(building_id);
    color_t color_mask = 0;
    if (overlay->type == OVERLAY_PROBLEMS) {
        city_overlay_problems_prepare_building(overlay_building);
    }
    if (overlay->show_building(overlay_building)) {
        if (is_building_selected(building)) {
            color_mask = get_building_color_mask(building);
        }
        if (building.type && building.type->is_farm()) {
            if (is_drawable_farmhouse(grid_offset, city_view_orientation())) {
                Image::from_id(map_image_at(grid_offset)).draw_isometric_footprint_from_draw_tile(x, y, color_mask, scale);
            } else if (map_property_is_draw_tile(grid_offset)) {
                Image::from_id(map_image_at(grid_offset)).draw_isometric_footprint_from_draw_tile(x, y, color_mask, scale);
            }
        } else {
            if (!building.draw_footprint({ x, y, grid_offset, color_mask, scale })) {
                Image::from_id(map_image_at(grid_offset)).draw_isometric_footprint_from_draw_tile(x, y, color_mask, scale);
            }
        }
    } else {
        int draw = 1;
        if (building.size() == 3 && building.type && building.type->is_farm()) {
            draw = is_drawable_farm_corner(grid_offset);
        }
        if (draw) {
            draw_flattened_building_footprint(building, x, y, image_offset, color_mask);
        }
    }
}

static void draw_roamer_frequency(int x, int y, int grid_offset)
{
    int travel_frequency = figure_roamer_preview_get_frequency(grid_offset);
    if (travel_frequency > 0 && travel_frequency <= FIGURE_ROAMER_PREVIEW_MAX_PASSAGES) {
        static color_t frequency_colors[] = {
            0x663377ff, 0x662266ee, 0x661155dd, 0x660044cc, 0x660033c4, 0x660022bb, 0x660011a4, 0x66000088
        };
        Image::from_id(Image::group(GROUP_TERRAIN_FLAT_TILE)).draw(x, y, frequency_colors[travel_frequency - 1], scale);
    } else if (travel_frequency == FIGURE_ROAMER_PREVIEW_ENTRY_TILE) {
        Image::blend_footprint_color(x, y, COLOR_MASK_RED, scale);
    } else if (travel_frequency == FIGURE_ROAMER_PREVIEW_EXIT_TILE) {
        Image::blend_footprint_color(x, y, COLOR_MASK_GREEN, scale);
    } else if (travel_frequency == FIGURE_ROAMER_PREVIEW_ENTRY_EXIT_TILE) {
        Image::from_id(Image::group(GROUP_TERRAIN_FLAT_TILE)).draw_isometric_footprint(x, y, COLOR_MASK_PINK, scale);
    }
}

static void draw_footprint(int x, int y, int grid_offset)
{
    building_construction_record_view_position(x, y, grid_offset);
    if (grid_offset < 0) {
        return;
    } else if (overlay->draw_custom_footprint && overlay->draw_custom_footprint(x, y, scale, grid_offset)) {
        return;
    }
    if (map_property_is_draw_tile(grid_offset)) {
        int terrain = map_terrain_get(grid_offset);
        if (terrain & TERRAIN_HIGHWAY && !(terrain & TERRAIN_GATEHOUSE)) {
            city_draw_highway_footprint(x, y, scale, grid_offset, COLOR_MASK_NONE);
        } else if (terrain & (TERRAIN_AQUEDUCT | TERRAIN_WALL)) {
            if (terrain & TERRAIN_ROAD) {
                // Draw the equivalent road tile.
                int image_id = Image::group(GROUP_TERRAIN_ROAD);
                if (map_tiles_is_paved_road(grid_offset)) {
                    const terrain_image *img = map_image_context_get_paved_road(grid_offset);
                    image_id += img->group_offset + img->item_offset;
                } else {
                    const terrain_image *img = map_image_context_get_dirt_road(grid_offset);
                    image_id += img->group_offset + img->item_offset + 49;
                }
                Image::from_id(image_id).draw_isometric_footprint_from_draw_tile(x, y, 0, scale);
            } else {
                // display grass
                int image_id = Image::group(GROUP_TERRAIN_GRASS_1) + (map_random_get(grid_offset) & 7);
                Image::from_id(image_id).draw_isometric_footprint_from_draw_tile(x, y, 0, scale);
            }
        } else if (city_draw_runtime_tile_footprint(grid_offset, x, y, COLOR_MASK_NONE, scale)) {
        } else if ((terrain & TERRAIN_ROAD) && !(terrain & TERRAIN_BUILDING)) {
            Image::from_id(map_image_at(grid_offset)).draw_isometric_footprint_from_draw_tile(x, y, 0, scale);
        } else if ((terrain & TERRAIN_BUILDING) && !map_is_bridge(grid_offset)) {
            city_with_overlay_draw_building_footprint(x, y, grid_offset, 0);
        } else {
            Image::from_id(map_image_at(grid_offset)).draw_isometric_footprint_from_draw_tile(x, y, 0, scale);
        }
    }
    if (config_get(CONFIG_UI_SHOW_GRID) && map_property_is_draw_tile(grid_offset)
                                        && !map_building_at(grid_offset) && scale <= 2.0f) {
        //grid is drawn by the renderer directly at zoom > 200%
        static int grid_id = 0;
        if (!grid_id) {
            grid_id = assets_get_image_id("UI\\Grid_Full", "Grid_Full");
        }
        Image::from_id(grid_id).draw(x, y, COLOR_GRID, scale);
    }
    draw_roamer_frequency(x, y, grid_offset);
}

static void draw_overlay_column(int x, int y, int height, column_color_type color_type)
{
    int image_id = Image::group(GROUP_OVERLAY_COLUMN);
    if (height > 10) {
        height = 10;
    }
    switch (color_type) {
        case COLUMN_COLOR_RED:
            image_id += 9;
            break;
        case COLUMN_COLOR_RED_TO_GREEN:
            image_id += height - (height % 3);
            break;
        case COLUMN_COLOR_GREEN_TO_RED:
            image_id += 9 - height + (height % 3);
            break;
        default:
            break;
    }

    int capital_height = image_get(image_id)->height;
    // base
    Image::from_id(image_id + 2).draw(x + 9, y - 8, 0, scale);
    if (height) {
        // column
        for (int i = 1; i < height; i++) {
            Image::from_id(image_id + 1).draw(x + 17, y - 8 - 10 * i + 13, 0, scale);
        }
        // capital
        Image::from_id(image_id).draw(x + 5, y - 8 - capital_height - 10 * (height - 1) + 13, 0, scale);
    }
}
static void draw_depot_resource(const Building &building, int x, int y)
{
    static const ImageGroupEntryRef cat =
        ImageGroupEntryRef::from_group("Admin_Logistics\\Cart_Depot_Cat", "Cart_Depot_Cat");
    const ImageGroupEntryRef &image = building.worker_count() ?
        resource_graphics(building.depot_order().resource_type).cart_image(1) :
        cat;
    image.draw(x + 11, y, COLOR_MASK_NONE, scale);
}

static void draw_permissions_flag(Building &building, int x, int y, color_t color_mask)
{
    city_draw_storage_permission_flag(building, x, y, color_mask, scale);
}

static void draw_warehouse_ornaments(int x, int y, color_t color_mask)
{
    Image::from_id(Image::group(GROUP_BUILDING_WAREHOUSE) + 17).draw(x - 4, y - 42, color_mask, scale);
}

static void draw_granary_stores(const image &image, Building &building, int x, int y, color_t color_mask)
{
    if (image.animation) {
        Image::from_id(Image::group(GROUP_BUILDING_GRANARY) + 1).draw(
            x + image.animation->sprite_offset_x,
            y + 60 + image.animation->sprite_offset_y - image.height,
            color_mask,
            scale);
    }

    if (building.resource_amount(RESOURCE_NONE) < FULL_GRANARY) {
        Image::from_id(Image::group(GROUP_BUILDING_GRANARY) + 2).draw(x + 33, y - 60, color_mask, scale);
        if (building.resource_amount(RESOURCE_NONE) < THREEQUARTERS_GRANARY) {
            Image::from_id(Image::group(GROUP_BUILDING_GRANARY) + 3).draw(x + 56, y - 50, color_mask, scale);
        }
        if (building.resource_amount(RESOURCE_NONE) < HALF_GRANARY) {
            Image::from_id(Image::group(GROUP_BUILDING_GRANARY) + 4).draw(x + 91, y - 50, color_mask, scale);
        }
        if (building.resource_amount(RESOURCE_NONE) < QUARTER_GRANARY) {
            Image::from_id(Image::group(GROUP_BUILDING_GRANARY) + 5).draw(x + 117, y - 62, color_mask, scale);
        }
    }
    draw_permissions_flag(building, x + 81, y - 101, color_mask);
}

static color_t building_top_color_mask(const Building &building)
{
    color_t color_mask = draw_building_as_deleted(building) ? building_construction_clear_color() : 0;
    return is_building_selected(building) ? get_building_color_mask(building) : color_mask;
}

static void draw_building_top(Building building, int x, int y, int grid_offset)
{
    color_t color_mask = building_top_color_mask(building);

    if (building.type && building.type->is_farm()) {
        if (is_drawable_farmhouse(grid_offset, city_view_orientation())) {
            Image::from_id(map_image_at(grid_offset)).draw_isometric_top_from_draw_tile(x, y, color_mask, scale);
        } else if (map_property_is_draw_tile(grid_offset)) {
            Image::from_id(map_image_at(grid_offset)).draw_isometric_top_from_draw_tile(x, y, color_mask, scale);
        }
        return;
    }
    if (building.type && building.type->is_warehouse()) {
        draw_warehouse_ornaments(x, y, color_mask);
        draw_permissions_flag(building, x + 19, y - 56, color_mask);
    }
    if (building_matches(building, "cart_depot")) {
        draw_depot_resource(building, x, y);
    }

    if (!building.draw_top({ x, y, grid_offset, color_mask, scale })) {
        if (building.type && building.type->is_granary()) {
            draw_granary_stores(*image_get(map_image_at(grid_offset)), building, x, y, color_mask);
            return;
        }
        Image::from_id(map_image_at(grid_offset)).draw_isometric_top_from_draw_tile(x, y, color_mask, scale);
    }
}

static void city_with_overlay_draw_building_top_for_building(Building building, int x, int y, int grid_offset)
{
    const ::building *overlay_building = building_get(building.id());
    if (overlay->show_building(overlay_building)) {
        draw_building_top(building, x, y, grid_offset);
    } else {
        int column_height = overlay->get_column_height(overlay_building);
        if (column_height != NO_COLUMN) {
            int draw = 1;
            if (building.type && building.type->is_farm()) {
                draw = is_drawable_farm_corner(grid_offset);
            }
            if (draw) {
                draw_overlay_column(x, y, column_height, overlay->column_type);
            }
        }
    }
}

void city_with_overlay_draw_building_top(int x, int y, int grid_offset)
{
    city_with_overlay_draw_building_top_for_building(Building(building_get(map_building_at(grid_offset))), x, y, grid_offset);
}

static void draw_top_for_building(Building building, int x, int y, int grid_offset)
{
    if (overlay->draw_custom_top && overlay->draw_custom_top(x, y, scale, grid_offset)) {
        return;
    }
    if (!map_property_is_draw_tile(grid_offset)) {
        return;
    }
    if (map_terrain_is(grid_offset, TERRAIN_BUILDING) && building.id()) {
        city_with_overlay_draw_building_top_for_building(building, x, y, grid_offset);
    } else if (!map_terrain_is(grid_offset, TERRAIN_BUILDING)) {
        if (!map_terrain_is(grid_offset, TERRAIN_WALL | TERRAIN_AQUEDUCT | TERRAIN_ROAD)) {
            color_t color_mask = 0;
            if (map_property_is_deleted(grid_offset) && !is_multi_tile_terrain(grid_offset)) {
                color_mask = building_construction_clear_color();
            }
            // terrain
            if (!city_draw_runtime_tile_top(grid_offset, x, y, color_mask, scale)) {
                Image::from_id(map_image_at(grid_offset)).draw_isometric_top_from_draw_tile(x, y, color_mask, scale);
            }
        }
    }
}

static void draw_top(int x, int y, int grid_offset)
{
    draw_top_for_building(Building(building_get(map_building_at(grid_offset))), x, y, grid_offset);
}

static void draw_top_render_tile(const CityViewRenderTile &tile)
{
    draw_top_for_building(tile.building, tile.x, tile.y, tile.grid_offset);
}

static int overlay_draws_building_animation(const Building &building)
{
    switch (overlay->type) {
        case OVERLAY_FIRE:
        case OVERLAY_CRIME:
            return building_matches(building, "prefecture") || building_matches(building, "burning_ruin");
        case OVERLAY_ENEMY:
            return building_matches(building, "prefecture") ||
                building_matches(building, "burning_ruin") ||
                (building.type && building.type->is_watchtower());
        case OVERLAY_DAMAGE:
            return building_matches(building, "engineers_post");
        case OVERLAY_WATER:
            return building_matches(building, "reservoir") || building_matches(building, "fountain");
        case OVERLAY_FOOD_STOCKS:
            return building_matches(building, "market") || (building.type && building.type->is_granary());
        default:
            return 0;
    }
}

static void draw_animation_for_building(Building building, int x, int y, int grid_offset)
{
    int draw = building.id() && overlay_draws_building_animation(building);

    int image_id = map_image_at(grid_offset);
    const image *img = image_get(image_id);
    if (map_is_bridge(grid_offset)) {
        city_draw_bridge(x, y, scale, grid_offset);
    } else {
        if (!draw || !building.id() || !map_property_is_draw_tile(grid_offset)) {
            return;
        }

        color_t color_mask = building_top_color_mask(building);
        if (building.draw_animation({ x, y, grid_offset, color_mask, scale })) {
            return;
        }
        if (img->animation) {
            if (building.type && building.type->is_granary()) {
                draw_granary_stores(*img, building, x, y, color_mask);
            } else {
                int frame_offset = building.animate().offset_for(Image::from_id(image_id), grid_offset);
                if (frame_offset > 0) {
                    if (frame_offset > img->animation->num_sprites) {
                        frame_offset = img->animation->num_sprites;
                    }
                    int y_offset = img->top ? img->top->original.height - FOOTPRINT_HALF_HEIGHT : 0;
                    Image::from_id(image_id + img->animation->start_offset + frame_offset).draw(x + img->animation->sprite_offset_x, y + img->animation->sprite_offset_y - y_offset, color_mask, scale);
                }
            }
        }
    }
}

static void draw_animation(int x, int y, int grid_offset)
{
    draw_animation_for_building(Building(building_get(map_building_at(grid_offset))), x, y, grid_offset);
}

static void draw_animation_render_tile(const CityViewRenderTile &tile)
{
    draw_animation_for_building(tile.building, tile.x, tile.y, tile.grid_offset);
}

static void draw_figures(int x, int y, int grid_offset)
{
    int figure_id = map_figure_at(grid_offset);
    while (figure_id) {
        Figure *f = Figure::get(figure_id);
        if (!f->is_ghost && overlay->show_figure(f)) {
            city_draw_figure(f, x, y, scale, 0);
        }
        figure_id = f->next_figure_id_on_same_tile;
    }
}

static void draw_figures_render_tile(const CityViewRenderTile &tile)
{
    draw_figures(tile.x, tile.y, tile.grid_offset);
}

static void draw_elevated_figures(int x, int y, int grid_offset)
{
    int figure_id = map_figure_at(grid_offset);
    while (figure_id > 0) {
        Figure *f = Figure::get(figure_id);
        if (((f->use_cross_country && !f->is_ghost && !f->dont_draw_elevated) || f->height_adjusted_ticks) && overlay->show_figure(f)) {
            city_draw_figure(f, x, y, scale, 0);
        } else if (f->building.id() == city_roamer_preview_selected_building_id) { //figure from selected building
            if (config_get(CONFIG_UI_SHOW_ROAMING_PATH)) {
                int highlight = FIGURE_HIGHLIGHT_GREEN;
                if (f->type == FIGURE_MARKET_SUPPLIER || f->type == FIGURE_DELIVERY_BOY) {
                    highlight = FIGURE_HIGHLIGHT_RED; //green highlight makes market supplier look indistinguishable
                }
                city_draw_figure(f, x, y, scale, highlight);
            }

        }
        figure_id = f->next_figure_id_on_same_tile;
    }
}

static int should_draw_top_before_deletion(int grid_offset)
{
    return is_multi_tile_terrain(grid_offset) && has_adjacent_deletion(grid_offset);
}

static void deletion_draw_terrain_top(int x, int y, int grid_offset)
{
    if (should_draw_top_before_deletion(grid_offset)) {
        draw_top(x, y, grid_offset);
    }
}

static void deletion_draw_animations(int x, int y, int grid_offset)
{
    if (map_property_is_deleted(grid_offset) || draw_building_as_deleted(Building(building_get(map_building_at(grid_offset))))) {
        Image::blend_footprint_color(x, y, building_construction_clear_color(), scale);
    }
    if (!should_draw_top_before_deletion(grid_offset)) {
        draw_top(x, y, grid_offset);
    }
    draw_animation(x, y, grid_offset);
}

static void draw_custom_layer(int x, int y, int grid_offset)
{
    overlay->draw_custom_layer(x, y, scale, grid_offset);
}

void city_with_overlay_draw(const map_tile *tile, unsigned int roamer_preview_building_id)
{
    if (!select_city_overlay()) {
        return;
    }

    PerformanceTrackerScope city_draw_scope(PERFORMANCE_TRACKER_BUCKET_CITY_DRAW);
    scale = city_view_get_scale() / 100.0f;
    city_roamer_preview_selected_building_id = roamer_preview_building_id;
    int x, y, width, height;
    city_view_get_viewport(&x, &y, &width, &height);
    graphics_fill_rect(x, y, width, height, COLOR_BLACK);
    int should_mark_deleting = city_building_ghost_mark_deleting(tile);
    {
        PerformanceTrackerScope scope(PERFORMANCE_TRACKER_BUCKET_CITY_DRAW_FOOTPRINT);
        city_view_foreach_valid_map_tile(draw_footprint);
    }
    if (!should_mark_deleting) {
        {
            PerformanceTrackerScope scope(PERFORMANCE_TRACKER_BUCKET_CITY_DRAW_MAIN_ROW);
            city_view_foreach_valid_render_tile_row(
                draw_figures_render_tile,
                draw_top_render_tile,
                draw_animation_render_tile,
                PERFORMANCE_TRACKER_BUCKET_CITY_DRAW_MAIN_FIGURES,
                PERFORMANCE_TRACKER_BUCKET_CITY_DRAW_MAIN_TOP,
                PERFORMANCE_TRACKER_BUCKET_CITY_DRAW_MAIN_ANIMATION
            );
        }
        {
            PerformanceTrackerScope scope(PERFORMANCE_TRACKER_BUCKET_CITY_DRAW_GHOST);
            city_building_ghost_draw(tile);
        }
        {
            PerformanceTrackerScope scope(PERFORMANCE_TRACKER_BUCKET_CITY_DRAW_ELEVATED);
            city_view_foreach_valid_map_tile(draw_elevated_figures);
        }
    } else {
        {
            PerformanceTrackerScope scope(PERFORMANCE_TRACKER_BUCKET_CITY_DRAW_DELETION);
            city_view_foreach_valid_map_tile(draw_figures);
            city_view_foreach_valid_map_tile(deletion_draw_terrain_top);
            city_view_foreach_valid_map_tile(deletion_draw_animations);
        }
        {
            PerformanceTrackerScope scope(PERFORMANCE_TRACKER_BUCKET_CITY_DRAW_GHOST);
            city_building_ghost_draw(tile);
        }
        {
            PerformanceTrackerScope scope(PERFORMANCE_TRACKER_BUCKET_CITY_DRAW_ELEVATED);
            city_view_foreach_valid_map_tile(draw_elevated_figures);
        }
    }
    if (overlay->draw_custom_layer) {
        PerformanceTrackerScope scope(PERFORMANCE_TRACKER_BUCKET_CITY_DRAW_OVERLAY);
        city_view_foreach_valid_map_tile(draw_custom_layer);
    }
    {
        PerformanceTrackerScope scope(PERFORMANCE_TRACKER_BUCKET_CITY_DRAW_WEATHER);
        update_weather();
    }
}

int city_with_overlay_get_tooltip_text(tooltip_context *c, int grid_offset)
{
    int overlay_type = overlay->type;
    int building_id = map_building_at(grid_offset);
    if (overlay->get_tooltip_for_building && !building_id) {
        return 0;
    }
    int overlay_requires_house =
        overlay_type != OVERLAY_WATER && overlay_type != OVERLAY_FIRE && overlay_type != OVERLAY_LEVY &&
        overlay_type != OVERLAY_DAMAGE && overlay_type != OVERLAY_NATIVE && overlay_type != OVERLAY_DESIRABILITY &&
        overlay_type != OVERLAY_PROBLEMS && overlay_type != OVERLAY_MOTHBALL && overlay_type != OVERLAY_ENEMY &&
        overlay_type != OVERLAY_LOGISTICS && overlay_type != OVERLAY_SICKNESS && overlay_type != OVERLAY_EFFICIENCY &&
        overlay_type != OVERLAY_HEALTH && overlay_type != OVERLAY_EMPLOYMENT;
    Building building(building_get(building_id));
    ::building *overlay_building = building_get(building_id);
    if (overlay_requires_house && !building.has_house_size()) {
        return 0;
    }
    if (overlay->get_tooltip_for_building) {
        return overlay->get_tooltip_for_building(c, overlay_building);
    } else if (overlay->get_tooltip_for_grid_offset) {
        return overlay->get_tooltip_for_grid_offset(c, grid_offset);
    }
    return 0;
}
