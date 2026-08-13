#pragma once

#include "building/building_type.h"
#include "game/mod_definition_loader.h"

#include <array>
#include <initializer_list>
#include <memory>
#include <string>
#include <string_view>

namespace building_type_registry_impl {

enum class GraphicsParseTargetScope {
    None,
    Default,
    Variant,
    ConstructionPhase
};

struct ParseState {
    std::unique_ptr<BuildingType> definition;
    int disabled = 0;
    int saw_root = 0;
    int saw_root_text = 0;
    size_t current_spawn_group_index = 0;
    int has_current_spawn_group = 0;
    int saw_identity = 0;
    int saw_model = 0;
    int saw_foundation = 0;
    int saw_button = 0;
    int saw_bridge = 0;
    int saw_rubble = 0;
    int saw_tile = 0;
    int saw_tool = 0;
    int saw_cycle = 0;
    int saw_temple = 0;
    int saw_sound = 0;
    int saw_market = 0;
    int saw_flags = 0;
    int saw_military = 0;
    int saw_military_formation = 0;
    int saw_desirability = 0;
    int saw_desirability_value = 0;
    int saw_desirability_step = 0;
    int saw_desirability_step_size = 0;
    int saw_desirability_range = 0;
    int saw_graphic = 0;
    int saw_composed = 0;
    int saw_construction = 0;
    int saw_construction_phase_graphics = 0;
    int saw_labor = 0;
    int saw_labor_employees = 0;
    int saw_labor_seeker = 0;
    int saw_labor_seeker_method = 0;
    int saw_labor_seeker_amount = 0;
    int saw_culture_modules = 0;
    int saw_storages = 0;
    int saw_production_methods = 0;
    int saw_distribution = 0;
    int saw_housing = 0;
    int saw_vacant_lot = 0;
    int saw_provider_water_access = 0;
    int saw_water_access_rule = 0;
    int saw_current_water_access_requirement_term = 0;
    int parsing_tile = 0;
    int parsing_provider_water_access = 0;
    int parsing_water_access_requirement = 0;
    int parsing_desirability = 0;
    int parsing_graphics = 0;
    int parsing_graphics_options = 0;
    int parsing_graphics_layer = 0;
    int parsing_composed = 0;
    int parsing_construction = 0;
    int parsing_construction_phase = 0;
    int parsing_labor = 0;
    int parsing_labor_seeker = 0;
    int parsing_military = 0;
    int parsing_culture_modules = 0;
    int parsing_storages = 0;
    int parsing_production_methods = 0;
    int parsing_housing = 0;
    int has_current_graphics_variant = 0;
    size_t current_graphics_variant_index = 0;
    GraphicsLayer *current_graphics_layer = nullptr;
    GraphicsParseTargetScope current_graphics_target_scope = GraphicsParseTargetScope::None;
    CompositionChildDef *current_composition_child = nullptr;
    LaborSeekerPolicy current_labor_seeker_policy;
    WaterAccessRequirementRule current_water_access_requirement_rule;
    WaterAccessRequirementWhere current_water_access_requirement_where = WaterAccessRequirementWhere::Footprint;
    int saw_spawn = 0;
    int error = 0;
};

extern std::array<std::unique_ptr<BuildingType>, BUILDING_TYPE_MAX> g_building_types;
extern mod_definition::DefinitionOverlayTracker g_building_type_overlays;
extern ParseState g_parse_state;

const BuildingType *definition_for_type(building_type type);
building_type type_from_attr(std::string_view attr);
int type_attr_is(building_type type, std::string_view attr);
int type_has_housing(building_type type);
int type_attr_is_any(building_type type, std::initializer_list<std::string_view> attrs);
int type_attr_is_any(building_type type, const char *const *attrs, int count);
template<typename Predicate>
building_type first_type_where(Predicate predicate)
{
    for (const std::unique_ptr<BuildingType> &definition : g_building_types) {
        if (definition && predicate(*definition)) {
            return definition->type();
        }
    }
    return BUILDING_NONE;
}
building_type type_from_bridge(BridgeType bridge_type);
building_type runtime_id_from_text(const char *text_id);
building_type building_type_for_housing_compatibility_level(int level, int footprint_size);
building_type vacant_lot_fill_type();
building_type vacant_lot_occupancy_type();
void clear_xml_runtime_property_fields();
int bridge_definition_attribute_is_valid_for_test(const char *type);
int building_type_geometry_attributes_are_valid_for_test(
    const char *foundation_path,
    const char *foundation_policy,
    const char *foundation_open_water,
    const char *model_size);
int smart_tool_mode_attributes_are_valid_for_test(
    const char *modifier,
    const char *type,
    const char *target,
    int footprint_size,
    const char *context = "default");
int building_type_water_access_open_water_attribute_is_valid_for_test(const char *value);
int graphics_status_icon_attributes_are_valid_for_test(const char *x, const char *y);
int graphics_overlay_summary_policy_is_valid_for_test(const char *mode);

}
