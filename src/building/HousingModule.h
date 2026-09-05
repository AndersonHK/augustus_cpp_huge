#pragma once

#include "building/HousingDef.h"
#include "building/HousingState.h"

class Building;
class Figure;

class HousingModule {
public:
    HousingModule(Building &owner, const building_type_registry_impl::HousingDef *definition, HousingState &state);

    void rebind(const building_type_registry_impl::HousingDef *definition);
    explicit operator bool() const;

    Building &owner();
    const Building &owner() const;
    const building_type_registry_impl::HousingDef &definition() const;
    HousingState &state();
    const HousingState &state() const;

    bool is_occupied() const;
    bool is_occupied_at_compatibility_level(int level) const;
    bool has_plebeian_residents() const;
    bool has_patrician_residents() const;
    const building_type_registry_impl::BuildingType *transition_target(
        building_type_registry_impl::HousingTransitionKind kind) const;
    bool consumes_goods_on_day(int day_of_month) const;
    int mars_offering_amount() const;
    int effective_capacity() const;
    int remaining_capacity() const;
    int add_population(int people);
    int remove_population(int people);
    bool track_immigrant(Figure &figure);
    bool clear_immigrant_if_matches(const Figure &figure);
    void clear_immigrant_reference();
    void remove_immigrant();

private:
    Building *owner_ = nullptr;
    const building_type_registry_impl::HousingDef *definition_ = nullptr;
    HousingState *state_ = nullptr;
};
