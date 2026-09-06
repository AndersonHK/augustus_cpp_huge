#pragma once
#include "core/buffer.h"
#include "building/building_type.h"
#include <string>
#include <vector>

enum class ScenarioOverrideKind { Housing, Capacity, Construction, Migration, HiddenRoute, RouteResource, CityName, Text };
struct ScenarioDefinitionOverride {
    ScenarioOverrideKind kind;
    std::string target;
    int field = 0;
    std::string resource;
    int value = 0;
    std::string text;
};

void scenario_definition_overrides_capture_defaults();
void scenario_definition_overrides_reset();
bool scenario_definition_override_set(ScenarioDefinitionOverride entry);
int scenario_definition_override_value(ScenarioOverrideKind kind, const std::string &target, int field, const std::string &resource, int fallback);
const char *scenario_definition_override_text(ScenarioOverrideKind kind, const std::string &target);
bool scenario_house_model_change(building_type type, int field, int amount, bool absolute);
int scenario_house_model_value(building_type type, int field);
int scenario_house_model_maximum(int field);
bool scenario_construction_requirement_change(building_type type, int phase, resource_type resource, int amount);
size_t scenario_definition_overrides_serialized_size();
void scenario_definition_overrides_write(buffer *buf);
bool scenario_definition_overrides_read(buffer *buf, std::vector<ScenarioDefinitionOverride> &entries);
void scenario_definition_overrides_apply(const std::vector<ScenarioDefinitionOverride> &entries);
int scenario_text_add(const char *text);
const uint8_t *scenario_text_get(int id);

void scenario_definition_overrides_clear_empire();
