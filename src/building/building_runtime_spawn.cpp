#include "building/building_runtime_internal.h"
#include "building/building_type_registry_internal.h"
#include "building/local_workforce.h"
#include "building/water_access_runtime.h"

#include "assets/image_group_payload.h"
#include "building/building_runtime_graphics.h"
#include "building/production_runtime_api.h"
#include "building/storage_runtime_api.h"
#include "core/crash_context.h"

extern "C" {
#include "building/armoury.h"
#include "building/barracks.h"
#include "building/building_runtime_api.h"
#include "building/count.h"
#include "building/caravanserai.h"
#include "building/distribution.h"
#include "building/granary.h"
#include "building/image.h"
#include "building/industry.h"
#include "building/lighthouse.h"
#include "building/monument.h"
#include "building/properties.h"
#include "building/temple.h"
#include "building/warehouse.h"
#include "city/buildings.h"
#include "city/data_private.h"
#include "city/population.h"
#include "core/calc.h"
#include "core/config.h"
#include "figure/action.h"
#include "figure/figure.h"
#include "figure/movement.h"
#include "figure/figure_runtime_api.h"
#include "game/animation.h"
#include "game/resource.h"
#include "game/time.h"
#include "map/building_tiles.h"
#include "map/road_access.h"
#include "map/sprite.h"
#include "map/terrain.h"
#include "core/log.h"
}

#include <cstdio>
#include <cstdint>
#include <string>

int building_runtime::worker_percentage() const
{
    return calc_percentage(building_->num_workers, model_get_building(building_->type)->laborers);
}

int building_runtime::default_spawn_delay() const
{
    static const std::vector<building_type_registry_impl::DelayBand> kDefaultDelayBands = {
        { 100, 3 },
        { 75, 7 },
        { 50, 15 },
        { 25, 29 },
        { 1, 44 }
    };
    int delay = evaluate_delay(kDefaultDelayBands);
    return delay < 0 ? 0 : delay;
}

void building_runtime::check_labor_problem()
{
    if (building_local_workforce_is_workforce_building(building_)) {
        if (building_->num_workers <= 0) {
            building_->show_on_problem_overlay = 2;
        }
        return;
    }
    if (building_->houses_covered <= 0) {
        building_->show_on_problem_overlay = 2;
    }
}

void building_runtime::generate_labor_seeker(int x, int y)
{
    if (city_population() <= 0) {
        return;
    }
    if (config_get(CONFIG_GP_CH_GLOBAL_LABOUR)) {
        if (building_->distance_from_entry) {
            building_->houses_covered = 100;
        } else {
            building_->houses_covered = 0;
        }
        return;
    }
    if (building_->figure_id2) {
        figure *existing = figure_get(building_->figure_id2);
        if (!existing->state || existing->type != FIGURE_LABOR_SEEKER || existing->building_id != building_->id) {
            building_->figure_id2 = 0;
        }
        return;
    }

    figure *labor_seeker = figure_create(FIGURE_LABOR_SEEKER, x, y, DIR_0_TOP);
    labor_seeker->action_state = FIGURE_ACTION_125_ROAMING;
    labor_seeker->building_id = building_->id;
    building_->figure_id2 = labor_seeker->id;
    figure_movement_init_roaming(labor_seeker);
}

void building_runtime::spawn_labor_seeker(int x, int y, int min_houses)
{
    if (config_get(CONFIG_GP_CH_GLOBAL_LABOUR)) {
        if (building_->distance_from_entry) {
            building_->houses_covered = 2 * min_houses;
        } else {
            building_->houses_covered = 0;
        }
    } else if (building_->houses_covered <= min_houses) {
        generate_labor_seeker(x, y);
    }
}

