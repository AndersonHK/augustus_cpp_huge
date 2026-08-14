#include "building/count.h"
#include "map/aqueduct.h"
#include "map/building.h"
#include "map/image.h"
#include "map/tiles.h"

#include "building/water_access_runtime.h"

#include "assets/assets.h"
#include "building/BuildingGeometry.h"
#include "building/building.h"
#include "building/building_runtime_internal.h"
#include "building/building_type_registry_internal.h"
#include "building/religion.h"
#include "building/water_access_type.h"
#include "core/direction.h"
#include "core/log.h"

#include "building/building_record.h"
#include "building/monument.h"
#include "game/performance_tracker.h"
#include "map/data.h"
#include "map/grid.h"
#include "map/terrain.h"
#include "map/water_navigation.h"
#include "scenario/property.h"
#include "scenario/map.h"


#include <algorithm>
#include <array>
#include <cstdint>
#include <deque>
#include <exception>
#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace {

constexpr int kNoWaterImageOffset = 15;
constexpr const char *kAccessWell = "well";
constexpr const char *kAccessFountain = "fountain";
constexpr const char *kAccessReservoir = "reservoir";
constexpr const char *kAccessLatrines = "latrines";

using building_type_registry_impl::BuildingType;
using building_type_registry_impl::BuildingGeometry;
using building_type_registry_impl::BuildingGeometryCell;
using building_type_registry_impl::BuildingGeometryPoint;
using building_type_registry_impl::WaterAccessDefinition;
using building_type_registry_impl::WaterAccessOrigin;
using building_type_registry_impl::WaterAccessProvideRule;
using building_type_registry_impl::WaterAccessRequirementMode;
using building_type_registry_impl::WaterAccessRequirementRule;
using building_type_registry_impl::WaterAccessRequirementTerm;
using building_type_registry_impl::WaterAccessRequirementTermKind;
using building_type_registry_impl::WaterAccessRequirementWhere;

struct MaskSet {
    std::array<uint8_t, GRID_SIZE * GRID_SIZE> access = {};
    std::array<uint8_t, GRID_SIZE * GRID_SIZE> providers = {};
};

struct PreviewState {
    int active = 0;
    std::array<uint8_t, GRID_SIZE * GRID_SIZE> highlight = {};
};

struct ProviderSnapshot {
    int building_id = 0;
    const BuildingType *definition = nullptr;
    BuildingGeometry geometry;
    int x = 0;
    int y = 0;
    int state = 0;
    int has_workers = 0;
    int active = 0;
};

struct ContributionCounts {
    std::array<std::array<uint16_t, 8>, GRID_SIZE * GRID_SIZE> access = {};
    std::array<std::array<uint16_t, 8>, GRID_SIZE * GRID_SIZE> providers = {};
};

struct RuntimeState {
    MaskSet masks;
    ContributionCounts counts;
    PreviewState preview;
    std::unordered_map<int, ProviderSnapshot> providers;
    std::unordered_set<int> aqueduct_tiles;
    std::unordered_set<int> wet_aqueduct_tiles;
    std::unordered_set<int> dirty_network_tiles;
    std::unordered_set<int> dirty_reservoirs;
    std::vector<int> changed_access_tiles;
    std::vector<int> dirty_buildings;
    std::vector<uint8_t> dirty_building_flags;
    std::deque<int> dirty_providers;
    std::unordered_set<int> queued_providers;
    BuildingGeometry neptune_geometry;
    WaterAccessProvideRule neptune_rule;
    int neptune_active = 0;
    int network_dirty = 0;
    int updating = 0;
    int refreshed = 0;
    int world_loading = 0;
} g_state;

struct PlannedProvider {
    const BuildingType *definition = nullptr;
    BuildingGeometry geometry;
    int origin_x = 0;
    int origin_y = 0;
};

struct SimulationInput {
    std::array<PlannedProvider, 2> planned_providers = {};
    int planned_provider_count = 0;
    int force_planned_provider_access = 0;
};

struct SimulationResult {
    MaskSet masks;
    std::array<uint8_t, GRID_SIZE * GRID_SIZE> wet_aqueduct = {};
};

uint8_t access_mask(const char *text_id)
{
    return building_type_registry_impl::water_access_mask_from_text(text_id);
}

const BuildingType *definition_from_attr(const char *text_id)
{
    return building_type_registry_impl::definition_for_type(building_type_registry_impl::type_from_attr(text_id));
}

const BuildingType *reservoir_definition()
{
    static const BuildingType *definition = nullptr;
    if (!definition) {
        definition = definition_from_attr("reservoir");
    }
    return definition;
}

const BuildingType *aqueduct_definition()
{
    static const BuildingType *definition = nullptr;
    if (!definition) {
        definition = definition_from_attr("aqueduct");
    }
    return definition;
}

const char *text_from_mask(uint8_t mask)
{
    for (uint8_t id = 0; id < 8; id++) {
        const uint8_t bit = static_cast<uint8_t>(1u << id);
        if (mask & bit) {
            return water_access_type_text_from_number_id(id);
        }
    }
    return nullptr;
}

const WaterAccessDefinition *water_definition_for_provider(const BuildingType *definition)
{
    return definition ? &definition->water_access() : nullptr;
}

const WaterAccessProvideRule *primary_provider_rule(const BuildingType *definition)
{
    const WaterAccessDefinition *water = water_definition_for_provider(definition);
    if (!water || water->provide_rules().empty()) {
        return nullptr;
    }
    for (const WaterAccessProvideRule &rule : water->provide_rules()) {
        if (rule.origin == WaterAccessOrigin::Footprint) {
            return &rule;
        }
    }
    return &water->provide_rules().front();
}

// Registry-derived type ranges for the current water runtime.
// They contain no save ids and are discarded by water_access_runtime_reset().
class WaterRuntimeTypeLists {
public:
    void clear()
    {
        provider_types_.clear();
        projected_state_types_.clear();
        open_water_types_.clear();
        initialized_ = false;
    }

    template<typename Visitor>
    void for_each_provider(Visitor &&visitor)
    {
        ensure_current();
        for_each(provider_types_, std::forward<Visitor>(visitor));
    }

    template<typename Visitor>
    void for_each_projected_state(Visitor &&visitor)
    {
        ensure_current();
        for_each(projected_state_types_, std::forward<Visitor>(visitor));
    }

    template<typename Visitor>
    void for_each_open_water(Visitor &&visitor)
    {
        ensure_current();
        for_each(open_water_types_, std::forward<Visitor>(visitor));
    }

private:
    struct Entry {
        building_type type = BUILDING_NONE;
        const BuildingType *definition = nullptr;
        const WaterAccessDefinition *water = nullptr;
    };

    void ensure_current()
    {
        if (initialized_) {
            return;
        }
        for (int value = BUILDING_NONE + 1; value < BUILDING_TYPE_MAX; ++value) {
            const building_type type = static_cast<building_type>(value);
            const BuildingType *definition = building_type_registry_impl::definition_for_type(type);
            const WaterAccessDefinition *water = water_definition_for_provider(definition);
            if (!definition || !water) {
                continue;
            }
            const Entry entry{ type, definition, water };
            if (water->has_provider()) {
                provider_types_.push_back(entry);
            }
            if (definition->has_housing() || water->has_provider() || water->has_requirements() ||
                water->requires_open_water()) {
                projected_state_types_.push_back(entry);
            }
            if (water->requires_open_water()) {
                open_water_types_.push_back(entry);
            }
        }
        initialized_ = true;
    }

