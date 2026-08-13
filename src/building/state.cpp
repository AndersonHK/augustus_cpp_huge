#include "building/roadblock.h"
#include "building/state.h"
#include "map/building_tiles.h"

#include "game/resource_id_bridge.h"
#include "building/BuildingGraphicsState.h"
#include "building/FoundationStateSaveBridge.h"
#include "building/HousingStateBridge.h"
#include "building/HousingProfileDef.h"
#include "building/LegacyBuildingSaveDto.h"
#include "building/building.h"
#include "building/building_record.h"
#include "building/building_runtime.h"
#include "building/building_runtime_internal.h"
#include "building/building_type_registry_internal.h"
#include "figure/figure.h"
#include "building/building_type_id_bridge.h"
#include "building/building_type_legacy_migration.h"
#include "building/housing_profile_registry.h"
#include "building/monument.h"
#include "core/log.h"
#include "game/save_version.h"
#include "map/building.h"
#include "map/grid.h"
#include "map/terrain.h"

#include <algorithm>
#include <cstdio>
#include <cstddef>
#include <cstring>
#include <vector>

#define TYPE_DATA_ORIGINAL_BUFFER_SIZE 42
#define TYPE_DATA_CURRENT_BUFFER_SIZE 26

namespace {

constexpr uint16_t LEGACY_SAVE_TYPE_MENU_FORT = 57;
constexpr uint16_t LEGACY_SAVE_TYPE_LIGHTHOUSE = 155;
constexpr uint16_t LEGACY_SAVE_TYPE_PAVILION_FIRST = 144;
constexpr uint16_t LEGACY_SAVE_TYPE_PAVILION_LAST = 148;

using building_type_registry_impl::definition_for_type;
using building_type_registry_impl::type_attr_is;
using building_type_registry_impl::type_attr_is_any;
using building_type_registry_impl::type_from_attr;

int type_has_distribution(building_type type)
{
    const building_type_registry_impl::BuildingType *definition = definition_for_type(type);
    return definition ? definition->has_distribution() : 0;
}

int type_is_caravanserai(building_type type)
{
    const building_type_registry_impl::BuildingType *definition = definition_for_type(type);
    return definition ? definition->is_caravanserai() : 0;
}

int type_is_large_temple_supplier(building_type type)
{
    return type_attr_is_any(type, {"large_temple_ceres", "large_temple_venus"});
}

int type_is_warehouse(building_type type)
{
    return type_attr_is(type, "warehouse");
}

int type_is_warehouse_space(building_type type)
{
    return type_attr_is(type, "warehouse_space");
}

int type_is_granary(building_type type)
{
    return type_attr_is(type, "granary");
}

int type_is_depot(building_type type)
{
    return type_attr_is(type, "cart_depot");
}

int type_is_roadblock_record(building_type type)
{
    const building_type_registry_impl::BuildingType *definition = definition_for_type(type);
    return definition && (definition->bridge().is_bridge() ||
        (definition->foundation_def() && definition->foundation_def()->has_owner_controlled_passage()));
}

int type_is_burning_ruin(building_type type)
{
    return type_attr_is(type, "burning_ruin");
}

int type_is_rubble_shell(building_type type)
{
    const building_type_registry_impl::BuildingType *definition = definition_for_type(type);
    return definition ? definition->has_rubble() : 0;
}

const building_type_registry_impl::BuildingType *original_type_for_save(const building *b)
{
    if (!b || !b->id) {
        return nullptr;
    }
    if (Building *building_object = Building::get(b->id)) {
        return building_object->Rubble ? building_object->Rubble->original_type() : nullptr;
    }
    return nullptr;
}

const RubbleState *rubble_state_for_save(const building *b)
{
    if (!b || !b->id) {
        return nullptr;
    }
    Building *building_object = Building::get(b->id);
    return building_object && building_object->Rubble ? building_object->Rubble->state() : nullptr;
}

void composition_state_to_legacy_record(const Building &building_object, building &record)
{
    // Save DTO boundary: synthesize the historical chain bytes from the
    // owner-bound runtime graph without restoring those links to live records.
    BuildingComposition *composition = building_object.Composition;
    if (!composition || !composition->is_composed()) {
        return;
    }
    BuildingComposition *owner = composition->owner_module();
    if (!owner || !owner->is_owner()) {
        return;
    }

    const std::vector<BuildingComposition *> &children = owner->children();
    if (composition == owner) {
        record.prev_part_building_id = 0;
        record.next_part_building_id = children.empty() || !children.front() || !children.front()->building() ?
            0 : static_cast<short>(children.front()->building()->id);
        return;
    }

    const auto found = std::find(children.begin(), children.end(), composition);
    if (found == children.end()) {
        record.prev_part_building_id = 0;
        record.next_part_building_id = 0;
        return;
    }
    const std::size_t index = static_cast<std::size_t>(found - children.begin());
    Building *previous = index == 0 ? owner->building() :
        (children[index - 1] ? children[index - 1]->building() : nullptr);
    Building *next = index + 1 < children.size() && children[index + 1] ?
        children[index + 1]->building() : nullptr;
    record.prev_part_building_id = previous ? static_cast<short>(previous->id) : 0;
    record.next_part_building_id = next ? static_cast<short>(next->id) : 0;
}

int type_uses_industry_state(const building *b)
{
    if (!b) {
        return 0;
    }
    return (b->output_resource_id || type_attr_is(b->type, "native_crops") ||
        type_attr_is(b->type, "shipyard") || type_attr_is(b->type, "wharf")) ? 1 : 0;
}

int type_uses_monthly_production_stats(building_type type)
{
    const building_type_registry_impl::BuildingType *definition = definition_for_type(type);
    if (definition && definition->is_farm()) {
        return 1;
    }
    return type_attr_is_any(type, {
        "marble_quarry",
        "iron_mine",
        "timber_yard",
        "clay_pit",
        "wine_workshop",
        "oil_workshop",
        "weapons_workshop",
        "furniture_workshop",
        "pottery_workshop",
        "wharf"
    });
}

int type_is_legacy_large_temple_or_oracle(building_type type)
{
    return type_attr_is_any(type, {
        "large_temple_ceres",
        "large_temple_neptune",
        "large_temple_mercury",
        "large_temple_mars",
        "large_temple_venus",
        "oracle"
    });
}

} // namespace

static uint16_t save_id_from_runtime_type(const building_type_registry_impl::BuildingType *runtime_type)
{
    return building_type_id_bridge_save_id_from_runtime(runtime_type ? runtime_type->type() : BUILDING_NONE);
}

static const building_type_registry_impl::BuildingType *runtime_type_from_save_id(uint16_t save_id)
{
    return definition_for_type(building_type_id_bridge_runtime_from_save_id(save_id));
}

static void write_rubble_type_data(buffer *buf, const building *b)
{
    const RubbleState *rubble_state = rubble_state_for_save(b);
    buffer_write_u16(buf, save_id_from_runtime_type(original_type_for_save(b)));
    buffer_write_u16(buf, rubble_state ? rubble_state->original_grid_offset : 0);
    buffer_write_u8(buf, rubble_state ? rubble_state->original_orientation : 0);
}

static void read_rubble_type_data(buffer *buf, building *b, int version, int for_preview)
{
    const building_type_registry_impl::BuildingType *original_type =
        runtime_type_from_save_id(buffer_read_u16(buf));
    int original_grid_offset = 0;
    int original_orientation = 0;
    if (version <= SAVE_GAME_LAST_RUBBLE_ORIGIN_GRID_OFFSET) {
        original_grid_offset = buffer_read_u16(buf);
        buffer_read_u8(buf); // old square size; current BuildingType owns footprint dimensions
        original_orientation = buffer_read_u8(buf);
        if (original_type && original_type->has_composition()) {
            const int rotation = (original_orientation % 4 + 4) % 4;
            const building_type_registry_impl::CompositionLayoutResult layout =
                building_type_registry_impl::build_composition_layout(
                    original_type,
                    original_type->composition(),
                    map_grid_offset_to_x(original_grid_offset),
                    map_grid_offset_to_y(original_grid_offset),
                    rotation);
            if (layout.valid()) {
                original_grid_offset = map_grid_offset(layout.bounds.min_x, layout.bounds.min_y);
            }
        }
    } else if (version <= SAVE_GAME_LAST_RUBBLE_ORIGIN_RECTANGLE) {
        const int original_x = buffer_read_u8(buf);
        const int original_y = buffer_read_u8(buf);
        buffer_read_u8(buf); // width and height were redundant copies of BuildingType data
        buffer_read_u8(buf);
        original_grid_offset = map_grid_offset(original_x, original_y);
        original_orientation = buffer_read_u8(buf);
    } else {
        original_grid_offset = buffer_read_u16(buf);
        original_orientation = buffer_read_u8(buf);
    }
    if (!b || for_preview) {
        return;
    }

    RubbleState rubble_state;
    rubble_state.original_grid_offset = static_cast<unsigned short>(original_grid_offset);
    rubble_state.original_orientation = static_cast<unsigned char>(original_orientation);
    rubble_state.original_type = original_type;
    building_runtime_stage_loaded_rubble_state(b->id, rubble_state);
}

