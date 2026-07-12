#include "building/building_record.h"
#include "maintenance.h"

#include "building/building.h"
#include "building/building_runtime_internal.h"
#include "building/building_type_registry_internal.h"
#include "building/destruction.h"
#include "building/house.h"
#include "building/image.h"
#include "building/list.h"
#include "building/monument.h"
#include "city/buildings.h"
#include "city/map.h"
#include "city/message.h"
#include "city/population.h"
#include "city/sentiment.h"
#include "city/view.h"
#include "city/warning.h"
#include "core/calc.h"
#include "core/random.h"
#include "figure/route.h"
#include "figuretype/migrant.h"
#include "game/state.h"
#include "game/tutorial.h"
#include "game/undo.h"
#include "map/building.h"
#include "map/building_tiles.h"
#include "map/grid.h"
#include "map/random.h"
#include "map/road_access.h"
#include "map/road_network.h"
#include "map/terrain.h"
#include "map/tiles.h"
#include "scenario/property.h"
#include "sound/effect.h"

static struct {
    int fire_spread_direction;
    int obstruction_message_displayed;
} data;

static int composed_main_uses_union_road_access(const building *b)
{
    const building_type_registry_impl::BuildingType *definition =
        b ? building_type_registry_impl::definition_for_type(b->type) : nullptr;
    return definition && definition->has_composition() &&
        !definition->is_warehouse() &&
        !building_type_registry_impl::type_attr_is(b->type, "hippodrome") &&
        !building_is_fort(b->type);
}

static int composed_main_road_access_area(const Building &building_object, int *x, int *y, int *size)
{
    const building *b = building_object.record();
    if (!b || !x || !y || !size || !composed_main_uses_union_road_access(b)) {
        return 0;
    }

    int min_x = b->x;
    int min_y = b->y;
    int max_x = b->x + b->size;
    int max_y = b->y + b->size;
    int saw_child = 0;
    building_object.for_each_part([&](Building part_object) {
        const building *part = part_object.record();
        if (part != b) {
            saw_child = 1;
        }
        if (part->x < min_x) {
            min_x = part->x;
        }
        if (part->y < min_y) {
            min_y = part->y;
        }
        if (part->x + part->size > max_x) {
            max_x = part->x + part->size;
        }
        if (part->y + part->size > max_y) {
            max_y = part->y + part->size;
        }
    });

    if (!saw_child) {
        const building_type_registry_impl::BuildingType *definition =
            building_type_registry_impl::definition_for_type(b->type);
        const building_type_registry_impl::ComposedBuildingDefinition &composition = definition->composition();
        int rotation = b->subtype.orientation % 4;
        if (rotation < 0) {
            rotation += 4;
        }
        const building_type_registry_impl::ComposedPartOffset main_offset =
            composition.main_offset_for_rotation(rotation);
        min_x = b->x - main_offset.x;
        min_y = b->y - main_offset.y;
        const int width = rotation % 2 ? composition.footprint_height() : composition.footprint_width();
        const int height = rotation % 2 ? composition.footprint_width() : composition.footprint_height();
        max_x = min_x + width;
        max_y = min_y + height;
    }

    const int width = max_x - min_x;
    const int height = max_y - min_y;
    *x = min_x;
    *y = min_y;
    *size = width > height ? width : height;
    return *size > 0;
}

static int is_storage_road_access_type(building_type type)
{
    return building_type_registry_impl::type_attr_is_any(type, {"warehouse", "warehouse_space", "granary"});
}

static bool is_burning_ruin(const Building &building)
{
    return building.Rubble && building.Rubble->is_burning();
}

void building_maintenance_update_fire_direction(void)
{
    data.fire_spread_direction = random_byte() & 7;
}

