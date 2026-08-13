#include "building/building_record.h"
#include "desirability.h"

#include "building/building.h"
#include "building/BuildingGeometry.h"
#include "building/building_type_registry_internal.h"
#include "building/HousingProfileDef.h"
#include "building/monument.h"
#include "building/properties.h"
#include "core/calc.h"
#include "map/data.h"
#include "map/grid.h"
#include "map/property.h"
#include "map/terrain.h"

#include <algorithm>

static grid_i8 desirability_grid;

void map_desirability_clear(void)
{
    map_grid_clear_i8(desirability_grid.items);
}

static void add_to_terrain(
    const building_type_registry_impl::BuildingGeometry &geometry,
    int desirability,
    int step,
    int step_size,
    int range)
{
    if (!geometry.valid() || range <= 0) {
        return;
    }
    range = std::min(range, 8);
    const int step_interval = std::max(1, step);
    for (int distance = 1; distance <= range; ++distance) {
        const int value = desirability + ((distance - 1) / step_interval) * step_size;
        for (const building_type_registry_impl::BuildingGeometryPoint &point :
            geometry.points_at_distance(distance)) {
            if (point.x < -1 || point.x > map_data.width || point.y < -1 || point.y > map_data.height) {
                continue;
            }
            const int grid_offset = map_grid_offset(point.x, point.y);
            desirability_grid.items[grid_offset] = static_cast<int8_t>(
                calc_bound(desirability_grid.items[grid_offset] + value, -100, 100));
        }
    }
}

static void add_to_terrain_cell(
    int x,
    int y,
    int desirability,
    int step,
    int step_size,
    int range)
{
    add_to_terrain(
        building_type_registry_impl::BuildingGeometry::from_world_cells({ { x, y } }),
        desirability,
        step,
        step_size,
        range);
}

static void update_buildings(void)
{
    int value;
    int value_bonus;
    int step;
    int step_size;
    int range;
    int venus_module2 = building_monument_gt_module_is_active(VENUS_MODULE_2_DESIRABILITY_ENTERTAINMENT);
    Building *venus_gt = grand_temple_for_god(GOD_VENUS, true);
    Building::for_each([&](Building *building) {
        if (!building || building->state_id() != BUILDING_STATE_IN_USE) {
            return;
        }

        const ::building *b = building->record();
        if (!b) {
            return;
        }

        const model_building *model = model_get_building(b->type);
        value = model->desirability_value;
        step = model->desirability_step;
        step_size = model->desirability_step_size;
        range = model->desirability_range;

        // Venus Module 2 House Desirability Bonus
        if (building->Housing && building->Housing->state().services.temple_venus && venus_module2) {
            const auto *profile = building->Housing->definition().profile;
            int legacy_level = profile ? profile->compatibility_level : -1;
            if (building->Housing->has_patrician_residents()) {
                value += 4;
                range += 1;
            } else if (legacy_level >= HOUSE_MIN && legacy_level <= HOUSE_LARGE_TENT) {
                // tents normally confer -3, -2, -1, 0, 0, 0 (range=3)
                // now this becomes -1, 0, 0, 0, 0, 0 (range=1)
                value += 2;
                range = 1;
            } else {
                if (range <= 1) {
                    range = 1;
                }
                value += 2;
            }
        }

        if (building_monument_is_monument(b) && b->monument.phase != MONUMENT_FINISHED) {
            value = 0;
            step = 0;
            step_size = 0;
            range = 0;
        }

        // Venus GT Base Bonus
        if (building->type->flags().venus_gt_bonus() && venus_gt) {
            value_bonus = ((value / 4) > 1) ? (value / 4) : 1;
            value += value_bonus;
            step += 1;
            range += 1;
        }

        const building_type_registry_impl::BuildingGeometry geometry =
            building_type_registry_impl::BuildingGeometry::query(*building);
        add_to_terrain(geometry, value, step, step_size, range);
    });
}

