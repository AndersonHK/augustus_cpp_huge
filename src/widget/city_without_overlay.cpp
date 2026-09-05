#include "building/building.h"
#include "building/building_type.h"
#include "building/connectable.h"
#include "building/construction.h"
#include "building/construction_clear.h"
#include "building/industry.h"
#include "building/rotation.h"
#include "building/storage.h"
#include "city/festival.h"
#include "city/labor.h"
#include "figure/roamer_preview.h"
#include "game/performance_tracker.h"
#include "game/state.h"
#include "graphics/graphics.h"
#include "graphics/image.h"
#include "graphics/weather.h"
#include "map/building.h"
#include "map/image.h"
#include "widget/city_building_ghost.h"
#include "widget/city_draw_highway.h"
#include "widget/city_figure.h"

#include "city_without_overlay.h"

#include "building/BuildingGraphics.h"
#include "building/BuildingGraphicsState.h"
#include "building/building.h"
#include "building/building_record.h"
#include "building/building_runtime_internal.h"
#include "building/building_type_registry_internal.h"
#include "building/dock.h"
#include "widget/city_draw.h"

#include "graphics/clouds.h"
#include "assets/assets.h"
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
#include "figure/FormationDestination.h"
#include "figure/formation_legion.h"
#include "game/resource.h"
#include "graphics/renderer.h"
#include "graphics/window.h"
#include "input/scroll.h"
#include "map/figure.h"
#include "map/grid.h"
#include "map/property.h"
#include "map/terrain.h"
#include "scenario/property.h"
#include "sound/city.h"

