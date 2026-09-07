#include "scenario/definition_overrides.h"
#include "properties.h"
#include "building/BuildingCityService.h"
#include "building/building_type_id_bridge.h"
#include "building/building_type_legacy_migration.h"
#include "game/legacy_model_defaults.generated.h"
#include "game/save_version.h"
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
static constexpr uint32_t MODEL_OVERLAY_MAGIC = 0x334f4d56; // VMO3: only explicitly edited model fields, followed by scenario definition overlays
static constexpr uint32_t LEGACY_MODEL_OVERLAY_MAGIC = 0x324f4d56;
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
        const uint32_t mask = model_scenario_override_fields(type);
        const char *id = building_type_id_bridge_text_from_runtime(type);
        if (!mask || !id || !*id) continue;
        entries.push_back({id, mask, type});
        bytes += 8 + std::strlen(id);
        for (size_t field = 0; field < MODEL_FIELDS.size(); ++field) if (mask & (1u << field)) bytes += 4;
    }
    buffer_init_dynamic(buf, bytes);
    buffer_write_u32(buf, MODEL_OVERLAY_MAGIC);
    buffer_write_u32(buf, static_cast<uint32_t>(entries.size()));
    for (const auto &entry : entries) {
        buffer_write_u32(buf, static_cast<uint32_t>(entry.id.size()));
        buffer_write_raw(buf, entry.id.data(), entry.id.size());
        buffer_write_u32(buf, entry.mask);
        for (size_t field = 0; field < MODEL_FIELDS.size(); ++field) {
            if (entry.mask & (1u << field)) buffer_write_i32(buf, buildings[entry.type].*MODEL_FIELDS[field]);
        }
    }
    scenario_definition_overrides_write(buf);
}

int model_load_model_data(buffer *buf, ModelDataFormat format, int source_save_version)
{
    if (!buf) return 0;
    if (format == ModelDataFormat::LegacyNativeSnapshot) {
        // Before runtime-defined IDs, the early fork retained the source enum
        // and appended clear_trees. Those fixed-layout snapshots are recoverable.
        if (source_save_version > SAVE_GAME_LAST_NO_FORMULAS_AND_MODEL_DATA && source_save_version <= SAVE_GAME_LAST_NO_BUILDING_TYPE_TABLE &&
            (buf->size == sizeof(legacy_model_defaults::buildings) ||
            buf->size == sizeof(legacy_model_defaults::buildings) + sizeof(model_building))) {
            return model_import_legacy_source_data(buf, source_save_version);
        }
        // These were indexed by the producer's runtime IDs, not its saved ID
        // table. No identity or edit provenance survives; never reinterpret them.
        std::copy(std::begin(mod_defaults), std::end(mod_defaults), std::begin(buildings));
        std::fill(std::begin(scenario_fields), std::end(scenario_fields), 0);
        log_warning("Repairing corrupted legacy native building models by restoring mod defaults", nullptr, 0);
        return 1;
    }
    buffer source = *buf;
    buffer_reset(&source);
    if (source.size < 12 || buffer_read_u32(&source) != source.size) return 0;
    const uint32_t magic = buffer_read_u32(&source);
    const bool legacy = format == ModelDataFormat::LegacyNativeOverlay;
    if (legacy ? (magic != LEGACY_MODEL_OVERLAY_MAGIC && magic != 0x314f4d56) : magic != MODEL_OVERLAY_MAGIC) return 0;
    const uint32_t count = buffer_read_u32(&source);
    if (count > (source.size - source.index) / (legacy ? 33 : 13)) return 0;
    struct Entry { building_type type; uint32_t mask; model_building model; };
    std::vector<Entry> entries;
    for (uint32_t index = 0; index < count; ++index) {
        const uint32_t size = buffer_read_u32(&source);
        if (!size || size > 1024 || size > source.size - source.index || source.size - source.index - size < 4) return 0;
        std::string id(size, '\0'); buffer_read_raw(&source, id.data(), size);
        if (id.find('\0') != std::string::npos) return 0;
        const uint32_t mask = buffer_read_u32(&source);
        if (!mask || (mask & ~63u)) return 0;
        model_building model{};
        for (size_t field = 0; field < MODEL_FIELDS.size(); ++field) {
            if (legacy || (mask & (1u << field))) model.*MODEL_FIELDS[field] = buffer_read_i32(&source);
        }
        if (source.overflow) return 0;
        if (legacy) continue; // VMO1/2 auto-promoted corrupt snapshots into apparent overrides.
        const building_type type = building_type_id_bridge_runtime_from_text(id.c_str());
        if (type == BUILDING_NONE) { log_warning("Scenario model override references an unavailable building", id.c_str(), 0); continue; }
        entries.push_back({type, mask, model});
    }
    std::vector<ScenarioDefinitionOverride> definition_overrides;
    if (magic != 0x314f4d56 && !scenario_definition_overrides_read(&source, definition_overrides)) return 0;
    if (source.overflow || source.index != source.size) return 0;
    if (legacy) {
        std::copy(std::begin(mod_defaults), std::end(mod_defaults), std::begin(buildings));
        std::fill(std::begin(scenario_fields), std::end(scenario_fields), 0);
        if (count) log_warning("Repairing corrupted legacy native building model overlays by restoring mod defaults", nullptr, count);
    }
    for (const auto &entry : entries) {
        for (size_t field = 0; field < MODEL_FIELDS.size(); ++field) {
            if (entry.mask & (1u << field)) buildings[entry.type].*MODEL_FIELDS[field] = entry.model.*MODEL_FIELDS[field];
        }
        scenario_fields[entry.type] |= entry.mask;
    }
    scenario_definition_overrides_apply(definition_overrides);
    return 1;
}

