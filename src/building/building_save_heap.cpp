#include "building/building_save_heap.h"

extern "C" {
#include "building/building.h"
#include "building/building_type_id_bridge.h"
#include "building/industry.h"
#include "building/roadblock.h"
#include "game/resource.h"
}

#include "core/crash_context.h"

#include <array>
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <list>
#include <memory>
#include <string>
#include <vector>

namespace {

constexpr uint32_t kHeapMagic = 0x50485342; // BSHP
constexpr uint32_t kHeapVersion = 2;
constexpr uint32_t kObjectHeaderSize = 20;

enum class BuildingSaveObjectKind : uint32_t {
    Instance = 1,
    House = 2,
    Supplier = 3,
    Dock = 4,
    Depot = 5,
    Industry = 6,
    Distribution = 7,
    Rubble = 8,
    Roadblock = 9,
    Entertainment = 10,
    Monument = 11,
    NativeStorage = 12,
    NativeProduction = 13,
    Extension = 14,
    Count = 15
};

class HeapWriter {
public:
    void write_u8(uint8_t value) { bytes_.push_back(value); }
    void write_i8(int8_t value) { write_u8(static_cast<uint8_t>(value)); }
    void write_u16(uint16_t value)
    {
        write_u8(static_cast<uint8_t>(value & 0xff));
        write_u8(static_cast<uint8_t>((value >> 8) & 0xff));
    }
    void write_i16(int16_t value) { write_u16(static_cast<uint16_t>(value)); }
    void write_u32(uint32_t value)
    {
        write_u8(static_cast<uint8_t>(value & 0xff));
        write_u8(static_cast<uint8_t>((value >> 8) & 0xff));
        write_u8(static_cast<uint8_t>((value >> 16) & 0xff));
        write_u8(static_cast<uint8_t>((value >> 24) & 0xff));
    }
    void write_i32(int32_t value) { write_u32(static_cast<uint32_t>(value)); }
    void append(const std::vector<uint8_t> &bytes)
    {
        bytes_.insert(bytes_.end(), bytes.begin(), bytes.end());
    }
    const std::vector<uint8_t> &bytes() const { return bytes_; }
    size_t size() const { return bytes_.size(); }

private:
    std::vector<uint8_t> bytes_;
};

static void fatal_save_heap(const char *message, const char *detail)
{
    ErrorContextScope scope("building_save_heap", detail);
    error_context_report_fatal_error_dialog("Vespasian Save Data Error", message, detail);
}

static uint16_t save_id_from_runtime_type(building_type runtime_id)
{
    return building_type_id_bridge_save_id_from_runtime(runtime_id);
}

static building_type runtime_type_from_save_id(uint16_t save_id, unsigned int building_id, uint8_t building_state)
{
    if (building_state != BUILDING_STATE_UNUSED && !building_type_id_bridge_save_id_is_in_table(save_id)) {
        char detail[160];
        snprintf(detail, sizeof(detail), "building %u save type id %u is outside the loaded table", building_id, save_id);
        fatal_save_heap("0xb6 building heap attempted to use a legacy enum fallback BuildingType id", detail);
    }
    if (building_type_id_bridge_save_id_is_missing(save_id)) {
        char detail[160];
        snprintf(detail, sizeof(detail), "building %u save type id %u is marked missing", building_id, save_id);
        fatal_save_heap("0xb6 building heap referenced a missing BuildingType", detail);
    }
    building_type runtime_id = building_type_id_bridge_runtime_from_save_id(save_id);
    if (building_state != BUILDING_STATE_UNUSED && save_id && runtime_id == BUILDING_NONE) {
        char detail[160];
        snprintf(detail, sizeof(detail), "building %u save type id %u resolved to BUILDING_NONE", building_id, save_id);
        fatal_save_heap("0xb6 building heap could not resolve a BuildingType", detail);
    }
    return runtime_id;
}

static bool is_supplier_payload_type(building_type type)
{
    return type == BUILDING_CARAVANSERAI || type == BUILDING_LARGE_TEMPLE_CERES ||
        type == BUILDING_LARGE_TEMPLE_VENUS || building_has_supplier_inventory(type);
}

static bool is_rubble_payload_type(building_type type)
{
    return type == BUILDING_BURNING_RUIN || type == BUILDING_WAREHOUSE_SPACE || type == BUILDING_WAREHOUSE;
}

static bool is_industry_payload_type(const building *b)
{
    return b->output_resource_id || b->type == BUILDING_NATIVE_CROPS ||
        b->type == BUILDING_SHIPYARD || b->type == BUILDING_WHARF;
}

static bool building_has_saved_monument_attributes(const building *b)
{
    return b && (b->monument.phase || b->monument.progress || b->monument.upgrades ||
        b->monument.secondary_frame);
}

class BuildingSaveObject {
public:
    BuildingSaveObject(BuildingSaveObjectKind kind, uint32_t object_id)
        : kind_(kind), object_id_(object_id)
    {
    }
    virtual ~BuildingSaveObject() = default;

    BuildingSaveObjectKind kind() const { return kind_; }
    uint32_t object_id() const { return object_id_; }
    uint32_t version() const { return 1; }
    std::vector<uint8_t> payload() const
    {
        HeapWriter writer;
        write_payload(writer);
        return writer.bytes();
    }
    virtual void apply(building *b) const = 0;

private:
    virtual void write_payload(HeapWriter &writer) const = 0;

    BuildingSaveObjectKind kind_;
    uint32_t object_id_;
};

class BuildingInstanceSaveObject final : public BuildingSaveObject {
public:
    explicit BuildingInstanceSaveObject(const building *b)
        : BuildingSaveObject(BuildingSaveObjectKind::Instance, b->id)
    {
        id = b->id;
        state = b->state;
        faction_id = b->faction_id;
        unknown_value = b->unknown_value;
        size = b->size;
        house_is_merged = b->house_is_merged;
        house_size = b->house_size;
        x = b->x;
        y = b->y;
        grid_offset = b->grid_offset;
        type_save_id = save_id_from_runtime_type(b->type);
        if (b->state != BUILDING_STATE_UNUSED && !type_save_id) {
            char detail[160];
            snprintf(detail, sizeof(detail), "building %u runtime type %d", b->id, static_cast<int>(b->type));
            fatal_save_heap("0xb6 building heap refused to save an active building without a BuildingType save id", detail);
        }
        subtype_raw = b->subtype.house_level;
        road_network_id = b->road_network_id;
        created_sequence = b->created_sequence;
        houses_covered = b->houses_covered;
        percentage_houses_covered = b->percentage_houses_covered;
        house_population = b->house_population;
        local_workforce_assigned = b->local_workforce_assigned;
        local_workforce_unemployed = b->local_workforce_unemployed;
        house_population_room = b->house_population_room;
        distance_from_entry = b->distance_from_entry;
        house_highest_population = b->house_highest_population;
        house_unreachable_ticks = b->house_unreachable_ticks;
        road_access_x = b->road_access_x;
        road_access_y = b->road_access_y;
        figure_id = b->figure_id;
        figure_id2 = b->figure_id2;
        immigrant_figure_id = b->immigrant_figure_id;
        figure_id4 = b->figure_id4;
        figure_spawn_delay = b->figure_spawn_delay;
        local_workforce_validation_delay = b->local_workforce_validation_delay;
        days_since_offering = b->days_since_offering;
        figure_roam_direction = b->figure_roam_direction;
        has_water_access = b->has_water_access;
        prev_part_building_id = b->prev_part_building_id;
        next_part_building_id = b->next_part_building_id;
        house_sentiment_message = b->house_sentiment_message;
        has_well_access = b->has_well_access;
        num_workers = b->num_workers;
        labor_category = b->labor_category;
        output_resource_id = b->output_resource_id;
        has_road_access = b->has_road_access;
        house_criminal_active = b->house_criminal_active;
        damage_risk = b->damage_risk;
        fire_risk = b->fire_risk;
        fire_duration = b->fire_duration;
        fire_proof = b->fire_proof;
        house_figure_generation_delay = b->house_figure_generation_delay;
        house_tax_coverage = b->house_tax_coverage;
        house_pantheon_access = b->house_pantheon_access;
        formation_id = b->formation_id;
        monthly_levy = b->monthly_levy;
        tax_income_or_storage = b->tax_income_or_storage;
        house_days_without_food = b->house_days_without_food;
        has_plague = b->has_plague;
        desirability = b->desirability;
        is_deleted = b->is_deleted;
        is_close_to_water = b->is_close_to_water;
        storage_id = b->storage_id;
        sentiment_raw = b->sentiment.house_happiness;
        show_on_problem_overlay = b->show_on_problem_overlay;
        house_tavern_wine_access = b->house_tavern_wine_access;
        house_tavern_food_access = b->house_tavern_food_access;
        house_arena_gladiator = b->house_arena_gladiator;
        house_arena_lion = b->house_arena_lion;
        is_tourism_venue = b->is_tourism_venue;
        tourism_disabled = b->tourism_disabled;
        tourism_income = b->tourism_income;
        tourism_income_this_year = b->tourism_income_this_year;
        variant = b->variant;
        upgrade_level = b->upgrade_level;
        strike_duration_days = b->strike_duration_days;
        sickness_level = b->sickness_level;
        sickness_duration = b->sickness_duration;
        sickness_doctor_cure = b->sickness_doctor_cure;
        fumigation_frame = b->fumigation_frame;
        fumigation_direction = b->fumigation_direction;
        has_latrines_access = b->has_latrines_access;
        for (int i = 0; i < RESOURCE_MAX; ++i) {
            resources[i] = b->resources[i];
            accepted_goods[i] = b->accepted_goods[i];
        }
    }