void building_maintenance_update_burning_ruins(void)
{
    scenario_climate climate = scenario_property_climate();
    int recalculate_terrain = 0;
    building_list_burning_clear();
    Building::for_each([&](Building *building_object) {
        building *b = const_cast<::building *>(building_object->record());
        if (!is_burning_ruin(*building_object)) {
            return;
        }
        const RubbleDef &rubble_definition = *building_object->Rubble->definition();
        if (b->fire_duration < 0) {
            b->fire_duration = 0;
        }
        b->fire_duration++;
        if (b->fire_duration > rubble_definition.burn_days) {
            game_undo_disable();
            building_change_type(b, rubble_definition.decay_type->type());
            b->fire_duration = 0;
            b->state = BUILDING_STATE_RUBBLE;
            if (building_runtime *runtime = building_runtime_impl::get_or_create_instance(b)) {
                map_building_tiles_add_rubble(runtime->building, b->x, b->y, building_image_get(&runtime->building));
            }
            recalculate_terrain = 1;
            return;
        }
        if (b->has_plague) {
            return;
        }
        building_list_burning_add(b->id);
        if (climate == CLIMATE_DESERT) {
            if (b->fire_duration & 3) { // check spread every 4 ticks
                return;
            }
        } else {
            if (b->fire_duration & 7) { // check spread every 8 ticks
                return;
            }
        }
        if ((b->house_figure_generation_delay & 3) != (random_byte() & 3)) {
            return;
        }
        int dir1 = data.fire_spread_direction - 1;
        if (dir1 < 0) {
            dir1 = 7;
        }
        int dir2 = data.fire_spread_direction + 1;
        if (dir2 > 7) {
            dir2 = 0;
        }

        int grid_offset = b->grid_offset;
        auto spread_fire_to = [&recalculate_terrain](int target_offset) {
            if (!map_building_exists_at(target_offset)) {
                return 0;
            }
            Building &target_building = map_building_at(target_offset);
            if (target_building.is_surface_terrain_tile()) {
                return 0;
            }
            building *target = const_cast<::building *>(target_building.record());
            if (!target || target->fire_proof) {
                return 0;
            }
            building_destroy_by_fire(target);
            sound_effect_play(SOUND_EFFECT_EXPLOSION);
            recalculate_terrain = 1;
            return 1;
        };
        if (!spread_fire_to(grid_offset + map_grid_direction_delta(data.fire_spread_direction)) &&
            !spread_fire_to(grid_offset + map_grid_direction_delta(dir1))) {
            spread_fire_to(grid_offset + map_grid_direction_delta(dir2));
        }
    });
    if (recalculate_terrain) {
        Route::updateLandTerrain();
    }
}

int building_maintenance_get_closest_burning_ruin(int x, int y, int *distance)
{
    int min_free_building_id = 0;
    int min_occupied_building_id = 0;
    int min_occupied_dist = *distance = 10000;

    int burning_size = building_list_burning_size();
    for (int i = 0; i < burning_size; i++) {
        int building_id = building_list_burning_item(i);
        Building *burning_building = Building::get(static_cast<unsigned int>(building_id));
        if (!burning_building || !is_burning_ruin(*burning_building)) {
            continue;
        }
        building *b = const_cast<::building *>(burning_building->record());
        if (b->has_plague) {
            continue;
        }
        int dist = calc_maximum_distance(x, y, b->x, b->y);
        if (b->figure_id4) {
            if (dist < min_occupied_dist) {
                min_occupied_dist = dist;
                min_occupied_building_id = building_id;
            }
        } else if (dist < *distance) {
            *distance = dist;
            min_free_building_id = building_id;
        }
    }
    if (!min_free_building_id && min_occupied_dist <= 2) {
        min_free_building_id = min_occupied_building_id;
        *distance = 2;
    }
    return min_free_building_id;
}

static void collapse_building(building *b)
{
    city_message_apply_sound_interval(MESSAGE_CAT_COLLAPSE);
    if (!tutorial_handle_collapse()) {
        city_message_post_with_popup_delay(MESSAGE_CAT_COLLAPSE, MESSAGE_COLLAPSED_BUILDING, b->type, b->grid_offset);
    }

    game_undo_disable();
    building_destroy_by_collapse(b);
}

static void fire_building(building *b)
{
    city_message_apply_sound_interval(MESSAGE_CAT_FIRE);
    if (!tutorial_handle_fire()) {
        city_message_post_with_popup_delay(MESSAGE_CAT_FIRE, MESSAGE_FIRE, b->type, b->grid_offset);
    }

    building_destroy_by_fire(b);
    sound_effect_play(SOUND_EFFECT_EXPLOSION);
}

