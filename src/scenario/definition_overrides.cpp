#include "scenario/definition_overrides.h"
#include "building/building_type_registry_internal.h"
#include "building/housing_profile_registry.h"
#include "core/log.h"
#include "scenario/event/parameter_data.h"
#include <algorithm>
#include <array>
#include <map>
#include <tuple>

namespace {
using namespace building_type_registry_impl;
using Key = std::tuple<ScenarioOverrideKind, std::string, int, std::string>;
std::map<Key, ScenarioDefinitionOverride> overrides;
std::map<std::string, HousingProfileDef> profile_defaults;
struct BuildingDefaults { ConstructionDefinition construction; int capacity; };
std::map<std::string, BuildingDefaults> building_defaults;
const std::array<int, 17> housing_maximum = {1000,1000,100,2,5,3,1,1,2,10,100,100,100,2,100,32767,1000};

int *housing_field(HousingProfileDef &profile, int field)
{
    auto &r = profile.requirements;
    switch (field) {
        case 0: return &profile.evolution.devolve_desirability;
        case 1: return &profile.evolution.evolve_desirability;
        case 2: return &r.entertainment;
        case 4: return &r.religion;
        case 5: return &r.education;
        case 6: return &r.barber;
        case 7: return &r.bathhouse;
        case 8: return &r.health;
        case 9: return &r.food_types;
        case 10: return &r.pottery;
        case 11: return &r.oil;
        case 12: return &r.furniture;
        case 13: return &r.wine;
        case 14: return &profile.prosperity;
        case 16: return &profile.tax_multiplier;
        default: return nullptr;
    }
}

BuildingType *building_for(const std::string &id)
{
    auto type = runtime_id_from_text(id.c_str());
    return type > BUILDING_NONE && type < BUILDING_TYPE_MAX ? g_building_types[type].get() : nullptr;
}

bool apply(const ScenarioDefinitionOverride &entry)
{
    if (entry.kind == ScenarioOverrideKind::Housing) {
        auto *profile = find_mutable_housing_profile_definition(entry.target.c_str());
        if (!profile || entry.field < 0 || entry.field > 16 || entry.value < (entry.field < 2 ? -1000 : 0) || entry.value > housing_maximum[entry.field]) return false;
        if (entry.field == 3) {
            if (entry.value < 0 || entry.value > 2) return false;
            profile->requirements.water = static_cast<HousingWaterRequirement>(entry.value);
        } else {
            int *field = housing_field(*profile, entry.field);
            if (!field) return false;
            *field = entry.value;
        }
    } else if (entry.kind == ScenarioOverrideKind::Capacity) {
        auto *building = building_for(entry.target);
        if (!building || !building->housing_def().profile || entry.value < 0 || entry.value > 32767) return false;
        building->housing_def().capacity = entry.value;
    } else if (entry.kind == ScenarioOverrideKind::Construction) {
        auto *building = building_for(entry.target);
        auto resource = resource_type_from_text_id(entry.resource.c_str());
        if (!building || !resource_is_declared(resource) || entry.value < 0) return false;
        return building->construction().set_scenario_requirement(entry.field, resource, entry.value);
    } else if (entry.kind == ScenarioOverrideKind::Migration) {
        return entry.target.empty() && entry.resource.empty() && (entry.field == 0 || entry.field == 1) && entry.value >= 0 && entry.value <= 1000000;
    } else if (entry.kind == ScenarioOverrideKind::HiddenRoute) {
        return !entry.target.empty() && entry.field == 0 && (entry.value == 0 || entry.value == 1);
    } else if (entry.kind == ScenarioOverrideKind::RouteResource) {
        return !entry.target.empty() && entry.field == 0 && entry.value >= 0 && resource_is_declared(resource_type_from_text_id(entry.resource.c_str()));
    }

    return true;
}

void write_text(buffer *buf, const std::string &text)
{
    buffer_write_u32(buf, static_cast<uint32_t>(text.size()));
    buffer_write_raw(buf, text.data(), text.size());
}
bool read_text(buffer *buf, std::string &text)
{
    if (buf->index > buf->size || buf->size - buf->index < 4) return false;
    uint32_t size = buffer_read_u32(buf);
    if (size > 65536 || size > buf->size - buf->index) return false;
    text.resize(size);
    buffer_read_raw(buf, text.data(), size);
    return text.find('\0') == std::string::npos;
}
}

void scenario_definition_overrides_capture_defaults()
{
    scenario_events_parameter_data_reset_mappings();
    overrides.clear(); profile_defaults.clear(); building_defaults.clear();
    for (auto &building : building_type_registry_impl::g_building_types) {
        if (!building) continue;
        building_defaults.emplace(building->attr(), BuildingDefaults{building->construction(), building->housing_def().capacity});
        if (const auto *profile = building->housing_def().profile) profile_defaults.emplace(profile->path_id, *profile);
    }
}

void scenario_definition_overrides_reset()
{
    for (const auto &[id, profile] : profile_defaults) {
        if (auto *current = building_type_registry_impl::find_mutable_housing_profile_definition(id.c_str())) *current = profile;
    }
    for (const auto &[id, baseline] : building_defaults) {
        if (auto *current = building_for(id)) { current->construction() = baseline.construction; current->housing_def().capacity = baseline.capacity; }
    }
    overrides.clear();
}

bool scenario_definition_override_set(ScenarioDefinitionOverride entry)
{
    if (!apply(entry)) return false;
    overrides[{entry.kind, entry.target, entry.field, entry.resource}] = std::move(entry);
    return true;
}