void building_runtime::run_labor_phase(const building_type_registry_impl::LaborDefinition &labor, const map_point &road)
{
    if (!labor.has_seeker_policy()) {
        return;
    }

    const building_type_registry_impl::LaborSeekerPolicy &labor_policy = labor.seeker_policy();
    if (labor_policy.method == building_type_registry_impl::LaborSeekerMethod::Workforce &&
        !config_get(CONFIG_GP_CH_GLOBAL_LABOUR)) {
        const int trigger_workers = labor_policy.amount;
        const int workforce_access = building_local_workforce_access_score(building_);
        if (workforce_access < trigger_workers) {
            if (!building_local_workforce_spawn_acquisition(building_, &road) && workforce_access > 0) {
                building_local_workforce_spawn_validation(building_, &road);
            }
        } else {
            building_local_workforce_spawn_validation(building_, &road);
        }
        return;
    }

    switch (labor_policy.method) {
        case building_type_registry_impl::LaborSeekerMethod::HousesSpawnIfBelow:
        case building_type_registry_impl::LaborSeekerMethod::Workforce:
            spawn_labor_seeker(road.x, road.y, labor_policy.amount);
            break;
        case building_type_registry_impl::LaborSeekerMethod::HousesGenerateIfBelow:
            if (building_->houses_covered <= labor_policy.amount) {
                generate_labor_seeker(road.x, road.y);
            }
            break;
        case building_type_registry_impl::LaborSeekerMethod::None:
        default:
            break;
    }
}

int building_runtime::has_figure_of_type(figure_type type)
{
    if (building_->figure_id <= 0) {
        return 0;
    }
    figure *existing = figure_get(building_->figure_id);
    if (existing->state && existing->building_id == building_->id && existing->type == type) {
        return 1;
    }
    building_->figure_id = 0;
    return 0;
}

int building_runtime::has_figure_of_any(const std::vector<figure_type> &types)
{
    if (building_->figure_id <= 0) {
        return 0;
    }

    // Group guards reason about one tracked slot. Do not call the single-type
    // helper repeatedly here because it clears the slot when that one type does
    // not match, which breaks alternates that intentionally share the slot.
    figure *existing = figure_get(building_->figure_id);
    if (!existing || !existing->state || existing->building_id != building_->id) {
        building_->figure_id = 0;
        return 0;
    }

    // Mixed entertainment venues share one legacy slot for alternate walker
    // types, such as amphitheater actors and gladiators.
    for (figure_type type : types) {
        if (existing->type == type) {
            return 1;
        }
    }
    building_->figure_id = 0;
    return 0;
}

unsigned int *building_runtime::figure_slot_storage(building_type_registry_impl::FigureSlot slot)
{
    switch (slot) {
        case building_type_registry_impl::FigureSlot::Primary:
            return &building_->figure_id;
        case building_type_registry_impl::FigureSlot::Secondary:
            return &building_->figure_id2;
        case building_type_registry_impl::FigureSlot::Quaternary:
            return &building_->figure_id4;
        case building_type_registry_impl::FigureSlot::None:
        default:
            return nullptr;
    }
}

int building_runtime::slot_has_live_figure(
    building_type_registry_impl::FigureSlot slot,
    figure_type primary_type,
    figure_type secondary_type)
{
    unsigned int *slot_value = figure_slot_storage(slot);
    if (!slot_value || *slot_value <= 0) {
        return 0;
    }

    figure *existing = figure_get(*slot_value);
    if (!existing || !existing->state || existing->building_id != building_->id) {
        *slot_value = 0;
        return 0;
    }
    if (existing->type != primary_type && existing->type != secondary_type) {
        *slot_value = 0;
        return 0;
    }
    return 1;
}

void building_runtime::send_supplier_to_destination(figure *supplier, int destination_building_id)
{
    if (!supplier) {
        return;
    }
    if (supplier->destination_building_id) {
        supplier->last_destinatation_id = supplier->destination_building_id;
    }

    supplier->destination_building_id = destination_building_id;
    ::building *destination = building_get(destination_building_id);
    if (!destination) {
        supplier->action_state = FIGURE_ACTION_146_SUPPLIER_RETURNING;
        supplier->destination_x = supplier->x;
        supplier->destination_y = supplier->y;
        return;
    }

    map_point road;
    int destination_found = 0;
    if (destination->type == BUILDING_WAREHOUSE) {
        if (map_has_road_access_warehouse(destination->x, destination->y, &road)) {
            destination_found = 1;
        }
    } else if (destination->type == BUILDING_GRANARY) {
        if (map_has_road_access_granary(destination->x, destination->y, &road)) {
            destination_found = 1;
        }
    } else if (destination->type == BUILDING_GRAND_TEMPLE_VENUS) {
        if (map_has_road_access(destination->x, destination->y, destination->size, &road)) {
            destination_found = 1;
        }
    }

    if (destination_found) {
        supplier->action_state = FIGURE_ACTION_145_SUPPLIER_GOING_TO_STORAGE;
        supplier->destination_x = road.x;
        supplier->destination_y = road.y;
    } else {
        supplier->action_state = FIGURE_ACTION_146_SUPPLIER_RETURNING;
        supplier->destination_x = supplier->x;
        supplier->destination_y = supplier->y;
    }
}

