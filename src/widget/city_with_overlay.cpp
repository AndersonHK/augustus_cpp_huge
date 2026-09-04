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
#include "map/grid.h"
#include "map/image.h"
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

#include "building/BuildingGraphics.h"
#include "building/BuildingGeometry.h"
#include "building/BuildingGeometryProjection.h"
#include "building/building.h"
#include "building/building_record.h"
#include "building/building_runtime_internal.h"
#include "building/building_type_registry_internal.h"
#include "widget/city_draw.h"

#include "building/properties.h"
#include "city/view.h"
#include "core/config.h"
#include "core/log.h"
#include "graphics/renderer.h"

#include "graphics/window.h"
#include "map/figure.h"
#include "map/property.h"
#include "map/random.h"
#include "map/terrain.h"
#include "widget/city_overlay.h"

static const city_overlay *overlay = 0;
static float scale = SCALE_NONE;
static unsigned int city_roamer_preview_selected_building_id = ((unsigned int) -1); //NO_POSITION default

#define SELECTED_BUILDING_COLOR_MASK COLOR_MASK_SKY_BLUE

static Building *runtime_building_for_draw_at(int grid_offset)
{
    return map_building_exists_at(grid_offset) ? &map_building_at(grid_offset) : nullptr;
}

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
    if (!labor_needed && !building.matches("warehouse_space")) { // account for warehouse case
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
    if (!config_get(CONFIG_UI_HIGHLIGHT_SELECTED_BUILDING)) { // if option not selected in config, abandon
        return 0;
    }
    Building *owner = building.type && building.type->bridge().is_bridge() ?
        &building.dynamic_bridge_owner() :
        (building.Composition ? building.Composition->owner() : const_cast<Building *>(&building));
    return building.id == city_roamer_preview_selected_building_id ||
        (owner && owner->id == city_roamer_preview_selected_building_id);
}

static void draw_flattened_building_footprint(
    const Building &building,
    int draw_grid_offset,
    int x,
    int y,
    int image_offset,
    color_t color_mask)
{
    int image_base = Image::group(GROUP_TERRAIN_OVERLAY) + image_offset;
    if (building.Housing) {
        image_base += 4;
    }
    if (map_is_bridge(draw_grid_offset)) {
        return;
    }

    const building_type_registry_impl::BuildingGeometry geometry =
        building_type_registry_impl::BuildingGeometry::query(building);
    if (!geometry.valid()) {
        return;
    }

    const int draw_x = map_grid_offset_to_x(draw_grid_offset);
    const int draw_y = map_grid_offset_to_y(draw_grid_offset);

    const std::vector<building_type_registry_impl::ProjectedBuildingGeometryCell> projected =
        building_type_registry_impl::project_building_geometry(
            geometry, draw_x, draw_y, city_view_orientation());
    for (const building_type_registry_impl::ProjectedBuildingGeometryCell &cell : projected) {
        const int tile_image = image_base + cell.overlay_image_variant;
        Image::from_id(tile_image).draw_isometric_footprint_from_draw_tile(
            x + cell.x, y + cell.y, color_mask, scale);
    }
}

static int building_draws_overlay_summary_at(const Building &building, int grid_offset)
{
    return !building.type ||
        building.type->graphics().draws_overlay_summary_at(building, grid_offset, city_view_orientation());
}

