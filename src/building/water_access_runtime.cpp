#include "building/count.h"
#include "building/house.h"
#include "building/image.h"
#include "map/aqueduct.h"
#include "map/building.h"
#include "map/building_tiles.h"
#include "map/image.h"
#include "map/tiles.h"

#include "building/water_access_runtime.h"

#include "assets/assets.h"
#include "building/building.h"
#include "building/building_type_registry_internal.h"
#include "building/religion.h"
#include "building/water_access_type.h"

#include "building/building_record.h"
#include "building/monument.h"
#include "city/view.h"
#include "map/data.h"
#include "map/grid.h"
#include "map/property.h"
#include "map/terrain.h"
#include "scenario/property.h"


#include <array>
#include <cstdint>
#include <cstring>
#include <vector>

namespace {

constexpr int kNoWaterImageOffset = 15;
constexpr const char *kAccessWell = "well";
constexpr const char *kAccessFountain = "fountain";
constexpr const char *kAccessReservoir = "reservoir";
constexpr const char *kAccessLatrines = "latrines";

using building_type_registry_impl::BuildingType;
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

struct RuntimeState {
    MaskSet masks;
    PreviewState preview;
    int refreshed = 0;
} g_state;

struct PlannedProvider {
    building_type type = BUILDING_NONE;
    int grid_offset = 0;
};

struct SimulationInput {
    std::array<PlannedProvider, 2> planned_providers = {};
    int planned_provider_count = 0;
    int preview_aqueduct_grid_offset = 0;
    int include_constructing_aqueduct = 0;
    int force_planned_provider_access = 0;
};

struct SimulationResult {
    MaskSet masks;
    std::array<uint8_t, GRID_SIZE * GRID_SIZE> wet_aqueduct = {};
};

std::array<uint8_t, GRID_SIZE * GRID_SIZE> *g_range_mask_target = nullptr;
uint8_t g_range_mask_bit = 0;
int g_range_mask_changed = 0;

uint8_t access_mask(const char *text_id)
{
    return building_type_registry_impl::water_access_mask_from_text(text_id);
}

building_type runtime_type_id(const char *text_id)
{
    return building_type_registry_impl::runtime_id_from_text(text_id);
}

building_type reservoir_type_id()
{
    static building_type type = BUILDING_NONE;
    if (type == BUILDING_NONE) {
        type = runtime_type_id("reservoir");
    }
    return type;
}

building_type draggable_reservoir_type_id()
{
    static building_type type = BUILDING_NONE;
    if (type == BUILDING_NONE) {
        type = runtime_type_id("draggable_reservoir");
    }
    return type;
}

building_type aqueduct_type_id()
{
    static building_type type = BUILDING_NONE;
    if (type == BUILDING_NONE) {
        type = runtime_type_id("aqueduct");
    }
    return type;
}

int definition_is(const BuildingType *definition, const char *text_id)
{
    return definition && definition->attr() && std::strcmp(definition->attr(), text_id) == 0;
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

building_type normalize_provider_building_type(building_type type)
{
    return type == draggable_reservoir_type_id() ? reservoir_type_id() : type;
}

const BuildingType *definition_for_provider(building_type type)
{
    type = normalize_provider_building_type(type);
    return type >= BUILDING_NONE && type < BUILDING_TYPE_MAX ?
        building_type_registry_impl::g_building_types[type].get() :
        nullptr;
}

const WaterAccessDefinition *water_definition_for_building_type(building_type type)
{
    const BuildingType *definition = definition_for_provider(type);
    return definition ? &definition->water_access() : nullptr;
}

const WaterAccessProvideRule *primary_provider_rule(building_type type)
{
    const WaterAccessDefinition *water = water_definition_for_building_type(type);
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

int provider_size_for_building_type(building_type type)
{
    const BuildingType *definition = definition_for_provider(type);
    return definition && definition->has_model() ? definition->model().size() : 1;
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

void mark_range_tile(int, int, int grid_offset)
{
    if (!g_range_mask_target || !map_grid_is_valid_offset(grid_offset)) {
        return;
    }
    g_range_mask_changed |= add_mask_tile(*g_range_mask_target, grid_offset, g_range_mask_bit);
}

void clear_masks(MaskSet &masks)
{
    masks.access.fill(0);
    masks.providers.fill(0);
}

void ensure_runtime_refreshed();

int area_has_access(
    const std::array<uint8_t, GRID_SIZE * GRID_SIZE> &mask,
    int x,
    int y,
    int size,
    uint8_t bit)
{
    if (!bit) {
        return 0;
    }
    for (int dy = 0; dy < size; dy++) {
        for (int dx = 0; dx < size; dx++) {
            int grid_offset = map_grid_offset(x + dx, y + dy);
            if (map_grid_is_valid_offset(grid_offset) && (mask[grid_offset] & bit)) {
                return 1;
            }
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

int has_water_source_access(int x, int y, int size)
{
    return map_terrain_exists_tile_in_area_with_type(x - 1, y - 1, size + 2, TERRAIN_WATER);
}

int requirement_term_is_satisfied(
    const WaterAccessDefinition &water,
    const WaterAccessRequirementTerm &term,
    int x,
    int y,
    int size,
    const MaskSet &masks)
{
    switch (term.kind) {
        case WaterAccessRequirementTermKind::Access:
            return term.where == WaterAccessRequirementWhere::Nodes ?
                nodes_have_access(water, masks.access, x, y, term.mask) :
                area_has_access(masks.access, x, y, size, term.mask);
        case WaterAccessRequirementTermKind::WaterSourceAny:
        case WaterAccessRequirementTermKind::WaterSourceFreshOnly:
            return has_water_source_access(x, y, size);
        default:
            return 0;
    }
}

int requirement_rule_is_satisfied(
    const WaterAccessDefinition &water,
    const WaterAccessRequirementRule &rule,
    int x,
    int y,
    int size,
    const MaskSet &masks)
{
    if (rule.terms.empty()) {
        return 0;
    }
    if (rule.mode == WaterAccessRequirementMode::Any) {
        for (const WaterAccessRequirementTerm &term : rule.terms) {
            if (requirement_term_is_satisfied(water, term, x, y, size, masks)) {
                return 1;
            }
        }
        return 0;
    }
    for (const WaterAccessRequirementTerm &term : rule.terms) {
        if (!requirement_term_is_satisfied(water, term, x, y, size, masks)) {
            return 0;
        }
    }
    return 1;
}

int requirements_are_satisfied(
    const WaterAccessDefinition &water,
    int x,
    int y,
    int size,
    const MaskSet &masks)
{
    for (const WaterAccessRequirementRule &rule : water.requirement_rules()) {
        if (!requirement_rule_is_satisfied(water, rule, x, y, size, masks)) {
            return 0;
        }
    }
    return 1;
}

int mark_building_footprint(
    std::array<uint8_t, GRID_SIZE * GRID_SIZE> &mask,
    int x,
    int y,
    int size,
    uint8_t bit)
{
    int changed = 0;
    for (int dy = 0; dy < size; dy++) {
        for (int dx = 0; dx < size; dx++) {
            changed |= add_mask_tile(mask, map_grid_offset(x + dx, y + dy), bit);
        }
    }
    return changed;
}

int mark_range(
    std::array<uint8_t, GRID_SIZE * GRID_SIZE> &mask,
    int grid_offset,
    int size,
    int range,
    uint8_t bit)
{
    g_range_mask_target = &mask;
    g_range_mask_bit = bit;
    g_range_mask_changed = 0;
    city_view_foreach_tile_in_range(grid_offset, size, range, mark_range_tile);
    g_range_mask_target = nullptr;
    g_range_mask_bit = 0;
    return g_range_mask_changed;
}

int mark_rule_from_footprint(
    MaskSet &masks,
    const WaterAccessProvideRule &rule,
    int x,
    int y,
    int grid_offset,
    int size)
{
    int changed = mark_building_footprint(masks.providers, x, y, size, rule.mask);
    if (rule.range > 0) {
        changed |= mark_range(masks.access, grid_offset, size, rule.range, rule.mask);
    } else {
        changed |= mark_building_footprint(masks.access, x, y, size, rule.mask);
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
            changed |= mark_range(masks.access, grid_offset, 1, rule.range, rule.mask);
        } else {
            changed |= add_mask_tile(masks.access, grid_offset, rule.mask);
        }
    }
    return changed;
}

int mark_provider_rules(
    MaskSet &masks,
    const WaterAccessDefinition &water,
    int x,
    int y,
    int grid_offset,
    int size)
{
    int changed = 0;
    for (const WaterAccessProvideRule &rule : water.provide_rules()) {
        if (rule.origin == WaterAccessOrigin::Nodes) {
            changed |= mark_rule_from_nodes(masks, water, rule, x, y);
        } else {
            changed |= mark_rule_from_footprint(masks, rule, x, y, grid_offset, size);
        }
    }
    return changed;
}

int building_has_required_workers(const building *b)
{
    if (!b) {
        return 0;
    }
    const model_building *model = model_get_building(b->type);
    return !model || model->laborers <= 0 || b->num_workers > 0;
}

int live_provider_is_active(const building *b, const WaterAccessDefinition &water, const MaskSet &requirement_masks)
{
    return b && b->state == BUILDING_STATE_IN_USE &&
        building_has_required_workers(b) &&
        requirements_are_satisfied(water, b->x, b->y, b->size, requirement_masks);
}

int mark_live_building_providers(SimulationResult &result, const MaskSet &requirement_masks)
{
    int changed = 0;
    for (int id = 1; id < building_count(); id++) {
        building *b = building_get(id);
        if (!b || b->state != BUILDING_STATE_IN_USE) {
            continue;
        }
        const WaterAccessDefinition *water = water_definition_for_building_type(b->type);
        if (!water || !water->has_provider() || !live_provider_is_active(b, *water, requirement_masks)) {
            continue;
        }
        changed |= mark_provider_rules(result.masks, *water, b->x, b->y, b->grid_offset, b->size);
    }
    return changed;
}

int mark_planned_providers(
    const SimulationInput &input,
    SimulationResult &result,
    const MaskSet &requirement_masks)
{
    int changed = 0;
    for (int i = 0; i < input.planned_provider_count; i++) {
        const PlannedProvider &planned = input.planned_providers[i];
        if (!planned.grid_offset) {
            continue;
        }
        const building_type type = normalize_provider_building_type(planned.type);
        const WaterAccessDefinition *water = water_definition_for_building_type(type);
        if (!water || !water->has_provider()) {
            continue;
        }
        const int x = map_grid_offset_to_x(planned.grid_offset);
        const int y = map_grid_offset_to_y(planned.grid_offset);
        const int size = provider_size_for_building_type(type);
        if (input.force_planned_provider_access ||
            !water->has_requirements() ||
            requirements_are_satisfied(*water, x, y, size, requirement_masks)) {
            changed |= mark_provider_rules(result.masks, *water, x, y, planned.grid_offset, size);
        }
    }
    return changed;
}

int mark_neptune_bonus(SimulationResult &result)
{
    if (!building_monument_gt_module_is_active(NEPTUNE_MODULE_2_CAPACITY_AND_WATER)) {
        return 0;
    }

    int neptune_gt_id = building_monument_get_grand_temple_for_god(GOD_NEPTUNE);
    building *b = neptune_gt_id ? building_get(neptune_gt_id) : nullptr;
    if (!b || !b->id) {
        return 0;
    }

    const uint8_t reservoir_mask = access_mask(kAccessReservoir);
    if (!reservoir_mask) {
        return 0;
    }
    WaterAccessProvideRule rule;
    rule.mask = reservoir_mask;
    rule.range = water_access_runtime_range_for_building(reservoir_type_id());
    rule.origin = WaterAccessOrigin::Footprint;
    return mark_rule_from_footprint(result.masks, rule, b->x, b->y, b->grid_offset, 7);
}

int is_aqueduct_tile_for_simulation(int grid_offset, const SimulationInput &input)
{
    if (!map_grid_is_valid_offset(grid_offset)) {
        return 0;
    }
    if (map_terrain_is(grid_offset, TERRAIN_AQUEDUCT)) {
        return 1;
    }
    if (input.include_constructing_aqueduct && map_property_is_constructing(grid_offset)) {
        return 1;
    }
    return input.preview_aqueduct_grid_offset && input.preview_aqueduct_grid_offset == grid_offset;
}

int mark_aqueduct_tile_providers(
    const SimulationInput &input,
    SimulationResult &result,
    const MaskSet &requirement_masks)
{
    const WaterAccessDefinition *water = water_definition_for_building_type(aqueduct_type_id());
    if (!water || !water->has_provider() || !water->has_requirements()) {
        return 0;
    }

    int changed = 0;
    int grid_offset = map_data.start_offset;
    for (int y = 0; y < map_data.height; y++, grid_offset += map_data.border_size) {
        for (int x = 0; x < map_data.width; x++, grid_offset++) {
            const int is_preview_tile = input.preview_aqueduct_grid_offset && input.preview_aqueduct_grid_offset == grid_offset;
            if (!is_aqueduct_tile_for_simulation(grid_offset, input) ||
                ((!input.force_planned_provider_access || !is_preview_tile) &&
                    !requirements_are_satisfied(*water, x, y, 1, requirement_masks))) {
                continue;
            }
            result.wet_aqueduct[grid_offset] = 1;
            changed |= mark_provider_rules(result.masks, *water, x, y, grid_offset, 1);
        }
    }
    return changed;
}

void build_water_masks(const SimulationInput &input, SimulationResult &result)
{
    SimulationResult current;

    for (;;) {
        SimulationResult next;
        // Providers are evaluated against the previous pass and written into a
        // fresh mask. This keeps node/range-0 connectors from satisfying their
        // own requirements in the same pass and removes scan-order propagation.
        mark_neptune_bonus(next);
        mark_live_building_providers(next, current.masks);
        mark_planned_providers(input, next, current.masks);
        mark_aqueduct_tile_providers(input, next, current.masks);

        if (next.masks.access == current.masks.access &&
            next.masks.providers == current.masks.providers &&
            next.wet_aqueduct == current.wet_aqueduct) {
            result = next;
            return;
        }
        current = next;
    }
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
            if (reservoir_mask && (result.masks.access[grid_offset] & reservoir_mask)) {
                map_terrain_add(grid_offset, TERRAIN_RESERVOIR_RANGE);
            }
            if (fountain_mask && (result.masks.access[grid_offset] & fountain_mask)) {
                map_terrain_add(grid_offset, TERRAIN_FOUNTAIN_RANGE);
            }
        }
    }
}

void project_house_water_state(building *b, const SimulationResult &result)
{
    b->has_well_access = area_has_access(result.masks.access, b->x, b->y, b->size, access_mask(kAccessWell)) ? 1 : 0;
    b->has_water_access = area_has_access(result.masks.access, b->x, b->y, b->size, access_mask(kAccessFountain)) ? 1 : 0;
    b->has_latrines_access = area_has_access(result.masks.access, b->x, b->y, b->size, access_mask(kAccessLatrines)) ? 1 : 0;
}

void refresh_water_graphic(building *b, const BuildingType *definition)
{
    if (!b || !definition || !definition->has_graphic() || !definition->water_access().has_requirements()) {
        return;
    }
    if (!Building(b).refresh_graphic_if_native()) {
        map_building_tiles_add(b->id, b->x, b->y, b->size, building_image_get(b), TERRAIN_BUILDING);
    }
}

void project_building_state(const SimulationResult &result)
{
    for (int id = 1; id < building_count(); id++) {
        building *b = building_get(id);
        if (!b || b->state != BUILDING_STATE_IN_USE) {
            continue;
        }

        if (b->house_size) {
            project_house_water_state(b, result);
        }

        const BuildingType *definition = definition_for_provider(b->type);
        if (!definition) {
            continue;
        }

        const WaterAccessDefinition &water = definition->water_access();
        if (water.has_requirements()) {
            b->has_water_access = static_cast<unsigned char>(
                building_has_required_workers(b) &&
                requirements_are_satisfied(water, b->x, b->y, b->size, result.masks));
        }
        if (definition_is(definition, "fountain")) {
            if (b->desirability > 60) {
                b->upgrade_level = 3;
            } else if (b->desirability > 40) {
                b->upgrade_level = 2;
            } else if (b->desirability > 20) {
                b->upgrade_level = 1;
            } else {
                b->upgrade_level = 0;
            }
        }
        refresh_water_graphic(b, definition);
    }
}

void build_preview_highlight(building_type type, const SimulationResult &preview_result)
{
    g_state.preview.highlight.fill(0);
    g_state.preview.active = 0;

    uint8_t bit = 0;
    if (const WaterAccessProvideRule *rule = primary_provider_rule(type)) {
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
    if (!g_state.refreshed) {
        water_access_runtime_refresh();
    }
}

} // namespace

void water_access_runtime_reset(void)
{
    clear_masks(g_state.masks);
    g_state.preview = PreviewState();
    g_state.refreshed = 0;
}

void water_access_runtime_refresh(void)
{
    SimulationInput input;
    SimulationResult result;
    build_water_masks(input, result);
    g_state.masks = result.masks;
    g_state.refreshed = 1;

    project_aqueduct_state(result);
    project_terrain_ranges(result);
    project_building_state(result);
}

void water_access_runtime_refresh_building(building *b)
{
    if (!b) {
        return;
    }

    const BuildingType *definition = definition_for_provider(b->type);
    if (!definition) {
        return;
    }

    const WaterAccessDefinition &water = definition->water_access();
    if (!water.has_provider() && !water.has_requirements()) {
        return;
    }

    water_access_runtime_refresh();
    if (water.has_requirements()) {
        b->has_water_access = static_cast<unsigned char>(
            building_has_required_workers(b) &&
            requirements_are_satisfied(water, b->x, b->y, b->size, g_state.masks));
    }
    if (b->state == BUILDING_STATE_IN_USE || b->state == BUILDING_STATE_MOTHBALLED || b->state == BUILDING_STATE_CREATED) {
        refresh_water_graphic(b, definition);
    }
}

int water_access_runtime_range_for_building(building_type type)
{
    type = normalize_provider_building_type(type);
    const WaterAccessProvideRule *rule = primary_provider_rule(type);
    int radius = rule ? rule->range : 0;
    const uint8_t mask = rule ? rule->mask : 0;

    if (mask & access_mask(kAccessWell)) {
        if (building_monument_working_grand_temple_for_god(GOD_NEPTUNE)) {
            radius++;
        }
    } else if (mask & access_mask(kAccessFountain)) {
        if (scenario_property_climate() == CLIMATE_DESERT) {
            radius--;
        }
        if (building_monument_working_grand_temple_for_god(GOD_NEPTUNE)) {
            radius++;
        }
    } else if (mask & access_mask(kAccessReservoir)) {
        if (building_monument_working_grand_temple_for_god(GOD_NEPTUNE)) {
            radius += 2;
        }
    }
    return radius < 0 ? 0 : radius;
}

const char *water_access_runtime_primary_provider_access_text(building_type type)
{
    const WaterAccessProvideRule *rule = primary_provider_rule(type);
    return rule ? text_from_mask(rule->mask) : nullptr;
}

int water_access_runtime_building_type_provides_access(building_type type)
{
    const WaterAccessDefinition *water = water_definition_for_building_type(type);
    return water && water->has_provider();
}

int water_access_runtime_building_type_provides_access_text(building_type type, const char *text_id)
{
    const uint8_t mask = access_mask(text_id);
    if (!mask) {
        return 0;
    }
    const WaterAccessDefinition *water = water_definition_for_building_type(type);
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

int water_access_runtime_building_type_requires_access_text(building_type type, const char *text_id)
{
    const uint8_t mask = access_mask(text_id);
    const WaterAccessDefinition *water = water_definition_for_building_type(type);
    if (!mask || !water) {
        return 0;
    }
    for (const WaterAccessRequirementRule &rule : water->requirement_rules()) {
        for (const WaterAccessRequirementTerm &term : rule.terms) {
            if (term.kind == WaterAccessRequirementTermKind::Access && (term.mask & mask)) {
                return 1;
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
    return mask && (g_state.masks.access[grid_offset] & mask);
}

int water_access_runtime_building_area_has_access(const building *b, const char *text_id)
{
    if (!b) {
        return 0;
    }
    ensure_runtime_refreshed();
    return area_has_access(g_state.masks.access, b->x, b->y, b->size, access_mask(text_id));
}

int water_access_runtime_building_has_required_access(const building *b)
{
    if (!b) {
        return 0;
    }
    ensure_runtime_refreshed();

    const BuildingType *definition = definition_for_provider(b->type);
    if (!definition || !definition->water_access().has_requirements()) {
        return 0;
    }
    return requirements_are_satisfied(definition->water_access(), b->x, b->y, b->size, g_state.masks);
}

int water_access_runtime_building_type_has_required_access_at(building_type type, int x, int y, int size)
{
    ensure_runtime_refreshed();
    const BuildingType *definition = definition_for_provider(type);
    if (!definition || !definition->water_access().has_requirements()) {
        return 1;
    }
    return requirements_are_satisfied(definition->water_access(), x, y, size, g_state.masks);
}

int water_access_runtime_reservoir_has_network_access(int grid_offset)
{
    ensure_runtime_refreshed();
    const BuildingType *definition = definition_for_provider(reservoir_type_id());
    if (!definition) {
        return 0;
    }
    return requirements_are_satisfied(
        definition->water_access(),
        map_grid_offset_to_x(grid_offset),
        map_grid_offset_to_y(grid_offset),
        provider_size_for_building_type(reservoir_type_id()),
        g_state.masks);
}

void water_access_runtime_begin_preview(building_type type, int primary_grid_offset, int secondary_grid_offset)
{
    ensure_runtime_refreshed();

    SimulationInput input;
    // Coverage previews answer "what range would this provider emit once working?";
    // ghost graphics still use the real requirement check to show wet/dry state.
    input.force_planned_provider_access = 1;
    if (type == aqueduct_type_id()) {
        input.preview_aqueduct_grid_offset = primary_grid_offset;
        input.include_constructing_aqueduct = 1;
    } else {
        if (primary_grid_offset) {
            PlannedProvider &provider = input.planned_providers[input.planned_provider_count++];
            provider.type = type;
            provider.grid_offset = primary_grid_offset;
        }
        if (secondary_grid_offset && secondary_grid_offset != primary_grid_offset && input.planned_provider_count < 2) {
            PlannedProvider &provider = input.planned_providers[input.planned_provider_count++];
            provider.type = type;
            provider.grid_offset = secondary_grid_offset;
        }
        if (type == draggable_reservoir_type_id()) {
            input.include_constructing_aqueduct = 1;
        }
    }

    SimulationResult preview_result;
    build_water_masks(input, preview_result);
    build_preview_highlight(type, preview_result);
}

void water_access_runtime_end_preview(void)
{
    g_state.preview = PreviewState();
}

int water_access_runtime_tile_has_preview_highlight(int grid_offset)
{
    return g_state.preview.active && map_grid_is_valid_offset(grid_offset) && g_state.preview.highlight[grid_offset];
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
        const building *b = building_get(map_building_at(grid_offset));
        const BuildingType *definition = b ? definition_for_provider(b->type) : nullptr;
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
