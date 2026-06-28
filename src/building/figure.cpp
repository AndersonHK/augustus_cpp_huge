#include "building/figure.h"
#include "building/entertainment.h"
#include "building/image.h"
#include "building/industry.h"
#include "map/building_tiles.h"
#include "map/image.h"
#include "map/road_access.h"

#include "building/building_record.h"
#include "building/building.h"
#include "building/building_runtime.h"
#include "building/building_runtime_internal.h"
#include "building/barracks.h"
#include "building/mess_hall.h"
#include "building/temple.h"
#include "figure/formation_legion.h"

#include "building/building_type_registry_internal.h"
#include "building/religion.h"
#include "figure/figure.h"
#include "figure/figure_runtime_api.h"

#include <algorithm>

#include "scenario/scenario.h"

#include "building/granary.h"
#include "building/monument.h"
#include "building/properties.h"
#include "building/warehouse.h"
#include "city/buildings.h"
#include "city/data_private.h"
#include "city/message.h"
#include "city/population.h"
#include "core/calc.h"
#include "core/image.h"
#include "figure/movement.h"
#include "game/resource.h"
#include "game/time.h"
#include "map/desirability.h"
#include "map/random.h"
#include "map/terrain.h"

Building BuildingFigureGenerator::working_pantheon()
{
    for (building_type type = BUILDING_NONE; type < BUILDING_TYPE_MAX; type = static_cast<building_type>(type + 1)) {
        const building_type_registry_impl::BuildingType *definition =
            building_type_registry_impl::definition_for_type(type);
        if (definition && definition->is_pantheon()) {
            return Building(building_get(building_monument_working(type)));
        }
    }
    return Building(nullptr);
}

int BuildingFigureGenerator::type_is_basic_temple(building_type type)
{
    const building_type_registry_impl::BuildingType *definition =
        building_type_registry_impl::definition_for_type(type);
    return definition &&
        (definition->is_temple(GOD_ALL, building_type_registry_impl::ReligionTier::Small) ||
            definition->is_temple(GOD_ALL, building_type_registry_impl::ReligionTier::Large));
}

int BuildingFigureGenerator::type_uses_native_spawn(building_type type)
{
    return building_type_registry_impl::type_attr_is_any(type, {
        "engineers_post",
        "prefecture",
        "actor_colony",
        "gladiator_school",
        "bathhouse",
        "library",
        "academy",
        "barber",
        "doctor",
        "hospital",
        "workcamp",
        "school",
        "lion_house",
        "chariot_maker",
        "amphitheater",
        "arena",
        "architect_guild",
        "grand_temple_ceres",
        "grand_temple_neptune",
        "grand_temple_mercury",
        "grand_temple_mars",
        "grand_temple_venus",
        "pantheon",
        "lighthouse",
        "watchtower",
        "caravanserai",
        "armoury"
    });
}

int BuildingFigureGenerator::figure_belongs_to_building(const Figure *f, const building *b)
{
    return f && b && f->building.id == b->id;
}

void BuildingFigureGenerator::attach_figure_to_building(Figure *f, building *b)
{
    if (f) {
        f->building = Building(b);
    }
}

int BuildingFigureGenerator::worker_percentage(const Building &building)
{
    return calc_percentage(building.employment_worker_count(), building.employment_required_workers());
}

int BuildingFigureGenerator::worker_slot_capacity(const Building &building, int pct75, int pct50, int pct1)
{
    int pct_workers = worker_percentage(building);
    if (pct_workers >= 75) {
        return pct75;
    }
    if (pct_workers >= 50) {
        return pct50;
    }
    if (pct_workers > 0) {
        return pct1;
    }
    return 0;
}

int BuildingFigureGenerator::has_figure_of_types(building *b, figure_type type1, figure_type type2)
{
    if (b->figure_id <= 0) {
        return 0;
    }
    Figure *f = Figure::get(b->figure_id);
    if (f && !f->is_dead() && figure_belongs_to_building(f, b) && (f->type == type1 || f->type == type2)) {
        return 1;
    } else {
        b->figure_id = 0;
        return 0;
    }
}