void building_maintenance_check_fire_collapse(void)
{
    city_sentiment_reset_protesters_criminals();

    scenario_climate climate = scenario_property_climate();
    int recalculate_terrain = 0;
    int random_global = random_byte() & 7;
    if (city_population() < 10) {
        return; // skip fire/collapse checks in very early game to avoid frustrating the player
    }
    Building::for_each([&](Building *building_object) {
        building *b = const_cast<::building *>(building_object->record());
        if (building_object->is_surface_terrain_tile()) {
            b->fire_risk = 0;
            b->damage_risk = 0;
            return;
        }
        if (b->state != BUILDING_STATE_IN_USE || b->fire_proof || b->state == BUILDING_STATE_RUBBLE) {
            return;
        }
        if (building_type_registry_impl::type_attr_is(b->type, "hippodrome") && b->prev_part_building_id) {
            return;
        }
        int random_building = (b->id + map_random_get(b->grid_offset)) & 7;
        int house_level = building_house_legacy_level(*building_object);
        // damage
        b->damage_risk += random_building == random_global ? 3 : 1;
        if (tutorial_extra_damage_risk()) {
            b->damage_risk += 5;
        }
        if (b->house_size && house_level >= HOUSE_MIN && house_level <= HOUSE_LARGE_TENT) {
            b->damage_risk = 0;
        }
        if (b->damage_risk > 200) {
            collapse_building(b);
            recalculate_terrain = 1;
            return;
        }
        // fire
        if (random_building == random_global) {
            int fire_increase = 0;
            if (!b->house_size) {
                fire_increase += 5;
            } else if (b->house_population <= 0) {
                fire_increase = 0;
            } else if (house_level >= HOUSE_MIN && house_level <= HOUSE_LARGE_SHACK) {
                fire_increase += 10;
            } else if (house_level >= HOUSE_MIN && house_level <= HOUSE_GRAND_INSULA) {
                fire_increase += 5;
            } else {
                fire_increase += 2;
            }
            if (tutorial_extra_fire_risk()) {
                fire_increase += 5;
            }
            if (climate == CLIMATE_NORTHERN) {
                fire_increase = 0;
            } else if (climate == CLIMATE_DESERT) {
                fire_increase += 3;
            }

            b->fire_risk = static_cast<short>(b->fire_risk + fire_increase);
        }
        if (b->fire_risk > 100) {
            fire_building(b);
            recalculate_terrain = 1;
        }
    });

    if (recalculate_terrain) {
        Route::updateLandTerrain();
    }
}

