#include "building/data_transfer.h"
#include "building/image.h"
#include "building/local_workforce.h"
#include "figuretype/wall.h"
#include "game/undo.h"
#include "map/bridge.h"
#include "map/building.h"
#include "map/building_tiles.h"
#include "map/tiles.h"

#include "building/building.h"
#include "building/building_record.h"
#include "building/building_runtime_internal.h"
#include "building/building_type_registry_internal.h"
#include "city/ratings.h"
#include "city/culture.h"
#include "destruction.h"

#include "city/message.h"
#include "city/population.h"
#include "core/image.h"
#include "figuretype/missile.h"
#include "map/grid.h"
#include "map/property.h"
#include "map/random.h"
#include "figure/route.h"
#include "map/terrain.h"
#include "sound/effect.h"

#include <string.h>
#include <vector>

static building_type burning_ruin_type()
{
    static building_type burning_ruin = BUILDING_NONE;
    if (burning_ruin == BUILDING_NONE) {
        burning_ruin = building_type_registry_impl::type_from_attr("burning_ruin");
    }
    return burning_ruin;
}

enum {
    DESTROY_COLLAPSE = 0,
    DESTROY_FIRE = 1,
    DESTROY_NO_RUBBLE = 2,
    DESTROY_EARTHQUAKE = 3, // earthquake collapses - non repairable, rubble where possible, remove from array at once
};
static void set_rubble_grid_info_for_all_parts(building *b);

static void destroy_without_rubble(building *b)
{
    game_undo_disable();
    city_culture_remove_building_module_capacity(b);
    building_runtime *runtime = building_runtime_impl::get_or_create_instance(b);
    Building &building_object = runtime->building;
    building_object.cleanup_figure_references_for_removal();
    building_local_workforce::remove_building(building_object);
    if (b->house_size && b->house_population) {
        city_population_remove_home_removed(b->house_population);
    }
    if (building_object.type && building_object.type->roadblock().is_bridge()) {
        map_bridge_remove(b->grid_offset, 0);
    }
    building_clear_related_data(b);

    map_building_tiles_remove(&building_object, b->x, b->y);
    b->state = BUILDING_STATE_DELETED_BY_GAME;
}

