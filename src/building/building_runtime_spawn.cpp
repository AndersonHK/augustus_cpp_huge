#include "building/count.h"
#include "building/distribution.h"
#include "building/industry.h"
#include "building/lighthouse.h"
#include "city/labor.h"
#include "figure/action.h"
#include "figure/figure.h"
#include "figure/figure_runtime_api.h"
#include "map/road_access.h"

#include "building/building.h"
#include "building/armoury.h"
#include "building/barracks.h"
#include "building/building_record.h"
#include "building/building_runtime_internal.h"
#include "building/building_type_registry_internal.h"
#include "building/caravanserai.h"
#include "building/local_workforce.h"
#include "building/temple.h"
#include "building/water_access_runtime.h"

#include "core/crash_context.h"

#include "building/granary.h"
#include "building/monument.h"
#include "building/properties.h"
#include "building/warehouse.h"
#include "city/buildings.h"
#include "city/data_private.h"
#include "city/population.h"
#include "core/calc.h"
#include "core/config.h"
#include "core/random.h"
#include "figure/movement.h"
#include "game/animation.h"
#include "game/resource.h"
#include "game/time.h"
#include "core/log.h"

#include <cstdio>
#include <cstdint>
#include <string>

namespace {

int figure_belongs_to_building(const Figure *f, const Building &building)
{
    return f && f->building.id() == building.id();
}

void attach_figure_to_building(Figure *f, const Building &building)
{
    if (f) {
        f->building = building;
    }
}

void send_supplier_to_storage_destination(Figure *supplier, const Building &destination)
{
    if (!supplier) {
        return;
    }
    if (supplier->destination_building.id()) {
        supplier->last_destination_id = supplier->destination_building.id();
    }

    supplier->destination_building = destination;

    map_point road;
    int destination_found = 0;
    if (!destination.type) {
        return;
    }
    const building_type_registry_impl::BuildingType &destination_type = *destination.type;
    if (destination_type.is_warehouse()) {
        if (map_has_road_access_warehouse(destination.x(), destination.y(), &road)) {
            destination_found = 1;
        }
    } else if (destination_type.is_granary()) {
        if (map_has_road_access_granary(destination.x(), destination.y(), &road)) {
            destination_found = 1;
        }
    } else if (destination_type.is_grand_temple_venus()) {
        if (map_has_road_access(destination.x(), destination.y(), destination.size(), &road)) {
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

} // namespace

int building_runtime::worker_percentage() const
{
    const int required_workers = type().required_workers();
    if (required_workers <= 0) {
        // Houses and other zero-labor spawn owners still need their XML
        // delay bands to fire; treat them as fully staffed for delay purposes.
        return 100;
    }
    return calc_percentage(building().worker_count(), required_workers);
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
    if (building_local_workforce::is_workforce_building(building())) {
        if (!building().worker_count()) {
            record().show_on_problem_overlay = 2;
        }
        return;
    }
    if (building().labor_access_score() <= 0) {
        record().show_on_problem_overlay = 2;
    }
}

void building_runtime::generate_labor_seeker(int x, int y)
{
    if (city_population() <= 0) {
        return;
    }
    if (config_get(CONFIG_GP_CH_GLOBAL_LABOUR)) {
        if (building().distance_from_entry()) {
            record().houses_covered = 100;
        } else {
            record().houses_covered = 0;
        }
        return;
    }
    if (record().figure_id2) {
        Building current = building();
        Figure *existing = Figure::get(record().figure_id2);
        if (!existing->state || existing->type != FIGURE_LABOR_SEEKER || !figure_belongs_to_building(existing, current)) {
            record().figure_id2 = 0;
        }
        return;
    }

    Figure *labor_seeker = Figure::create(FIGURE_LABOR_SEEKER, x, y, DIR_0_TOP);
    labor_seeker->action_state = FIGURE_ACTION_125_ROAMING;
    attach_figure_to_building(labor_seeker, building());
    record().figure_id2 = labor_seeker->id();
    figure_movement_init_roaming(labor_seeker);
}

void building_runtime::spawn_labor_seeker(int x, int y, int min_houses)
{
    if (config_get(CONFIG_GP_CH_GLOBAL_LABOUR)) {
        if (building().distance_from_entry()) {
            record().houses_covered = 2 * min_houses;
        } else {
            record().houses_covered = 0;
        }
    } else if (building().labor_access_score() <= min_houses) {
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
        const float workforce_access = building().labor_access_score();
        Building current = building();
        if (workforce_access < trigger_workers) {
            if (!building_local_workforce::spawn_acquisition(current, &road) && workforce_access > 0) {
                building_local_workforce::spawn_validation(current, &road);
            }
        } else {
            building_local_workforce::spawn_validation(current, &road);
        }
        return;
    }

    switch (labor_policy.method) {
        case building_type_registry_impl::LaborSeekerMethod::HousesSpawnIfBelow:
        case building_type_registry_impl::LaborSeekerMethod::Workforce:
            spawn_labor_seeker(road.x, road.y, labor_policy.amount);
            break;
        case building_type_registry_impl::LaborSeekerMethod::HousesGenerateIfBelow:
            if (building().labor_access_score() <= labor_policy.amount) {
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
    if (!building().has_primary_figure()) {
        return 0;
    }
    Figure *existing = Figure::get(record().figure_id);
    Building current = building();
    if (existing->state && figure_belongs_to_building(existing, current) && existing->type == type) {
        return 1;
    }
    record().figure_id = 0;
    return 0;
}

int building_runtime::has_figure_of_any(const std::vector<figure_type> &types)
{
    if (!building().has_primary_figure()) {
        return 0;
    }

    // Group guards reason about one tracked slot. Do not call the single-type
    // helper repeatedly here because it clears the slot when that one type does
    // not match, which breaks alternates that intentionally share the slot.
    Figure *existing = Figure::get(record().figure_id);
    Building current = building();
    if (!existing || !existing->state || !figure_belongs_to_building(existing, current)) {
        record().figure_id = 0;
        return 0;
    }

    // Mixed entertainment venues share one legacy slot for alternate walker
    // types, such as amphitheater actors and gladiators.
    for (figure_type type : types) {
        if (existing->type == type) {
            return 1;
        }
    }
    record().figure_id = 0;
    return 0;
}

void building_runtime::run_labor_phase_if_defined(const map_point &road)
{
    if (definition() && type().has_labor()) {
        run_labor_phase(type().labor(), road);
    }
}

unsigned int *building_runtime::figure_slot_storage(building_type_registry_impl::FigureSlot slot)
{
    switch (slot) {
        case building_type_registry_impl::FigureSlot::Primary:
            return &record().figure_id;
        case building_type_registry_impl::FigureSlot::Secondary:
            return &record().figure_id2;
        case building_type_registry_impl::FigureSlot::Quaternary:
            return &record().figure_id4;
        case building_type_registry_impl::FigureSlot::None:
        default:
            return nullptr;
    }
}

unsigned int building_runtime::find_live_owned_figure(figure_type primary_type, figure_type secondary_type) const
{
    // Compatibility scan for figures that predate XML-owned slots, or that
    // lost their tracked slot through legacy cleanup. Spawns identify ownership
    // by building relation, so a live owned walker can safely rehydrate the slot.
    Building current = building();
    for (unsigned int i = 1; i < Figure::count(); i++) {
        Figure *existing = Figure::get(i);
        if (!existing || !existing->state || !figure_belongs_to_building(existing, current)) {
            continue;
        }
        if (existing->type == primary_type || (secondary_type != FIGURE_NONE && existing->type == secondary_type)) {
            return existing->id();
        }
    }
    return 0;
}

int building_runtime::slot_has_live_figure(
    building_type_registry_impl::FigureSlot slot,
    figure_type primary_type,
    figure_type secondary_type)
{
    // XML spawn slots are backed by legacy figure_id fields. Before declaring
    // the slot free, adopt any already-live owned figure so old saves and older
    // hardcoded creation paths do not double-spawn the same walker role.
    unsigned int *slot_value = figure_slot_storage(slot);
    if (!slot_value) {
        return 0;
    }
    if (*slot_value <= 0) {
        // Residential walkers created before BuildingType-owned slots did not
        // populate the house slot. Adopt one live owned walker to avoid a
        // duplicate during the save compatibility window.
        *slot_value = find_live_owned_figure(primary_type, secondary_type);
        return *slot_value > 0;
    }

    Figure *existing = Figure::get(*slot_value);
    Building current = building();
    if (!existing || !existing->state || !figure_belongs_to_building(existing, current)) {
        *slot_value = 0;
        *slot_value = find_live_owned_figure(primary_type, secondary_type);
        return *slot_value > 0;
    }
    if (existing->type != primary_type && existing->type != secondary_type) {
        *slot_value = find_live_owned_figure(primary_type, secondary_type);
        if (*slot_value > 0) {
            return 1;
        }
        *slot_value = 0;
        return 0;
    }
    return 1;
}

int building_runtime::spawn_caravanserai_supplier(const map_point &road)
{
    if (slot_has_live_figure(
            building_type_registry_impl::FigureSlot::Primary,
            FIGURE_CARAVANSERAI_SUPPLIER,
            FIGURE_LABOR_SEEKER)) {
        return 0;
    }

    Building destination = building_caravanserai_get_storage_destination(building());
    if (!destination.id()) {
        return 0;
    }

    Figure *supplier = Figure::create(FIGURE_CARAVANSERAI_SUPPLIER, road.x, road.y, DIR_0_TOP);
    if (!supplier) {
        return 0;
    }

    attach_figure_to_building(supplier, building());
    supplier->collecting_item_id = record().data.market.fetch_inventory_id;
    building().set_primary_figure_id(supplier->id());
    send_supplier_to_storage_destination(supplier, destination);
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

    Building destination = building_lighthouse_get_storage_destination(building());
    if (!destination.id()) {
        return 0;
    }

    Figure *supplier = Figure::create(FIGURE_LIGHTHOUSE_SUPPLIER, road.x, road.y, DIR_0_TOP);
    if (!supplier) {
        return 0;
    }

    attach_figure_to_building(supplier, building());
    supplier->collecting_item_id = resource_timber();
    building().set_primary_figure_id(supplier->id());
    send_supplier_to_storage_destination(supplier, destination);
    return 1;
}

int building_runtime::spawn_temple_supplier(const map_point &road)
{
    if (!type().is_ceres_temple() && !type().is_venus_temple()) {
        return 0;
    }
    if (type().is_ceres_temple() &&
        !building_monument_gt_module_is_active(CERES_MODULE_2_DISTRIBUTE_FOOD)) {
        return 0;
    }
    if (type().is_venus_temple() &&
        !building_monument_gt_module_is_active(VENUS_MODULE_1_DISTRIBUTE_WINE)) {
        return 0;
    }
    if (slot_has_live_figure(
            building_type_registry_impl::FigureSlot::Secondary,
            FIGURE_PRIEST_SUPPLIER,
            FIGURE_LABOR_SEEKER)) {
        return 0;
    }

    Building current = building();
    if (const building_type_registry_impl::Distribution *distribution = current.type->distribution()) {
        distribution->update_demands(current);
    }
    Building destination = building_temple_get_storage_destination(current);
    if (!destination.id()) {
        return 0;
    }

    Figure *supplier = Figure::create(FIGURE_PRIEST_SUPPLIER, road.x, road.y, DIR_0_TOP);
    if (!supplier) {
        return 0;
    }
    attach_figure_to_building(supplier, building());
    supplier->collecting_item_id = record().data.market.fetch_inventory_id;
    assign_figure_slot(building_type_registry_impl::FigureSlot::Secondary, supplier->id());
    send_supplier_to_storage_destination(supplier, destination);
    return 1;
}

int building_runtime::spawn_temple_destination_priest(const map_point &road)
{
    if (type().is_pantheon() ||
        !building_monument_pantheon_module_is_active(PANTHEON_MODULE_1_DESTINATION_PRIESTS) ||
        slot_has_live_figure(building_type_registry_impl::FigureSlot::Quaternary, FIGURE_PRIEST)) {
        return 0;
    }

    building_type pantheon_type = building_type_registry_impl::runtime_id_from_text("pantheon");
    if (!building_monument_working(pantheon_type)) {
        return 0;
    }
    Building pantheon = Building::first_of_type(pantheon_type);
    if (!pantheon.id()) {
        return 0;
    }

    Figure *priest = Figure::create(FIGURE_PRIEST, road.x, road.y, DIR_4_BOTTOM);
    if (!priest) {
        return 0;
    }
    assign_figure_slot(building_type_registry_impl::FigureSlot::Quaternary, priest->id());
    priest->destination_building = pantheon;
    attach_figure_to_building(priest, building());
    priest->action_state = FIGURE_ACTION_212_DESTINATION_PRIEST_CREATED;
    return 1;
}

int building_runtime::spawn_temple_mars_mess_hall_priest(const map_point &road)
{
    if (!type().is_mars_temple() ||
        !building_monument_gt_module_is_active(MARS_MODULE_1_MESS_HALL)) {
        return 0;
    }

    Building mess_hall = Building::first_of_type(building_type_registry_impl::runtime_id_from_text("mess_hall"));
    if (!mess_hall.id() || !mess_hall.is_in_use() || !mess_hall.type || !mess_hall.type->is_mess_hall()) {
        return 0;
    }

    if (building().has_secondary_figure()) {
        Figure *existing = Figure::get(record().figure_id2);
        if (existing && existing->state == FIGURE_STATE_ALIVE && existing->type == FIGURE_PRIEST) {
            return 0;
        }
        if (!existing || existing->state != FIGURE_STATE_ALIVE) {
            record().figure_id2 = 0;
        }
    }

    int food_to_deliver = building_temple_mars_food_to_deliver(building(), mess_hall);
    if (food_to_deliver < 0) {
        return 0;
    }

    Figure *priest = Figure::create(FIGURE_PRIEST, road.x, road.y, DIR_4_BOTTOM);
    if (!priest) {
        return 0;
    }
    priest->collecting_item_id = static_cast<unsigned char>(food_to_deliver);
    assign_figure_slot(building_type_registry_impl::FigureSlot::Secondary, priest->id());
    priest->destination_building = mess_hall;
    attach_figure_to_building(priest, building());
    priest->action_state = FIGURE_ACTION_214_DESTINATION_MARS_PRIEST_CREATED;
    return 1;
}

int building_runtime::spawn_temple_neptune_chariot(const map_point &road)
{
    if (!type().is_neptune_temple() ||
        !building_monument_gt_module_is_active(NEPTUNE_MODULE_1_HIPPODROME_ACCESS) ||
        building().has_secondary_figure()) {
        return 0;
    }

    record().days_since_offering++;
    if (record().days_since_offering <= 1) {
        return 0;
    }

    Building current = building();
    Figure *charioteer = figure_runtime_create_profiled(
        FIGURE_CHARIOTEER,
        road.x,
        road.y,
        DIR_0_TOP,
        current,
        "venue_seeker");
    if (!charioteer) {
        charioteer = Figure::create(FIGURE_CHARIOTEER, road.x, road.y, DIR_0_TOP);
        if (!charioteer) {
            return 0;
        }
        charioteer->action_state = FIGURE_ACTION_90_ENTERTAINER_AT_SCHOOL_CREATED;
        attach_figure_to_building(charioteer, current);
    }
    assign_figure_slot(building_type_registry_impl::FigureSlot::Secondary, charioteer->id());
    record().days_since_offering = 0;
    return 1;
}

int building_runtime::spawn_grand_temple_mars_recruit(const map_point &road)
{
    if (!type().is_grand_temple_mars()) {
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

    record().figure_spawn_delay++;
    if (record().figure_spawn_delay <= spawn_delay) {
        return 0;
    }
    record().figure_spawn_delay = 0;

    Barracks barracks(record());
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
            break;
    }

    if (!has_figure_of_type(FIGURE_PRIEST)) {
        Figure *priest = Figure::create(FIGURE_PRIEST, road.x, road.y, DIR_0_TOP);
        if (priest) {
            priest->action_state = FIGURE_ACTION_125_ROAMING;
            attach_figure_to_building(priest, building());
            building().set_primary_figure_id(priest->id());
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

    run_labor_phase_if_defined(road);
    if (has_figure_of_type(FIGURE_WORK_CAMP_ARCHITECT)) {
        return;
    }

    int spawn_delay = default_spawn_delay();
    if (!spawn_delay) {
        return;
    }

    record().figure_spawn_delay++;
    if (record().figure_spawn_delay <= spawn_delay) {
        return;
    }

    record().figure_spawn_delay = 0;
    if (!building_monument_get_monument(road.x, road.y, RESOURCE_NONE, building().road_network_id(), 0)) {
        return;
    }

    Figure *architect = Figure::create(FIGURE_WORK_CAMP_ARCHITECT, road.x, road.y, DIR_4_BOTTOM);
    if (!architect) {
        return;
    }
    architect->action_state = FIGURE_ACTION_206_WORK_CAMP_ARCHITECT_CREATED;
    attach_figure_to_building(architect, building());
    building().set_primary_figure_id(architect->id());
}

void building_runtime::spawn_caravanserai()
{
    check_labor_problem();

    map_point road;
    if (!resolve_road_access(building_type_registry_impl::RoadAccessMode::Normal, &road)) {
        return;
    }

    run_labor_phase_if_defined(road);

    int spawn_delay = default_spawn_delay();
    if (!spawn_delay) {
        return;
    }

    record().figure_spawn_delay++;
    if (record().figure_spawn_delay <= spawn_delay) {
        return;
    }

    record().figure_spawn_delay = 0;
    spawn_caravanserai_supplier(road);
}

void building_runtime::spawn_lighthouse()
{
    check_labor_problem();

    map_point road;
    if (!resolve_road_access(building_type_registry_impl::RoadAccessMode::Normal, &road)) {
        return;
    }

    run_labor_phase_if_defined(road);

    int spawn_delay = default_spawn_delay();
    if (!spawn_delay) {
        return;
    }

    record().figure_spawn_delay++;
    if (record().figure_spawn_delay <= spawn_delay) {
        return;
    }

    record().figure_spawn_delay = 0;
    spawn_lighthouse_supplier(road);
}

void building_runtime::spawn_watchtower()
{
    check_labor_problem();
    if (building().has_primary_figure() || building().has_secondary_figure()) {
        return;
    }

    map_point road;
    if (!resolve_road_access(building_type_registry_impl::RoadAccessMode::Normal, &road)) {
        return;
    }

    run_labor_phase_if_defined(road);
    if (building().has_secondary_figure()) {
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

    record().figure_spawn_delay++;
    if (record().figure_spawn_delay <= spawn_delay) {
        return;
    }

    record().figure_spawn_delay = 0;
    Figure *primary_watchman = Figure::create(FIGURE_WATCHMAN, road.x, road.y, DIR_0_TOP);
    if (!primary_watchman) {
        return;
    }
    primary_watchman->action_state = FIGURE_ACTION_220_WATCHMAN_PATROL_INITIATE;
    attach_figure_to_building(primary_watchman, building());
    building().set_primary_figure_id(primary_watchman->id());

    Figure *secondary_watchman = Figure::create(FIGURE_WATCHMAN, road.x, road.y, DIR_0_TOP);
    if (!secondary_watchman) {
        return;
    }
    secondary_watchman->action_state = FIGURE_ACTION_220_WATCHMAN_PATROL_INITIATE;
    attach_figure_to_building(secondary_watchman, building());
    assign_figure_slot(building_type_registry_impl::FigureSlot::Secondary, secondary_watchman->id());
}

void building_runtime::spawn_armoury()
{
    check_labor_problem();

    map_point road;
    if (!resolve_road_access(building_type_registry_impl::RoadAccessMode::Normal, &road)) {
        return;
    }

    run_labor_phase_if_defined(road);

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

    record().figure_spawn_delay++;
    if (record().figure_spawn_delay <= spawn_delay) {
        return;
    }

    record().figure_spawn_delay = 0;
    if (!Armoury(record()).is_needed()) {
        return;
    }

    Figure *warehouseman = Figure::create(FIGURE_WAREHOUSEMAN, road.x, road.y, DIR_4_BOTTOM);
    if (!warehouseman) {
        return;
    }
    warehouseman->action_state = FIGURE_ACTION_50_WAREHOUSEMAN_CREATED;
    warehouseman->collecting_item_id = resource_weapons();
    attach_figure_to_building(warehouseman, building());
    assign_figure_slot(target_slot, warehouseman->id());
}

int building_runtime::resolve_road_access(building_type_registry_impl::RoadAccessMode mode, map_point *road) const
{
    switch (mode) {
        case building_type_registry_impl::RoadAccessMode::Normal:
            return building().has_road_access(road);
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
            return building().entertainment_days1() > 0;
        case building_type_registry_impl::SpawnCondition::Days1NotPositive:
            return building().entertainment_days1() <= 0;
        case building_type_registry_impl::SpawnCondition::Days2Positive:
            return building().entertainment_days2() > 0;
        case building_type_registry_impl::SpawnCondition::Days1OrDays2Positive:
            return building().entertainment_days1() > 0 || building().entertainment_days2() > 0;
        default:
            return 0;
    }
}

int building_runtime::evaluate_spawn_chance(const building_type_registry_impl::SpawnPolicy &policy)
{
    // Chance gates are expressed as parts per million so XML can encode both
    // small daily probabilities and deterministic gates without float drift.
    // Bands use the first matching minimum value in author order; divisors turn
    // a live source value such as unemployed workers into source / divisor.
    int chance_per_million = policy.chance_per_million;
    int source_value = 0;
    switch (policy.chance_source) {
        case building_type_registry_impl::SpawnChanceSource::CityUnemploymentPercent:
            source_value = city_labor_unemployment_percentage();
            break;
        case building_type_registry_impl::SpawnChanceSource::HouseUnemployedWorkers: {
            Building current = building();
            source_value = building_local_workforce::house_available_workers(current);
            break;
        }
        case building_type_registry_impl::SpawnChanceSource::None:
        default:
            break;
    }

    if (policy.chance_divisor > 0) {
        const long long scaled = static_cast<long long>(source_value) * 1000000;
        chance_per_million = static_cast<int>((scaled + policy.chance_divisor / 2) / policy.chance_divisor);
    } else if (!policy.chance_bands.empty()) {
        chance_per_million = 0;
        for (const building_type_registry_impl::ChanceBand &band : policy.chance_bands) {
            if (source_value >= band.min_value) {
                chance_per_million = band.chance_per_million;
                break;
            }
        }
    }

    if (chance_per_million < 0) {
        return 1;
    }
    if (chance_per_million <= 0) {
        return 0;
    }
    if (chance_per_million >= 1000000) {
        return 1;
    }

    random_generate_next();
    const int roll = static_cast<int>(
        (((static_cast<unsigned int>(random_short()) & 0x7fff) << 15) |
            (static_cast<unsigned int>(random_short_alt()) & 0x7fff)) % 1000000);
    return roll < chance_per_million;
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
        return record().figure_spawn_delay;
    }
    if (spawn_delay_counters_.size() <= policy_index) {
        return 0;
    }
    return spawn_delay_counters_[policy_index];
}

void building_runtime::set_spawn_delay_counter(size_t policy_index, unsigned char value)
{
    if (policy_index == 0) {
        record().figure_spawn_delay = value;
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
            record().figure_id = figure_id;
            break;
        case building_type_registry_impl::FigureSlot::Secondary:
            record().figure_id2 = figure_id;
            break;
        case building_type_registry_impl::FigureSlot::Quaternary:
            record().figure_id4 = figure_id;
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
        Building current = building();
        Figure *spawned = policy.profile.empty() ?
            Figure::create(policy.spawn_figure, road.x, road.y, static_cast<direction_type>(policy.spawn_direction)) :
            figure_runtime_create_profiled(
                policy.spawn_figure,
                road.x,
                road.y,
                static_cast<direction_type>(policy.spawn_direction),
                current,
                policy.profile.c_str());
        if (!spawned) {
            continue;
        }
        if (policy.profile.empty()) {
            spawned->action_state = policy.action_state;
            attach_figure_to_building(spawned, current);
        }
        // A multi-spawn policy still only owns one legacy tracked slot today; later spawns remain untracked for now.
        if (!spawned_any) {
            assign_figure_slot(policy.figure_slot, spawned->id());
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

    if (policy.mark_problem_if_no_water && !building().has_water_access()) {
        record().show_on_problem_overlay = 2;
    }

    if (policy.require_water_access && !building().has_water_access()) {
        return 0;
    }

    if (!evaluate_spawn_chance(policy)) {
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
    // A spawn group may exist only to run residential or ambient spawns. Labor
    // overlays and labor-seeker checks belong only to definitions with explicit
    // labor data, and only once per building-generation pass.
    const int has_labor_phase = run_labor && definition() && type().has_labor();
    if (has_labor_phase) {
        check_labor_problem();
    }
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

    if (has_labor_phase) {
        run_labor_phase(type().labor(), road);
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
    if (!building().id() || !definition() || !building().is_in_use()) {
        return;
    }

    refresh_runtime_state();

    const std::vector<building_type_registry_impl::SpawnDelayGroup> &spawn_groups = type().spawn_groups();
    if (spawn_groups.empty()) {
        if (type().has_graphic()) {
            set_building_graphic();
        }

        if (type().is_architect_guild()) {
            spawn_architect_guild();
        } else if (type().is_caravanserai()) {
            spawn_caravanserai();
        } else if (type().is_lighthouse()) {
            spawn_lighthouse();
        } else if (type().is_watchtower()) {
            spawn_watchtower();
        } else if (type().is_armoury()) {
            spawn_armoury();
        }
        return;
    }

    // Groups own the shared delay/guard phase, then policies inside them can either cooperate or block one another.
    for (size_t i = 0; i < spawn_groups.size(); i++) {
        const building_type_registry_impl::SpawnDelayGroup &group = spawn_groups[i];
        if (!group.policies.empty()) {
            spawn_service_roamer_group(group, i, i == 0);
        }
    }
}