    template<typename Visitor>
    static void for_each(const std::vector<Entry> &types, Visitor &&visitor)
    {
        for (const Entry &entry : types) {
            for (Building &building : Building::of_type(entry.type)) {
                visitor(building, *entry.definition, *entry.water);
            }
        }
    }

    std::vector<Entry> provider_types_;
    std::vector<Entry> projected_state_types_;
    std::vector<Entry> open_water_types_;
    bool initialized_ = false;
};

WaterRuntimeTypeLists g_water_runtime_types;

BuildingGeometry planned_geometry(
    const BuildingType *definition,
    int x,
    int y,
    int rotation)
{
    const building_type_registry_impl::FoundationDef *foundation =
        definition ? definition->foundation_def() : nullptr;
    return foundation ? BuildingGeometry::from_foundation(*foundation, x, y, rotation) : BuildingGeometry{};
}

std::vector<int> required_water_offsets(const BuildingGeometry &geometry)
{
    std::vector<int> result;
    if (!geometry.valid()) {
        return result;
    }
    for (const BuildingGeometryCell &cell : geometry.cells()) {
        if (cell.requires_water && map_grid_is_inside(cell.x, cell.y, 1)) {
            result.push_back(map_grid_offset(cell.x, cell.y));
        }
    }
    return result;
}

int add_mask_tile(std::array<uint8_t, GRID_SIZE * GRID_SIZE> &mask, int grid_offset, uint8_t bit)
{
    if (!bit || !map_grid_is_valid_offset(grid_offset)) {
        return 0;
    }
    const uint8_t before = mask[grid_offset];
    mask[grid_offset] = static_cast<uint8_t>(mask[grid_offset] | bit);
    return before != mask[grid_offset];
}

void clear_masks(MaskSet &masks)
{
    masks.access.fill(0);
    masks.providers.fill(0);
}

void clear_preview_state()
{
    g_state.preview.active = 0;
    g_state.preview.highlight.fill(0);
}

void ensure_runtime_refreshed();
void project_changed_state();

int geometry_has_access(
    const std::array<uint8_t, GRID_SIZE * GRID_SIZE> &mask,
    const BuildingGeometry &geometry,
    uint8_t bit)
{
    if (!bit || !geometry.valid()) {
        return 0;
    }
    for (const BuildingGeometryCell &cell : geometry.cells()) {
        if (!map_grid_is_inside(cell.x, cell.y, 1)) {
            continue;
        }
        if (mask[map_grid_offset(cell.x, cell.y)] & bit) {
            return 1;
        }
    }
    return 0;
}

int nodes_have_access(
    const WaterAccessDefinition &water,
    const std::array<uint8_t, GRID_SIZE * GRID_SIZE> &mask,
    int x,
    int y,
    uint8_t bit)
{
    if (!bit) {
        return 0;
    }
    for (const building_type_registry_impl::WaterAccessNode &node : water.requirement_nodes()) {
        const int grid_offset = map_grid_offset(x + node.x, y + node.y);
        if (map_grid_is_valid_offset(grid_offset) && (mask[grid_offset] & bit)) {
            return 1;
        }
    }
    return 0;
}

int has_water_source_access(const BuildingGeometry &geometry)
{
    if (!geometry.valid()) {
        return 0;
    }
    for (const BuildingGeometryCell &cell : geometry.cells()) {
        if (map_grid_is_inside(cell.x, cell.y, 1) &&
            map_terrain_is(map_grid_offset(cell.x, cell.y), TERRAIN_WATER)) {
            return 1;
        }
    }
    for (const BuildingGeometryPoint &candidate : geometry.points_at_distance(1)) {
        if (map_grid_is_inside(candidate.x, candidate.y, 1) &&
            map_terrain_is(map_grid_offset(candidate.x, candidate.y), TERRAIN_WATER)) {
            return 1;
        }
    }
    return 0;
}

int requirement_term_is_satisfied(
    const WaterAccessDefinition &water,
    const WaterAccessRequirementTerm &term,
    const BuildingGeometry &geometry,
    int x,
    int y,
    const MaskSet &masks)
{
    switch (term.kind) {
        case WaterAccessRequirementTermKind::Access:
            return term.where == WaterAccessRequirementWhere::Nodes ?
                nodes_have_access(water, masks.access, x, y, term.mask) :
                geometry_has_access(masks.access, geometry, term.mask);
        case WaterAccessRequirementTermKind::WaterSourceAny:
        case WaterAccessRequirementTermKind::WaterSourceFreshOnly:
            return has_water_source_access(geometry);
        default:
            return 0;
    }
}

int requirement_rule_is_satisfied(
    const WaterAccessDefinition &water,
    const WaterAccessRequirementRule &rule,
    const BuildingGeometry &geometry,
    int x,
    int y,
    const MaskSet &masks)
{
    if (rule.terms.empty()) {
        return 0;
    }
    if (rule.mode == WaterAccessRequirementMode::Any) {
        for (const WaterAccessRequirementTerm &term : rule.terms) {
            if (requirement_term_is_satisfied(water, term, geometry, x, y, masks)) {
                return 1;
            }
        }
        return 0;
    }
    for (const WaterAccessRequirementTerm &term : rule.terms) {
        if (!requirement_term_is_satisfied(water, term, geometry, x, y, masks)) {
            return 0;
        }
    }
    return 1;
}

int requirements_are_satisfied(
    const WaterAccessDefinition &water,
    const BuildingGeometry &geometry,
    int x,
    int y,
    const MaskSet &masks)
{
    if (!geometry.valid()) {
        return 0;
    }
    for (const WaterAccessRequirementRule &rule : water.requirement_rules()) {
        if (!requirement_rule_is_satisfied(water, rule, geometry, x, y, masks)) {
            return 0;
        }
    }
    return 1;
}

int mark_geometry_footprint(
    std::array<uint8_t, GRID_SIZE * GRID_SIZE> &mask,
    const BuildingGeometry &geometry,
    uint8_t bit)
{
    int changed = 0;
    if (!geometry.valid()) {
        return changed;
    }
    for (const BuildingGeometryCell &cell : geometry.cells()) {
        if (map_grid_is_inside(cell.x, cell.y, 1)) {
            changed |= add_mask_tile(mask, map_grid_offset(cell.x, cell.y), bit);
        }
    }
    return changed;
}

int mark_geometry_range(
    std::array<uint8_t, GRID_SIZE * GRID_SIZE> &mask,
    const BuildingGeometry &geometry,
    int range,
    uint8_t bit)
{
    int changed = 0;
    if (!geometry.valid()) {
        return changed;
    }
    for (int distance = 1; distance <= range; ++distance) {
        for (const BuildingGeometryPoint &point : geometry.points_at_distance(distance)) {
            if (map_grid_is_inside(point.x, point.y, 1)) {
                changed |= add_mask_tile(mask, map_grid_offset(point.x, point.y), bit);
            }
        }
    }
    return changed;
}

