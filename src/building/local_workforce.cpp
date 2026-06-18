#include "building/building.h"
#include "building/count.h"
#include "city/labor.h"
#include "figure/action.h"

#include "building/local_workforce.h"

#include "building/building_type_registry_internal.h"
#include "figure/figure_type_registry_internal.h"
#include "map/routing_distance.h"

extern "C" {
#include "building/building_record.h"
#include "building/building_type_api.h"
#include "building/properties.h"
#include "city/population.h"
#include "core/config.h"
#include "figure/figure.h"
#include "figure/figure_runtime_api.h"
#include "figure/movement.h"
#include "figure/route.h"
#include "game/time.h"
}

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

const building_type_registry_impl::LaborSeekerPolicy *labor_policy_for(const building *b)
{
    if (!is_live_building(b)) {
        return nullptr;
    }
    const std::unique_ptr<building_type_registry_impl::BuildingType> &definition =
        building_type_registry_impl::g_building_types[b->type];
    if (!definition || !definition->has_labor() || !definition->labor().has_seeker_policy()) {
        return nullptr;
    }
    return &definition->labor().seeker_policy();
}

int uses_workforce(const building *b)
{
    const building_type_registry_impl::LaborSeekerPolicy *policy = labor_policy_for(b);
    return policy &&
        policy->method == building_type_registry_impl::LaborSeekerMethod::Workforce;
}

int uses_active_workforce(const building *b)
{
    return uses_workforce(b) &&
        !config_get(CONFIG_GP_CH_GLOBAL_LABOUR);
}

