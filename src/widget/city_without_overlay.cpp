#include "building/building_type.h"
#include "building/connectable.h"
#include "building/construction.h"
#include "building/construction_clear.h"
#include "building/image.h"
#include "building/industry.h"
#include "building/rotation.h"
#include "building/storage.h"
#include "city/entertainment.h"
#include "city/festival.h"
#include "city/labor.h"
#include "city/view_render.h"
#include "figure/roamer_preview.h"
#include "game/performance_tracker.h"
#include "game/state.h"
#include "graphics/graphics.h"
#include "graphics/image.h"
#include "graphics/weather.h"
#include "map/building.h"
#include "map/image.h"
#include "widget/city_bridge.h"
#include "widget/city_building_ghost.h"
#include "widget/city_draw_highway.h"
#include "widget/city_figure.h"

#include "city_without_overlay.h"

#include "building/animations.h"
#include "building/building.h"
#include "building/building_record.h"
#include "building/building_type_registry_internal.h"
#include "building/dock.h"
#include "widget/city_draw.h"

#include "graphics/clouds.h"
#include "assets/assets.h"
#include "building/building_type_api.h"
#include "building/granary.h"
#include "building/monument.h"
#include "building/properties.h"
#include "city/buildings.h"
#include "city/population.h"
#include "city/ratings.h"
#include "city/view.h"
#include "core/calc.h"
#include "core/config.h"
#include "core/time.h"
#include "figure/formation_legion.h"
#include "game/resource.h"
#include "graphics/renderer.h"
#include "graphics/window.h"
#include "input/scroll.h"
#include "map/figure.h"
#include "map/grid.h"
#include "map/property.h"
#include "map/sprite.h"
#include "map/terrain.h"
#include "scenario/property.h"
#include "sound/city.h"

#include <cstring>

#define OFFSET(x,y) (x + GRID_SIZE * y)

#define SELECTED_BUILDING_COLOR_MASK COLOR_MASK_SKY_BLUE

static const int ADJACENT_OFFSETS[2][4][7] = {
    {
        {OFFSET(-1, 0), OFFSET(-1, -1),  OFFSET(-1, -2), OFFSET(0, -2), OFFSET(1, -2)},
        {OFFSET(0, -1), OFFSET(1, -1),  OFFSET(2, -1), OFFSET(2, 0), OFFSET(2, 1)},
        {OFFSET(1, 0), OFFSET(1, 1),  OFFSET(1, 2), OFFSET(0, 2), OFFSET(-1, 2)},
        {OFFSET(0, 1), OFFSET(-1, 1),  OFFSET(-2, 1), OFFSET(-2, 0), OFFSET(-2, -1)}
    },
    {
        {OFFSET(-1, 0), OFFSET(-1, -1),  OFFSET(-1, -2), OFFSET(-1, -3), OFFSET(0, -3),  OFFSET(1, -3), OFFSET(2, -3)},
        {OFFSET(0, -1), OFFSET(1, -1),  OFFSET(2, -1), OFFSET(3, -1), OFFSET(3, 0),  OFFSET(3, 1), OFFSET(3, 2)},
        {OFFSET(1, 0), OFFSET(1, 1),  OFFSET(1, 2), OFFSET(1, 3), OFFSET(0, 3),  OFFSET(-1, 3), OFFSET(-2, 3)},
        {OFFSET(0, 1), OFFSET(-1, 1),  OFFSET(-2, 1), OFFSET(-3, 1), OFFSET(-3, 0),  OFFSET(-3, -1), OFFSET(-3, -2)}
    }
};

static const building_type_registry_impl::BuildingType *building_type_definition_from_attr(const char *text_id)
{
    for (const std::unique_ptr<building_type_registry_impl::BuildingType> &definition :
        building_type_registry_impl::g_building_types) {
        if (definition && std::strcmp(definition->attr(), text_id) == 0) {
            return definition.get();
        }
    }
    return nullptr;
}

static int building_matches(const Building &building, const char *text_id)
{
    const building_type_registry_impl::BuildingType *definition = building.type;
    return definition && std::strcmp(definition->attr(), text_id) == 0;
}

static struct {
    time_millis last_water_animation_time;
    int advance_water_animation;

    int image_id_water_first;
    int image_id_water_last;
    unsigned int selected_figure_id;
    unsigned int highlighted_formation;
    unsigned int selected_building_id;
    unsigned int hovered_building_id;
    const map_tile *cursor_tile;
    pixel_coordinate *selected_figure_coord;

    float scale;
} draw_context;

static void init_draw_context(int selected_figure_id, pixel_coordinate *figure_coord, int highlighted_formation)
{
    draw_context.advance_water_animation = 0;
    if (!selected_figure_id) {
        time_millis now = time_get_millis();
        if (now - draw_context.last_water_animation_time > 60) {
            draw_context.last_water_animation_time = now;
            draw_context.advance_water_animation = 1;
        }
    }
    draw_context.image_id_water_first = Image::group(GROUP_TERRAIN_WATER);
    draw_context.image_id_water_last = 5 + draw_context.image_id_water_first;
    draw_context.selected_figure_id = selected_figure_id;
    draw_context.selected_figure_coord = figure_coord;
    draw_context.highlighted_formation = highlighted_formation;
    draw_context.scale = city_view_get_scale() / 100.0f;

    // Determine hovered building - only if config enabled and not scrolling
    draw_context.hovered_building_id = 0;
    if (config_get(CONFIG_UI_CV_CURSOR_SHADOW) && draw_context.cursor_tile && draw_context.cursor_tile->grid_offset &&
        !scroll_in_progress()) {
        int building_id = map_building_at(draw_context.cursor_tile->grid_offset);
        if (building_id) {
            draw_context.hovered_building_id = Building(building_get(building_id)).main().id();

        }
    }
}

static int draw_building_as_deleted(const Building &building)
{
    Building main_building = building.main();
    return main_building.id() && (main_building.is_deleted() || map_property_is_deleted(main_building.grid_offset()));
}