int BuildingFigureGenerator::has_figure_of_type(building *b, figure_type type)
{
    return has_figure_of_types(b, type, static_cast<figure_type>(0));
}

int BuildingFigureGenerator::worker_spawn_delay_days(
    const Building &building,
    int pct100,
    int pct75,
    int pct50,
    int pct25,
    int pct1)
{
    int pct_workers = worker_percentage(building);
    if (pct_workers >= 100) {
        return pct100;
    } else if (pct_workers >= 75) {
        return pct75;
    } else if (pct_workers >= 50) {
        return pct50;
    } else if (pct_workers >= 25) {
        return pct25;
    } else if (pct_workers >= 1) {
        return pct1;
    } else {
        return -1;
    }
}

int BuildingFigureGenerator::worker_scaled_spawn_delay(
    const Building &building,
    int pct100,
    int pct75,
    int pct50,
    int pct25,
    int pct1)
{
    const int delay_days = worker_spawn_delay_days(building, pct100, pct75, pct50, pct25, pct1);
    return delay_days < 0 ? 0 : game_time_scale_legacy_day_ticks(delay_days);
}

int BuildingFigureGenerator::default_spawn_delay(const Building &building)
{
    return worker_scaled_spawn_delay(building, 3, 7, 15, 29, 44);
}

void BuildingFigureGenerator::create_roaming_figure(building *b, int x, int y, figure_type type)
{
    Figure *f = Figure::create(type, x, y, DIR_0_TOP);
    f->action_state = FIGURE_ACTION_125_ROAMING;
    attach_figure_to_building(f, b);
    b->figure_id = f->id();
    figure_movement_init_roaming(f);
}

void BuildingFigureGenerator::spawn_figure_warehouse(building *b)
{
    building_runtime(Building(b)).check_labor_problem();
    building *space = b;
    for (int i = 0; i < 8; i++) {
        space = building_next(space);
        if (space->id) {
            space->show_on_problem_overlay = b->show_on_problem_overlay;
        }
    }
    map_point road;
    if (map_has_road_access_warehouse(b->x, b->y, &road)) {
        building_runtime(Building(b)).run_labor_phase_if_defined(road);
        if (has_figure_of_type(b, FIGURE_WAREHOUSEMAN)) {
            return;
        }
        int resource;
        Building warehouse(b);
        int task = building_warehouse_determine_worker_task(warehouse, &resource);
        if (task != WAREHOUSE_TASK_NONE) {
            Figure *f = Figure::create(FIGURE_WAREHOUSEMAN, road.x, road.y, DIR_4_BOTTOM);
            f->action_state = FIGURE_ACTION_50_WAREHOUSEMAN_CREATED;
            f->loads_sold_or_carrying = 0; // spawn with 0, load is decided by the action.
            //this way we transfer resources from buildings to figures directly, without having to keep track
            if (task == WAREHOUSE_TASK_GETTING) {
                f->resource_id = RESOURCE_NONE;
                f->collecting_item_id = resource;
            } else {
                f->resource_id = resource;
            }
            b->figure_id = f->id();
            attach_figure_to_building(f, b);
        }
    }
}

void BuildingFigureGenerator::spawn_figure_granary(building *b)
{
    building_runtime(Building(b)).check_labor_problem();
    map_update_granary_internal_roads(b); //internal roads updated basing on surrounding terrain
    map_point road;
    if (map_has_road_access_granary(b->x, b->y, &road)) {
        building_runtime(Building(b)).run_labor_phase_if_defined(road);
        if (has_figure_of_type(b, FIGURE_WAREHOUSEMAN)) {
            return;
        }
        int task = building_granary_determine_worker_task(Building(b));
        if (task != GRANARY_TASK_NONE) {
            Figure *f = Figure::create(FIGURE_WAREHOUSEMAN, b->x + 1, b->y + 1, DIR_4_BOTTOM);
            //spawn in the center of the granary
            f->action_state = FIGURE_ACTION_50_WAREHOUSEMAN_CREATED;
            //f->loads_sold_or_carrying = 1; //set loads at action, not spawn
            f->resource_id = task;
            b->figure_id = f->id();
            attach_figure_to_building(f, b);
        }
    }
}