static const char *safe_text(const char *text)
{
    return text && *text ? text : "<none>";
}

static int building_record_requires_type_definition(const building *b)
{
    return b ? b->state != BUILDING_STATE_UNUSED : 0;
}

static const char *loaded_building_type_problem(const building *b, uint16_t saved_type, int missing_save_type)
{
    if (!building_record_requires_type_definition(b)) {
        return 0;
    }
    if (!saved_type) {
        return "empty_save_type_id";
    }
    if (missing_save_type) {
        return "save_type_missing_from_active_registry";
    }
    if (b->type == BUILDING_NONE) {
        return "save_type_resolved_to_none";
    }
    if (!building_type_registry_impl::definition_for_type(b->type)) {
        return "runtime_type_has_no_definition";
    }
    return 0;
}

static void format_loaded_building_type_problem(
    char *detail,
    size_t detail_size,
    const building *b,
    uint16_t saved_type,
    const char *reason)
{
    const building_type runtime_type = b ? b->type : BUILDING_NONE;
    const uint16_t runtime_save_type = building_type_id_bridge_save_id_from_runtime(runtime_type);
    const char *save_text_id = building_type_id_bridge_text_from_save_id(saved_type);
    const char *runtime_text_id = building_type_id_bridge_text_from_runtime(runtime_type);
    const char *legacy_save_text_id = building_type_legacy_migration_text_id_for_enum(saved_type);
    const char *legacy_runtime_text_id =
        building_type_legacy_migration_text_id_for_enum(static_cast<uint16_t>(runtime_type));

    std::snprintf(
        detail,
        detail_size,
        "reason=%s building_id=%u state=%d saved_type=%u save_text_id=%s legacy_save_text_id=%s "
        "runtime_type=%d runtime_text_id=%s runtime_save_type=%u legacy_runtime_text_id=%s "
        "registry_has_definition=%d x=%d y=%d grid_offset=%d prev_part=%d next_part=%d deleted=%d",
        safe_text(reason),
        b ? b->id : 0,
        b ? b->state : BUILDING_STATE_UNUSED,
        saved_type,
        safe_text(save_text_id),
        safe_text(legacy_save_text_id),
        static_cast<int>(runtime_type),
        safe_text(runtime_text_id),
        runtime_save_type,
        safe_text(legacy_runtime_text_id),
        building_type_registry_impl::definition_for_type(runtime_type) != nullptr,
        b ? b->x : 0,
        b ? b->y : 0,
        b ? b->grid_offset : 0,
        b ? b->prev_part_building_id : 0,
        b ? b->next_part_building_id : 0,
        b ? b->is_deleted : 0);
}

static void log_loaded_building_type_problem(const building *b, uint16_t saved_type, const char *reason)
{
    char detail[1200];
    format_loaded_building_type_problem(detail, sizeof(detail), b, saved_type, reason);
    log_warning("Building save contained an unsupported building type; removing saved building", detail, b ? b->id : 0);
}

static void remove_tiles_for_unsupported_building(const building *b)
{
    if (!b) {
        return;
    }
    int x = b->x;
    int y = b->y;
    if (!map_grid_is_inside(x, y, 1)) {
        if (!map_grid_is_valid_offset(b->grid_offset)) {
            return;
        }
        x = map_grid_offset_to_x(b->grid_offset);
        y = map_grid_offset_to_y(b->grid_offset);
    }
    map_legacy_building_tiles_remove(x, y);
}

static int detach_unsupported_surface_record(const building *b)
{
    if (!b) {
        return 0;
    }

    int grid_offset = b->grid_offset;
    if (!map_grid_is_valid_offset(grid_offset) && map_grid_is_inside(b->x, b->y, 1)) {
        grid_offset = map_grid_offset(b->x, b->y);
    }
    if (!map_grid_is_valid_offset(grid_offset)) {
        return 0;
    }

    if (map_building_loaded_id_at(grid_offset) != b->id) {
        return 1;
    }
    if (map_terrain_is(grid_offset, TERRAIN_BUILDING)) {
        return 0;
    }

    map_building_set_loaded_id(grid_offset, 0);
    return 1;
}

static void quarantine_loaded_building_type_problem(
    building *b,
    uint16_t saved_type,
    const char *reason,
    int for_preview)
{
    if (!b) {
        return;
    }
    log_loaded_building_type_problem(b, saved_type, reason);
    if (!for_preview) {
        b->type = BUILDING_NONE;
        if (!detach_unsupported_surface_record(b)) {
            remove_tiles_for_unsupported_building(b);
        }
    }
    const unsigned int id = b->id;
    memset(b, 0, sizeof(building));
    b->id = id;
}

static void skip_remaining_building_record(buffer *buf, size_t record_start, int building_buf_size)
{
    if (!buf || building_buf_size <= 0) {
        return;
    }
    size_t bytes_read = buf->index - record_start;
    if (bytes_read < static_cast<size_t>(building_buf_size)) {
        buffer_skip(buf, static_cast<int>(static_cast<size_t>(building_buf_size) - bytes_read));
    }
}

static int remaining_building_record_bytes(const buffer *buf, size_t record_start, int building_buf_size)
{
    if (!buf || building_buf_size <= 0) {
        return 0;
    }
    size_t record_end = record_start + static_cast<size_t>(building_buf_size);
    if (buf->index >= record_end) {
        return 0;
    }
    return static_cast<int>(record_end - buf->index);
}

static int flat_resource_slots_left_in_record(const buffer *buf, size_t record_start, int building_buf_size)
{
    int bytes = remaining_building_record_bytes(buf, record_start, building_buf_size);
    if (building_buf_size >= BUILDING_STATE_FOUNDATION_TERRAIN_DELTAS) {
        bytes -= building_type_registry_impl::FOUNDATION_SAVE_TERRAIN_BYTES;
    }
    if (building_buf_size >= BUILDING_STATE_LATRINES && bytes > 0) {
        bytes--;
    }
    if (bytes <= 0) {
        return 0;
    }
    int slots = bytes / 3;
    if (slots < 0) {
        return 0;
    }
    return slots > RESOURCE_SLOT_COUNT ? RESOURCE_SLOT_COUNT : slots;
}

static void normalize_monument_phase_after_load(building *b)
{
    if (!building_monument_is_monument(b)) {
        return;
    }

    if (!b->monument.phase || b->monument.phase == building_monument_phases(b->type)) {
        b->monument.phase = MONUMENT_FINISHED;
    }
}

static int saved_type_is_monument(uint16_t save_id, building_type runtime_type)
{
    if (building_monument_text_id_is_monument(building_type_id_bridge_text_from_save_id(save_id))) {
        return 1;
    }
    return building_monument_type_is_monument(runtime_type);
}

static int is_industry_type(const building *b)
{
    return type_uses_industry_state(b);
}

static building_type get_fort_type(building *b)
{
    switch (b->subtype.fort_figure_type) {
        case FIGURE_FORT_JAVELIN:
            return type_from_attr("fort_javelin");
        case FIGURE_FORT_MOUNTED:
            return type_from_attr("fort_mounted");
        case FIGURE_FORT_LEGIONARY:
            return type_from_attr("fort_legionaries");
        case FIGURE_FORT_INFANTRY:
            return type_from_attr("fort_swords");
        case FIGURE_FORT_ARCHER:
            return type_from_attr("fort_archers");
        default:
            return BUILDING_NONE;
    }

}

static void copy_subtype_bits(short value, building *b)
{
    static_assert(sizeof(b->subtype) == sizeof(value));
    memcpy(&b->subtype, &value, sizeof(value));
}

static void write_foundation_terrain_state(
    buffer *buf,
    const building_type_registry_impl::FoundationTerrainSaveState &state)
{
    buffer_write_u8(buf, state.published);
    for (uint32_t value : state.added) {
        buffer_write_u32(buf, value);
    }
    for (uint32_t value : state.removed) {
        buffer_write_u32(buf, value);
    }
}

