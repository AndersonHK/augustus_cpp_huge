#include "city/trade_ledger.h"
#include "building/figure.h"
#include "building/entertainment.h"
#include "building/industry.h"
#include "map/road_access.h"

#include "building/building_record.h"
#include "building/building.h"
#include "building/building_runtime.h"
#include "building/building_runtime_internal.h"
#include "building/barracks.h"
#include "building/mess_hall.h"
#include "building/temple.h"
#include "figure/formation_legion.h"
#include "figuretype/native.h"

#include "building/building_type_registry_internal.h"
#include "building/religion.h"
#include "figure/figure.h"
#include "figure/figure_runtime_api.h"

#include <algorithm>

#include "building/granary.h"
#include "building/monument.h"
#include "city/monument_gifts.h"
#include "building/properties.h"
#include "building/warehouse.h"
#include "core/calc.h"
#include "figure/movement.h"
#include "game/resource.h"
#include "game/time.h"

namespace {

building_runtime *runtime_for_record(building *record)
{
    return building_runtime_impl::get_or_create_instance(record);
}

} // namespace

int BuildingFigureGenerator::type_is_basic_temple(building_type type)
{
    const building_type_registry_impl::BuildingType *definition =
        building_type_registry_impl::definition_for_type(type);
    return definition &&
        (definition->is_temple(std::nullopt, building_type_registry_impl::ReligionTier::Small) ||
            definition->is_temple(std::nullopt, building_type_registry_impl::ReligionTier::Large));
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
    return f && b && f->building && f->building->id == b->id;
}