static void destroy_on_fire(building *b, int plagued)
{
    game_undo_disable();
    city_culture_remove_building_module_capacity(b);
    building_runtime *runtime = building_runtime_impl::get_or_create_instance(b);
    Building &building_object = runtime->building;
    building_object.cleanup_figure_references_for_removal();
    building_local_workforce::remove_building(building_object);
    b->fire_risk = 0;
    b->damage_risk = 0;
    if (b->house_size && b->house_population) {
        city_population_remove_home_removed(b->house_population);
    }
    // save original info for rubble data
    building_type og_type = static_cast<building_type>(b->type);
    const building_type_registry_impl::BuildingType *og_definition =
        building_type_registry_impl::definition_for_type(og_type);
    int og_size = b->size;
    int og_orientation = b->subtype.orientation;
    int og_grid_offset = b->grid_offset;

    b->house_population = 0;
    b->house_size = 0;
    b->sickness_level = 0;
    b->sickness_doctor_cure = 0;
    b->fumigation_frame = 0;
    b->fumigation_direction = 0;
    b->sickness_duration = 0;
    b->output_resource_id = 0;
    b->distance_from_entry = 0;
    if (!building_can_repair_type(b->type)) {
        building_clear_related_data(b); //retain the building data in the rubble until rubble is cleared
    }

    const int waterside_building = building_object.type &&
        building_object.type->has_foundation() &&
        building_object.type->foundation().has_water_requirement();
    int num_tiles;
    if (b->size >= 2 && b->size <= 5) {
        num_tiles = b->size * b->size;
    } else {
        num_tiles = 0;
    }
    const building_type burning_ruin = burning_ruin_type();
    map_building_tiles_remove(&building_object, b->x, b->y);
    if (map_terrain_is(b->grid_offset, TERRAIN_WATER)) {
        b->state = BUILDING_STATE_RUBBLE;
    } else if (burning_ruin != BUILDING_NONE) {
        building_change_type(b, burning_ruin);
    } else {
        b->state = BUILDING_STATE_RUBBLE;
    }
    b->figure_id4 = 0;
    b->tax_income_or_storage = 0;
    b->fire_duration = (b->house_figure_generation_delay & 7) + 1;
    b->fire_proof = 1;
    b->size = 1;
    b->has_plague = static_cast<unsigned char>(plagued);
    if (!building_can_repair_type(static_cast<building_type>(og_type))) {
        memset(&b->data, 0, sizeof(b->data)); // removes all data - don't do it for repairable buildings
    }
    map_building_set_rubble_grid_building_id(og_grid_offset, b->id, og_size);
    if (building_runtime *rubble_runtime = building_runtime_impl::get_or_create_instance(b)) {
        if (rubble_runtime->building.Rubble) {
            RubbleState *rubble_state = rubble_runtime->building.Rubble->state();
            if (rubble_state) {
                rubble_state->original_grid_offset = static_cast<unsigned short>(og_grid_offset);
                rubble_state->original_size = static_cast<unsigned char>(og_size);
                rubble_state->original_orientation = static_cast<unsigned char>(og_orientation);
                rubble_state->original_type = og_definition;
            }
        }
    }
    if (!waterside_building) {
        if (building_runtime *tile_runtime = building_runtime_impl::get_or_create_instance(b)) {
            map_building_tiles_add(tile_runtime->building, b->x, b->y, 1,
                building_image_get(&tile_runtime->building), TERRAIN_BUILDING);
        }
    }

    static const int x_tiles[] = {
        0, 1, 1, 0, 2, 2, 2, 1, 0, 3, 3, 3, 3, 2, 1, 0, 4, 4, 4, 4, 4, 3, 2, 1, 0, 5, 5, 5, 5, 5, 5, 4, 3, 2, 1, 0
    };
    static const int y_tiles[] = {
        0, 0, 1, 1, 0, 1, 2, 2, 2, 0, 1, 2, 3, 3, 3, 3, 0, 1, 2, 3, 4, 4, 4, 4, 4, 0, 1, 2, 3, 4, 5, 5, 5, 5, 5, 5
    };
    for (int tile = waterside_building ? 0 : 1; tile < num_tiles; tile++) {
        int x = x_tiles[tile] + b->x;
        int y = y_tiles[tile] + b->y;
        if (map_terrain_is(map_grid_offset(x, y), TERRAIN_WATER)) {
            continue;
        }
        if (burning_ruin == BUILDING_NONE) {
            continue;
        }
        building *ruin = building_create(burning_ruin, x, y);
        if (building_runtime *ruin_tile_runtime = building_runtime_impl::get_or_create_instance(ruin)) {
            map_building_tiles_add(ruin_tile_runtime->building, ruin->x, ruin->y, 1,
                building_image_get(&ruin_tile_runtime->building), TERRAIN_BUILDING);
        }
        ruin->fire_duration = (ruin->house_figure_generation_delay & 7) + 1;
        ruin->figure_id4 = 0;
        ruin->fire_proof = 1;
        ruin->has_plague = static_cast<unsigned char>(plagued);
        if (building_runtime *ruin_runtime = building_runtime_impl::get_or_create_instance(ruin)) {
            if (ruin_runtime->building.Rubble) {
                RubbleState *rubble_state = ruin_runtime->building.Rubble->state();
                if (rubble_state) {
                    rubble_state->original_grid_offset = static_cast<unsigned short>(og_grid_offset);
                    rubble_state->original_size = static_cast<unsigned char>(og_size);
                    rubble_state->original_orientation = static_cast<unsigned char>(og_orientation);
                    rubble_state->original_type = og_definition;
                }
            }
        }
    }
    if (waterside_building) {
        Route::updateWaterTerrain();
    }
}