void building_maintenance_check_rome_access(void)
{
    const map_tile *entry_point = city_map_entry_point();
    const Route::DistanceQuery entry_route =
        Route::DistanceQuery::fromPoint({ entry_point->x, entry_point->y });
    int problem_grid_offset = 0;
    Building::for_each([&](Building *building_object) {
        building *b = const_cast<::building *>(building_object->record());
        if (b->state != BUILDING_STATE_IN_USE) {
            return;
        }
        Route::RoadResult road_access;
        b->distance_from_entry = 0;
        if (b->prev_part_building_id > 0) {
            building *main_building = building_main(b);
            if (main_building) {
                b->road_network_id = main_building->road_network_id;
                b->distance_from_entry = main_building->distance_from_entry;
                b->road_access_x = main_building->road_access_x;
                b->road_access_y = main_building->road_access_y;
                b->has_road_access = main_building->has_road_access;
                b->labor_access_score = main_building->labor_access_score;
            }
            return;
        }
        if (b->house_size) {
            int x_road = 0;
            int y_road = 0;
            if (!map_closest_road_within_radius(b->x, b->y, b->size, 2, &x_road, &y_road)) {
                // no road: eject people
                b->house_unreachable_ticks++;
                if (b->house_unreachable_ticks > 4) {
                    if (b->house_population) {
                        migrant_create_homeless(*building_object, b->house_population);
                        b->house_population = 0;
                        b->house_unreachable_ticks = 0;
                    }
                    b->state = BUILDING_STATE_UNDO;
                }
            } else {
                Route::RoadResult route = entry_route.findRoad({ x_road, y_road });
                if (route) {
                    // reachable from rome
                    b->distance_from_entry = static_cast<short>(route.distance);
                    b->house_unreachable_ticks = 0;
                } else if ((route = entry_route.findReachableRoad(b->x, b->y, b->size, 2))) {
                    x_road = route.road.x;
                    y_road = route.road.y;
                    b->distance_from_entry = static_cast<short>(route.distance);
                    b->house_unreachable_ticks = 0;
                } else {
                    // no reachable road in radius
                    if (!b->house_unreachable_ticks) {
                        b->distance_from_entry = 1;
                        problem_grid_offset = b->grid_offset;
                    }
                    b->house_unreachable_ticks++;
                    if (b->house_unreachable_ticks > 8) {
                        b->house_unreachable_ticks = 0;
                        b->state = BUILDING_STATE_UNDO;
                    }
                }
                b->road_access_x = static_cast<unsigned char>(x_road);
                b->road_access_y = static_cast<unsigned char>(y_road);
            }
        } else if (building_type_registry_impl::type_attr_is(b->type, "granary")) {
            map_point road_acces_point;
            if (map_has_road_access_granary(b->x, b->y, &road_acces_point)) {
                road_access = entry_route.findRoad(road_acces_point);
            }
        } else if (building_type_registry_impl::type_attr_is(b->type, "warehouse")) {
            map_point road_acces_point;
            if (map_has_road_access_warehouse(b->x, b->y, &road_acces_point)) {
                road_access = entry_route.findRoad(road_acces_point);
            }
        } else {
            int access_x = 0;
            int access_y = 0;
            int access_size = 0;
            if (composed_main_road_access_area(*building_object, &access_x, &access_y, &access_size)) {
                road_access = entry_route.findRoadToLargestNetwork(access_x, access_y, access_size);
            } else if (building_type_registry_impl::type_attr_is(b->type, "hippodrome")) {
                int rotated = b->subtype.orientation != 0;
                road_access = entry_route.findHippodromeRoadToLargestNetwork(b->x, b->y, rotated);
            } else if (building_monument_is_unfinished_monument(b)) {
                road_access = entry_route.findMonumentConstructionRoadToLargestNetwork(b->x, b->y, b->size);
            } else if (building_is_fort(b->type)) {
                road_access = entry_route.findRoadToLargestNetwork(b->x, b->y, b->size);
                if (!road_access) {
                    road_access = entry_route.findReachableTile(b->x, b->y, b->size, 1);
                }
            } else {
                road_access = entry_route.findRoadToLargestNetwork(b->x, b->y, b->size);
            }
        }
        if (road_access) {
            b->road_network_id = static_cast<unsigned char>(map_road_network_get(road_access.grid_offset));
            b->distance_from_entry = static_cast<short>(road_access.distance);
            b->road_access_x = static_cast<unsigned char>(road_access.road.x);
            b->road_access_y = static_cast<unsigned char>(road_access.road.y);
        }
        if (!is_storage_road_access_type(b->type) && b->house_unreachable_ticks == 0) {
            b->has_road_access = b->distance_from_entry > 0;
        }
    });
    const map_tile *exit_point = city_map_exit_point();

    if (!entry_route.distanceTo(exit_point->grid_offset)) {
        // no route through city
        if (city_population() <= 0) {
            return;
        }
        if (!data.obstruction_message_displayed) {
            data.obstruction_message_displayed = 1;
            city_message_post(1, MESSAGE_ROAD_TO_ROME_WARNING, 0, 0);
            game_state_pause();
            return;
        }
        for (int i = 0; i < 15; i++) {
            Route::deleteFirstWallOrAqueduct(entry_point->x, entry_point->y);
            Route::deleteFirstWallOrAqueduct(exit_point->x, exit_point->y);

            map_tiles_update_all_walls();
            map_tiles_update_all_aqueducts(0);
            map_tiles_update_all_empty_land();
            map_tiles_update_all_meadow();

            Route::updateLandTerrain();
            Route::updateWallTerrain();

            if (entry_route.distanceTo(exit_point->grid_offset)) {
                city_message_post(1, MESSAGE_ROAD_TO_ROME_OBSTRUCTED, 0, 0);
                game_undo_disable();
                return;
            }
        }
        building_destroy_last_placed();
    } else if (problem_grid_offset) {
        // parts of city disconnected
        city_warning_show(WARNING_CITY_BOXED_IN, translation_for_key("TR_CITY_WARNING_CITY_BOXED_IN"));
        city_warning_show(WARNING_CITY_BOXED_IN_PEOPLE_WILL_PERISH, translation_for_key("TR_CITY_WARNING_CITY_BOXED_IN_PEOPLE_WILL_PERISH"));
        city_view_go_to_grid_offset(problem_grid_offset);
        game_state_pause();
    } else {
        data.obstruction_message_displayed = 0;
    }
}