static building_type_registry_impl::FoundationTerrainSaveState read_foundation_terrain_state(buffer *buf)
{
    building_type_registry_impl::FoundationTerrainSaveState state;
    state.published = buffer_read_u8(buf);
    for (uint32_t &value : state.added) {
        value = buffer_read_u32(buf);
    }
    for (uint32_t &value : state.removed) {
        value = buffer_read_u32(buf);
    }
    return state;
}

static short subtype_bits(const building &b)
{
    short value = 0;
    static_assert(sizeof(b.subtype) == sizeof(value));
    memcpy(&value, &b.subtype, sizeof(value));
    return value;
}

static building_save_bridge::LegacyBuildingSaveDto legacy_save_dto_for(const building &b)
{
    building_save_bridge::LegacyBuildingDefinitionView view;
    view.non_housing_subtype_value = subtype_bits(b);
    const building_type_registry_impl::BuildingType *definition = definition_for_type(b.type);
    if (definition) {
        const int rotation = Building::get(b.id) ? Building::get(b.id)->orientation() : 0;
        const building_type_registry_impl::FoundationDef *foundation = definition->foundation_def();
        if (foundation) {
            view.foundation_width = foundation->rotated_width(rotation);
            view.foundation_height = foundation->rotated_height(rotation);
            view.foundation_cell_count = static_cast<int>(foundation->rotated_cells(rotation).size());
        }
        const building_type_registry_impl::HousingProfileDef *profile = definition->housing_def().profile;
        if (profile) {
            view.housing_compatibility_level = profile->compatibility_level;
        }
    }
    return building_save_bridge::synthesize_legacy_building_save_dto(view);
}

static void normalize_native_housing_after_load(
    building *b,
    const building_save_bridge::LegacyBuildingSaveDto &legacy,
    int save_version)
{
    if (!b || b->state == BUILDING_STATE_UNUSED) {
        return;
    }

    const building_type_registry_impl::BuildingType *current_definition =
        building_type_registry_impl::definition_for_type(b->type);
    const building_type_registry_impl::HousingProfileDef *profile =
        current_definition ? current_definition->housing_def().profile : nullptr;
    int housing_level = profile ? profile->compatibility_level : -1;
    const building_type_registry_impl::HousingProfileDef *saved_profile =
        building_type_registry_impl::find_housing_profile_definition_for_compatibility_level(
            legacy.housing_level_or_subtype);
    if (profile && saved_profile == profile) {
        housing_level = legacy.housing_level_or_subtype;
    }
    if (housing_level < 0) {
        return;
    }
    if (housing_level == HOUSE_SMALL_TENT && legacy.housing_population <= 0) {
        building_type vacant_lot_type = building_type_registry_impl::vacant_lot_fill_type();
        if (vacant_lot_type != BUILDING_NONE) {
            b->type = vacant_lot_type;
        }
        return;
    }
    if (!building_save_bridge::legacy_housing_identity_needs_disambiguation(
            save_version, SAVE_GAME_LAST_NO_BUILDING_TYPE_TABLE) ||
        !legacy.house_size) {
        return;
    }
    int footprint_size = legacy.house_size > 0 ? legacy.house_size : legacy.size;
    if (legacy.house_is_merged && footprint_size < 2) {
        footprint_size = 2;
    }

    building_type native_type =
        building_type_registry_impl::building_type_for_housing_compatibility_level(housing_level, footprint_size);
    if (native_type == BUILDING_NONE || native_type == b->type) {
        return;
    }

    b->type = native_type;
}

static int migrate_legacy_entertainment_show_days(int days)
{
    int active_days = days * 2;
    return active_days > 255 ? 255 : active_days;
}

static void write_type_data(
    buffer *buf,
    const building *b,
    const building_save_bridge::LegacyBuildingSaveDto &legacy)
{
    // This function should ALWAYS write 26 bytes.
    // If you don't write 26 bytes, the function will pad them at the end.
    // If you need more than 26 bytes, don't use the type data.
    size_t buffer_index = buf->index;
    const int is_dock = type_attr_is(b->type, "dock");
    const building_type_registry_impl::BuildingType *definition = definition_for_type(b->type);

    if (definition && definition->has_housing()) {
        buffer_write_u8(buf, legacy.housing.theater);
        buffer_write_u8(buf, legacy.housing.amphitheater_actor);
        buffer_write_u8(buf, legacy.housing.amphitheater_gladiator);
        buffer_write_u8(buf, legacy.housing.colosseum_gladiator);
        buffer_write_u8(buf, legacy.housing.colosseum_lion);
        buffer_write_u8(buf, legacy.housing.hippodrome);
        buffer_write_u8(buf, legacy.housing.school);
        buffer_write_u8(buf, legacy.housing.library);
        buffer_write_u8(buf, legacy.housing.academy);
        buffer_write_u8(buf, legacy.housing.barber);
        buffer_write_u8(buf, legacy.housing.clinic);
        buffer_write_u8(buf, legacy.housing.bathhouse);
        buffer_write_u8(buf, legacy.housing.hospital);
        buffer_write_u8(buf, legacy.housing.temple_ceres);
        buffer_write_u8(buf, legacy.housing.temple_neptune);
        buffer_write_u8(buf, legacy.housing.temple_mercury);
        buffer_write_u8(buf, legacy.housing.temple_mars);
        buffer_write_u8(buf, legacy.housing.temple_venus);
        buffer_write_u8(buf, legacy.housing.no_space_to_expand);
        buffer_write_u8(buf, legacy.housing.num_foods);
        buffer_write_u8(buf, legacy.housing.entertainment);
        buffer_write_u8(buf, legacy.housing.education);
        buffer_write_u8(buf, legacy.housing.health);
        buffer_write_u8(buf, legacy.housing.num_gods);
        buffer_write_u8(buf, legacy.housing.devolve_delay);
        buffer_write_u8(buf, legacy.housing.evolve_text_id);
    } else if (type_is_caravanserai(b->type) || type_is_large_temple_supplier(b->type)) {
        buffer_write_u8(buf, b->data.market.fetch_inventory_id);
    } else if (type_has_distribution(b->type)) {
        buffer_write_u8(buf, b->data.market.fetch_inventory_id);
        buffer_write_u8(buf, b->data.market.is_mess_hall);
    } else if (type_is_depot(b->type)) {
        buffer_write_i8(buf, static_cast<int8_t>(b->data.depot.current_order.resource_type));
        buffer_write_i32(buf, b->data.depot.current_order.src_storage_id);
        buffer_write_i32(buf, b->data.depot.current_order.dst_storage_id);
        buffer_write_i8(buf, static_cast<int8_t>(b->data.depot.current_order.condition.condition_type));
        buffer_write_i8(buf, static_cast<int8_t>(b->data.depot.current_order.condition.threshold));
        for (int i = 0; i < 3; i++) {
            buffer_write_i16(buf, static_cast<int16_t>(b->data.distribution.cartpusher_ids[i]));
        }
    } else if (is_dock) {
        buffer_write_i16(buf, b->data.dock.queued_docker_id);
        buffer_write_u8(buf, b->data.dock.has_accepted_route_ids);
        buffer_write_i32(buf, b->data.dock.accepted_route_ids);
        buffer_write_u8(buf, b->data.dock.num_ships);
        buffer_write_i8(buf, b->data.dock.orientation);
        for (int i = 0; i < 3; i++) {
            buffer_write_i16(buf, static_cast<int16_t>(b->data.distribution.cartpusher_ids[i]));
        }
        buffer_write_i16(buf, b->data.dock.trade_ship_id);
    } else if (type_is_roadblock_record(b->type)) {
        buffer_write_u16(buf, b->data.roadblock.exceptions);
        if (type_is_warehouse(b->type)) {
            write_rubble_type_data(buf, b);
        }
    } else if (is_industry_type(b)) {
        buffer_write_i16(buf, b->data.industry.progress);
        buffer_write_u8(buf, b->data.industry.is_stockpiling);
        buffer_write_u8(buf, b->data.industry.has_fish);
        buffer_write_u8(buf, b->data.industry.blessing_days_left);
        buffer_write_u8(buf, static_cast<unsigned char>(b->subtype.orientation));
        buffer_write_u8(buf, b->data.industry.has_raw_materials);
        buffer_write_u8(buf, b->data.industry.curse_days_left);
        if (type_uses_monthly_production_stats(b->type)) {
            buffer_write_u8(buf, b->data.industry.age_months);
            buffer_write_u8(buf, b->data.industry.average_production_per_month);
            buffer_write_i16(buf, b->data.industry.production_current_month);
        }
        buffer_write_i16(buf, static_cast<int16_t>(b->data.industry.fishing_boat_id));
        buffer_write_i16(buf, static_cast<int16_t>(b->data.industry.second_fishing_boat_id));
    } else if (type_is_rubble_shell(b->type)) {
        write_rubble_type_data(buf, b);
    } else {
        buffer_write_u8(buf, b->data.entertainment.num_shows);
        buffer_write_u8(buf, b->data.entertainment.days1);
        buffer_write_u8(buf, b->data.entertainment.days2);
        buffer_write_u8(buf, b->data.entertainment.play);
    }
    int remaining_bytes = TYPE_DATA_CURRENT_BUFFER_SIZE - (int) (buf->index - buffer_index);
    for (int i = 0; i < remaining_bytes; i++) {
        buffer_write_u8(buf, 0);
    }
}