int building_runtime::spawn_caravanserai_supplier(const map_point &road)
{
    if (slot_has_live_figure(
            building_type_registry_impl::FigureSlot::Primary,
            FIGURE_CARAVANSERAI_SUPPLIER,
            FIGURE_LABOR_SEEKER)) {
        return 0;
    }

    int destination_building_id = building_caravanserai_get_storage_destination(building_);
    if (!destination_building_id) {
        return 0;
    }

    figure *supplier = figure_create(FIGURE_CARAVANSERAI_SUPPLIER, road.x, road.y, DIR_0_TOP);
    if (!supplier) {
        return 0;
    }

    supplier->building_id = building_->id;
    supplier->collecting_item_id = building_->data.market.fetch_inventory_id;
    building_->figure_id = supplier->id;
    send_supplier_to_destination(supplier, destination_building_id);
    return 1;
}

int building_runtime::spawn_lighthouse_supplier(const map_point &road)
{
    if (slot_has_live_figure(
            building_type_registry_impl::FigureSlot::Primary,
            FIGURE_LIGHTHOUSE_SUPPLIER,
            FIGURE_LABOR_SEEKER)) {
        return 0;
    }

    int destination_building_id = building_lighthouse_get_storage_destination(building_);
    if (!destination_building_id) {
        return 0;
    }

    figure *supplier = figure_create(FIGURE_LIGHTHOUSE_SUPPLIER, road.x, road.y, DIR_0_TOP);
    if (!supplier) {
        return 0;
    }

    supplier->building_id = building_->id;
    supplier->collecting_item_id = RESOURCE_TIMBER;
    building_->figure_id = supplier->id;
    send_supplier_to_destination(supplier, destination_building_id);
    return 1;
}

int building_runtime::spawn_temple_supplier(const map_point &road)
{
    if (!building_is_ceres_temple(building_->type) && !building_is_venus_temple(building_->type)) {
        return 0;
    }
    if (building_is_ceres_temple(building_->type) &&
        !building_monument_gt_module_is_active(CERES_MODULE_2_DISTRIBUTE_FOOD)) {
        return 0;
    }
    if (building_is_venus_temple(building_->type) &&
        !building_monument_gt_module_is_active(VENUS_MODULE_1_DISTRIBUTE_WINE)) {
        return 0;
    }
    if (slot_has_live_figure(
            building_type_registry_impl::FigureSlot::Secondary,
            FIGURE_PRIEST_SUPPLIER,
            FIGURE_LABOR_SEEKER)) {
        return 0;
    }

    building_distribution_update_demands(building_);
    int destination_building_id = building_temple_get_storage_destination(building_);
    if (!destination_building_id) {
        return 0;
    }

    figure *supplier = figure_create(FIGURE_PRIEST_SUPPLIER, road.x, road.y, DIR_0_TOP);
    if (!supplier) {
        return 0;
    }
    supplier->building_id = building_->id;
    supplier->collecting_item_id = building_->data.market.fetch_inventory_id;
    building_->figure_id2 = supplier->id;
    send_supplier_to_destination(supplier, destination_building_id);
    return 1;
}

int building_runtime::spawn_temple_destination_priest(const map_point &road)
{
    if (building_->type == BUILDING_PANTHEON ||
        !building_monument_pantheon_module_is_active(PANTHEON_MODULE_1_DESTINATION_PRIESTS) ||
        slot_has_live_figure(building_type_registry_impl::FigureSlot::Quaternary, FIGURE_PRIEST)) {
        return 0;
    }

    int pantheon_id = building_monument_working(BUILDING_PANTHEON);
    if (!pantheon_id) {
        return 0;
    }

    figure *priest = figure_create(FIGURE_PRIEST, road.x, road.y, DIR_4_BOTTOM);
    if (!priest) {
        return 0;
    }
    building_->figure_id4 = priest->id;
    priest->destination_building_id = pantheon_id;
    priest->building_id = building_->id;
    priest->action_state = FIGURE_ACTION_212_DESTINATION_PRIEST_CREATED;
    return 1;
}