void BuildingFigureGenerator::spawn_figure_tower(building *b)
{
    building_runtime(Building(b)).check_labor_problem();
    map_point road;
    if (map_has_road_access(b->x, b->y, b->size, &road)) {
        building_runtime(Building(b)).run_labor_phase_if_defined(road);
        if (b->num_workers <= 0) {
            return;
        }
        if (!b->figure_id4 && b->figure_id) { // has sentry but no ballista -> create
            Figure *f = Figure::create(FIGURE_BALLISTA, b->x, b->y, DIR_0_TOP);
            b->figure_id4 = f->id();
            attach_figure_to_building(f, b);
            f->action_state = FIGURE_ACTION_180_BALLISTA_CREATED;
        }
    }
}

void BuildingFigureGenerator::spawn_figure_market(building *b)
{
    Building(b).refresh_graphic();
    building_runtime(Building(b)).check_labor_problem();
    map_point road;
    if (map_has_road_access(b->x, b->y, b->size, &road)) {
        building_runtime(Building(b)).run_labor_phase_if_defined(road);
        int spawn_delay = worker_scaled_spawn_delay(Building(b), 2, 5, 10, 20, 30);
        if (!spawn_delay) {
            return;
        }
        // market trader
        if (!has_figure_of_type(b, FIGURE_MARKET_TRADER)) {
            b->figure_spawn_delay++;
            if (b->figure_spawn_delay <= spawn_delay) {
                return;
            }
            b->figure_spawn_delay = 0;
            create_roaming_figure(b, road.x, road.y, FIGURE_MARKET_TRADER);
        }
        // market supplier or labor seeker
        building_runtime(Building(b)).spawn_market_supplier(road);
    }
}

void BuildingFigureGenerator::spawn_figure_grand_temple_mars(building *b)
{
    building_runtime(Building(b)).check_labor_problem();
    map_point road;
    if (map_has_road_access(b->x, b->y, b->size, &road)) {
        building_runtime(Building(b)).run_labor_phase_if_defined(road);
        int spawn_delay = worker_spawn_delay_days(Building(b), 8, 12, 16, 32, 48);
        if (spawn_delay < 0) {
            return;
        }

        // recruitment penalty for no food
        if (city_data.mess_hall.food_stress_cumulative > 20) {
            spawn_delay += city_data.mess_hall.food_stress_cumulative - 20;
        }
        spawn_delay = game_time_scale_legacy_day_ticks(spawn_delay);

        b->figure_spawn_delay++;
        if (b->figure_spawn_delay > spawn_delay) {
            b->figure_spawn_delay = 0;
            map_has_road_access(b->x, b->y, b->size, &road);
            Barracks barracks(b);
            switch (barracks.priority()) {
                case PRIORITY_FORT:
                case PRIORITY_FORT_JAVELIN:
                case PRIORITY_FORT_MOUNTED:
                case PRIORITY_FORT_AUXILIA_INFANTRY:
                case PRIORITY_FORT_AUXILIA_ARCHERY:
                    if (!barracks.create_soldier(road.x, road.y)) {
                        barracks.create_tower_sentry(road.x, road.y);
                    }
                    break;
                default:
                    if (!barracks.create_tower_sentry(road.x, road.y)) {
                        barracks.create_soldier(road.x, road.y);
                    }
            }
            if (!has_figure_of_type(b, FIGURE_PRIEST)) {
                create_roaming_figure(b, road.x, road.y, FIGURE_PRIEST);
            }
        }

        // Pantheon Module 1 Bonus
        if (!b->figure_id4 && building_monument_pantheon_module_is_active(PANTHEON_MODULE_1_DESTINATION_PRIESTS)) {
            Figure *f = Figure::create(FIGURE_PRIEST, road.x, road.y, DIR_4_BOTTOM);
            Building pantheon = working_pantheon();
            b->figure_id4 = f->id();
            f->destination_building = pantheon;
            attach_figure_to_building(f, b);
            f->action_state = FIGURE_ACTION_212_DESTINATION_PRIEST_CREATED;
        }
    }
}

