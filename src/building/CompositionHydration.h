#pragma once

#include "building/CompositionDef.h"

#include <string>
#include <vector>

namespace building_type_registry_impl {

struct LoadedCompositionCandidate {
    unsigned int id = 0;
    unsigned int previous_id = 0;
    unsigned int next_id = 0;
    const BuildingType *type = nullptr;
    int x = 0;
    int y = 0;
    bool live = false;
};

enum class CompositionHydrationActionKind {
    AdoptExisting,
    CreateMissing
};

struct CompositionHydrationAction {
    CompositionHydrationActionKind kind = CompositionHydrationActionKind::CreateMissing;
    CompositionLayoutMember expected;
    unsigned int existing_id = 0;
};

enum class CompositionHydrationError {
    None,
    InvalidLayout,
    InvalidOwner,
    DuplicateOrCyclicId
};

struct CompositionHydrationPlan {
    std::vector<CompositionHydrationAction> actions;
    std::vector<unsigned int> unrelated_tail_ids;
    std::vector<std::string> warnings;
    CompositionHydrationError error = CompositionHydrationError::None;
    std::string detail;

    bool valid() const;
};

// Plans save-chain repair without scanning unrelated building records. The
// caller supplies only the records reached by following the owner's saved
// next-part chain. A record is adopted only when its type and position exactly
// identify a declared role. Missing roles become CreateMissing actions;
// records which cannot safely be associated with a declared role are returned
// as an unrelated tail and must not be adopted.
CompositionHydrationPlan plan_composition_hydration(
    const CompositionLayoutResult &expected_layout,
    unsigned int owner_id,
    const std::vector<LoadedCompositionCandidate> &saved_chain);

} // namespace building_type_registry_impl