#define SELECTED_BUILDING_COLOR_MASK COLOR_MASK_SKY_BLUE

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
        if (map_building_exists_at(draw_context.cursor_tile->grid_offset)) {
            Building &selected = map_building_at(draw_context.cursor_tile->grid_offset);
            Building *owner = selected.type && selected.type->bridge().is_bridge() ?
                &selected.dynamic_bridge_owner() :
                (selected.Composition ? selected.Composition->owner() : &selected);
            draw_context.hovered_building_id = owner ? owner->id : 0;

        }
    }
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
    if (!labor_needed && !building.matches("warehouse_space")) {
        // account for warehouse case
        color_mask = COLOR_MASK_NONE;
    } else {
        if (building.matches("latrines") || building.matches("fountain")) {
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
    Building *owner = building.type && building.type->bridge().is_bridge() ?
        &building.dynamic_bridge_owner() :
        (building.Composition ? building.Composition->owner() : const_cast<Building *>(&building));
    const unsigned int main_part_id = owner ? owner->id : 0;
    return building.id == draw_context.selected_building_id || main_part_id == draw_context.selected_building_id;
}

static int is_building_hovered(const Building &building)
{
    if (!draw_context.hovered_building_id) {
        return 0;
    }
    Building *owner = building.type && building.type->bridge().is_bridge() ?
        &building.dynamic_bridge_owner() :
        (building.Composition ? building.Composition->owner() : const_cast<Building *>(&building));
    const unsigned int main_part_id = owner ? owner->id : 0;
    return building.id == draw_context.hovered_building_id || main_part_id == draw_context.hovered_building_id;
}

static color_t building_draw_color_mask(const Building &building)
{
    if (city_draw_building_as_deleted(building)) {
        return building_construction_clear_color();
    }
    if (is_building_selected(building)) {
        return get_building_color_mask(building);
    }
    return is_building_hovered(building) ? COLOR_MASK_HOVER : 0;
}

static color_t building_draw_color_mask(const Building &building, int grid_offset, bool tint_multi_tile_deleted)
{
    if (map_property_is_deleted(grid_offset) &&
        (tint_multi_tile_deleted || !city_draw_is_multi_tile_terrain(grid_offset))) {
        return building_construction_clear_color();
    }
    return building_draw_color_mask(building);
}

static void draw_footprint_render_tile(const CityDrawTileCommand &command)
{
    const int x = command.x;
    const int y = command.y;
    const int grid_offset = command.grid_offset;
    sound_city_progress_ambient();
    building_construction_record_view_position(x, y, grid_offset);
    if (grid_offset < 0 || !map_property_is_draw_tile(grid_offset)) {
        return;
    }
    // Valid grid_offset and leftmost tile -> draw
    Building *building = command.building;
    int building_id = building ? building->id : 0;
    color_t color_mask = 0;
    int is_cursor_tile = (draw_context.cursor_tile && grid_offset == draw_context.cursor_tile->grid_offset);

    if (building_id) {
        color_mask = building_draw_color_mask(*building);
        int view_x, view_y, view_width, view_height;
        city_view_get_viewport(&view_x, &view_y, &view_width, &view_height);

        if (building->is_in_use()) {
            int direction;
            if (x < view_x + 100) {
                direction = SOUND_DIRECTION_LEFT;
            } else if (x > view_x + view_width - 100) {
                direction = SOUND_DIRECTION_RIGHT;
            } else {
                direction = SOUND_DIRECTION_CENTER;
            }
            if (building->is_unfinished_monument()) {
                sound_city_mark_construction_site_view(direction);
            } else {
                sound_city_mark_building_view(building->type ? building->type->type() : BUILDING_NONE,
                    building->worker_count(), direction, building->has_water_access());
            }
        }
    }
    if (map_terrain_is(grid_offset, TERRAIN_GARDEN)) {
        sound_city_mark_building_view(
            building_type_registry_impl::type_from_attr("gardens"), 0, SOUND_DIRECTION_CENTER, 0);
    }

    // Apply hover effect to non-building tiles if cursor is on them, config enabled, and not scrolling
    if (!building_id && is_cursor_tile && !map_property_is_deleted(grid_offset) &&
        config_get(CONFIG_UI_CV_CURSOR_SHADOW) && !scroll_in_progress()) {
        color_mask = COLOR_MASK_HOVER;
    }

    int image_id = map_image_at(grid_offset);
    if (map_property_is_constructing(grid_offset)) {
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
    const int tile_visual_first = building && building->is_surface_terrain_tile();
    const int terrain_foundation = building && building->Graphics().uses_terrain_foundation();
    if (terrain_foundation && !map_terrain_is(grid_offset, TERRAIN_HIGHWAY)) {
        city_draw_terrain_foundation_footprint(
            grid_offset, x, y, color_mask, draw_context.scale);
    }
    if (map_terrain_is(grid_offset, TERRAIN_HIGHWAY) && !map_terrain_is(grid_offset, TERRAIN_GATEHOUSE)) {
        city_draw_highway_footprint(x, y, draw_context.scale, grid_offset, color_mask);
    } else if (building_id && !tile_visual_first) {
        building->draw_footprint({ x, y, grid_offset, color_mask, draw_context.scale });
    } else if (city_draw_runtime_tile_footprint(grid_offset, x, y, color_mask, draw_context.scale)) {
        // Terrain-owned runtime tiles remain a separate provisional path.
    } else {
        Image::from_id(image_id).draw_isometric_footprint_from_draw_tile(x, y, color_mask, draw_context.scale, RENDER_DESTINATION_GEOMETRY_SHARED_CITY_TILE);
    }
    if (!building_id && config_get(CONFIG_UI_SHOW_GRID)) {
        city_draw_grid_overlay(x, y, draw_context.scale);
    }
    draw_roamer_frequency(x, y, grid_offset);
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
        if (building.matches(sprite.text_id) && has_workshop_raw_materials(building)) {
            Image::from_id(Image::group(GROUP_BUILDING_WORKSHOP_RAW_MATERIAL) + sprite.image_offset)
                .draw(x + sprite.x, y + sprite.y, color_mask, draw_context.scale);
            return;
        }
    }

    if (building.matches("brickworks")) {
        if (building.industry_has_raw_materials() || building.has_required_raw_amount_for_production(resource_clay())) {
            Image::from_id(Image::group(GROUP_BUILDING_WORKSHOP_RAW_MATERIAL) + 4).draw(x + 47, y + 24, color_mask, draw_context.scale);
        }
        if (building.industry_has_raw_materials() || building.has_required_raw_amount_for_production(resource_sand())) {
            ImageGroupEntryRef::from_group("Industry\\Sand_Supplied_Workshop", "Sand_Supplied_Workshop").draw(x + 67, y + 12, color_mask, draw_context.scale);
        }
    }
    if (building.matches("concrete_maker") && has_workshop_raw_materials(building)) {
        ImageGroupEntryRef::from_group("Industry\\Sand_Supplied_Workshop", "Sand_Supplied_Workshop").draw(x + 47, y + 24, color_mask, draw_context.scale);
    }
}

static void draw_mothball_icon(const Building &building, int x, int y)
{
    const bool mothballed = building.is_mothballed();
    if (!building.industry_is_stockpiling() && !mothballed) {
        return;
    }
    const ImageGroupEntryRef icon = ImageGroupEntryRef::from_group(
        mothballed ? "UI\\Mothball_Sprite" : "UI\\Stockpile_Sprite",
        mothballed ? "Mothball_Sprite" : "Stockpile_Sprite");
    const ImageGroupEntryRef placement_icon = ImageGroupEntryRef::from_group("UI\\Mothball_Sprite", "Mothball_Sprite");
    int mothball_x = 0;
    int mothball_y = 0;
    if (!building.mothball_status_icon_offset(
            placement_icon.width(),
            placement_icon.height(),
            &mothball_x,
            &mothball_y)) {
        return;
    }
    x += mothball_x;
    y += mothball_y;

    icon.draw(x, y, COLOR_MASK_NONE, draw_context.scale);
}

static void draw_senate_rating_flags(const Building &building, int x, int y, color_t color_mask)
{
    if (building.matches("senate")) {
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

static color_t terrain_draw_color_mask(int grid_offset, bool tint_multi_tile_deleted)
{
    if (map_property_is_deleted(grid_offset) &&
        (tint_multi_tile_deleted || !city_draw_is_multi_tile_terrain(grid_offset))) {
        return building_construction_clear_color();
    }
    return 0;
}

static void draw_top_for_building(Building *building, int x, int y, int grid_offset)
{
    if (!map_property_is_draw_tile(grid_offset)) {
        return;
    }
    color_t color_mask = building ? building_draw_color_mask(*building, grid_offset, false) :
        terrain_draw_color_mask(grid_offset, false);

    const int tile_visual_first = building && building->is_surface_terrain_tile();
    if ((!building || tile_visual_first) &&
        city_draw_runtime_tile_top(grid_offset, x, y, color_mask, draw_context.scale)) {
        return;
    }
    if (building && !tile_visual_first) {
        building->draw_top({ x, y, grid_offset, color_mask, draw_context.scale });
    } else if (!city_draw_runtime_tile_top(grid_offset, x, y, color_mask, draw_context.scale)) {
        Image::from_id(map_image_at(grid_offset)).draw_isometric_top_from_draw_tile(x, y, color_mask, draw_context.scale);
    }
    if (building) {
        draw_senate_rating_flags(*building, x, y, color_mask);
    draw_mothball_icon(*building, x, y);
        draw_workshop_raw_material_storage(*building, x, y, color_mask);
    }

}

static void draw_top_render_tile(const CityDrawTileCommand &command)
{
    draw_top_for_building(command.building, command.x, command.y, command.grid_offset);
}

static void draw_figures(Figure *first_figure, int x, int y)
{
    Figure *f = first_figure;
    while (f) {
        const bool elevated = f->draws_elevated();
        if (!elevated && f->id() == draw_context.selected_figure_id) {
            if (!f->is_ghost || f->height_adjusted_ticks) {
                city_draw_selected_figure(f, x, y, draw_context.scale, draw_context.selected_figure_coord);
            }
        } else if (!elevated && !f->is_ghost) {
            int highlight = f->formation_id > 0 && f->formation_id == draw_context.highlighted_formation;
            city_draw_figure(f, x, y, draw_context.scale, highlight);
        }
        const unsigned int next_figure_id = f->next_figure_id_on_same_tile;
        f = next_figure_id ? Figure::get(next_figure_id) : nullptr;
    }
}

static void draw_figures_render_tile(const CityDrawTileCommand &command)
{
    draw_figures(command.first_figure, command.x, command.y);
    FormationDestination::draw_at(command.grid_offset, command.x, command.y, draw_context.scale, draw_context.highlighted_formation);
}

static void draw_fumigation(Building &building, int x, int y, color_t color_mask)
{
    const int smoke_image_base = Image::group(GROUP_FIGURE_EXPLOSION);
    int image_id = smoke_image_base + building.fumigation_frame();
    Image::from_id(image_id).draw(x, y, color_mask, draw_context.scale);
    if (image_id == smoke_image_base + 3) {
        building.set_fumigation_direction(0);
    }
    if (image_id == smoke_image_base) {
        building.set_fumigation_direction(1);
    }
    building.animate().advance_fumigation();
}

static void get_plague_icon_position_for_house(const Building &building, int *x, int *y, int is_fumigating)
{
    const Image &icon = Image::from_id(is_fumigating ? Image::group(GROUP_FIGURE_EXPLOSION) + 3 : Image::group(GROUP_PLAGUE_SKULL));
    building.mothball_status_icon_offset(icon.width(), icon.height(), x, y);
}

static void draw_plague(Building &building, int x, int y, color_t color_mask)
{
    int x_pos = 0;
    int y_pos = 0;
    int is_fumigating = building.is_being_fumigated();

    if (building.Housing) {
        get_plague_icon_position_for_house(building, &x_pos, &y_pos, is_fumigating);
        if (x_pos || y_pos) {
            x_pos += x;
            y_pos += y;
        }
    } else if (building.matches("dock")) {
        x_pos = x + (is_fumigating ? 68 : 88);
        y_pos = y + (is_fumigating ? -38 : -84);
    } else if (building.type && building.type->is_warehouse()) {
        x_pos = x + (is_fumigating ? 10 : 12);
        y_pos = y + (is_fumigating ? -64 : -84);
    } else if (building.type && building.type->is_granary()) {
        x_pos = x + (is_fumigating ? 70 : 80);
        y_pos = y + (is_fumigating ? -114 : -124);
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

static void draw_animation_for_building(Building *building, int x, int y, int grid_offset)
{
    if (!building) {
        return;
    }
    const bool draw_tile = map_property_is_draw_tile(grid_offset);
    color_t color_mask = building_draw_color_mask(*building, grid_offset, true);
    if (draw_tile && !building->is_surface_terrain_tile()) {
        building->draw_animation({ x, y, grid_offset, color_mask, draw_context.scale });
        if (building->has_plague()) {
            draw_plague(*building, x, y, color_mask);
        }
    } else if (draw_tile && building->has_plague()) {
        draw_plague(*building, x, y, color_mask);
    }
}

static void draw_animation_render_tile(const CityDrawTileCommand &command)
{
    if (command.building) {
        draw_animation_for_building(command.building, command.x, command.y, command.grid_offset);
    }
}

static void draw_elevated_figures(Figure *first_figure, int x, int y)
{
    Figure *f = first_figure;
    while (f) {
        if (f->draws_elevated() && (!f->is_ghost || f->height_adjusted_ticks)) {
            int highlight = f->formation_id > 0 && f->formation_id == draw_context.highlighted_formation;
            if (f->id() == draw_context.selected_figure_id) city_draw_selected_figure(f, x, y, draw_context.scale, draw_context.selected_figure_coord);
            else city_draw_figure(f, x, y, draw_context.scale, highlight);
        } else if (f->building && f->building->id == draw_context.selected_building_id) { //figure originates from selected building
            if (config_get(CONFIG_UI_SHOW_ROAMING_PATH)) {
                int highlight = FIGURE_HIGHLIGHT_GREEN;
                if (f->type == FIGURE_MARKET_SUPPLIER || f->type == FIGURE_DELIVERY_BOY) {
                    highlight = FIGURE_HIGHLIGHT_RED; //green highlight makes market supplier look indistinguishable
                }
                city_draw_figure(f, x, y, draw_context.scale, highlight);
            }

        }
        const unsigned int next_figure_id = f->next_figure_id_on_same_tile;
        f = next_figure_id ? Figure::get(next_figure_id) : nullptr;
    }
}

static void draw_elevated_figures_render_tile(const CityDrawTileCommand &command)
{
    draw_elevated_figures(command.first_figure, command.x, command.y);
}

static void deletion_draw_terrain_top_render_tile(const CityDrawTileCommand &command)
{
    if (map_property_is_draw_tile(command.grid_offset) && city_draw_should_draw_top_before_deletion(command.grid_offset)) {
        draw_top_for_building(command.building, command.x, command.y, command.grid_offset);
    }
}

static void deletion_draw_figures_animations_render_tile(const CityDrawTileCommand &command)
{
    Building *building = command.building;
    if (map_property_is_deleted(command.grid_offset) || (building && city_draw_building_as_deleted(*building))) {
        color_t color = building_construction_clear_color();
        if (color == COLOR_MASK_RED || color == COLOR_MASK_GREEN) {
            Image::blend_footprint_color(command.x, command.y, color, draw_context.scale);
            // blending mode only work in standard RED or GREEN, any other colors have to be drawn flat.
            // passing a different color will just draw the red blending by default
        } else {
            Image::from_id(Image::group(GROUP_TERRAIN_FLAT_TILE)).draw_isometric_footprint(command.x, command.y, color, draw_context.scale);
            // no blending, just draw the flat tile
        }
    }
    if (map_property_is_draw_tile(command.grid_offset) && !city_draw_should_draw_top_before_deletion(command.grid_offset)) {
        draw_top_for_building(building, command.x, command.y, command.grid_offset);
    }
    draw_figures(command.first_figure, command.x, command.y);
    draw_animation_for_building(building, command.x, command.y, command.grid_offset);
}

static void deletion_draw_remaining_render_tile(const CityDrawTileCommand &command)
{
    draw_elevated_figures(command.first_figure, command.x, command.y);
}

static void draw_connectable_construction_ghost_render_tile(const CityDrawTileCommand &command)
{
    const int x = command.x;
    const int y = command.y;
    const int grid_offset = command.grid_offset;
    if (!map_property_is_constructing(grid_offset)) {
        return;
    }
    building_type type = static_cast<building_type>(building_construction_type());
    type = building_connectable_preview_type(type, grid_offset);
    const building_type_registry_impl::BuildingType *definition =
        building_type_registry_impl::definition_for_type(type);
    if (!definition) {
        return;
    }

    building record = {};
    record.type = type;
    record.state = BUILDING_STATE_IN_USE;
    record.grid_offset = static_cast<short>(grid_offset);
    record.x = static_cast<unsigned char>(map_grid_offset_to_x(grid_offset));
    record.y = static_cast<unsigned char>(map_grid_offset_to_y(grid_offset));
    if (building_rotation_type_has_rotations(type)) {
        record.subtype.orientation = static_cast<short>(building_rotation_get_rotation());
    }
    BuildingGraphicsState graphics_state;
    Building building(record, definition, graphics_state);
    const BuildingDrawContext context = {
        x, y, grid_offset, COLOR_MASK_BUILDING_GHOST, draw_context.scale, 1
    };
    building.draw_footprint(context);
    building.draw_top(context);
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
    CityViewRenderCommandBuffer render_commands;
    city_draw_prepare_render_tile_rows(render_commands);
    const CityViewRenderPhase footprint_phase[] = { { draw_footprint_render_tile, PERFORMANCE_TRACKER_BUCKET_CITY_DRAW_FOOTPRINT } };
    city_draw_render_tile_rows(render_commands, footprint_phase, 1);
    if (!should_mark_deleting) {
        const CityViewRenderPhase phases[] = {
            { draw_top_render_tile, PERFORMANCE_TRACKER_BUCKET_CITY_DRAW_MAIN_TOP },
            { draw_figures_render_tile, PERFORMANCE_TRACKER_BUCKET_CITY_DRAW_MAIN_FIGURES },
            { draw_animation_render_tile, PERFORMANCE_TRACKER_BUCKET_CITY_DRAW_MAIN_ANIMATION },
        };
        city_draw_render_tile_rows(render_commands, phases, 3);
        if (!selected_figure_id) {
            PerformanceTrackerScope scope(PERFORMANCE_TRACKER_BUCKET_CITY_DRAW_GHOST);
            if (building_is_connectable(building_construction_type())) {
                const CityViewRenderPhase connectable_phase[] = { { draw_connectable_construction_ghost_render_tile, PERFORMANCE_TRACKER_BUCKET_CITY_DRAW_GHOST } };
                city_draw_render_tile_rows(render_commands, connectable_phase, 1);
            }
            city_building_ghost_draw(tile);
        }
        const CityViewRenderPhase elevated_phases[] = {
            { draw_elevated_figures_render_tile, PERFORMANCE_TRACKER_BUCKET_CITY_DRAW_ELEVATED },
        };
        city_draw_render_tile_rows(render_commands, elevated_phases, 1);
    } else {
        const CityViewRenderPhase terrain_phase[] = { { deletion_draw_terrain_top_render_tile, PERFORMANCE_TRACKER_BUCKET_CITY_DRAW_DELETION } };
        const CityViewRenderPhase figures_phase[] = { { deletion_draw_figures_animations_render_tile, PERFORMANCE_TRACKER_BUCKET_CITY_DRAW_DELETION } };
        const CityViewRenderPhase remaining_phase[] = { { deletion_draw_remaining_render_tile, PERFORMANCE_TRACKER_BUCKET_CITY_DRAW_DELETION } };
        city_draw_render_tile_rows(render_commands, terrain_phase, 1);
        city_draw_render_tile_rows(render_commands, figures_phase, 1);
        city_draw_render_tile_rows(render_commands, remaining_phase, 1);
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