int mark_rule_from_footprint(
    MaskSet &masks,
    const WaterAccessProvideRule &rule,
    const BuildingGeometry &geometry)
{
    int changed = mark_geometry_footprint(masks.providers, geometry, rule.mask);
    if (rule.range > 0) {
        changed |= mark_geometry_range(masks.access, geometry, rule.range, rule.mask);
    } else {
        changed |= mark_geometry_footprint(masks.access, geometry, rule.mask);
    }
    return changed;
}

int mark_rule_from_nodes(
    MaskSet &masks,
    const WaterAccessDefinition &water,
    const WaterAccessProvideRule &rule,
    int x,
    int y)
{
    int changed = 0;
    for (const building_type_registry_impl::WaterAccessNode &node : water.provider_nodes()) {
        const int grid_offset = map_grid_offset(x + node.x, y + node.y);
        changed |= add_mask_tile(masks.providers, grid_offset, rule.mask);
        if (rule.range > 0) {
            changed |= mark_geometry_range(masks.access, BuildingGeometry::from_world_cells({
                { x + node.x, y + node.y }
            }), rule.range, rule.mask);
        } else {
            changed |= add_mask_tile(masks.access, grid_offset, rule.mask);
        }
    }
    return changed;
}

int mark_provider_rules(
    MaskSet &masks,
    const WaterAccessDefinition &water,
    const BuildingGeometry &geometry,
    int x,
    int y)
{
    int changed = 0;
    for (const WaterAccessProvideRule &rule : water.provide_rules()) {
        if (rule.origin == WaterAccessOrigin::Nodes) {
            changed |= mark_rule_from_nodes(masks, water, rule, x, y);
        } else {
            changed |= mark_rule_from_footprint(masks, rule, geometry);
        }
    }
    return changed;
}

int building_has_required_workers(const Building *building)
{
    if (!building) {
        return 0;
    }
    const int required_workers = building->type ? building->type->required_workers() : 0;
    return required_workers <= 0 || building->worker_count() > 0;
}

void queue_dirty_building(int building_id)
{
    if (building_id <= 0) {
        return;
    }
    if (g_state.dirty_building_flags.size() <= static_cast<size_t>(building_id)) {
        g_state.dirty_building_flags.resize(static_cast<size_t>(building_id) + 1, 0);
    }
    if (!g_state.dirty_building_flags[building_id]) {
        g_state.dirty_building_flags[building_id] = 1;
        g_state.dirty_buildings.push_back(building_id);
    }
}

void queue_provider_evaluation(int building_id)
{
    if (building_id > 0 && g_state.queued_providers.insert(building_id).second) {
        g_state.dirty_providers.push_back(building_id);
    }
}

void queue_building_at_access_tile(int grid_offset)
{
    if (!map_building_exists_at(grid_offset)) {
        return;
    }
    Building *building = &map_building_at(grid_offset);
    if (building->Composition) {
        if (Building *owner = building->Composition->owner()) {
            building = owner;
        }
    }
    queue_dirty_building(building->id);
    if (g_state.providers.contains(building->id)) {
        queue_provider_evaluation(building->id);
    }
}

void adjust_count(
    std::array<std::array<uint16_t, 8>, GRID_SIZE * GRID_SIZE> &counts,
    std::array<uint8_t, GRID_SIZE * GRID_SIZE> &mask,
    int grid_offset,
    uint8_t bits,
    int delta,
    int is_access)
{
    if (!bits || !map_grid_is_valid_offset(grid_offset)) {
        return;
    }
    for (int bit_index = 0; bit_index < 8; ++bit_index) {
        const uint8_t bit = static_cast<uint8_t>(1u << bit_index);
        if (!(bits & bit)) {
            continue;
        }
        uint16_t &count = counts[grid_offset][bit_index];
        const bool was_set = count != 0;
        if (delta > 0) {
            if (count == UINT16_MAX) {
                log_error("Water access contribution count overflow", 0, grid_offset);
                std::terminate();
            }
            ++count;
        } else {
            if (!count) {
                log_error("Water access contribution count underflow", 0, grid_offset);
                std::terminate();
            }
            --count;
        }
        const bool is_set = count != 0;
        if (was_set == is_set) {
            continue;
        }
        if (is_set) {
            mask[grid_offset] = static_cast<uint8_t>(mask[grid_offset] | bit);
        } else {
            mask[grid_offset] = static_cast<uint8_t>(mask[grid_offset] & ~bit);
        }
        if (is_access) {
            g_state.changed_access_tiles.push_back(grid_offset);
            queue_building_at_access_tile(grid_offset);
        }
    }
}

void adjust_geometry_footprint(
    std::array<std::array<uint16_t, 8>, GRID_SIZE * GRID_SIZE> &counts,
    std::array<uint8_t, GRID_SIZE * GRID_SIZE> &mask,
    const BuildingGeometry &geometry,
    uint8_t bits,
    int delta,
    int is_access)
{
    if (!geometry.valid()) {
        return;
    }
    for (const BuildingGeometryCell &cell : geometry.cells()) {
        if (map_grid_is_inside(cell.x, cell.y, 1)) {
            adjust_count(counts, mask, map_grid_offset(cell.x, cell.y), bits, delta, is_access);
        }
    }
}

void adjust_geometry_range(
    const BuildingGeometry &geometry,
    int range,
    uint8_t bits,
    int delta)
{
    if (!geometry.valid()) {
        return;
    }
    for (int distance = 1; distance <= range; ++distance) {
        for (const BuildingGeometryPoint &point : geometry.points_at_distance(distance)) {
            if (map_grid_is_inside(point.x, point.y, 1)) {
                adjust_count(g_state.counts.access, g_state.masks.access,
                    map_grid_offset(point.x, point.y), bits, delta, 1);
            }
        }
    }
}

void adjust_provider_rule(
    const WaterAccessDefinition &water,
    const WaterAccessProvideRule &rule,
    const BuildingGeometry &geometry,
    int x,
    int y,
    int delta)
{
    if (rule.origin == WaterAccessOrigin::Nodes) {
        for (const building_type_registry_impl::WaterAccessNode &node : water.provider_nodes()) {
            const int grid_offset = map_grid_offset(x + node.x, y + node.y);
            adjust_count(g_state.counts.providers, g_state.masks.providers,
                grid_offset, rule.mask, delta, 0);
            if (rule.range > 0) {
                adjust_geometry_range(BuildingGeometry::from_world_cells({{ x + node.x, y + node.y }}),
                    rule.range, rule.mask, delta);
            } else {
                adjust_count(g_state.counts.access, g_state.masks.access,
                    grid_offset, rule.mask, delta, 1);
            }
        }
        return;
    }

    adjust_geometry_footprint(g_state.counts.providers, g_state.masks.providers,
        geometry, rule.mask, delta, 0);
    if (rule.range > 0) {
        adjust_geometry_range(geometry, rule.range, rule.mask, delta);
    } else {
        adjust_geometry_footprint(g_state.counts.access, g_state.masks.access,
            geometry, rule.mask, delta, 1);
    }
}

void adjust_provider_contributions(const ProviderSnapshot &provider, int delta)
{
    if (!provider.definition || !provider.geometry.valid()) {
        return;
    }
    const WaterAccessDefinition &water = provider.definition->water_access();
    for (const WaterAccessProvideRule &rule : water.provide_rules()) {
        adjust_provider_rule(water, rule, provider.geometry, provider.x, provider.y, delta);
    }
}

