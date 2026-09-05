#pragma once

#include <array>
#include <cstddef>
#include <functional>
#include <string>
#include <vector>

namespace building_type_registry_impl {

class BuildingType;

enum class CompositionChildOrientation {
    Canonical,
    InheritOwner
};

struct CompositionOffset {
    int x = 0;
    int y = 0;
    bool defined = false;
};

struct CompositionChildDef {
    std::string type_reference;
    std::string role;
    const BuildingType *type = nullptr;
    CompositionChildOrientation orientation = CompositionChildOrientation::Canonical;

    void set_canonical_offset(int x, int y);
    void set_rotation_override(int rotation, int x, int y);
    CompositionOffset offset_for_rotation(
        int rotation,
        int owner_width,
        int owner_height,
        int child_width,
        int child_height) const;
    CompositionOffset canonical_offset() const;
    bool has_canonical_offset() const;
    bool has_complete_rotation_overrides() const;
    bool has_partial_rotation_overrides() const;

private:
    CompositionOffset canonical_offset_;
    std::array<CompositionOffset, 4> rotation_overrides_;
};

class CompositionDef {
public:
    CompositionChildDef &add_child(std::string type_reference, std::string role);
    const std::vector<CompositionChildDef> &children() const;
    std::vector<CompositionChildDef> &children();
    bool has_any() const;

private:
    std::vector<CompositionChildDef> children_;
};

struct CompositionFoundationCell {
    int x = 0;
    int y = 0;
};

struct CompositionFoundationView {
    int width = 0;
    int height = 0;
    std::vector<CompositionFoundationCell> occupied_cells;
};

using CompositionFoundationResolver = std::function<bool(
    const BuildingType &type,
    int rotation,
    CompositionFoundationView *foundation,
    std::string *error)>;

struct CompositionLayoutMember {
    const BuildingType *type = nullptr;
    std::string role;
    std::size_t definition_index = 0;
    int x = 0;
    int y = 0;
    int foundation_rotation = 0;
    int building_orientation = 0;
    bool is_owner = false;
};

struct CompositionLayoutBounds {
    int min_x = 0;
    int min_y = 0;
    int max_x = 0;
    int max_y = 0;

    int width() const;
    int height() const;
};

enum class CompositionLayoutError {
    None,
    MissingOwner,
    MissingFoundation,
    MissingChildType,
    SelfReference,
    NestedComposition,
    MissingRole,
    DuplicateRole,
    MissingOffset,
    PartialRotationOverrides,
    CanonicalOverrideMismatch,
    EmptyFoundation,
    Overlap
};

struct CompositionLayoutResult {
    std::vector<CompositionLayoutMember> members;
    CompositionLayoutBounds bounds;
    CompositionLayoutError error = CompositionLayoutError::None;
    std::string detail;

    bool valid() const;
};

CompositionLayoutResult build_composition_layout(
    const BuildingType *owner_type,
    const CompositionDef &definition,
    int owner_x,
    int owner_y,
    int owner_rotation,
    const CompositionFoundationResolver &resolve_foundation);

// Default production adapter. BuildingType must have a resolved FoundationDef;
// no model-size or square-footprint fallback is used.
bool resolve_composition_foundation(
    const BuildingType &type,
    int rotation,
    CompositionFoundationView *foundation,
    std::string *error);
CompositionLayoutResult build_composition_layout(
    const BuildingType *owner_type,
    const CompositionDef &definition,
    int owner_x,
    int owner_y,
    int owner_rotation);

int normalize_composition_rotation(int rotation);
CompositionOffset rotate_composition_offset(
    CompositionOffset offset,
    int rotation,
    int owner_width,
    int owner_height,
    int child_width,
    int child_height);

} // namespace building_type_registry_impl