    BuildingInstanceSaveObject(buffer *payload, uint32_t object_id)
        : BuildingSaveObject(BuildingSaveObjectKind::Instance, object_id)
    {
        id = buffer_read_u32(payload);
        if (id != object_id) {
            fatal_save_heap("0xb6 building heap instance id does not match its object header", "BuildingInstanceSaveObject");
        }
        state = buffer_read_u8(payload);
        faction_id = buffer_read_u8(payload);
        unknown_value = buffer_read_u8(payload);
        size = buffer_read_u8(payload);
        house_is_merged = buffer_read_u8(payload);
        house_size = buffer_read_u8(payload);
        x = buffer_read_u8(payload);
        y = buffer_read_u8(payload);
        grid_offset = buffer_read_i16(payload);
        type_save_id = buffer_read_u16(payload);
        subtype_raw = buffer_read_i16(payload);
        road_network_id = buffer_read_u8(payload);
        created_sequence = buffer_read_u16(payload);
        houses_covered = buffer_read_i16(payload);
        percentage_houses_covered = buffer_read_i16(payload);
        house_population = buffer_read_i16(payload);
        local_workforce_assigned = buffer_read_i16(payload);
        local_workforce_unemployed = buffer_read_i16(payload);
        house_population_room = buffer_read_i16(payload);
        distance_from_entry = buffer_read_i16(payload);
        house_highest_population = buffer_read_i16(payload);
        house_unreachable_ticks = buffer_read_i16(payload);
        road_access_x = buffer_read_u8(payload);
        road_access_y = buffer_read_u8(payload);
        figure_id = buffer_read_u32(payload);
        figure_id2 = buffer_read_u32(payload);
        immigrant_figure_id = buffer_read_u32(payload);
        figure_id4 = buffer_read_u32(payload);
        figure_spawn_delay = buffer_read_u8(payload);
        local_workforce_validation_delay = buffer_read_u8(payload);
        days_since_offering = buffer_read_u8(payload);
        figure_roam_direction = buffer_read_u8(payload);
        has_water_access = buffer_read_u8(payload);
        prev_part_building_id = buffer_read_i16(payload);
        next_part_building_id = buffer_read_i16(payload);
        house_sentiment_message = buffer_read_u8(payload);
        has_well_access = buffer_read_u8(payload);
        num_workers = buffer_read_i16(payload);
        labor_category = buffer_read_u8(payload);
        output_resource_id = buffer_read_u8(payload);
        has_road_access = buffer_read_u8(payload);
        house_criminal_active = buffer_read_u8(payload);
        damage_risk = buffer_read_i16(payload);
        fire_risk = buffer_read_i16(payload);
        fire_duration = buffer_read_i16(payload);
        fire_proof = buffer_read_u8(payload);
        house_figure_generation_delay = buffer_read_u8(payload);
        house_tax_coverage = buffer_read_u8(payload);
        house_pantheon_access = buffer_read_u8(payload);
        formation_id = buffer_read_i16(payload);
        monthly_levy = buffer_read_i8(payload);
        tax_income_or_storage = buffer_read_i32(payload);
        house_days_without_food = buffer_read_u8(payload);
        has_plague = buffer_read_u8(payload);
        desirability = buffer_read_i8(payload);
        is_deleted = buffer_read_u8(payload);
        is_close_to_water = buffer_read_u8(payload);
        storage_id = buffer_read_u8(payload);
        sentiment_raw = buffer_read_i8(payload);
        show_on_problem_overlay = buffer_read_u8(payload);
        house_tavern_wine_access = buffer_read_u8(payload);
        house_tavern_food_access = buffer_read_u8(payload);
        house_arena_gladiator = buffer_read_u8(payload);
        house_arena_lion = buffer_read_u8(payload);
        is_tourism_venue = buffer_read_u8(payload);
        tourism_disabled = buffer_read_u8(payload);
        tourism_income = buffer_read_u8(payload);
        tourism_income_this_year = buffer_read_u8(payload);
        variant = buffer_read_u8(payload);
        upgrade_level = buffer_read_u8(payload);
        strike_duration_days = buffer_read_u8(payload);
        sickness_level = buffer_read_u8(payload);
        sickness_duration = buffer_read_u8(payload);
        sickness_doctor_cure = buffer_read_u8(payload);
        fumigation_frame = buffer_read_u8(payload);
        fumigation_direction = buffer_read_u8(payload);
        has_latrines_access = buffer_read_u8(payload);

        uint16_t resource_count = buffer_read_u16(payload);
        if (resource_count > RESOURCE_MAX) {
            fatal_save_heap("0xb6 building heap has too many resource entries", "BuildingInstanceSaveObject.resources");
        }
        for (uint16_t i = 0; i < resource_count; ++i) {
            resources[i] = buffer_read_i16(payload);
        }
        uint16_t accepted_count = buffer_read_u16(payload);
        if (accepted_count > RESOURCE_MAX) {
            fatal_save_heap("0xb6 building heap has too many accepted-good entries", "BuildingInstanceSaveObject.accepted_goods");
        }
        for (uint16_t i = 0; i < accepted_count; ++i) {
            accepted_goods[i] = buffer_read_u8(payload);
        }
    }

    uint32_t saved_id() const { return id; }
    uint8_t saved_state() const { return state; }
    uint16_t saved_type_id() const { return type_save_id; }

