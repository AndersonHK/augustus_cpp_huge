#pragma once

#include "building/building.h"
#include "building/workforce_types.h"
#include "core/buffer.h"

#include "map/point.h"

#include <cstddef>
#include <functional>
#include <memory>
#include <vector>

class Figure;

void building_local_workforce_clear(void);
void building_local_workforce_initialize_city(void);

void building_local_workforce_save_state(buffer *buf);
void building_local_workforce_load_state(buffer *buf, int has_saved_state);

namespace building_local_workforce {

struct LaborReservation {
    LaborReservation(Figure &seeker, Building &house, workforce_count workers)
        : seeker(seeker), house(house), workers(workers) {}

    Figure &seeker;
    Building &house;
    const workforce_count workers;
};

class WorkforceAllocationTable {
public:
    using AssignedSourceVisitor = std::function<void(unsigned int house_id, int workers)>;
    using RecordPredicate = std::function<int(unsigned int workplace_id, unsigned int house_id, int workers)>;

    void clear();
    void reserve(size_t records);
    size_t size() const;
    int assignedWorkersForHouse(unsigned int house_id) const;
    int assignedWorkersForWorkplace(unsigned int workplace_id) const;
    void add(unsigned int workplace_id, unsigned int house_id, int workers);
    void appendLoadedRecord(unsigned int workplace_id, unsigned int house_id, int workers);
    void writeSaveRecords(buffer *buf) const;
    void releaseWorkplaceSource(unsigned int workplace_id, unsigned int house_id);
    int releaseFromWorkplace(unsigned int workplace_id, int workers);
    int releaseFromHouse(unsigned int house_id, int workers);
    void replaceHouse(unsigned int from_house_id, unsigned int to_house_id);
    void removeIf(const RecordPredicate &predicate);
    void forEachAssignedSource(unsigned int workplace_id, const AssignedSourceVisitor &visitor) const;

private:
    struct Record {
        unsigned int workplace_id = 0;
        unsigned int house_id = 0;
        int workers = 0;
    };

    int releaseFromRecord(size_t index, int workers);
    void mergeDuplicates();

    std::vector<Record> records_;
};

struct AssignedWorkforceSource {
    unsigned int house_id = 0;
    int workers = 0;
};

class RouteAccessSelectorContext {
public:
    using AssignedSourceVisitor = std::function<void(const AssignedWorkforceSource &)>;

    virtual ~RouteAccessSelectorContext() = default;

    virtual void forEachPopulatedLaborSourceHouse(const std::function<void(Building &)> &visitor) const = 0;
    virtual int houseHasUnemployedWorkers(Building &house) const = 0;
    virtual int usesActiveWorkforce(const Building &workplace) const = 0;
    virtual void forEachAssignedSource(unsigned int workplace_id, const AssignedSourceVisitor &visitor) const = 0;
    virtual void releaseWorkplaceSource(unsigned int workplace_id, unsigned int house_id) const = 0;
};

class LocalWorkforceRouteAccessContext : public RouteAccessSelectorContext {
public:
    explicit LocalWorkforceRouteAccessContext(WorkforceAllocationTable &allocations);

    void forEachPopulatedLaborSourceHouse(const std::function<void(Building &)> &visitor) const override;
    int houseHasUnemployedWorkers(Building &house) const override;
    int usesActiveWorkforce(const Building &building) const override;
    void forEachAssignedSource(
        unsigned int workplace_id,
        const AssignedSourceVisitor &visitor) const override;
    void releaseWorkplaceSource(unsigned int workplace_id, unsigned int house_id) const override;

private:
    WorkforceAllocationTable &allocations_;
};

class LocalWorkforceRuntimeState {
public:
    using AssignedSourceVisitor = WorkforceAllocationTable::AssignedSourceVisitor;
    using RecordPredicate = WorkforceAllocationTable::RecordPredicate;

    void clear();
    void initializeCity();
    void preserveAllocationsForNextCityInitialize();
    LocalWorkforceRouteAccessContext routeAccessContext();

    int assignedWorkersForHouse(unsigned int house_id) const;
    int assignedWorkersForWorkplace(unsigned int workplace_id) const;
    workforce_count reservedWorkersForHouse(const Building &house) const;
    LaborReservation *reservationFor(const Figure &seeker) const;
    LaborReservation &reserve(Figure &seeker, Building &house, workforce_count workers);
    std::unique_ptr<LaborReservation> takeReservation(Figure &seeker);
    void cancelReservation(Figure &seeker);
    void cancelReservationsForBuilding(Building &building);
    void addAllocation(unsigned int workplace_id, unsigned int house_id, int workers);
    void reserveLoadedAllocations(size_t records);
    void appendLoadedAllocation(unsigned int workplace_id, unsigned int house_id, int workers);
    size_t savePayloadSize() const;
    void writeAllocationSavePayload(buffer *buf) const;
    void releaseWorkplaceSource(unsigned int workplace_id, unsigned int house_id);
    int releaseFromWorkplace(unsigned int workplace_id, int workers);
    int releaseFromHouse(unsigned int house_id, int workers);
    void replaceHouse(unsigned int from_house_id, unsigned int to_house_id);
    void removeAllocationsIf(const RecordPredicate &predicate);
    void forEachAssignedSource(unsigned int workplace_id, const AssignedSourceVisitor &visitor) const;

private:
    WorkforceAllocationTable allocations_;
    std::vector<std::unique_ptr<LaborReservation>> reservations_;
    int preserve_allocations_on_next_city_initialize_ = 0;
};

int is_workforce_building(const Building &building);
void refresh_access_scores(void);
void refresh_access_score(Building &building);
int access_score(const Building &building);
int house_available_workers(Building &house);
int labor_seeker_is_workforce(const Figure *f);
void reconcile_house(Building &house);
void remove_building(Building &building);
void change_building(Building &building);
void remove_labor_seeker(Figure &seeker);
void replace_house(Building &from, const Building &to);
int spawn_acquisition(Building &workplace, const map_point *road);
int spawn_validation(Building &workplace, const map_point *road);
int prepare_labor_seeker_target(Figure *f);
void labor_seeker_arrived(Figure *f);
void labor_seeker_failed(Figure *f);

} // namespace building_local_workforce
