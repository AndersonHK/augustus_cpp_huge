#include "building/FoundationDef.h"

#include "map/terrain.h"

#include <algorithm>
#include <set>
#include <utility>

namespace building_type_registry_impl {

namespace {

int normalize_rotation(int rotation)
{
    return (rotation % 4 + 4) % 4;
}

template <typename IncludeCell>
std::vector<FoundationPerimeterCell> rotated_perimeter(
    const std::vector<RotatedFoundationCell> &rotated,
    IncludeCell include_cell)
{
    std::set<std::pair<int, int>> occupied;
    for (const RotatedFoundationCell &cell : rotated) {
        occupied.emplace(cell.x, cell.y);
    }

    static const int neighbors[][2] = { { 0, -1 }, { 1, 0 }, { 0, 1 }, { -1, 0 } };
    std::set<std::pair<int, int>> perimeter;
    for (const RotatedFoundationCell &cell : rotated) {
        if (!cell.definition || !include_cell(*cell.definition)) {
            continue;
        }
        for (const auto &neighbor : neighbors) {
            const std::pair<int, int> candidate = {
                cell.x + neighbor[0], cell.y + neighbor[1]
            };
            if (!occupied.count(candidate)) {
                perimeter.insert(candidate);
            }
        }
    }

    std::vector<FoundationPerimeterCell> result;
    result.reserve(perimeter.size());
    for (const auto &cell : perimeter) {
        result.push_back({ cell.first, cell.second });
    }
    std::sort(result.begin(), result.end(), [](const FoundationPerimeterCell &left,
        const FoundationPerimeterCell &right) {
        return left.y != right.y ? left.y < right.y : left.x < right.x;
    });
    return result;
}

} // namespace

FoundationDef::FoundationDef(std::string path)
    : path_(std::move(path))
{
}

const char *FoundationDef::path() const { return path_.c_str(); }
int FoundationDef::width() const { return width_; }
int FoundationDef::height() const { return height_; }
int FoundationDef::rotates() const { return rotates_; }

int FoundationDef::rotated_width(int rotation) const
{
    return normalize_rotation(rotation) % 2 ? height_ : width_;
}

int FoundationDef::rotated_height(int rotation) const
{
    return normalize_rotation(rotation) % 2 ? width_ : height_;
}

uint16_t FoundationDef::default_permissions() const { return default_permissions_; }
uint16_t FoundationDef::configurable_permissions() const { return configurable_permissions_; }
uint32_t FoundationDef::site_requirements() const { return site_requirements_; }
const std::vector<FoundationCellDefinition> &FoundationDef::cells() const { return cells_; }

std::vector<RotatedFoundationCell> FoundationDef::rotated_cells(int rotation) const
{
    const int normalized = normalize_rotation(rotation);
    std::vector<RotatedFoundationCell> result;
    result.reserve(cells_.size());
    for (const FoundationCellDefinition &cell : cells_) {
        RotatedFoundationCell rotated;
        rotated.definition = &cell;
        switch (normalized) {
            case 1:
                rotated.x = cell.y;
                rotated.y = width_ - 1 - cell.x;
                break;
            case 2:
                rotated.x = width_ - 1 - cell.x;
                rotated.y = height_ - 1 - cell.y;
                break;
            case 3:
                rotated.x = height_ - 1 - cell.y;
                rotated.y = cell.x;
                break;
            default:
                rotated.x = cell.x;
                rotated.y = cell.y;
                break;
        }
        result.push_back(rotated);
    }
    return result;
}

std::vector<FoundationPerimeterCell> FoundationDef::rotated_access_perimeter(int rotation) const
{
    const std::vector<RotatedFoundationCell> rotated = rotated_cells(rotation);
    bool has_passage = false;
    for (const RotatedFoundationCell &cell : rotated) {
        has_passage |= cell.definition && cell.definition->passage != FoundationPassage::None;
    }
    return rotated_perimeter(rotated, [has_passage](const FoundationCellDefinition &cell) {
        return !has_passage || cell.passage != FoundationPassage::None;
    });
}

std::vector<FoundationPerimeterCell> FoundationDef::rotated_water_perimeter(int rotation) const
{
    return rotated_perimeter(rotated_cells(rotation), [](const FoundationCellDefinition &cell) {
        return (cell.required_terrain & TERRAIN_WATER) != 0;
    });
}

const FoundationCellDefinition *FoundationDef::cell_at(int x, int y) const
{
    const auto found = std::find_if(cells_.begin(), cells_.end(), [x, y](const FoundationCellDefinition &cell) {
        return cell.x == x && cell.y == y;
    });
    return found == cells_.end() ? nullptr : &*found;
}

void FoundationDef::set_dimensions(int width, int height)
{
    width_ = width;
    height_ = height;
}

void FoundationDef::set_rotates(int rotates) { rotates_ = rotates != 0; }

void FoundationDef::set_permissions(uint16_t default_permissions, uint16_t configurable_permissions)
{
    default_permissions_ = default_permissions;
    configurable_permissions_ = configurable_permissions;
}

void FoundationDef::set_site_requirements(uint32_t requirements) { site_requirements_ = requirements; }

void FoundationDef::add_cell(FoundationCellDefinition cell) { cells_.push_back(std::move(cell)); }

int FoundationDef::has_cells() const { return !cells_.empty(); }

int FoundationDef::has_water_requirement() const
{
    return std::any_of(cells_.begin(), cells_.end(), [](const FoundationCellDefinition &cell) {
        return (cell.required_terrain & TERRAIN_WATER) != 0;
    });
}

int FoundationDef::requires_terrain(uint32_t terrain) const
{
    return terrain && std::any_of(cells_.begin(), cells_.end(), [terrain](const FoundationCellDefinition &cell) {
        return (cell.required_terrain & terrain) != 0;
    });
}

int FoundationDef::adds_terrain(uint32_t terrain) const
{
    return terrain && std::any_of(cells_.begin(), cells_.end(), [terrain](const FoundationCellDefinition &cell) {
        return (cell.added_terrain & terrain) != 0;
    });
}

int FoundationDef::has_owner_controlled_passage() const
{
    return std::any_of(cells_.begin(), cells_.end(), [](const FoundationCellDefinition &cell) {
        return cell.passage == FoundationPassage::OwnerControlled;
    });
}

} // namespace building_type_registry_impl
