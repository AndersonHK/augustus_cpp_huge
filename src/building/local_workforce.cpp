#include "building/building.h"
#include "building/count.h"
#include "city/labor.h"
#include "figure/action.h"

#include "building/local_workforce.h"

#include "building/building_type_registry_internal.h"
#include "building/housing_type.h"
#include "building/monument.h"
#include "figure/figure.h"
#include "figure/figure_runtime_api.h"
#include "figure/figure_type_registry_internal.h"
#include "figure/route.h"

#include "building/building_record.h"
#include "city/population.h"
#include "core/config.h"
#include "game/time.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace {

constexpr uint32_t kSaveFormatVersion = 1;
constexpr int kValidationDelayDays = 16;
constexpr int kDefaultLaborSeekerMaxRoamLength = 384;
constexpr unsigned char kLaborSeekerTripAcquire = 0;
constexpr unsigned char kLaborSeekerTripValidate = 1;
constexpr const char *kLaborSeekerAcquireProfile = "acquisition";
constexpr const char *kLaborSeekerValidateProfile = "validation";

struct WorkforceAllocation {
    unsigned int workplace_id = 0;
    unsigned int house_id = 0;
    int workers = 0;
};

std::vector<WorkforceAllocation> g_allocations;
int g_preserve_allocations_on_next_city_initialize = 0;

int is_live_building(const building *b)
{
    return b && b->id && b->state == BUILDING_STATE_IN_USE;
}

int is_live_building(const Building &building)
{
    return building.id() && building.is_in_use();
}

const building_type_registry_impl::LaborSeekerPolicy *labor_policy_for(const Building &building)
{
    if (!is_live_building(building)) {
        return nullptr;
    }
    const building_type_registry_impl::BuildingType *definition = building.type;
    if (!definition || !definition->has_labor() || !definition->labor().has_seeker_policy()) {
        return nullptr;
    }
    return &definition->labor().seeker_policy();
}

int uses_workforce(const Building &building)
{
    const building_type_registry_impl::LaborSeekerPolicy *policy = labor_policy_for(building);
    return policy &&
        policy->method == building_type_registry_impl::LaborSeekerMethod::Workforce;
}

int uses_active_workforce(const Building &building)
{
    return uses_workforce(building) &&
        !config_get(CONFIG_GP_CH_GLOBAL_LABOUR);
}

int required_workers(const Building &building)
{
    if (!is_live_building(building)) {
        return 0;
    }

    if (!building.type) {
        return 0;
    }
    int workers = building.employment_required_workers();
    if (building.type->is_fountain() && building_monument_working_grand_temple_for_god(GOD_NEPTUNE)) {
        workers /= 2;
        if (workers == 0) {
            workers = 1;
        }
    }
    return workers;
}

int labor_seeker_max_roam_length()
{
    const figure_type_registry_impl::FigureTypeProfile *profile =
        figure_type_registry_impl::profile_for(FIGURE_LABOR_SEEKER, kLaborSeekerAcquireProfile);
    if (!profile) {
        profile = figure_type_registry_impl::default_profile_for(FIGURE_LABOR_SEEKER);
    }
    if (!profile) {
        return kDefaultLaborSeekerMaxRoamLength;
    }
    const int max_roam_length = profile->movement_profile().max_roam_length;
    return max_roam_length > 0 ? max_roam_length : kDefaultLaborSeekerMaxRoamLength;
}

int assigned_workers_for_house(unsigned int house_id)
{
    int assigned = 0;
    for (const WorkforceAllocation &allocation : g_allocations) {
        if (allocation.house_id == house_id) {
            assigned += allocation.workers;
        }
    }
    return assigned;
}

int assigned_workers_for_workplace(unsigned int workplace_id)
{
    int assigned = 0;
    for (const WorkforceAllocation &allocation : g_allocations) {
        if (allocation.workplace_id == workplace_id) {
            assigned += allocation.workers;
        }
    }
    return assigned;
}