void adjust_aqueduct_contributions(int grid_offset, int delta)
{
    const BuildingType *definition = aqueduct_definition();
    if (!definition) {
        return;
    }
    const WaterAccessDefinition &water = definition->water_access();
    const BuildingGeometry geometry = planned_geometry(
        definition, map_grid_offset_to_x(grid_offset), map_grid_offset_to_y(grid_offset), 0);
    for (const WaterAccessProvideRule &rule : water.provide_rules()) {
        adjust_provider_rule(water, rule, geometry,
            map_grid_offset_to_x(grid_offset), map_grid_offset_to_y(grid_offset), delta);
    }
}

int mark_planned_providers(
    const SimulationInput &input,
    SimulationResult &result,
    const MaskSet &requirement_masks)
{
    int changed = 0;
    for (int i = 0; i < input.planned_provider_count; i++) {
        const PlannedProvider &planned = input.planned_providers[i];
        if (!planned.geometry.valid()) {
            continue;
        }
        const BuildingType *definition = planned.definition;
        const WaterAccessDefinition *water = water_definition_for_provider(definition);
        if (!water || !water->has_provider()) {
            continue;
        }
        if (input.force_planned_provider_access ||
            !water->has_requirements() ||
            requirements_are_satisfied(
                *water, planned.geometry, planned.origin_x, planned.origin_y, requirement_masks)) {
            changed |= mark_provider_rules(
                result.masks, *water, planned.geometry, planned.origin_x, planned.origin_y);
        }
    }
    return changed;
}

void set_aqueduct_to_no_water(int grid_offset)
{
    map_aqueduct_set_water_access(grid_offset, 0);
    if (map_terrain_is(grid_offset, TERRAIN_HIGHWAY)) {
        map_image_set(grid_offset, map_tiles_highway_get_aqueduct_image(grid_offset));
        return;
    }

    int image_id = map_image_at(grid_offset);
    if (image_id < image_group(GROUP_BUILDING_AQUEDUCT_NO_WATER)) {
        map_image_set(grid_offset, image_id + kNoWaterImageOffset);
    }
}

void set_aqueduct_to_water(int grid_offset)
{
    map_aqueduct_set_water_access(grid_offset, 1);
    int image_id = map_image_at(grid_offset);
    if (map_terrain_is(grid_offset, TERRAIN_HIGHWAY)) {
        map_image_set(grid_offset, map_tiles_highway_get_aqueduct_image(grid_offset));
    } else if (image_id >= image_group(GROUP_BUILDING_AQUEDUCT_NO_WATER)) {
        map_image_set(grid_offset, image_id - kNoWaterImageOffset);
    }
}

void project_aqueduct_state(const SimulationResult &result)
{
    int grid_offset = map_data.start_offset;
    for (int y = 0; y < map_data.height; y++, grid_offset += map_data.border_size) {
        for (int x = 0; x < map_data.width; x++, grid_offset++) {
            if (map_terrain_is(grid_offset, TERRAIN_AQUEDUCT)) {
                set_aqueduct_to_no_water(grid_offset);
            }
        }
    }

    grid_offset = map_data.start_offset;
    for (int y = 0; y < map_data.height; y++, grid_offset += map_data.border_size) {
        for (int x = 0; x < map_data.width; x++, grid_offset++) {
            if (map_terrain_is(grid_offset, TERRAIN_AQUEDUCT) && result.wet_aqueduct[grid_offset]) {
                set_aqueduct_to_water(grid_offset);
            }
        }
    }
}

void project_terrain_ranges(const SimulationResult &result)
{
    const uint8_t reservoir_mask = access_mask(kAccessReservoir);
    const uint8_t fountain_mask = access_mask(kAccessFountain);
    map_terrain_remove_all(TERRAIN_FOUNTAIN_RANGE | TERRAIN_RESERVOIR_RANGE);

    int grid_offset = map_data.start_offset;
    for (int y = 0; y < map_data.height; y++, grid_offset += map_data.border_size) {
        for (int x = 0; x < map_data.width; x++, grid_offset++) {
            if (reservoir_mask ? (result.masks.access[grid_offset] & reservoir_mask) : 0) {
                map_terrain_add(grid_offset, TERRAIN_RESERVOIR_RANGE);
            }
            if (fountain_mask ? (result.masks.access[grid_offset] & fountain_mask) : 0) {
                map_terrain_add(grid_offset, TERRAIN_FOUNTAIN_RANGE);
            }
        }
    }
}

bool project_building_state_one(Building &building_object, const MaskSet &masks)
{
    building *record = const_cast<::building *>(building_object.record());
    const BuildingType *definition = building_object.type;
    if (!record || !definition || !building_object.is_in_use()) {
        return false;
    }

    bool changed = false;
    BuildingGeometry geometry;
    if (building_object.Housing || definition->water_access().has_requirements()) {
        geometry = BuildingGeometry::query(building_object);
    }
    if (building_object.Housing) {
        const unsigned char well = geometry_has_access(masks.access, geometry, access_mask(kAccessWell)) ? 1 : 0;
        const unsigned char fountain = geometry_has_access(masks.access, geometry, access_mask(kAccessFountain)) ? 1 : 0;
        const unsigned char latrines = geometry_has_access(masks.access, geometry, access_mask(kAccessLatrines)) ? 1 : 0;
        changed |= record->has_well_access != well;
        changed |= record->has_water_access != fountain;
        changed |= record->has_latrines_access != latrines;
        record->has_well_access = well;
        building_object.set_has_water_access(fountain);
        record->has_latrines_access = latrines;
    }

    const WaterAccessDefinition &water = definition->water_access();
    if (water.has_requirements() || water.requires_open_water()) {
        int has_access = building_has_required_workers(&building_object);
        if (has_access && water.has_requirements()) {
            has_access = requirements_are_satisfied(
                water, geometry, building_object.x(), building_object.y(), masks);
        }
        if (has_access && water.requires_open_water()) {
            has_access = water_access_runtime_building_has_open_water_access(&building_object);
        }
        changed |= record->has_water_access != static_cast<unsigned char>(has_access);
        building_object.set_has_water_access(has_access);
    }

    if (definition->attr_is("fountain")) {
        unsigned char upgrade = 0;
        if (record->desirability > 60) {
            upgrade = 3;
        } else if (record->desirability > 40) {
            upgrade = 2;
        } else if (record->desirability > 20) {
            upgrade = 1;
        }
        changed |= record->upgrade_level != upgrade;
        record->upgrade_level = upgrade;
    }
    return changed;
}

bool provider_is_reservoir(const ProviderSnapshot &provider)
{
    return provider.definition && provider.definition->attr_is("reservoir");
}

void queue_network_tile(int grid_offset)
{
    if (map_grid_is_valid_offset(grid_offset)) {
        g_state.dirty_network_tiles.insert(grid_offset);
        g_state.network_dirty = 1;
    }
}

void queue_reservoir_network(const ProviderSnapshot &provider)
{
    if (!provider_is_reservoir(provider)) {
        return;
    }
    g_state.dirty_reservoirs.insert(provider.building_id);
    for (const building_type_registry_impl::WaterAccessNode &node :
            provider.definition->water_access().provider_nodes()) {
        queue_network_tile(map_grid_offset(provider.x + node.x, provider.y + node.y));
    }
    g_state.network_dirty = 1;
}

