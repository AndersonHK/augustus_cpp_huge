#include "building/building_record.h"
#include "water.h"

#include "building/BuildingGeometry.h"
#include "building/building.h"
#include "building/building_type.h"
#include "building/building_type_registry_internal.h"
#include "map/figure.h"
#include "map/grid.h"
#include "map/point.h"
#include "map/terrain.h"
#include "figure/action.h"
#include "figure/figure_runtime_api.h"
#include "figure/route.h"

static Figure *live_fishing_boat(unsigned int id)
{
    if (!id) {
        return nullptr;
    }
    Figure *figure = Figure::get(id);
    return figure && figure->state == FIGURE_STATE_ALIVE && figure->type == FIGURE_FISHING_BOAT ? figure : nullptr;
}

static int live_fishing_boat_id(unsigned int id)
{
    return live_fishing_boat(id) ? 1 : 0;
}

static int wharf_has_boat_id(const building *wharf, unsigned int boat_id)
{
    return wharf && boat_id &&
        (wharf->data.industry.fishing_boat_id == boat_id ||
            wharf->data.industry.second_fishing_boat_id == boat_id);
}

static void clear_stale_fishing_boat_slots(building *wharf)
{
    if (!wharf) {
        return;
    }
    if (wharf->data.industry.fishing_boat_id && !live_fishing_boat_id(wharf->data.industry.fishing_boat_id)) {
        wharf->data.industry.fishing_boat_id = 0;
    }
    if (wharf->data.industry.second_fishing_boat_id && !live_fishing_boat_id(wharf->data.industry.second_fishing_boat_id)) {
        wharf->data.industry.second_fishing_boat_id = 0;
    }
}

static int fishing_boat_capacity(Building wharf, building_type_registry_impl::SpawnSource source)
{
    int capacity = 0;
    const building_type_registry_impl::BuildingType *definition = wharf.type;
    if (!definition) {
        return 0;
    }
    for (const building_type_registry_impl::SpawnDelayGroup &group : definition->spawn_groups()) {
        for (const building_type_registry_impl::SpawnPolicy &policy : group.policies) {
            if (policy.special_mode != building_type_registry_impl::SpecialSpawnMode::FishingBoat) {
                continue;
            }
            if (source == building_type_registry_impl::SpawnSource::None || policy.spawn_source == source) {
                capacity += policy.capacity;
            }
        }
    }
    return capacity;
}

static int wharf_has_shipyard_fishing_boat_room(Building wharf, int live_boats)
{
    const int shipyard_capacity = fishing_boat_capacity(wharf, building_type_registry_impl::SpawnSource::Shipyard);
    if (shipyard_capacity <= 0) {
        return 0;
    }
    if (fishing_boat_capacity(wharf, building_type_registry_impl::SpawnSource::Self) > 0) {
        const building *record = wharf.record();
        return record && !live_fishing_boat_id(record->data.industry.second_fishing_boat_id);
    }
    return live_boats < shipyard_capacity;
}

static int wharf_live_fishing_boat_count(building *wharf)
{
    clear_stale_fishing_boat_slots(wharf);
    return live_fishing_boat_id(wharf->data.industry.fishing_boat_id) +
        live_fishing_boat_id(wharf->data.industry.second_fishing_boat_id);
}

static int num_surrounding_water_tiles(int grid_offset)
{
    int amount = 0;
    for (int i = 0; i < DIR_8_NONE; i++) {
        if (map_terrain_is(grid_offset + map_grid_direction_delta(i), TERRAIN_WATER)) {
            amount++;
        }
    }
    return amount;
}

static int wharf_tile(const Building &wharf, map_point *tile)
{
    if (!tile) {
        return 0;
    }

    const building_type_registry_impl::BuildingGeometry geometry =
        building_type_registry_impl::BuildingGeometry::query(wharf);
    int best_grid_offset = -1;
    int best_water_neighbors = -1;
    for (const building_type_registry_impl::BuildingGeometryPoint &candidate :
        geometry.water_candidates()) {
        if (!map_grid_is_inside(candidate.x, candidate.y, 1)) {
            continue;
        }
        const int grid_offset = map_grid_offset(candidate.x, candidate.y);
        if (!map_terrain_is(grid_offset, TERRAIN_WATER) ||
            map_terrain_is(grid_offset, TERRAIN_BUILDING)) {
            continue;
        }
        const int water_neighbors = num_surrounding_water_tiles(grid_offset);
        // The perimeter is sorted in map order. On an equally open two-cell
        // waterside face, retaining the later cell preserves the historical
        // docking point without encoding orientation-specific coordinates.
        if (water_neighbors >= best_water_neighbors) {
            best_grid_offset = grid_offset;
            best_water_neighbors = water_neighbors;
        }
    }
    if (best_grid_offset < 0) {
        return 0;
    }
    map_point_store_result(
        map_grid_offset_to_x(best_grid_offset), map_grid_offset_to_y(best_grid_offset), tile);
    return 1;
}

int map_water_wharf_live_fishing_boats(Building wharf)
{
    building *record = const_cast<building *>(wharf.record());
    return record ? wharf_live_fishing_boat_count(record) : 0;
}

