#pragma once

#include "building/building_type.h"
#include "building/building_type_api.h"

#include <array>
#include <memory>
#include <string>

namespace building_type_registry_impl {

enum class GraphicsParseTargetScope {
    None,
    Default,
    Variant,
    ConstructionPhase
};

struct ParseState {
    std::unique_ptr<BuildingType> definition;
    size_t current_spawn_group_index = 0;
    int has_current_spawn_group = 0;
    int saw_identity = 0;
    int saw_model = 0;
    int saw_foundation = 0;
    int saw_button = 0;
    int saw_sound = 0;
    int saw_event_data = 0;
    int saw_flags = 0;
    int saw_desirability = 0;
    int saw_desirability_value = 0;
    int saw_desirability_step = 0;
    int saw_desirability_step_size = 0;
    int saw_desirability_range = 0;
    int saw_graphic = 0;
    int saw_construction = 0;
    int saw_construction_phase_graphics = 0;
    int saw_labor = 0;
    int saw_labor_employees = 0;
    int saw_labor_seeker = 0;
    int saw_labor_seeker_method = 0;
    int saw_labor_seeker_amount = 0;
    int saw_storages = 0;
    int saw_production_methods = 0;
    int saw_housing = 0;
    int saw_state = 0;
    int saw_provider_water_access = 0;
    int saw_provider_water_access_type = 0;
    int saw_provider_water_access_range = 0;
    int saw_provider_water_access_requirement = 0;
    int parsing_state = 0;
    int parsing_provider_water_access = 0;
    int parsing_desirability = 0;
    int parsing_graphics = 0;
    int parsing_construction = 0;
    int parsing_construction_phase = 0;
    int parsing_labor = 0;
    int parsing_labor_seeker = 0;
    int parsing_storages = 0;
    int parsing_production_methods = 0;
    int parsing_housing = 0;
    int has_current_graphics_variant = 0;
    size_t current_graphics_variant_index = 0;
    GraphicsParseTargetScope current_graphics_target_scope = GraphicsParseTargetScope::None;
    LaborSeekerPolicy current_labor_seeker_policy;
    int saw_spawn = 0;
    int error = 0;
};

extern std::string g_building_type_path;
extern std::array<std::unique_ptr<BuildingType>, BUILDING_TYPE_MAX> g_building_types;
extern ParseState g_parse_state;

int directory_exists(const char *path);
const BuildingType *definition_for_type(building_type type);
building_type runtime_id_from_text(const char *text_id);
void refresh_building_type_path();
void refresh_known_building_type_ids();
void clear_xml_runtime_property_fields();

}