int possible_workers_for_house(const Building &house)
{
    const building *record = building_get(house.id());
    if (!record || !is_live_building(house) || !house.has_house_size() ||
        record->house_population <= 0 || city_population() <= 0) {
        return 0;
    }
    const building_type_registry_impl::HousingType *housing = house.type ? house.type->housing_type() : nullptr;
    if (!housing || housing->resident_class() != building_type_registry_impl::HousingResidentClass::Plebeian) {
        return 0;
    }

    const int workers_available = city_labor_workers_available();
    if (workers_available <= 0) {
        return 0;
    }

    const int64_t numerator = static_cast<int64_t>(record->house_population) * workers_available;
    return static_cast<int>((numerator + city_population() - 1) / city_population());
}

void refresh_house_unemployed(Building &house)
{
    building *record = building_get(house.id());
    if (!record || !is_live_building(house) || !house.has_house_size()) {
        return;
    }

    record->local_workforce_assigned = static_cast<short>(assigned_workers_for_house(house.id()));
    const int unemployed = possible_workers_for_house(house) - record->local_workforce_assigned;
    record->local_workforce_unemployed = static_cast<short>(std::max(0, unemployed));
}

void refresh_house_unemployed(building *house)
{
    if (!is_live_building(house) || !house->house_size) {
        return;
    }
    Building house_object(*house);
    refresh_house_unemployed(house_object);
}

int access_workers_for_workplace(const Building &workplace)
{
    const building *record = building_get(workplace.id());
    if (!record || !is_live_building(workplace)) {
        return 0;
    }
    if (!uses_active_workforce(workplace)) {
        return std::max<int>(0, record->houses_covered);
    }
    return std::min(assigned_workers_for_workplace(workplace.id()), required_workers(workplace));
}

void refresh_access_score(Building &workplace)
{
    if (building *record = building_get(workplace.id())) {
        record->labor_access_score = static_cast<float>(access_workers_for_workplace(workplace));
    }
}

void refresh_access_scores()
{
    for (int id = 1; id < building_count(); id++) {
        building *record = building_get(id);
        if (!record) {
            continue;
        }
        Building workplace(*record);
        refresh_access_score(workplace);
    }
}

void add_allocation(unsigned int workplace_id, unsigned int house_id, int workers)
{
    if (workers <= 0) {
        return;
    }

    for (WorkforceAllocation &allocation : g_allocations) {
        if (allocation.workplace_id == workplace_id && allocation.house_id == house_id) {
            allocation.workers += workers;
            return;
        }
    }
    g_allocations.push_back({ workplace_id, house_id, workers });
}

int labor_seeker_slot_is_busy(Building &workplace)
{
    building *record = building_get(workplace.id());
    if (!record || !is_live_building(workplace) || !record->figure_id2) {
        return 0;
    }

    Figure *existing = Figure::get(record->figure_id2);
    if (existing->state && existing->building.id() == workplace.id()) {
        return 1;
    }
    record->figure_id2 = 0;
    return 0;
}

int release_from_record(size_t index, int workers)
{
    if (index >= g_allocations.size() || workers <= 0) {
        return 0;
    }

    WorkforceAllocation &allocation = g_allocations[index];
    const int released = std::min(workers, allocation.workers);
    allocation.workers -= released;
    if (allocation.workers <= 0) {
        g_allocations.erase(g_allocations.begin() + static_cast<std::ptrdiff_t>(index));
    }
    return released;
}

void release_workplace_source(unsigned int workplace_id, unsigned int house_id)
{
    for (size_t i = g_allocations.size(); i > 0; i--) {
        WorkforceAllocation &allocation = g_allocations[i - 1];
        if (allocation.workplace_id == workplace_id && allocation.house_id == house_id) {
            release_from_record(i - 1, allocation.workers);
        }
    }

    building *house = building_get(house_id);
    refresh_house_unemployed(house);
    building *workplace_record = building_get(workplace_id);
    if (is_live_building(workplace_record)) {
        Building workplace(*workplace_record);
        refresh_access_score(workplace);
    }
}

void retire_labor_seeker(Figure *f)
{
    if (!f) {
        return;
    }

    building *workplace = building_get(f->building.id());
    if (is_live_building(workplace) && workplace->figure_id2 == f->id()) {
        workplace->figure_id2 = 0;
    }
    f->state = FIGURE_STATE_DEAD;
}

