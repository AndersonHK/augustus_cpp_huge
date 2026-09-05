#include "building/HousingModule.h"

#include "building/building.h"
#include "building/HousingProfileDef.h"
#include "building/monument.h"
#include "figure/figure.h"

#include <algorithm>
#include <cassert>

using building_type_registry_impl::HousingResidentClass;

HousingModule::HousingModule(
    Building &owner,
    const building_type_registry_impl::HousingDef *definition,
    HousingState &state)
    : owner_(&owner), definition_(definition), state_(&state)
{
}

void HousingModule::rebind(const building_type_registry_impl::HousingDef *definition)
{
    definition_ = definition;
}

HousingModule::operator bool() const
{
    return definition_ && definition_->profile;
}

Building &HousingModule::owner()
{
    assert(owner_);
    return *owner_;
}

const Building &HousingModule::owner() const
{
    assert(owner_);
    return *owner_;
}

const building_type_registry_impl::HousingDef &HousingModule::definition() const
{
    assert(definition_);
    return *definition_;
}

HousingState &HousingModule::state()
{
    assert(state_);
    return *state_;
}

const HousingState &HousingModule::state() const
{
    assert(state_);
    return *state_;
}

bool HousingModule::is_occupied() const
{
    return *this && owner_ && owner_->is_in_use() && state_->population > 0;
}

bool HousingModule::is_occupied_at_compatibility_level(int level) const
{
    return is_occupied() && definition_->profile->compatibility_level == level;
}

bool HousingModule::has_plebeian_residents() const
{
    return *this && definition_->profile->resident_class_value == HousingResidentClass::Plebeian;
}

bool HousingModule::has_patrician_residents() const
{
    return *this && definition_->profile->resident_class_value == HousingResidentClass::Patrician;
}

const building_type_registry_impl::BuildingType *HousingModule::transition_target(
    building_type_registry_impl::HousingTransitionKind kind) const
{
    return definition_ ? definition_->transition(kind).type : nullptr;
}

bool HousingModule::consumes_goods_on_day(int day_of_month) const
{
    return definition_ && definition_->consumes_goods_on_day(day_of_month);
}

int HousingModule::mars_offering_amount() const
{
    return definition_ ? definition_->mars_offering_amount : 0;
}

int HousingModule::effective_capacity() const
{
    int capacity = definition_ ? definition_->capacity : 0;
    if (state_ && state_->services.temple_neptune &&
        building_monument_gt_module_is_active(NEPTUNE_MODULE_2_CAPACITY_AND_WATER)) {
        capacity += (capacity + 1) / 20;
    }
    return capacity;
}

int HousingModule::remaining_capacity() const
{
    return definition_ ? std::max(0, definition_->capacity - state_->population) : 0;
}

int HousingModule::add_population(int people)
{
    const int added = std::clamp(people, 0, remaining_capacity());
    state_->population += static_cast<int16_t>(added);
    state_->highest_population = std::max(state_->highest_population, state_->population);
    state_->population_room = static_cast<int16_t>(remaining_capacity());
    return added;
}

int HousingModule::remove_population(int people)
{
    const int removed = std::clamp(people, 0, static_cast<int>(state_->population));
    state_->population -= static_cast<int16_t>(removed);
    state_->population_room = static_cast<int16_t>(remaining_capacity());
    return removed;
}

bool HousingModule::track_immigrant(Figure &figure)
{
    if (!owner_ || !state_ || !figure.id() || figure.state != FIGURE_STATE_ALIVE ||
        (figure.type != FIGURE_IMMIGRANT && figure.type != FIGURE_HOMELESS) ||
        figure.immigrant_building != owner_ || figure.destination_building != owner_ ||
        (state_->immigrant_figure_id && state_->immigrant_figure_id != figure.id())) {
        return false;
    }
    state_->immigrant_figure_id = figure.id();
    return true;
}

bool HousingModule::clear_immigrant_if_matches(const Figure &figure)
{
    if (state_ && state_->immigrant_figure_id == figure.id()) {
        clear_immigrant_reference();
        return true;
    }
    return false;
}

void HousingModule::clear_immigrant_reference()
{
    if (state_) {
        state_->immigrant_figure_id = 0;
    }
}

void HousingModule::remove_immigrant()
{
    if (!owner_ || !state_) {
        return;
    }
    const unsigned int figure_id = state_->immigrant_figure_id;
    clear_immigrant_reference();
    if (!figure_id) {
        return;
    }
    Figure *figure = Figure::get(figure_id);
    if (!figure || figure->id() != figure_id || !figure->state ||
        (figure->immigrant_building != owner_ && figure->destination_building != owner_)) {
        return;
    }
    figure->remove();
}