    void apply(building *b) const override
    {
        b->id = id;
        b->state = state;
        b->faction_id = faction_id;
        b->unknown_value = unknown_value;
        b->size = size;
        b->house_is_merged = house_is_merged;
        b->house_size = house_size;
        b->x = x;
        b->y = y;
        b->grid_offset = grid_offset;
        b->type = runtime_type_from_save_id(type_save_id, id, state);
        b->subtype.house_level = subtype_raw;
        b->road_network_id = road_network_id;
        b->created_sequence = created_sequence;
        b->houses_covered = houses_covered;
        b->percentage_houses_covered = percentage_houses_covered;
        b->house_population = house_population;
        b->local_workforce_assigned = local_workforce_assigned;
        b->local_workforce_unemployed = local_workforce_unemployed;
        b->house_population_room = house_population_room;
        b->distance_from_entry = distance_from_entry;
        b->house_highest_population = house_highest_population;
        b->house_unreachable_ticks = house_unreachable_ticks;
        b->road_access_x = road_access_x;
        b->road_access_y = road_access_y;
        b->figure_id = figure_id;
        b->figure_id2 = figure_id2;
        b->immigrant_figure_id = immigrant_figure_id;
        b->figure_id4 = figure_id4;
        b->figure_spawn_delay = figure_spawn_delay;
        b->local_workforce_validation_delay = local_workforce_validation_delay;
        b->days_since_offering = days_since_offering;
        b->figure_roam_direction = figure_roam_direction;
        b->has_water_access = has_water_access;
        b->prev_part_building_id = prev_part_building_id;
        b->next_part_building_id = next_part_building_id;
        b->house_sentiment_message = house_sentiment_message;
        b->has_well_access = has_well_access;
        b->num_workers = num_workers;
        b->labor_category = labor_category;
        b->output_resource_id = output_resource_id;
        b->has_road_access = has_road_access;
        b->house_criminal_active = house_criminal_active;
        b->damage_risk = damage_risk;
        b->fire_risk = fire_risk;
        b->fire_duration = fire_duration;
        b->fire_proof = fire_proof;
        b->house_figure_generation_delay = house_figure_generation_delay;
        b->house_tax_coverage = house_tax_coverage;
        b->house_pantheon_access = house_pantheon_access;
        b->formation_id = formation_id;
        b->monthly_levy = monthly_levy;
        b->tax_income_or_storage = tax_income_or_storage;
        b->house_days_without_food = house_days_without_food;
        b->has_plague = has_plague;
        b->desirability = desirability;
        b->is_deleted = is_deleted;
        b->is_close_to_water = is_close_to_water;
        b->storage_id = storage_id;
        b->sentiment.house_happiness = sentiment_raw;
        b->show_on_problem_overlay = show_on_problem_overlay;
        b->house_tavern_wine_access = house_tavern_wine_access;
        b->house_tavern_food_access = house_tavern_food_access;
        b->house_arena_gladiator = house_arena_gladiator;
        b->house_arena_lion = house_arena_lion;
        b->is_tourism_venue = is_tourism_venue;
        b->tourism_disabled = tourism_disabled;
        b->tourism_income = tourism_income;
        b->tourism_income_this_year = tourism_income_this_year;
        b->variant = variant;
        b->upgrade_level = upgrade_level;
        b->strike_duration_days = strike_duration_days;
        b->sickness_level = sickness_level;
        b->sickness_duration = sickness_duration;
        b->sickness_doctor_cure = sickness_doctor_cure;
        b->fumigation_frame = fumigation_frame;
        b->fumigation_direction = fumigation_direction;
        b->has_latrines_access = has_latrines_access;
        for (int i = 0; i < RESOURCE_MAX; ++i) {
            b->resources[i] = resources[i];
            b->accepted_goods[i] = accepted_goods[i];
        }
    }

private:
    void write_payload(HeapWriter &writer) const override
    {
        writer.write_u32(id);
        writer.write_u8(state);
        writer.write_u8(faction_id);
        writer.write_u8(unknown_value);
        writer.write_u8(size);
        writer.write_u8(house_is_merged);
        writer.write_u8(house_size);
        writer.write_u8(x);
        writer.write_u8(y);
        writer.write_i16(grid_offset);
        writer.write_u16(type_save_id);
        writer.write_i16(subtype_raw);
        writer.write_u8(road_network_id);
        writer.write_u16(created_sequence);
        writer.write_i16(houses_covered);
        writer.write_i16(percentage_houses_covered);
        writer.write_i16(house_population);
        writer.write_i16(local_workforce_assigned);
        writer.write_i16(local_workforce_unemployed);
        writer.write_i16(house_population_room);
        writer.write_i16(distance_from_entry);
        writer.write_i16(house_highest_population);
        writer.write_i16(house_unreachable_ticks);
        writer.write_u8(road_access_x);
        writer.write_u8(road_access_y);
        writer.write_u32(figure_id);
        writer.write_u32(figure_id2);
        writer.write_u32(immigrant_figure_id);
        writer.write_u32(figure_id4);
        writer.write_u8(figure_spawn_delay);
        writer.write_u8(local_workforce_validation_delay);
        writer.write_u8(days_since_offering);
        writer.write_u8(figure_roam_direction);
        writer.write_u8(has_water_access);
        writer.write_i16(prev_part_building_id);
        writer.write_i16(next_part_building_id);
        writer.write_u8(house_sentiment_message);
        writer.write_u8(has_well_access);
        writer.write_i16(num_workers);
        writer.write_u8(labor_category);
        writer.write_u8(output_resource_id);
        writer.write_u8(has_road_access);
        writer.write_u8(house_criminal_active);
        writer.write_i16(damage_risk);
        writer.write_i16(fire_risk);
        writer.write_i16(fire_duration);
        writer.write_u8(fire_proof);
        writer.write_u8(house_figure_generation_delay);
        writer.write_u8(house_tax_coverage);
        writer.write_u8(house_pantheon_access);
        writer.write_i16(formation_id);
        writer.write_i8(monthly_levy);
        writer.write_i32(tax_income_or_storage);
        writer.write_u8(house_days_without_food);
        writer.write_u8(has_plague);
        writer.write_i8(desirability);
        writer.write_u8(is_deleted);
        writer.write_u8(is_close_to_water);
        writer.write_u8(storage_id);
        writer.write_i8(sentiment_raw);
        writer.write_u8(show_on_problem_overlay);
        writer.write_u8(house_tavern_wine_access);
        writer.write_u8(house_tavern_food_access);
        writer.write_u8(house_arena_gladiator);
        writer.write_u8(house_arena_lion);
        writer.write_u8(is_tourism_venue);
        writer.write_u8(tourism_disabled);
        writer.write_u8(tourism_income);
        writer.write_u8(tourism_income_this_year);
        writer.write_u8(variant);
        writer.write_u8(upgrade_level);
        writer.write_u8(strike_duration_days);
        writer.write_u8(sickness_level);
        writer.write_u8(sickness_duration);
        writer.write_u8(sickness_doctor_cure);
        writer.write_u8(fumigation_frame);
        writer.write_u8(fumigation_direction);
        writer.write_u8(has_latrines_access);
        writer.write_u16(RESOURCE_MAX);
        for (int i = 0; i < RESOURCE_MAX; ++i) {
            writer.write_i16(resources[i]);
        }
        writer.write_u16(RESOURCE_MAX);
        for (int i = 0; i < RESOURCE_MAX; ++i) {
            writer.write_u8(accepted_goods[i]);
        }
    }

