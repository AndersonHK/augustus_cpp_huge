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
#include "building/building_runtime_internal.h"
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
#include <memory>
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
    const BuildingType *definition = nullptr;
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

const BuildingType *draggable_reservoir_definition()
{
    static const BuildingType *definition = nullptr;
    if (!definition) {
        definition = definition_from_attr("draggable_reservoir");
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

const BuildingType *normalize_provider_definition(const BuildingType *definition)
{
    return definition == draggable_reservoir_definition() ? reservoir_definition() : definition;
}

const BuildingType *definition_for_provider(const BuildingType *definition)
{
    return normalize_provider_definition(definition);
}

const WaterAccessDefinition *water_definition_for_provider(const BuildingType *definition)
{
    definition = definition_for_provider(definition);
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

int provider_size_for_definition(const BuildingType *definition)
{
    definition = definition_for_provider(definition);
    const int declared_size = definition ? definition->declared_model_size() : 0;
    return declared_size > 0 ? declared_size : 1;
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

void clear_simulation_result(SimulationResult &result)
{
    clear_masks(result.masks);
    result.wet_aqueduct.fill(0);
}

void clear_preview_state()
{
    g_state.preview.active = 0;
    g_state.preview.highlight.fill(0);
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

int building_has_required_workers(const Building *building)
{
    if (!building) {
        return 0;
    }
    const int required_workers = building->type ? building->type->required_workers() : 0;
    return required_workers <= 0 || building->worker_count() > 0;
}

int live_provider_is_active(const Building *building, const WaterAccessDefinition &water, const MaskSet &requirement_masks)
{
    if (!building || !building->is_in_use() || !building_has_required_workers(building)) {
        return 0;
    }
    return requirements_are_satisfied(water, building->x(), building->y(), building->size(), requirement_masks);
}

int mark_live_building_providers(SimulationResult &result, const MaskSet &requirement_masks)
{
    int changed = 0;
    Building::for_each([&](Building *building_object) {
        if (!building_object->is_in_use()) {
            return;
        }
        const WaterAccessDefinition *water = water_definition_for_provider(building_object->type);
        if (!water || !water->has_provider() || !live_provider_is_active(building_object, *water, requirement_masks)) {
            return;
        }
        changed |= mark_provider_rules(result.masks, *water,
            building_object->x(),
            building_object->y(),
            building_object->grid_offset(),
            building_object->size());
    });
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
        const BuildingType *definition = definition_for_provider(planned.definition);
        const WaterAccessDefinition *water = water_definition_for_provider(definition);
        if (!water || !water->has_provider()) {
            continue;
        }
        const int x = map_grid_offset_to_x(planned.grid_offset);
        const int y = map_grid_offset_to_y(planned.grid_offset);
        const int size = provider_size_for_definition(definition);
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

    Building *neptune_gt = grand_temple_for_god(GOD_NEPTUNE, false);
    if (!neptune_gt) {
        return 0;
    }

    const uint8_t reservoir_mask = access_mask(kAccessReservoir);
    if (!reservoir_mask) {
        return 0;
    }
    WaterAccessProvideRule rule;
    rule.mask = reservoir_mask;
    rule.range = water_access_runtime_range_for_building(reservoir_definition());
    rule.origin = WaterAccessOrigin::Footprint;
    return mark_rule_from_footprint(
        result.masks,
        rule,
        neptune_gt->x(),
        neptune_gt->y(),
        neptune_gt->grid_offset(),
        7);
}

int is_aqueduct_tile_for_simulation(int grid_offset, const SimulationInput &input)
{
    if (!map_grid_is_valid_offset(grid_offset)) {
        return 0;
    }
    if (map_terrain_is(grid_offset, TERRAIN_AQUEDUCT)) {
        return 1;
    }
    if (input.include_constructing_aqueduct) {
        if (map_property_is_constructing(grid_offset)) {
            return 1;
        }
    }
    return input.preview_aqueduct_grid_offset ?
        input.preview_aqueduct_grid_offset == grid_offset :
        0;
}

int mark_aqueduct_tile_providers(
    const SimulationInput &input,
    SimulationResult &result,
    const MaskSet &requirement_masks)
{
    const WaterAccessDefinition *water = water_definition_for_provider(aqueduct_definition());
    if (!water || !water->has_provider() || !water->has_requirements()) {
        return 0;
    }

    int changed = 0;
    int grid_offset = map_data.start_offset;
    for (int y = 0; y < map_data.height; y++, grid_offset += map_data.border_size) {
        for (int x = 0; x < map_data.width; x++, grid_offset++) {
            const int is_preview_tile = input.preview_aqueduct_grid_offset ?
                input.preview_aqueduct_grid_offset == grid_offset :
                0;
            if (!is_aqueduct_tile_for_simulation(grid_offset, input)) {
                continue;
            }
            if (!input.force_planned_provider_access || !is_preview_tile) {
                if (!requirements_are_satisfied(*water, x, y, 1, requirement_masks)) {
                    continue;
                }
            }
            result.wet_aqueduct[grid_offset] = 1;
            changed |= mark_provider_rules(result.masks, *water, x, y, grid_offset, 1);
        }
    }
    return changed;
}

void build_water_masks(const SimulationInput &input, SimulationResult &result)
{
    auto current = std::make_unique<SimulationResult>();
    auto next = std::make_unique<SimulationResult>();

    for (;;) {
        clear_simulation_result(*next);
        // Providers are evaluated against the previous pass and written into a
        // fresh mask. This keeps node/range-0 connectors from satisfying their
        // own requirements in the same pass and removes scan-order propagation.
        mark_neptune_bonus(*next);
        mark_live_building_providers(*next, current->masks);
        mark_planned_providers(input, *next, current->masks);
        mark_aqueduct_tile_providers(input, *next, current->masks);

        if (next->masks.access == current->masks.access &&
            next->masks.providers == current->masks.providers &&
            next->wet_aqueduct == current->wet_aqueduct) {
            result = *next;
            return;
        }
        std::swap(current, next);
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
            if (reservoir_mask ? (result.masks.access[grid_offset] & reservoir_mask) : 0) {
                map_terrain_add(grid_offset, TERRAIN_RESERVOIR_RANGE);
            }
            if (fountain_mask ? (result.masks.access[grid_offset] & fountain_mask) : 0) {
                map_terrain_add(grid_offset, TERRAIN_FOUNTAIN_RANGE);
            }
        }
    }
}

void project_house_water_state(Building *building_object, const SimulationResult &result)
{
    building *record = const_cast<::building *>(building_object->record());
    if (!record) {
        return;
    }
    record->has_well_access =
        area_has_access(result.masks.access,
            building_object->x(),
            building_object->y(),
            building_object->size(),
            access_mask(kAccessWell)) ? 1 : 0;
    building_object->set_has_water_access(
        area_has_access(result.masks.access,
            building_object->x(),
            building_object->y(),
            building_object->size(),
            access_mask(kAccessFountain)) ? 1 : 0);
    record->has_latrines_access =
        area_has_access(result.masks.access,
            building_object->x(),
            building_object->y(),
            building_object->size(),
            access_mask(kAccessLatrines)) ? 1 : 0;
}

void refresh_water_graphic(Building *building, const BuildingType *definition)
{
    if (!building || !definition || !definition->has_graphic() || !definition->water_access().has_requirements()) {
        return;
    }
    if (!building->refresh_graphic_if_native()) {
        map_building_tiles_add(*building,
            building->x(),
            building->y(),
            building->size(),
            building_image_get(building),
            TERRAIN_BUILDING);
    }
}

void project_building_state(const SimulationResult &result)
{
    Building::for_each([&](Building *building_object) {
        building *b = const_cast<building *>(building_object->record());
        if (!building_object->is_in_use()) {
            return;
        }

        if (building_object->has_house_size()) {
            project_house_water_state(building_object, result);
        }

        const BuildingType *definition = definition_for_provider(building_object->type);
        if (!definition) {
            return;
        }

        const WaterAccessDefinition &water = definition->water_access();
        if (water.has_requirements()) {
            int has_access = 0;
            if (building_has_required_workers(building_object)) {
                has_access = requirements_are_satisfied(water,
                    building_object->x(),
                    building_object->y(),
                    building_object->size(),
                    result.masks);
            }
            building_object->set_has_water_access(has_access);
        }
        if (b) {
            if (definition->attr_is("fountain")) {
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
        }
        refresh_water_graphic(building_object, definition);
    });
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
    if (!g_state.refreshed) {
        water_access_runtime_refresh();
    }
}

} // namespace

void water_access_runtime_reset(void)
{
    clear_masks(g_state.masks);
    clear_preview_state();
    g_state.refreshed = 0;
}

void water_access_runtime_refresh(void)
{
    SimulationInput input;
    auto result = std::make_unique<SimulationResult>();
    build_water_masks(input, *result);
    g_state.masks = result->masks;
    g_state.refreshed = 1;

    project_aqueduct_state(*result);
    project_terrain_ranges(*result);
    project_building_state(*result);
}

void water_access_runtime_refresh_building(Building *building)
{
    if (!building) {
        return;
    }

    const BuildingType *definition = definition_for_provider(building->type);
    if (!definition) {
        return;
    }

    const WaterAccessDefinition &water = definition->water_access();
    if (!water.has_provider() && !water.has_requirements()) {
        return;
    }

    water_access_runtime_refresh();
    if (water.has_requirements()) {
        int has_access = 0;
        if (building_has_required_workers(building)) {
            has_access = requirements_are_satisfied(water,
                building->x(),
                building->y(),
                building->size(),
                g_state.masks);
        }
        building->set_has_water_access(has_access);
    }
    const int state = building->state_id();
    if (state == BUILDING_STATE_IN_USE || state == BUILDING_STATE_MOTHBALLED || state == BUILDING_STATE_CREATED) {
        refresh_water_graphic(building, definition);
    }
}

int water_access_runtime_range_for_building(const BuildingType *definition)
{
    definition = definition_for_provider(definition);
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
    return area_has_access(g_state.masks.access,
        building->x(),
        building->y(),
        building->size(),
        access_mask(text_id));
}

int water_access_runtime_building_has_required_access(const Building *building)
{
    if (!building) {
        return 0;
    }
    ensure_runtime_refreshed();

    const BuildingType *definition = definition_for_provider(building->type);
    if (!definition || !definition->water_access().has_requirements()) {
        return 0;
    }
    return requirements_are_satisfied(definition->water_access(),
        building->x(),
        building->y(),
        building->size(),
        g_state.masks);
}

int water_access_runtime_building_type_has_required_access_at(const BuildingType *definition, int x, int y, int size)
{
    ensure_runtime_refreshed();
    definition = definition_for_provider(definition);
    if (!definition || !definition->water_access().has_requirements()) {
        return 1;
    }
    return requirements_are_satisfied(definition->water_access(), x, y, size, g_state.masks);
}

int water_access_runtime_reservoir_has_network_access(int grid_offset)
{
    ensure_runtime_refreshed();
    const BuildingType *definition = definition_for_provider(reservoir_definition());
    if (!definition) {
        return 0;
    }
    return requirements_are_satisfied(
        definition->water_access(),
        map_grid_offset_to_x(grid_offset),
        map_grid_offset_to_y(grid_offset),
        provider_size_for_definition(definition),
        g_state.masks);
}

void water_access_runtime_begin_preview(const BuildingType *definition, int primary_grid_offset, int secondary_grid_offset)
{
    ensure_runtime_refreshed();

    SimulationInput input;
    // Coverage previews answer "what range would this provider emit once working?";
    // ghost graphics still use the real requirement check to show wet/dry state.
    input.force_planned_provider_access = 1;
    if (definition == aqueduct_definition()) {
        input.preview_aqueduct_grid_offset = primary_grid_offset;
        input.include_constructing_aqueduct = 1;
    } else {
        if (primary_grid_offset) {
            PlannedProvider &provider = input.planned_providers[input.planned_provider_count++];
            provider.definition = definition;
            provider.grid_offset = primary_grid_offset;
        }
        if (secondary_grid_offset) {
            if (secondary_grid_offset != primary_grid_offset && input.planned_provider_count < 2) {
                PlannedProvider &provider = input.planned_providers[input.planned_provider_count++];
                provider.definition = definition;
                provider.grid_offset = secondary_grid_offset;
            }
        }
        if (definition == draggable_reservoir_definition()) {
            input.include_constructing_aqueduct = 1;
        }
    }

    auto preview_result = std::make_unique<SimulationResult>();
    build_water_masks(input, *preview_result);
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
        const BuildingType *definition = building ? definition_for_provider(building->type) : nullptr;
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
