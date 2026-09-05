#include "building/building.h"
#include "building/count.h"
#include "city/labor.h"
#include "figure/action.h"

#include "building/local_workforce.h"
#include "building/local_workforce_route_access.h"

#include "building/building_runtime_internal.h"
#include "building/building_type_registry_internal.h"
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

building_local_workforce::LocalWorkforceRuntimeState g_runtime_state;
void refresh_house_unemployed(Building &house);

const figure_type_registry_impl::FigureTypeProfile *labor_seeker_profile(const char *profile_id)
{
    const figure_type_registry_impl::FigureTypeProfile *profile =
        figure_type_registry_impl::profile_for(FIGURE_LABOR_SEEKER, profile_id);
    return profile ? profile : figure_type_registry_impl::default_profile_for(FIGURE_LABOR_SEEKER);
}

RoutePolicy labor_seeker_route_policy(const char *profile_id)
{
    const figure_type_registry_impl::FigureTypeProfile *profile = labor_seeker_profile(profile_id);
    if (!profile) {
        RoutePolicy fallback = RoutePolicy::fromKind(RoutePolicyKind::CitizenRoadGarden);
        fallback.permission = PERMISSION_LABOR_SEEKER;
        return fallback;
    }
    return profile->pathing_policy()
        .routePolicySelection(PERMISSION_LABOR_SEEKER, RouteNeighborhood::FourWay)
        .policy;
}

} // namespace