static void add_garden_desirability(
    int x,
    int y,
    const model_building *model,
    int venus_bonus)
{
    if (!model) {
        return;
    }

    int value = model->desirability_value;
    int step = model->desirability_step;
    int step_size = model->desirability_step_size;
    int range = model->desirability_range;

    if (venus_bonus) {
        int value_bonus = ((value / 4) > 1) ? (value / 4) : 1;
        value += value_bonus;
        step += 1;
        range += 1;
    }

    const building_type_registry_impl::BuildingGeometry geometry =
        building_type_registry_impl::BuildingGeometry::from_world_cells({ { x, y } });
    add_to_terrain(geometry, value, step, step_size, range);
}

static void update_terrain(void)
{
    const building_type garden_type = building_type_registry_impl::type_from_attr("gardens");
    const building_type highway_type = building_type_registry_impl::type_from_attr("highway");
    const building_type plaza_type = building_type_registry_impl::type_from_attr("plaza");
    const building_type earthquake_type = building_type_registry_impl::vacant_lot_fill_type();
    const model_building *garden_model =
        garden_type != BUILDING_NONE ? model_get_building(garden_type) : nullptr;
    const model_building *highway_model =
        highway_type != BUILDING_NONE ? model_get_building(highway_type) : nullptr;
    const model_building *plaza_model =
        plaza_type != BUILDING_NONE ? model_get_building(plaza_type) : nullptr;
    const model_building *earthquake_model =
        earthquake_type != BUILDING_NONE ? model_get_building(earthquake_type) : nullptr;
    const int venus_garden_bonus = grand_temple_for_god(GOD_VENUS, true) != nullptr;

    int grid_offset = map_data.start_offset;
    for (int y = 0; y < map_data.height; y++, grid_offset += map_data.border_size) {
        for (int x = 0; x < map_data.width; x++, grid_offset++) {
            int terrain = map_terrain_get(grid_offset);
            if (map_property_is_plaza_earthquake_or_overgrown_garden(grid_offset)) {
                const model_building *model = nullptr;
                if (terrain & TERRAIN_ROAD) {
                    model = plaza_model;
                } else if (terrain & TERRAIN_ROCK) {
                    // earthquake fault line: slight negative
                    model = earthquake_model;
                } else if (terrain & TERRAIN_GARDEN) {
                    add_garden_desirability(x, y, garden_model, venus_garden_bonus);
                    continue;
                } else {
                    // invalid plaza/earthquake flag
                    map_property_clear_plaza_earthquake_or_overgrown_garden(grid_offset);
                    continue;
                }
                if (!model) {
                    continue;
                }
                add_to_terrain_cell(x, y,
                    model->desirability_value,
                    model->desirability_step,
                    model->desirability_step_size,
                    model->desirability_range);
            } else if (terrain & TERRAIN_GARDEN) {
                add_garden_desirability(x, y, garden_model, venus_garden_bonus);
            } else if (terrain & TERRAIN_RUBBLE) {
                add_to_terrain_cell(x, y, -2, 1, 1, 2);
            } else if (terrain & TERRAIN_HIGHWAY) {
                if (highway_model) {
                    add_to_terrain_cell(x, y,
                        highway_model->desirability_value,
                        highway_model->desirability_step,
                        highway_model->desirability_step_size,
                        highway_model->desirability_range);
                }
            } else if (terrain & TERRAIN_AQUEDUCT) {
                add_to_terrain_cell(x, y, -2, 1, 1, 2);
            }
        }
    }
}

void map_desirability_update(void)
{
    map_desirability_clear();
    update_buildings();
    update_terrain();
}

int map_desirability_get(int grid_offset)
{
    return desirability_grid.items[grid_offset];
}

void map_desirability_save_state(buffer *buf)
{
    map_grid_save_state_i8(desirability_grid.items, buf);
}

void map_desirability_load_state(buffer *buf)
{
    map_grid_load_state_i8(desirability_grid.items, buf);
}