    uint32_t id = 0;
    uint8_t state = 0;
    uint8_t faction_id = 0;
    uint8_t unknown_value = 0;
    uint8_t size = 0;
    uint8_t house_is_merged = 0;
    uint8_t house_size = 0;
    uint8_t x = 0;
    uint8_t y = 0;
    int16_t grid_offset = 0;
    uint16_t type_save_id = 0;
    int16_t subtype_raw = 0;
    uint8_t road_network_id = 0;
    uint16_t created_sequence = 0;
    int16_t houses_covered = 0;
    int16_t percentage_houses_covered = 0;
    int16_t house_population = 0;
    int16_t local_workforce_assigned = 0;
    int16_t local_workforce_unemployed = 0;
    int16_t house_population_room = 0;
    int16_t distance_from_entry = 0;
    int16_t house_highest_population = 0;
    int16_t house_unreachable_ticks = 0;
    uint8_t road_access_x = 0;
    uint8_t road_access_y = 0;
    uint32_t figure_id = 0;
    uint32_t figure_id2 = 0;
    uint32_t immigrant_figure_id = 0;
    uint32_t figure_id4 = 0;
    uint8_t figure_spawn_delay = 0;
    uint8_t local_workforce_validation_delay = 0;
    uint8_t days_since_offering = 0;
    uint8_t figure_roam_direction = 0;
    uint8_t has_water_access = 0;
    int16_t prev_part_building_id = 0;
    int16_t next_part_building_id = 0;
    uint8_t house_sentiment_message = 0;
    uint8_t has_well_access = 0;
    int16_t num_workers = 0;
    uint8_t labor_category = 0;
    uint8_t output_resource_id = 0;
    uint8_t has_road_access = 0;
    uint8_t house_criminal_active = 0;
    int16_t damage_risk = 0;
    int16_t fire_risk = 0;
    int16_t fire_duration = 0;
    uint8_t fire_proof = 0;
    uint8_t house_figure_generation_delay = 0;
    uint8_t house_tax_coverage = 0;
    uint8_t house_pantheon_access = 0;
    int16_t formation_id = 0;
    int8_t monthly_levy = 0;
    int32_t tax_income_or_storage = 0;
    uint8_t house_days_without_food = 0;
    uint8_t has_plague = 0;
    int8_t desirability = 0;
    uint8_t is_deleted = 0;
    uint8_t is_close_to_water = 0;
    uint8_t storage_id = 0;
    int8_t sentiment_raw = 0;
    uint8_t show_on_problem_overlay = 0;
    uint8_t house_tavern_wine_access = 0;
    uint8_t house_tavern_food_access = 0;
    uint8_t house_arena_gladiator = 0;
    uint8_t house_arena_lion = 0;
    uint8_t is_tourism_venue = 0;
    uint8_t tourism_disabled = 0;
    uint8_t tourism_income = 0;
    uint8_t tourism_income_this_year = 0;
    uint8_t variant = 0;
    uint8_t upgrade_level = 0;
    uint8_t strike_duration_days = 0;
    uint8_t sickness_level = 0;
    uint8_t sickness_duration = 0;
    uint8_t sickness_doctor_cure = 0;
    uint8_t fumigation_frame = 0;
    uint8_t fumigation_direction = 0;
    uint8_t has_latrines_access = 0;
    int16_t resources[RESOURCE_MAX] = { 0 };
    uint8_t accepted_goods[RESOURCE_MAX] = { 0 };
};

template <BuildingSaveObjectKind Kind>
class EmptyPayloadObject : public BuildingSaveObject {
public:
    explicit EmptyPayloadObject(uint32_t object_id)
        : BuildingSaveObject(Kind, object_id)
    {
    }
    explicit EmptyPayloadObject(buffer *, uint32_t object_id)
        : BuildingSaveObject(Kind, object_id)
    {
    }
    void apply(building *) const override {}

private:
    void write_payload(HeapWriter &) const override {}
};

class HouseSaveObject final : public BuildingSaveObject {
public:
    explicit HouseSaveObject(const building *b)
        : BuildingSaveObject(BuildingSaveObjectKind::House, b->id)
    {
        theater = b->data.house.theater;
        amphitheater_actor = b->data.house.amphitheater_actor;
        amphitheater_gladiator = b->data.house.amphitheater_gladiator;
        colosseum_gladiator = b->data.house.colosseum_gladiator;
        colosseum_lion = b->data.house.colosseum_lion;
        hippodrome = b->data.house.hippodrome;
        school = b->data.house.school;
        library = b->data.house.library;
        academy = b->data.house.academy;
        barber = b->data.house.barber;
        clinic = b->data.house.clinic;
        bathhouse = b->data.house.bathhouse;
        hospital = b->data.house.hospital;
        temple_ceres = b->data.house.temple_ceres;
        temple_neptune = b->data.house.temple_neptune;
        temple_mercury = b->data.house.temple_mercury;
        temple_mars = b->data.house.temple_mars;
        temple_venus = b->data.house.temple_venus;
        no_space_to_expand = b->data.house.no_space_to_expand;
        num_foods = b->data.house.num_foods;
        entertainment = b->data.house.entertainment;
        education = b->data.house.education;
        health = b->data.house.health;
        num_gods = b->data.house.num_gods;
        devolve_delay = b->data.house.devolve_delay;
        evolve_text_id = b->data.house.evolve_text_id;
    }
    explicit HouseSaveObject(buffer *payload, uint32_t object_id)
        : BuildingSaveObject(BuildingSaveObjectKind::House, object_id)
    {
        theater = buffer_read_u8(payload);
        amphitheater_actor = buffer_read_u8(payload);
        amphitheater_gladiator = buffer_read_u8(payload);
        colosseum_gladiator = buffer_read_u8(payload);
        colosseum_lion = buffer_read_u8(payload);
        hippodrome = buffer_read_u8(payload);
        school = buffer_read_u8(payload);
        library = buffer_read_u8(payload);
        academy = buffer_read_u8(payload);
        barber = buffer_read_u8(payload);
        clinic = buffer_read_u8(payload);
        bathhouse = buffer_read_u8(payload);
        hospital = buffer_read_u8(payload);
        temple_ceres = buffer_read_u8(payload);
        temple_neptune = buffer_read_u8(payload);
        temple_mercury = buffer_read_u8(payload);
        temple_mars = buffer_read_u8(payload);
        temple_venus = buffer_read_u8(payload);
        no_space_to_expand = buffer_read_u8(payload);
        num_foods = buffer_read_u8(payload);
        entertainment = buffer_read_u8(payload);
        education = buffer_read_u8(payload);
        health = buffer_read_u8(payload);
        num_gods = buffer_read_u8(payload);
        devolve_delay = buffer_read_u8(payload);
        evolve_text_id = buffer_read_u8(payload);
    }
    void apply(building *b) const override
    {
        b->data.house.theater = theater;
        b->data.house.amphitheater_actor = amphitheater_actor;
        b->data.house.amphitheater_gladiator = amphitheater_gladiator;
        b->data.house.colosseum_gladiator = colosseum_gladiator;
        b->data.house.colosseum_lion = colosseum_lion;
        b->data.house.hippodrome = hippodrome;
        b->data.house.school = school;
        b->data.house.library = library;
        b->data.house.academy = academy;
        b->data.house.barber = barber;
        b->data.house.clinic = clinic;
        b->data.house.bathhouse = bathhouse;
        b->data.house.hospital = hospital;
        b->data.house.temple_ceres = temple_ceres;
        b->data.house.temple_neptune = temple_neptune;
        b->data.house.temple_mercury = temple_mercury;
        b->data.house.temple_mars = temple_mars;
        b->data.house.temple_venus = temple_venus;
        b->data.house.no_space_to_expand = no_space_to_expand;
        b->data.house.num_foods = num_foods;
        b->data.house.entertainment = entertainment;
        b->data.house.education = education;
        b->data.house.health = health;
        b->data.house.num_gods = num_gods;
        b->data.house.devolve_delay = devolve_delay;
        b->data.house.evolve_text_id = evolve_text_id;
    }

private:
    void write_payload(HeapWriter &writer) const override
    {
        writer.write_u8(theater);
        writer.write_u8(amphitheater_actor);
        writer.write_u8(amphitheater_gladiator);
        writer.write_u8(colosseum_gladiator);
        writer.write_u8(colosseum_lion);
        writer.write_u8(hippodrome);
        writer.write_u8(school);
        writer.write_u8(library);
        writer.write_u8(academy);
        writer.write_u8(barber);
        writer.write_u8(clinic);
        writer.write_u8(bathhouse);
        writer.write_u8(hospital);
        writer.write_u8(temple_ceres);
        writer.write_u8(temple_neptune);
        writer.write_u8(temple_mercury);
        writer.write_u8(temple_mars);
        writer.write_u8(temple_venus);
        writer.write_u8(no_space_to_expand);
        writer.write_u8(num_foods);
        writer.write_u8(entertainment);
        writer.write_u8(education);
        writer.write_u8(health);
        writer.write_u8(num_gods);
        writer.write_u8(devolve_delay);
        writer.write_u8(evolve_text_id);
    }