void trim_workplace_to_required(Building &workplace)
{
    if (!uses_workforce(workplace)) {
        return;
    }

    int excess = assigned_workers_for_workplace(workplace.id()) - required_workers(workplace);
    for (size_t i = g_allocations.size(); i > 0 && excess > 0; i--) {
        const WorkforceAllocation &allocation = g_allocations[i - 1];
        if (allocation.workplace_id != workplace.id()) {
            continue;
        }
        excess -= release_from_record(i - 1, excess);
    }
}

void trim_house_to_possible(Building &house)
{
    if (!is_live_building(house) || !house.has_house_size()) {
        return;
    }

    int excess = assigned_workers_for_house(house.id()) - possible_workers_for_house(house);
    for (size_t i = g_allocations.size(); i > 0 && excess > 0; i--) {
        const WorkforceAllocation &allocation = g_allocations[i - 1];
        if (allocation.house_id != house.id()) {
            continue;
        }
        excess -= release_from_record(i - 1, excess);
    }
    refresh_house_unemployed(house);
}

void trim_house_to_possible(building *house)
{
    if (!is_live_building(house) || !house->house_size) {
        return;
    }
    Building house_object(*house);
    trim_house_to_possible(house_object);
}

void clamp_allocation_table()
{
    for (size_t i = g_allocations.size(); i > 0; i--) {
        const WorkforceAllocation &allocation = g_allocations[i - 1];
        building *workplace_record = building_get(allocation.workplace_id);
        building *house = building_get(allocation.house_id);
        if (!workplace_record || allocation.workers <= 0) {
            g_allocations.erase(g_allocations.begin() + static_cast<std::ptrdiff_t>(i - 1));
            continue;
        }
        Building workplace(*workplace_record);
        if (!uses_workforce(workplace)) {
            g_allocations.erase(g_allocations.begin() + static_cast<std::ptrdiff_t>(i - 1));
        } else if (!is_live_building(house) || !house->house_size) {
            release_from_record(i - 1, allocation.workers);
        }
    }

    for (int id = 1; id < building_count(); id++) {
        building *record = building_get(id);
        if (!record) {
            continue;
        }
        Building workplace(*record);
        trim_workplace_to_required(workplace);
    }
}

void rebuild_counters_from_allocations()
{
    for (int id = 1; id < building_count(); id++) {
        building *b = building_get(id);
        if (!b || !b->id) {
            continue;
        }
        b->local_workforce_assigned = 0;
        b->local_workforce_unemployed = 0;
        b->local_workforce_validation_delay = 0;
    }

    for (int id = 1; id < building_count(); id++) {
        building *house = building_get(id);
        if (house && house->house_size) {
            refresh_house_unemployed(house);
        }
    }
}

int find_nearest_reachable_house_with_unemployed(const map_point *road, map_point *target_road, int max_distance)
{
    if (!road || !target_road || max_distance <= 0) {
        return 0;
    }

    const Route::DistanceQuery route_query =
        Route::DistanceQuery::fromRoad(
            *road,
            PERMISSION_LABOR_SEEKER,
            PERFORMANCE_TRACKER_ROUTE_PURPOSE_LOCAL_WORKFORCE);
    if (!route_query) {
        return 0;
    }

    int best_house_id = 0;
    int best_distance = 0x7fffffff;
    map_point best_road = { 0, 0 };
    for (int id = 1; id < building_count(); id++) {
        building *house = building_get(id);
        if (!is_live_building(house) || !house->house_size || house->house_population <= 0) {
            continue;
        }

        refresh_house_unemployed(house);
        if (house->local_workforce_unemployed <= 0) {
            continue;
        }

        const Route::RoadResult route = route_query.findAccessRoad(*house, 2, max_distance, true);
        if (!route) {
            continue;
        }

        if (route.distance < best_distance) {
            best_distance = route.distance;
            best_house_id = house->id;
            best_road = route.road;
        }
    }

    if (!best_house_id) {
        return 0;
    }
    *target_road = best_road;
    return best_house_id;
}

int remaining_roam_length(const Figure *f)
{
    if (!f) {
        return 0;
    }

    const int max_roam_length = f->max_roam_length > 0 ? f->max_roam_length : labor_seeker_max_roam_length();
    return std::max(0, max_roam_length - f->roam_length);
}

