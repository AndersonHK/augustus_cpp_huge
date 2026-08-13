#include "building/BuildingGraphics.h"

#include "building/building.h"
#include "building/building_record.h"
#include "building/building_runtime.h"
#include "core/crash_context.h"

#include <cstdio>
#include <exception>

namespace {

const char *safe_text(const char *text)
{
    return text && *text ? text : "<none>";
}

[[noreturn]] void report_missing_graphics_state(const Building *owner, const char *operation)
{
    const building *record = owner ? owner->record() : nullptr;
    int foundation_width = 0;
    int foundation_height = 0;
    int foundation_cells = 0;
    if (owner && owner->Foundation) {
        const auto &foundation = *owner->Foundation;
        const int rotation = foundation.state().is_published()
            ? foundation.state().rotation()
            : (foundation.definition().rotates() ? owner->orientation() : 0);
        foundation_width = foundation.width(rotation);
        foundation_height = foundation.height(rotation);
        foundation_cells = static_cast<int>(foundation.cells(rotation).size());
    }
    char detail[800];
    std::snprintf(
        detail,
        sizeof(detail),
        "operation=%s record=%p id=%u state=%d type=%s x=%d y=%d grid_offset=%d foundation=%dx%d cells=%d",
        safe_text(operation),
        static_cast<const void *>(record),
        record ? record->id : 0,
        record ? record->state : 0,
        owner && owner->type ? safe_text(owner->type->attr()) : "<none>",
        record ? record->x : 0,
        record ? record->y : 0,
        record ? record->grid_offset : 0,
        foundation_width,
        foundation_height,
        foundation_cells);

    error_context_report_fatal_error_dialog(
        "Building runtime error",
        "BuildingGraphics operation was requested without a BuildingGraphicsState.",
        detail);
    std::terminate();
}

}

BuildingGraphics::BuildingGraphics(
    Building &owner,
    const building_type_registry_impl::BuildingGraphicsDef *definition,
    BuildingGraphicsState *state)
{
    bind(owner, definition, state);
}

void BuildingGraphics::bind(
    Building &owner,
    const building_type_registry_impl::BuildingGraphicsDef *definition,
    BuildingGraphicsState *state)
{
    owner_ = &owner;
    definition_ = definition;
    state_ = state;
}

void BuildingGraphics::bind_owner(Building &owner)
{
    owner_ = &owner;
}

building_runtime *BuildingGraphics::runtime_instance() const
{
    return owner_ ? owner_->runtime_instance() : nullptr;
}

int BuildingGraphics::state_is_runtime_state(building_runtime *runtime) const
{
    return runtime && state_ == &runtime->graphics_state();
}

const building_type_registry_impl::BuildingGraphicsDef *BuildingGraphics::definition() const
{
    return definition_;
}

BuildingGraphicsState *BuildingGraphics::state()
{
    return state_;
}

const BuildingGraphicsState *BuildingGraphics::state() const
{
    return state_;
}

int BuildingGraphics::is_bound() const
{
    return owner_ && definition_ && state_;
}

unsigned char BuildingGraphics::variant() const
{
    if (!state_) {
        report_missing_graphics_state(owner_, "variant");
    }
    return state_->variant();
}

int BuildingGraphics::rotation() const
{
    if (!state_) {
        report_missing_graphics_state(owner_, "rotation");
    }
    if (definition_ && definition_->has_variants()) {
        return state_->variant();
    }
    return owner_ ? owner_->orientation() : 0;
}

int BuildingGraphics::set_variant(int variant)
{
    if (!state_) {
        report_missing_graphics_state(owner_, "set_variant");
    }
    if (building_runtime *runtime = runtime_instance(); state_is_runtime_state(runtime)) {
        const unsigned char before = runtime->graphics_variant();
        runtime->set_graphics_variant(variant);
        return runtime->graphics_variant() != before;
    }
    return state_->set_variant(variant);
}

void BuildingGraphics::assign_variant(int force_reseed)
{
    if (building_runtime *runtime = runtime_instance()) {
        if (!state_is_runtime_state(runtime)) {
            report_missing_graphics_state(owner_, "assign_variant");
        }
        runtime->assign_graphic_variant(force_reseed);
        return;
    }
    report_missing_graphics_state(owner_, "assign_variant");
}

int BuildingGraphics::draw_footprint(const BuildingDrawContext &ctx) const
{
    return is_bound() ? definition_->draw_footprint(*owner_, ctx) : 0;
}

int BuildingGraphics::draw_top(const BuildingDrawContext &ctx) const
{
    return is_bound() ? definition_->draw_top(*owner_, ctx) : 0;
}

int BuildingGraphics::draw_animation(const BuildingDrawContext &ctx) const
{
    return is_bound() ? definition_->draw_animation(*owner_, ctx) : 0;
}

int BuildingGraphics::draw_gatehouse_overlay(const BuildingDrawContext &ctx, int view_orientation) const
{
    return is_bound() ? definition_->draw_gatehouse_overlay(*owner_, ctx, view_orientation) : 0;
}

int BuildingGraphics::uses_terrain_foundation() const
{
    building_runtime *runtime = runtime_instance();
    return is_bound() && runtime && runtime->resolve_graphics_cache() &&
        runtime->cached_graphics_uses_terrain_foundation();
}

int BuildingGraphics::mothball_status_icon_offset(
    int icon_width,
    int icon_height,
    int *x,
    int *y) const
{
    return is_bound() ?
        definition_->mothball_status_icon_offset(*owner_, icon_width, icon_height, x, y) :
        0;
}