void BuildingFigureGenerator::spawn_figure_tavern(building *b)
{
    building_runtime(Building(b)).check_labor_problem();
    map_point road;
    if (map_has_road_access(b->x, b->y, b->size, &road)) {
        building_runtime(Building(b)).run_labor_phase_if_defined(road);
        int spawn_delay = default_spawn_delay(Building(b)) * 2;
        if (!spawn_delay) {
            return;
        }
        b->figure_spawn_delay++;
        if (b->figure_spawn_delay > spawn_delay) {
            b->figure_spawn_delay = 0;
            Building(b).refresh_graphic();
            if (!has_figure_of_type(b, FIGURE_BARKEEP) && b->resources[resource_wine()] >= 20) {
                b->resources[resource_wine()] -= 20;
                int resource_decay = b->resources[resource_meat()] && b->resources[resource_fish()] ? 20 : 40;
                b->resources[resource_meat()] -= calc_bound(resource_decay, resource_decay, b->resources[resource_meat()]);
                b->resources[resource_fish()] -= calc_bound(resource_decay, resource_decay, b->resources[resource_fish()]);
                create_roaming_figure(b, road.x, road.y, FIGURE_BARKEEP);
            }
        }
        building_runtime(Building(b)).spawn_tavern_supplier(road);
    }
}

void BuildingFigureGenerator::spawn_figure_temple(building *b)
{
    building_runtime(Building(b)).check_labor_problem();
    map_point road;
    if (map_has_road_access(b->x, b->y, b->size, &road)) {
        Building temple(b);
        building_runtime runtime(temple);
        runtime.run_labor_phase_if_defined(road);
        Building building_object(b);
        int spawn_delay;
        if (building_object.employment_required_workers() <= 0) {
            spawn_delay = game_time_scale_legacy_day_ticks(7);
        } else {
            spawn_delay = worker_scaled_spawn_delay(building_object, 3, 7, 10, 15, 20);
            if (!spawn_delay) {
                return;
            }
        }
        b->figure_spawn_delay++;
        if (b->figure_spawn_delay > spawn_delay) {
            if (!has_figure_of_type(b, FIGURE_PRIEST)) {
                create_roaming_figure(b, road.x, road.y, FIGURE_PRIEST);
            }
            runtime.spawn_temple_neptune_chariot(road);
            b->figure_spawn_delay = 0;
        }

        runtime.spawn_temple_mars_mess_hall_priest(road);

        // Ceres Module 2 Bonus
        if (building_is_ceres_temple(b->type) && building_monument_gt_module_is_active(CERES_MODULE_2_DISTRIBUTE_FOOD) && !b->figure_id2) {
            building_runtime(Building(b)).spawn_temple_distribution_supplier(road);
        }

        // Venus Module 1 Bonus
        if (building_is_venus_temple(b->type) && building_monument_gt_module_is_active(VENUS_MODULE_1_DISTRIBUTE_WINE) && !b->figure_id2) {
            building_runtime(Building(b)).spawn_temple_distribution_supplier(road);
        }

        runtime.spawn_temple_destination_priest(road);
    }
}

