#pragma once

#include "map/TerrainSet.h"
#include <string>
#include <array>
#include <utility>

class TerrainRegistry;
class ImageGroupEntry;
enum class WaterShoreShape { Other, Open, North, East, South, West, Count };
struct TerrainWaterImage {
    std::string group;
    std::vector<std::string> images;
    std::vector<const ImageGroupEntry *> bound_images;
};

struct TerrainTraits {
    bool land = true;
    bool sea = true;
    bool enemy = true;
    bool herd = true;
    bool earthquake = true;
    bool water = false;
    bool blocks_construction = false;
    bool clearable = false;
    bool editable = false;
    bool paintable = false;
};

// Definitions are address-stable and immutable after startup binding. Terrain
// names are for authored data, archive ledgers and inspection, never tile queries.
class Terrain final {
public:
    Terrain(std::string name, TerrainTraits traits) : name_(std::move(name)), traits_(traits) {}
    Terrain(const Terrain &) = delete;
    Terrain &operator=(const Terrain &) = delete;
    Terrain(Terrain &&) = delete;
    Terrain &operator=(Terrain &&) = delete;

    const std::string &name() const { return name_; }
    bool allows_land() const { return traits_.land; }
    bool allows_sea() const { return traits_.sea; }
    bool allows_enemy() const { return traits_.enemy; }
    bool allows_herd() const { return traits_.herd; }
    bool allows_earthquake() const { return traits_.earthquake; }
    bool is_water() const { return traits_.water; }
    bool blocks_construction() const { return traits_.blocks_construction; }
    bool clearable() const { return traits_.clearable; }
    bool editable() const { return traits_.editable; }
    bool paintable() const { return traits_.paintable; }
    const TerrainSet &includes() const { return includes_; }
    int graphics_priority() const { return graphics_priority_; }
    bool has_water_images() const { return std::any_of(water_images_.begin(), water_images_.end(), [](const TerrainWaterImage &image) { return !image.images.empty(); }); }
    const ImageGroupEntry *water_image(WaterShoreShape shape, unsigned variation) const;
    bool exists_at(int grid_offset) const;
    void add_at(int grid_offset) const;
    void remove_at(int grid_offset) const;

private:
    friend class TerrainRegistry;
    std::string name_;
    TerrainTraits traits_;
    TerrainSet includes_;
    int graphics_priority_ = 0;
    std::array<TerrainWaterImage, static_cast<size_t>(WaterShoreShape::Count)> water_images_;
};

// Native systems bind these common roles once. Optional extension roles are
// empty sets when their owning mod is absent. Custom definitions participate in
// traversal and construction collections through their authored properties.
struct TerrainTypes {
    TerrainSet tree, rock, water, building, shrub, garden, road, reservoir_range;
    TerrainSet aqueduct, elevation, access_ramp, meadow, rubble, fountain_range;
    TerrainSet wall, gatehouse, originally_tree;
    TerrainSet highway_top_left, highway_bottom_left, highway_top_right, highway_bottom_right;
    TerrainSet shallow_water;
    std::array<TerrainSet, 4> highway_quadrants;
    std::array<TerrainSet, 8> highway_directions;
    TerrainSet highway, wall_or_gatehouse, not_clear_except_road, not_clear, clearable;
    TerrainSet impassable, impassable_enemy, impassable_herd, impassable_earthquake;
    TerrainSet elevation_rock, all, map_edge, clear, paintable;
};

const TerrainTypes &terrain_types();

inline std::string terrain_names(const TerrainSet &terrains)
{
    std::vector<std::string> names;
    for (const auto *terrain : terrains.entries()) names.push_back(terrain->name());
    std::sort(names.begin(), names.end());
    std::string result;
    for (const auto &name : names) { if (!result.empty()) result += " | "; result += name; }
    return result.empty() ? "none" : result;
}