int building_runtime::spawn_temple_mars_mess_hall_priest(const map_point &road)
{
    if (!building_is_mars_temple(building_->type) ||
        !building_monument_gt_module_is_active(MARS_MODULE_1_MESS_HALL)) {
        return 0;
    }

    int mess_hall_id = city_buildings_get_mess_hall();
    ::building *mess_hall = building_get(mess_hall_id);
    if (!mess_hall_id || !mess_hall || mess_hall->type != BUILDING_MESS_HALL) {
        return 0;
    }

    if (building_->figure_id2) {
        figure *existing = figure_get(building_->figure_id2);
        if (existing && existing->state == FIGURE_STATE_ALIVE && existing->type == FIGURE_PRIEST) {
            return 0;
        }
        if (!existing || existing->state != FIGURE_STATE_ALIVE) {
            building_->figure_id2 = 0;
        }
    }

    int food_to_deliver = building_temple_mars_food_to_deliver(building_, mess_hall_id);
    if (food_to_deliver < 0) {
        return 0;
    }

    figure *priest = figure_create(FIGURE_PRIEST, road.x, road.y, DIR_4_BOTTOM);
    if (!priest) {
        return 0;
    }
    priest->collecting_item_id = static_cast<unsigned char>(food_to_deliver);
    building_->figure_id2 = priest->id;
    priest->destination_building_id = mess_hall_id;
    priest->building_id = building_->id;
    priest->action_state = FIGURE_ACTION_214_DESTINATION_MARS_PRIEST_CREATED;
    return 1;
}

int building_runtime::spawn_temple_neptune_chariot(const map_point &road)
{
    if (!building_is_neptune_temple(building_->type) ||
        !building_monument_gt_module_is_active(NEPTUNE_MODULE_1_HIPPODROME_ACCESS) ||
        building_->figure_id2) {
        return 0;
    }

    building_->days_since_offering++;
    if (building_->days_since_offering <= 1) {
        return 0;
    }

    figure *charioteer = figure_runtime_create_profiled(
        FIGURE_CHARIOTEER,
        road.x,
        road.y,
        DIR_0_TOP,
        building_->id,
        "venue_seeker");
    if (!charioteer) {
        charioteer = figure_create(FIGURE_CHARIOTEER, road.x, road.y, DIR_0_TOP);
        if (!charioteer) {
            return 0;
        }
        charioteer->action_state = FIGURE_ACTION_90_ENTERTAINER_AT_SCHOOL_CREATED;
        charioteer->building_id = building_->id;
    }
    building_->figure_id2 = charioteer->id;
    building_->days_since_offering = 0;
    return 1;
}

int building_runtime::spawn_grand_temple_mars_recruit(const map_point &road)
{
    if (building_->type != BUILDING_GRAND_TEMPLE_MARS) {
        return 0;
    }

    static const std::vector<building_type_registry_impl::DelayBand> kGrandTempleMarsDelays = {
        { 100, 8 },
        { 75, 12 },
        { 50, 16 },
        { 25, 32 },
        { 1, 48 }
    };
    int spawn_delay = evaluate_delay(kGrandTempleMarsDelays);
    if (spawn_delay < 0) {
        return 0;
    }
    if (city_data.mess_hall.food_stress_cumulative > 20) {
        spawn_delay += game_time_scale_legacy_day_ticks(city_data.mess_hall.food_stress_cumulative - 20);
    }

    building_->figure_spawn_delay++;
    if (building_->figure_spawn_delay <= spawn_delay) {
        return 0;
    }
    building_->figure_spawn_delay = 0;

    switch (building_->subtype.barracks_priority) {
        case PRIORITY_FORT:
        case PRIORITY_FORT_JAVELIN:
        case PRIORITY_FORT_MOUNTED:
        case PRIORITY_FORT_AUXILIA_INFANTRY:
        case PRIORITY_FORT_AUXILIA_ARCHERY:
            if (!building_barracks_create_soldier(building_, road.x, road.y)) {
                building_barracks_create_tower_sentry(building_, road.x, road.y);
            }
            break;
        default:
            if (!building_barracks_create_tower_sentry(building_, road.x, road.y)) {
                building_barracks_create_soldier(building_, road.x, road.y);
            }
            break;
    }

    if (!has_figure_of_type(FIGURE_PRIEST)) {
        figure *priest = figure_create(FIGURE_PRIEST, road.x, road.y, DIR_0_TOP);
        if (priest) {
            priest->action_state = FIGURE_ACTION_125_ROAMING;
            priest->building_id = building_->id;
            building_->figure_id = priest->id;
            figure_movement_init_roaming(priest);
        }
    }
    return 1;
}