void BuildingFigureGenerator::spawn_figure_senate_forum(building *b)
{
    if (b && building_type_registry_impl::type_attr_is(b->type, "senate")) {
        Building(b).refresh_graphic();
    }
    if (b && building_type_registry_impl::type_attr_is(b->type, "forum")) {
        Building(b).refresh_graphic();
    }
    building_runtime(Building(b)).check_labor_problem();
    if (has_figure_of_type(b, FIGURE_TAX_COLLECTOR)) {
        return;
    }
    map_point road;
    if (map_has_road_access(b->x, b->y, b->size, &road)) {
        building_runtime(Building(b)).run_labor_phase_if_defined(road);
        int spawn_delay_days = worker_spawn_delay_days(Building(b), 0, 1, 3, 7, 15);
        if (spawn_delay_days < 0) {
            return;
        }
        int spawn_delay = game_time_scale_legacy_day_ticks(spawn_delay_days);
        b->figure_spawn_delay++;
        if (b->figure_spawn_delay > spawn_delay) {
            b->figure_spawn_delay = 0;
            Figure *f = Figure::create(FIGURE_TAX_COLLECTOR, road.x, road.y, DIR_0_TOP);
            f->action_state = FIGURE_ACTION_40_TAX_COLLECTOR_CREATED;
            attach_figure_to_building(f, b);
            b->figure_id = f->id();
        }
    }
}

void BuildingFigureGenerator::spawn_figure_mission_post(building *b)
{
    if (has_figure_of_type(b, FIGURE_MISSIONARY)) {
        return;
    }
    map_point road;
    if (map_has_road_access(b->x, b->y, b->size, &road)) {
        if (city_population() > 0) {
            b->figure_spawn_delay++;
            if (b->figure_spawn_delay > game_time_scale_legacy_day_ticks(1)) {
                b->figure_spawn_delay = 0;
                create_roaming_figure(b, road.x, road.y, FIGURE_MISSIONARY);
            }
        }
    }
}

void BuildingFigureGenerator::spawn_figure_industry(building *b)
{
    building_runtime(Building(b)).check_labor_problem();
    map_point road;
    Building building_obj(b);
    if (building_obj.has_road_access(&road)) {
        building_runtime(Building(b)).run_labor_phase_if_defined(road);
        if (has_figure_of_type(b, FIGURE_CART_PUSHER)) {
            return;
        }
        resource_type output_resource = RESOURCE_NONE;
        int output_cart_loads = 0;
        if (building_obj.reserve_output_storage_loads(&output_resource, &output_cart_loads)) {
            b->output_resource_id = output_resource;
            Figure *f = Figure::create(FIGURE_CART_PUSHER, road.x, road.y, DIR_4_BOTTOM);
            f->action_state = FIGURE_ACTION_20_CARTPUSHER_INITIAL;
            f->resource_id = output_resource;
            attach_figure_to_building(f, b);
            b->figure_id = f->id();
            f->wait_ticks = game_time_scale_legacy_day_ticks(30);
            f->loads_sold_or_carrying = static_cast<unsigned char>(output_cart_loads > 0 ? output_cart_loads : 1);
            return;
        }
    }
}

void BuildingFigureGenerator::spawn_figure_dock(building *b)
{
    building_runtime(Building(b)).check_labor_problem();
    map_point road;
    if (map_has_road_access(b->x, b->y, b->size, &road)) {
        building_runtime(Building(b)).run_labor_phase_if_defined(road);
        int max_dockers = worker_slot_capacity(Building(b), 3, 2, 1);
        // count existing dockers
        int existing_dockers = 0;
        for (int i = 0; i < 3; i++) {
            if (b->data.distribution.cartpusher_ids[i]) {
                Figure *docker = Figure::get(b->data.distribution.cartpusher_ids[i]);
                if (docker && !docker->is_dead() && docker->type == FIGURE_DOCKER &&
                    figure_belongs_to_building(docker, b)) {
                    existing_dockers++;
                } else {
                    b->data.distribution.cartpusher_ids[i] = 0;
                }
            }
        }
        if (existing_dockers > max_dockers) {
            // too many dockers, kill one of them
            for (int i = 2; i >= 0; i--) {
                if (b->data.distribution.cartpusher_ids[i]) {
                    Figure::get(b->data.distribution.cartpusher_ids[i])->state = FIGURE_STATE_DEAD;
                    b->data.distribution.cartpusher_ids[i] = 0;
                    break;
                }
            }
        } else if (existing_dockers < max_dockers) {
            Figure *f = Figure::create(FIGURE_DOCKER, road.x, road.y, DIR_4_BOTTOM);
            f->action_state = FIGURE_ACTION_132_DOCKER_IDLING;
            attach_figure_to_building(f, b);
            for (int i = 0; i < 3; i++) {
                if (!b->data.distribution.cartpusher_ids[i]) {
                    b->data.distribution.cartpusher_ids[i] = f->id();
                    break;
                }
            }
        }
    }
}