int model_import_legacy_source_data(buffer *buf, int save_version)
{
    // Only shared legacy and the fixed-enum early fork belong here. Dedicated
    // post-fork Augustus archives need their own producer/identity converter.
    if (!buf || (buf->size != sizeof(legacy_model_defaults::buildings) &&
        buf->size != sizeof(legacy_model_defaults::buildings) + sizeof(model_building)) ||
        save_version <= SAVE_GAME_LAST_NO_FORMULAS_AND_MODEL_DATA || save_version > SAVE_GAME_LAST_NO_BUILDING_TYPE_TABLE) return 0;
    buffer source = *buf;
    buffer_reset(&source);
    int ambiguous_fields = 0;
    for (size_t index = 0; index < buf->size / sizeof(model_building); ++index) {
        const auto &baseline = index < std::size(legacy_model_defaults::buildings) ? legacy_model_defaults::buildings[index] : legacy_model_defaults::clear_trees_extension;
        model_building model{};
        uint32_t mask = 0;
        for (size_t field = 0; field < MODEL_FIELDS.size(); ++field) {
            const int value = buffer_read_i32(&source);
            model.*MODEL_FIELDS[field] = value;
            bool source_default = value == baseline.*MODEL_FIELDS[field];
            if (!source_default && save_version == 0xaa) {
                for (const auto &alternative : legacy_model_defaults::version_170_alternatives) {
                    if (alternative.building == index && alternative.field == field && alternative.value == value) { source_default = true; ++ambiguous_fields; break; }
                }
            }
            if (!source_default) mask |= 1u << field;
        }
        if (!mask) continue;
        const char *id = building_type_legacy_migration_text_id_for_enum(static_cast<uint16_t>(index));
        const building_type type = id ? building_type_id_bridge_runtime_from_text(id) : BUILDING_NONE;
        if (type == BUILDING_NONE) { log_warning("Legacy scenario model override references an unavailable building", id, static_cast<int>(index)); continue; }
        for (size_t field = 0; field < MODEL_FIELDS.size(); ++field) {
            if (mask & (1u << field)) buildings[type].*MODEL_FIELDS[field] = model.*MODEL_FIELDS[field];
        }
        scenario_fields[type] |= mask;
    }
    if (ambiguous_fields) log_warning("Repairing ambiguous legacy model values matching older upstream defaults by inheriting mod definitions", nullptr, ambiguous_fields);
    return !source.overflow;
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

uint32_t model_scenario_override_fields(building_type type)
{
    return valid_building_type(type) ? scenario_fields[type] : 0;
}

model_building *model_get_building(building_type type)
{
    if (!valid_building_type(type)) {
        return (model_building *) &NO_BUILDING_MODEL;
    }
    return &buildings[type];
}