bool provider_can_participate(const ProviderSnapshot &provider)
{
    return provider.state == BUILDING_STATE_IN_USE && provider.has_workers &&
        provider.definition && provider.geometry.valid();
}

void set_provider_active(ProviderSnapshot &provider, bool active)
{
    if (provider.active == static_cast<int>(active)) {
        return;
    }
    if (provider.active) {
        adjust_provider_contributions(provider, -1);
    }
    provider.active = active ? 1 : 0;
    if (provider.active) {
        adjust_provider_contributions(provider, 1);
    }
    queue_dirty_building(provider.building_id);
}

void update_neptune_contribution()
{
    Building *temple = building_monument_gt_module_is_active(NEPTUNE_MODULE_2_CAPACITY_AND_WATER) ?
        grand_temple_for_god(GOD_NEPTUNE, false) : nullptr;
    const bool active = temple && temple->is_in_use();
    const BuildingGeometry geometry = active ? BuildingGeometry::query(*temple) : BuildingGeometry{};
    const BuildingType *reservoir = reservoir_definition();
    WaterAccessProvideRule desired_rule;
    if (active && reservoir) {
        desired_rule.mask = access_mask(kAccessReservoir);
        desired_rule.range = water_access_runtime_range_for_building(reservoir);
        desired_rule.origin = WaterAccessOrigin::Footprint;
    }
    const bool same_source = g_state.neptune_active == static_cast<int>(active) &&
        (!active || (g_state.neptune_geometry.valid() && temple &&
            g_state.neptune_geometry.owner() == temple &&
            g_state.neptune_rule.mask == desired_rule.mask &&
            g_state.neptune_rule.range == desired_rule.range &&
            g_state.neptune_rule.origin == desired_rule.origin));
    if (same_source) {
        return;
    }

    if (g_state.neptune_active && reservoir) {
        adjust_provider_rule(reservoir->water_access(), g_state.neptune_rule,
            g_state.neptune_geometry, 0, 0, -1);
    }
    g_state.neptune_active = active ? 1 : 0;
    g_state.neptune_geometry = geometry;
    g_state.neptune_rule = desired_rule;
    if (active && reservoir) {
        adjust_provider_rule(reservoir->water_access(), g_state.neptune_rule,
            g_state.neptune_geometry, 0, 0, 1);
    }
}

void rebuild_cached_water_network()
{
    std::unordered_map<int, std::vector<int>> reservoirs_by_connector;
    for (auto &[id, provider] : g_state.providers) {
        if (!provider_is_reservoir(provider)) {
            continue;
        }
        if (!provider_can_participate(provider)) {
            continue;
        }
        for (const building_type_registry_impl::WaterAccessNode &node :
                provider.definition->water_access().provider_nodes()) {
            reservoirs_by_connector[map_grid_offset(provider.x + node.x, provider.y + node.y)].push_back(id);
        }
    }

    std::unordered_set<int> visited_aqueducts;
    std::unordered_set<int> visited_reservoirs;

    auto set_aqueduct_active = [](int offset, bool active) {
        const bool was_active = g_state.wet_aqueduct_tiles.contains(offset);
        if (was_active == active) {
            return;
        }
        if (active) {
            g_state.wet_aqueduct_tiles.insert(offset);
            adjust_aqueduct_contributions(offset, 1);
            set_aqueduct_to_water(offset);
        } else {
            g_state.wet_aqueduct_tiles.erase(offset);
            adjust_aqueduct_contributions(offset, -1);
            if (map_terrain_is(offset, TERRAIN_AQUEDUCT)) {
                set_aqueduct_to_no_water(offset);
            } else {
                map_aqueduct_set_water_access(offset, 0);
            }
        }
    };

    auto visit_component = [&](int first_aqueduct, int first_reservoir) {
        std::deque<int> aqueduct_queue;
        std::deque<int> reservoir_queue;
        std::vector<int> component_aqueducts;
        std::vector<int> component_reservoirs;
        bool supplied = false;
        if (first_aqueduct >= 0) {
            aqueduct_queue.push_back(first_aqueduct);
        }
        if (first_reservoir > 0) {
            reservoir_queue.push_back(first_reservoir);
        }

        while (!aqueduct_queue.empty() || !reservoir_queue.empty()) {
            while (!aqueduct_queue.empty()) {
                const int offset = aqueduct_queue.front();
                aqueduct_queue.pop_front();
                if (!g_state.aqueduct_tiles.contains(offset) || !visited_aqueducts.insert(offset).second) {
                    continue;
                }
                component_aqueducts.push_back(offset);
                for (int direction = 0; direction < 8; direction += 2) {
                    const int neighbor = offset + map_grid_direction_delta(direction);
                    if (g_state.aqueduct_tiles.contains(neighbor) && !visited_aqueducts.contains(neighbor)) {
                        aqueduct_queue.push_back(neighbor);
                    }
                }
                const auto connector = reservoirs_by_connector.find(offset);
                if (connector != reservoirs_by_connector.end()) {
                    for (int reservoir_id : connector->second) {
                        if (!visited_reservoirs.contains(reservoir_id)) {
                            reservoir_queue.push_back(reservoir_id);
                        }
                    }
                }
            }

            while (!reservoir_queue.empty()) {
                const int reservoir_id = reservoir_queue.front();
                reservoir_queue.pop_front();
                if (!visited_reservoirs.insert(reservoir_id).second) {
                    continue;
                }
                auto found = g_state.providers.find(reservoir_id);
                if (found == g_state.providers.end() || !provider_can_participate(found->second)) {
                    continue;
                }
                ProviderSnapshot &provider = found->second;
                component_reservoirs.push_back(reservoir_id);
                supplied = supplied || has_water_source_access(provider.geometry);
                for (const building_type_registry_impl::WaterAccessNode &node :
                        provider.definition->water_access().provider_nodes()) {
                    const int connector = map_grid_offset(provider.x + node.x, provider.y + node.y);
                    if (g_state.aqueduct_tiles.contains(connector) && !visited_aqueducts.contains(connector)) {
                        aqueduct_queue.push_back(connector);
                    }
                }
            }
        }

        for (int offset : component_aqueducts) {
            set_aqueduct_active(offset, supplied);
        }
        for (int reservoir_id : component_reservoirs) {
            auto found = g_state.providers.find(reservoir_id);
            if (found != g_state.providers.end()) {
                set_provider_active(found->second, supplied);
            }
        }
    };

    for (int reservoir_id : g_state.dirty_reservoirs) {
        auto found = g_state.providers.find(reservoir_id);
        if (found != g_state.providers.end() && !provider_can_participate(found->second)) {
            set_provider_active(found->second, false);
        }
    }
    for (int offset : g_state.dirty_network_tiles) {
        if (!g_state.aqueduct_tiles.contains(offset)) {
            set_aqueduct_active(offset, false);
        } else if (!visited_aqueducts.contains(offset)) {
            visit_component(offset, 0);
        }
    }
    for (int reservoir_id : g_state.dirty_reservoirs) {
        if (!visited_reservoirs.contains(reservoir_id)) {
            visit_component(-1, reservoir_id);
        }
    }
    g_state.dirty_network_tiles.clear();
    g_state.dirty_reservoirs.clear();
    g_state.network_dirty = 0;
}