void BuildingFigureGenerator::spawn_figure_native_hut(building *b)
{
    if (b && building_type_registry_impl::type_attr_is(b->type, "native_hut")) {
        map_image_set(b->grid_offset, image_group(GROUP_BUILDING_NATIVE) + (map_random_get(b->grid_offset) & 1));
    } else {
        map_image_set(b->grid_offset, building_image_get(b));
    }

    if (has_figure_of_type(b, FIGURE_INDIGENOUS_NATIVE)) {
        return;
    }
    int x_out, y_out;
    if (b->subtype.native_meeting_center_id > 0
        && map_terrain_get_adjacent_road_or_clear_land(b->x, b->y, b->size, &x_out, &y_out)) {
        b->figure_spawn_delay++;
        if (b->figure_spawn_delay > game_time_scale_legacy_day_ticks(4)) {
            b->figure_spawn_delay = 0;
            Figure *f = Figure::create(FIGURE_INDIGENOUS_NATIVE, x_out, y_out, DIR_0_TOP);
            f->action_state = FIGURE_ACTION_158_NATIVE_CREATED;
            attach_figure_to_building(f, b);
            b->figure_id = f->id();
        }
    }
}

void BuildingFigureGenerator::spawn_figure_native_meeting(building *b)
{
    if (building_runtime *runtime = building_runtime_impl::get_or_create_instance(b)) {
        map_building_tiles_add(runtime->building, b->x, b->y, 2, building_image_get(b), TERRAIN_BUILDING);
    }
    int pacified = b->sentiment.native_anger < 100;
    if (pacified && !has_figure_of_type(b, FIGURE_NATIVE_TRADER)) {
        int x_out, y_out;
        if (map_terrain_get_adjacent_road_or_clear_land(b->x, b->y, b->size, &x_out, &y_out)) {
            b->figure_spawn_delay++;
            if (b->figure_spawn_delay > game_time_scale_legacy_day_ticks(8)) {
                b->figure_spawn_delay = 0;
                Figure *f = Figure::create(FIGURE_NATIVE_TRADER, x_out, y_out, DIR_0_TOP);
                f->action_state = FIGURE_ACTION_162_NATIVE_TRADER_CREATED;
                attach_figure_to_building(f, b);
                b->figure_id = f->id();
            }
        }
    }
}

void BuildingFigureGenerator::spawn_figure_barracks(building *b)
{
    building_runtime(Building(b)).check_labor_problem();
    map_point road;
    if (map_has_road_access(b->x, b->y, b->size, &road)) {
        building_runtime(Building(b)).run_labor_phase_if_defined(road);
        int spawn_delay = worker_spawn_delay_days(Building(b), 8, 12, 16, 32, 48);
        if (spawn_delay < 0) {
            return;
        }

        // recruitment penalty for no food
        if (city_data.mess_hall.food_stress_cumulative > 20) {
            spawn_delay += city_data.mess_hall.food_stress_cumulative - 20;
        }
        spawn_delay = game_time_scale_legacy_day_ticks(spawn_delay);

        b->figure_spawn_delay++;
        if (b->figure_spawn_delay > spawn_delay) {
            b->figure_spawn_delay = 0;
            map_has_road_access(b->x, b->y, b->size, &road);
            Barracks barracks(b);
            switch (barracks.priority()) {
                case PRIORITY_FORT:
                case PRIORITY_FORT_JAVELIN:
                case PRIORITY_FORT_MOUNTED:
                case PRIORITY_FORT_AUXILIA_INFANTRY:
                case PRIORITY_FORT_AUXILIA_ARCHERY:
                    if (!barracks.create_soldier(road.x, road.y)) {
                        barracks.create_tower_sentry(road.x, road.y);
                    }
                    break;
                default:
                    if (!barracks.create_tower_sentry(road.x, road.y)) {
                        barracks.create_soldier(road.x, road.y);
                    }
            }
        }
    }
}