int scenario_definition_override_value(ScenarioOverrideKind kind, const std::string &target, int field, const std::string &resource, int fallback)
{
    auto found = overrides.find({kind, target, field, resource});
    return found == overrides.end() ? fallback : found->second.value;
}

const char *scenario_definition_override_text(ScenarioOverrideKind kind, const std::string &target)
{
    auto found = overrides.find({kind, target, 0, {}});
    return found == overrides.end() ? nullptr : found->second.text.c_str();
}

bool scenario_house_model_change(building_type type, int field, int amount, bool absolute)
{
    using namespace building_type_registry_impl;
    const auto *building = definition_for_type(type);
    if (!building || !building->housing_def().profile || field < 0 || field > 16) return false;
    const auto &housing = building->housing_def();
    auto *profile = find_mutable_housing_profile_definition(housing.profile_path.c_str());
    if (!profile) return false;
    int current = field == 15 ? housing.capacity : field == 3 ? static_cast<int>(profile->requirements.water) : *housing_field(*profile, field);
    // Native capacities are per building; the event selector names that building definition.
    int value = static_cast<int>(std::clamp<int64_t>(static_cast<int64_t>(amount) + (absolute ? 0 : current), field < 2 ? -1000 : 0, housing_maximum[field]));
    return scenario_definition_override_set({field == 15 ? ScenarioOverrideKind::Capacity : ScenarioOverrideKind::Housing, field == 15 ? building->attr() : profile->path_id, field, {}, value});
}

int scenario_house_model_value(building_type type, int field)
{
    const auto *building = building_type_registry_impl::definition_for_type(type);
    if (!building || !building->housing_def().profile || field < 0 || field > 16) return 0;
    const auto &housing = building->housing_def();
    auto *profile = building_type_registry_impl::find_mutable_housing_profile_definition(housing.profile_path.c_str());
    if (!profile) return 0;
    return field == 15 ? housing.capacity : field == 3 ? static_cast<int>(profile->requirements.water) : *housing_field(*profile, field);
}

int scenario_house_model_maximum(int field) { return field >= 0 && field < 17 ? housing_maximum[field] : 0; }

bool scenario_construction_requirement_change(building_type type, int phase, resource_type resource, int amount)
{
    auto *building = building_type_registry_impl::definition_for_type(type);
    if (!building || !resource_is_declared(resource)) return false;
    return scenario_definition_override_set({ScenarioOverrideKind::Construction, building->attr(), phase, resource_text_id(resource), std::max(0, amount)});
}

size_t scenario_definition_overrides_serialized_size()
{
    size_t bytes = 4;
    for (const auto &[key, entry] : overrides) bytes += 24 + entry.target.size() + entry.resource.size() + entry.text.size();
    return bytes;
}

void scenario_definition_overrides_write(buffer *buf)
{
    buffer_write_u32(buf, static_cast<uint32_t>(overrides.size()));
    for (const auto &[key, entry] : overrides) {
        buffer_write_u32(buf, static_cast<uint32_t>(entry.kind)); write_text(buf, entry.target);
        buffer_write_i32(buf, entry.field); write_text(buf, entry.resource); buffer_write_i32(buf, entry.value); write_text(buf, entry.text);
    }
}

bool scenario_definition_overrides_read(buffer *buf, std::vector<ScenarioDefinitionOverride> &entries)
{
    if (buf->index > buf->size || buf->size - buf->index < 4) return false;
    uint32_t count = buffer_read_u32(buf);
    if (count > (buf->size - buf->index) / 24) return false;
    std::map<Key, bool> seen;
    for (uint32_t i = 0; i < count; ++i) {
        ScenarioDefinitionOverride entry{};
        uint32_t kind = buffer_read_u32(buf);
        if (kind > static_cast<uint32_t>(ScenarioOverrideKind::Text)) return false;
        entry.kind = static_cast<ScenarioOverrideKind>(kind);
        if (!read_text(buf, entry.target)) return false;
        entry.field = buffer_read_i32(buf);
        if (!read_text(buf, entry.resource)) return false;
        entry.value = buffer_read_i32(buf);
        if (!read_text(buf, entry.text) || buf->overflow || !seen.emplace(Key{entry.kind, entry.target, entry.field, entry.resource}, true).second) return false;
        entries.push_back(std::move(entry));
    }
    return !buf->overflow;
}

void scenario_definition_overrides_apply(const std::vector<ScenarioDefinitionOverride> &entries)
{
    for (const auto &entry : entries) if (!scenario_definition_override_set(entry)) log_warning("Skipping unavailable scenario definition override", entry.target.c_str(), entry.field);
}

int scenario_text_add(const char *text)
{
    int id = 1;
    while (scenario_definition_override_text(ScenarioOverrideKind::Text, std::to_string(id))) ++id;
    scenario_definition_override_set({ScenarioOverrideKind::Text, std::to_string(id), 0, {}, 0, text ? text : ""});
    return id;
}

const uint8_t *scenario_text_get(int id)
{
    const char *text = scenario_definition_override_text(ScenarioOverrideKind::Text, std::to_string(id));
    return reinterpret_cast<const uint8_t *>(text ? text : "");
}

void scenario_definition_overrides_clear_empire()
{
    for (auto it = overrides.begin(); it != overrides.end();) {
        const auto kind = it->second.kind;
        if (kind == ScenarioOverrideKind::HiddenRoute || kind == ScenarioOverrideKind::RouteResource || kind == ScenarioOverrideKind::CityName) it = overrides.erase(it);
        else ++it;
    }
}