Figure *map_water_wharf_live_fishing_boat(Building wharf)
{
    building *record = const_cast<building *>(wharf.record());
    if (!record) {
        return nullptr;
    }
    clear_stale_fishing_boat_slots(record);
    if (Figure *boat = live_fishing_boat(record->data.industry.fishing_boat_id)) {
        return boat;
    }
    return live_fishing_boat(record->data.industry.second_fishing_boat_id);
}

int map_water_wharf_has_self_fishing_boat_room(Building wharf)
{
    building *record = const_cast<building *>(wharf.record());
    return record && fishing_boat_capacity(wharf, building_type_registry_impl::SpawnSource::Self) > 0 &&
        !live_fishing_boat_id(record->data.industry.fishing_boat_id);
}

int map_water_assign_fishing_boat_to_wharf(Figure *boat, Building wharf, map_point *tile)
{
    building *record = const_cast<building *>(wharf.record());
    if (!boat || !record || record->state != BUILDING_STATE_IN_USE) {
        return 0;
    }
    map_point access_tile;
    if (!wharf_tile(wharf, &access_tile)) {
        return 0;
    }
    clear_stale_fishing_boat_slots(record);
    if (!wharf_has_boat_id(record, boat->id())) {
        const int self_capacity = fishing_boat_capacity(wharf, building_type_registry_impl::SpawnSource::Self);
        const int self_boat = boat->building && boat->building->id == wharf.id;
        if (self_capacity > 0 && self_boat && !record->data.industry.fishing_boat_id) {
            record->data.industry.fishing_boat_id = boat->id();
        } else if (self_capacity > 0 && !self_boat &&
            fishing_boat_capacity(wharf, building_type_registry_impl::SpawnSource::Shipyard) > 0 &&
            !record->data.industry.second_fishing_boat_id) {
            record->data.industry.second_fishing_boat_id = boat->id();
        } else if (self_capacity <= 0 &&
            wharf_live_fishing_boat_count(record) <
                fishing_boat_capacity(wharf, building_type_registry_impl::SpawnSource::None)) {
            if (!record->data.industry.fishing_boat_id) {
                record->data.industry.fishing_boat_id = boat->id();
            } else if (!record->data.industry.second_fishing_boat_id) {
                record->data.industry.second_fishing_boat_id = boat->id();
            } else {
                return 0;
            }
        } else {
            return 0;
        }
    }
    if (tile) {
        map_point_store_result(access_tile.x, access_tile.y, tile);
    }
    return record->id;
}

void map_water_clear_fishing_boat_from_wharf(Building wharf, unsigned int boat_id)
{
    building *record = const_cast<building *>(wharf.record());
    if (!record || !boat_id) {
        return;
    }
    if (record->data.industry.fishing_boat_id == boat_id) {
        record->data.industry.fishing_boat_id = 0;
    }
    if (record->data.industry.second_fishing_boat_id == boat_id) {
        record->data.industry.second_fishing_boat_id = 0;
    }
}

static Building *find_wharf_for_new_fishing_boat(Figure *boat, map_point *tile, int assign)
{
    Building *wharf = nullptr;
    const building_type wharf_type = building_type_registry_impl::type_from_attr("wharf");
    if (wharf_type == BUILDING_NONE) {
        return nullptr;
    }
    for (int pass = 0; pass < 2 && !wharf; pass++) {
        for (Building &candidate : Building::of_type(wharf_type)) {
            if (wharf ||
                candidate.state_id() != BUILDING_STATE_IN_USE) {
                continue;
            }
            building *record = const_cast<building *>(candidate.record());
            if (!record) {
                continue;
            }
            map_point access_tile;
            if (!wharf_tile(candidate, &access_tile)) {
                continue;
            }
            clear_stale_fishing_boat_slots(record);
            const int live_boats = wharf_live_fishing_boat_count(record);
            if (wharf_has_boat_id(record, boat ? boat->id() : 0) ||
                (wharf_has_shipyard_fishing_boat_room(candidate, live_boats) && (pass == 1 || live_boats == 0))) {
                wharf = &candidate;
            }
        }
    }
    if (!wharf) {
        return nullptr;
    }
    if (assign) {
        return map_water_assign_fishing_boat_to_wharf(boat, *wharf, tile) ? wharf : nullptr;
    }
    return wharf_tile(*wharf, tile) ? wharf : nullptr;
}

Building *map_water_assign_wharf_for_new_fishing_boat(Figure *boat, map_point *tile)
{
    return find_wharf_for_new_fishing_boat(boat, tile, 1);
}

int map_water_has_wharf_for_new_fishing_boat(void)
{
    map_point tile;
    return find_wharf_for_new_fishing_boat(nullptr, &tile, 0) != nullptr;
}

int map_water_shipyard_can_spawn_fishing_boat(Building shipyard)
{
    building *record = const_cast<building *>(shipyard.record());
    map_point tile;
    return record && !live_fishing_boat_id(record->figure_id) &&
        map_water_has_wharf_for_new_fishing_boat() &&
        map_water_can_spawn_fishing_boat(shipyard, &tile);
}