void building_state_save_to_buffer(buffer *buf, const building *b)
{
    const building *source_record = b;
    building staged_record = *b;
    building_type_registry_impl::FoundationTerrainSaveState foundation_terrain_state;
    Building *building_object = Building::get(source_record->id);
    if (building_object) {
        composition_state_to_legacy_record(*building_object, staged_record);
        if (building_object->Foundation) {
            foundation_terrain_state = building_type_registry_impl::foundation_terrain_state_for_save(
                building_object->Foundation->definition(), building_object->Foundation->state());
        }
        if (building_object->Foundation &&
            building_object->Foundation->has_owner_controlled_passage() &&
            !(building_object->type && building_object->type->bridge().is_bridge())) {
            staged_record.data.roadblock.exceptions =
                building_object->Foundation->roadblock_state().permissions();
        }
    }
    b = &staged_record;
    building_save_bridge::LegacyBuildingSaveDto legacy = legacy_save_dto_for(*b);
    if (building_object && building_object->Housing) {
        housing_state_to_legacy_save(building_object->Housing->state(), legacy);
    } else {
        legacy.housing_monthly_levy_or_building_levy = b->levy_amount;
        legacy.housing_tax_income_or_collected_tax = b->collected_tax_income;
        legacy.housing_happiness_or_native_anger = b->native_anger;
    }
    if (building_object && building_object->Rubble && building_object->Rubble->state()) {
        legacy.housing_generation_delay_or_rubble_seed = building_object->Rubble->state()->random_seed;
    }
    buffer_write_u8(buf, b->state);
    buffer_write_u8(buf, b->faction_id);
    buffer_write_u8(buf, b->unknown_value);
    buffer_write_u8(buf, legacy.size);
    buffer_write_u8(buf, legacy.house_is_merged);
    buffer_write_u8(buf, legacy.house_size);
    buffer_write_u8(buf, b->x);
    buffer_write_u8(buf, b->y);
    buffer_write_i16(buf, b->grid_offset);
    buffer_write_u16(buf, building_type_id_bridge_save_id_from_runtime(b->type));
    buffer_write_i16(buf, legacy.housing_level_or_subtype);
    buffer_write_u8(buf, b->road_network_id);
    buffer_write_u8(buf, legacy.housing_monthly_levy_or_building_levy);
    buffer_write_u16(buf, b->created_sequence);
    buffer_write_i16(buf, b->houses_covered);
    buffer_write_i16(buf, b->percentage_houses_covered);
    buffer_write_i16(buf, legacy.housing_population);
    buffer_write_i16(buf, legacy.housing_population_room);
    buffer_write_i16(buf, b->distance_from_entry);
    buffer_write_i16(buf, legacy.housing_highest_population);
    buffer_write_i16(buf, legacy.housing_unreachable_ticks);
    buffer_write_u8(buf, b->road_access_x);
    buffer_write_u8(buf, b->road_access_y);
    buffer_write_i16(buf, static_cast<int16_t>(b->figure_id));
    buffer_write_i16(buf, static_cast<int16_t>(b->figure_id2));
    buffer_write_i16(buf, static_cast<int16_t>(legacy.housing_immigrant_figure_id));
    buffer_write_i16(buf, static_cast<int16_t>(b->figure_id4));
    buffer_write_u8(buf, b->figure_spawn_delay);
    buffer_write_u8(buf, b->days_since_offering);
    buffer_write_u8(buf, b->figure_roam_direction);
    buffer_write_u8(buf, b->has_water_access);
    buffer_write_u8(buf, legacy.housing_tavern_wine_access);
    buffer_write_u8(buf, legacy.housing_tavern_food_access);
    buffer_write_i16(buf, b->prev_part_building_id); // bridge chain or synthesized composition DTO
    buffer_write_i16(buf, b->next_part_building_id); // bridge chain or synthesized composition DTO
    buffer_write_i16(buf, 0); // Q: what was here and why was it removed? can we replace it with something useful?
    buffer_write_u8(buf, legacy.housing_sentiment_message);
    buffer_write_u8(buf, b->has_well_access);
    buffer_write_i16(buf, b->num_workers);
    buffer_write_u8(buf, b->labor_category);
    buffer_write_u8(buf, b->output_resource_id);
    buffer_write_u8(buf, b->has_road_access);
    buffer_write_u8(buf, legacy.housing_criminal_active);
    buffer_write_i16(buf, b->damage_risk);
    buffer_write_i16(buf, b->fire_risk);
    buffer_write_i16(buf, b->fire_duration);
    buffer_write_u8(buf, b->fire_proof);
    buffer_write_u8(buf, legacy.housing_generation_delay_or_rubble_seed);
    buffer_write_u8(buf, legacy.housing_tax_coverage);
    buffer_write_u8(buf, legacy.housing_pantheon_access);
    buffer_write_i16(buf, b->formation_id);
    write_type_data(buf, b, legacy);
    buffer_write_i32(buf, legacy.housing_tax_income_or_collected_tax);
    buffer_write_u8(buf, legacy.housing_days_without_food);
    buffer_write_u8(buf, b->has_plague);
    buffer_write_i8(buf, b->desirability);
    buffer_write_u8(buf, b->is_deleted);
    buffer_write_u8(buf, b->is_close_to_water);
    buffer_write_u8(buf, b->storage_id);
    buffer_write_i8(buf, legacy.housing_happiness_or_native_anger);
    buffer_write_u8(buf, b->show_on_problem_overlay);

    // expanded building data
    // Monuments
    buffer_write_i32(buf, b->monument.upgrades);
    buffer_write_i16(buf, b->monument.progress);
    buffer_write_i16(buf, b->monument.phase);

    // Tourism
    buffer_write_u8(buf, legacy.housing_arena_gladiator);
    buffer_write_u8(buf, legacy.housing_arena_lion);
    buffer_write_u8(buf, b->is_tourism_venue);
    buffer_write_u8(buf, b->tourism_disabled);
    buffer_write_u8(buf, b->tourism_income);
    buffer_write_u8(buf, b->tourism_income_this_year);

    // Variants and upgrades
    // Save writes the full building record table; runtime modules are only consulted here to recompose peeled fields.
    unsigned char graphics_variant = 0;
    if (b->id && (b->state == BUILDING_STATE_CREATED || b->state == BUILDING_STATE_IN_USE ||
            b->state == BUILDING_STATE_MOTHBALLED)) {
        if (building_object && building_object->runtime_instance()) {
            graphics_variant = building_object->runtime_instance()->graphics_variant();
        }
    }
    buffer_write_u8(buf, graphics_variant);
    buffer_write_u8(buf, b->upgrade_level);

    //strikes
    buffer_write_u8(buf, b->strike_duration_days);

    // sickness
    buffer_write_u8(buf, b->sickness_level);
    buffer_write_u8(buf, b->sickness_duration);
    buffer_write_u8(buf, b->sickness_doctor_cure);
    buffer_write_u8(buf, b->fumigation_frame);
    buffer_write_u8(buf, b->fumigation_direction);

    // extra resources
    for (int i = 0; i < RESOURCE_SLOT_COUNT; i++) {
        buffer_write_i16(buf, b->resources[i]);
    }

    // accepted goods
    for (int i = 0; i < RESOURCE_SLOT_COUNT; i++) {
        buffer_write_u8(buf, building_accepted_good_save_value(b, static_cast<resource_type>(i)));
    }

    // latrines
    buffer_write_u8(buf, b->has_latrines_access);

    write_foundation_terrain_state(buf, foundation_terrain_state);

    // New building state code should always be added at the end to preserve savegame retrocompatibility
    // Also, don't forget to update BUILDING_STATE_CURRENT_BUFFER_SIZE and if possible, add a new macro like
    // BUILDING_STATE_NEW_FEATURE_BUFFER_SIZE with the full building state buffer size including all added features
    // up until that point in Augustus' development
}