void building_runtime::spawn_architect_guild()
{
    check_labor_problem();

    map_point road;
    if (!resolve_road_access(building_type_registry_impl::RoadAccessMode::Normal, &road)) {
        return;
    }

    if (definition_ && definition_->has_labor()) {
        run_labor_phase(definition_->labor(), road);
    }
    if (has_figure_of_type(FIGURE_WORK_CAMP_ARCHITECT)) {
        return;
    }

    int spawn_delay = default_spawn_delay();
    if (!spawn_delay) {
        return;
    }

    building_->figure_spawn_delay++;
    if (building_->figure_spawn_delay <= spawn_delay) {
        return;
    }

    building_->figure_spawn_delay = 0;
    if (!building_monument_get_monument(road.x, road.y, RESOURCE_NONE, building_->road_network_id, 0)) {
        return;
    }

    figure *architect = figure_create(FIGURE_WORK_CAMP_ARCHITECT, road.x, road.y, DIR_4_BOTTOM);
    if (!architect) {
        return;
    }
    architect->action_state = FIGURE_ACTION_206_WORK_CAMP_ARCHITECT_CREATED;
    architect->building_id = building_->id;
    building_->figure_id = architect->id;
}

void building_runtime::spawn_caravanserai()
{
    check_labor_problem();

    map_point road;
    if (!resolve_road_access(building_type_registry_impl::RoadAccessMode::Normal, &road)) {
        return;
    }

    if (definition_ && definition_->has_labor()) {
        run_labor_phase(definition_->labor(), road);
    }

    int spawn_delay = default_spawn_delay();
    if (!spawn_delay) {
        return;
    }

    building_->figure_spawn_delay++;
    if (building_->figure_spawn_delay <= spawn_delay) {
        return;
    }

    building_->figure_spawn_delay = 0;
    spawn_caravanserai_supplier(road);
}

void building_runtime::spawn_lighthouse()
{
    check_labor_problem();

    map_point road;
    if (!resolve_road_access(building_type_registry_impl::RoadAccessMode::Normal, &road)) {
        return;
    }

    if (definition_ && definition_->has_labor()) {
        run_labor_phase(definition_->labor(), road);
    }

    int spawn_delay = default_spawn_delay();
    if (!spawn_delay) {
        return;
    }

    building_->figure_spawn_delay++;
    if (building_->figure_spawn_delay <= spawn_delay) {
        return;
    }

    building_->figure_spawn_delay = 0;
    spawn_lighthouse_supplier(road);
}

