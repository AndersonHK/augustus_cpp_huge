#include "building/CompositionHydration.h"

#include <set>

namespace building_type_registry_impl {

bool CompositionHydrationPlan::valid() const
{
    return error == CompositionHydrationError::None;
}

static bool exact_candidate_match(
    const LoadedCompositionCandidate &candidate,
    const CompositionLayoutMember &expected)
{
    return candidate.live && candidate.type == expected.type &&
        candidate.x == expected.x && candidate.y == expected.y;
}

static std::size_t later_exact_match(
    const LoadedCompositionCandidate &candidate,
    const std::vector<CompositionLayoutMember> &members,
    std::size_t first)
{
    for (std::size_t index = first; index < members.size(); ++index) {
        if (!members[index].is_owner && exact_candidate_match(candidate, members[index])) {
            return index;
        }
    }
    return members.size();
}

CompositionHydrationPlan plan_composition_hydration(
    const CompositionLayoutResult &expected_layout,
    unsigned int owner_id,
    const std::vector<LoadedCompositionCandidate> &saved_chain)
{
    CompositionHydrationPlan plan;
    if (!expected_layout.valid() || expected_layout.members.empty() ||
        !expected_layout.members.front().is_owner) {
        plan.error = CompositionHydrationError::InvalidLayout;
        plan.detail = "composition hydration requires a valid owner-first layout";
        return plan;
    }
    if (!owner_id) {
        plan.error = CompositionHydrationError::InvalidOwner;
        plan.detail = "composition hydration owner has no saved id";
        return plan;
    }

    std::set<unsigned int> ids;
    ids.insert(owner_id);
    for (const LoadedCompositionCandidate &candidate : saved_chain) {
        if (!candidate.id || !ids.insert(candidate.id).second) {
            plan.error = CompositionHydrationError::DuplicateOrCyclicId;
            plan.detail = "composition save chain contains a zero, duplicate, or cyclic id";
            return plan;
        }
    }

    // Legacy warehouses serialize bays in row order, whereas native XML uses
    // role order. Index the owner's trusted chain prefix by exact geometry so
    // existing bays (and their stock) are never replaced because of ordering.
    std::vector<const LoadedCompositionCandidate *> candidates_by_member(expected_layout.members.size(), nullptr);
    std::size_t candidate_index = 0;
    for (; candidate_index < saved_chain.size(); ++candidate_index) {
        const LoadedCompositionCandidate &candidate = saved_chain[candidate_index];
        const std::size_t member_index = later_exact_match(candidate, expected_layout.members, 1);
        if (member_index == expected_layout.members.size() || candidates_by_member[member_index]) {
            plan.warnings.push_back("composition chain reaches an unrelated, non-live, or duplicate role; remaining records were not adopted");
            break;
        }
        candidates_by_member[member_index] = &candidate;
    }

    unsigned int expected_previous_id = owner_id;
    for (std::size_t member_index = 1; member_index < expected_layout.members.size(); ++member_index) {
        const CompositionLayoutMember &expected = expected_layout.members[member_index];
        const LoadedCompositionCandidate *candidate = candidates_by_member[member_index];
        if (!candidate) {
            plan.actions.push_back(CompositionHydrationAction{ CompositionHydrationActionKind::CreateMissing, expected, 0 });
            continue;
        }
        if (candidate->previous_id != expected_previous_id) {
            plan.warnings.push_back(
                "composition child has a stale previous-part id; runtime ownership will replace it");
        }
        plan.actions.push_back(CompositionHydrationAction{
            CompositionHydrationActionKind::AdoptExisting, expected, candidate->id
        });
        expected_previous_id = candidate->id;
    }
    for (; candidate_index < saved_chain.size(); ++candidate_index) {
        plan.unrelated_tail_ids.push_back(saved_chain[candidate_index].id);
    }
    if (!plan.unrelated_tail_ids.empty()) {
        plan.warnings.push_back(
            "composition save chain has undeclared records; they were not adopted by the composition");
    }
    return plan;
}

} // namespace building_type_registry_impl
