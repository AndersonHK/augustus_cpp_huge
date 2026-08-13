#pragma once

#include <string>

namespace building_type_registry_impl {

class BuildingType;

enum class CompositionMembership {
    Standalone,
    Owner,
    Child
};

enum class CompositionTypeReplacementAction {
    NoChange,
    ReplaceStandalone,
    Reject
};

enum class CompositionTypeReplacementError {
    None,
    MissingTarget,
    InconsistentMembership,
    MemberReplacementRequiresOwner,
    WholeCompositionReplacementUnsupported,
    CompositionCreationUnsupported
};

struct CompositionTypeReplacementPlan {
    CompositionTypeReplacementAction action = CompositionTypeReplacementAction::Reject;
    CompositionTypeReplacementError error = CompositionTypeReplacementError::None;
    std::string detail;

    bool accepted() const;
};

// Type replacement currently has no placement result from which it could
// transactionally create, delete, or reposition composition members. The
// planner therefore permits only standalone-to-standalone replacement and
// rejects every structural composition change before runtime mutation.
CompositionTypeReplacementPlan plan_composition_type_replacement(
    const BuildingType *current_type,
    const BuildingType *target_type,
    CompositionMembership membership);

} // namespace building_type_registry_impl