static void destroy_linked_parts(building *b, int destruction_method, int plagued)
{
    building_runtime *runtime = building_runtime_impl::get_or_create_instance(b);
    if (!runtime) {
        return;
    }

    Building &destroyed = runtime->building;
    std::vector<Building *> parts;
    for (Building *part = &destroyed.main(); part; part = part->next()) {
        parts.push_back(part);
        if (!part->next_part_id() || parts.size() >= 64) {
            break;
        }
    }

    for (Building *part_building : parts) {
        if (!part_building || part_building->id == destroyed.id) {
            continue;
        }
        building *part = const_cast<building *>(part_building->record());
        switch (destruction_method) {
            case DESTROY_NO_RUBBLE:
                destroy_without_rubble(part);
                break;
            case DESTROY_FIRE:
                destroy_on_fire(part, plagued);
                break;
            case DESTROY_EARTHQUAKE:
                part_building->cleanup_figure_references_for_removal();
                part->state = BUILDING_STATE_DELETED_BY_GAME;
                break;
            default:
                part_building->cleanup_figure_references_for_removal();
                map_building_tiles_set_rubble(part_building, part->x, part->y, part->size);
                part->state = BUILDING_STATE_RUBBLE;
                break;
        }
    }

    // Unlink the buildings to prevent corrupting the building table
    if (destruction_method != DESTROY_COLLAPSE) { // collapse leaves rubble which needs the links for repair
        // destroy fire would be on the same boat, but warehouses are fire-resistant so no need to include them here
        // same applies to hippodromes, which are also further non-repairable
        for (Building *part_building : parts) {
            building *part = const_cast<building *>(part_building->record());
            part->next_part_building_id = 0;
            part->prev_part_building_id = 0;
        }
    }

}

void building_destroy_by_collapse(building *b)
{
    city_culture_remove_building_module_capacity(b);
    building_runtime *runtime = building_runtime_impl::get_or_create_instance(b);
    Building &building_object = runtime->building;
    building_object.cleanup_figure_references_for_removal();
    building_local_workforce::remove_building(building_object);
    b->state = BUILDING_STATE_RUBBLE;
    if (building_type_registry_impl::type_attr_is(b->type, "tower")) {
        figure_kill_tower_sentries_in_building(b);
    }
    set_rubble_grid_info_for_all_parts(b);
    map_building_tiles_set_rubble(&building_object, b->x, b->y, b->size);
    figure_create_explosion_cloud(b->x, b->y, b->size, 0);
    destroy_linked_parts(b, DESTROY_COLLAPSE, 0);

}

void building_destroy_by_fire(building *b)
{
    destroy_on_fire(b, 0);
    destroy_linked_parts(b, DESTROY_FIRE, 0);
}

void building_destroy_by_earthquake(building *b)
{
    city_culture_remove_building_module_capacity(b);
    building_runtime *runtime = building_runtime_impl::get_or_create_instance(b);
    Building &building_object = runtime->building;
    building_object.cleanup_figure_references_for_removal();
    building_local_workforce::remove_building(building_object);
    int grid_offset = b->grid_offset; // save before destroying building
    int size = b->size;
    b->state = BUILDING_STATE_DELETED_BY_GAME;
    map_building_tiles_set_rubble(&building_object, b->x, b->y, b->size);
    destroy_linked_parts(b, DESTROY_EARTHQUAKE, 0);
    map_building_set_rubble_grid_building_id(grid_offset, 0, size);
}

void building_destroy_by_plague(building *b)
{
    destroy_on_fire(b, 1);
    destroy_linked_parts(b, DESTROY_FIRE, 1);
}

void building_destroy_without_rubble(building *b)
{
    destroy_linked_parts(b, DESTROY_NO_RUBBLE, 0);
    destroy_without_rubble(b);
}

void building_destroy_by_rioter(building *b)
{
    destroy_on_fire(b, 0);
    destroy_linked_parts(b, DESTROY_FIRE, 0);
}