void BuildingFigureGenerator::spawn_figure_military_academy(building *b)
{
    building_runtime(Building(b)).check_labor_problem();
    map_point road;
    if (map_has_road_access(b->x, b->y, b->size, &road)) {
        building_runtime(Building(b)).run_labor_phase_if_defined(road);
    }
}

void BuildingFigureGenerator::spawn_figure_fort_supplier(building *fort)
{
    const int mess_hall_id = city_buildings_get_mess_hall();
    building *mess_hall = mess_hall_id ? building_get(mess_hall_id) : nullptr;
    MessHall(Building(mess_hall)).spawn_fort_supplier_to(Building(fort));
}

void BuildingFigureGenerator::spawn_figure_mess_hall(building *b)
{
    building_runtime(Building(b)).check_labor_problem();
    map_point road;
    if (map_has_road_access(b->x, b->y, b->size, &road)) {
        building_runtime(Building(b)).run_labor_phase_if_defined(road);
        int spawn_delay = worker_scaled_spawn_delay(Building(b), 7, 15, 29, 44, 59);
        if (!spawn_delay) {
            return;
        }

        building_runtime(Building(b)).spawn_primary_mess_hall_supplier(road);
        if (b->figure_id) {
            b->figure_spawn_delay++;
        } else {
            b->figure_spawn_delay = 0;
        }
        if (building_monument_working_grand_temple_for_god(GOD_MARS) && b->figure_spawn_delay >= spawn_delay) {
            building_runtime(Building(b)).spawn_secondary_mess_hall_supplier(road);
            b->figure_spawn_delay = 0;
        }
    }
}

void BuildingFigureGenerator::spawn_figure_depot(building *b)
{
    building_runtime(Building(b)).check_labor_problem();

    map_point road;
    if (map_has_road_access(b->x, b->y, b->size, &road)) {
        building_runtime(Building(b)).run_labor_phase_if_defined(road);

        int max_carts = worker_slot_capacity(Building(b), 3, 2, 1);

        int existing_carts = 0;
        for (int i = 0; i < 3; i++) {
            if (b->data.distribution.cartpusher_ids[i]) {
                Figure *f = Figure::get(b->data.distribution.cartpusher_ids[i]);
                if (f && !f->is_dead() && f->type == FIGURE_DEPOT_CART_PUSHER &&
                    figure_belongs_to_building(f, b)) {
                    existing_carts++;
                } else {
                    b->data.distribution.cartpusher_ids[i] = 0;
                }
            }
        }

        if (existing_carts >= max_carts) {
            return;
        }

        int spawn_delay = default_spawn_delay(Building(b));
        b->figure_spawn_delay++;
        if (b->figure_spawn_delay > spawn_delay) {
            b->figure_spawn_delay = 0;
            Building owner(b);
            Figure *f = figure_runtime_create_profiled(
                FIGURE_DEPOT_CART_PUSHER,
                road.x,
                road.y,
                DIR_0_TOP,
                owner,
                "order_runner");
            if (!f) {
                return;
            }

            for (int i = 0; i < 3; i++) {
                if (!b->data.distribution.cartpusher_ids[i]) {
                    b->data.distribution.cartpusher_ids[i] = f->id();
                    break;
                }
            }
        }
    }
}

void BuildingFigureGenerator::advance_native_crop_graphics(building *record)
{
    if (!record) {
        return;
    }
    Building crops(record);
    const int option_count = crops.type ? crops.type->graphics().production_progress_option_count() : 0;
    if (option_count <= 0) {
        return;
    }

    record->data.industry.progress++;
    if (record->data.industry.progress >= option_count) {
        record->data.industry.progress = 0;
    }
    crops.refresh_graphic_if_native();
}