static void read_type_data(
    buffer *buf,
    building *b,
    building_save_bridge::LegacyBuildingSaveDto *legacy,
    int version,
    int save_type_is_monument,
    int for_preview)
{
    // This function should ALWAYS read 42 bytes for versions before or at SAVE_GAME_LAST_STATIC_RESOURCES.
    // The only exception is for Caravanserai on old savegame versions, which due to an oversight only read 41 bytes.
    // For versions after SAVE_GAME_LAST_STATIC_RESOURCES, the function should ALWAYS read 26 bytes.
    // If you don't need to read all bytes, they will be automatically skipped at the end.
    int type_data_bytes;
    if (version <= SAVE_GAME_LAST_STATIC_RESOURCES) {
        type_data_bytes = TYPE_DATA_ORIGINAL_BUFFER_SIZE;

        // Old savegame versions had a bug where the caravanserai's building type data size was off by 1
        // Old save versions don't need to skip the byte, while new save versions do
        if (type_is_caravanserai(b->type) && version <= SAVE_GAME_LAST_CARAVANSERAI_WRONG_OFFSET) {
            type_data_bytes -= 1;
        }
    } else {
        type_data_bytes = TYPE_DATA_CURRENT_BUFFER_SIZE;
    }
    size_t buffer_index = buf->index;
    const int is_dock = type_attr_is(b->type, "dock");
    const building_type_registry_impl::BuildingType *definition = definition_for_type(b->type);

    if (definition && definition->has_housing() && legacy) {
        if (version <= SAVE_GAME_LAST_STATIC_RESOURCES) {
            for (int i = 0; i < resource_id_bridge_legacy_inventory_count(); i++) {
                b->resources[resource_map_legacy_inventory(i)] = buffer_read_i16(buf);
            }
        }
        legacy->housing.theater = buffer_read_u8(buf);
        legacy->housing.amphitheater_actor = buffer_read_u8(buf);
        legacy->housing.amphitheater_gladiator = buffer_read_u8(buf);
        legacy->housing.colosseum_gladiator = buffer_read_u8(buf);
        legacy->housing.colosseum_lion = buffer_read_u8(buf);
        legacy->housing.hippodrome = buffer_read_u8(buf);
        legacy->housing.school = buffer_read_u8(buf);
        legacy->housing.library = buffer_read_u8(buf);
        legacy->housing.academy = buffer_read_u8(buf);
        legacy->housing.barber = buffer_read_u8(buf);
        legacy->housing.clinic = buffer_read_u8(buf);
        legacy->housing.bathhouse = buffer_read_u8(buf);
        legacy->housing.hospital = buffer_read_u8(buf);
        legacy->housing.temple_ceres = buffer_read_u8(buf);
        legacy->housing.temple_neptune = buffer_read_u8(buf);
        legacy->housing.temple_mercury = buffer_read_u8(buf);
        legacy->housing.temple_mars = buffer_read_u8(buf);
        legacy->housing.temple_venus = buffer_read_u8(buf);
        legacy->housing.no_space_to_expand = buffer_read_u8(buf);
        legacy->housing.num_foods = buffer_read_u8(buf);
        legacy->housing.entertainment = buffer_read_u8(buf);
        legacy->housing.education = buffer_read_u8(buf);
        legacy->housing.health = buffer_read_u8(buf);
        legacy->housing.num_gods = buffer_read_u8(buf);
        legacy->housing.devolve_delay = buffer_read_u8(buf);
        legacy->housing.evolve_text_id = buffer_read_u8(buf);
        // Do not place this after supplier-distribution or monument handling.
        // Because Caravanserai is monument AND supplier building and resources_needed / inventory is same memory spot
    } else if (type_is_caravanserai(b->type)) {
        if (version <= SAVE_GAME_LAST_STATIC_RESOURCES) {
            for (int i = 0; i < resource_id_bridge_legacy_resource_count(); i++) {
                b->resources[resource_remap(i)] = buffer_read_i16(buf);
            }
        }
        if (version <= SAVE_GAME_LAST_MONUMENT_TYPE_DATA) {
            b->monument.upgrades = buffer_read_i32(buf);
            b->monument.progress = buffer_read_i16(buf);
            b->monument.phase = buffer_read_i16(buf);
        }
        b->data.market.fetch_inventory_id =
            static_cast<unsigned char>(resource_map_legacy_inventory(buffer_read_u8(buf)));
        // As above, Ceres and Venus temples are both monuments and suppliers
    } else if (type_is_large_temple_supplier(b->type)) {
        if (version <= SAVE_GAME_LAST_STATIC_RESOURCES) {
            for (int i = 0; i < resource_id_bridge_legacy_resource_count(); i++) {
                b->resources[resource_remap(i)] = buffer_read_i16(buf);
            }
        }
        if (version <= SAVE_GAME_LAST_MONUMENT_TYPE_DATA) {
            b->monument.upgrades = buffer_read_i32(buf);
            b->monument.progress = buffer_read_i16(buf);
            b->monument.phase = buffer_read_i16(buf);
            if (!b->monument.phase) { // Compatibility fix
                b->monument.phase = MONUMENT_FINISHED;
            }
        }
        b->data.market.fetch_inventory_id =
            static_cast<unsigned char>(resource_map_legacy_inventory(buffer_read_u8(buf)));
    } else if (type_has_distribution(b->type)) {
        if (version <= SAVE_GAME_LAST_STATIC_RESOURCES) {
            buffer_skip(buf, 2);
            for (int i = 0; i < resource_id_bridge_legacy_inventory_count(); i++) {
                b->resources[resource_map_legacy_inventory(i)] = buffer_read_i16(buf);
            }
            int pottery_demand = buffer_read_i16(buf);
            if (b->accepted_goods[resource_pottery()]) {
                building_set_distribution_demand(
                    b, resource_pottery(), static_cast<unsigned char>(std::clamp(pottery_demand, 0, 254)));
            }
            int furniture_demand = buffer_read_i16(buf);
            if (b->accepted_goods[resource_furniture()]) {
                building_set_distribution_demand(
                    b, resource_furniture(), static_cast<unsigned char>(std::clamp(furniture_demand, 0, 254)));
            }
            int oil_demand = buffer_read_i16(buf);
            if (b->accepted_goods[resource_oil()]) {
                building_set_distribution_demand(
                    b, resource_oil(), static_cast<unsigned char>(std::clamp(oil_demand, 0, 254)));
            }
            int wine_demand = buffer_read_i16(buf);
            if (b->accepted_goods[resource_wine()]) {
                building_set_distribution_demand(
                    b, resource_wine(), static_cast<unsigned char>(std::clamp(wine_demand, 0, 254)));
            }
        }
        b->data.market.fetch_inventory_id =
            static_cast<unsigned char>(resource_map_legacy_inventory(buffer_read_u8(buf)));
        b->data.market.is_mess_hall = buffer_read_u8(buf);
    } else if (type_is_granary(b->type) && version <= SAVE_GAME_LAST_GRANARY_WAREHOUSE_NON_ROADBLOCKS) {
        if (version <= SAVE_GAME_LAST_STATIC_RESOURCES) {
            buffer_skip(buf, 2);
            for (int i = 0; i < resource_id_bridge_legacy_resource_count(); i++) {
                b->resources[resource_remap(i)] = buffer_read_i16(buf);
            }
        }
        b->data.roadblock.exceptions = ROADBLOCK_PERMISSION_ALL;
    } else if (type_is_warehouse(b->type) || type_is_granary(b->type)) {
        if (version <= SAVE_GAME_LAST_GRANARY_WAREHOUSE_NON_ROADBLOCKS) {
            b->data.roadblock.exceptions = ROADBLOCK_PERMISSION_ALL;
        } else {
            b->data.roadblock.exceptions = buffer_read_u16(buf);
        }
        if (type_is_warehouse(b->type)) {
            read_rubble_type_data(buf, b, version, for_preview);
        }
    } else if (save_type_is_monument && version <= SAVE_GAME_LAST_MONUMENT_TYPE_DATA) {
        if (version <= SAVE_GAME_LAST_STATIC_RESOURCES) {
            for (int i = 0; i < resource_id_bridge_legacy_resource_count(); i++) {
                b->resources[resource_remap(i)] = buffer_read_i16(buf);
            }
            if (b->resources[RESOURCE_NONE] < 0) {
                b->resources[RESOURCE_NONE] = 1;
            }
        }
        b->monument.upgrades = buffer_read_i32(buf);
        b->monument.progress = buffer_read_i16(buf);
        b->monument.phase = buffer_read_i16(buf);
    } else if (type_is_depot(b->type)) {
        b->data.depot.current_order.resource_type = resource_remap(buffer_read_i8(buf));
        b->data.depot.current_order.src_storage_id = buffer_read_i32(buf);
        b->data.depot.current_order.dst_storage_id = buffer_read_i32(buf);
        b->data.depot.current_order.condition.condition_type = (order_condition_type) buffer_read_i8(buf);
        b->data.depot.current_order.condition.threshold = buffer_read_i8(buf);
        for (int i = 0; i < 3; i++) {
            b->data.distribution.cartpusher_ids[i] = buffer_read_i16(buf);
        }
    } else if (is_dock) {
        b->data.dock.queued_docker_id = buffer_read_i16(buf);
        b->data.dock.has_accepted_route_ids = buffer_read_u8(buf);
        b->data.dock.accepted_route_ids = buffer_read_i32(buf);
        if (version <= SAVE_GAME_LAST_STATIC_RESOURCES) {
            buffer_skip(buf, 20);
        }
        b->data.dock.num_ships = buffer_read_u8(buf);
        if (version <= SAVE_GAME_LAST_STATIC_RESOURCES) {
            buffer_skip(buf, 2);
        }
        b->data.dock.orientation = buffer_read_i8(buf);
        b->subtype.orientation = b->data.dock.orientation;
        if (version <= SAVE_GAME_LAST_STATIC_RESOURCES) {
            buffer_skip(buf, 3);
        }
        for (int i = 0; i < 3; i++) {
            b->data.distribution.cartpusher_ids[i] = buffer_read_i16(buf);
        }
        b->data.dock.trade_ship_id = buffer_read_i16(buf);
    } else if (type_is_roadblock_record(b->type)) {
        b->data.roadblock.exceptions = buffer_read_u16(buf);
    } else if (is_industry_type(b)) {
        b->data.industry.progress = buffer_read_i16(buf);
        if (version <= SAVE_GAME_LAST_STATIC_RESOURCES) {
            buffer_skip(buf, 11);
        }
        b->data.industry.is_stockpiling = buffer_read_u8(buf);
        b->data.industry.has_fish = buffer_read_u8(buf);
        if (version <= SAVE_GAME_LAST_STATIC_RESOURCES) {
            buffer_skip(buf, 14);
        }
        b->data.industry.blessing_days_left = buffer_read_u8(buf);
        b->subtype.orientation = buffer_read_u8(buf);
        b->data.industry.has_raw_materials = buffer_read_u8(buf);
        if (version <= SAVE_GAME_LAST_STATIC_RESOURCES) {
            buffer_skip(buf, 1);
        }
        b->data.industry.curse_days_left = buffer_read_u8(buf);
        if (type_uses_monthly_production_stats(b->type)) {
            b->data.industry.age_months = buffer_read_u8(buf);
            b->data.industry.average_production_per_month = buffer_read_u8(buf);
            b->data.industry.production_current_month = buffer_read_i16(buf);
            if (version <= SAVE_GAME_LAST_STATIC_RESOURCES) {
                buffer_skip(buf, 2);
            }
        } else if (version <= SAVE_GAME_LAST_STATIC_RESOURCES) {
            buffer_skip(buf, 6);
        }
        b->data.industry.fishing_boat_id = buffer_read_i16(buf);
        if ((int) (buf->index - buffer_index) + 2 <= type_data_bytes) {
            b->data.industry.second_fishing_boat_id = buffer_read_i16(buf);
        } else {
            b->data.industry.second_fishing_boat_id = 0;
        }
    } else if (type_is_rubble_shell(b->type) && version > SAVE_GAME_LAST_U16_GRIDS) {
        read_rubble_type_data(buf, b, version, for_preview);
    } else {
        if (version <= SAVE_GAME_LAST_STATIC_RESOURCES) {
            buffer_skip(buf, 26);
        }
        b->data.entertainment.num_shows = buffer_read_u8(buf);
        b->data.entertainment.days1 = buffer_read_u8(buf);
        b->data.entertainment.days2 = buffer_read_u8(buf);
        b->data.entertainment.play = buffer_read_u8(buf);
        if (version <= SAVE_GAME_LAST_LEGACY_ENTERTAINMENT_SHOW_HALF_DAYS) {
            b->data.entertainment.days1 =
                static_cast<unsigned char>(migrate_legacy_entertainment_show_days(b->data.entertainment.days1));
            b->data.entertainment.days2 =
                static_cast<unsigned char>(migrate_legacy_entertainment_show_days(b->data.entertainment.days2));
        }
    }
    int remaining_bytes = type_data_bytes - (int) (buf->index - buffer_index);
    if (remaining_bytes > 0) {
        buffer_skip(buf, remaining_bytes);
    }
}

