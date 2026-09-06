#include "scenario/definition_overrides.h"
#include "properties.h"
#include "building/BuildingCityService.h"
#include "building/building_type_id_bridge.h"
#include "core/log.h"
#include <algorithm>
#include <array>
#include <cstring>
#include <string>
#include <vector>

int model_get_construction_cost(building_type type)
{
    return city_service_construction_cost(type, model_get_building(type)->cost);
}

#include <stdint.h>
#include <stdlib.h>

static model_building buildings[BUILDING_TYPE_MAX];
static model_building mod_defaults[BUILDING_TYPE_MAX];
static uint32_t scenario_fields[BUILDING_TYPE_MAX];
static constexpr std::array<int model_building::*, 6> MODEL_FIELDS = {&model_building::cost, &model_building::desirability_value,
    &model_building::desirability_step, &model_building::desirability_step_size, &model_building::desirability_range, &model_building::laborers};
static constexpr uint32_t MODEL_OVERLAY_MAGIC = 0x324f4d56; // VMO2: keyed models followed by other scenario definition overlays
static building_properties properties[BUILDING_TYPE_MAX];

static const model_building NO_BUILDING_MODEL = {
    .cost = 0,
    .desirability_value = 0,
    .desirability_step = 0,
    .desirability_step_size = 0,
    .desirability_range = 0,
    .laborers = 0
};

static int valid_building_type(building_type type)
{
    return type > BUILDING_NONE && type < BUILDING_TYPE_MAX;
}

void building_properties_init(void)
{
}

void building_properties_clear_xml_runtime_fields(building_type type)
{
    if (!valid_building_type(type)) {
        return;
    }
    properties[type] = {};
}

void building_properties_apply_xml_sound_id(building_type type, int sound_id)
{
    if (!valid_building_type(type)) {
        return;
    }
    properties[type].sound_id = sound_id;
}

void building_properties_apply_xml_fire_proof(building_type type, int fire_proof)
{
    if (!valid_building_type(type)) {
        return;
    }
    properties[type].fire_proof = fire_proof;
}

void building_properties_apply_xml_draw_desirability_range(building_type type, int draw_desirability_range)
{
    if (!valid_building_type(type)) {
        return;
    }
    properties[type].draw_desirability_range = draw_desirability_range;
}

void building_properties_apply_xml_venus_gt_bonus(building_type type, int venus_gt_bonus)
{
    if (!valid_building_type(type)) {
        return;
    }
    properties[type].venus_gt_bonus = venus_gt_bonus;
}

const building_properties *building_properties_for_type(building_type type)
{
    if (!valid_building_type(type)) {
        return &properties[BUILDING_NONE];
    }
    return &properties[type];
}

void model_reset(void)
{
    scenario_definition_overrides_reset();
    for (building_type type = BUILDING_NONE; type < BUILDING_TYPE_MAX; type = (building_type) (type + 1)) {
        buildings[type] = NO_BUILDING_MODEL;
        mod_defaults[type] = NO_BUILDING_MODEL;
        scenario_fields[type] = 0;
    }
}

void model_save_model_data(buffer *buf)
{
    struct Entry { std::string id; uint32_t mask; building_type type; };
    std::vector<Entry> entries;
    size_t bytes = 8 + scenario_definition_overrides_serialized_size();
    for (int index = 1; index < BUILDING_TYPE_MAX; ++index) {
        const auto type = static_cast<building_type>(index);
        uint32_t mask = scenario_fields[index];
        for (size_t field = 0; field < MODEL_FIELDS.size(); ++field) {
            if (buildings[index].*MODEL_FIELDS[field] != mod_defaults[index].*MODEL_FIELDS[field]) mask |= 1u << field;
        }
        const char *id = building_type_id_bridge_text_from_runtime(type);
        if (!mask || !id || !*id) continue;
        entries.push_back({id, mask, type});
        bytes += 8 + std::strlen(id) + MODEL_FIELDS.size() * 4;
    }
    buffer_init_dynamic(buf, bytes);
    buffer_write_u32(buf, MODEL_OVERLAY_MAGIC);
    buffer_write_u32(buf, static_cast<uint32_t>(entries.size()));
    for (const auto &entry : entries) {
        buffer_write_u32(buf, static_cast<uint32_t>(entry.id.size()));
        buffer_write_raw(buf, entry.id.data(), entry.id.size());
        buffer_write_u32(buf, entry.mask);
        for (auto field : MODEL_FIELDS) buffer_write_i32(buf, buildings[entry.type].*field);
    }
    scenario_definition_overrides_write(buf);
}

int model_load_model_data(buffer *buf, bool keyed)
{
    if (!buf) return 0;
    if (!keyed) {
        const size_t count = std::min(buf->size / sizeof(model_building), static_cast<size_t>(BUILDING_TYPE_MAX));
        buffer_read_raw(buf, buildings, count * sizeof(model_building));
        return !buf->overflow;
    }
    buffer source = *buf;
    buffer_reset(&source);
    if (source.size < 12 || buffer_read_u32(&source) != source.size) return 0;
    const uint32_t magic = buffer_read_u32(&source);
    if (magic != MODEL_OVERLAY_MAGIC && magic != 0x314f4d56) return 0;
    const uint32_t count = buffer_read_u32(&source);
    if (count > (source.size - source.index) / 33) return 0;
    struct Entry { building_type type; uint32_t mask; model_building model; };
    std::vector<Entry> entries;
    for (uint32_t index = 0; index < count; ++index) {
        const uint32_t size = buffer_read_u32(&source);
        if (!size || size > 1024 || size > source.size - source.index || source.size - source.index - size < 28) return 0;
        std::string id(size, '\0'); buffer_read_raw(&source, id.data(), size);
        if (id.find('\0') != std::string::npos) return 0;
        const uint32_t mask = buffer_read_u32(&source);
        if (mask & ~63u) return 0;
        model_building model{};
        for (auto field : MODEL_FIELDS) model.*field = buffer_read_i32(&source);
        const building_type type = building_type_id_bridge_runtime_from_text(id.c_str());
        if (type == BUILDING_NONE) { log_warning("Scenario model override references an unavailable building", id.c_str(), 0); continue; }
        entries.push_back({type, mask, model});
    }
    std::vector<ScenarioDefinitionOverride> definition_overrides;
    if (magic == MODEL_OVERLAY_MAGIC && !scenario_definition_overrides_read(&source, definition_overrides)) return 0;
    if (source.overflow || source.index != source.size) return 0;
    for (const auto &entry : entries) {
        for (size_t field = 0; field < MODEL_FIELDS.size(); ++field) {
            if (entry.mask & (1u << field)) buildings[entry.type].*MODEL_FIELDS[field] = entry.model.*MODEL_FIELDS[field];
        }
        scenario_fields[entry.type] |= entry.mask;
    }
    scenario_definition_overrides_apply(definition_overrides);
    return 1;
}

void model_capture_mod_defaults(void)
{
    std::copy(std::begin(buildings), std::end(buildings), std::begin(mod_defaults));
}

const model_building *model_get_mod_defaults(building_type type)
{
    return valid_building_type(type) ? &mod_defaults[type] : &NO_BUILDING_MODEL;
}

void model_mark_scenario_override(building_type type, int field)
{
    if (valid_building_type(type) && field >= 0 && field < static_cast<int>(MODEL_FIELDS.size())) scenario_fields[type] |= 1u << field;
}

model_building *model_get_building(building_type type)
{
    if (!valid_building_type(type)) {
        return (model_building *) &NO_BUILDING_MODEL;
    }
    return &buildings[type];
}