int find_nearest_assigned_source(Building &workplace, const map_point *road, map_point *target_road)
{
    if (!uses_active_workforce(workplace) || !road || !target_road) {
        return 0;
    }

    const Route::DistanceQuery route_query =
        Route::DistanceQuery::fromRoad(
            *road,
            PERMISSION_LABOR_SEEKER,
            PERFORMANCE_TRACKER_ROUTE_PURPOSE_LOCAL_WORKFORCE);
    if (!route_query) {
        return 0;
    }

    const int max_roam_length = labor_seeker_max_roam_length();
    int best_house_id = 0;
    int best_distance = 0x7fffffff;
    map_point best_road = { 0, 0 };
    std::vector<int> house_ids_to_release;
    for (const WorkforceAllocation &allocation : g_allocations) {
        if (allocation.workplace_id != workplace.id() || allocation.workers <= 0) {
            continue;
        }

        building *house = building_get(allocation.house_id);
        if (!is_live_building(house) || !house->house_size) {
            house_ids_to_release.push_back(allocation.house_id);
            continue;
        }

        const Route::RoadResult route = route_query.findAccessRoad(*house, 2, max_roam_length, true);
        if (!route) {
            house_ids_to_release.push_back(allocation.house_id);
            continue;
        }

        if (route.distance < best_distance) {
            best_distance = route.distance;
            best_house_id = house->id;
            best_road = route.road;
        }
    }

    for (int house_id : house_ids_to_release) {
        release_workplace_source(workplace.id(), house_id);
    }

    if (!best_house_id) {
        return 0;
    }
    *target_road = best_road;
    return best_house_id;
}

int prepare_labor_seeker_target(Figure *f)
{
    if (!f) {
        return 0;
    }

    Building workplace = f->building;
    building *workplace_record = building_get(workplace.id());
    if (!uses_active_workforce(workplace) || !workplace_record || workplace_record->figure_id2 != f->id()) {
        return 0;
    }
    if (f->destination_building.id()) {
        Building house = f->destination_building;
        if (is_live_building(house) && house.has_house_size()) {
            const map_point source_road = { f->x, f->y };
            const map_point target_road = { f->destination_x, f->destination_y };
            RoutePolicy reachability_policy = RoutePolicy::fromKind(RoutePolicyKind::CitizenRoadGarden);
            reachability_policy.permission = PERMISSION_LABOR_SEEKER;
            Route::Request reachability = Route::Request::between(
                source_road,
                target_road,
                reachability_policy,
                PERFORMANCE_TRACKER_ROUTE_PURPOSE_LOCAL_WORKFORCE);
            reachability.max_tiles = remaining_roam_length(f);
            reachability.require_same_road_network = true;
            const int target_is_reachable = Route::Planner::canReach(reachability);
            if (!target_is_reachable) {
                if (f->collecting_item_id == kLaborSeekerTripValidate) {
                    release_workplace_source(workplace.id(), house.id());
                }
                f->destination_building = Building(nullptr);
            } else {
                if (f->collecting_item_id == kLaborSeekerTripValidate) {
                    return 1;
                }
                refresh_house_unemployed(house);
                building *house_record = building_get(house.id());
                if (house_record && house_record->local_workforce_unemployed > 0) {
                    return 1;
                }
            }
        } else if (f->collecting_item_id == kLaborSeekerTripValidate) {
            release_workplace_source(workplace.id(), f->destination_building.id());
        }
        f->destination_building = Building(nullptr);
    }

    const int max_distance = remaining_roam_length(f);
    if (max_distance <= 0) {
        return 0;
    }

    map_point source_road = { f->x, f->y };
    map_point target_road = { 0, 0 };
    const int house_id = find_nearest_reachable_house_with_unemployed(&source_road, &target_road, max_distance);
    if (!house_id) {
        return 0;
    }

    f->destination_building = Building(building_get(house_id));
    f->destination_x = static_cast<unsigned char>(target_road.x);
    f->destination_y = static_cast<unsigned char>(target_road.y);
    Route::remove(f);
    return 1;
}