void settle_non_network_providers()
{
    const size_t limit = (g_state.providers.size() + 1) * (g_state.providers.size() + 1);
    size_t evaluations = 0;
    while (!g_state.dirty_providers.empty()) {
        const int id = g_state.dirty_providers.front();
        g_state.dirty_providers.pop_front();
        g_state.queued_providers.erase(id);
        auto found = g_state.providers.find(id);
        if (found == g_state.providers.end() || provider_is_reservoir(found->second)) {
            continue;
        }
        ProviderSnapshot &provider = found->second;
        bool active = false;
        if (provider_can_participate(provider)) {
            const WaterAccessDefinition &water = provider.definition->water_access();
            active = requirements_are_satisfied(
                water, provider.geometry, provider.x, provider.y, g_state.masks);
            if (active && water.requires_open_water()) {
                Building *building = Building::get(id);
                active = building && water_access_runtime_building_has_open_water_access(building);
            }
        }
        set_provider_active(provider, active);
        if (++evaluations > limit) {
            log_error("Water provider dependency graph did not converge", 0,
                static_cast<int>(g_state.providers.size()));
            std::terminate();
        }
    }
}

void sync_provider(Building &building)
{
    const BuildingType *definition = building.type;
    const bool is_provider = definition && definition->water_access().has_provider();
    auto found = g_state.providers.find(building.id);
    if (!is_provider) {
        if (found != g_state.providers.end()) {
            queue_reservoir_network(found->second);
            if (found->second.active) {
                adjust_provider_contributions(found->second, -1);
            }
            g_state.providers.erase(found);
        }
        return;
    }

    const BuildingGeometry geometry = BuildingGeometry::query(building);
    const int state = building.state_id();
    const int has_workers = building_has_required_workers(&building);
    if (found == g_state.providers.end()) {
        ProviderSnapshot snapshot;
        snapshot.building_id = building.id;
        snapshot.definition = definition;
        snapshot.geometry = geometry;
        snapshot.x = building.x();
        snapshot.y = building.y();
        snapshot.state = state;
        snapshot.has_workers = has_workers;
        g_state.providers.emplace(building.id, std::move(snapshot));
        queue_reservoir_network(g_state.providers.at(building.id));
        queue_provider_evaluation(building.id);
        queue_dirty_building(building.id);
        return;
    }

    ProviderSnapshot &snapshot = found->second;
    const bool geometry_changed = snapshot.definition != definition || snapshot.x != building.x() ||
        snapshot.y != building.y();
    const bool activity_input_changed = snapshot.state != state || snapshot.has_workers != has_workers;
    if (geometry_changed) {
        queue_reservoir_network(snapshot);
        if (snapshot.active) {
            adjust_provider_contributions(snapshot, -1);
            snapshot.active = 0;
        }
        snapshot.definition = definition;
        snapshot.geometry = geometry;
        snapshot.x = building.x();
        snapshot.y = building.y();
        queue_reservoir_network(snapshot);
        queue_provider_evaluation(building.id);
    }
    snapshot.state = state;
    snapshot.has_workers = has_workers;
    if (activity_input_changed) {
        queue_reservoir_network(snapshot);
        queue_provider_evaluation(building.id);
        queue_dirty_building(building.id);
    }
}

void seed_provider_cache()
{
    g_water_runtime_types.for_each_provider([&](
        Building &building,
        const BuildingType &,
        const WaterAccessDefinition &) {
        sync_provider(building);
    });
}

void refresh_water_graphic(Building *building, const BuildingType *definition)
{
    if (!building || !definition || !definition->has_graphic() ||
        (!definition->water_access().has_requirements() &&
            !definition->water_access().requires_open_water())) {
        return;
    }
    building->refresh_graphic();
}

void project_building_state(const SimulationResult &result)
{
    g_water_runtime_types.for_each_projected_state([&](
        Building &building_object,
        const BuildingType &definition,
        const WaterAccessDefinition &) {
        if (project_building_state_one(building_object, result.masks)) {
            refresh_water_graphic(&building_object, &definition);
        }
    });
}

void project_changed_state()
{
    const uint8_t reservoir_mask = access_mask(kAccessReservoir);
    const uint8_t fountain_mask = access_mask(kAccessFountain);
    std::sort(g_state.changed_access_tiles.begin(), g_state.changed_access_tiles.end());
    g_state.changed_access_tiles.erase(
        std::unique(g_state.changed_access_tiles.begin(), g_state.changed_access_tiles.end()),
        g_state.changed_access_tiles.end());
    for (int grid_offset : g_state.changed_access_tiles) {
        if (reservoir_mask && (g_state.masks.access[grid_offset] & reservoir_mask)) {
            map_terrain_add(grid_offset, TERRAIN_RESERVOIR_RANGE);
        } else {
            map_terrain_remove(grid_offset, TERRAIN_RESERVOIR_RANGE);
        }
        if (fountain_mask && (g_state.masks.access[grid_offset] & fountain_mask)) {
            map_terrain_add(grid_offset, TERRAIN_FOUNTAIN_RANGE);
        } else {
            map_terrain_remove(grid_offset, TERRAIN_FOUNTAIN_RANGE);
        }
    }

    for (int building_id : g_state.dirty_buildings) {
        if (Building *building = Building::get(building_id)) {
            const BuildingType *definition = building->type;
            if (definition && project_building_state_one(*building, g_state.masks)) {
                refresh_water_graphic(building, definition);
            }
        }
        if (building_id > 0 && static_cast<size_t>(building_id) < g_state.dirty_building_flags.size()) {
            g_state.dirty_building_flags[building_id] = 0;
        }
    }
    g_state.changed_access_tiles.clear();
    g_state.dirty_buildings.clear();
}

void build_preview_highlight(const BuildingType *definition, const SimulationResult &preview_result)
{
    g_state.preview.highlight.fill(0);
    g_state.preview.active = 0;

    uint8_t bit = 0;
    if (const WaterAccessProvideRule *rule = primary_provider_rule(definition)) {
        bit = rule->mask;
    }
    if (!bit) {
        return;
    }

    g_state.preview.active = 1;
    for (size_t i = 0; i < g_state.preview.highlight.size(); i++) {
        if ((g_state.masks.access[i] & bit) || (g_state.masks.providers[i] & bit) ||
            (preview_result.masks.access[i] & bit) || (preview_result.masks.providers[i] & bit)) {
            g_state.preview.highlight[i] = 1;
        }
    }
}

void ensure_runtime_refreshed()
{
    if (g_state.world_loading) {
        return;
    }
    if (!g_state.refreshed) {
        water_access_runtime_refresh();
    } else if (!g_state.updating) {
        water_access_runtime_update();
    }
}

} // namespace

void water_access_runtime_reset(void)
{
    g_water_runtime_types.clear();
    clear_masks(g_state.masks);
    g_state.counts.access.fill({});
    g_state.counts.providers.fill({});
    clear_preview_state();
    g_state.providers.clear();
    g_state.aqueduct_tiles.clear();
    g_state.wet_aqueduct_tiles.clear();
    g_state.dirty_network_tiles.clear();
    g_state.dirty_reservoirs.clear();
    g_state.changed_access_tiles.clear();
    g_state.dirty_buildings.clear();
    g_state.dirty_building_flags.clear();
    g_state.dirty_providers.clear();
    g_state.queued_providers.clear();
    g_state.neptune_geometry = {};
    g_state.neptune_rule = {};
    g_state.neptune_active = 0;
    g_state.network_dirty = 0;
    g_state.updating = 0;
    g_state.refreshed = 0;
    g_state.world_loading = 0;
}