void building_runtime::spawn_watchtower()
{
    check_labor_problem();
    if (building_->figure_id || building_->figure_id2) {
        return;
    }

    map_point road;
    if (!resolve_road_access(building_type_registry_impl::RoadAccessMode::Normal, &road)) {
        return;
    }

    if (definition_ && definition_->has_labor()) {
        run_labor_phase(definition_->labor(), road);
    }
    if (building_->figure_id2) {
        return;
    }
    if (!slot_has_live_figure(building_type_registry_impl::FigureSlot::Quaternary, FIGURE_WATCHTOWER_ARCHER)) {
        return;
    }

    static const std::vector<building_type_registry_impl::DelayBand> kWatchtowerDelays = {
        { 100, 10 },
        { 75, 20 },
        { 50, 30 },
        { 25, 40 },
        { 1, 60 }
    };
    int spawn_delay = evaluate_delay(kWatchtowerDelays);
    if (spawn_delay < 0) {
        return;
    }

    building_->figure_spawn_delay++;
    if (building_->figure_spawn_delay <= spawn_delay) {
        return;
    }

    building_->figure_spawn_delay = 0;
    figure *primary_watchman = figure_create(FIGURE_WATCHMAN, road.x, road.y, DIR_0_TOP);
    if (!primary_watchman) {
        return;
    }
    primary_watchman->action_state = FIGURE_ACTION_220_WATCHMAN_PATROL_INITIATE;
    primary_watchman->building_id = building_->id;
    building_->figure_id = primary_watchman->id;

    figure *secondary_watchman = figure_create(FIGURE_WATCHMAN, road.x, road.y, DIR_0_TOP);
    if (!secondary_watchman) {
        return;
    }
    secondary_watchman->action_state = FIGURE_ACTION_220_WATCHMAN_PATROL_INITIATE;
    secondary_watchman->building_id = building_->id;
    building_->figure_id2 = secondary_watchman->id;
}

void building_runtime::spawn_armoury()
{
    check_labor_problem();

    map_point road;
    if (!resolve_road_access(building_type_registry_impl::RoadAccessMode::Normal, &road)) {
        return;
    }

    if (definition_ && definition_->has_labor()) {
        run_labor_phase(definition_->labor(), road);
    }

    static const std::vector<building_type_registry_impl::DelayBand> kArmouryDelays = {
        { 100, 3 },
        { 75, 8 },
        { 50, 16 },
        { 25, 24 },
        { 1, 48 }
    };
    int pct_workers = worker_percentage();
    int carts_available = pct_workers >= 100 ? 2 : 1;
    int spawn_delay = evaluate_delay(kArmouryDelays);
    if (spawn_delay < 0) {
        return;
    }

    int has_primary = slot_has_live_figure(building_type_registry_impl::FigureSlot::Primary, FIGURE_WAREHOUSEMAN);
    int has_quaternary = slot_has_live_figure(building_type_registry_impl::FigureSlot::Quaternary, FIGURE_WAREHOUSEMAN);
    if (has_primary && carts_available == 1) {
        return;
    }
    if (has_primary && has_quaternary) {
        return;
    }

    building_type_registry_impl::FigureSlot target_slot =
        has_primary ? building_type_registry_impl::FigureSlot::Quaternary : building_type_registry_impl::FigureSlot::Primary;

    building_->figure_spawn_delay++;
    if (building_->figure_spawn_delay <= spawn_delay) {
        return;
    }

    building_->figure_spawn_delay = 0;
    if (!building_armoury_is_needed(building_)) {
        return;
    }

    figure *warehouseman = figure_create(FIGURE_WAREHOUSEMAN, road.x, road.y, DIR_4_BOTTOM);
    if (!warehouseman) {
        return;
    }
    warehouseman->action_state = FIGURE_ACTION_50_WAREHOUSEMAN_CREATED;
    warehouseman->collecting_item_id = RESOURCE_WEAPONS;
    warehouseman->building_id = building_->id;
    assign_figure_slot(target_slot, warehouseman->id);
}

int building_runtime::resolve_road_access(building_type_registry_impl::RoadAccessMode mode, map_point *road) const
{
    switch (mode) {
        case building_type_registry_impl::RoadAccessMode::Normal:
            return map_has_road_access(building_->x, building_->y, building_->size, road);
        case building_type_registry_impl::RoadAccessMode::None:
        default:
            return 0;
    }
}

int building_runtime::evaluate_delay(const std::vector<building_type_registry_impl::DelayBand> &delay_bands) const
{
    int pct_workers = worker_percentage();
    for (const building_type_registry_impl::DelayBand &delay_band : delay_bands) {
        if (pct_workers >= delay_band.min_worker_percentage) {
            return game_time_scale_legacy_day_ticks(delay_band.delay);
        }
    }
    return -1;
}