int BuildingFigureGenerator::building_uses_runtime_spawn(const building *b)
{
    if (!b) {
        return 0;
    }
    const building_type_registry_impl::BuildingType *definition =
        building_type_registry_impl::definition_for_type(b->type);
    if (definition && definition->has_native_production()) {
        return 1;
    }
    if (definition && !definition->spawn_groups().empty()) {
        return 1;
    }
    if (b && building_type_registry_impl::type_attr_is_any(b->type, {"senate", "forum"})) {
        return 1;
    }
    if (type_is_basic_temple(b->type)) {
        return b->monument.phase <= 0;
    }
    if (definition && definition->is_theater()) {
        return 1;
    }

    return type_uses_native_spawn(b->type);
}

void BuildingFigureGenerator::generate()
{
    for (int i = 1; i < building_count(); i++) {
        building *b = building_get(static_cast<unsigned int>(i));
        if (!b || !b->id) {
            continue;
        }

        Building building_object(b);
        if (b->state != BUILDING_STATE_IN_USE) {
            b->show_on_problem_overlay = 1;
            continue;
        }
        if (!building_object.is_main_part() || building_monument_is_unfinished_monument(b)) {
            continue;
        }

        b->show_on_problem_overlay = 0;
        if (building_uses_runtime_spawn(b)) {
            building_object.spawn_figure();
        } else if (building_is_raw_resource_producer(b->type) ||
            building_is_farm(b->type) || building_is_workshop(b->type)) {
            spawn_figure_industry(b);
        } else if (b && building_type_registry_impl::type_attr_is_any(b->type, {"senate_1_unused", "forum_2_unused"})) {
            spawn_figure_senate_forum(b);
        } else if (building_object.type->is_warehouse()) {
            spawn_figure_warehouse(b);
        } else if (building_object.type->is_granary()) {
            spawn_figure_granary(b);
        } else if (b && building_type_registry_impl::type_attr_is(b->type, "tower")) {
            spawn_figure_tower(b);
        } else if (b && building_type_registry_impl::type_attr_is(b->type, "hippodrome")) {
            BuildingEntertainment(Building(b)).spawn_hippodrome_service();
        } else if (b && building_type_registry_impl::type_attr_is(b->type, "colosseum")) {
            BuildingEntertainment(Building(b)).spawn_colosseum_service();
        } else if (b && building_type_registry_impl::type_attr_is(b->type, "market")) {
            spawn_figure_market(b);
        } else if (b && building_type_registry_impl::type_attr_is(b->type, "mission_post")) {
            spawn_figure_mission_post(b);
        } else if (building_object.type->attr_is("dock")) {
            spawn_figure_dock(b);
        } else if (b && building_type_registry_impl::type_attr_is_any(b->type, {"native_hut", "native_hut_alt"})) {
            spawn_figure_native_hut(b);
        } else if (b && building_type_registry_impl::type_attr_is(b->type, "native_meeting")) {
            spawn_figure_native_meeting(b);
        } else if (b && building_type_registry_impl::type_attr_is(b->type, "native_crops")) {
            advance_native_crop_graphics(b);
        } else if (building_is_fort(b->type)) {
            Building fort(*b);
            formation_legion_update_recruit_status(fort);
            spawn_figure_fort_supplier(b);
        } else if (b && building_type_registry_impl::type_attr_is(b->type, "barracks")) {
            spawn_figure_barracks(b);
        } else if (b && building_type_registry_impl::type_attr_is(b->type, "military_academy")) {
            spawn_figure_military_academy(b);
        } else if (building_object.type->is_mess_hall()) {
            spawn_figure_mess_hall(b);
        } else if (b && building_type_registry_impl::type_attr_is(b->type, "tavern")) {
            spawn_figure_tavern(b);
        } else if (b && building_type_registry_impl::type_attr_is(b->type, "cart_depot")) {
            spawn_figure_depot(b);
        }
    }
}

void building_figure_generate(void)
{
    BuildingFigureGenerator generator;
    generator.generate();
}