int map_water_spawn_fishing_boat_from_shipyard(Building shipyard)
{
    building *record = const_cast<building *>(shipyard.record());
    map_point boat_tile;
    if (!record || live_fishing_boat_id(record->figure_id) ||
        !map_water_has_wharf_for_new_fishing_boat() ||
        !map_water_can_spawn_fishing_boat(shipyard, &boat_tile)) {
        return 0;
    }
    Figure *boat =
        figure_runtime_create_profiled(FIGURE_FISHING_BOAT, boat_tile.x, boat_tile.y, DIR_0_TOP, shipyard, "fish_fetch");
    if (!boat) {
        return 0;
    }
    record->figure_id = boat->id();
    return 1;
}

int map_water_spawn_fishing_boat_from_wharf(Building wharf)
{
    building *record = const_cast<building *>(wharf.record());
    map_point boat_tile;
    map_point wharf_point;
    if (!record || !map_water_wharf_has_self_fishing_boat_room(wharf) ||
        !map_water_can_spawn_fishing_boat(wharf, &boat_tile)) {
        return 0;
    }

    Figure *boat =
        figure_runtime_create_profiled(FIGURE_FISHING_BOAT, boat_tile.x, boat_tile.y, DIR_0_TOP, wharf, "fish_fetch");
    if (!boat || !map_water_assign_fishing_boat_to_wharf(boat, wharf, &wharf_point)) {
        if (boat) {
            boat->state = FIGURE_STATE_DEAD;
        }
        return 0;
    }
    boat->action_state = FIGURE_ACTION_193_FISHING_BOAT_GOING_TO_WHARF;
    boat->destination_x = static_cast<unsigned char>(wharf_point.x);
    boat->destination_y = static_cast<unsigned char>(wharf_point.y);
    boat->source_x = static_cast<unsigned char>(wharf_point.x);
    boat->source_y = static_cast<unsigned char>(wharf_point.y);
    Route::remove(boat);
    return 1;
}

int map_water_find_alternative_fishing_boat_tile(Figure *boat, map_point *tile)
{
    if (map_figure_at(boat->grid_offset) == static_cast<int>(boat->id())) {
        return 0;
    }
    for (int radius = 1; radius <= 5; radius++) {
        int x_min, y_min, x_max, y_max;
        map_grid_get_area(boat->x, boat->y, 1, radius, &x_min, &y_min, &x_max, &y_max);

        for (int yy = y_min; yy <= y_max; yy++) {
            for (int xx = x_min; xx <= x_max; xx++) {
                int grid_offset = map_grid_offset(xx, yy);
                if (!map_has_figure_at(grid_offset) && map_terrain_is(grid_offset, TERRAIN_WATER)) {
                    map_point_store_result(xx, yy, tile);
                    return 1;
                }
            }
        }
    }
    return 0;
}

int map_water_find_shipwreck_tile(Figure *wreck, map_point *tile)
{
    if (map_terrain_is(wreck->grid_offset, TERRAIN_WATER) &&
        map_figure_at(wreck->grid_offset) == static_cast<int>(wreck->id())) {
        return 0;
    }
    for (int radius = 1; radius <= 5; radius++) {
        int x_min, y_min, x_max, y_max;
        map_grid_get_area(wreck->x, wreck->y, 1, radius, &x_min, &y_min, &x_max, &y_max);

        for (int yy = y_min; yy <= y_max; yy++) {
            for (int xx = x_min; xx <= x_max; xx++) {
                int grid_offset = map_grid_offset(xx, yy);
                if (!map_has_figure_at(grid_offset) || map_figure_at(grid_offset) == static_cast<int>(wreck->id())) {
                    if (map_terrain_is(grid_offset, TERRAIN_WATER) &&
                        map_terrain_is(map_grid_offset(xx, yy - 2), TERRAIN_WATER) &&
                        map_terrain_is(map_grid_offset(xx, yy + 2), TERRAIN_WATER) &&
                        map_terrain_is(map_grid_offset(xx - 2, yy), TERRAIN_WATER) &&
                        map_terrain_is(map_grid_offset(xx + 2, yy), TERRAIN_WATER)) {
                        map_point_store_result(xx, yy, tile);
                        return 1;
                    }
                }
            }
        }
    }
    return 0;
}

int map_water_can_spawn_fishing_boat(const Building &building, map_point *tile)
{
    const building_type_registry_impl::BuildingGeometry geometry =
        building_type_registry_impl::BuildingGeometry::query(building);
    for (const building_type_registry_impl::BuildingGeometryPoint &candidate :
        geometry.water_candidates()) {
        if (!map_grid_is_inside(candidate.x, candidate.y, 1)) {
            continue;
        }
        const int grid_offset = map_grid_offset(candidate.x, candidate.y);
        if (map_terrain_is(grid_offset, TERRAIN_WATER) &&
            !map_terrain_is(grid_offset, TERRAIN_BUILDING) &&
            num_surrounding_water_tiles(grid_offset) >= 8) {
            map_point_store_result(candidate.x, candidate.y, tile);
            return 1;
        }
    }
    return 0;
}
