#include "building/CompositionDef.h"

#include "building/building_type.h"

#include <algorithm>
#include <limits>
#include <set>
#include <utility>

namespace building_type_registry_impl {

int normalize_composition_rotation(int rotation)
{
    int normalized = rotation % 4;
    return normalized < 0 ? normalized + 4 : normalized;
}

CompositionOffset rotate_composition_offset(
    CompositionOffset offset,
    int rotation,
    int owner_width,
    int owner_height,
    int child_width,
    int child_height)
{
    if (!offset.defined) {
        return offset;
    }
    switch (normalize_composition_rotation(rotation)) {
        case 1:
            return CompositionOffset{ offset.y, owner_width - child_width - offset.x, true };
        case 2:
            return CompositionOffset{
                owner_width - child_width - offset.x,
                owner_height - child_height - offset.y,
                true
            };
        case 3:
            return CompositionOffset{ owner_height - child_height - offset.y, offset.x, true };
        default:
            return offset;
    }
}

void CompositionChildDef::set_canonical_offset(int x, int y)
{
    canonical_offset_ = CompositionOffset{ x, y, true };
}

void CompositionChildDef::set_rotation_override(int rotation, int x, int y)
{
    rotation_overrides_[normalize_composition_rotation(rotation)] = CompositionOffset{ x, y, true };
}

CompositionOffset CompositionChildDef::offset_for_rotation(
    int rotation,
    int owner_width,
    int owner_height,
    int child_width,
    int child_height) const
{
    const int normalized = normalize_composition_rotation(rotation);
    if (has_complete_rotation_overrides()) {
        return rotation_overrides_[normalized];
    }
    if (has_partial_rotation_overrides()) {
        return CompositionOffset();
    }
    return rotate_composition_offset(
        canonical_offset_, normalized, owner_width, owner_height, child_width, child_height);
}

bool CompositionChildDef::has_canonical_offset() const
{
    return canonical_offset_.defined;
}

CompositionOffset CompositionChildDef::canonical_offset() const
{
    return canonical_offset_;
}

bool CompositionChildDef::has_complete_rotation_overrides() const
{
    return std::all_of(rotation_overrides_.begin(), rotation_overrides_.end(),
        [](const CompositionOffset &offset) { return offset.defined; });
}

bool CompositionChildDef::has_partial_rotation_overrides() const
{
    const bool has_any_override = std::any_of(rotation_overrides_.begin(), rotation_overrides_.end(),
        [](const CompositionOffset &offset) { return offset.defined; });
    return has_any_override && !has_complete_rotation_overrides();
}

CompositionChildDef &CompositionDef::add_child(std::string type_reference, std::string role)
{
    children_.push_back(CompositionChildDef());
    CompositionChildDef &child = children_.back();
    child.type_reference = std::move(type_reference);
    child.role = std::move(role);
    return child;
}

const std::vector<CompositionChildDef> &CompositionDef::children() const
{
    return children_;
}

std::vector<CompositionChildDef> &CompositionDef::children()
{
    return children_;
}

bool CompositionDef::has_any() const
{
    return !children_.empty();
}

int CompositionLayoutBounds::width() const
{
    return max_x - min_x;
}

int CompositionLayoutBounds::height() const
{
    return max_y - min_y;
}

bool CompositionLayoutResult::valid() const
{
    return error == CompositionLayoutError::None;
}

static CompositionLayoutResult layout_error(CompositionLayoutError error, std::string detail)
{
    CompositionLayoutResult result;
    result.error = error;
    result.detail = std::move(detail);
    return result;
}

CompositionLayoutResult build_composition_layout(
    const BuildingType *owner_type,
    const CompositionDef &definition,
    int owner_x,
    int owner_y,
    int owner_rotation,
    const CompositionFoundationResolver &resolve_foundation)
{
    if (!owner_type) {
        return layout_error(CompositionLayoutError::MissingOwner, "composition has no owner type");
    }
    if (!resolve_foundation) {
        return layout_error(CompositionLayoutError::MissingFoundation, "composition has no foundation resolver");
    }

    const int normalized_rotation = normalize_composition_rotation(owner_rotation);
    CompositionLayoutResult result;
    result.members.reserve(definition.children().size() + 1);

    CompositionFoundationView canonical_owner_foundation;
    std::string canonical_owner_error;
    if (!resolve_foundation(*owner_type, 0, &canonical_owner_foundation, &canonical_owner_error) ||
        canonical_owner_foundation.width <= 0 || canonical_owner_foundation.height <= 0) {
        return layout_error(CompositionLayoutError::MissingFoundation,
            "composition owner: " + canonical_owner_error);
    }
    result.members.push_back(CompositionLayoutMember{
        owner_type, "owner", std::numeric_limits<std::size_t>::max(), owner_x, owner_y,
        normalized_rotation, normalized_rotation, true
    });

    std::set<std::string> roles;
    roles.insert("owner");
    for (std::size_t index = 0; index < definition.children().size(); ++index) {
        const CompositionChildDef &child = definition.children()[index];
        if (!child.type) {
            return layout_error(CompositionLayoutError::MissingChildType,
                "composition child '" + child.role + "' has no resolved type");
        }
        if (child.type == owner_type) {
            return layout_error(CompositionLayoutError::SelfReference,
                "composition child '" + child.role + "' references its owner type");
        }
        if (child.type->has_composition()) {
            return layout_error(CompositionLayoutError::NestedComposition,
                "composition child '" + child.role + "' is itself composed");
        }
        if (child.role.empty()) {
            return layout_error(CompositionLayoutError::MissingRole,
                "composition child has no role");
        }
        if (!roles.insert(child.role).second) {
            return layout_error(CompositionLayoutError::DuplicateRole,
                "composition role '" + child.role + "' is duplicated");
        }
        if (child.has_partial_rotation_overrides()) {
            return layout_error(CompositionLayoutError::PartialRotationOverrides,
                "composition child '" + child.role + "' must override all four rotations");
        }
        if (!child.has_canonical_offset()) {
            return layout_error(CompositionLayoutError::MissingOffset,
                "composition child '" + child.role + "' has no canonical offset");
        }
        if (child.has_complete_rotation_overrides()) {
            const CompositionOffset canonical = child.canonical_offset();
            const CompositionOffset rotation_zero = child.offset_for_rotation(0, 1, 1, 1, 1);
            if (canonical.x != rotation_zero.x || canonical.y != rotation_zero.y) {
                return layout_error(CompositionLayoutError::CanonicalOverrideMismatch,
                    "composition child '" + child.role + "' rotation 0 override differs from its canonical offset");
            }
        }

        CompositionFoundationView canonical_child_foundation;
        std::string canonical_child_error;
        if (!resolve_foundation(*child.type, 0, &canonical_child_foundation, &canonical_child_error) ||
            canonical_child_foundation.width <= 0 || canonical_child_foundation.height <= 0) {
            return layout_error(CompositionLayoutError::MissingFoundation,
                "composition child '" + child.role + "': " + canonical_child_error);
        }
        const CompositionOffset offset = child.offset_for_rotation(
            normalized_rotation,
            canonical_owner_foundation.width,
            canonical_owner_foundation.height,
            canonical_child_foundation.width,
            canonical_child_foundation.height);
        // Missing canonical offsets and incomplete override sets were rejected above.
        result.members.push_back(CompositionLayoutMember{
            child.type,
            child.role,
            index,
            owner_x + offset.x,
            owner_y + offset.y,
            normalized_rotation,
            child.orientation == CompositionChildOrientation::InheritOwner ? normalized_rotation : 0,
            false
        });
    }

    std::set<std::pair<int, int>> occupied;
    bool has_bounds = false;
    for (const CompositionLayoutMember &member : result.members) {
        CompositionFoundationView foundation;
        std::string foundation_error;
        if (!resolve_foundation(*member.type, member.foundation_rotation, &foundation, &foundation_error)) {
            return layout_error(CompositionLayoutError::MissingFoundation,
                "composition member '" + member.role + "': " + foundation_error);
        }
        if (foundation.width <= 0 || foundation.height <= 0 || foundation.occupied_cells.empty()) {
            return layout_error(CompositionLayoutError::EmptyFoundation,
                "composition member '" + member.role + "' has an empty foundation");
        }

        for (const CompositionFoundationCell &cell : foundation.occupied_cells) {
            if (cell.x < 0 || cell.y < 0 || cell.x >= foundation.width || cell.y >= foundation.height) {
                return layout_error(CompositionLayoutError::EmptyFoundation,
                    "composition member '" + member.role + "' has an out-of-bounds foundation cell");
            }
            const int world_x = member.x + cell.x;
            const int world_y = member.y + cell.y;
            if (!occupied.insert(std::make_pair(world_x, world_y)).second) {
                return layout_error(CompositionLayoutError::Overlap,
                    "composition member '" + member.role + "' overlaps another member");
            }
            if (!has_bounds) {
                result.bounds = CompositionLayoutBounds{ world_x, world_y, world_x + 1, world_y + 1 };
                has_bounds = true;
            } else {
                result.bounds.min_x = std::min(result.bounds.min_x, world_x);
                result.bounds.min_y = std::min(result.bounds.min_y, world_y);
                result.bounds.max_x = std::max(result.bounds.max_x, world_x + 1);
                result.bounds.max_y = std::max(result.bounds.max_y, world_y + 1);
            }
        }
    }

    return result;
}

bool resolve_composition_foundation(
    const BuildingType &type,
    int rotation,
    CompositionFoundationView *foundation,
    std::string *error)
{
    if (!foundation) {
        if (error) {
            *error = "composition foundation output is missing";
        }
        return false;
    }
    const FoundationDef *definition = type.foundation_def();
    if (!definition) {
        if (error) {
            *error = std::string("building type '") + type.attr() + "' has no resolved foundation";
        }
        return false;
    }

    foundation->width = definition->rotated_width(rotation);
    foundation->height = definition->rotated_height(rotation);
    foundation->occupied_cells.clear();
    const std::vector<RotatedFoundationCell> cells = definition->rotated_cells(rotation);
    foundation->occupied_cells.reserve(cells.size());
    for (const RotatedFoundationCell &cell : cells) {
        foundation->occupied_cells.push_back(CompositionFoundationCell{ cell.x, cell.y });
    }
    return true;
}

CompositionLayoutResult build_composition_layout(
    const BuildingType *owner_type,
    const CompositionDef &definition,
    int owner_x,
    int owner_y,
    int owner_rotation)
{
    return build_composition_layout(
        owner_type,
        definition,
        owner_x,
        owner_y,
        owner_rotation,
        resolve_composition_foundation);
}

} // namespace building_type_registry_impl