namespace building_local_workforce {

void LocalWorkforceRuntimeState::clear()
{
    reservations_.clear();
    allocations_.clear();
    preserve_allocations_on_next_city_initialize_ = 0;
}

void LocalWorkforceRuntimeState::initializeCity()
{
    reservations_.clear();
    if (!preserve_allocations_on_next_city_initialize_) {
        allocations_.clear();
    }
    preserve_allocations_on_next_city_initialize_ = 0;
}

void LocalWorkforceRuntimeState::preserveAllocationsForNextCityInitialize()
{
    preserve_allocations_on_next_city_initialize_ = 1;
}

building_local_workforce::LocalWorkforceRouteAccessContext LocalWorkforceRuntimeState::routeAccessContext()
{
    return building_local_workforce::LocalWorkforceRouteAccessContext(allocations_);
}

int LocalWorkforceRuntimeState::assignedWorkersForHouse(unsigned int house_id) const
{
    return allocations_.assignedWorkersForHouse(house_id);
}

int LocalWorkforceRuntimeState::assignedWorkersForWorkplace(unsigned int workplace_id) const
{
    return allocations_.assignedWorkersForWorkplace(workplace_id);
}

workforce_count LocalWorkforceRuntimeState::reservedWorkersForHouse(const Building &house) const
{
    int workers = 0;
    for (const std::unique_ptr<LaborReservation> &reservation : reservations_) {
        if (&reservation->house == &house) {
            workers += reservation->workers;
        }
    }
    return static_cast<workforce_count>(workers);
}

LaborReservation *LocalWorkforceRuntimeState::reservationFor(const Figure &seeker) const
{
    for (const std::unique_ptr<LaborReservation> &reservation : reservations_) {
        if (&reservation->seeker == &seeker) {
            return reservation.get();
        }
    }
    return nullptr;
}

LaborReservation &LocalWorkforceRuntimeState::reserve(
    Figure &seeker,
    Building &house,
    workforce_count workers)
{
    cancelReservation(seeker);
    reservations_.push_back(std::make_unique<LaborReservation>(seeker, house, workers));
    refresh_house_unemployed(house);
    return *reservations_.back();
}

std::unique_ptr<LaborReservation> LocalWorkforceRuntimeState::takeReservation(Figure &seeker)
{
    for (auto it = reservations_.begin(); it != reservations_.end(); ++it) {
        if (&(*it)->seeker != &seeker) {
            continue;
        }
        std::unique_ptr<LaborReservation> reservation = std::move(*it);
        reservations_.erase(it);
    seeker.set_destination_building(nullptr);
        return reservation;
    }
    return nullptr;
}

void LocalWorkforceRuntimeState::cancelReservation(Figure &seeker)
{
    std::unique_ptr<LaborReservation> reservation = takeReservation(seeker);
    if (reservation) {
        refresh_house_unemployed(reservation->house);
    }
}

void LocalWorkforceRuntimeState::cancelReservationsForBuilding(Building &building)
{
    std::vector<Building *> houses;
    for (auto it = reservations_.begin(); it != reservations_.end();) {
        LaborReservation &reservation = **it;
        Figure &seeker = reservation.seeker;
        const int house_changed = &reservation.house == &building;
        const int workplace_changed = seeker.building == &building;
        if (!house_changed && !workplace_changed) {
            ++it;
            continue;
        }

        Building *workplace = seeker.building;
        if (workplace) {
            ::building *record = const_cast<::building *>(workplace->record());
            if (record && record->figure_id2 == seeker.id()) {
                record->figure_id2 = 0;
            }
        }
        if (workplace_changed) {
        seeker.set_home_building(nullptr);
        }
    seeker.set_destination_building(nullptr);
        seeker.state = FIGURE_STATE_DEAD;
        if (!house_changed &&
            std::find(houses.begin(), houses.end(), &reservation.house) == houses.end()) {
            houses.push_back(&reservation.house);
        }
        it = reservations_.erase(it);
    }
    for (Building *house : houses) {
        refresh_house_unemployed(*house);
    }
}

void LocalWorkforceRuntimeState::addAllocation(unsigned int workplace_id, unsigned int house_id, int workers)
{
    allocations_.add(workplace_id, house_id, workers);
}

void LocalWorkforceRuntimeState::reserveLoadedAllocations(size_t records)
{
    allocations_.reserve(records);
}

void LocalWorkforceRuntimeState::appendLoadedAllocation(
    unsigned int workplace_id,
    unsigned int house_id,
    int workers)
{
    allocations_.appendLoadedRecord(workplace_id, house_id, workers);
}

size_t LocalWorkforceRuntimeState::savePayloadSize() const
{
    return 2 * sizeof(uint32_t) +
        allocations_.size() * 3 * sizeof(uint32_t);
}

void LocalWorkforceRuntimeState::writeAllocationSavePayload(buffer *buf) const
{
    buffer_write_u32(buf, static_cast<uint32_t>(allocations_.size()));
    allocations_.writeSaveRecords(buf);
}

void LocalWorkforceRuntimeState::releaseWorkplaceSource(unsigned int workplace_id, unsigned int house_id)
{
    allocations_.releaseWorkplaceSource(workplace_id, house_id);
}

int LocalWorkforceRuntimeState::releaseFromWorkplace(unsigned int workplace_id, int workers)
{
    return allocations_.releaseFromWorkplace(workplace_id, workers);
}

int LocalWorkforceRuntimeState::releaseFromHouse(unsigned int house_id, int workers)
{
    return allocations_.releaseFromHouse(house_id, workers);
}

void LocalWorkforceRuntimeState::replaceHouse(unsigned int from_house_id, unsigned int to_house_id)
{
    allocations_.replaceHouse(from_house_id, to_house_id);
}

void LocalWorkforceRuntimeState::removeAllocationsIf(const RecordPredicate &predicate)
{
    allocations_.removeIf(predicate);
}

void LocalWorkforceRuntimeState::forEachAssignedSource(
    unsigned int workplace_id,
    const AssignedSourceVisitor &visitor) const
{
    allocations_.forEachAssignedSource(workplace_id, visitor);
}

} // namespace building_local_workforce