static int is_multi_tile_terrain(int grid_offset)
{
    return (!map_building_at(grid_offset) && map_property_multi_tile_size(grid_offset) > 1);
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

static void draw_roamer_frequency(int x, int y, int grid_offset)
{
    int travel_frequency = figure_roamer_preview_get_frequency(grid_offset);
    if (travel_frequency > 0 && travel_frequency <= FIGURE_ROAMER_PREVIEW_MAX_PASSAGES) {
        static const color_t frequency_colors[] = {
            0x663377ff, 0x662266ee, 0x661155dd, 0x660044cc, 0x660033c4, 0x660022bb, 0x660011a4, 0x66000088
        };
        Image::from_id(Image::group(GROUP_TERRAIN_FLAT_TILE)).draw(x, y, frequency_colors[travel_frequency - 1], draw_context.scale);
    } else if (travel_frequency == FIGURE_ROAMER_PREVIEW_ENTRY_TILE) {
        Image::blend_footprint_color(x, y, COLOR_MASK_RED, draw_context.scale);
    } else if (travel_frequency == FIGURE_ROAMER_PREVIEW_EXIT_TILE) {
        Image::blend_footprint_color(x, y, COLOR_MASK_GREEN, draw_context.scale);
    } else if (travel_frequency == FIGURE_ROAMER_PREVIEW_ENTRY_EXIT_TILE) {
        Image::from_id(Image::group(GROUP_TERRAIN_FLAT_TILE)).draw_isometric_footprint(x, y, COLOR_MASK_PINK, draw_context.scale);
    }
}

static color_t get_building_color_mask(const Building &building)
{
    color_t color_mask = COLOR_MASK_NONE;
    const building_type type = building.type ? building.type->type() : BUILDING_NONE;
    const model_building *model = model_get_building(type);
    int labor_needed = model->laborers;
    if (!labor_needed && !building_matches(building, "warehouse_space")) {
        // account for warehouse case
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
    if (!config_get(CONFIG_UI_HIGHLIGHT_SELECTED_BUILDING)) {
        return 0;
    }
    unsigned int main_part_id = building.main().id();
    return building.id() == draw_context.selected_building_id || main_part_id == draw_context.selected_building_id;
}

static int is_building_hovered(const Building &building)
{
    if (!draw_context.hovered_building_id) {
        return 0;
    }
    unsigned int main_part_id = building.main().id();
    return building.id() == draw_context.hovered_building_id || main_part_id == draw_context.hovered_building_id;
}

static void draw_footprint(int x, int y, int grid_offset)
{
    sound_city_progress_ambient();
    building_construction_record_view_position(x, y, grid_offset);
    if (grid_offset < 0 || !map_property_is_draw_tile(grid_offset)) {
        return;
    }
    // Valid grid_offset and leftmost tile -> draw
    int building_id = map_building_at(grid_offset);
    Building building(nullptr);
    color_t color_mask = 0;
    int is_cursor_tile = (draw_context.cursor_tile && grid_offset == draw_context.cursor_tile->grid_offset);

    if (building_id) {
        building = Building(building_get(building_id));
        if (draw_building_as_deleted(building)) {
            color_mask = building_construction_clear_color();
        } else if (is_building_selected(building)) {
            color_mask = get_building_color_mask(building);
        } else if (is_building_hovered(building)) {
            // Hover effect - only if not deleted or selected
            color_mask = COLOR_MASK_HOVER;
        }
        int view_x, view_y, view_width, view_height;
        city_view_get_viewport(&view_x, &view_y, &view_width, &view_height);

        if (building.is_in_use()) {
            int direction;
            if (x < view_x + 100) {
                direction = SOUND_DIRECTION_LEFT;
            } else if (x > view_x + view_width - 100) {
                direction = SOUND_DIRECTION_RIGHT;
            } else {
                direction = SOUND_DIRECTION_CENTER;
            }
            if (building.is_unfinished_monument()) {
                sound_city_mark_construction_site_view(direction);
            } else {
                sound_city_mark_building_view(building.type ? building.type->type() : BUILDING_NONE,
                    building.worker_count(), direction, building.has_water_access());
            }
        }
    }
    if (map_terrain_is(grid_offset, TERRAIN_GARDEN)) {
        const building_type_registry_impl::BuildingType *gardens = building_type_definition_from_attr("gardens");
        sound_city_mark_building_view(gardens ? gardens->type() : BUILDING_NONE, 0, SOUND_DIRECTION_CENTER, 0);
    }

    // Apply hover effect to non-building tiles if cursor is on them, config enabled, and not scrolling
    if (!building_id && is_cursor_tile && !map_property_is_deleted(grid_offset) &&
        config_get(CONFIG_UI_CV_CURSOR_SHADOW) && !scroll_in_progress()) {
        color_mask = COLOR_MASK_HOVER;
    }

    int image_id = map_image_at(grid_offset);
    const int use_custom_ghost_preview =
        map_property_is_constructing(grid_offset) &&
        building_construction_uses_custom_ghost_preview() &&
        !building_construction_draw_as_constructing();
    if (map_property_is_constructing(grid_offset) && !use_custom_ghost_preview) { //&&
        //  !building_is_connectable(building_construction_type())) {
        image_id = Image::group(GROUP_TERRAIN_OVERLAY);
    }
    if (draw_context.advance_water_animation &&
        image_id >= draw_context.image_id_water_first &&
        image_id <= draw_context.image_id_water_last) {
        image_id++;
        if (image_id > draw_context.image_id_water_last) {
            image_id = draw_context.image_id_water_first;
        }
        map_image_set(grid_offset, image_id);
    }
    if (map_terrain_is(grid_offset, TERRAIN_HIGHWAY) && !map_terrain_is(grid_offset, TERRAIN_GATEHOUSE)) {
        city_draw_highway_footprint(x, y, draw_context.scale, grid_offset, color_mask);
    } else if (building_id &&
        building.draw_footprint({ x, y, grid_offset, color_mask, draw_context.scale })) {
        // Runtime-managed buildings draw from ImageGroupPayload here; legacy tile image ids remain as compatibility state.
    } else if (!building_id && !use_custom_ghost_preview &&
        city_draw_runtime_tile_footprint(grid_offset, x, y, color_mask, draw_context.scale)) {
        // Runtime-managed terrain tiles draw from ImageGroupPayload here; legacy tile image ids remain as compatibility state.
    } else {
        Image::from_id(image_id).draw_isometric_footprint_from_draw_tile(x, y, color_mask, draw_context.scale);
    }
    if (!building_id && config_get(CONFIG_UI_SHOW_GRID) && draw_context.scale <= 2.0f) {
        //grid is drawn by the renderer directly at zoom > 200%
        static int grid_id = 0;
        if (!grid_id) {
            grid_id = assets_get_image_id("UI", "Grid_Full");
        }
        Image::from_id(grid_id).draw(x, y, COLOR_GRID, draw_context.scale);
    }
    draw_roamer_frequency(x, y, grid_offset);
}

static void draw_hippodrome_spectators(const Building &building, int x, int y, color_t color_mask)
{
    int building_part = 1;
    if (!building.previous_part_id()) {
        building_part = 0;
    } else if (!building.next_part_id()) {
        building_part = 2;
    } else {
        building_part = 1;
    }
    int orientation = building_rotation_get_building_orientation(building.orientation());
    int population = city_population();
    if ((building_part == 0) && population > 2000) {
        // first building part
        switch (orientation) {
            case DIR_0_TOP:
                Image::from_id(Image::group(GROUP_BUILDING_HIPPODROME_2) + 6).draw(x + 147, y - 72, color_mask, draw_context.scale);
                break;
            case DIR_2_RIGHT:
                Image::from_id(Image::group(GROUP_BUILDING_HIPPODROME_1) + 8).draw(x + 58, y - 79, color_mask, draw_context.scale);
                break;
            case DIR_4_BOTTOM:
                Image::from_id(Image::group(GROUP_BUILDING_HIPPODROME_2) + 8).draw(x + 119, y - 80, color_mask, draw_context.scale);
                break;
            case DIR_6_LEFT:
                Image::from_id(Image::group(GROUP_BUILDING_HIPPODROME_1) + 6).draw(x, y - 72, color_mask, draw_context.scale);
        }
    } else if ((building_part == 1) && population > 100) {
        // middle building part
        switch (orientation) {
            case DIR_0_TOP:
            case DIR_4_BOTTOM:
                Image::from_id(Image::group(GROUP_BUILDING_HIPPODROME_2) + 7).draw(x + 122, y - 79, color_mask, draw_context.scale);
                break;
            case DIR_2_RIGHT:
            case DIR_6_LEFT:
                Image::from_id(Image::group(GROUP_BUILDING_HIPPODROME_1) + 7).draw(x, y - 80, color_mask, draw_context.scale);
        }
    } else if ((building_part == 2) && population > 1000) {
        // last building part
        switch (orientation) {
            case DIR_0_TOP:
                Image::from_id(Image::group(GROUP_BUILDING_HIPPODROME_2) + 8).draw(x + 119, y - 80, color_mask, draw_context.scale);
                break;
            case DIR_2_RIGHT:
                Image::from_id(Image::group(GROUP_BUILDING_HIPPODROME_1) + 6).draw(x, y - 72, color_mask, draw_context.scale);
                break;
            case DIR_4_BOTTOM:
                Image::from_id(Image::group(GROUP_BUILDING_HIPPODROME_2) + 6).draw(x + 147, y - 72, color_mask, draw_context.scale);
                break;
            case DIR_6_LEFT:
                Image::from_id(Image::group(GROUP_BUILDING_HIPPODROME_1) + 8).draw(x + 58, y - 79, color_mask, draw_context.scale);
                break;
        }
    }
}

static void draw_entertainment_spectators(const Building &building, int x, int y, color_t color_mask)
{
    if (building_matches(building, "hippodrome") && building.main().worker_count() > 0
        && city_entertainment_hippodrome_has_race()) {
        draw_hippodrome_spectators(building, x, y, color_mask);
    }
}

static int has_workshop_raw_materials(const Building &building)
{
    return building.loads_stored() >= 2 * resource_units_per_load() || building.industry_has_raw_materials();
}

static void draw_workshop_raw_material_storage(const Building &building, int x, int y, color_t color_mask)
{
    struct RawMaterialSprite {
        const char *text_id;
        int image_offset;
        int x;
        int y;
    };

    static const RawMaterialSprite sprites[] = {
        {"wine_workshop", 0, 45, 23},
        {"oil_workshop", 1, 35, 15},
        {"weapons_workshop", 3, 46, 24},
        {"furniture_workshop", 2, 48, 19},
        {"pottery_workshop", 4, 47, 24},
    };

    for (const RawMaterialSprite &sprite : sprites) {
        if (building_matches(building, sprite.text_id) && has_workshop_raw_materials(building)) {
            Image::from_id(Image::group(GROUP_BUILDING_WORKSHOP_RAW_MATERIAL) + sprite.image_offset)
                .draw(x + sprite.x, y + sprite.y, color_mask, draw_context.scale);
            return;
        }
    }

    if (building_matches(building, "brickworks")) {
        if (building.industry_has_raw_materials() || building.has_required_raw_amount_for_production(resource_clay())) {
            Image::from_id(Image::group(GROUP_BUILDING_WORKSHOP_RAW_MATERIAL) + 4).draw(x + 47, y + 24, color_mask, draw_context.scale);
        }
        if (building.industry_has_raw_materials() || building.has_required_raw_amount_for_production(resource_sand())) {
            Image::from_id(assets_get_image_id("Industry", "Sand_Supplied_Workshop")).draw(x + 67, y + 12, color_mask, draw_context.scale);
        }
    }
    if (building_matches(building, "concrete_maker") && has_workshop_raw_materials(building)) {
        Image::from_id(assets_get_image_id("Industry", "Sand_Supplied_Workshop")).draw(x + 47, y + 24, color_mask, draw_context.scale);
    }
}

static void get_mothball_icon_position(const Building &building, int *x, int *y)
{
    const Image &building_image = Image::from_id(building.image_id());
    int icon_id = assets_get_image_id("UI", "Mothball_Sprite");

    if (building.type && building.type->is_warehouse()) {
        *x += 21;
        *y -= 60;
    } else if (building.type && building.type->is_granary()) {
        *x += 83;
        *y -= 120;
    } else if (building_matches(building, "fountain")) {
        *x += 20;
        *y -= 15;
    } else if (building.type && building.type->is_farm()) {
        *x += 50;
        *y -= 50;
    } else {
        *x = (building_image.width() - Image::from_id(icon_id).width()) / 2;
        *y = (-Image::from_id(icon_id).height() / 2) + 10;
    }
    if (const Image *top = building_image.top()) {
        *y -= top->original_height();
    }
}

static void draw_mothball_icon(const Building &building, int x, int y, color_t color_mask, int grid_offset)
{
    if (!building.industry_is_stockpiling() && !building.is_mothballed()) {
        return;
    }
    if (!building.is_main_part()) {
        return;
    }
    if (building.type && building.type->is_farm()) {
        if (map_property_multi_tile_size(grid_offset) == 1) {
            return; //crop tile
        }
    }

    int mothball_x = 0;
    int mothball_y = 0;
    get_mothball_icon_position(building, &mothball_x, &mothball_y);
    x += mothball_x;
    y += mothball_y;

    if (building.is_mothballed()) {
        Image::from_id(assets_get_image_id("UI", "Mothball_Sprite")).draw(x, y, COLOR_MASK_NONE, draw_context.scale);
    } else if (building.industry_is_stockpiling()) {
        Image::from_id(assets_get_image_id("UI", "Stockpile_Sprite")).draw(x, y, COLOR_MASK_NONE, draw_context.scale);
    }
}

static void draw_senate_rating_flags(const Building &building, int x, int y, color_t color_mask)
{
    if (building_matches(building, "senate")) {
        // rating flags
        int image_id = Image::group(GROUP_BUILDING_SENATE);
        Image::from_id(image_id + 1).draw(x + 138, y + 44 - city_rating_culture() / 2, color_mask, draw_context.scale);
        Image::from_id(image_id + 2).draw(x + 168, y + 36 - city_rating_prosperity() / 2, color_mask, draw_context.scale);
        Image::from_id(image_id + 3).draw(x + 198, y + 27 - city_rating_peace() / 2, color_mask, draw_context.scale);
        Image::from_id(image_id + 4).draw(x + 228, y + 19 - city_rating_favor() / 2, color_mask, draw_context.scale);
        // unemployed
        image_id = Image::group(GROUP_FIGURE_HOMELESS);
        int unemployment_pct = city_labor_unemployment_percentage_for_senate();
        if (unemployment_pct > 0) {
            Image::from_id(image_id + 108).draw(x + 80, y, color_mask, draw_context.scale);
        }
        if (unemployment_pct > 5) {
            Image::from_id(image_id + 104).draw(x + 230, y - 30, color_mask, draw_context.scale);
        }
        if (unemployment_pct > 10) {
            Image::from_id(image_id + 107).draw(x + 100, y + 20, color_mask, draw_context.scale);
        }
        if (unemployment_pct > 15) {
            Image::from_id(image_id + 106).draw(x + 235, y - 10, color_mask, draw_context.scale);
        }
        if (unemployment_pct > 20) {
            Image::from_id(image_id + 106).draw(x + 66, y + 20, color_mask, draw_context.scale);
        }
    }
}

static void draw_top_for_building(Building building, int x, int y, int grid_offset)
{
    if (!map_property_is_draw_tile(grid_offset)) {
        return;
    }
    int image_id = map_image_at(grid_offset);
    color_t color_mask = 0;
    if (draw_building_as_deleted(building) || (map_property_is_deleted(grid_offset) && !is_multi_tile_terrain(grid_offset))) {
        color_mask = building_construction_clear_color();
    } else if (is_building_selected(building)) {
        color_mask = get_building_color_mask(building);
    } else if (is_building_hovered(building)) {
        // Hover effect for tops - only if not deleted or selected
        color_mask = COLOR_MASK_HOVER;
    }

    if (!building.draw_top({ x, y, grid_offset, color_mask, draw_context.scale })) {
        Image::from_id(image_id).draw_isometric_top_from_draw_tile(x, y, color_mask, draw_context.scale);
    }
    // specific buildings
    if (building.id() > 0) { //dont draw or calculate for non-buildings
        draw_senate_rating_flags(building, x, y, color_mask);
        draw_mothball_icon(building, x, y, color_mask, grid_offset);
        draw_entertainment_spectators(building, x, y, color_mask);
        draw_workshop_raw_material_storage(building, x, y, color_mask);
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

static void draw_figures(int x, int y, int grid_offset)
{
    unsigned int figure_id = map_figure_at(grid_offset);
    while (figure_id) {
        Figure *f = Figure::get(figure_id);
        if (figure_id == draw_context.selected_figure_id) {
            if (!f->is_ghost || f->height_adjusted_ticks) {
                city_draw_selected_figure(f, x, y, draw_context.scale, draw_context.selected_figure_coord);
            }
        } else if (!f->is_ghost) {
            int highlight = f->formation_id > 0 && f->formation_id == draw_context.highlighted_formation;
            city_draw_figure(f, x, y, draw_context.scale, highlight);
        }
        figure_id = f->next_figure_id_on_same_tile;
    }
}

static void draw_figures_render_tile(const CityViewRenderTile &tile)
{
    draw_figures(tile.x, tile.y, tile.grid_offset);
}

static void draw_fumigation(Building &building, int x, int y, color_t color_mask)
{
    int image_id = Image::group(GROUP_FIGURE_EXPLOSION); // smoke image_id
    image_id += building.fumigation_frame();
    Image::from_id(image_id).draw(x, y, color_mask, draw_context.scale);
    if (image_id == Image::group(GROUP_FIGURE_EXPLOSION) + 3) {
        building.set_fumigation_direction(0);
    }
    if (image_id == Image::group(GROUP_FIGURE_EXPLOSION)) {
        building.set_fumigation_direction(1);
    }
    building.animate().advance_fumigation();
}

static void get_plague_icon_position_for_house(const Building &building, int *x, int *y, int is_fumigating)
{
    const Image &building_image = Image::from_id(building.image_id());
    int icon_id = is_fumigating ? Image::group(GROUP_FIGURE_EXPLOSION) + 3 : Image::group(GROUP_PLAGUE_SKULL);

    *x = (building_image.width() - Image::from_id(icon_id).width()) / 2;
    *y = -Image::from_id(icon_id).height() / 2;

    if (const Image *top = building_image.top()) {
        *y -= top->original_height();
    }
}

static void draw_plague(Building &building, int x, int y, color_t color_mask)
{
    int x_pos = 0;
    int y_pos = 0;
    int is_fumigating = building.is_being_fumigated();

    if (building.type && building.type->has_housing()) {
        get_plague_icon_position_for_house(building, &x_pos, &y_pos, is_fumigating);
        if (x_pos || y_pos) {
            x_pos += x;
            y_pos += y;
        }
    } else if (building.type && building.type->attr() && std::strcmp(building.type->attr(), "dock") == 0) {
        if (is_fumigating) {
            x_pos = x + 68;
            y_pos = y - 38;
        } else {
            x_pos = x + 88;
            y_pos = y - 84;
        }
    } else if (building.type && building.type->is_warehouse()) {
        if (is_fumigating) {
            x_pos = x + 10;
            y_pos = y - 64;
        } else {
            x_pos = x + 12;
            y_pos = y - 84;
        }
    } else if (building.type && building.type->is_granary()) {
        if (is_fumigating) {
            x_pos = x + 70;
            y_pos = y - 114;
        } else {
            x_pos = x + 80;
            y_pos = y - 124;
        }
    }

    if (x_pos && y_pos) {
        if (is_fumigating) {
            draw_fumigation(building, x_pos, y_pos, color_mask);
        } else {
            building.set_fumigation_direction(1);
            Image::from_id(Image::group(GROUP_PLAGUE_SKULL)).draw(x_pos, y_pos, color_mask, draw_context.scale);
        }
    }
}

static int depot_cart_image_id(resource_type resource)
{
    if (resource == resource_vegetables()) {
        return assets_get_image_id("Admin_Logistics", "Cart_Depot_Vegetables");
    } else if (resource == resource_fruit()) {
        return assets_get_image_id("Admin_Logistics", "Cart_Depot_Fruit");
    } else if (resource == resource_meat()) {
        return assets_get_image_id("Admin_Logistics", "Cart_Depot_Meat");
    } else if (resource == resource_fish()) {
        return assets_get_image_id("Admin_Logistics", "Cart_Depot_Fish");
    } else if (resource == resource_vines()) {
        return assets_get_image_id("Admin_Logistics", "Cart_Depot_Grapes");
    } else if (resource == resource_pottery()) {
        return assets_get_image_id("Admin_Logistics", "Cart_Depot_Pottery");
    } else if (resource == resource_furniture()) {
        return assets_get_image_id("Admin_Logistics", "Cart_Depot_Furniture");
    } else if (resource == resource_oil()) {
        return assets_get_image_id("Admin_Logistics", "Cart_Depot_Oil");
    } else if (resource == resource_wine()) {
        return assets_get_image_id("Admin_Logistics", "Cart_Depot_Wine");
    } else if (resource == resource_marble()) {
        return assets_get_image_id("Admin_Logistics", "Cart_Depot_Marble");
    } else if (resource == resource_weapons()) {
        return assets_get_image_id("Admin_Logistics", "Cart_Depot_Weapons");
    } else if (resource == resource_clay()) {
        return assets_get_image_id("Admin_Logistics", "Cart_Depot_Clay");
    } else if (resource == resource_timber()) {
        return assets_get_image_id("Admin_Logistics", "Cart_Depot_Timber");
    } else if (resource == resource_olives()) {
        return assets_get_image_id("Admin_Logistics", "Cart_Depot_Olives");
    } else if (resource == resource_iron()) {
        return assets_get_image_id("Admin_Logistics", "Cart_Depot_Iron");
    } else if (resource == resource_gold()) {
        return assets_get_image_id("Admin_Logistics", "Cart_Depot_Gold");
    } else if (resource == resource_sand()) {
        return assets_get_image_id("Admin_Logistics", "Cart_Depot_Sand");
    } else if (resource == resource_stone()) {
        return assets_get_image_id("Admin_Logistics", "Cart_Depot_Stone");
    } else if (resource == resource_bricks()) {
        return assets_get_image_id("Admin_Logistics", "Cart_Depot_Bricks");
    }
    return assets_get_image_id("Admin_Logistics", "Cart_Depot_Wheat");
}

static void draw_depot_resource(const Building &building, int x, int y)
{
    const int image_id = building.worker_count() ?
        depot_cart_image_id(building.depot_order().resource_type) :
        assets_get_image_id("Admin_Logistics", "Cart_Depot_Cat");
    Image::from_id(image_id).draw(x + 11, y, COLOR_MASK_NONE, draw_context.scale);
}

static void draw_dock_workers(const Building &building, int x, int y, color_t color_mask)
{
    if (!building.has_plague()) {
        int num_dockers = building.dock_idle_worker_count();
        if (num_dockers > 0) {
            int image_dock = map_image_at(building.grid_offset());
            int image_dockers = Image::group(GROUP_BUILDING_DOCK_DOCKERS);
            if (image_dock == Image::group(GROUP_BUILDING_DOCK_1)) {
                image_dockers += 0;
            } else if (image_dock == Image::group(GROUP_BUILDING_DOCK_2)) {
                image_dockers += 3;
            } else if (image_dock == Image::group(GROUP_BUILDING_DOCK_3)) {
                image_dockers += 6;
            } else {
                image_dockers += 9;
            }
            if (num_dockers == 2) {
                image_dockers += 1;
            } else if (num_dockers == 3) {
                image_dockers += 2;
            }
            const image *img = image_get(image_dockers);
            if (img->animation) {
                Image::from_id(image_dockers).draw(x + img->animation->sprite_offset_x, y + img->animation->sprite_offset_y, color_mask, draw_context.scale);
            }
        }
    }
}

static void draw_permissions_flag(Building &building, int x, int y, color_t color_mask)
{
    if (building.has_plague()) {
        return;
    }
    static int base_permission_image[8];
    if (!base_permission_image[0]) {
        base_permission_image[0] = 0xdeadbeef; // Invalid image ID, just to confirm the other values have been set
        base_permission_image[1] = assets_get_image_id("UI", "Warehouse_Flag_Market");
        base_permission_image[2] = assets_get_image_id("UI", "Warehouse_Flag_Land");
        base_permission_image[3] = assets_get_image_id("UI", "Warehouse_Flag_Market_Land");
        base_permission_image[4] = assets_get_image_id("UI", "Warehouse_Flag_Sea");
        base_permission_image[5] = assets_get_image_id("UI", "Warehouse_Flag_Market_Sea");
        base_permission_image[6] = assets_get_image_id("UI", "Warehouse_Flag_Land_Sea");
        base_permission_image[7] = assets_get_image_id("UI", "Warehouse_Flag_All");
    }
    int permissions = building.blocked_storage_permission_mask();
    if (!permissions) {
        return;
    }
    Image::from_id(base_permission_image[permissions] + building.warehouse_flag_frame()).draw(x, y, color_mask, draw_context.scale);

    building.animate().advance_storage_flag(Image::from_id(base_permission_image[permissions]));
}

static void draw_warehouse_ornaments(int x, int y, color_t color_mask)
{
    Image::from_id(Image::group(GROUP_BUILDING_WAREHOUSE) + 17).draw(x - 4, y - 42, color_mask, draw_context.scale);
}

static void draw_granary_stores(const image &image, const Building &building, int x, int y, color_t color_mask)
{
    if (image.animation) {
        Image::from_id(Image::group(GROUP_BUILDING_GRANARY) + 1).draw(
            x + image.animation->sprite_offset_x,
            y + 60 + image.animation->sprite_offset_y - image.height,
            color_mask,
            draw_context.scale);
    }
    if (building.resource_amount(RESOURCE_NONE) < FULL_GRANARY) {
        Image::from_id(Image::group(GROUP_BUILDING_GRANARY) + 2).draw(x + 33, y - 60, color_mask, draw_context.scale);
    }
    if (building.resource_amount(RESOURCE_NONE) < THREEQUARTERS_GRANARY) {
        Image::from_id(Image::group(GROUP_BUILDING_GRANARY) + 3).draw(x + 56, y - 50, color_mask, draw_context.scale);
    }
    if (building.resource_amount(RESOURCE_NONE) < HALF_GRANARY) {
        Image::from_id(Image::group(GROUP_BUILDING_GRANARY) + 4).draw(x + 91, y - 50, color_mask, draw_context.scale);
    }
    if (building.resource_amount(RESOURCE_NONE) < QUARTER_GRANARY) {
        Image::from_id(Image::group(GROUP_BUILDING_GRANARY) + 5).draw(x + 117, y - 62, color_mask, draw_context.scale);
    }
}

static void draw_ceres_module_crops(int x, int y, int image_offset, color_t color_mask)
{
    int image_id = assets_get_image_id("Monuments", "Ceres Module 1 Crop");
    Image::from_id(image_id + image_offset).draw(x, y, color_mask, draw_context.scale);
}

static void draw_neptune_fountain(int x, int y, int image_offset, color_t color_mask)
{
    int image_id = assets_get_image_id("Monuments", "Neptune Module 2 Fountain");
    Image::from_id(image_id + image_offset).draw(x, y, color_mask, draw_context.scale);
}

static void draw_animation_for_building(Building building, int x, int y, int grid_offset)
{
    int image_id = map_image_at(grid_offset);
    const image *img = image_get(image_id);
    const int has_building = building.id() > 0;
    color_t color_mask = 0;
    if (draw_building_as_deleted(building) || map_property_is_deleted(grid_offset)) {
        color_mask = building_construction_clear_color();
    } else if (is_building_selected(building)) {
        color_mask = get_building_color_mask(building);
    } else if (is_building_hovered(building)) {
        // Hover effect for animations - only if not deleted or selected
        color_mask = COLOR_MASK_HOVER;
    }
    if (has_building && map_property_is_draw_tile(grid_offset) &&
        building.draw_animation({ x, y, grid_offset, color_mask, draw_context.scale })) {
        if (building.has_plague()) {
            draw_plague(building, x, y, color_mask);
        }
    } else if (has_building && img->animation) {
        if (map_property_is_draw_tile(grid_offset)) {
            if (building.type && building.type->attr() && std::strcmp(building.type->attr(), "dock") == 0) {
                draw_dock_workers(building, x, y, color_mask);
            } else if (building.type && building.type->is_warehouse()) {
                draw_warehouse_ornaments(x, y, color_mask);
                draw_permissions_flag(building, x + 19, y - 56, color_mask);
            } else if (building.type && building.type->is_granary()) {
                draw_granary_stores(*img, building, x, y, color_mask);
                draw_permissions_flag(building, x + 81, y - 101, color_mask);
            } else if (building_matches(building, "burning_ruin") && building.has_plague()) {
                Image::from_id(Image::group(GROUP_PLAGUE_SKULL)).draw(x + 18, y - 32, color_mask, draw_context.scale);
            }
            int frame_offset = building.animate().offset_for(Image::from_id(image_id), grid_offset);
            if (!building_matches(building, "hippodrome") && frame_offset > 0) {
                int y_offset = img->top ? img->top->original.height - FOOTPRINT_HALF_HEIGHT : 0;
                if (frame_offset > img->animation->num_sprites) {
                    frame_offset = img->animation->num_sprites;
                }
                if (building_matches(building, "grand_temple_ceres") && building.monument_upgrade_level() == 1) {
                    draw_ceres_module_crops(x + 190, y + 95 - y_offset, building.monument_secondary_frame(), color_mask);
                }
                if (building_matches(building, "grand_temple_neptune") && building.monument_upgrade_level() == 2) {
                    draw_neptune_fountain(x + 98, y + 87 - y_offset, (frame_offset - 1) % 5, color_mask);
                }
                if (building.type && building.type->is_granary()) {
                    Image::from_id(image_id + img->animation->start_offset + frame_offset + 5).draw(x + 77, y - 49, color_mask, draw_context.scale);
                } else {
                    Image::from_id(image_id + img->animation->start_offset + frame_offset).draw(x + img->animation->sprite_offset_x, y + img->animation->sprite_offset_y - y_offset, color_mask, draw_context.scale);
                }
                if (building_matches(building, "colosseum")) {
                    int festival_id = calc_bound(city_festival_games_active(), 0, 4);
                    int extra_x = festival_id ? 57 : 127;
                    int extra_y = festival_id ? 12 : 93;
                    int overlay_id = assets_get_image_id("Monuments", "Col Base Overlay") + festival_id;
                    Image::from_id(overlay_id).draw(x + extra_x, y + extra_y - y_offset, color_mask, draw_context.scale);
                }
            }
            if (building.has_plague()) {
                draw_plague(building, x, y, color_mask);
            }
            if (building_matches(building, "depot")) {
                draw_depot_resource(building, x, y);
            }
        }
    } else if (map_property_is_draw_tile(grid_offset) && has_building && building.has_plague()) {
        draw_plague(building, x, y, color_mask);
    } else if (map_sprite_bridge_at(grid_offset)) {
        city_draw_bridge(x, y, draw_context.scale, grid_offset);
    } else if (building_is_fort(building.type ? building.type->type() : BUILDING_NONE)) {
        if (map_property_is_draw_tile(grid_offset)) {
            image_id = assets_get_image_id("Military", "Fort_Jav_Flag_Central");
            switch (building.fort_figure_type()) {
                case FIGURE_FORT_LEGIONARY: image_id += 2; break;
                case FIGURE_FORT_MOUNTED: image_id += 1; break;
                case FIGURE_FORT_JAVELIN: break;
            }
            switch (scenario_property_climate()) {
                case CLIMATE_DESERT: image_id += 3; break;
                case CLIMATE_NORTHERN: image_id += 6; break;
                default: break;
            }
            if (building.fort_figure_type() == FIGURE_FORT_INFANTRY) {
                image_id = assets_get_image_id("Military", "fort_aux_inf_flag_central");
                switch (scenario_property_climate()) {
                    case CLIMATE_DESERT: image_id += 2; break;
                    case CLIMATE_NORTHERN: image_id += 1; break;
                    default: break;
                }
            }
            if (building.fort_figure_type() == FIGURE_FORT_ARCHER) {
                image_id = assets_get_image_id("Military", "fort_aux_arch_flag_central");
                switch (scenario_property_climate()) {
                    case CLIMATE_DESERT: image_id += 2; break;
                    case CLIMATE_NORTHERN: image_id += 1; break;
                    default: break;
                }
            }
            Image::from_id(image_id).draw(x + 81, y + 5, draw_building_as_deleted(building) ? building_construction_clear_color() : COLOR_MASK_NONE, draw_context.scale);
        }
    } else if (building_matches(building, "gatehouse")) {
        int xy = map_property_multi_tile_xy(grid_offset);
        int orientation = city_view_orientation();
        if ((orientation == DIR_0_TOP && xy == EDGE_X1Y1) ||
            (orientation == DIR_2_RIGHT && xy == EDGE_X0Y1) ||
            (orientation == DIR_4_BOTTOM && xy == EDGE_X0Y0) ||
            (orientation == DIR_6_LEFT && xy == EDGE_X1Y0)) {
            image_id = Image::group(GROUP_BUILDING_GATEHOUSE);
            color_mask = draw_building_as_deleted(building) ? building_construction_clear_color() : 0;
            if (building.orientation() == 1) {
                if (orientation == DIR_0_TOP || orientation == DIR_4_BOTTOM) {
                    Image::from_id(image_id).draw(x - 22, y - 80, color_mask, draw_context.scale);
                } else {
                    Image::from_id(image_id + 1).draw(x - 18, y - 81, color_mask, draw_context.scale);
                }
            } else if (building.orientation() == 2) {
                if (orientation == DIR_0_TOP || orientation == DIR_4_BOTTOM) {
                    Image::from_id(image_id + 1).draw(x - 18, y - 81, color_mask, draw_context.scale);
                } else {
                    Image::from_id(image_id).draw(x - 22, y - 80, color_mask, draw_context.scale);
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

static void draw_elevated_figures(int x, int y, int grid_offset)
{
    int figure_id = map_figure_at(grid_offset);


    while (figure_id > 0) {
        Figure *f = Figure::get(figure_id);

        if ((f->use_cross_country && !f->is_ghost && !f->dont_draw_elevated) || f->height_adjusted_ticks) {
            int highlight = f->formation_id > 0 && f->formation_id == draw_context.highlighted_formation;
            city_draw_figure(f, x, y, draw_context.scale, highlight);
        } else if (f->building.id() == draw_context.selected_building_id) { //figure originates from selected building
            if (config_get(CONFIG_UI_SHOW_ROAMING_PATH)) {
                int highlight = FIGURE_HIGHLIGHT_GREEN;
                if (f->type == FIGURE_MARKET_SUPPLIER || f->type == FIGURE_DELIVERY_BOY) {
                    highlight = FIGURE_HIGHLIGHT_RED; //green highlight makes market supplier look indistinguishable
                }
                city_draw_figure(f, x, y, draw_context.scale, highlight);
            }

        }
        figure_id = f->next_figure_id_on_same_tile;
    }
}

static void draw_elevated_figures_render_tile(const CityViewRenderTile &tile)
{
    draw_elevated_figures(tile.x, tile.y, tile.grid_offset);
}

static void draw_hippodrome_ornaments_for_building(Building building, int x, int y, int grid_offset)
{
    int image_id = map_image_at(grid_offset);
    const image *img = image_get(image_id);
    if (img->animation && map_property_is_draw_tile(grid_offset) && building_matches(building, "hippodrome")) {
        int top_height = img->top ? img->top->original.height : 0;
        Image::from_id(image_id + 1).draw(
            x + img->animation->sprite_offset_x,
            y + img->animation->sprite_offset_y - top_height + FOOTPRINT_HALF_HEIGHT,
            draw_building_as_deleted(building) ? building_construction_clear_color() : COLOR_MASK_NONE,
            draw_context.scale);
    }
}

static void draw_hippodrome_ornaments(int x, int y, int grid_offset)
{
    draw_hippodrome_ornaments_for_building(Building(building_get(map_building_at(grid_offset))), x, y, grid_offset);
}

static void draw_hippodrome_ornaments_render_tile(const CityViewRenderTile &tile)
{
    draw_hippodrome_ornaments_for_building(tile.building, tile.x, tile.y, tile.grid_offset);
}

static int should_draw_top_before_deletion(int grid_offset)
{
    return (is_multi_tile_terrain(grid_offset) != 0) && (has_adjacent_deletion(grid_offset) != 0);
}

static void deletion_draw_terrain_top(int x, int y, int grid_offset)
{
    if (map_property_is_draw_tile(grid_offset) && should_draw_top_before_deletion(grid_offset)) {
        draw_top(x, y, grid_offset);
    }
}

static void deletion_draw_figures_animations(int x, int y, int grid_offset)
{
    if (map_property_is_deleted(grid_offset) || draw_building_as_deleted(Building(building_get(map_building_at(grid_offset))))) {
        color_t color = building_construction_clear_color();
        if (color == COLOR_MASK_RED || color == COLOR_MASK_GREEN) {
            Image::blend_footprint_color(x, y, color, draw_context.scale);
            // blending mode only work in standard RED or GREEN, any other colors have to be drawn flat.
            // passing a different color will just draw the red blending by default
        } else {
            Image::from_id(Image::group(GROUP_TERRAIN_FLAT_TILE)).draw_isometric_footprint(x, y, color, draw_context.scale);
            // no blending, just draw the flat tile
        }
    }
    if (map_property_is_draw_tile(grid_offset) && !should_draw_top_before_deletion(grid_offset)) {
        draw_top(x, y, grid_offset);
    }
    draw_figures(x, y, grid_offset);
    draw_animation(x, y, grid_offset);
}

static void deletion_draw_remaining(int x, int y, int grid_offset)
{
    draw_elevated_figures(x, y, grid_offset);
    draw_hippodrome_ornaments(x, y, grid_offset);
}

static void draw_connectable_construction_ghost(int x, int y, int grid_offset)
{
    if (!map_property_is_constructing(grid_offset)) {
        return;
    }
    static building b;
    b.type = static_cast<building_type>(building_construction_type());
    if (building_connectable_gate_type(b.type) && map_terrain_is(grid_offset, TERRAIN_ROAD)) {
        b.type = static_cast<building_type>(building_connectable_gate_type(b.type));
    }
    b.grid_offset = grid_offset;
    if (building_properties_for_type(b.type)->rotation_offset) {
        b.subtype.orientation = building_rotation_get_rotation();
    }
    int image_id = building_image_get(&b);
    Image::from_id(image_id).draw_isometric_footprint_from_draw_tile(x, y, COLOR_MASK_BUILDING_GHOST, draw_context.scale);
    Image::from_id(image_id).draw_isometric_top_from_draw_tile(x, y, COLOR_MASK_BUILDING_GHOST, draw_context.scale);
}

static int get_highlighted_formation_id(const map_tile *tile)
{
    if (!config_get(CONFIG_UI_HIGHLIGHT_LEGIONS)) {
        return 0;
    }
    int highlighted_formation_id = formation_legion_or_herd_at_grid_offset(tile->grid_offset);
    if (highlighted_formation_id <= 0) {
        return 0;
    }
    formation *highlighted_formation = formation_get(highlighted_formation_id);
    if (highlighted_formation->in_distant_battle) {
        return 0;
    }
    int selected_formation_id = formation_get_selected();
    // don't highlight friendly legions if we've already selected one
    if (selected_formation_id && highlighted_formation_id != selected_formation_id && !highlighted_formation->is_herd) {
        return 0;
    }
    // don't highlight herds unless we have a formation selected
    if (!selected_formation_id && highlighted_formation->is_herd) {
        return 0;
    }
    if (config_get(CONFIG_GP_CH_AUTO_KILL_ANIMALS) && highlighted_formation->is_herd) {
        return 0;
    }
    return highlighted_formation_id;
}

static void update_clouds(void)
{
    int camera_x, camera_y;
    if (game_state_is_paused() || (!window_is(WINDOW_CITY) && !window_is(WINDOW_CITY_MILITARY))) {
        clouds_pause();
    }
    city_view_get_camera_in_pixels(&camera_x, &camera_y);
    clouds_draw(camera_x, camera_y, GRID_SIZE * 60, GRID_SIZE * 30, draw_context.scale);
}

/***
 * TODO:
 *
 * The following code should be executed, depending on a new "console window" output
 *
 * For now, we'll leave it commented out so it's added in the future.
 */

 /**
 static void draw_routing(int x, int y, int grid_offset)
 {
     int tx = map_grid_offset_to_x(grid_offset);
     int ty = map_grid_offset_to_y(grid_offset);
     map_routing_distance_grid *distance = map_routing_get_distance_grid();
     int16_t dist = distance->determined.items[grid_offset];
     if (!dist) {
         return;
     }
     if (tx == distance->dst_x && ty == distance->dst_y) {
         int dst_image_id = assets_get_image_id("UI", "Happy God Icon");
         Image::from_id(dst_image_id).draw(x + 29 - 10, y + 15 - 10, 0, 1);
     }
     uint8_t text[20];
     string_from_int(text, dist, 0);
     text_draw_centered(text, x, y, 58, FONT_NORMAL_BLACK, COLOR_WHITE);
 }

 static void draw_highway_terrain(int x, int y, int grid_offset)
 {
     int offset = -8;
     int terrain = map_terrain_get(grid_offset);
     if (terrain & TERRAIN_HIGHWAY_TOP_LEFT) {
         text_draw_centered("TL", x + offset, y + 6, 58, FONT_SMALL_PLAIN, COLOR_WHITE);
         offset += 16;
     }
     if (terrain & TERRAIN_HIGHWAY_TOP_RIGHT) {
         text_draw_centered("TR", x + offset, y + 6, 58, FONT_SMALL_PLAIN, COLOR_WHITE);
         offset += 16;
     }
     if (terrain & TERRAIN_HIGHWAY_BOTTOM_LEFT) {
         text_draw_centered("BL", x + offset, y + 6, 58, FONT_SMALL_PLAIN, COLOR_WHITE);
         offset += 16;
     }
     if (terrain & TERRAIN_HIGHWAY_BOTTOM_RIGHT) {
         text_draw_centered("BR", x + offset, y + 6, 58, FONT_SMALL_PLAIN, COLOR_WHITE);
         offset += 16;
     }
 }

 static void draw_tile_coords(int x, int y, int grid_offset)
 {
     int tx = map_grid_offset_to_x(grid_offset);
     int ty = map_grid_offset_to_y(grid_offset);
     uint8_t text[20];
     string_from_int(text, tx, 0);
     int len = string_length(text);
     string_copy(",", text + len, 2);
     len++;
     //string_from_int(text + len, ty, 0);
     //text_draw_centered(text, x, y + 4, 58, FONT_SMALL_PLAIN, COLOR_WHITE);
     string_from_int(text, grid_offset, 0);
     text_draw_centered(text, x, y + 10, 58, FONT_SMALL_PLAIN, COLOR_WHITE);
 }

 static void draw_road_network_id(int x, int y, int grid_offset)
 {
     int road_network_id = map_road_network_get(grid_offset);
     uint8_t text[20];
     string_from_int(text, road_network_id, 0);
     text_draw_centered(text, x, y + 4, 58, FONT_NORMAL_BLACK, COLOR_WHITE);
 }
 ***/

void city_without_overlay_draw(int selected_figure_id, pixel_coordinate *figure_coord, const map_tile *tile, unsigned int roamer_preview_building_id)
{
    PerformanceTrackerScope city_draw_scope(PERFORMANCE_TRACKER_BUCKET_CITY_DRAW);
    int highlighted_formation_id = get_highlighted_formation_id(tile);
    draw_context.cursor_tile = (map_tile *) tile;//store the tile under the cursor
    init_draw_context(selected_figure_id, figure_coord, highlighted_formation_id);

    if (roamer_preview_building_id) {
        draw_context.selected_building_id = roamer_preview_building_id;//store the clicked building id
    }
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
                draw_top_render_tile,
                draw_figures_render_tile,
                draw_animation_render_tile,
                PERFORMANCE_TRACKER_BUCKET_CITY_DRAW_MAIN_TOP,
                PERFORMANCE_TRACKER_BUCKET_CITY_DRAW_MAIN_FIGURES,
                PERFORMANCE_TRACKER_BUCKET_CITY_DRAW_MAIN_ANIMATION
            );
        }
        if (!selected_figure_id) {
            PerformanceTrackerScope scope(PERFORMANCE_TRACKER_BUCKET_CITY_DRAW_GHOST);
            if (building_is_connectable(building_construction_type())) {
                city_view_foreach_valid_map_tile(draw_connectable_construction_ghost);
            }
            city_building_ghost_draw(tile);
        }
        {
            PerformanceTrackerScope scope(PERFORMANCE_TRACKER_BUCKET_CITY_DRAW_ELEVATED);
            city_view_foreach_valid_render_tile_row(
                draw_elevated_figures_render_tile,
                draw_hippodrome_ornaments_render_tile,
                0
            );
        }
    } else {
        PerformanceTrackerScope scope(PERFORMANCE_TRACKER_BUCKET_CITY_DRAW_DELETION);
        city_view_foreach_valid_map_tile(deletion_draw_terrain_top);
        city_view_foreach_valid_map_tile(deletion_draw_figures_animations);
        city_view_foreach_valid_map_tile(deletion_draw_remaining);
    }
    {
        PerformanceTrackerScope scope(PERFORMANCE_TRACKER_BUCKET_CITY_DRAW_CLOUDS);
        update_clouds();
    }
    {
        PerformanceTrackerScope scope(PERFORMANCE_TRACKER_BUCKET_CITY_DRAW_WEATHER);
        update_weather();
    }
}