int create_labor_seeker(
    Building &workplace,
    const map_point *source_road,
    const map_point *target_road,
    int house_id,
    unsigned char trip_type)
{
    if (!uses_active_workforce(workplace) || !source_road) {
        return 0;
    }
    if (labor_seeker_slot_is_busy(workplace)) {
        return 0;
    }

    const char *profile_id = trip_type == kLaborSeekerTripValidate ?
        kLaborSeekerValidateProfile :
        kLaborSeekerAcquireProfile;
    Figure *labor_seeker = figure_runtime_create_profiled(
        FIGURE_LABOR_SEEKER,
        source_road->x,
        source_road->y,
        DIR_0_TOP,
        workplace,
        profile_id);
    if (!labor_seeker || !labor_seeker->id()) {
        return 0;
    }

    building *record = building_get(workplace.id());
    if (!record) {
        labor_seeker->state = FIGURE_STATE_DEAD;
        return 0;
    }
    // The trip flag is retained for save compatibility; the profile owns the behavior contract.
    labor_seeker->destination_building = house_id > 0 ? Building(building_get(house_id)) : Building(nullptr);
    labor_seeker->destination_x = static_cast<unsigned char>(target_road ? target_road->x : source_road->x);
    labor_seeker->destination_y = static_cast<unsigned char>(target_road ? target_road->y : source_road->y);
    labor_seeker->collecting_item_id = trip_type;
    record->figure_id2 = labor_seeker->id();
    return 1;
}

int retarget_labor_seeker_to_unemployed(Figure *f)
{
    if (!f) {
        return 0;
    }

    Building workplace = f->building;
    building *workplace_record = building_get(workplace.id());
    if (!uses_active_workforce(workplace) || !workplace_record || workplace_record->figure_id2 != f->id()) {
        return 0;
    }
    if (access_workers_for_workplace(workplace) >= required_workers(workplace)) {
        return 0;
    }

    const int max_distance = remaining_roam_length(f);
    if (max_distance <= 0) {
        return 0;
    }

    map_point source_road = { f->x, f->y };
    map_point target_road = { 0, 0 };
    const int house_id = find_nearest_reachable_house_with_unemployed(&source_road, &target_road, max_distance);
    if (!house_id) {
        return 0;
    }

    f->action_state = FIGURE_ACTION_125_ROAMING;
    f->destination_building = Building(building_get(house_id));
    f->destination_x = static_cast<unsigned char>(target_road.x);
    f->destination_y = static_cast<unsigned char>(target_road.y);
    f->collecting_item_id = kLaborSeekerTripAcquire;
    Route::remove(f);
    return 1;
}

void handle_arrival(Figure *f)
{
    Building workplace = f->building;
    Building house = f->destination_building;
    building *house_record = building_get(house.id());
    if (!uses_active_workforce(workplace)) {
        retire_labor_seeker(f);
        return;
    }
    if (!is_live_building(house) || !house.has_house_size()) {
        if (retarget_labor_seeker_to_unemployed(f)) {
            return;
        }
        retire_labor_seeker(f);
        return;
    }

    if (f->collecting_item_id == kLaborSeekerTripValidate) {
        trim_house_to_possible(house);
        if (retarget_labor_seeker_to_unemployed(f)) {
            return;
        }
        retire_labor_seeker(f);
        return;
    }

    refresh_house_unemployed(house);
    const int needed = required_workers(workplace) - access_workers_for_workplace(workplace);
    const int assigned = std::min<int>(std::max(0, needed), house_record ? house_record->local_workforce_unemployed : 0);
    if (assigned > 0) {
        add_allocation(workplace.id(), house.id(), assigned);
        refresh_house_unemployed(house);
    }
    if (retarget_labor_seeker_to_unemployed(f)) {
        return;
    }
    retire_labor_seeker(f);
}

void kill_labor_seekers_for_building(unsigned int building_id)
{
    if (!building_id) {
        return;
    }

    for (unsigned int id = 1; id < Figure::count(); id++) {
        Figure *f = Figure::get(id);
        if (!f || f->state != FIGURE_STATE_ALIVE || f->type != FIGURE_LABOR_SEEKER) {
            continue;
        }
        if (f->building.id() != building_id && f->destination_building.id() != building_id) {
            continue;
        }
        building *workplace = building_get(f->building.id());
        if (is_live_building(workplace) && workplace->figure_id2 == f->id()) {
            workplace->figure_id2 = 0;
        }
        f->state = FIGURE_STATE_DEAD;
    }
}

