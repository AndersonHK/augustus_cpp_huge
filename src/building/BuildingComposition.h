#pragma once

#include "building/CompositionDef.h"

#include <cstddef>
#include <functional>
#include <string>
#include <vector>

class Building;

class BuildingComposition {
public:
    static constexpr std::size_t no_definition_index = static_cast<std::size_t>(-1);

    BuildingComposition() = default;

    void bind_standalone(Building *building);
    void bind_owner(Building *building, const building_type_registry_impl::CompositionDef *definition);
    void clear();

    bool attach_children(
        const std::vector<BuildingComposition *> &children,
        std::string *error = nullptr);
    bool complete(std::string *error = nullptr) const;
    void require_complete(const char *operation) const;

    bool is_composed() const;
    bool is_owner() const;
    bool is_child() const;
    Building *building() const;
    Building *owner() const;
    BuildingComposition *owner_module() const;
    const building_type_registry_impl::CompositionDef *definition() const;
    const building_type_registry_impl::CompositionChildDef *child_definition() const;
    std::size_t definition_index() const;
    const std::vector<BuildingComposition *> &children() const;

    // Visits a standalone building or a complete owner/child graph. A published
    // incomplete composition is an invariant violation, never an empty result.
    void for_each_member(const std::function<void(Building &)> &visitor) const;

private:
    Building *building_ = nullptr;
    BuildingComposition *owner_ = nullptr;
    const building_type_registry_impl::CompositionDef *definition_ = nullptr;
    std::size_t definition_index_ = no_definition_index;
    std::vector<BuildingComposition *> children_;
};