static void migrate_accepted_goods(building *b, int permissions)
{
    const int is_dock = type_attr_is(b->type, "dock");
    int max = is_dock ? resource_id_bridge_legacy_resource_count() : resource_id_bridge_legacy_inventory_count();
    for (int i = 0; i < max; i++) {
        int goods_bit = 1 << i;
        int id = is_dock ? resource_remap(i) : resource_map_legacy_inventory(i);
        b->accepted_goods[id] = !(permissions & goods_bit);
    }
}

static int dock_has_any_accepted_goods(const building *b)
{
    if (!b) {
        return 0;
    }
    for (resource_type r = (RESOURCE_NONE + 1); r < RESOURCE_SLOT_COUNT; r = static_cast<resource_type>(r + 1)) {
        if (b->accepted_goods[r]) {
            return 1;
        }
    }
    return 0;
}

static void repair_dock_accepted_goods_if_empty(building *b)
{
    if (!b || !type_attr_is(b->type, "dock") || dock_has_any_accepted_goods(b)) {
        return;
    }
    for (resource_type r = (RESOURCE_NONE + 1); r < RESOURCE_SLOT_COUNT; r = static_cast<resource_type>(r + 1)) {
        b->accepted_goods[r] = 1;
    }
}

static void apply_definition_record_properties_after_load(building *b)
{
    const building_type_registry_impl::BuildingType *definition = definition_for_type(b->type);
    if (!definition) {
        return;
    }

    if (!definition->has_flags()) {
        return;
    }
    const auto &flags = definition->flags();
    if (flags.has_fire_proof()) {
        b->fire_proof = static_cast<unsigned char>(flags.fire_proof() ? 1 : 0);
        if (b->fire_proof) {
            b->damage_risk = 0;
            b->fire_risk = 0;
            if (!definition->has_rubble() || !definition->rubble().is_burning()) {
                b->fire_duration = 0;
            }
        }
    }
}

