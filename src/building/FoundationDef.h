#pragma once

#include "map/TerrainSet.h"

#include <stdint.h>

#include <string>
#include <vector>

namespace building_type_registry_impl {

enum class FoundationPassage {
    None,
    Uncontrolled,
    OwnerControlled
};

struct FoundationCellDefinition {
    char symbol = 0;
    int x = 0;
    int y = 0;
    TerrainSet required_terrain;
    TerrainSet permitted_blocking_terrain;
    TerrainSet added_terrain;
    TerrainSet removed_terrain;
    std::string support_type;
    int binds_building = 1;
    FoundationPassage passage = FoundationPassage::None;
};

struct FoundationProximityRequirement {
    TerrainSet terrain;
    int min_distance = 0;
    int max_distance = 0;
    int min_count = 1;
    bool navigable = false;
    bool sea = false;
    bool match_any = false;
    bool orthogonal_distance = false;
    bool exclude_map_flags = false;
    std::string warning_key;
    std::string name;
    bool placement = true;
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
    bool meets_proximity(const FoundationProximityRequirement &requirement, int x, int y, int rotation) const;
    const std::vector<FoundationProximityRequirement> &proximity_requirements() const { return proximity_requirements_; }
    void add_proximity_requirement(FoundationProximityRequirement requirement) { proximity_requirements_.push_back(requirement); }
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
    void add_cell(FoundationCellDefinition cell);

    int has_cells() const;
    int has_water_requirement() const;
    int requires_terrain(TerrainSet terrain) const;
    int adds_terrain(TerrainSet terrain) const;
    int has_owner_controlled_passage() const;

private:
    std::string path_;
    int width_ = 0;
    int height_ = 0;
    int rotates_ = 0;
    uint16_t default_permissions_ = 0;
    uint16_t configurable_permissions_ = 0;
    std::vector<FoundationProximityRequirement> proximity_requirements_;
    std::vector<FoundationCellDefinition> cells_;
};

} // namespace building_type_registry_impl