void water_access_runtime_begin_world_load(void)
{
    water_access_runtime_reset();
    g_state.world_loading = 1;
}

void water_access_runtime_finish_world_load(void)
{
    g_state.world_loading = 0;
    water_access_runtime_refresh();
}

void water_access_runtime_refresh(void)
{
    if (g_state.updating || g_state.world_loading) {
        return;
    }
    g_state.updating = 1;
    clear_masks(g_state.masks);
    g_state.counts.access.fill({});
    g_state.counts.providers.fill({});
    g_state.providers.clear();
    g_state.aqueduct_tiles.clear();
    g_state.wet_aqueduct_tiles.clear();
    g_state.dirty_network_tiles.clear();
    g_state.dirty_reservoirs.clear();
    g_state.changed_access_tiles.clear();
    g_state.dirty_buildings.clear();
    g_state.dirty_building_flags.clear();
    g_state.dirty_providers.clear();
    g_state.queued_providers.clear();
    g_state.neptune_geometry = {};
    g_state.neptune_rule = {};
    g_state.neptune_active = 0;
    g_state.network_dirty = 0;
    g_state.refreshed = 1;

    int grid_offset = map_data.start_offset;
    for (int y = 0; y < map_data.height; ++y, grid_offset += map_data.border_size) {
        for (int x = 0; x < map_data.width; ++x, ++grid_offset) {
            if (map_terrain_is(grid_offset, TERRAIN_AQUEDUCT)) {
                g_state.aqueduct_tiles.insert(grid_offset);
                queue_network_tile(grid_offset);
            }
        }
    }
    seed_provider_cache();
    update_neptune_contribution();
    rebuild_cached_water_network();
    settle_non_network_providers();
    auto result = std::make_unique<SimulationResult>();
    result->masks = g_state.masks;
    for (int offset : g_state.wet_aqueduct_tiles) {
        result->wet_aqueduct[offset] = 1;
    }
    project_aqueduct_state(*result);
    project_terrain_ranges(*result);
    project_building_state(*result);
    g_state.changed_access_tiles.clear();
    g_state.dirty_buildings.clear();
    std::fill(g_state.dirty_building_flags.begin(), g_state.dirty_building_flags.end(), uint8_t{0});
    g_state.updating = 0;
}

void water_access_runtime_update(void)
{
    if (g_state.world_loading) {
        return;
    }
    if (!g_state.refreshed) {
        water_access_runtime_refresh();
        return;
    }
    if (g_state.updating) {
        return;
    }
    g_state.updating = 1;
    update_neptune_contribution();
    if (g_state.network_dirty) {
        rebuild_cached_water_network();
    }
    settle_non_network_providers();
    project_changed_state();
    g_state.updating = 0;
}

void water_access_runtime_refresh_building(Building *building)
{
    if (!building || g_state.world_loading) {
        return;
    }
    ensure_runtime_refreshed();
    if (g_state.updating) {
        queue_dirty_building(building->id);
        return;
    }
    g_state.updating = 1;
    sync_provider(*building);
    queue_dirty_building(building->id);
    update_neptune_contribution();
    if (g_state.network_dirty) {
        rebuild_cached_water_network();
    }
    settle_non_network_providers();
    project_changed_state();
    g_state.updating = 0;
}

void water_access_runtime_building_changed(Building *building)
{
    if (!building || !g_state.refreshed || g_state.world_loading) {
        return;
    }
    sync_provider(*building);
    queue_dirty_building(building->id);
}

void water_access_runtime_remove_building(Building *building)
{
    if (!building || !g_state.refreshed || g_state.updating || g_state.world_loading) {
        return;
    }
    auto found = g_state.providers.find(building->id);
    if (found == g_state.providers.end()) {
        return;
    }
    g_state.updating = 1;
    queue_reservoir_network(found->second);
    if (found->second.active) {
        adjust_provider_contributions(found->second, -1);
    }
    g_state.providers.erase(found);
    if (g_state.network_dirty) {
        rebuild_cached_water_network();
    }
    settle_non_network_providers();
    project_changed_state();
    g_state.updating = 0;
}

void water_access_runtime_terrain_changed(int grid_offset, int old_terrain, int new_terrain)
{
    const int changed = old_terrain ^ new_terrain;
    if (!(changed & (TERRAIN_AQUEDUCT | TERRAIN_WATER)) || !g_state.refreshed || g_state.world_loading) {
        return;
    }
    if (changed & TERRAIN_AQUEDUCT) {
        if (new_terrain & TERRAIN_AQUEDUCT) {
            g_state.aqueduct_tiles.insert(grid_offset);
        } else {
            g_state.aqueduct_tiles.erase(grid_offset);
        }
        queue_network_tile(grid_offset);
        for (int direction = 0; direction < 8; direction += 2) {
            queue_network_tile(grid_offset + map_grid_direction_delta(direction));
        }
    }

    const int changed_x = map_grid_offset_to_x(grid_offset);
    const int changed_y = map_grid_offset_to_y(grid_offset);
    for (const auto &[id, provider] : g_state.providers) {
        if (!provider_is_reservoir(provider)) {
            continue;
        }
        bool affected = (changed & TERRAIN_WATER) &&
            provider.geometry.contains_within_range(changed_x, changed_y, 1);
        if (changed & TERRAIN_AQUEDUCT) {
            for (const building_type_registry_impl::WaterAccessNode &node :
                    provider.definition->water_access().provider_nodes()) {
                if (map_grid_offset(provider.x + node.x, provider.y + node.y) == grid_offset) {
                    affected = true;
                    break;
                }
            }
        }
        if (affected) {
            queue_reservoir_network(provider);
        }
    }
}

int water_access_runtime_range_for_building(const BuildingType *definition)
{
    const WaterAccessProvideRule *rule = primary_provider_rule(definition);
    int radius = rule ? rule->range : 0;
    const uint8_t mask = rule ? rule->mask : 0;

    if (mask & access_mask(kAccessWell)) {
        if (grand_temple_for_god(GOD_NEPTUNE, true)) {
            radius++;
        }
    } else if (mask & access_mask(kAccessFountain)) {
        if (scenario_property_climate() == CLIMATE_DESERT) {
            radius--;
        }
        if (grand_temple_for_god(GOD_NEPTUNE, true)) {
            radius++;
        }
    } else if (mask & access_mask(kAccessReservoir)) {
        if (grand_temple_for_god(GOD_NEPTUNE, true)) {
            radius += 2;
        }
    }
    return radius < 0 ? 0 : radius;
}

const char *water_access_runtime_primary_provider_access_text(const BuildingType *definition)
{
    const WaterAccessProvideRule *rule = primary_provider_rule(definition);
    return rule ? text_from_mask(rule->mask) : nullptr;
}

int water_access_runtime_building_type_provides_access(const BuildingType *definition)
{
    const WaterAccessDefinition *water = water_definition_for_provider(definition);
    return water ? water->has_provider() : 0;
}

