#pragma once

#include <stdint.h>

#include <string>
#include <vector>

namespace building_type_registry_impl {

enum class FoundationPassage {
    None,
    Uncontrolled,
    OwnerControlled
};

enum FoundationSiteRequirement : uint32_t {
    FOUNDATION_SITE_MEADOW = 1u << 0,
    FOUNDATION_SITE_ROCK = 1u << 1,
    FOUNDATION_SITE_TREE = 1u << 2,
    FOUNDATION_SITE_WATER = 1u << 3,
    FOUNDATION_SITE_WALL = 1u << 4,
    FOUNDATION_SITE_DISTANT_WATER = 1u << 5
};

struct FoundationCellDefinition {
    char symbol = 0;
    int x = 0;
    int y = 0;
    uint32_t required_terrain = 0;
    uint32_t permitted_blocking_terrain = 0;
    uint32_t added_terrain = 0;
    uint32_t removed_terrain = 0;
    int binds_building = 1;
    FoundationPassage passage = FoundationPassage::None;
};

struct RotatedFoundationCell {
    const FoundationCellDefinition *definition = nullptr;
    int x = 0;
    int y = 0;
};

struct FoundationPerimeterCell {
    int x = 0;
    int y = 0;
};

class FoundationDef {
public:
    explicit FoundationDef(std::string path = {});

    const char *path() const;
    int width() const;
    int height() const;
    int rotates() const;
    int rotated_width(int rotation) const;
    int rotated_height(int rotation) const;
    uint16_t default_permissions() const;
    uint16_t configurable_permissions() const;
    uint32_t site_requirements() const;
    const std::vector<FoundationCellDefinition> &cells() const;
    std::vector<RotatedFoundationCell> rotated_cells(int rotation) const;
    // Cardinal cells immediately outside the active footprint. When passage
    // cells are authored, only their exterior neighbors are entrances.
    std::vector<FoundationPerimeterCell> rotated_access_perimeter(int rotation) const;
    // Cardinal cells immediately outside foundation cells which require
    // water. This is the authoritative waterside interface after rotation.
    std::vector<FoundationPerimeterCell> rotated_water_perimeter(int rotation) const;
    const FoundationCellDefinition *cell_at(int x, int y) const;

    void set_dimensions(int width, int height);
    void set_rotates(int rotates);
    void set_permissions(uint16_t default_permissions, uint16_t configurable_permissions);
    void set_site_requirements(uint32_t requirements);
    void add_cell(FoundationCellDefinition cell);

    int has_cells() const;
    int has_water_requirement() const;
    int requires_terrain(uint32_t terrain) const;
    int adds_terrain(uint32_t terrain) const;
    int has_owner_controlled_passage() const;

private:
    std::string path_;
    int width_ = 0;
    int height_ = 0;
    int rotates_ = 0;
    uint16_t default_permissions_ = 0;
    uint16_t configurable_permissions_ = 0;
    uint32_t site_requirements_ = 0;
    std::vector<FoundationCellDefinition> cells_;
};

} // namespace building_type_registry_impl