int building_runtime::evaluate_condition(building_type_registry_impl::SpawnCondition condition) const
{
    switch (condition) {
        case building_type_registry_impl::SpawnCondition::Always:
            return 1;
        case building_type_registry_impl::SpawnCondition::Days1Positive:
            return building_->data.entertainment.days1 > 0;
        case building_type_registry_impl::SpawnCondition::Days1NotPositive:
            return building_->data.entertainment.days1 <= 0;
        case building_type_registry_impl::SpawnCondition::Days2Positive:
            return building_->data.entertainment.days2 > 0;
        case building_type_registry_impl::SpawnCondition::Days1OrDays2Positive:
            return building_->data.entertainment.days1 > 0 || building_->data.entertainment.days2 > 0;
        default:
            return 0;
    }
}

int building_runtime::should_apply_graphic_for_timing(
    const building_type_registry_impl::SpawnDelayGroup &group,
    building_type_registry_impl::GraphicTiming timing) const
{
    for (const building_type_registry_impl::SpawnPolicy &policy : group.policies) {
        if (policy.graphic_timing == timing && evaluate_condition(policy.condition)) {
            return 1;
        }
    }
    return 0;
}

unsigned char building_runtime::get_spawn_delay_counter(size_t policy_index) const
{
    if (policy_index == 0) {
        return building_->figure_spawn_delay;
    }
    if (spawn_delay_counters_.size() <= policy_index) {
        return 0;
    }
    return spawn_delay_counters_[policy_index];
}

void building_runtime::set_spawn_delay_counter(size_t policy_index, unsigned char value)
{
    if (policy_index == 0) {
        building_->figure_spawn_delay = value;
        return;
    }
    if (spawn_delay_counters_.size() <= policy_index) {
        spawn_delay_counters_.resize(policy_index + 1, 0);
    }
    spawn_delay_counters_[policy_index] = value;
}

void building_runtime::assign_figure_slot(building_type_registry_impl::FigureSlot slot, unsigned int figure_id)
{
    switch (slot) {
        case building_type_registry_impl::FigureSlot::Primary:
            building_->figure_id = figure_id;
            break;
        case building_type_registry_impl::FigureSlot::Secondary:
            building_->figure_id2 = figure_id;
            break;
        case building_type_registry_impl::FigureSlot::Quaternary:
            building_->figure_id4 = figure_id;
            break;
        case building_type_registry_impl::FigureSlot::None:
        default:
            break;
    }
}

int building_runtime::create_spawned_figure(const building_type_registry_impl::SpawnPolicy &policy, const map_point &road)
{
    if (policy.figure_slot != building_type_registry_impl::FigureSlot::None &&
        slot_has_live_figure(policy.figure_slot, policy.spawn_figure)) {
        return 0;
    }

    int spawned_any = 0;
    int spawn_count = policy.spawn_count > 0 ? policy.spawn_count : 1;
    for (int i = 0; i < spawn_count; i++) {
        // XML profiles are the handoff point between BuildingType spawn policy and FigureType behavior.
        figure *spawned = policy.profile.empty() ?
            figure_create(policy.spawn_figure, road.x, road.y, static_cast<direction_type>(policy.spawn_direction)) :
            figure_runtime_create_profiled(
                policy.spawn_figure,
                road.x,
                road.y,
                static_cast<direction_type>(policy.spawn_direction),
                building_->id,
                policy.profile.c_str());
        if (!spawned) {
            continue;
        }
        if (policy.profile.empty()) {
            spawned->action_state = policy.action_state;
            spawned->building_id = building_->id;
        }
        // A multi-spawn policy still only owns one legacy tracked slot today; later spawns remain untracked for now.
        if (!spawned_any) {
            assign_figure_slot(policy.figure_slot, spawned->id);
        }
        if (policy.profile.empty() && policy.init_roaming) {
            figure_movement_init_roaming(spawned);
        }
        spawned_any = 1;
    }
    return spawned_any;
}