    uint8_t theater = 0;
    uint8_t amphitheater_actor = 0;
    uint8_t amphitheater_gladiator = 0;
    uint8_t colosseum_gladiator = 0;
    uint8_t colosseum_lion = 0;
    uint8_t hippodrome = 0;
    uint8_t school = 0;
    uint8_t library = 0;
    uint8_t academy = 0;
    uint8_t barber = 0;
    uint8_t clinic = 0;
    uint8_t bathhouse = 0;
    uint8_t hospital = 0;
    uint8_t temple_ceres = 0;
    uint8_t temple_neptune = 0;
    uint8_t temple_mercury = 0;
    uint8_t temple_mars = 0;
    uint8_t temple_venus = 0;
    uint8_t no_space_to_expand = 0;
    uint8_t num_foods = 0;
    uint8_t entertainment = 0;
    uint8_t education = 0;
    uint8_t health = 0;
    uint8_t num_gods = 0;
    uint8_t devolve_delay = 0;
    uint8_t evolve_text_id = 0;
};

class SupplierSaveObject final : public BuildingSaveObject {
public:
    explicit SupplierSaveObject(const building *b)
        : BuildingSaveObject(BuildingSaveObjectKind::Supplier, b->id),
          fetch_inventory_id(b->data.market.fetch_inventory_id),
          is_mess_hall(b->data.market.is_mess_hall)
    {
    }
    explicit SupplierSaveObject(buffer *payload, uint32_t object_id)
        : BuildingSaveObject(BuildingSaveObjectKind::Supplier, object_id),
          fetch_inventory_id(buffer_read_u8(payload)),
          is_mess_hall(buffer_read_u8(payload))
    {
    }
    void apply(building *b) const override
    {
        b->data.market.fetch_inventory_id = fetch_inventory_id;
        b->data.market.is_mess_hall = is_mess_hall;
    }

private:
    void write_payload(HeapWriter &writer) const override
    {
        writer.write_u8(fetch_inventory_id);
        writer.write_u8(is_mess_hall);
    }

    uint8_t fetch_inventory_id = 0;
    uint8_t is_mess_hall = 0;
};

class DockSaveObject final : public BuildingSaveObject {
public:
    explicit DockSaveObject(const building *b)
        : BuildingSaveObject(BuildingSaveObjectKind::Dock, b->id)
    {
        queued_docker_id = b->data.dock.queued_docker_id;
        num_ships = b->data.dock.num_ships;
        orientation = b->data.dock.orientation;
        trade_ship_id = b->data.dock.trade_ship_id;
        has_accepted_route_ids = b->data.dock.has_accepted_route_ids;
        accepted_route_ids = b->data.dock.accepted_route_ids;
        for (int i = 0; i < 3; ++i) {
            cartpusher_ids[i] = b->data.distribution.cartpusher_ids[i];
        }
    }
    explicit DockSaveObject(buffer *payload, uint32_t object_id)
        : BuildingSaveObject(BuildingSaveObjectKind::Dock, object_id)
    {
        queued_docker_id = buffer_read_i16(payload);
        num_ships = buffer_read_u8(payload);
        orientation = buffer_read_i8(payload);
        trade_ship_id = buffer_read_i16(payload);
        has_accepted_route_ids = buffer_read_u8(payload);
        accepted_route_ids = buffer_read_i32(payload);
        for (int i = 0; i < 3; ++i) {
            cartpusher_ids[i] = buffer_read_u32(payload);
        }
    }
    void apply(building *b) const override
    {
        b->data.dock.queued_docker_id = queued_docker_id;
        b->data.dock.num_ships = num_ships;
        b->data.dock.orientation = orientation;
        b->data.dock.trade_ship_id = trade_ship_id;
        b->data.dock.has_accepted_route_ids = has_accepted_route_ids;
        b->data.dock.accepted_route_ids = accepted_route_ids;
        for (int i = 0; i < 3; ++i) {
            b->data.distribution.cartpusher_ids[i] = cartpusher_ids[i];
        }
    }

private:
    void write_payload(HeapWriter &writer) const override
    {
        writer.write_i16(queued_docker_id);
        writer.write_u8(num_ships);
        writer.write_i8(orientation);
        writer.write_i16(trade_ship_id);
        writer.write_u8(has_accepted_route_ids);
        writer.write_i32(accepted_route_ids);
        for (int i = 0; i < 3; ++i) {
            writer.write_u32(cartpusher_ids[i]);
        }
    }

    int16_t queued_docker_id = 0;
    uint8_t num_ships = 0;
    int8_t orientation = 0;
    int16_t trade_ship_id = 0;
    uint8_t has_accepted_route_ids = 0;
    int32_t accepted_route_ids = 0;
    uint32_t cartpusher_ids[3] = { 0, 0, 0 };
};

class DepotSaveObject final : public BuildingSaveObject {
public:
    explicit DepotSaveObject(const building *b)
        : BuildingSaveObject(BuildingSaveObjectKind::Depot, b->id)
    {
        resource = b->data.depot.current_order.resource_type;
        src_storage_id = b->data.depot.current_order.src_storage_id;
        dst_storage_id = b->data.depot.current_order.dst_storage_id;
        condition_type = b->data.depot.current_order.condition.condition_type;
        threshold = b->data.depot.current_order.condition.threshold;
        for (int i = 0; i < 3; ++i) {
            cartpusher_ids[i] = b->data.distribution.cartpusher_ids[i];
        }
    }
    explicit DepotSaveObject(buffer *payload, uint32_t object_id)
        : BuildingSaveObject(BuildingSaveObjectKind::Depot, object_id)
    {
        resource = static_cast<resource_type>(buffer_read_i32(payload));
        src_storage_id = buffer_read_u32(payload);
        dst_storage_id = buffer_read_u32(payload);
        condition_type = static_cast<order_condition_type>(buffer_read_i32(payload));
        threshold = buffer_read_i32(payload);
        for (int i = 0; i < 3; ++i) {
            cartpusher_ids[i] = buffer_read_u32(payload);
        }
    }
    void apply(building *b) const override
    {
        b->data.depot.current_order.resource_type = resource;
        b->data.depot.current_order.src_storage_id = src_storage_id;
        b->data.depot.current_order.dst_storage_id = dst_storage_id;
        b->data.depot.current_order.condition.condition_type = condition_type;
        b->data.depot.current_order.condition.threshold = threshold;
        for (int i = 0; i < 3; ++i) {
            b->data.distribution.cartpusher_ids[i] = cartpusher_ids[i];
        }
    }

private:
    void write_payload(HeapWriter &writer) const override
    {
        writer.write_i32(static_cast<int32_t>(resource));
        writer.write_u32(src_storage_id);
        writer.write_u32(dst_storage_id);
        writer.write_i32(static_cast<int32_t>(condition_type));
        writer.write_i32(threshold);
        for (int i = 0; i < 3; ++i) {
            writer.write_u32(cartpusher_ids[i]);
        }
    }