int building_destroy_first_of_type(building_type type)
{
    for (building *b = building_first_of_type(type); b; b = b->next_of_type) {
        if (b->state != BUILDING_STATE_IN_USE && b->state != BUILDING_STATE_MOTHBALLED) {
            continue;
        }
        int grid_offset = b->grid_offset;
        game_undo_disable();
        building_destroy_by_collapse(b);
        Route::updateLandTerrain();
        return grid_offset;
    }
    return 0;
}

void building_destroy_last_placed(void)
{
    int highest_sequence = 0;
    building *last_building = 0;
    Building::for_each([&](Building *runtime_building) {
        building *b = const_cast<::building *>(runtime_building->record());
        if (b->state == BUILDING_STATE_CREATED || b->state == BUILDING_STATE_IN_USE) {
            if (b->created_sequence > highest_sequence) {
                highest_sequence = b->created_sequence;
                last_building = b;
            }
        }
    });
    if (last_building) {
        city_message_post(1, MESSAGE_ROAD_TO_ROME_BLOCKED, 0, last_building->grid_offset);
        game_undo_disable();
        building_destroy_by_collapse(last_building);
        Route::updateLandTerrain();
    }
}

void building_destroy_increase_enemy_damage(int grid_offset, int max_damage)
{
    if (map_building_damage_increase(grid_offset) > max_damage) {
        building_destroy_by_enemy(map_grid_offset_to_x(grid_offset),
            map_grid_offset_to_y(grid_offset), grid_offset);
    }
}

static void set_rubble_grid_info_for_all_parts(building *b)
{
    building_runtime *runtime = building_runtime_impl::get_or_create_instance(b);
    if (!runtime) {
        return;
    }
    Building &main_building = runtime->building.main();
    b = const_cast<building *>(main_building.record()); //get main warehouse building to copy data from
    const building_type_registry_impl::BuildingType *main_type = main_building.type;
    building *part = b; //initialize part iterator - start with main building
    for (int i = 0; i < 9 && part->id > 0; i++) {
        building *next_part = building_next(part);
        if (building_runtime *part_runtime = building_runtime_impl::get_or_create_instance(part)) {
            if (part_runtime->building.Rubble) {
                if (RubbleState *rubble_state = part_runtime->building.Rubble->state()) {
                    rubble_state->original_grid_offset = static_cast<unsigned short>(b->grid_offset);
                    rubble_state->original_size =
                        static_cast<unsigned char>(main_type && main_type->is_warehouse() ? 3 : b->size);
                    rubble_state->original_orientation = static_cast<unsigned char>(b->subtype.orientation);
                    rubble_state->original_type = main_type;
                }
            }
        }
        part = next_part;
    }
}

void building_destroy_by_enemy(int x, int y, int grid_offset)
{
    if (map_building_exists_at(grid_offset)) {
        Building &destroyed_building = map_building_at(grid_offset);
        building *b = const_cast<::building *>(destroyed_building.record());
        if (b->state == BUILDING_STATE_IN_USE || b->state == BUILDING_STATE_MOTHBALLED) {
            city_ratings_peace_building_destroyed(destroyed_building);
            building_destroy_by_collapse(b);
        }
    } else {
        if (map_terrain_is(grid_offset, TERRAIN_WALL)) {
            figure_kill_tower_sentries_at(x, y);
        }
        if (map_terrain_is(grid_offset, TERRAIN_GARDEN)) {
            map_terrain_remove(grid_offset, TERRAIN_CLEARABLE);
            map_tiles_update_region_empty_land(x, y, x, y);
            map_property_clear_plaza_earthquake_or_overgrown_garden(grid_offset);
            map_tiles_update_all_gardens();
        } else {
            map_building_tiles_set_rubble(nullptr, x, y, 1);
        }
    }
    figure_tower_sentry_reroute();
    map_tiles_update_area_walls(x, y, 3);
    map_tiles_update_region_aqueducts(x - 3, y - 3, x + 3, y + 3);
    Route::updateLandTerrain();
    Route::updateWallTerrain();
}