void city_with_overlay_draw_building_footprint(int x, int y, int grid_offset, int image_offset)
{
    if (!map_building_exists_at(grid_offset)) {
        return;
    }
    Building building = map_building_at(grid_offset);
    ::building *overlay_building = const_cast<::building *>(building.record());
    color_t color_mask = 0;
    if (overlay->type == OVERLAY_PROBLEMS) {
        city_overlay_problems_prepare_building(overlay_building);
    }
    if (overlay->show_building(overlay_building)) {
        if (is_building_selected(building)) {
            color_mask = get_building_color_mask(building);
        }
        building.draw_footprint({ x, y, grid_offset, color_mask, scale });
    } else {
        if (building_draws_overlay_summary_at(building, grid_offset)) {
            draw_flattened_building_footprint(building, grid_offset, x, y, image_offset, color_mask);
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

static void draw_footprint_render_tile(const CityDrawTileCommand &command)
{
    const int x = command.x;
    const int y = command.y;
    const int grid_offset = command.grid_offset;
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
        } else {
            Building *building = command.building;
            if (building && building->Graphics().uses_terrain_foundation()) {
                city_draw_terrain_foundation_footprint(
                    grid_offset, x, y, COLOR_MASK_NONE, scale);
            }
            const int runtime_tile_drawn = map_building_exists_at(grid_offset) &&
                map_building_at(grid_offset).is_surface_terrain_tile() &&
                city_draw_runtime_tile_footprint(grid_offset, x, y, COLOR_MASK_NONE, scale);
            if (!runtime_tile_drawn) {
                if (terrain & (TERRAIN_AQUEDUCT | TERRAIN_WALL)) {
                    if (terrain & TERRAIN_ROAD) {
                        city_draw_terrain_foundation_footprint(
                            grid_offset, x, y, COLOR_MASK_NONE, scale);
                    } else {
                        // display grass
                        int image_id = Image::group(GROUP_TERRAIN_GRASS_1) + (map_random_get(grid_offset) & 7);
                        Image::from_id(image_id).draw_isometric_footprint_from_draw_tile(x, y, 0, scale);
                    }
                } else {
                    const int runtime_tile_fallback_drawn =
                        city_draw_runtime_tile_footprint(grid_offset, x, y, COLOR_MASK_NONE, scale);
                    if (!runtime_tile_fallback_drawn) {
                        if ((terrain & TERRAIN_ROAD) && !(terrain & TERRAIN_BUILDING)) {
                            Image::from_id(map_image_at(grid_offset)).draw_isometric_footprint_from_draw_tile(x, y, 0, scale);
                        } else if (terrain & TERRAIN_BUILDING) {
                            if (map_is_bridge(grid_offset)) {
                                Image::from_id(map_image_at(grid_offset)).draw_isometric_footprint_from_draw_tile(x, y, 0, scale);
                            }
                            city_with_overlay_draw_building_footprint(x, y, grid_offset, 0);
                        } else {
                            Image::from_id(map_image_at(grid_offset)).draw_isometric_footprint_from_draw_tile(x, y, 0, scale);
                        }
                    }
                }
            }
        }
    }
    if (config_get(CONFIG_UI_SHOW_GRID) && map_property_is_draw_tile(grid_offset)
                                        && !map_building_exists_at(grid_offset)) {
        city_draw_grid_overlay(x, y, scale);
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
static color_t building_top_color_mask(const Building &building)
{
    return is_building_selected(building) ? get_building_color_mask(building) :
        city_draw_building_as_deleted(building) ? building_construction_clear_color() : 0;
}

static void draw_building_top(Building &building, int x, int y, int grid_offset)
{
    color_t color_mask = building_top_color_mask(building);

    if (building.matches("cart_depot")) {
        city_draw_depot_resource(building, x, y, scale);
    }

    building.draw_top({ x, y, grid_offset, color_mask, scale });
}

static void city_with_overlay_draw_building_top_for_building(Building *building, int x, int y, int grid_offset)
{
    if (!building) {
        return;
    }
    const ::building *overlay_building = building->record();
    if (overlay->show_building(overlay_building)) {
        draw_building_top(*building, x, y, grid_offset);
    } else {
        int column_height = overlay->get_column_height(overlay_building);
        if (column_height != NO_COLUMN && building_draws_overlay_summary_at(*building, grid_offset)) {
            draw_overlay_column(x, y, column_height, overlay->column_type);
        }
    }
}

void city_with_overlay_draw_building_top(int x, int y, int grid_offset)
{
    city_with_overlay_draw_building_top_for_building(runtime_building_for_draw_at(grid_offset), x, y, grid_offset);
}

static void draw_top_for_building(Building *building, int x, int y, int grid_offset)
{
    if (overlay->draw_custom_top && overlay->draw_custom_top(x, y, scale, grid_offset)) {
        return;
    }
    if (!map_property_is_draw_tile(grid_offset)) {
        return;
    }
    if (building && building->is_surface_terrain_tile()) {
        color_t color_mask = building_top_color_mask(*building);
        if (building->draw_top({ x, y, grid_offset, color_mask, scale })) {
            return;
        }
        if (city_draw_runtime_tile_top(grid_offset, x, y, color_mask, scale)) {
            return;
        }
    }
    if (map_terrain_is(grid_offset, TERRAIN_BUILDING) && building) {
        city_with_overlay_draw_building_top_for_building(building, x, y, grid_offset);
    } else if (!map_terrain_is(grid_offset, TERRAIN_BUILDING)) {
        if (!map_terrain_is(grid_offset, TERRAIN_WALL | TERRAIN_AQUEDUCT | TERRAIN_ROAD)) {
            color_t color_mask = 0;
            if (map_property_is_deleted(grid_offset) && !city_draw_is_multi_tile_terrain(grid_offset)) {
                color_mask = building_construction_clear_color();
            }
            // terrain
            if (!city_draw_runtime_tile_top(grid_offset, x, y, color_mask, scale)) {
                Image::from_id(map_image_at(grid_offset)).draw_isometric_top_from_draw_tile(x, y, color_mask, scale);
            }
        }
    }
}

static void draw_top_render_tile(const CityDrawTileCommand &command)
{
    if (command.building) {
        draw_top_for_building(command.building, command.x, command.y, command.grid_offset);
    }
}

static int overlay_draws_building_animation(const Building &building)
{
    switch (overlay->type) {
        case OVERLAY_FIRE:
        case OVERLAY_CRIME:
            return building.matches("prefecture") || building.matches("burning_ruin");
        case OVERLAY_ENEMY:
            return building.matches("prefecture") ||
                building.matches("burning_ruin") ||
                (building.type && building.type->is_watchtower());
        case OVERLAY_DAMAGE:
            return building.matches("engineers_post");
        case OVERLAY_WATER:
            return building.matches("reservoir") || building.matches("fountain");
        case OVERLAY_FOOD_STOCKS:
            return building.matches("market") || (building.type && building.type->is_granary());
        case OVERLAY_ENTERTAINMENT:
        case OVERLAY_THEATER:
        case OVERLAY_AMPHITHEATER:
        case OVERLAY_ARENA:
        case OVERLAY_COLOSSEUM:
        case OVERLAY_HIPPODROME:
        case OVERLAY_TAVERN:
            return overlay->show_building(building.record());
        default:
            return 0;
    }
}

static void draw_animation_for_building(Building *building, int x, int y, int grid_offset)
{
    if (!building || !overlay_draws_building_animation(*building) || !map_property_is_draw_tile(grid_offset)) {
        return;
    }

    color_t color_mask = building_top_color_mask(*building);
    building->draw_animation({ x, y, grid_offset, color_mask, scale });
}

static void draw_animation_render_tile(const CityDrawTileCommand &command)
{
    if (command.building) {
        draw_animation_for_building(command.building, command.x, command.y, command.grid_offset);
    }
}

static void draw_figures(Figure *first_figure, int x, int y)
{
    Figure *f = first_figure;
    while (f) {
        const bool elevated = f->draws_elevated();
        if (!elevated && !f->is_ghost && overlay->show_figure(f)) {
            city_draw_figure(f, x, y, scale, 0);
        }
        const unsigned int next_figure_id = f->next_figure_id_on_same_tile;
        f = next_figure_id ? Figure::get(next_figure_id) : nullptr;
    }
}

static void draw_figures_render_tile(const CityDrawTileCommand &command)
{
    draw_figures(command.first_figure, command.x, command.y);
}

static void draw_elevated_figures(Figure *first_figure, int x, int y)
{
    Figure *f = first_figure;
    while (f) {
        if (f->draws_elevated() && (!f->is_ghost || f->height_adjusted_ticks) && overlay->show_figure(f)) {
            city_draw_figure(f, x, y, scale, 0);
        } else if (f->building && f->building->id == city_roamer_preview_selected_building_id) { //figure from selected building
            if (config_get(CONFIG_UI_SHOW_ROAMING_PATH)) {
                int highlight = FIGURE_HIGHLIGHT_GREEN;
                if (f->type == FIGURE_MARKET_SUPPLIER || f->type == FIGURE_DELIVERY_BOY) {
                    highlight = FIGURE_HIGHLIGHT_RED; //green highlight makes market supplier look indistinguishable
                }
                city_draw_figure(f, x, y, scale, highlight);
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
    if (city_draw_should_draw_top_before_deletion(command.grid_offset)) {
        draw_top_for_building(command.building, command.x, command.y, command.grid_offset);
    }
}

static void deletion_draw_animations_render_tile(const CityDrawTileCommand &command)
{
    Building *building = command.building;
    if (map_property_is_deleted(command.grid_offset) || (building && city_draw_building_as_deleted(*building))) {
        Image::blend_footprint_color(command.x, command.y, building_construction_clear_color(), scale);
    }
    if (!city_draw_should_draw_top_before_deletion(command.grid_offset)) {
        draw_top_for_building(building, command.x, command.y, command.grid_offset);
    }
    draw_animation_for_building(building, command.x, command.y, command.grid_offset);
}

static void draw_custom_layer_render_tile(const CityDrawTileCommand &command)
{
    overlay->draw_custom_layer(command.x, command.y, scale, command.grid_offset);
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
    CityViewRenderCommandBuffer render_commands;
    city_draw_prepare_render_tile_rows(render_commands);
    const CityViewRenderPhase footprint_phase[] = { { draw_footprint_render_tile, PERFORMANCE_TRACKER_BUCKET_CITY_DRAW_FOOTPRINT } };
    city_draw_render_tile_rows(render_commands, footprint_phase, 1);
    if (!should_mark_deleting) {
        const CityViewRenderPhase phases[] = {
            { draw_figures_render_tile, PERFORMANCE_TRACKER_BUCKET_CITY_DRAW_MAIN_FIGURES },
            { draw_top_render_tile, PERFORMANCE_TRACKER_BUCKET_CITY_DRAW_MAIN_TOP },
            { draw_animation_render_tile, PERFORMANCE_TRACKER_BUCKET_CITY_DRAW_MAIN_ANIMATION },
        };
        city_draw_render_tile_rows(render_commands, phases, 3);
        {
            PerformanceTrackerScope scope(PERFORMANCE_TRACKER_BUCKET_CITY_DRAW_GHOST);
            city_building_ghost_draw(tile);
        }
        const CityViewRenderPhase elevated_phases[] = {
            { draw_elevated_figures_render_tile, PERFORMANCE_TRACKER_BUCKET_CITY_DRAW_ELEVATED },
        };
        city_draw_render_tile_rows(render_commands, elevated_phases, 1);
    } else {
        const CityViewRenderPhase figures_phase[] = { { draw_figures_render_tile, PERFORMANCE_TRACKER_BUCKET_CITY_DRAW_DELETION } };
        const CityViewRenderPhase terrain_phase[] = { { deletion_draw_terrain_top_render_tile, PERFORMANCE_TRACKER_BUCKET_CITY_DRAW_DELETION } };
        const CityViewRenderPhase animation_phase[] = { { deletion_draw_animations_render_tile, PERFORMANCE_TRACKER_BUCKET_CITY_DRAW_DELETION } };
        city_draw_render_tile_rows(render_commands, figures_phase, 1);
        city_draw_render_tile_rows(render_commands, terrain_phase, 1);
        city_draw_render_tile_rows(render_commands, animation_phase, 1);
        {
            PerformanceTrackerScope scope(PERFORMANCE_TRACKER_BUCKET_CITY_DRAW_GHOST);
            city_building_ghost_draw(tile);
        }
        const CityViewRenderPhase elevated_phase[] = { { draw_elevated_figures_render_tile, PERFORMANCE_TRACKER_BUCKET_CITY_DRAW_ELEVATED } };
        city_draw_render_tile_rows(render_commands, elevated_phase, 1);
    }
    if (overlay->draw_custom_layer) {
        const CityViewRenderPhase custom_layer_phase[] = { { draw_custom_layer_render_tile, PERFORMANCE_TRACKER_BUCKET_CITY_DRAW_OVERLAY } };
        city_draw_render_tile_rows(render_commands, custom_layer_phase, 1);
    }
    {
        PerformanceTrackerScope scope(PERFORMANCE_TRACKER_BUCKET_CITY_DRAW_WEATHER);
        update_weather();
    }
}

int city_with_overlay_get_tooltip_text(tooltip_context *c, int grid_offset)
{
    int overlay_type = overlay->type;
    Building *building = map_building_exists_at(grid_offset) ? &map_building_at(grid_offset) : nullptr;
    if (overlay->get_tooltip_for_building && !building) {
        return 0;
    }
    int overlay_requires_house =
        overlay_type != OVERLAY_WATER && overlay_type != OVERLAY_FIRE && overlay_type != OVERLAY_LEVY &&
        overlay_type != OVERLAY_DAMAGE && overlay_type != OVERLAY_NATIVE && overlay_type != OVERLAY_DESIRABILITY &&
        overlay_type != OVERLAY_PROBLEMS && overlay_type != OVERLAY_MOTHBALL && overlay_type != OVERLAY_ENEMY &&
        overlay_type != OVERLAY_LOGISTICS && overlay_type != OVERLAY_SICKNESS && overlay_type != OVERLAY_EFFICIENCY &&
        overlay_type != OVERLAY_HEALTH && overlay_type != OVERLAY_EMPLOYMENT;
    const ::building *overlay_building = building ? building->record() : nullptr;
    if (overlay_requires_house && (!building || !building->Housing)) {
        return 0;
    }
    if (overlay->get_tooltip_for_building) {
        return overlay->get_tooltip_for_building(c, overlay_building);
    } else if (overlay->get_tooltip_for_grid_offset) {
        return overlay->get_tooltip_for_grid_offset(c, grid_offset);
    }
    return 0;
}