namespace {

int is_live_building(const building *b)
{
    return b && b->id && b->state == BUILDING_STATE_IN_USE;
}

int is_live_building(const Building &building)
{
    return building.id && building.is_in_use();
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
    if (building.type->is_fountain() && grand_temple_for_god(GOD_NEPTUNE, true)) {
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
        labor_seeker_profile(kLaborSeekerAcquireProfile);
    if (!profile) {
        return kDefaultLaborSeekerMaxRoamLength;
    }
    const int max_roam_length = profile->movement_profile().max_roam_length;
    return max_roam_length > 0 ? max_roam_length : kDefaultLaborSeekerMaxRoamLength;
}

int possible_workers_for_house(const Building &house)
{
    const building *record = house.record();
    if (!record || !is_live_building(house) || !house.Housing ||
        house.Housing->state().population <= 0 || city_population() <= 0) {
        return 0;
    }
    if (!house.Housing->has_plebeian_residents()) {
        return 0;
    }

    const int workers_available = city_labor_workers_available();
    if (workers_available <= 0) {
        return 0;
    }

    const int64_t numerator = static_cast<int64_t>(house.Housing->state().population) * workers_available;
    return static_cast<int>((numerator + city_population() - 1) / city_population());
}

void refresh_house_unemployed(Building &house)
{
    building *record = const_cast<building *>(house.record());
    if (!record || !is_live_building(house) || !house.Housing) {
        return;
    }

    record->local_workforce_assigned = static_cast<short>(g_runtime_state.assignedWorkersForHouse(house.id));
    const int unemployed = possible_workers_for_house(house) -
        record->local_workforce_assigned -
        g_runtime_state.reservedWorkersForHouse(house);
    record->local_workforce_unemployed = static_cast<short>(std::max(0, unemployed));
}

void refresh_house_unemployed(building *house)
{
    if (!is_live_building(house)) {
        return;
    }
    if (building_runtime *runtime = building_runtime_impl::get_or_create_instance(house)) {
        if (runtime->building.Housing) {
            refresh_house_unemployed(runtime->building);
        }
    }
}

int access_workers_for_workplace(const Building &workplace)
{
    const building *record = workplace.record();
    if (!record || !is_live_building(workplace)) {
        return 0;
    }
    if (!uses_active_workforce(workplace)) {
        return std::max<int>(0, record->houses_covered);
    }
    return std::min(g_runtime_state.assignedWorkersForWorkplace(workplace.id), required_workers(workplace));
}

void refresh_access_score(Building &workplace)
{
    building *record = const_cast<building *>(workplace.record());
    record->labor_access_score = static_cast<float>(access_workers_for_workplace(workplace));
}

void refresh_access_scores()
{
    Building::for_each([](Building *building) {
        refresh_access_score(*building);
    });
}

int labor_seeker_slot_is_busy(Building &workplace)
{
    building *record = const_cast<building *>(workplace.record());
    if (!record || !is_live_building(workplace) || !record->figure_id2) {
        return 0;
    }

    Figure *existing = Figure::get(record->figure_id2);
    if (existing->state && existing->building && existing->building->id == workplace.id) {
        return 1;
    }
    record->figure_id2 = 0;
    return 0;
}

void release_workplace_source(unsigned int workplace_id, unsigned int house_id)
{
    g_runtime_state.releaseWorkplaceSource(workplace_id, house_id);

    if (Building *house = Building::get(house_id)) {
        refresh_house_unemployed(const_cast<::building *>(house->record()));
    }
    if (Building *workplace = Building::get(workplace_id)) {
        if (is_live_building(*workplace)) {
            refresh_access_score(*workplace);
        }
    }
}

void retire_labor_seeker(Figure *f)
{
    if (!f) {
        return;
    }

    g_runtime_state.cancelReservation(*f);
    if (!f->building) {
        f->state = FIGURE_STATE_DEAD;
        return;
    }
    building *workplace = const_cast<building *>(f->building->record());
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

    int excess = g_runtime_state.assignedWorkersForWorkplace(workplace.id) - required_workers(workplace);
    g_runtime_state.releaseFromWorkplace(workplace.id, excess);
}

void trim_house_to_possible(Building &house)
{
    if (!is_live_building(house) || !house.Housing) {
        return;
    }

    int excess = g_runtime_state.assignedWorkersForHouse(house.id) - possible_workers_for_house(house);
    g_runtime_state.releaseFromHouse(house.id, excess);
    refresh_house_unemployed(house);
}

void trim_house_to_possible(building *house)
{
    if (!is_live_building(house)) {
        return;
    }
    if (building_runtime *runtime = building_runtime_impl::get_or_create_instance(house)) {
        if (runtime->building.Housing) {
            trim_house_to_possible(runtime->building);
        }
    }
}

void clamp_allocation_table()
{
    g_runtime_state.removeAllocationsIf([](unsigned int workplace_id, unsigned int house_id, int workers) {
        Building *workplace = Building::get(workplace_id);
        Building *house = Building::get(house_id);
        const building *house_record = house ? house->record() : nullptr;
        if (!workplace || workers <= 0) {
            return 1;
        }
        return (!uses_workforce(*workplace) || !house_record || !is_live_building(*house) ||
            !house->Housing) ? 1 : 0;
    });

    Building::for_each(BuildingRuntimeList::Labor, [](Building *building) {
        trim_workplace_to_required(*building);
    });
}

void rebuild_counters_from_allocations()
{
    Building::for_each([](Building *building) {
        ::building *record = const_cast<::building *>(building->record());
        record->local_workforce_assigned = 0;
        record->local_workforce_unemployed = 0;
        record->local_workforce_validation_delay = 0;
    });

    Building::for_each(BuildingRuntimeList::Housing, [](Building *house) {
        refresh_house_unemployed(*house);
    });
}

int remaining_roam_length(const Figure *f)
{
    if (!f) {
        return 0;
    }

    const int max_roam_length = f->max_roam_length > 0 ? f->max_roam_length : labor_seeker_max_roam_length();
    return std::max(0, max_roam_length - f->roam_length);
}

workforce_count workers_to_reserve(Figure &seeker, Building &house)
{
    if (seeker.collecting_item_id == kLaborSeekerTripValidate || !seeker.building) {
        return 0;
    }
    refresh_house_unemployed(house);
    const building *house_record = house.record();
    const int needed = required_workers(*seeker.building) - access_workers_for_workplace(*seeker.building);
    return static_cast<workforce_count>(
        std::min<int>(std::max(0, needed), house_record ? house_record->local_workforce_unemployed : 0));
}

building_local_workforce::LaborReservation *ensure_labor_reservation(Figure &seeker)
{
    if (building_local_workforce::LaborReservation *reservation = g_runtime_state.reservationFor(seeker)) {
        return reservation;
    }
    if (!seeker.building || !seeker.destination_building) {
        return nullptr;
    }
    Building &house = *seeker.destination_building;
    if (!is_live_building(house) || !house.Housing) {
        seeker.set_destination_building(nullptr);
        return nullptr;
    }
    const workforce_count workers = workers_to_reserve(seeker, house);
    if (seeker.collecting_item_id == kLaborSeekerTripAcquire && workers <= 0) {
    seeker.set_destination_building(nullptr);
        return nullptr;
    }
    return &g_runtime_state.reserve(seeker, house, workers);
}

int prepare_labor_seeker_target(Figure *f)
{
    if (!f || !f->building) {
        return 0;
    }

    Building &workplace = *f->building;
    building *workplace_record = const_cast<building *>(workplace.record());
    if (!uses_active_workforce(workplace) || !workplace_record || workplace_record->figure_id2 != f->id()) {
        return 0;
    }
    building_local_workforce::LaborReservation *reservation = ensure_labor_reservation(*f);
    if (!reservation) {
        return 0;
    }

    Building &house = reservation->house;
    if (!is_live_building(house) || !house.Housing) {
        if (f->collecting_item_id == kLaborSeekerTripValidate) {
            release_workplace_source(workplace.id, house.id);
        }
        g_runtime_state.cancelReservation(*f);
        return 0;
    }

    return 1;
}

int create_labor_seeker(
    Building &workplace,
    const map_point *source_road,
    const building_local_workforce::HouseRouteSelection &target,
    unsigned char trip_type)
{
    if (!uses_active_workforce(workplace) || !source_road || !target) {
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

    building *record = const_cast<building *>(workplace.record());
    // The trip flag is retained for save compatibility; the profile owns the behavior contract.
    labor_seeker->collecting_item_id = trip_type;
        labor_seeker->set_destination_building(target.house);
    labor_seeker->destination_x = static_cast<unsigned char>(target.road().x);
    labor_seeker->destination_y = static_cast<unsigned char>(target.road().y);
    if (!ensure_labor_reservation(*labor_seeker)) {
        labor_seeker->state = FIGURE_STATE_DEAD;
        labor_seeker->set_destination_building(nullptr);
        return 0;
    }
    record->figure_id2 = labor_seeker->id();
    return 1;
}

int retarget_labor_seeker_to_unemployed(Figure *f)
{
    if (!f || !f->building) {
        return 0;
    }

    g_runtime_state.cancelReservation(*f);
    Building &workplace = *f->building;
    building *workplace_record = const_cast<building *>(workplace.record());
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

    const map_point source_road = { f->x, f->y };
    building_local_workforce::LocalWorkforceRouteAccessContext context = g_runtime_state.routeAccessContext();
    const figure_type_registry_impl::PathingMode::RoutePolicySelection route_selection =
        figure_runtime_route_policy_selection(f, RouteNeighborhood::FourWay);
    const building_local_workforce::RouteAccessSelector selector =
        building_local_workforce::RouteAccessSelector::fromRoad(
            source_road,
            max_distance,
            route_selection.policy,
            context);
    const building_local_workforce::HouseRouteSelection target = selector.nearestUnemployedHouse();
    if (!target) {
        return 0;
    }

    f->action_state = FIGURE_ACTION_125_ROAMING;
        f->set_destination_building(target.house);
    f->destination_x = static_cast<unsigned char>(target.road().x);
    f->destination_y = static_cast<unsigned char>(target.road().y);
    f->collecting_item_id = kLaborSeekerTripAcquire;
    if (!ensure_labor_reservation(*f)) {
        f->set_destination_building(nullptr);
        return 0;
    }
    Route::remove(f);
    return 1;
}

void handle_arrival(Figure *f)
{
    if (!f || !f->building) {
        retire_labor_seeker(f);
        return;
    }
    building_local_workforce::LaborReservation *reservation = ensure_labor_reservation(*f);
    if (!reservation) {
        retire_labor_seeker(f);
        return;
    }
    Building &workplace = *f->building;
    Building &house = reservation->house;
    if (!uses_active_workforce(workplace)) {
        retire_labor_seeker(f);
        return;
    }
    if (!is_live_building(house) || !house.Housing) {
        if (retarget_labor_seeker_to_unemployed(f)) {
            return;
        }
        retire_labor_seeker(f);
        return;
    }

    if (f->collecting_item_id == kLaborSeekerTripValidate) {
        g_runtime_state.cancelReservation(*f);
        trim_house_to_possible(house);
        if (retarget_labor_seeker_to_unemployed(f)) {
            return;
        }
        retire_labor_seeker(f);
        return;
    }

    std::unique_ptr<building_local_workforce::LaborReservation> completed =
        g_runtime_state.takeReservation(*f);
    if (completed && completed->workers > 0) {
        g_runtime_state.addAllocation(workplace.id, completed->house.id, completed->workers);
        refresh_house_unemployed(completed->house);
    }
    if (retarget_labor_seeker_to_unemployed(f)) {
        return;
    }
    retire_labor_seeker(f);
}

void remove_allocations_for_building(unsigned int building_id)
{
    if (!building_id) {
        return;
    }

    std::vector<unsigned int> house_ids_to_refresh;
    g_runtime_state.forEachAssignedSource(
        building_id,
        [&house_ids_to_refresh](unsigned int house_id, int) {
            house_ids_to_refresh.push_back(house_id);
        });
    for (unsigned int house_id : house_ids_to_refresh) {
        g_runtime_state.releaseWorkplaceSource(building_id, house_id);
        if (Building *house = Building::get(house_id)) {
            refresh_house_unemployed(const_cast<::building *>(house->record()));
        }
    }
    g_runtime_state.releaseFromHouse(building_id, g_runtime_state.assignedWorkersForHouse(building_id));
}

} // namespace

namespace building_local_workforce {

void WorkforceAllocationTable::clear()
{
    records_.clear();
}

void WorkforceAllocationTable::reserve(size_t records)
{
    records_.reserve(records);
}

size_t WorkforceAllocationTable::size() const
{
    return records_.size();
}

int WorkforceAllocationTable::assignedWorkersForHouse(unsigned int house_id) const
{
    int assigned = 0;
    for (const Record &allocation : records_) {
        if (allocation.house_id == house_id) {
            assigned += allocation.workers;
        }
    }
    return assigned;
}

int WorkforceAllocationTable::assignedWorkersForWorkplace(unsigned int workplace_id) const
{
    int assigned = 0;
    for (const Record &allocation : records_) {
        if (allocation.workplace_id == workplace_id) {
            assigned += allocation.workers;
        }
    }
    return assigned;
}

void WorkforceAllocationTable::add(unsigned int workplace_id, unsigned int house_id, int workers)
{
    if (workers <= 0) {
        return;
    }

    for (Record &allocation : records_) {
        if (allocation.workplace_id == workplace_id && allocation.house_id == house_id) {
            allocation.workers += workers;
            return;
        }
    }
    records_.push_back({ workplace_id, house_id, workers });
}

void WorkforceAllocationTable::appendLoadedRecord(unsigned int workplace_id, unsigned int house_id, int workers)
{
    if (workers > 0) {
        records_.push_back({ workplace_id, house_id, workers });
    }
}

void WorkforceAllocationTable::writeSaveRecords(buffer *buf) const
{
    if (!buf) {
        return;
    }

    for (const Record &allocation : records_) {
        buffer_write_u32(buf, allocation.workplace_id);
        buffer_write_u32(buf, allocation.house_id);
        buffer_write_u32(buf, static_cast<uint32_t>(std::max(0, allocation.workers)));
    }
}

void WorkforceAllocationTable::releaseWorkplaceSource(unsigned int workplace_id, unsigned int house_id)
{
    for (size_t i = records_.size(); i > 0; i--) {
        Record &allocation = records_[i - 1];
        if (allocation.workplace_id == workplace_id && allocation.house_id == house_id) {
            releaseFromRecord(i - 1, allocation.workers);
        }
    }
}

int WorkforceAllocationTable::releaseFromWorkplace(unsigned int workplace_id, int workers)
{
    int released = 0;
    for (size_t i = records_.size(); i > 0 && workers > 0; i--) {
        const Record &allocation = records_[i - 1];
        if (allocation.workplace_id != workplace_id) {
            continue;
        }
        const int just_released = releaseFromRecord(i - 1, workers);
        workers -= just_released;
        released += just_released;
    }
    return released;
}

int WorkforceAllocationTable::releaseFromHouse(unsigned int house_id, int workers)
{
    int released = 0;
    for (size_t i = records_.size(); i > 0 && workers > 0; i--) {
        const Record &allocation = records_[i - 1];
        if (allocation.house_id != house_id) {
            continue;
        }
        const int just_released = releaseFromRecord(i - 1, workers);
        workers -= just_released;
        released += just_released;
    }
    return released;
}

void WorkforceAllocationTable::replaceHouse(unsigned int from_house_id, unsigned int to_house_id)
{
    if (!from_house_id || !to_house_id || from_house_id == to_house_id) {
        return;
    }

    for (Record &allocation : records_) {
        if (allocation.house_id == from_house_id) {
            allocation.house_id = to_house_id;
        }
    }
    mergeDuplicates();
}

void WorkforceAllocationTable::removeIf(const RecordPredicate &predicate)
{
    if (!predicate) {
        return;
    }

    for (size_t i = records_.size(); i > 0; i--) {
        const Record &allocation = records_[i - 1];
        if (predicate(allocation.workplace_id, allocation.house_id, allocation.workers)) {
            records_.erase(records_.begin() + static_cast<std::ptrdiff_t>(i - 1));
        }
    }
}

void WorkforceAllocationTable::forEachAssignedSource(
    unsigned int workplace_id,
    const AssignedSourceVisitor &visitor) const
{
    if (!visitor) {
        return;
    }

    for (const Record &allocation : records_) {
        if (allocation.workplace_id == workplace_id) {
            visitor(allocation.house_id, allocation.workers);
        }
    }
}

int WorkforceAllocationTable::releaseFromRecord(size_t index, int workers)
{
    if (index >= records_.size() || workers <= 0) {
        return 0;
    }

    Record &allocation = records_[index];
    const int released = std::min(workers, allocation.workers);
    allocation.workers -= released;
    if (allocation.workers <= 0) {
        records_.erase(records_.begin() + static_cast<std::ptrdiff_t>(index));
    }
    return released;
}

void WorkforceAllocationTable::mergeDuplicates()
{
    for (size_t i = 0; i < records_.size(); i++) {
        Record &current = records_[i];
        if (current.workers <= 0) {
            continue;
        }
        for (size_t j = records_.size(); j > i + 1; j--) {
            Record &other = records_[j - 1];
            if (other.workplace_id == current.workplace_id && other.house_id == current.house_id) {
                current.workers += other.workers;
                records_.erase(records_.begin() + static_cast<std::ptrdiff_t>(j - 1));
            }
        }
    }
}

LocalWorkforceRouteAccessContext::LocalWorkforceRouteAccessContext(WorkforceAllocationTable &allocations)
    : allocations_(allocations)
{}

void LocalWorkforceRouteAccessContext::forEachPopulatedLaborSourceHouse(
    const std::function<void(Building &)> &visitor) const
{
    Building::for_each(BuildingRuntimeList::Housing, [&visitor](Building *house) {
        if (house->is_labor_source_house() && house->Housing->state().population > 0) visitor(*house);
    });
}

int LocalWorkforceRouteAccessContext::houseHasUnemployedWorkers(Building &house) const
{
    return building_local_workforce::house_available_workers(house) > 0;
}

int LocalWorkforceRouteAccessContext::usesActiveWorkforce(const Building &building) const
{
    return ::uses_active_workforce(building);
}

void LocalWorkforceRouteAccessContext::forEachAssignedSource(
    unsigned int workplace_id,
    const AssignedSourceVisitor &visitor) const
{
    allocations_.forEachAssignedSource(workplace_id, [&visitor](unsigned int house_id, int workers) {
        building_local_workforce::AssignedWorkforceSource source;
        source.house_id = house_id;
        source.workers = workers;
        visitor(source);
    });
}

void LocalWorkforceRouteAccessContext::releaseWorkplaceSource(unsigned int workplace_id, unsigned int house_id) const
{
    ::release_workplace_source(workplace_id, house_id);
}

} // namespace building_local_workforce

void building_local_workforce_clear(void)
{
    g_runtime_state.clear();
}

void building_local_workforce_initialize_city(void)
{
    g_runtime_state.initializeCity();
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
    const size_t payload_size = g_runtime_state.savePayloadSize();
    buffer_init_dynamic(buf, payload_size);
    buffer_write_u32(buf, kSaveFormatVersion);
    g_runtime_state.writeAllocationSavePayload(buf);
}

void building_local_workforce_load_state(buffer *buf, int has_saved_state)
{
    building_local_workforce_clear();
    if (!has_saved_state || !buf || !buf->data || !buf->size) {
        clamp_allocation_table();
        rebuild_counters_from_allocations();
        g_runtime_state.preserveAllocationsForNextCityInitialize();
        return;
    }

    const size_t payload_size = buffer_load_dynamic(buf);
    if (payload_size < 2 * sizeof(uint32_t)) {
        clamp_allocation_table();
        rebuild_counters_from_allocations();
        g_runtime_state.preserveAllocationsForNextCityInitialize();
        return;
    }

    const uint32_t format_version = buffer_read_u32(buf);
    const uint32_t record_count = buffer_read_u32(buf);
    if (format_version != kSaveFormatVersion) {
        clamp_allocation_table();
        rebuild_counters_from_allocations();
        g_runtime_state.preserveAllocationsForNextCityInitialize();
        return;
    }

    const size_t max_records = (payload_size - 2 * sizeof(uint32_t)) / (3 * sizeof(uint32_t));
    const size_t records_to_read = std::min<size_t>(record_count, max_records);
    g_runtime_state.reserveLoadedAllocations(records_to_read);
    for (size_t i = 0; i < records_to_read; i++) {
        const unsigned int workplace_id = buffer_read_u32(buf);
        const unsigned int house_id = buffer_read_u32(buf);
        const int workers = static_cast<int>(buffer_read_u32(buf));
        g_runtime_state.appendLoadedAllocation(workplace_id, house_id, workers);
    }
    clamp_allocation_table();
    rebuild_counters_from_allocations();
    g_runtime_state.preserveAllocationsForNextCityInitialize();
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
    const building *record = house.record();
    return std::max<int>(0, record->local_workforce_unemployed);
}

int labor_seeker_is_workforce(const Figure *f)
{
    if (!f || f->type != FIGURE_LABOR_SEEKER || !f->building) {
        return 0;
    }
    return uses_active_workforce(*f->building);
}

void reconcile_house(Building &house)
{
    trim_house_to_possible(house);
}

void remove_building(Building &target)
{
    ::g_runtime_state.cancelReservationsForBuilding(target);
    remove_allocations_for_building(target.id);
    building *record = const_cast<building *>(target.record());
    record->local_workforce_assigned = 0;
    record->local_workforce_unemployed = 0;
    record->local_workforce_validation_delay = 0;
}

void change_building(Building &target)
{
    ::g_runtime_state.cancelReservationsForBuilding(target);
}

void remove_labor_seeker(Figure &seeker)
{
    ::g_runtime_state.cancelReservation(seeker);
}

void replace_house(Building &from, const Building &to)
{
    if (!from.id || !to.id || from.id == to.id) {
        return;
    }

    g_runtime_state.cancelReservationsForBuilding(from);
    g_runtime_state.replaceHouse(from.id, to.id);
    clamp_allocation_table();
    rebuild_counters_from_allocations();
}

int spawn_acquisition(Building &workplace, const map_point *road)
{
    if (!uses_active_workforce(workplace) || !road) {
        return 0;
    }
    if (access_workers_for_workplace(workplace) >= required_workers(workplace)) {
        return 0;
    }
    if (labor_seeker_slot_is_busy(workplace)) {
        return 0;
    }

    LocalWorkforceRouteAccessContext context = g_runtime_state.routeAccessContext();
    const RouteAccessSelector selector =
        RouteAccessSelector::fromRoad(
            *road,
            labor_seeker_max_roam_length(),
            labor_seeker_route_policy(kLaborSeekerAcquireProfile),
            context);
    const HouseRouteSelection target = selector.nearestUnemployedHouse();
    if (!target) {
        return 0;
    }
    return create_labor_seeker(workplace, road, target, kLaborSeekerTripAcquire);
}

int spawn_validation(Building &workplace, const map_point *road)
{
    if (!uses_active_workforce(workplace) || !road) {
        return 0;
    }
    building *record = const_cast<building *>(workplace.record());
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

    LocalWorkforceRouteAccessContext context = g_runtime_state.routeAccessContext();
    const RouteAccessSelector selector =
        RouteAccessSelector::fromRoad(
            *road,
            labor_seeker_max_roam_length(),
            labor_seeker_route_policy(kLaborSeekerValidateProfile),
            context);
    const HouseRouteSelection target = selector.nearestAssignedSourceReleasingUnreachable(workplace);
    if (!target) {
        return 0;
    }
    return create_labor_seeker(workplace, road, target, kLaborSeekerTripValidate);
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
    LaborReservation *reservation = g_runtime_state.reservationFor(*f);
    if (f->collecting_item_id == kLaborSeekerTripValidate && f->building && reservation) {
        ::release_workplace_source(f->building->id, reservation->house.id);
    }
    g_runtime_state.cancelReservation(*f);
    if (::retarget_labor_seeker_to_unemployed(f)) {
        return;
    }
    ::retire_labor_seeker(f);
}

} // namespace building_local_workforce