    resource_type resource = RESOURCE_NONE;
    uint32_t src_storage_id = 0;
    uint32_t dst_storage_id = 0;
    order_condition_type condition_type = ORDER_CONDITION_NEVER;
    int32_t threshold = 0;
    uint32_t cartpusher_ids[3] = { 0, 0, 0 };
};

class IndustrySaveObject final : public BuildingSaveObject {
public:
    explicit IndustrySaveObject(const building *b)
        : BuildingSaveObject(BuildingSaveObjectKind::Industry, b->id)
    {
        progress = b->data.industry.progress;
        blessing_days_left = b->data.industry.blessing_days_left;
        curse_days_left = b->data.industry.curse_days_left;
        has_raw_materials = b->data.industry.has_raw_materials;
        has_fish = b->data.industry.has_fish;
        is_stockpiling = b->data.industry.is_stockpiling;
        orientation = b->data.industry.orientation;
        fishing_boat_id = b->data.industry.fishing_boat_id;
        age_months = b->data.industry.age_months;
        average_production_per_month = b->data.industry.average_production_per_month;
        production_current_month = b->data.industry.production_current_month;
    }
    explicit IndustrySaveObject(buffer *payload, uint32_t object_id)
        : BuildingSaveObject(BuildingSaveObjectKind::Industry, object_id)
    {
        progress = buffer_read_i16(payload);
        blessing_days_left = buffer_read_u8(payload);
        curse_days_left = buffer_read_u8(payload);
        has_raw_materials = buffer_read_u8(payload);
        has_fish = buffer_read_u8(payload);
        is_stockpiling = buffer_read_u8(payload);
        orientation = buffer_read_u8(payload);
        fishing_boat_id = buffer_read_u32(payload);
        age_months = buffer_read_u8(payload);
        average_production_per_month = buffer_read_u8(payload);
        production_current_month = buffer_read_i16(payload);
    }
    void apply(building *b) const override
    {
        b->data.industry.progress = progress;
        b->data.industry.blessing_days_left = blessing_days_left;
        b->data.industry.curse_days_left = curse_days_left;
        b->data.industry.has_raw_materials = has_raw_materials;
        b->data.industry.has_fish = has_fish;
        b->data.industry.is_stockpiling = is_stockpiling;
        b->data.industry.orientation = orientation;
        b->data.industry.fishing_boat_id = fishing_boat_id;
        b->data.industry.age_months = age_months;
        b->data.industry.average_production_per_month = average_production_per_month;
        b->data.industry.production_current_month = production_current_month;
    }

private:
    void write_payload(HeapWriter &writer) const override
    {
        writer.write_i16(progress);
        writer.write_u8(blessing_days_left);
        writer.write_u8(curse_days_left);
        writer.write_u8(has_raw_materials);
        writer.write_u8(has_fish);
        writer.write_u8(is_stockpiling);
        writer.write_u8(orientation);
        writer.write_u32(fishing_boat_id);
        writer.write_u8(age_months);
        writer.write_u8(average_production_per_month);
        writer.write_i16(production_current_month);
    }

    int16_t progress = 0;
    uint8_t blessing_days_left = 0;
    uint8_t curse_days_left = 0;
    uint8_t has_raw_materials = 0;
    uint8_t has_fish = 0;
    uint8_t is_stockpiling = 0;
    uint8_t orientation = 0;
    uint32_t fishing_boat_id = 0;
    uint8_t age_months = 0;
    uint8_t average_production_per_month = 0;
    int16_t production_current_month = 0;
};

class RubbleSaveObject final : public BuildingSaveObject {
public:
    explicit RubbleSaveObject(const building *b)
        : BuildingSaveObject(BuildingSaveObjectKind::Rubble, b->id),
          og_type_save_id(save_id_from_runtime_type(static_cast<building_type>(b->data.rubble.og_type))),
          og_grid_offset(b->data.rubble.og_grid_offset),
          og_size(b->data.rubble.og_size),
          og_orientation(b->data.rubble.og_orientation)
    {
    }
    explicit RubbleSaveObject(buffer *payload, uint32_t object_id)
        : BuildingSaveObject(BuildingSaveObjectKind::Rubble, object_id),
          og_type_save_id(buffer_read_u16(payload)),
          og_grid_offset(buffer_read_u16(payload)),
          og_size(buffer_read_u8(payload)),
          og_orientation(buffer_read_u8(payload))
    {
    }
    void apply(building *b) const override
    {
        if (og_type_save_id && !building_type_id_bridge_save_id_is_in_table(og_type_save_id)) {
            fatal_save_heap("0xb6 rubble payload attempted to use a legacy enum fallback BuildingType id", "RubbleSaveObject");
        }
        b->data.rubble.og_type = static_cast<unsigned short>(
            building_type_id_bridge_runtime_from_save_id(og_type_save_id));
        b->data.rubble.og_grid_offset = og_grid_offset;
        b->data.rubble.og_size = og_size;
        b->data.rubble.og_orientation = og_orientation;
    }

private:
    void write_payload(HeapWriter &writer) const override
    {
        writer.write_u16(og_type_save_id);
        writer.write_u16(og_grid_offset);
        writer.write_u8(og_size);
        writer.write_u8(og_orientation);
    }

    uint16_t og_type_save_id = 0;
    uint16_t og_grid_offset = 0;
    uint8_t og_size = 0;
    uint8_t og_orientation = 0;
};

class RoadblockSaveObject final : public BuildingSaveObject {
public:
    explicit RoadblockSaveObject(const building *b)
        : BuildingSaveObject(BuildingSaveObjectKind::Roadblock, b->id),
          exceptions(b->data.roadblock.exceptions)
    {
    }
    explicit RoadblockSaveObject(buffer *payload, uint32_t object_id)
        : BuildingSaveObject(BuildingSaveObjectKind::Roadblock, object_id),
          exceptions(buffer_read_u16(payload))
    {
    }
    void apply(building *b) const override
    {
        b->data.roadblock.exceptions = exceptions;
    }

private:
    void write_payload(HeapWriter &writer) const override
    {
        writer.write_u16(exceptions);
    }

    uint16_t exceptions = 0;
};

class EntertainmentSaveObject final : public BuildingSaveObject {
public:
    explicit EntertainmentSaveObject(const building *b)
        : BuildingSaveObject(BuildingSaveObjectKind::Entertainment, b->id)
    {
        num_shows = b->data.entertainment.num_shows;
        days1 = b->data.entertainment.days1;
        days2 = b->data.entertainment.days2;
        play = b->data.entertainment.play;
    }
    explicit EntertainmentSaveObject(buffer *payload, uint32_t object_id)
        : BuildingSaveObject(BuildingSaveObjectKind::Entertainment, object_id)
    {
        num_shows = buffer_read_u8(payload);
        days1 = buffer_read_u8(payload);
        days2 = buffer_read_u8(payload);
        play = buffer_read_u8(payload);
    }
    void apply(building *b) const override
    {
        b->data.entertainment.num_shows = num_shows;
        b->data.entertainment.days1 = days1;
        b->data.entertainment.days2 = days2;
        b->data.entertainment.play = play;
    }

private:
    void write_payload(HeapWriter &writer) const override
    {
        writer.write_u8(num_shows);
        writer.write_u8(days1);
        writer.write_u8(days2);
        writer.write_u8(play);
    }

    uint8_t num_shows = 0;
    uint8_t days1 = 0;
    uint8_t days2 = 0;
    uint8_t play = 0;
};

class MonumentAttributesSaveObject final : public BuildingSaveObject {
public:
    explicit MonumentAttributesSaveObject(const building *b)
        : BuildingSaveObject(BuildingSaveObjectKind::Monument, b->id),
          upgrades(b->monument.upgrades),
          progress(b->monument.progress),
          phase(b->monument.phase),
          secondary_frame(b->monument.secondary_frame)
    {
    }
    explicit MonumentAttributesSaveObject(buffer *payload, uint32_t object_id)
        : BuildingSaveObject(BuildingSaveObjectKind::Monument, object_id),
          upgrades(buffer_read_i32(payload)),
          progress(buffer_read_i16(payload)),
          phase(buffer_read_i16(payload)),
          secondary_frame(buffer_read_i16(payload))
    {
    }
    bool has_saved_attributes() const
    {
        return phase || progress || upgrades || secondary_frame;
    }
    void apply(building *b) const override
    {
        b->monument.upgrades = upgrades;
        b->monument.progress = progress;
        b->monument.phase = phase;
        b->monument.secondary_frame = secondary_frame;
    }

private:
    void write_payload(HeapWriter &writer) const override
    {
        writer.write_i32(upgrades);
        writer.write_i16(progress);
        writer.write_i16(phase);
        writer.write_i16(secondary_frame);
    }

    int32_t upgrades = 0;
    int16_t progress = 0;
    int16_t phase = 0;
    int16_t secondary_frame = 0;
};

using DistributionSaveObject = EmptyPayloadObject<BuildingSaveObjectKind::Distribution>;
using NativeStorageSaveObject = EmptyPayloadObject<BuildingSaveObjectKind::NativeStorage>;
using NativeProductionSaveObject = EmptyPayloadObject<BuildingSaveObjectKind::NativeProduction>;
using ExtensionSaveObject = EmptyPayloadObject<BuildingSaveObjectKind::Extension>;

class BuildingSaveHeap {
public:
    void clear()
    {
        for (ObjectList &list : objects_) {
            list.clear();
        }
        highest_building_id_ = 0;
    }