void remove_allocations_for_building(unsigned int building_id)
{
    if (!building_id) {
        return;
    }

    for (size_t i = g_allocations.size(); i > 0; i--) {
        const WorkforceAllocation &allocation = g_allocations[i - 1];
        if (allocation.workplace_id == building_id) {
            const unsigned int house_id = allocation.house_id;
            g_allocations.erase(g_allocations.begin() + static_cast<std::ptrdiff_t>(i - 1));
            building *house = building_get(house_id);
            refresh_house_unemployed(house);
        } else if (allocation.house_id == building_id) {
            building *workplace = building_get(allocation.workplace_id);
            release_from_record(i - 1, allocation.workers);
        }
    }
}

void merge_duplicate_allocations()
{
    for (size_t i = 0; i < g_allocations.size(); i++) {
        WorkforceAllocation &current = g_allocations[i];
        if (current.workers <= 0) {
            continue;
        }
        for (size_t j = g_allocations.size(); j > i + 1; j--) {
            WorkforceAllocation &other = g_allocations[j - 1];
            if (other.workplace_id == current.workplace_id && other.house_id == current.house_id) {
                current.workers += other.workers;
                g_allocations.erase(g_allocations.begin() + static_cast<std::ptrdiff_t>(j - 1));
            }
        }
    }
}

} // namespace

void building_local_workforce_clear(void)
{
    g_allocations.clear();
    g_preserve_allocations_on_next_city_initialize = 0;
}

void building_local_workforce_initialize_city(void)
{
    if (!g_preserve_allocations_on_next_city_initialize) {
        g_allocations.clear();
    }
    g_preserve_allocations_on_next_city_initialize = 0;
    clamp_allocation_table();
    rebuild_counters_from_allocations();
    refresh_access_scores();
}

void building_local_workforce_save_state(buffer *buf)
{
    if (!buf) {
        return;
    }

    clamp_allocation_table();
    const size_t payload_size =
        2 * sizeof(uint32_t) +
        g_allocations.size() * 3 * sizeof(uint32_t);
    buffer_init_dynamic(buf, payload_size);
    buffer_write_u32(buf, kSaveFormatVersion);
    buffer_write_u32(buf, static_cast<uint32_t>(g_allocations.size()));
    for (const WorkforceAllocation &allocation : g_allocations) {
        buffer_write_u32(buf, allocation.workplace_id);
        buffer_write_u32(buf, allocation.house_id);
        buffer_write_u32(buf, static_cast<uint32_t>(std::max(0, allocation.workers)));
    }
}

void building_local_workforce_load_state(buffer *buf, int has_saved_state)
{
    building_local_workforce_clear();
    if (!has_saved_state || !buf || !buf->data || !buf->size) {
        clamp_allocation_table();
        rebuild_counters_from_allocations();
        g_preserve_allocations_on_next_city_initialize = 1;
        return;
    }

    const size_t payload_size = buffer_load_dynamic(buf);
    if (payload_size < 2 * sizeof(uint32_t)) {
        clamp_allocation_table();
        rebuild_counters_from_allocations();
        g_preserve_allocations_on_next_city_initialize = 1;
        return;
    }

    const uint32_t format_version = buffer_read_u32(buf);
    const uint32_t record_count = buffer_read_u32(buf);
    if (format_version != kSaveFormatVersion) {
        clamp_allocation_table();
        rebuild_counters_from_allocations();
        g_preserve_allocations_on_next_city_initialize = 1;
        return;
    }

    const size_t max_records = (payload_size - 2 * sizeof(uint32_t)) / (3 * sizeof(uint32_t));
    const size_t records_to_read = std::min<size_t>(record_count, max_records);
    g_allocations.reserve(records_to_read);
    for (size_t i = 0; i < records_to_read; i++) {
        WorkforceAllocation allocation;
        allocation.workplace_id = buffer_read_u32(buf);
        allocation.house_id = buffer_read_u32(buf);
        allocation.workers = static_cast<int>(buffer_read_u32(buf));
        if (allocation.workers > 0) {
            g_allocations.push_back(allocation);
        }
    }
    clamp_allocation_table();
    rebuild_counters_from_allocations();
    g_preserve_allocations_on_next_city_initialize = 1;
}