int building_state_load_from_buffer(buffer *buf, building *b, int building_buf_size, int save_version, int for_preview)
{
    size_t record_start = buf->index;
    unsigned char legacy_graphics_variant = 0;
    building_save_bridge::LegacyBuildingSaveDto legacy;
    b->state = buffer_read_u8(buf);
    b->faction_id = buffer_read_u8(buf);
    b->unknown_value = buffer_read_u8(buf);
    legacy.size = buffer_read_u8(buf);
    legacy.house_is_merged = buffer_read_u8(buf);
    legacy.house_size = buffer_read_u8(buf);
    b->x = buffer_read_u8(buf);
    b->y = buffer_read_u8(buf);
    b->grid_offset = buffer_read_i16(buf);
    uint16_t saved_building_type = buffer_read_u16(buf);
    int missing_building_type = building_type_id_bridge_save_id_is_missing(saved_building_type);
    b->type = building_type_id_bridge_runtime_from_save_id(saved_building_type);
    const int is_dock = type_attr_is(b->type, "dock");
    legacy.housing_level_or_subtype = buffer_read_i16(buf);
    if (type_is_warehouse_space(b->type)) {
        b->subtype.warehouse_resource_id = static_cast<short>(resource_remap(legacy.housing_level_or_subtype));
    } else if (save_version <= SAVE_GAME_LAST_STATIC_RESOURCES &&
        (is_dock || type_has_distribution(b->type))) {
        migrate_accepted_goods(b, legacy.housing_level_or_subtype);
    } else if (saved_building_type == LEGACY_SAVE_TYPE_MENU_FORT) { // Forts used to use a generic type for the main building
        b->subtype.fort_figure_type = legacy.housing_level_or_subtype;
        b->type = get_fort_type(b); // get the correct fort type to ensure compatibility
    } else {
        const building_type_registry_impl::BuildingType *definition = definition_for_type(b->type);
        if (!definition || !definition->has_housing()) {
            copy_subtype_bits(legacy.housing_level_or_subtype, b);
        }
    }
    b->road_network_id = buffer_read_u8(buf);
    legacy.housing_monthly_levy_or_building_levy = buffer_read_u8(buf);
    b->created_sequence = buffer_read_u16(buf);
    b->houses_covered = buffer_read_i16(buf);
    b->labor_access_score = 0.0f;
    b->percentage_houses_covered = buffer_read_i16(buf);
    legacy.housing_population = buffer_read_i16(buf);
    legacy.housing_population_room = buffer_read_i16(buf);
    b->distance_from_entry = buffer_read_i16(buf);
    legacy.housing_highest_population = buffer_read_i16(buf);
    legacy.housing_unreachable_ticks = buffer_read_i16(buf);
    b->road_access_x = buffer_read_u8(buf);
    b->road_access_y = buffer_read_u8(buf);
    b->figure_id = buffer_read_i16(buf);
    b->figure_id2 = buffer_read_i16(buf);
    legacy.housing_immigrant_figure_id = static_cast<uint16_t>(buffer_read_i16(buf));
    b->figure_id4 = buffer_read_i16(buf);
    b->figure_spawn_delay = buffer_read_u8(buf);
    b->days_since_offering = buffer_read_u8(buf);
    b->figure_roam_direction = buffer_read_u8(buf);
    b->has_water_access = buffer_read_u8(buf);
    legacy.housing_tavern_wine_access = buffer_read_u8(buf);
    legacy.housing_tavern_food_access = buffer_read_u8(buf);
    // Legacy load DTO boundary. Native composition consumes these ids during
    // hydration; only dynamic bridges retain them afterward.
    b->prev_part_building_id = buffer_read_i16(buf);
    b->next_part_building_id = buffer_read_i16(buf);
    int loads_stored = buffer_read_i16(buf);
    legacy.housing_sentiment_message = buffer_read_u8(buf);
    b->has_well_access = buffer_read_u8(buf);
    b->num_workers = buffer_read_i16(buf);
    b->labor_category = buffer_read_u8(buf);
    b->output_resource_id = static_cast<unsigned char>(resource_remap(buffer_read_u8(buf)));
    b->has_road_access = buffer_read_u8(buf);
    legacy.housing_criminal_active = buffer_read_u8(buf);
    b->damage_risk = buffer_read_i16(buf);
    b->fire_risk = buffer_read_i16(buf);
    b->fire_duration = buffer_read_i16(buf);
    b->fire_proof = buffer_read_u8(buf);
    legacy.housing_generation_delay_or_rubble_seed = buffer_read_u8(buf);
    legacy.housing_tax_coverage = buffer_read_u8(buf);
    legacy.housing_pantheon_access = buffer_read_u8(buf);
    b->formation_id = buffer_read_i16(buf);

    const char *early_type_problem = loaded_building_type_problem(b, saved_building_type, missing_building_type);
    if (early_type_problem) {
        quarantine_loaded_building_type_problem(b, saved_building_type, early_type_problem, for_preview);
        skip_remaining_building_record(buf, record_start, building_buf_size);
        return 1;
    }

    int save_type_monument = saved_type_is_monument(saved_building_type, b->type);
    read_type_data(buf, b, &legacy, save_version, save_type_monument, for_preview);
    const building_type_registry_impl::BuildingType *loaded_definition = definition_for_type(b->type);
    const building_type_registry_impl::FoundationDef *loaded_foundation =
        loaded_definition ? loaded_definition->foundation_def() : nullptr;
    if (loaded_foundation && loaded_foundation->rotates()) {
        b->subtype.orientation = static_cast<short>(
            building_type_registry_impl::foundation_rotation_from_save(
                *loaded_foundation,
                b->subtype.orientation,
                save_version,
                SAVE_GAME_LAST_NO_FOUNDATION_TERRAIN_DELTAS));
        if (is_dock) {
            // Compatibility alias for legacy dock code. Building orientation is
            // authoritative for foundation and native graphics runtime state.
            b->data.dock.orientation = static_cast<signed char>(b->subtype.orientation);
        }
    }
    normalize_native_housing_after_load(b, legacy, save_version);
    legacy.housing_tax_income_or_collected_tax = buffer_read_i32(buf);
    legacy.housing_days_without_food = buffer_read_u8(buf);
    b->has_plague = buffer_read_u8(buf);
    b->desirability = buffer_read_i8(buf);
    b->is_deleted = buffer_read_u8(buf);
    b->is_close_to_water = buffer_read_u8(buf);
    b->storage_id = buffer_read_u8(buf);
    legacy.housing_happiness_or_native_anger = buffer_read_i8(buf);
    b->show_on_problem_overlay = buffer_read_u8(buf);

    // Wharves produce fish and don't need any progress
    if (type_attr_is(b->type, "wharf")) {
        b->output_resource_id = static_cast<unsigned char>(resource_fish());
        b->data.industry.progress = 0;
    }

    // Triumphal arches may have wrong orientation
    if (type_attr_is(b->type, "triumphal_arch") && b->subtype.orientation == 3) {
        b->subtype.orientation = 2;
    }

    if (building_buf_size < BUILDING_STATE_STRIKES) {
        // Backwards compatibility fixes for sentiment update
        if (legacy.housing_population && legacy.housing_happiness_or_native_anger < 20) {
            legacy.housing_happiness_or_native_anger = 30;
        }

        // Backwards compatibility fixes for culture update
        if (building_monument_is_monument(b) && legacy.housing_level_or_subtype && !type_attr_is(b->type, "hippodrome") &&
            saved_building_type <= LEGACY_SAVE_TYPE_LIGHTHOUSE) {
            b->monument.phase = legacy.housing_level_or_subtype;
        }

        if ((type_attr_is(b->type, "hippodrome") || type_attr_is(b->type, "colosseum")) && !b->monument.phase) {
            b->monument.phase = MONUMENT_FINISHED;
        }

        if (building_monument_is_monument(b) && type_is_legacy_large_temple_or_oracle(b->type) && !b->monument.phase) {
            b->monument.phase = MONUMENT_FINISHED;
        }

    }

    if (save_version < SAVE_GAME_ROADBLOCK_DATA_MOVED_FROM_SUBTYPE) {
        // Backwards compatibility - roadblock data used to be stored in b->subtype
        if (type_is_roadblock_record(b->type)) {
            b->data.roadblock.exceptions = b->subtype.orientation;
        }
    }

    // Backwards compatibility - double the current progress of industry buildings, except for wheat farms
    if (save_version < SAVE_GAME_LAST_NO_GOLD_AND_MINTING && b->output_resource_id && !type_attr_is(b->type, "wheat_farm")) {
        b->data.industry.progress *= 2;
    }

    // Backwards compatibility - set roadblock permissions for gatehouses and triumphal arches
    if (save_version <= SAVE_GAME_LAST_MONUMENT_TYPE_DATA) {
        if (type_attr_is(b->type, "triumphal_arch")) {
            b->data.roadblock.exceptions = ROADBLOCK_PERMISSION_ALL;
        } else if (type_attr_is(b->type, "gatehouse")) {
            b->data.roadblock.exceptions = 0;
        }
    }

    // To keep backward savegame compatibility, only fill more recent building struct elements
    // if building_buf_size is the correct size when those elements are included
    // For example, if you add an int (4 bytes) to the building state struct, in order to check
    // if the samegame version has that new int, you should add the folloging code:
    // if (building_buf_size >= BULDING_STATE_ORIGINAL_BUFFER_SIZE + 4) {
    //    b->new_var = buffer_read_i32(buf);
    // }
    // Or even better:
    // if (building_buf_size >= BULDING_STATE_NEW_FEATURE_BUFFER_SIZE) {
    //    b->new_var = buffer_read_i32(buf);
    // }
    // Building state variables are automatically set to 0, so if the savegame version doesn't include
    // that information, you can be assured that the game will read it as 0

    if (save_version > SAVE_GAME_LAST_MONUMENT_TYPE_DATA) {
        b->monument.upgrades = buffer_read_i32(buf);
        b->monument.progress = buffer_read_i16(buf);
        b->monument.phase = buffer_read_i16(buf);
    }

    normalize_monument_phase_after_load(b);

    if (building_buf_size >= BUILDING_STATE_TOURISM_BUFFER_SIZE) {
        legacy.housing_arena_gladiator = buffer_read_u8(buf);
        legacy.housing_arena_lion = buffer_read_u8(buf);
        b->is_tourism_venue = buffer_read_u8(buf);
        b->tourism_disabled = buffer_read_u8(buf);
        b->tourism_income = buffer_read_u8(buf);
        b->tourism_income_this_year = buffer_read_u8(buf);
    }

    if (building_buf_size >= BUILDING_STATE_VARIANTS_AND_UPGRADES) {
        legacy_graphics_variant = buffer_read_u8(buf);
        b->upgrade_level = buffer_read_u8(buf);
    }
    if (building_buf_size < BUILDING_STATE_VARIANTS_AND_UPGRADES &&
        saved_building_type >= LEGACY_SAVE_TYPE_PAVILION_FIRST &&
        saved_building_type <= LEGACY_SAVE_TYPE_PAVILION_LAST &&
        type_attr_is(b->type, "pavilion")) {
        legacy_graphics_variant = static_cast<unsigned char>(saved_building_type - LEGACY_SAVE_TYPE_PAVILION_FIRST);
    }

    if (building_buf_size >= BUILDING_STATE_STRIKES) {
        b->strike_duration_days = buffer_read_u8(buf);
    }

    if (building_buf_size >= BUILDING_STATE_SICKNESS) {
        b->sickness_level = buffer_read_u8(buf);
        b->sickness_duration = buffer_read_u8(buf);
        b->sickness_doctor_cure = buffer_read_u8(buf);
        b->fumigation_frame = buffer_read_u8(buf);
        b->fumigation_direction = buffer_read_u8(buf);
    }

    if (save_version > SAVE_GAME_LAST_STATIC_RESOURCES) {
        int resource_slots = flat_resource_slots_left_in_record(buf, record_start, building_buf_size);
        for (int i = 0; i < resource_slots; i++) {
            b->resources[resource_remap(i)] = buffer_read_i16(buf);
        }
        for (int i = 0; i < resource_slots; i++) {
            building_load_accepted_good(b, resource_remap(i), buffer_read_u8(buf));
        }
    }
    repair_dock_accepted_goods_if_empty(b);

    if (building_buf_size >= BUILDING_STATE_LATRINES &&
        remaining_building_record_bytes(buf, record_start, building_buf_size) > 0) {
        b->has_latrines_access = buffer_read_u8(buf);
    }

    if (building_buf_size >= BUILDING_STATE_FOUNDATION_TERRAIN_DELTAS &&
        save_version > SAVE_GAME_LAST_NO_FOUNDATION_TERRAIN_DELTAS) {
        const building_type_registry_impl::FoundationTerrainSaveState foundation_state =
            read_foundation_terrain_state(buf);
        if (!for_preview && b->id && foundation_state.published == 1) {
            building_runtime_stage_loaded_foundation_state(b->id, foundation_state);
        }
    }

    // Update resource requirement changes on monuments
    if (building_monument_is_monument(b) && b->monument.phase != MONUMENT_FINISHED) {
        for (int resource = 0; resource < RESOURCE_SLOT_COUNT; resource++) {
            resource_type resource_type_id = (resource_type) resource;
            int resource_needed_for_phase =
                building_monument_resources_needed_for_monument_type(b->type, resource_type_id, b->monument.phase);
            if (b->resources[resource_type_id] > resource_needed_for_phase) {
                b->resources[resource_type_id] = static_cast<short>(resource_needed_for_phase);
            }
        }
    }

    // Backwards compatibility - update loads stored to the proper new variable
    if (save_version <= SAVE_GAME_LAST_NO_NEW_MONUMENT_RESOURCES && !building_monument_is_unfinished_monument(b)) {
        if (type_attr_is(b->type, "grand_temple_mars") || type_attr_is(b->type, "barracks")) {
            b->resources[resource_weapons()] = static_cast<short>(loads_stored);
        } else if (type_attr_is(b->type, "pottery_workshop")) {
            b->resources[resource_clay()] = static_cast<short>(loads_stored * resource_units_per_load());
        } else if (type_attr_is(b->type, "oil_workshop")) {
            b->resources[resource_oil()] = static_cast<short>(loads_stored * resource_units_per_load());
        } else if (type_attr_is(b->type, "wine_workshop")) {
            b->resources[resource_vines()] = static_cast<short>(loads_stored * resource_units_per_load());
        } else if (type_attr_is(b->type, "furniture_workshop")) {
            b->resources[resource_timber()] = static_cast<short>(loads_stored * resource_units_per_load());
        } else if (type_attr_is(b->type, "weapons_workshop")) {
            b->resources[resource_iron()] = static_cast<short>(loads_stored * resource_units_per_load());
        } else if (type_attr_is(b->type, "city_mint")) {
            b->resources[resource_gold()] = static_cast<short>(loads_stored);
        } else if (type_attr_is(b->type, "small_temple_neptune") || type_attr_is(b->type, "large_temple_neptune")) {
            b->days_since_offering = static_cast<unsigned char>(loads_stored);
        } else if (type_is_warehouse_space(b->type)) {
            b->resources[b->subtype.warehouse_resource_id] = static_cast<short>(loads_stored);
        } else if (type_attr_is(b->type, "lighthouse")) {
            b->resources[resource_timber()] = static_cast<short>(loads_stored);
        } else if (type_attr_is(b->type, "grand_temple_venus")) {
            b->resources[resource_wine()] = static_cast<short>(loads_stored);
        }
    }

    // Fix bug where warehouses have invalid resources stored
    if (type_is_warehouse_space(b->type)) {
        for (int r = (RESOURCE_NONE + 1); r < RESOURCE_SLOT_COUNT; r++) {
            if (r != b->subtype.warehouse_resource_id) {
                b->resources[r] = 0;
            }
        }
    }

    if (
        (type_attr_is(b->type, "lighthouse") || type_is_caravanserai(b->type)) &&
        b->figure_id2 &&
        !for_preview &&
        Figure::get(b->figure_id2)->type != FIGURE_LABOR_SEEKER
    ) {
        b->figure_id = b->figure_id2;
        b->figure_id2 = 0;
    }

    // Old save barracks and temple of mars should accept weapons by default
    if (type_attr_is(b->type, "barracks") || type_attr_is(b->type, "grand_temple_mars")) {
        if (!b->accepted_goods[RESOURCE_NONE]) {
            b->accepted_goods[RESOURCE_NONE] = 1; // set RESOURCE_NONE to 1 to mark this as a new save compatibility
            b->accepted_goods[resource_weapons()] = 1;
        }
    }

    // The following code should only be executed if the savegame includes building information that is not
    // supported on this specific version of Augustus. The extra bytes in the buffer must be skipped in order
    // to prevent reading bogus data for the next building
    skip_remaining_building_record(buf, record_start, building_buf_size);

    const char *type_problem = loaded_building_type_problem(b, saved_building_type, missing_building_type);
    if (type_problem) {
        quarantine_loaded_building_type_problem(b, saved_building_type, type_problem, for_preview);
        return 1;
    }

    apply_definition_record_properties_after_load(b);

    const building_type_registry_impl::BuildingType *definition =
        building_type_registry_impl::definition_for_type(b->type);
    if (!definition || !definition->has_housing()) {
        b->levy_amount = legacy.housing_monthly_levy_or_building_levy;
        b->collected_tax_income = legacy.housing_tax_income_or_collected_tax;
        b->native_anger = legacy.housing_happiness_or_native_anger;
    }

    if (!for_preview) {
        // Keep load as full records plus staged module state until building_runtime_initialize_city_graphics_cache().
        BuildingGraphicsState graphics_state;
        graphics_state.set_variant(legacy_graphics_variant);
        building_runtime_stage_loaded_graphics_state(
            b->id,
            graphics_state);
        if (definition && definition->has_housing()) {
            building_runtime_stage_loaded_housing_state(
                b->id, housing_state_from_legacy_save(legacy));
        }
        RubbleState rubble_state;
        if (building_runtime_loaded_rubble_state(b->id, &rubble_state)) {
            rubble_state.random_seed = legacy.housing_generation_delay_or_rubble_seed;
            building_runtime_stage_loaded_rubble_state(b->id, rubble_state);
        }
    }
    return 0;
}