int required_workers(const building *b)
{
    return is_live_building(b) ? building_get_laborers(b->type) : 0;
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

int possible_workers_for_house(const building *house)
{
    if (!is_live_building(house) || !house->house_size || house->house_population <= 0 || city_population() <= 0) {
        return 0;
    }
    if (!building_type_registry_housing_has_resident_class(
            house->type, BUILDING_TYPE_HOUSING_RESIDENT_PLEBEIAN)) {
        return 0;
    }

    const int workers_available = city_labor_workers_available();
    if (workers_available <= 0) {
        return 0;
    }

    const int64_t numerator = static_cast<int64_t>(house->house_population) * workers_available;
    return static_cast<int>((numerator + city_population() - 1) / city_population());
}

void refresh_house_unemployed(building *house)
{
    if (!is_live_building(house) || !house->house_size) {
        return;
    }

    house->local_workforce_assigned = static_cast<short>(assigned_workers_for_house(house->id));
    const int unemployed = possible_workers_for_house(house) - house->local_workforce_assigned;
    house->local_workforce_unemployed = static_cast<short>(std::max(0, unemployed));
}

int access_workers_for_workplace(const building *workplace)
{
    if (!is_live_building(workplace)) {
        return 0;
    }
    if (!uses_active_workforce(workplace)) {
        return std::max<int>(0, workplace->houses_covered);
    }
    return std::min(assigned_workers_for_workplace(workplace->id), required_workers(workplace));
}

void refresh_access_score(building *workplace)
{
    if (!workplace) {
        return;
    }
    workplace->labor_access_score = static_cast<float>(access_workers_for_workplace(workplace));
}

void refresh_access_scores()
{
    for (int id = 1; id < building_count(); id++) {
        refresh_access_score(building_get(id));
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

int labor_seeker_slot_is_busy(building *workplace)
{
    if (!is_live_building(workplace) || !workplace->figure_id2) {
        return 0;
    }

    figure *existing = figure_get(workplace->figure_id2);
    if (existing->state && existing->building_id == workplace->id) {
        return 1;
    }
    workplace->figure_id2 = 0;
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
}

void retire_labor_seeker(figure *f)
{
    if (!f) {
        return;
    }

    building *workplace = building_get(f->building_id);
    if (is_live_building(workplace) && workplace->figure_id2 == f->id) {
        workplace->figure_id2 = 0;
    }
    f->state = FIGURE_STATE_DEAD;
}

void trim_workplace_to_required(building *workplace)
{
    if (!uses_workforce(workplace)) {
        return;
    }

    int excess = assigned_workers_for_workplace(workplace->id) - required_workers(workplace);
    for (size_t i = g_allocations.size(); i > 0 && excess > 0; i--) {
        const WorkforceAllocation &allocation = g_allocations[i - 1];
        if (allocation.workplace_id != workplace->id) {
            continue;
        }
        excess -= release_from_record(i - 1, excess);
    }
}

void trim_house_to_possible(building *house)
{
    if (!is_live_building(house) || !house->house_size) {
        return;
    }

    int excess = assigned_workers_for_house(house->id) - possible_workers_for_house(house);
    for (size_t i = g_allocations.size(); i > 0 && excess > 0; i--) {
        const WorkforceAllocation &allocation = g_allocations[i - 1];
        if (allocation.house_id != house->id) {
            continue;
        }
        excess -= release_from_record(i - 1, excess);
    }
    refresh_house_unemployed(house);
}

void clamp_allocation_table()
{
    for (size_t i = g_allocations.size(); i > 0; i--) {
        const WorkforceAllocation &allocation = g_allocations[i - 1];
        building *workplace = building_get(allocation.workplace_id);
        building *house = building_get(allocation.house_id);
        if (!uses_workforce(workplace) || allocation.workers <= 0) {
            g_allocations.erase(g_allocations.begin() + static_cast<std::ptrdiff_t>(i - 1));
        } else if (!is_live_building(house) || !house->house_size) {
            release_from_record(i - 1, allocation.workers);
        }
    }

    for (int id = 1; id < building_count(); id++) {
        trim_workplace_to_required(building_get(id));
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

    if (!routing_distance::prepare_from_road(*road)) {
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

        const routing_distance::BuildingRoadResult route =
            routing_distance::find_access_road_to_building(house, 2, max_distance, 1);
        if (!route.reachable) {
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

int remaining_roam_length(const figure *f)
{
    if (!f) {
        return 0;
    }

    const int max_roam_length = f->max_roam_length > 0 ? f->max_roam_length : labor_seeker_max_roam_length();
    return std::max(0, max_roam_length - f->roam_length);
}

int find_nearest_assigned_source(building *workplace, const map_point *road, map_point *target_road)
{
    if (!uses_active_workforce(workplace) || !road || !target_road) {
        return 0;
    }

    if (!routing_distance::prepare_from_road(*road)) {
        return 0;
    }

    const int max_roam_length = labor_seeker_max_roam_length();
    int best_house_id = 0;
    int best_distance = 0x7fffffff;
    int house_id_to_release = 0;
    map_point best_road = { 0, 0 };
    for (const WorkforceAllocation &allocation : g_allocations) {
        if (allocation.workplace_id != workplace->id || allocation.workers <= 0) {
            continue;
        }

        building *house = building_get(allocation.house_id);
        if (!is_live_building(house) || !house->house_size) {
            if (!house_id_to_release) {
                house_id_to_release = allocation.house_id;
            }
            continue;
        }

        const routing_distance::BuildingRoadResult route =
            routing_distance::find_access_road_to_building(house, 2, max_roam_length, 1);
        if (!route.reachable) {
            if (!house_id_to_release) {
                house_id_to_release = allocation.house_id;
            }
            continue;
        }

        if (route.distance < best_distance) {
            best_distance = route.distance;
            best_house_id = house->id;
            best_road = route.road;
        }
    }

    if (house_id_to_release) {
        release_workplace_source(workplace->id, house_id_to_release);
        return 0;
    }

    if (!best_house_id) {
        return 0;
    }
    *target_road = best_road;
    return best_house_id;
}

int prepare_labor_seeker_target(figure *f)
{
    if (!f) {
        return 0;
    }

    building *workplace = building_get(f->building_id);
    if (!uses_active_workforce(workplace) || workplace->figure_id2 != f->id) {
        return 0;
    }
    if (f->destination_building_id) {
        building *house = building_get(f->destination_building_id);
        if (is_live_building(house) && house->house_size) {
            if (f->collecting_item_id == kLaborSeekerTripValidate) {
                return 1;
            }
            refresh_house_unemployed(house);
            if (house->local_workforce_unemployed > 0) {
                return 1;
            }
        } else if (f->collecting_item_id == kLaborSeekerTripValidate) {
            release_workplace_source(f->building_id, f->destination_building_id);
        }
        f->destination_building_id = 0;
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

    f->destination_building_id = static_cast<unsigned int>(house_id);
    f->destination_x = static_cast<unsigned char>(target_road.x);
    f->destination_y = static_cast<unsigned char>(target_road.y);
    figure_route_remove(f);
    return 1;
}

int create_labor_seeker(
    building *workplace,
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
    figure *labor_seeker = figure_runtime_create_profiled(
        FIGURE_LABOR_SEEKER,
        source_road->x,
        source_road->y,
        DIR_0_TOP,
        workplace->id,
        profile_id);
    if (!labor_seeker || !labor_seeker->id) {
        return 0;
    }

    // The trip flag is retained for save compatibility; the profile owns the behavior contract.
    labor_seeker->destination_building_id = static_cast<unsigned int>(std::max(0, house_id));
    labor_seeker->destination_x = static_cast<unsigned char>(target_road ? target_road->x : source_road->x);
    labor_seeker->destination_y = static_cast<unsigned char>(target_road ? target_road->y : source_road->y);
    labor_seeker->collecting_item_id = trip_type;
    workplace->figure_id2 = labor_seeker->id;
    return 1;
}

int retarget_labor_seeker_to_unemployed(figure *f)
{
    if (!f) {
        return 0;
    }

    building *workplace = building_get(f->building_id);
    if (!uses_active_workforce(workplace) || workplace->figure_id2 != f->id) {
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
    f->destination_building_id = static_cast<unsigned int>(house_id);
    f->destination_x = static_cast<unsigned char>(target_road.x);
    f->destination_y = static_cast<unsigned char>(target_road.y);
    f->collecting_item_id = kLaborSeekerTripAcquire;
    figure_route_remove(f);
    return 1;
}

void handle_arrival(figure *f)
{
    building *workplace = building_get(f->building_id);
    building *house = building_get(f->destination_building_id);
    if (!uses_active_workforce(workplace)) {
        retire_labor_seeker(f);
        return;
    }
    if (!is_live_building(house) || !house->house_size) {
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
    const int assigned = std::min<int>(std::max(0, needed), house->local_workforce_unemployed);
    if (assigned > 0) {
        add_allocation(workplace->id, house->id, assigned);
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

    for (unsigned int id = 1; id < figure_count(); id++) {
        figure *f = figure_get(id);
        if (!f || f->state != FIGURE_STATE_ALIVE || f->type != FIGURE_LABOR_SEEKER) {
            continue;
        }
        if (f->building_id != building_id && f->destination_building_id != building_id) {
            continue;
        }
        building *workplace = building_get(f->building_id);
        if (is_live_building(workplace) && workplace->figure_id2 == f->id) {
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

} // namespace

extern "C" void building_local_workforce_clear(void)
{
    g_allocations.clear();
    g_preserve_allocations_on_next_city_initialize = 0;
}

extern "C" void building_local_workforce_initialize_city(void)
{
    if (!g_preserve_allocations_on_next_city_initialize) {
        g_allocations.clear();
    }
    g_preserve_allocations_on_next_city_initialize = 0;
    clamp_allocation_table();
    rebuild_counters_from_allocations();
    refresh_access_scores();
}

extern "C" void building_local_workforce_save_state(buffer *buf)
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

extern "C" void building_local_workforce_load_state(buffer *buf, int has_saved_state)
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

extern "C" int building_local_workforce_is_workforce_building(const building *b)
{
    return uses_active_workforce(b);
}

extern "C" void building_local_workforce_refresh_access_score(building *b)
{
    refresh_access_score(b);
}

extern "C" void building_local_workforce_refresh_access_scores(void)
{
    refresh_access_scores();
}

extern "C" int building_local_workforce_access_score(const building *b)
{
    return b ? static_cast<int>(b->labor_access_score) : 0;
}

extern "C" int building_local_workforce_house_available_workers(building *house)
{
    refresh_house_unemployed(house);
    return house ? std::max<int>(0, house->local_workforce_unemployed) : 0;
}

extern "C" int building_local_workforce_labor_seeker_is_workforce(const figure *f)
{
    if (!f || f->type != FIGURE_LABOR_SEEKER) {
        return 0;
    }
    return uses_active_workforce(building_get(f->building_id));
}

extern "C" void building_local_workforce_reconcile_house(building *house)
{
    trim_house_to_possible(house);
}

extern "C" void building_local_workforce_remove_building(building *b)
{
    if (!b || !b->id) {
        return;
    }
    remove_allocations_for_building(b->id);
    kill_labor_seekers_for_building(b->id);
    b->local_workforce_assigned = 0;
    b->local_workforce_unemployed = 0;
    b->local_workforce_validation_delay = 0;
}

extern "C" int building_local_workforce_spawn_acquisition(building *workplace, const map_point *road)
{
    clamp_allocation_table();
    if (!uses_active_workforce(workplace) || !road) {
        return 0;
    }
    if (access_workers_for_workplace(workplace) >= required_workers(workplace)) {
        return 0;
    }

    return create_labor_seeker(workplace, road, nullptr, 0, kLaborSeekerTripAcquire);
}

extern "C" int building_local_workforce_spawn_validation(building *workplace, const map_point *road)
{
    clamp_allocation_table();
    if (!uses_active_workforce(workplace) || !road) {
        return 0;
    }
    if (access_workers_for_workplace(workplace) <= 0) {
        workplace->local_workforce_validation_delay = 0;
        return 0;
    }
    if (labor_seeker_slot_is_busy(workplace)) {
        return 0;
    }

    workplace->local_workforce_validation_delay++;
    if (workplace->local_workforce_validation_delay < game_time_scale_legacy_day_ticks(kValidationDelayDays)) {
        return 0;
    }
    workplace->local_workforce_validation_delay = 0;

    map_point target_road = { 0, 0 };
    const int house_id = find_nearest_assigned_source(workplace, road, &target_road);
    if (!house_id) {
        return 0;
    }
    return create_labor_seeker(workplace, road, &target_road, house_id, kLaborSeekerTripValidate);
}

extern "C" int building_local_workforce_prepare_labor_seeker_target(figure *f)
{
    return prepare_labor_seeker_target(f);
}

extern "C" void building_local_workforce_labor_seeker_arrived(figure *f)
{
    handle_arrival(f);
}

extern "C" void building_local_workforce_labor_seeker_failed(figure *f)
{
    if (!f) {
        return;
    }
    if (f->collecting_item_id == kLaborSeekerTripValidate) {
        release_workplace_source(f->building_id, f->destination_building_id);
    }
    f->destination_building_id = 0;
    if (retarget_labor_seeker_to_unemployed(f)) {
        return;
    }
    retire_labor_seeker(f);
}

extern "C" void building_local_workforce_cancel_labor_seeker(figure *f)
{
    retire_labor_seeker(f);
}