namespace building_local_workforce {

int is_workforce_building(const Building &building)
{
    return uses_active_workforce(building);
}

void refresh_access_scores(void)
{
    ::refresh_access_scores();
}

void refresh_access_score(Building &building)
{
    ::refresh_access_score(building);
}

int access_score(const Building &building)
{
    return static_cast<int>(building.labor_access_score());
}

int house_available_workers(Building &house)
{
    refresh_house_unemployed(house);
    building *record = building_get(house.id());
    return std::max<int>(0, record ? record->local_workforce_unemployed : 0);
}

int labor_seeker_is_workforce(const Figure *f)
{
    if (!f || f->type != FIGURE_LABOR_SEEKER) {
        return 0;
    }
    return uses_active_workforce(f->building);
}

void reconcile_house(Building &house)
{
    trim_house_to_possible(house);
}

void remove_building(Building &target)
{
    remove_allocations_for_building(target.id());
    kill_labor_seekers_for_building(target.id());
    building *record = building_get(target.id());
    if (!record) {
        return;
    }
    record->local_workforce_assigned = 0;
    record->local_workforce_unemployed = 0;
    record->local_workforce_validation_delay = 0;
}

void replace_house(const Building &from, const Building &to)
{
    if (!from.id() || !to.id() || from.id() == to.id()) {
        return;
    }

    for (WorkforceAllocation &allocation : g_allocations) {
        if (allocation.house_id == from.id()) {
            allocation.house_id = to.id();
        }
    }
    merge_duplicate_allocations();
    clamp_allocation_table();
    rebuild_counters_from_allocations();
}

int spawn_acquisition(Building &workplace, const map_point *road)
{
    clamp_allocation_table();
    if (!uses_active_workforce(workplace) || !road) {
        return 0;
    }
    if (access_workers_for_workplace(workplace) >= required_workers(workplace)) {
        return 0;
    }

    map_point target_road = { 0, 0 };
    const int house_id = find_nearest_reachable_house_with_unemployed(
        road,
        &target_road,
        labor_seeker_max_roam_length());
    if (!house_id) {
        return 0;
    }
    return create_labor_seeker(workplace, road, &target_road, house_id, kLaborSeekerTripAcquire);
}

int spawn_validation(Building &workplace, const map_point *road)
{
    clamp_allocation_table();
    if (!uses_active_workforce(workplace) || !road) {
        return 0;
    }
    building *record = building_get(workplace.id());
    if (!record) {
        return 0;
    }
    if (access_workers_for_workplace(workplace) <= 0) {
        record->local_workforce_validation_delay = 0;
        return 0;
    }
    if (labor_seeker_slot_is_busy(workplace)) {
        return 0;
    }

    record->local_workforce_validation_delay++;
    if (record->local_workforce_validation_delay < game_time_scale_legacy_day_ticks(kValidationDelayDays)) {
        return 0;
    }
    record->local_workforce_validation_delay = 0;

    map_point target_road = { 0, 0 };
    const int house_id = find_nearest_assigned_source(workplace, road, &target_road);
    if (!house_id) {
        return 0;
    }
    return create_labor_seeker(workplace, road, &target_road, house_id, kLaborSeekerTripValidate);
}

int prepare_labor_seeker_target(Figure *f)
{
    return ::prepare_labor_seeker_target(f);
}

void labor_seeker_arrived(Figure *f)
{
    ::handle_arrival(f);
}

void labor_seeker_failed(Figure *f)
{
    if (!f) {
        return;
    }
    if (f->collecting_item_id == kLaborSeekerTripValidate) {
        ::release_workplace_source(f->building.id(), f->destination_building.id());
    }
    f->destination_building = Building(nullptr);
    if (::retarget_labor_seeker_to_unemployed(f)) {
        return;
    }
    ::retire_labor_seeker(f);
}

} // namespace building_local_workforce
