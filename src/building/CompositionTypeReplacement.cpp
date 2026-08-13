#include "building/CompositionTypeReplacement.h"

#include "building/building_type.h"

namespace building_type_registry_impl {

bool CompositionTypeReplacementPlan::accepted() const
{
    return action != CompositionTypeReplacementAction::Reject;
}

static CompositionTypeReplacementPlan reject_replacement(
    CompositionTypeReplacementError error,
    const char *detail)
{
    CompositionTypeReplacementPlan plan;
    plan.error = error;
    plan.detail = detail ? detail : "composition type replacement was rejected";
    return plan;
}

CompositionTypeReplacementPlan plan_composition_type_replacement(
    const BuildingType *current_type,
    const BuildingType *target_type,
    CompositionMembership membership)
{
    if (!target_type) {
        return reject_replacement(
            CompositionTypeReplacementError::MissingTarget,
            "replacement target has no BuildingType definition");
    }
    if (current_type == target_type) {
        CompositionTypeReplacementPlan plan;
        plan.action = CompositionTypeReplacementAction::NoChange;
        return plan;
    }
    if (membership == CompositionMembership::Child) {
        return reject_replacement(
            CompositionTypeReplacementError::MemberReplacementRequiresOwner,
            "composition child replacement must enter through its owner");
    }
    if (membership == CompositionMembership::Owner) {
        return reject_replacement(
            CompositionTypeReplacementError::WholeCompositionReplacementUnsupported,
            "whole-composition type replacement requires a validated placement transaction");
    }
    if (current_type && current_type->has_composition()) {
        return reject_replacement(
            CompositionTypeReplacementError::InconsistentMembership,
            "composed BuildingType is missing its owner runtime relationship");
    }
    if (target_type->has_composition()) {
        return reject_replacement(
            CompositionTypeReplacementError::CompositionCreationUnsupported,
            "standalone type replacement cannot create composition children");
    }

    CompositionTypeReplacementPlan plan;
    plan.action = CompositionTypeReplacementAction::ReplaceStandalone;
    return plan;
}

} // namespace building_type_registry_impl