int building_runtime::try_spawn_policy(const building_type_registry_impl::SpawnPolicy &policy, const map_point &road)
{
    if (!evaluate_condition(policy.condition)) {
        return 0;
    }

    if (policy.mark_problem_if_no_water && !building_->has_water_access) {
        building_->show_on_problem_overlay = 2;
    }

    if (policy.require_water_access && !building_->has_water_access) {
        return 0;
    }

    if (policy.graphic_timing == building_type_registry_impl::GraphicTiming::BeforeSuccessfulSpawn) {
        set_building_graphic();
    }

    switch (policy.mode) {
        case building_type_registry_impl::SpawnMode::ServiceRoamer:
            return create_spawned_figure(policy, road);
        case building_type_registry_impl::SpawnMode::TempleSupplier:
            return spawn_temple_supplier(road);
        case building_type_registry_impl::SpawnMode::TempleDestinationPriest:
            return spawn_temple_destination_priest(road);
        case building_type_registry_impl::SpawnMode::TempleMarsMessHallPriest:
            return spawn_temple_mars_mess_hall_priest(road);
        case building_type_registry_impl::SpawnMode::TempleNeptuneChariot:
            return spawn_temple_neptune_chariot(road);
        case building_type_registry_impl::SpawnMode::GrandTempleMarsRecruit:
            return spawn_grand_temple_mars_recruit(road);
        case building_type_registry_impl::SpawnMode::None:
        default:
            return 0;
    }
}

void building_runtime::spawn_service_roamer_group(
    const building_type_registry_impl::SpawnDelayGroup &group,
    size_t group_index,
    int run_labor)
{
    check_labor_problem();
    if (should_apply_graphic_for_timing(group, building_type_registry_impl::GraphicTiming::OnSpawnEntry)) {
        set_building_graphic();
    }

    if (group.guard_timing == building_type_registry_impl::GuardTiming::BeforeRoadAccess &&
        !group.existing_figures.empty() && has_figure_of_any(group.existing_figures)) {
        return;
    }

    map_point road;
    if (!resolve_road_access(group.road_access_mode, &road)) {
        return;
    }

    if (run_labor && definition_ && definition_->has_labor()) {
        run_labor_phase(definition_->labor(), road);
    }

    if (group.guard_timing == building_type_registry_impl::GuardTiming::AfterLaborSeeker &&
        !group.existing_figures.empty() && has_figure_of_any(group.existing_figures)) {
        return;
    }

    if (should_apply_graphic_for_timing(group, building_type_registry_impl::GraphicTiming::BeforeDelayCheck)) {
        set_building_graphic();
    }

    if (!group.policies.empty() &&
        group.policies.front().mode == building_type_registry_impl::SpawnMode::GrandTempleMarsRecruit) {
        for (const building_type_registry_impl::SpawnPolicy &policy : group.policies) {
            if (try_spawn_policy(policy, road) && policy.block_on_success) {
                break;
            }
        }
        return;
    }

    int spawn_delay = evaluate_delay(group.delay_bands);
    if (spawn_delay < 0) {
        return;
    }

    unsigned char delay_counter = get_spawn_delay_counter(group_index);
    delay_counter++;
    if (delay_counter <= spawn_delay) {
        set_spawn_delay_counter(group_index, delay_counter);
        return;
    }

    set_spawn_delay_counter(group_index, 0);
    for (const building_type_registry_impl::SpawnPolicy &policy : group.policies) {
        if (try_spawn_policy(policy, road) && policy.block_on_success) {
            break;
        }
    }
}

void building_runtime::spawn_figure()
{
    if (!building_ || !definition_ || building_->state != BUILDING_STATE_IN_USE) {
        return;
    }

    refresh_runtime_state();

    const std::vector<building_type_registry_impl::SpawnDelayGroup> &spawn_groups = definition_->spawn_groups();
    if (spawn_groups.empty()) {
        if (definition_->has_graphic()) {
            set_building_graphic();
        }

        switch (building_->type) {
            case BUILDING_ARCHITECT_GUILD:
                spawn_architect_guild();
                return;
            case BUILDING_CARAVANSERAI:
                spawn_caravanserai();
                return;
            case BUILDING_LIGHTHOUSE:
                spawn_lighthouse();
                return;
            case BUILDING_WATCHTOWER:
                spawn_watchtower();
                return;
            case BUILDING_ARMOURY:
                spawn_armoury();
                return;
            default:
                return;
        }
    }

    // Groups own the shared delay/guard phase, then policies inside them can either cooperate or block one another.
    for (size_t i = 0; i < spawn_groups.size(); i++) {
        const building_type_registry_impl::SpawnDelayGroup &group = spawn_groups[i];
        if (!group.policies.empty()) {
            spawn_service_roamer_group(group, i, i == 0);
        }
    }
}