void BuildingFigureGenerator::attach_figure_to_building(Figure *f, building *b)
{
    if (f) {
        building_runtime *runtime = runtime_for_record(b);
        f->set_home_building(runtime ? &runtime->building : nullptr);
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
    building_runtime *runtime = runtime_for_record(b);
    if (!runtime) {
        return;
    }
    Building &building = runtime->building;
    runtime->check_labor_problem();
    if (!building.Composition) {
        return;
    }
    building.Composition->for_each_member([b](Building &part) {
        ::building *space = const_cast<::building *>(part.record());
        if (space) {
            space->show_on_problem_overlay = b->show_on_problem_overlay;
        }
    });
    map_point road;
    if (map_has_road_access_building(b->x, b->y, &road)) {
        runtime->run_labor_phase_if_defined(road);
        if (has_figure_of_type(b, FIGURE_WAREHOUSEMAN)) {
            return;
        }
        int resource;
        int task = building_warehouse_determine_worker_task(building, &resource);
        if (task != WAREHOUSE_TASK_NONE) {
            Figure *f = Figure::create(FIGURE_WAREHOUSEMAN, road.x, road.y, DIR_4_BOTTOM);
            f->action_state = FIGURE_ACTION_50_WAREHOUSEMAN_CREATED;
            f->loads_sold_or_carrying = 0; // spawn with 0, load is decided by the action.
            //this way we transfer resources from buildings to figures directly, without having to keep track
            if (task == WAREHOUSE_TASK_GETTING) {
                f->resource_id = RESOURCE_NONE;
                f->collecting_item_id = static_cast<unsigned char>(resource);
            } else {
                f->resource_id = static_cast<unsigned char>(resource);
            }
            b->figure_id = f->id();
            attach_figure_to_building(f, b);
        }
    }
}

void BuildingFigureGenerator::spawn_figure_granary(building *b)
{
    building_runtime *runtime = runtime_for_record(b);
    if (!runtime) {
        return;
    }
    Building &building = runtime->building;
    runtime->check_labor_problem();
    map_update_building_internal_roads(b); // Internal passage roads follow their surrounding terrain.
    map_point road;
    if (map_has_road_access_building(b->x, b->y, &road)) {
        runtime->run_labor_phase_if_defined(road);
        if (has_figure_of_type(b, FIGURE_WAREHOUSEMAN)) {
            return;
        }
        int task = building_granary_determine_worker_task(building);
        if (task != GRANARY_TASK_NONE) {
            Figure *f = Figure::create(FIGURE_WAREHOUSEMAN, b->x + 1, b->y + 1, DIR_4_BOTTOM);
            //spawn in the center of the granary
            f->action_state = FIGURE_ACTION_50_WAREHOUSEMAN_CREATED;
            //f->loads_sold_or_carrying = 1; //set loads at action, not spawn
            f->resource_id = static_cast<unsigned char>(task);
            b->figure_id = f->id();
            attach_figure_to_building(f, b);
        }
    }
}

void BuildingFigureGenerator::spawn_figure_market(building *b)
{
    building_runtime *runtime = runtime_for_record(b);
    if (!runtime) {
        return;
    }
    Building &building = runtime->building;
    building.refresh_graphic();
    runtime->check_labor_problem();
    map_point road;
    if (map_has_road_access_building(b->x, b->y, &road)) {
        runtime->run_labor_phase_if_defined(road);
        int spawn_delay = worker_scaled_spawn_delay(building, 2, 5, 10, 20, 30);
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
        runtime->spawn_market_supplier(road);
    }
}

void BuildingFigureGenerator::spawn_figure_tavern(building *b)
{
    building_runtime *runtime = runtime_for_record(b);
    if (!runtime) {
        return;
    }
    Building &building = runtime->building;
    runtime->check_labor_problem();
    map_point road;
    if (map_has_road_access_building(b->x, b->y, &road)) {
        runtime->run_labor_phase_if_defined(road);
        int spawn_delay = default_spawn_delay(building) * 2;
        if (!spawn_delay) {
            return;
        }
        b->figure_spawn_delay++;
        if (b->figure_spawn_delay > spawn_delay) {
            b->figure_spawn_delay = 0;
            building.refresh_graphic();
            if (!has_figure_of_type(b, FIGURE_BARKEEP) && b->resources[resource_wine()] >= 20) {
                city_trade_ledger_consumed(resource_wine(), 20);
                b->resources[resource_wine()] -= 20;
                int resource_decay = b->resources[resource_meat()] && b->resources[resource_fish()] ? 20 : 40;
                b->resources[resource_meat()] = static_cast<short>(
                    b->resources[resource_meat()] - calc_bound(resource_decay, resource_decay, b->resources[resource_meat()]));
                b->resources[resource_fish()] = static_cast<short>(
                    b->resources[resource_fish()] - calc_bound(resource_decay, resource_decay, b->resources[resource_fish()]));
                create_roaming_figure(b, road.x, road.y, FIGURE_BARKEEP);
            }
        }
        runtime->spawn_tavern_supplier(road);
    }
}

void BuildingFigureGenerator::spawn_figure_senate_forum(building *b)
{
    building_runtime *runtime = runtime_for_record(b);
    if (!runtime) {
        return;
    }
    Building &building = runtime->building;
    if (b && building_type_registry_impl::type_attr_is(b->type, "senate")) {
        building.refresh_graphic();
    }
    if (b && building_type_registry_impl::type_attr_is(b->type, "forum")) {
        building.refresh_graphic();
    }
    runtime->check_labor_problem();
    if (has_figure_of_type(b, FIGURE_TAX_COLLECTOR)) {
        return;
    }
    map_point road;
    if (map_has_road_access_building(b->x, b->y, &road)) {
        runtime->run_labor_phase_if_defined(road);
        int spawn_delay_days = worker_spawn_delay_days(building, 0, 1, 3, 7, 15);
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

void BuildingFigureGenerator::spawn_figure_industry(building *b)
{
    building_runtime *runtime = runtime_for_record(b);
    if (!runtime) {
        return;
    }
    Building &building_obj = runtime->building;
    runtime->check_labor_problem();
    map_point road;
    if (building_obj.has_road_access(&road)) {
        runtime->run_labor_phase_if_defined(road);
        if (has_figure_of_type(b, FIGURE_CART_PUSHER)) {
            return;
        }
        resource_type output_resource = RESOURCE_NONE;
        int output_cart_loads = 0;
        if (building_obj.reserve_output_storage_loads(&output_resource, &output_cart_loads)) {
            b->output_resource_id = static_cast<unsigned char>(output_resource);
            Figure *f = Figure::create(FIGURE_CART_PUSHER, road.x, road.y, DIR_4_BOTTOM);
            f->action_state = FIGURE_ACTION_20_CARTPUSHER_INITIAL;
            f->resource_id = static_cast<unsigned char>(output_resource);
            attach_figure_to_building(f, b);
            b->figure_id = f->id();
            f->wait_ticks = static_cast<short>(game_time_scale_legacy_day_ticks(30));
            f->loads_sold_or_carrying = static_cast<unsigned char>(output_cart_loads > 0 ? output_cart_loads : 1);
            return;
        }
    }
}

void BuildingFigureGenerator::spawn_figure_dock(building *b)
{
    building_runtime *runtime = runtime_for_record(b);
    if (!runtime) {
        return;
    }
    Building &building = runtime->building;
    runtime->check_labor_problem();
    map_point road;
    if (map_has_road_access_building(b->x, b->y, &road)) {
        runtime->run_labor_phase_if_defined(road);
        int max_dockers = worker_slot_capacity(building, 3, 2, 1);
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

void BuildingFigureGenerator::spawn_figure_mess_hall(building *b)
{
    building_runtime *runtime = runtime_for_record(b);
    if (!runtime) {
        return;
    }
    Building &building = runtime->building;
    runtime->check_labor_problem();
    map_point road;
    if (map_has_road_access_building(b->x, b->y, &road)) {
        runtime->run_labor_phase_if_defined(road);
        int spawn_delay = worker_scaled_spawn_delay(building, 7, 15, 29, 44, 59);
        if (!spawn_delay) {
            return;
        }

        runtime->spawn_primary_mess_hall_supplier(road);
        if (b->figure_id) {
            b->figure_spawn_delay++;
        } else {
            b->figure_spawn_delay = 0;
        }
        if (grand_temple_for_god(GOD_MARS, true) && b->figure_spawn_delay >= spawn_delay) {
            runtime->spawn_secondary_mess_hall_supplier(road);
            b->figure_spawn_delay = 0;
        }
    }
}

void BuildingFigureGenerator::spawn_figure_depot(building *b)
{
    building_runtime *runtime = runtime_for_record(b);
    if (!runtime) {
        return;
    }
    Building &building = runtime->building;
    runtime->check_labor_problem();

    map_point road;
    if (map_has_road_access_building(b->x, b->y, &road)) {
        runtime->run_labor_phase_if_defined(road);

        int max_carts = worker_slot_capacity(building, 3, 2, 1);

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

        int spawn_delay = default_spawn_delay(building);
        b->figure_spawn_delay++;
        if (b->figure_spawn_delay > spawn_delay) {
            b->figure_spawn_delay = 0;
            Figure *f = figure_runtime_create_profiled(
                FIGURE_DEPOT_CART_PUSHER,
                road.x,
                road.y,
                DIR_0_TOP,
                building,
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
    Building::for_each([&](Building *runtime_building) {
        building *b = const_cast<building *>(runtime_building->record());

        Building &building_object = *runtime_building;
        if (b->state != BUILDING_STATE_IN_USE) {
            b->show_on_problem_overlay = 1;
            return;
        }
        if (building_object.type && building_monument_is_unfinished_monument(b)) {
            const auto &gift = building_object.type->construction().gift;
            city_monument_gift_supply(building_object, gift.materials, gift.workers, gift.loads_per_delivery);
        }
        if ((building_object.Composition && building_object.Composition->is_child()) ||
            building_object.is_dynamic_bridge_segment() ||
            building_monument_is_unfinished_monument(b)) {
            return;
        }

        b->show_on_problem_overlay = 0;
        if (building_object.type && building_object.type->has_race()) {
            BuildingEntertainment(building_object).spawn_race_participants();
        }
        if (building_uses_runtime_spawn(b)) {
            building_object.spawn_figure();
        } else if (building_is_raw_resource_producer(building_object.type) ||
            building_object.type->is_farm() || building_is_workshop(building_object.type)) {
            spawn_figure_industry(b);
        } else if (b && building_type_registry_impl::type_attr_is_any(b->type, {"senate_1_unused", "forum_2_unused"})) {
            spawn_figure_senate_forum(b);
        } else if (building_object.type->is_warehouse()) {
            spawn_figure_warehouse(b);
        } else if (building_object.type->is_granary()) {
            spawn_figure_granary(b);
        } else if (b && building_type_registry_impl::type_attr_is(b->type, "tower")) {
            building_military_spawn_tower(building_object);
        } else if (b && building_type_registry_impl::type_attr_is(b->type, "hippodrome")) {
            BuildingEntertainment(building_object).spawn_hippodrome_service();
        } else if (b && building_type_registry_impl::type_attr_is(b->type, "colosseum")) {
            BuildingEntertainment(building_object).spawn_colosseum_service();
        } else if (b && building_type_registry_impl::type_attr_is(b->type, "market")) {
            spawn_figure_market(b);
        } else if (b && building_type_registry_impl::type_attr_is(b->type, "mission_post")) {
            NativeBuildingSpawner(building_object).spawn_missionary();
        } else if (building_object.type->attr_is("dock")) {
            spawn_figure_dock(b);
        } else if (b && building_type_registry_impl::type_attr_is_any(b->type, {"native_hut", "native_hut_alt"})) {
            NativeBuildingSpawner(building_object).spawn_hut_resident();
        } else if (b && building_type_registry_impl::type_attr_is(b->type, "native_meeting")) {
            NativeBuildingSpawner(building_object).spawn_meeting_trader();
        } else if (b && building_type_registry_impl::type_attr_is(b->type, "native_crops")) {
            NativeBuildingSpawner(building_object).advance_crop_graphics();
        } else if (building_is_fort(b->type)) {
            formation_legion_update_recruit_status(building_object);
            MessHall::spawn_supplier_for_fort(building_object);
        } else if (b && building_type_registry_impl::type_attr_is(b->type, "barracks")) {
            Barracks(building_object).spawn_recruitment();
        } else if (b && building_type_registry_impl::type_attr_is(b->type, "military_academy")) {
            building_military_run_academy(building_object);
        } else if (building_object.type->is_mess_hall()) {
            spawn_figure_mess_hall(b);
        } else if (b && building_type_registry_impl::type_attr_is(b->type, "tavern")) {
            spawn_figure_tavern(b);
        } else if (b && building_type_registry_impl::type_attr_is(b->type, "cart_depot")) {
            spawn_figure_depot(b);
        }
    });
}

void building_figure_generate(void)
{
    BuildingFigureGenerator generator;
    generator.generate();
}