int water_access_runtime_building_type_provides_access_text(const BuildingType *definition, const char *text_id)
{
    const uint8_t mask = access_mask(text_id);
    if (!mask) {
        return 0;
    }
    const WaterAccessDefinition *water = water_definition_for_provider(definition);
    if (!water) {
        return 0;
    }
    for (const WaterAccessProvideRule &rule : water->provide_rules()) {
        if (rule.mask & mask) {
            return 1;
        }
    }
    return 0;
}

int water_access_runtime_building_type_requires_access_text(const BuildingType *definition, const char *text_id)
{
    const uint8_t mask = access_mask(text_id);
    const WaterAccessDefinition *water = water_definition_for_provider(definition);
    if (!mask || !water) {
        return 0;
    }
    for (const WaterAccessRequirementRule &rule : water->requirement_rules()) {
        for (const WaterAccessRequirementTerm &term : rule.terms) {
            if (term.kind == WaterAccessRequirementTermKind::Access) {
                if (term.mask & mask) {
                    return 1;
                }
            }
        }
    }
    return 0;
}

int water_access_runtime_tile_has_access(int grid_offset, const char *text_id)
{
    ensure_runtime_refreshed();
    if (!map_grid_is_valid_offset(grid_offset)) {
        return 0;
    }
    const uint8_t mask = access_mask(text_id);
    return mask ? (g_state.masks.access[grid_offset] & mask) : 0;
}

int water_access_runtime_building_area_has_access(const Building *building, const char *text_id)
{
    if (!building) {
        return 0;
    }
    ensure_runtime_refreshed();
    return geometry_has_access(
        g_state.masks.access, BuildingGeometry::query(*building), access_mask(text_id));
}

int water_access_runtime_building_type_has_open_water_access_at(
    const BuildingType *definition,
    int x,
    int y,
    int rotation)
{
    if (!definition || !definition->water_access().requires_open_water()) {
        return 1;
    }
    const std::vector<int> water_cells = required_water_offsets(planned_geometry(definition, x, y, rotation));
    return water_navigation::can_reach_adjacent(scenario_map_river_entry(), water_cells) ? 1 : 0;
}

int water_access_runtime_building_has_open_water_access(const Building *building)
{
    if (!building || !building->type) {
        return 0;
    }
    if (!building->type->water_access().requires_open_water()) {
        return 1;
    }
    return water_navigation::can_reach_adjacent(
        scenario_map_river_entry(), required_water_offsets(BuildingGeometry::query(*building))) ? 1 : 0;
}

void water_access_runtime_update_open_water_access(void)
{
    PerformanceTrackerScope lookup_scope(PERFORMANCE_TRACKER_BUCKET_WATER_ACCESS_LOOKUP);
    g_water_runtime_types.for_each_open_water([](
        Building &building,
        const BuildingType &,
        const WaterAccessDefinition &) {
        if (!building.is_in_use()) {
            return;
        }
        building.set_has_water_access(water_access_runtime_building_has_open_water_access(&building));
    });
}

int water_access_runtime_building_has_required_access(const Building *building)
{
    if (!building) {
        return 0;
    }
    ensure_runtime_refreshed();

    const BuildingType *definition = building->type;
    if (!definition) {
        return 0;
    }
    const WaterAccessDefinition &water = definition->water_access();
    if (!water.has_requirements() && !water.requires_open_water()) {
        return 0;
    }
    if (water.has_requirements() && !requirements_are_satisfied(water,
            BuildingGeometry::query(*building), building->x(), building->y(), g_state.masks)) {
        return 0;
    }
    return !water.requires_open_water() || water_access_runtime_building_has_open_water_access(building);
}

int water_access_runtime_building_type_has_required_access_at(
    const BuildingType *definition,
    int x,
    int y,
    int rotation)
{
    ensure_runtime_refreshed();
    if (!definition || !definition->water_access().has_requirements()) {
        return 1;
    }
    return requirements_are_satisfied(
        definition->water_access(), planned_geometry(definition, x, y, rotation), x, y, g_state.masks);
}

int water_access_runtime_reservoir_has_network_access(int grid_offset)
{
    ensure_runtime_refreshed();
    const BuildingType *definition = reservoir_definition();
    if (!definition) {
        return 0;
    }
    return requirements_are_satisfied(
        definition->water_access(),
        planned_geometry(definition, map_grid_offset_to_x(grid_offset), map_grid_offset_to_y(grid_offset), 0),
        map_grid_offset_to_x(grid_offset),
        map_grid_offset_to_y(grid_offset),
        g_state.masks);
}

void water_access_runtime_begin_preview(
    const BuildingType *definition,
    int primary_grid_offset,
    int secondary_grid_offset,
    int primary_rotation,
    int secondary_rotation)
{
    ensure_runtime_refreshed();

    SimulationInput input;
    // Coverage previews answer "what range would this provider emit once working?";
    // ghost graphics still use the real requirement check to show wet/dry state.
    input.force_planned_provider_access = 1;
    if (primary_grid_offset) {
        PlannedProvider &provider = input.planned_providers[input.planned_provider_count++];
        provider.definition = definition;
        provider.origin_x = map_grid_offset_to_x(primary_grid_offset);
        provider.origin_y = map_grid_offset_to_y(primary_grid_offset);
        provider.geometry = planned_geometry(
            definition, provider.origin_x, provider.origin_y, primary_rotation);
    }
    if (secondary_grid_offset) {
        if (secondary_grid_offset != primary_grid_offset && input.planned_provider_count < 2) {
            PlannedProvider &provider = input.planned_providers[input.planned_provider_count++];
            provider.definition = definition;
            provider.origin_x = map_grid_offset_to_x(secondary_grid_offset);
            provider.origin_y = map_grid_offset_to_y(secondary_grid_offset);
            provider.geometry = planned_geometry(
                definition, provider.origin_x, provider.origin_y, secondary_rotation);
        }
    }

    auto preview_result = std::make_unique<SimulationResult>();
    preview_result->masks = g_state.masks;
    mark_planned_providers(input, *preview_result, g_state.masks);
    build_preview_highlight(definition, *preview_result);
}

void water_access_runtime_end_preview(void)
{
    clear_preview_state();
}

int water_access_runtime_tile_has_preview_highlight(int grid_offset)
{
    if (!g_state.preview.active || !map_grid_is_valid_offset(grid_offset)) {
        return 0;
    }
    return g_state.preview.highlight[grid_offset];
}

int water_access_runtime_should_draw_overlay_at(int grid_offset)
{
    if (!map_grid_is_valid_offset(grid_offset)) {
        return 0;
    }
    if (map_terrain_is(grid_offset, TERRAIN_ACCESS_RAMP | TERRAIN_AQUEDUCT)) {
        return 0;
    }
    if (map_terrain_is(grid_offset, TERRAIN_BUILDING)) {
        const Building *building = map_building_exists_at(grid_offset) ? &map_building_at(grid_offset) : nullptr;
        const BuildingType *definition = building ? building->type : nullptr;
        if (definition && definition->water_access().has_provider()) {
            return 0;
        }
        if (definition && definition->is_temple(GOD_NEPTUNE, building_type_registry_impl::ReligionTier::Grand) &&
            building_monument_gt_module_is_active(NEPTUNE_MODULE_2_CAPACITY_AND_WATER)) {
            return 0;
        }
    }
    return 1;
}