    void capture_building(const building *b)
    {
        if (!b) {
            fatal_save_heap("0xb6 building heap capture received a null building", "capture_building");
        }
        add(std::make_unique<BuildingInstanceSaveObject>(b));
        if (b->state == BUILDING_STATE_UNUSED) {
            return;
        }
        uint16_t type_save_id = save_id_from_runtime_type(b->type);
        if (building_has_saved_monument_attributes(b) &&
            !building_type_id_bridge_save_id_has_phased_construction(type_save_id)) {
            char detail[160];
            snprintf(detail, sizeof(detail), "building %u runtime type %d save type id %u",
                b->id, static_cast<int>(b->type), type_save_id);
            fatal_save_heap(
                "0xb6 building heap refused to save monument attributes for a type without saved phased construction",
                detail);
        }
        if (building_is_house(b->type)) {
            add(std::make_unique<HouseSaveObject>(b));
        } else if (b->type == BUILDING_DEPOT) {
            add(std::make_unique<DepotSaveObject>(b));
        } else if (b->type == BUILDING_DOCK) {
            add(std::make_unique<DockSaveObject>(b));
        } else if (is_industry_payload_type(b)) {
            add(std::make_unique<IndustrySaveObject>(b));
        } else if (!is_supplier_payload_type(b->type) && !building_type_is_roadblock(b->type) &&
            !is_rubble_payload_type(b->type)) {
            add(std::make_unique<EntertainmentSaveObject>(b));
        }
        if (is_supplier_payload_type(b->type)) {
            add(std::make_unique<SupplierSaveObject>(b));
        }
        if (building_type_is_roadblock(b->type)) {
            add(std::make_unique<RoadblockSaveObject>(b));
        }
        if (is_rubble_payload_type(b->type)) {
            add(std::make_unique<RubbleSaveObject>(b));
        }
        add(std::make_unique<MonumentAttributesSaveObject>(b));
    }

    void write(buffer *buf) const
    {
        HeapWriter heap;
        const uint32_t object_count = checked_object_count();
        heap.write_u32(kHeapMagic);
        heap.write_u32(kHeapVersion);
        heap.write_u32(highest_building_id_);
        heap.write_u32(object_count);

        std::vector<const BuildingSaveObject *> ordered_objects;
        ordered_objects.reserve(object_count);
        for (const ObjectList &list : objects_) {
            for (const std::unique_ptr<BuildingSaveObject> &object : list) {
                ordered_objects.push_back(object.get());
            }
        }
        for (size_t i = 0; i < ordered_objects.size(); ++i) {
            const BuildingSaveObject *object = ordered_objects[i];
            std::vector<uint8_t> payload = object->payload();
            const uint32_t payload_size = static_cast<uint32_t>(payload.size());
            const uint32_t next_offset = i + 1 < ordered_objects.size() ?
                static_cast<uint32_t>(heap.size() + kObjectHeaderSize + payload_size) : 0;
            heap.write_u32(static_cast<uint32_t>(object->kind()));
            heap.write_u32(object->version());
            heap.write_u32(object->object_id());
            heap.write_u32(payload_size);
            heap.write_u32(next_offset);
            heap.append(payload);
        }

        buffer_init_dynamic(buf, heap.size());
        buffer_write_raw(buf, heap.bytes().data(), heap.bytes().size());
    }

    void load(buffer *buf)
    {
        clear();
        if (!buf || !buf->data || !buf->size) {
            fatal_save_heap("0xb6 building heap piece is empty", "building_save_heap_load_current");
        }
        buffer heap = *buf;
        const size_t payload_size = buffer_load_dynamic(&heap);
        if (payload_size < sizeof(uint32_t) * 4) {
            fatal_save_heap("0xb6 building heap payload is too small", "building_save_heap_load_current");
        }
        const uint32_t magic = buffer_read_u32(&heap);
        if (magic != kHeapMagic) {
            fatal_save_heap(
                "0xb6 building save attempted to load the legacy building buffer; this line is wrong and needs to be cleaned up",
                "missing BSHP magic");
        }
        const uint32_t version = buffer_read_u32(&heap);
        if (version == 1) {
            fatal_save_heap(
                "0xb6 building heap uses the retired v1 object layout without explicit per-building monument attributes",
                "building_save_heap_load_current");
        }
        if (version != kHeapVersion) {
            fatal_save_heap("Unsupported 0xb6 building heap version", "building_save_heap_load_current");
        }
        highest_building_id_ = buffer_read_u32(&heap);
        const uint32_t object_count = buffer_read_u32(&heap);
        for (uint32_t i = 0; i < object_count; ++i) {
            const size_t object_offset = heap.index - sizeof(uint32_t);
            const uint32_t kind = buffer_read_u32(&heap);
            const uint32_t object_version = buffer_read_u32(&heap);
            const uint32_t object_id = buffer_read_u32(&heap);
            const uint32_t object_payload_size = buffer_read_u32(&heap);
            const uint32_t next_offset = buffer_read_u32(&heap);
            if (object_id >= highest_building_id_) {
                fatal_save_heap("0xb6 building heap object id is outside the declared building count", kind_name(kind));
            }
            if (object_version != 1) {
                fatal_save_heap("Unsupported 0xb6 building heap object version", kind_name(kind));
            }
            if (heap.index + object_payload_size > heap.size) {
                fatal_save_heap("0xb6 building heap object overruns the save piece", kind_name(kind));
            }
            buffer payload;
            buffer_init(&payload, &heap.data[heap.index], object_payload_size);
            std::unique_ptr<BuildingSaveObject> object = read_object(kind, object_id, &payload);
            if (payload.overflow || payload.index != payload.size) {
                fatal_save_heap("0xb6 building heap object length mismatch", kind_name(kind));
            }
            add(std::move(object));
            heap.index += object_payload_size;
            if (next_offset && next_offset != heap.index - sizeof(uint32_t)) {
                fatal_save_heap("0xb6 building heap object next_offset is invalid", kind_name(kind));
            }
            if (!next_offset && i + 1 != object_count) {
                fatal_save_heap("0xb6 building heap object chain ended early", kind_name(kind));
            }
            if (heap.index <= object_offset) {
                fatal_save_heap("0xb6 building heap object did not advance the reader", kind_name(kind));
            }
        }
        if (heap.overflow || !buffer_at_end(&heap)) {
            fatal_save_heap("0xb6 building heap has unread or overflowed bytes", "building_save_heap_load_current");
        }
        validate_loaded_heap();
    }

    int building_count() const
    {
        return static_cast<int>(highest_building_id_);
    }

    void materialize_building(int id, building *b) const
    {
        const BuildingInstanceSaveObject *instance = instance_for(id);
        if (!instance) {
            char detail[128];
            snprintf(detail, sizeof(detail), "building %d has no instance object", id);
            fatal_save_heap("0xb6 building heap is missing a required instance object", detail);
        }
        instance->apply(b);
        for (const ObjectList &list : objects_) {
            for (const std::unique_ptr<BuildingSaveObject> &object : list) {
                if (object->object_id() == static_cast<uint32_t>(id) &&
                    object->kind() != BuildingSaveObjectKind::Instance) {
                    object->apply(b);
                }
            }
        }
        if (b->state != BUILDING_STATE_UNUSED && !has_object(BuildingSaveObjectKind::Monument, id)) {
            char detail[128];
            snprintf(detail, sizeof(detail), "building %d type %d", id, static_cast<int>(b->type));
            fatal_save_heap("0xb6 active building is missing its explicit monument attribute payload", detail);
        }
        if (b->state != BUILDING_STATE_UNUSED && b->type == BUILDING_NONE) {
            char detail[128];
            snprintf(detail, sizeof(detail), "building %d resolved to BUILDING_NONE", id);
            fatal_save_heap("0xb6 building heap materialized an active building without a type", detail);
        }
    }

private:
    using ObjectList = std::list<std::unique_ptr<BuildingSaveObject>>;

    void add(std::unique_ptr<BuildingSaveObject> object)
    {
        if (!object) {
            fatal_save_heap("0xb6 building heap received a null object", "BuildingSaveHeap::add");
        }
        const size_t kind_index = static_cast<size_t>(object->kind());
        if (kind_index == 0 || kind_index >= objects_.size()) {
            fatal_save_heap("0xb6 building heap object kind is out of range", "BuildingSaveHeap::add");
        }
        if (has_object(object->kind(), static_cast<int>(object->object_id()))) {
            fatal_save_heap("0xb6 building heap has a duplicate object for one building",
                kind_name(static_cast<uint32_t>(object->kind())));
        }
        highest_building_id_ = std::max(highest_building_id_, object->object_id() + 1);
        objects_[kind_index].push_back(std::move(object));
    }

    uint32_t checked_object_count() const
    {
        size_t count = 0;
        for (const ObjectList &list : objects_) {
            count += list.size();
        }
        if (count > static_cast<size_t>(0xffffffffu)) {
            fatal_save_heap("0xb6 building heap has too many objects", "checked_object_count");
        }
        return static_cast<uint32_t>(count);
    }

    bool has_object(BuildingSaveObjectKind kind, int object_id) const
    {
        const size_t kind_index = static_cast<size_t>(kind);
        if (kind_index >= objects_.size()) {
            return false;
        }
        for (const std::unique_ptr<BuildingSaveObject> &object : objects_[kind_index]) {
            if (object->object_id() == static_cast<uint32_t>(object_id)) {
                return true;
            }
        }
        return false;
    }

    const BuildingInstanceSaveObject *instance_for(int object_id) const
    {
        for (const std::unique_ptr<BuildingSaveObject> &object :
             objects_[static_cast<size_t>(BuildingSaveObjectKind::Instance)]) {
            if (object->object_id() == static_cast<uint32_t>(object_id)) {
                return static_cast<const BuildingInstanceSaveObject *>(object.get());
            }
        }
        return nullptr;
    }

    const MonumentAttributesSaveObject *monument_attributes_for(int object_id) const
    {
        for (const std::unique_ptr<BuildingSaveObject> &object :
             objects_[static_cast<size_t>(BuildingSaveObjectKind::Monument)]) {
            if (object->object_id() == static_cast<uint32_t>(object_id)) {
                return static_cast<const MonumentAttributesSaveObject *>(object.get());
            }
        }
        return nullptr;
    }

    void validate_loaded_heap() const
    {
        for (uint32_t id = 0; id < highest_building_id_; ++id) {
            const BuildingInstanceSaveObject *instance = instance_for(static_cast<int>(id));
            if (!instance) {
                char detail[128];
                snprintf(detail, sizeof(detail), "building %u", id);
                fatal_save_heap("0xb6 building heap has a missing instance in its object list", detail);
            }
            if (instance->saved_state() != BUILDING_STATE_UNUSED && instance->saved_type_id() == 0) {
                char detail[128];
                snprintf(detail, sizeof(detail), "building %u", id);
                fatal_save_heap("0xb6 building heap has an active instance with no BuildingType save id", detail);
            }
            if (instance->saved_state() != BUILDING_STATE_UNUSED &&
                !has_object(BuildingSaveObjectKind::Monument, static_cast<int>(id))) {
                char detail[128];
                snprintf(detail, sizeof(detail), "building %u", id);
                fatal_save_heap("0xb6 building heap has an active instance without a monument attribute payload", detail);
            }
            const MonumentAttributesSaveObject *monument_attributes =
                monument_attributes_for(static_cast<int>(id));
            if (instance->saved_state() != BUILDING_STATE_UNUSED && monument_attributes &&
                monument_attributes->has_saved_attributes() &&
                !building_type_id_bridge_save_id_has_phased_construction(instance->saved_type_id())) {
                char detail[160];
                snprintf(detail, sizeof(detail), "building %u save type id %u", id, instance->saved_type_id());
                fatal_save_heap(
                    "0xb6 building heap loaded monument attributes for a type without saved phased construction",
                    detail);
            }
        }
    }

    static const char *kind_name(uint32_t kind)
    {
        switch (static_cast<BuildingSaveObjectKind>(kind)) {
            case BuildingSaveObjectKind::Instance: return "Instance";
            case BuildingSaveObjectKind::House: return "House";
            case BuildingSaveObjectKind::Supplier: return "Supplier";
            case BuildingSaveObjectKind::Dock: return "Dock";
            case BuildingSaveObjectKind::Depot: return "Depot";
            case BuildingSaveObjectKind::Industry: return "Industry";
            case BuildingSaveObjectKind::Distribution: return "Distribution";
            case BuildingSaveObjectKind::Rubble: return "Rubble";
            case BuildingSaveObjectKind::Roadblock: return "Roadblock";
            case BuildingSaveObjectKind::Entertainment: return "Entertainment";
            case BuildingSaveObjectKind::Monument: return "Monument";
            case BuildingSaveObjectKind::NativeStorage: return "NativeStorage";
            case BuildingSaveObjectKind::NativeProduction: return "NativeProduction";
            case BuildingSaveObjectKind::Extension: return "Extension";
            default: return "Unknown";
        }
    }

    static std::unique_ptr<BuildingSaveObject> read_object(uint32_t kind, uint32_t object_id, buffer *payload)
    {
        switch (static_cast<BuildingSaveObjectKind>(kind)) {
            case BuildingSaveObjectKind::Instance:
                return std::make_unique<BuildingInstanceSaveObject>(payload, object_id);
            case BuildingSaveObjectKind::House:
                return std::make_unique<HouseSaveObject>(payload, object_id);
            case BuildingSaveObjectKind::Supplier:
                return std::make_unique<SupplierSaveObject>(payload, object_id);
            case BuildingSaveObjectKind::Dock:
                return std::make_unique<DockSaveObject>(payload, object_id);
            case BuildingSaveObjectKind::Depot:
                return std::make_unique<DepotSaveObject>(payload, object_id);
            case BuildingSaveObjectKind::Industry:
                return std::make_unique<IndustrySaveObject>(payload, object_id);
            case BuildingSaveObjectKind::Distribution:
                return std::make_unique<DistributionSaveObject>(payload, object_id);
            case BuildingSaveObjectKind::Rubble:
                return std::make_unique<RubbleSaveObject>(payload, object_id);
            case BuildingSaveObjectKind::Roadblock:
                return std::make_unique<RoadblockSaveObject>(payload, object_id);
            case BuildingSaveObjectKind::Entertainment:
                return std::make_unique<EntertainmentSaveObject>(payload, object_id);
            case BuildingSaveObjectKind::Monument:
                return std::make_unique<MonumentAttributesSaveObject>(payload, object_id);
            case BuildingSaveObjectKind::NativeStorage:
                return std::make_unique<NativeStorageSaveObject>(payload, object_id);
            case BuildingSaveObjectKind::NativeProduction:
                return std::make_unique<NativeProductionSaveObject>(payload, object_id);
            case BuildingSaveObjectKind::Extension:
                return std::make_unique<ExtensionSaveObject>(payload, object_id);
            default:
                fatal_save_heap("0xb6 building heap contains an unknown object kind", kind_name(kind));
                return nullptr;
        }
    }

    std::array<ObjectList, static_cast<size_t>(BuildingSaveObjectKind::Count)> objects_;
    uint32_t highest_building_id_ = 0;
};

BuildingSaveHeap &heap()
{
    static BuildingSaveHeap instance;
    return instance;
}

} // namespace

extern "C" void building_save_heap_clear(void)
{
    heap().clear();
}

extern "C" void building_save_heap_capture_building(const building *b)
{
    heap().capture_building(b);
}

extern "C" void building_save_heap_write_current(buffer *buf)
{
    heap().write(buf);
}

extern "C" void building_save_heap_load_current(buffer *buf)
{
    heap().load(buf);
}

extern "C" int building_save_heap_building_count(void)
{
    return heap().building_count();
}

extern "C" void building_save_heap_materialize_building(int id, building *b)
{
    heap().materialize_building(id, b);
}
