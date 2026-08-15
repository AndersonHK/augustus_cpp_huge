#include "building/building_type.h"
#include "building/plague_state.h"
#include "building/HousingProfileDef.h"
#include "building/construction.h"
#include "building/construction_building.h"
#include "building/construction_clear.h"
#include "building/distribution.h"
#include "building/industry.h"
#include "building/local_workforce.h"
#include "building/roadblock.h"
#include "building/state.h"
#include "building/storage.h"
#include "building/storage_runtime.h"
#include "city/culture.h"
#include "city/warning.h"
#include "game/undo.h"
#include "map/aqueduct.h"
#include "map/bridge.h"
#include "map/building.h"
#include "map/building_tiles.h"
#include "map/road_access.h"
#include "map/tiles.h"
#include "map/water.h"
#include "map/water_supply.h"

#include "building.h"

#include "building/BuildingGraphics.h"
#include "building/BuildingGraphicsState.h"
#include "building/BuildingGeometry.h"
#include "building/water_access_runtime.h"
#include "building/CompositionHydration.h"
#include "building/CompositionTypeReplacement.h"
#include "building/building_record.h"
#include "building/building_runtime_internal.h"
#include "building/dock.h"
#include "building/building_type_registry_internal.h"
#include "building/production_method.h"
#include "building/production_runtime.h"
#include "core/crash_context.h"
#include "figure/figure.h"
#include "figure/formation_legion.h"
#include "figure/PathingMode.h"
#include "figuretype/fishing_boat.h"

#include <algorithm>
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <deque>
#include <exception>
#include <initializer_list>
#include <source_location>
#include <string>
#include <vector>

#include "building/building_type_id_bridge.h"
#include "building/building_type_legacy_migration.h"
#include "building/granary.h"
#include "building/menu.h"
#include "building/monument.h"
#include "building/properties.h"
#include "building/warehouse.h"
#include "city/buildings.h"
#include "city/finance.h"
#include "city/population.h"
#include "core/calc.h"
#include "core/config.h"
#include "core/log.h"
#include "figuretype/missile.h"
#include "game/difficulty.h"
#include "game/save_version.h"
#include "map/desirability.h"
#include "map/elevation.h"
#include "map/figure.h"
#include "map/grid.h"
#include "map/image.h"
#include "map/property.h"
#include "map/random.h"
#include "map/road_network.h"
#include "map/sprite.h"
#include "figure/route.h"
#include "map/terrain.h"

#define WATER_DESIRABILITY_RANGE 3
#define WATER_DESIRABILITY_BONUS 15

static struct {
    std::deque<building> buildings;
    building *first_of_type[BUILDING_TYPE_MAX];
    building *last_of_type[BUILDING_TYPE_MAX];
} data = {};

using building_type_registry_impl::type_attr_is;
using building_type_registry_impl::type_attr_is_any;
using building_type_registry_impl::type_from_attr;
using building_type_registry_impl::definition_for_type;

static int geometry_has_water_within_range(
    const building_type_registry_impl::BuildingGeometry &geometry,
    int range)
{
    if (!geometry.valid() || range < 0) {
        return 0;
    }
    const building_type_registry_impl::BuildingGeometryBounds &bounds = geometry.bounds();
    for (int y = bounds.min_y - range; y < bounds.max_y + range; ++y) {
        for (int x = bounds.min_x - range; x < bounds.max_x + range; ++x) {
            if (map_grid_is_inside(x, y, 1) && geometry.contains_within_range(x, y, range) &&
                map_terrain_is(map_grid_offset(x, y), TERRAIN_WATER)) {
                return 1;
            }
        }
    }
    return 0;
}

static int output_cart_capacity_from_methods(
    const std::vector<building_type_registry_impl::ProductionMethod *> &methods,
    resource_type resource)
{
    int capacity = 0;
    for (const building_type_registry_impl::ProductionMethod *method : methods) {
        if (method && method->outputs_to_building_storage() && method->output_resource() == resource) {
            capacity = std::max(capacity, method->cart_capacity());
        }
    }
    return capacity;
}

static int output_cart_capacity_for_definition(
    const building_type_registry_impl::BuildingType *definition,
    resource_type resource)
{
    if (!definition) {
        return 1;
    }

    int capacity = output_cart_capacity_from_methods(definition->production_methods(), resource);
    for (const building_type_registry_impl::CompositionChildDef &part : definition->composition().children()) {
        const building_type_registry_impl::BuildingType *part_definition = part.type;
        if (part_definition) {
            capacity = std::max(capacity,
                output_cart_capacity_from_methods(part_definition->production_methods(), resource));
        }
    }
    return capacity > 0 ? capacity : 1;
}

static struct {
    int created_sequence;
    int incorrect_houses;
    int unfixable_houses;
} extra;

static void initialize_building_slot(building &record, unsigned int id)
{
    building_runtime_impl::discard_instance_for_reused_record(&record);
    record = {};
    record.id = id;
}

static void resize_buildings(size_t size)
{
    const size_t old_size = data.buildings.size();
    data.buildings.resize(size);
    for (size_t i = old_size; i < data.buildings.size(); i++) {
        initialize_building_slot(data.buildings[i], static_cast<unsigned int>(i));
    }
}

static building *building_slot(unsigned int id)
{
    return id < data.buildings.size() ? &data.buildings[id] : nullptr;
}

static Building *runtime_building_for_record(building *record)
{
    building_runtime *runtime = building_runtime_impl::get_or_create_instance(record);
    return runtime ? &runtime->building : nullptr;
}

static const building_type_registry_impl::BuildingType *definition_for_record(const building *record)
{
    return record ? definition_for_type(record->type) : nullptr;
}

static int record_foundation_rotation(
    const building &record,
    const building_type_registry_impl::FoundationDef &foundation)
{
    return foundation.rotates() ? (record.subtype.orientation % 4 + 4) % 4 : 0;
}

static int record_foundation_contains(const building &record, int x, int y)
{
    const building_type_registry_impl::BuildingType *definition = definition_for_record(&record);
    const building_type_registry_impl::FoundationDef *foundation =
        definition ? definition->foundation_def() : nullptr;
    if (!foundation) {
        return 0;
    }
    const int rotation = record_foundation_rotation(record, *foundation);
    for (const building_type_registry_impl::RotatedFoundationCell &cell :
        foundation->rotated_cells(rotation)) {
        if (record.x + cell.x == x && record.y + cell.y == y) {
            return 1;
        }
    }
    return 0;
}

static int record_matches(const building *record, const char *attr)
{
    const building_type_registry_impl::BuildingType *definition = definition_for_record(record);
    if (definition == nullptr) {
        return 0;
    }
    return definition->attr_is(attr);
}

static building *first_building_slot()
{
    return data.buildings.empty() ? nullptr : &data.buildings.front();
}

static int building_slot_is_active(const building &record)
{
    if (record.state != BUILDING_STATE_UNUSED) {
        return 1;
    }
    return game_undo_contains_building(record.id);
}

static void trim_buildings()
{
    while (data.buildings.size() > 1 && !building_slot_is_active(data.buildings.back())) {
        data.buildings.pop_back();
    }
}

static building *new_building_slot()
{
    for (size_t i = 1; i < data.buildings.size(); i++) {
        if (!building_slot_is_active(data.buildings[i])) {
            initialize_building_slot(data.buildings[i], static_cast<unsigned int>(i));
            return &data.buildings[i];
        }
    }

    resize_buildings(data.buildings.size() + 1);
    return &data.buildings.back();
}

static const char *safe_text(const char *text)
{
    return text && *text ? text : "<none>";
}

static int building_record_requires_type_definition(const building *record)
{
    return record && record->state != BUILDING_STATE_UNUSED;
}

[[noreturn]] static void report_invalid_building_constructor(
    const std::source_location &location,
    const char *message)
{
    char detail[700];
    std::snprintf(
        detail,
        sizeof(detail),
        "caller=%s:%u function=%s",
        safe_text(location.file_name()),
        static_cast<unsigned>(location.line()),
        safe_text(location.function_name()));

    error_context_report_fatal_error_dialog(
        "Building runtime error",
        message,
        detail);
    std::terminate();
}

static building *require_building_record(building *record, const std::source_location &location)
{
    if (!record) {
        report_invalid_building_constructor(location, "Building cannot be constructed from a null record.");
    }
    return record;
}

static BuildingGraphicsState *require_building_graphics_state(
    BuildingGraphicsState *graphics_state,
    const std::source_location &location)
{
    if (!graphics_state) {
        report_invalid_building_constructor(location, "Building cannot be constructed without a BuildingGraphicsState.");
    }
    return graphics_state;
}

static const building_type_registry_impl::BuildingType *definition_for_constructor_record(
    building *record,
    const std::source_location &location)
{
    return building_type_registry_impl::definition_for_type(require_building_record(record, location)->type);
}

static void report_missing_building_type_definition(
    const building *record,
    const std::source_location &location,
    const char *stage)
{
    const building_type runtime_type = record ? record->type : BUILDING_NONE;
    const uint16_t runtime_save_id = building_type_id_bridge_save_id_from_runtime(runtime_type);
    const char *runtime_text_id = building_type_id_bridge_text_from_runtime(runtime_type);
    const char *legacy_text_id =
        building_type_legacy_migration_text_id_for_enum(static_cast<uint16_t>(runtime_type));

    char detail[1400];
    std::snprintf(
        detail,
        sizeof(detail),
        "stage=%s caller=%s:%u function=%s record=%p id=%u state=%d runtime_type=%d runtime_text_id=%s "
        "runtime_save_id=%u legacy_text_id=%s registry_has_definition=%d x=%d y=%d grid_offset=%d "
        "prev_part=%d next_part=%d deleted=%d",
        safe_text(stage),
        safe_text(location.file_name()),
        static_cast<unsigned>(location.line()),
        safe_text(location.function_name()),
        static_cast<const void *>(record),
        record ? record->id : 0,
        record ? record->state : BUILDING_STATE_UNUSED,
        static_cast<int>(runtime_type),
        safe_text(runtime_text_id),
        runtime_save_id,
        safe_text(legacy_text_id),
        building_type_registry_impl::definition_for_type(runtime_type) != nullptr,
        record ? record->x : 0,
        record ? record->y : 0,
        record ? record->grid_offset : 0,
        record ? record->prev_part_building_id : 0,
        record ? record->next_part_building_id : 0,
        record ? record->is_deleted : 0);

    error_context_report_fatal_error_dialog(
        "Building runtime error",
        "Building has no type definition.",
        detail);
    std::terminate();
}

static void report_missing_building_graphics_state(
    const building *record,
    const building_type_registry_impl::BuildingType *type_definition,
    const std::source_location &construction_location,
    const std::source_location &graphics_call_location)
{
    char detail[1600];
    std::snprintf(
        detail,
        sizeof(detail),
        "graphics_caller=%s:%u function=%s constructed_at=%s:%u constructed_function=%s record=%p id=%u "
        "state=%d runtime_type=%d runtime_text_id=%s definition=%s x=%d y=%d grid_offset=%d "
        "prev_part=%d next_part=%d deleted=%d",
        safe_text(graphics_call_location.file_name()),
        static_cast<unsigned>(graphics_call_location.line()),
        safe_text(graphics_call_location.function_name()),
        safe_text(construction_location.file_name()),
        static_cast<unsigned>(construction_location.line()),
        safe_text(construction_location.function_name()),
        static_cast<const void *>(record),
        record ? record->id : 0,
        record ? record->state : BUILDING_STATE_UNUSED,
        record ? static_cast<int>(record->type) : static_cast<int>(BUILDING_NONE),
        record ? safe_text(building_type_id_bridge_text_from_runtime(record->type)) : "<none>",
        type_definition ? safe_text(type_definition->attr()) : "<none>",
        record ? record->x : 0,
        record ? record->y : 0,
        record ? record->grid_offset : 0,
        record ? record->prev_part_building_id : 0,
        record ? record->next_part_building_id : 0,
        record ? record->is_deleted : 0);

    error_context_report_fatal_error_dialog(
        "Building runtime error",
        "Building graphics were requested without a BuildingGraphicsState.",
        detail);
    std::terminate();
}

Building::Building(::building *record, BuildingGraphicsState *graphics_state, const std::source_location &location)
    : Building(
        record,
        definition_for_constructor_record(record, location),
        graphics_state,
        nullptr,
        nullptr,
        location)
{}

Building::Building(::building &record, BuildingGraphicsState &graphics_state, const std::source_location &location)
    : Building(&record, &graphics_state, location)
{}

Building::Building(
    ::building *record,
    const building_type_registry_impl::BuildingType *type_definition,
    BuildingGraphicsState *graphics_state,
    const std::source_location &location)
    : Building(record, type_definition, graphics_state, nullptr, nullptr, location)
{}

Building::Building(
    ::building *record,
    const building_type_registry_impl::BuildingType *type_definition,
    BuildingGraphicsState *graphics_state,
    const RubbleDef *rubble_definition,
    RubbleState *rubble_state,
    const std::source_location &location)
    : type(type_definition)
    , Rubble(nullptr)
    , record_(require_building_record(record, location))
    , construction_location_(location)
{
    bind_record_fields();
    bind_graphics(require_building_graphics_state(graphics_state, location));
    bind_rubble(rubble_definition, rubble_state);
    if (building_record_requires_type_definition(record_) && !type) {
        report_missing_building_type_definition(record_, location, "constructor");
    }
}

Building::Building(
    ::building &record,
    const building_type_registry_impl::BuildingType *type_definition,
    BuildingGraphicsState &graphics_state,
    const std::source_location &location)
    : Building(&record, type_definition, &graphics_state, location)
{}

Building::Building(const Building &other)
    : type(other.type)
    , Rubble(nullptr)
    , Foundation(other.Foundation)
    , Housing(other.Housing)
    , Composition(other.Composition)
    , Formation(other.Formation)
    , record_(other.record_)
    , construction_location_(other.construction_location_)
{
    bind_record_fields();
    bind_graphics(other.graphics_.state());
    bind_rubble(other.Rubble ? other.Rubble->definition() : nullptr, other.Rubble ? other.Rubble->state() : nullptr);
}

Building &Building::operator=(const Building &other)
{
    if (this == &other) {
        return *this;
    }
    type = other.type;
    Rubble = nullptr;
    Foundation = other.Foundation;
    Housing = other.Housing;
    Composition = other.Composition;
    Formation = other.Formation;
    record_ = other.record_;
    construction_location_ = other.construction_location_;
    bind_record_fields();
    bind_graphics(other.graphics_.state());
    bind_rubble(other.Rubble ? other.Rubble->definition() : nullptr, other.Rubble ? other.Rubble->state() : nullptr);
    return *this;
}

void Building::bind_record_fields()
{
    id = RecordField<unsigned int>(record_ ? &record_->id : nullptr);
    storage_id = RecordField<unsigned char, unsigned int>(record_ ? &record_->storage_id : nullptr);
    dock_has_accepted_route_ids =
        RecordField<unsigned char, int>(record_ ? &record_->data.dock.has_accepted_route_ids : nullptr);
}

void Building::bind_graphics(BuildingGraphicsState *graphics_state)
{
    graphics_.bind(*this, type ? &type->graphics() : nullptr, graphics_state);
}

void Building::bind_rubble(const RubbleDef *rubble_definition, RubbleState *rubble_state)
{
    if (!rubble_definition || !rubble_definition->has_any() || !rubble_state) {
        rubble_.bind(nullptr, nullptr);
        Rubble = nullptr;
        return;
    }
    rubble_.bind(rubble_definition, rubble_state);
    Rubble = &rubble_;
}

Building::TypeRange::iterator::iterator(::building *record)
    : record_(record)
{}

Building &Building::TypeRange::iterator::operator*() const
{
    return building_runtime_impl::get_or_create_instance(record_)->building;
}

Building::TypeRange::iterator &Building::TypeRange::iterator::operator++()
{
    record_ = record_ ? record_->next_of_type : nullptr;
    return *this;
}

bool Building::TypeRange::iterator::operator!=(const iterator &other) const
{
    return record_ != other.record_;
}

Building::TypeRange::TypeRange(building_type type)
    : type_(type)
{}

Building::TypeRange::iterator Building::TypeRange::begin() const
{
    return iterator(data.first_of_type[type_]);
}

Building::TypeRange::iterator Building::TypeRange::end() const
{
    return iterator(nullptr);
}

Building::TypeRange Building::of_type(building_type type)
{
    return TypeRange(type);
}

Building *Building::first_of_type(building_type type)
{
    building_runtime *runtime = building_runtime_impl::get_or_create_instance(data.first_of_type[type]);
    return runtime ? &runtime->building : nullptr;
}

Building *Building::get(unsigned int id)
{
    if (!id) {
        return nullptr;
    }
    if (building_runtime *runtime = building_runtime_impl::get_ephemeral_instance(id)) {
        return &runtime->building;
    }
    if (static_cast<size_t>(id) >= building_runtime_impl::g_runtime_instances.size()) {
        return nullptr;
    }
    building_runtime *runtime = building_runtime_impl::g_runtime_instances[id].get();
    return runtime && runtime->building.id ? &runtime->building : nullptr;
}

void Building::for_each(const std::function<void(Building *)> &visitor)
{
    building_runtime_for_each(visitor);
}

void Building::for_each(BuildingRuntimeList list, const std::function<void(Building *)> &visitor)
{
    building_runtime_for_each(list, visitor);
}

int Building::count()
{
    return building_count();
}

const ::building *Building::record() const
{
    return record_;
}

Building &Building::dynamic_bridge_owner() const
{
    if (building_runtime *runtime = building_runtime_impl::get_ephemeral_main_instance(record_)) {
        return runtime->building;
    }
    if (!record_->id) {
        std::terminate();
    }
    // Record chains are retained only for dynamic bridges.
    building *part_record = building_slot(record_->id);
    for (int guard = 0; guard < 64; ++guard) {
        if (!part_record || part_record->prev_part_building_id <= 0) {
            building_runtime *runtime = building_runtime_impl::get_or_create_instance(part_record);
            if (runtime) {
                return runtime->building;
            }
            std::terminate();
        }
        part_record = building_slot(part_record->prev_part_building_id);
    }
    if (building_runtime *runtime = building_runtime_impl::get_or_create_instance(first_building_slot())) {
        return runtime->building;
    }
    std::terminate();
}

[[noreturn]] static void report_missing_building_graphics_definition(
    const Building &building,
    const std::source_location &location)
{
    const ::building *record = building.record();
    char detail[1200];
    std::snprintf(
        detail,
        sizeof(detail),
        "caller=%s:%u function=%s record=%p id=%u state=%d runtime_type=%d definition=%s "
        "x=%d y=%d grid_offset=%d surface_terrain=%d",
        safe_text(location.file_name()),
        static_cast<unsigned>(location.line()),
        safe_text(location.function_name()),
        static_cast<const void *>(record),
        record ? record->id : 0,
        record ? record->state : BUILDING_STATE_UNUSED,
        record ? static_cast<int>(record->type) : static_cast<int>(BUILDING_NONE),
        building.type ? safe_text(building.type->attr()) : "<none>",
        record ? record->x : 0,
        record ? record->y : 0,
        record ? record->grid_offset : 0,
        building.is_surface_terrain_tile());
    error_context_report_fatal_error_dialog(
        "Building graphics invariant violated",
        "A world building has no native graphics definition.",
        detail);
    std::terminate();
}

Building *Building::dynamic_bridge_next() const
{
    if (building_runtime *runtime = building_runtime_impl::get_ephemeral_next_instance(record_)) {
        return &runtime->building;
    }
    // Outside ephemeral previews this is the dynamic-bridge segment iterator.
    const int next_id = record_->next_part_building_id;
    building_runtime *runtime = building_runtime_impl::get_or_create_instance(
        next_id > 0 ? building_slot(next_id) : nullptr);
    return runtime ? &runtime->building : nullptr;
}

Building *Building::next_of_type() const
{
    building_runtime *runtime = building_runtime_impl::get_or_create_instance(record_->next_of_type);
    return runtime ? &runtime->building : nullptr;
}

int Building::matches(const char *text_id) const
{
    return type && text_id && type->attr_is(text_id);
}

int Building::grid_offset() const
{
    return record_->grid_offset;
}

int Building::x() const
{
    return record_->x;
}

int Building::y() const
{
    return record_->y;
}

int Building::is_dynamic_bridge_owner() const
{
    return type && type->bridge().is_bridge() && record_->prev_part_building_id == 0;
}

int Building::is_dynamic_bridge_segment() const
{
    return type && type->bridge().is_bridge() && record_->prev_part_building_id != 0;
}

int Building::has_dynamic_bridge_predecessor() const
{
    return type && type->bridge().is_bridge() && record_->prev_part_building_id > 0;
}

int Building::has_dynamic_bridge_next() const
{
    return type && type->bridge().is_bridge() && record_->next_part_building_id != 0;
}

int Building::road_network_id() const
{
    return record_->road_network_id;
}

int Building::distance_from_entry() const
{
    return record_->distance_from_entry;
}

void Building::set_distance_from_entry(int value)
{
    record_->distance_from_entry = static_cast<short>(value);
}

int Building::road_access_x() const
{
    return record_->road_access_x;
}

int Building::road_access_y() const
{
    return record_->road_access_y;
}

int Building::state_id() const
{
    return record_->state;
}

int Building::formation_id() const
{
    return record_->formation_id;
}

void Building::set_formation_id(int formation_id)
{
    record_->formation_id = static_cast<short>(formation_id);
}

int Building::is_deleted() const
{
    return record_->is_deleted;
}

int Building::is_in_use() const
{
    return record_->state == BUILDING_STATE_IN_USE;
}

int Building::is_mothballed() const
{
    return record_->state == BUILDING_STATE_MOTHBALLED;
}

int Building::is_fire_proof() const
{
    return record_->fire_proof;
}

Building::MaintenanceRiskOutcome Building::apply_maintenance_risk(const MaintenanceRiskTick &tick)
{
    if ((Composition && Composition->is_child()) || is_surface_terrain_tile()) {
        // Risk belongs to the fixed-composition owner and not to terrain-backed
        // surface objects. Keep compatibility fields inert behind this message.
        record_->fire_risk = 0;
        record_->damage_risk = 0;
        return MaintenanceRiskOutcome::None;
    }
    if (record_->state != BUILDING_STATE_IN_USE || record_->fire_proof ||
        record_->state == BUILDING_STATE_RUBBLE) {
        return MaintenanceRiskOutcome::None;
    }

    const int random_building = (record_->id + map_random_get(record_->grid_offset)) & 7;
    const auto *profile = Housing ? Housing->definition().profile : nullptr;
    const int house_level = profile ? profile->compatibility_level : -1;

    record_->damage_risk = static_cast<short>(
        record_->damage_risk +
        (random_building == tick.random_selector ? 3 : 1) +
        tick.damage_risk_bonus);
    if (Housing && house_level >= HOUSE_MIN && house_level <= HOUSE_LARGE_TENT) {
        record_->damage_risk = 0;
    }
    if (record_->damage_risk > 200) {
        return MaintenanceRiskOutcome::Collapse;
    }

    if (random_building == tick.random_selector) {
        int fire_increase = 0;
        if (!Housing) {
            fire_increase = 5;
        } else if (Housing->state().population <= 0) {
            fire_increase = 0;
        } else if (house_level >= HOUSE_MIN && house_level <= HOUSE_LARGE_SHACK) {
            fire_increase = 10;
        } else if (house_level >= HOUSE_MIN && house_level <= HOUSE_GRAND_INSULA) {
            fire_increase = 5;
        } else {
            fire_increase = 2;
        }
        fire_increase += tick.fire_risk_bonus;
        if (tick.suppress_fire) {
            fire_increase = 0;
        }
        record_->fire_risk = static_cast<short>(record_->fire_risk + fire_increase);
    }
    if (record_->fire_risk > 100) {
        return MaintenanceRiskOutcome::Fire;
    }
    return MaintenanceRiskOutcome::None;
}

int Building::has_plague() const
{
    return record_->has_plague;
}

void Building::advance_plague_day()
{
    building_plague_advance_day(*record_);
}

void Building::apply_plague_treatment()
{
    building_plague_apply_treatment(*record_);
}

int Building::has_cached_road_access() const
{
    return record_->has_road_access;
}

int Building::cached_road_access_point(map_point *road) const
{
    Building *owner_object = type && type->bridge().is_bridge() ?
        &dynamic_bridge_owner() :
        (Composition ? Composition->owner() : const_cast<Building *>(this));
    const building *owner = owner_object ? owner_object->record() : nullptr;
    if (!owner || !owner->has_road_access) {
        return 0;
    }
    if (road) {
        map_point_store_result(owner->road_access_x, owner->road_access_y, road);
    }
    return 1;
}

int Building::access_area_touches_same_road_network(
    const map_point &source_road,
    int radius,
    bool allow_highways) const
{
    if (radius <= 0) {
        return 0;
    }

    const int source_grid_offset = map_grid_offset(source_road.x, source_road.y);
    if (!map_grid_is_valid_offset(source_grid_offset)) {
        return 0;
    }

    const int source_network_id =
        figure_type_registry_impl::PathingMode::citizenRoadNetworkAt(source_grid_offset, allow_highways);
    if (source_network_id <= 0) {
        return 0;
    }

    const building_type_registry_impl::BuildingGeometry geometry =
        building_type_registry_impl::BuildingGeometry::query(*this);
    if (!geometry.valid()) {
        return 0;
    }
    const building_type_registry_impl::BuildingGeometryBounds &bounds = geometry.bounds();
    const int x_min = bounds.min_x - radius;
    const int y_min = bounds.min_y - radius;
    const int x_max = bounds.max_x + radius - 1;
    const int y_max = bounds.max_y + radius - 1;
    return figure_type_registry_impl::PathingMode::citizenAreaTouchesRoadNetwork(
        x_min,
        y_min,
        x_max,
        y_max,
        source_network_id,
        allow_highways);
}

building_runtime *Building::runtime_instance() const
{
    if (building_runtime *runtime = building_runtime_impl::get_ephemeral_instance(record_)) {
        return runtime;
    }
    return record_ && record_->id ? building_runtime_impl::get_or_create_instance(record_) : nullptr;
}

BuildingGraphics &Building::Graphics(const std::source_location &location) const
{
    BuildingGraphicsState *graphics_state = graphics_.state();
    if (!graphics_state) {
        report_missing_building_graphics_state(record_, type, construction_location_, location);
    }
    graphics_.bind(const_cast<Building &>(*this), type ? &type->graphics() : nullptr, graphics_state);
    return graphics_;
}

building_type_registry_impl::BuildingAnimation Building::animate()
{
    return building_type_registry_impl::BuildingAnimation(*this);
}

int Building::draw_footprint(const BuildingDrawContext &ctx)
{
    if (!building_record_requires_type_definition(record_)) {
        return 0;
    }
    if (!type || (!type->has_graphic() && !is_surface_terrain_tile())) {
        report_missing_building_graphics_definition(*this, std::source_location::current());
    }
    return type->has_graphic() ? Graphics().draw_footprint(ctx) : 0;
}

int Building::draw_top(const BuildingDrawContext &ctx)
{
    if (!building_record_requires_type_definition(record_)) {
        return 0;
    }
    if (!type || (!type->has_graphic() && !is_surface_terrain_tile())) {
        report_missing_building_graphics_definition(*this, std::source_location::current());
    }
    return type->has_graphic() ? Graphics().draw_top(ctx) : 0;
}

int Building::draw_animation(const BuildingDrawContext &ctx)
{
    if (!building_record_requires_type_definition(record_)) {
        return 0;
    }
    if (!type || (!type->has_graphic() && !is_surface_terrain_tile())) {
        report_missing_building_graphics_definition(*this, std::source_location::current());
    }
    return type->has_graphic() ? Graphics().draw_animation(ctx) : 0;
}

int Building::mothball_status_icon_offset(int icon_width, int icon_height, int *x, int *y) const
{
    return building_record_requires_type_definition(record_) && type ?
        Graphics().mothball_status_icon_offset(icon_width, icon_height, x, y) :
        0;
}

void Building::refresh_graphic()
{
    if (!building_record_requires_type_definition(record_)) {
        return;
    }
    if (!type || (!type->has_graphic() && !is_surface_terrain_tile())) {
        report_missing_building_graphics_definition(*this, std::source_location::current());
    }
    if (type->has_graphic()) {
        building_runtime_impl::get_or_create_instance(record_)->set_building_graphic();
    }
}

void Building::spawn_figure()
{
    Building *owner = Composition ? Composition->owner() : this;
    if (owner && owner->record_) {
        if (building_runtime *instance = building_runtime_impl::get_or_create_instance(owner->record_)) {
            instance->spawn_figure();
        }
    }
}

int Building::worker_count() const
{
    return record_ ? record_->num_workers : 0;
}

int Building::employment_worker_count() const
{
    if (!record_) {
        return 0;
    }

    if (!Composition) {
        return record_->num_workers;
    }
    int workers = 0;
    int member_count = 0;
    Composition->for_each_member([&](Building &part) {
        if (!part.record_) {
            return;
        }
        workers += part.record_->num_workers;
        member_count++;
    });
    return member_count ? workers : record_->num_workers;
}

int Building::employment_required_workers() const
{
    if (!record_ || !type) {
        return 0;
    }

    if (!Composition) {
        return type->required_workers();
    }
    int workers = 0;
    int member_count = 0;
    Composition->for_each_member([&](Building &part) {
        if (!part.type) {
            return;
        }
        workers += part.type->required_workers();
        member_count++;
    });
    return member_count ? workers : type->required_workers();
}

float Building::labor_access_score() const
{
    return record_ ? record_->labor_access_score : 0.0f;
}

int Building::has_required_workers() const
{
    if (!record_) {
        return 0;
    }
    return type && employment_worker_count() >= employment_required_workers();
}

int Building::has_road_access(map_point *road) const
{
    if (!record_) {
        return 0;
    }
    Building *owner = type && type->bridge().is_bridge() ?
        &dynamic_bridge_owner() :
        (Composition ? Composition->owner() : const_cast<Building *>(this));
    const building *owner_record = owner ? owner->record() : nullptr;
    if (!owner_record) {
        return 0;
    }
    return map_has_road_access_building(owner_record->x, owner_record->y, road);
}

int Building::query_road_access_point(map_point *road) const
{
    if (!record_) {
        return 0;
    }
    Building *owner = type && type->bridge().is_bridge() ?
        &dynamic_bridge_owner() :
        (Composition ? Composition->owner() : const_cast<Building *>(this));
    const building *owner_record = owner ? owner->record() : nullptr;
    if (!owner_record) {
        return 0;
    }
    return map_has_road_access_building(owner_record->x, owner_record->y, road);
}

int Building::storage_destination_road_access_point(map_point *road) const
{
    Building *owner = Composition ? Composition->owner() : const_cast<Building *>(this);
    if (!owner || !owner->type ||
        (!owner->type->is_storage() &&
            !owner->type->is_temple(GOD_VENUS, building_type_registry_impl::ReligionTier::Grand))) {
        return 0;
    }
    return owner->query_road_access_point(road);
}

int Building::has_water_access() const
{
    return record_ && record_->has_water_access;
}

int Building::is_working() const
{
    if (!record_) {
        return 0;
    }

    const building_type_registry_impl::BuildingType *type_definition = type;
    if (!type_definition) {
        return worker_count() > 0;
    }
    if (type_definition->required_workers() > 0 && !worker_count()) {
        return 0;
    }
    if (type_definition->water_access().has_requirements() && !has_water_access()) {
        return 0;
    }
    return 1;
}

int Building::has_primary_figure() const
{
    return record_ && record_->figure_id > 0;
}

int Building::has_secondary_figure() const
{
    return record_ && record_->figure_id2 > 0;
}

int Building::has_quaternary_figure() const
{
    return record_ && record_->figure_id4 > 0;
}

int Building::clear_figure_slot_if_matches(unsigned int figure_id)
{
    if (!record_ || !figure_id) {
        return 0;
    }
    int cleared = 0;
    if (record_->figure_id == figure_id) {
        record_->figure_id = 0;
        cleared = 1;
    }
    if (record_->figure_id2 == figure_id) {
        record_->figure_id2 = 0;
        cleared = 1;
    }
    if (record_->figure_id4 == figure_id) {
        record_->figure_id4 = 0;
        cleared = 1;
    }
    return cleared;
}

int Building::clear_distribution_cartpusher_slot_if_matches(unsigned int figure_id)
{
    if (!record_ || !figure_id) {
        return 0;
    }
    int cleared = 0;
    for (int i = 0; i < 3; i++) {
        if (record_->data.distribution.cartpusher_ids[i] == figure_id) {
            record_->data.distribution.cartpusher_ids[i] = 0;
            cleared = 1;
        }
    }
    return cleared;
}

unsigned int Building::distribution_cartpusher_id(int index) const
{
    if (!record_ || index < 0 || index >= 3) {
        return 0;
    }
    return record_->data.distribution.cartpusher_ids[index];
}

void Building::set_figure_spawn_delay(int ticks)
{
    if (record_) {
        record_->figure_spawn_delay = static_cast<unsigned char>(ticks);
    }
}

int Building::resource_amount(resource_type resource) const
{
    return record_ && resource >= RESOURCE_NONE && resource < RESOURCE_SLOT_COUNT ? record_->resources[resource] : 0;
}

void Building::add_resource(resource_type resource, int amount)
{
    if (record_ && amount != 0 && resource >= RESOURCE_NONE && resource < RESOURCE_SLOT_COUNT) {
        record_->resources[resource] = static_cast<short>(record_->resources[resource] + amount);
        invalidate_graphic();
    }
}

void Building::set_resource_amount(resource_type resource, int amount)
{
    if (record_ && resource >= RESOURCE_NONE && resource < RESOURCE_SLOT_COUNT &&
        record_->resources[resource] != amount) {
        record_->resources[resource] = static_cast<short>(amount);
        invalidate_graphic();
    }
}

int Building::add_storage_resource(
    resource_type resource, int amount, building_type_registry_impl::StorageRole role)
{
    if (!record_ || resource == RESOURCE_NONE || resource < RESOURCE_NONE || resource >= RESOURCE_SLOT_COUNT) {
        return 0;
    }

    const size_t slot_count = storage_runtime_impl::get_slot_count(*this);
    for (size_t i = 0; i < slot_count; i++) {
        BuildingStorage *storage = storage_runtime_impl::get_or_create(*this, i);
        if (storage && storage->type() && storage->type()->role() == role && storage->handles_resource(resource)) {
            return storage->add(resource, amount);
        }
    }
    return 0;
}

int Building::storage_resource_amount(resource_type resource, building_type_registry_impl::StorageRole role) const
{
    if (!record_ || resource == RESOURCE_NONE || resource < RESOURCE_NONE || resource >= RESOURCE_SLOT_COUNT) {
        return 0;
    }

    int total = 0;
    const size_t slot_count = storage_runtime_impl::get_slot_count(*this);
    for (size_t i = 0; i < slot_count; i++) {
        const BuildingStorage *storage = storage_runtime_impl::get_or_create(*this, i);
        if (storage && storage->type() && storage->type()->role() == role &&
            storage->handles_resource(resource)) {
            total += storage->amount(resource);
        }
    }
    return total;
}

int Building::input_storage_available_space(resource_type resource) const
{
    if (!record_ || resource == RESOURCE_NONE || resource < RESOURCE_NONE || resource >= RESOURCE_SLOT_COUNT) {
        return 0;
    }

    int space = 0;
    const size_t slot_count = storage_runtime_impl::get_slot_count(*this);
    for (size_t i = 0; i < slot_count; i++) {
        const BuildingStorage *storage = storage_runtime_impl::get_or_create(*this, i);
        if (storage && storage->type() && storage->type()->is_input()) {
            space += storage->available_space(resource);
        }
    }
    return space;
}

int Building::reserve_input_storage_load(resource_type resource, Figure &figure)
{
    if (!record_ || resource == RESOURCE_NONE || resource < RESOURCE_NONE || resource >= RESOURCE_SLOT_COUNT) {
        return 0;
    }

    const size_t slot_count = storage_runtime_impl::get_slot_count(*this);
    for (size_t i = 0; i < slot_count; i++) {
        BuildingStorage *storage = storage_runtime_impl::get_or_create(*this, i);
        if (storage && storage->reserve_inbound_load(resource, figure)) {
            return 1;
        }
    }
    return 0;
}

void Building::release_input_storage_reservation(const Figure &figure)
{
    if (!record_ || !figure.id()) {
        return;
    }

    const size_t slot_count = storage_runtime_impl::get_slot_count(*this);
    for (size_t i = 0; i < slot_count; i++) {
        if (BuildingStorage *storage = storage_runtime_impl::get_or_create(*this, i)) {
            storage->release_inbound(figure);
        }
    }
}

int Building::receive_input_storage_loads(resource_type resource, int loads, Figure &figure)
{
    if (!record_ || resource == RESOURCE_NONE || resource < RESOURCE_NONE || resource >= RESOURCE_SLOT_COUNT) {
        return 0;
    }

    const size_t slot_count = storage_runtime_impl::get_slot_count(*this);
    for (size_t i = 0; i < slot_count; i++) {
        BuildingStorage *storage = storage_runtime_impl::get_or_create(*this, i);
        if (storage && storage->handles_resource(resource)) {
            const int received = storage->receive_inbound_loads(resource, loads, figure);
            if (received > 0) {
                return received;
            }
        }
    }
    return 0;
}

int Building::reserved_legacy_storage_loads(resource_type resource, unsigned int ignore_figure_id) const
{
    if (!record_ || resource < RESOURCE_NONE || resource >= RESOURCE_SLOT_COUNT) {
        return 0;
    }
    building_runtime *runtime = runtime_instance();
    return runtime ? runtime->reserved_legacy_storage_loads(resource, ignore_figure_id) : 0;
}

int Building::reserve_legacy_storage_loads(resource_type resource, int loads, Figure &figure)
{
    if (!record_ || resource == RESOURCE_NONE || resource < RESOURCE_NONE || resource >= RESOURCE_SLOT_COUNT ||
        loads <= 0 || !figure.id()) {
        return 0;
    }
    building_runtime *runtime = runtime_instance();
    return runtime ? runtime->reserve_legacy_storage_loads(resource, loads, figure) : 0;
}

void Building::release_legacy_storage_reservation(const Figure &figure)
{
    if (!record_ || !figure.id()) {
        return;
    }
    if (building_runtime *runtime = runtime_instance()) {
        runtime->release_legacy_storage_reservation(figure);
    }
}

void Building::set_fetch_inventory_id(resource_type resource)
{
    if (record_) {
        record_->data.market.fetch_inventory_id = static_cast<unsigned char>(resource);
    }
}

bool Building::accepts_good(resource_type resource) const
{
    return record_ && resource >= RESOURCE_NONE && resource < RESOURCE_SLOT_COUNT && record_->accepted_goods[resource];
}

void Building::set_accepted_good(resource_type resource, bool accepted)
{
    if (record_ && resource >= RESOURCE_NONE && resource < RESOURCE_SLOT_COUNT) {
        record_->accepted_goods[resource] = accepted;
        if (!accepted) {
            set_distribution_demand(resource, 0);
        }
    }
}

void Building::invalidate_graphic()
{
    if (record_) {
        if (building_runtime *instance = building_runtime_impl::get_or_create_instance(record_)) {
            instance->invalidate_graphics_cache();
        }
    }
}

void Building::release_storage_reservations()
{
    if (!record_) {
        return;
    }
    storage_runtime_impl::release_all_reservations(*this);
    if (building_runtime *runtime = runtime_instance()) {
        runtime->release_all_legacy_storage_reservations();
    }
}

formation *Building::formation_object() const
{
    if (Formation) {
        return Formation;
    }
    if (!Composition || !Composition->is_child()) {
        return nullptr;
    }
    Building *owner = Composition->owner();
    return owner && owner != this ? owner->Formation : nullptr;
}

void Building::toggle_accepted_good(resource_type resource)
{
    set_accepted_good(resource, !accepts_good(resource));
}

unsigned char Building::distribution_demand(resource_type resource) const
{
    return building_distribution_demand(record_, resource);
}

void Building::set_distribution_demand(resource_type resource, unsigned char demand)
{
    building_set_distribution_demand(record_, resource, demand);
}

unsigned char building_distribution_demand(const building *b, resource_type resource)
{
    if (!b) {
        return 0;
    }
    if (resource == resource_pottery()) {
        return b->data.market.pottery_demand;
    }
    if (resource == resource_furniture()) {
        return b->data.market.furniture_demand;
    }
    if (resource == resource_oil()) {
        return b->data.market.oil_demand;
    }
    return resource == resource_wine() ? b->data.market.wine_demand : 0;
}

void building_set_distribution_demand(building *b, resource_type resource, unsigned char demand)
{
    if (!b) {
        return;
    }
    if (resource == resource_pottery()) {
        b->data.market.pottery_demand = demand;
    } else if (resource == resource_furniture()) {
        b->data.market.furniture_demand = demand;
    } else if (resource == resource_oil()) {
        b->data.market.oil_demand = demand;
    } else if (resource == resource_wine()) {
        b->data.market.wine_demand = demand;
    }
}

unsigned char building_accepted_good_save_value(const building *b, resource_type resource)
{
    if (!b || resource < RESOURCE_NONE || resource >= RESOURCE_SLOT_COUNT || !b->accepted_goods[resource]) {
        return 0;
    }
    return static_cast<unsigned char>(1 + std::min<int>(building_distribution_demand(b, resource), 254));
}

void building_load_accepted_good(building *b, resource_type resource, unsigned char value)
{
    if (!b || resource < RESOURCE_NONE || resource >= RESOURCE_SLOT_COUNT) {
        return;
    }
    b->accepted_goods[resource] = value != 0;
    building_set_distribution_demand(b, resource, value ? value - 1 : 0);
}

void Building::copy_accepted_goods(unsigned char *dst, int count) const
{
    if (!dst) {
        return;
    }
    if (count > RESOURCE_SLOT_COUNT) {
        count = RESOURCE_SLOT_COUNT;
    }
    if (!record_) {
        memset(dst, 0, count);
        return;
    }
    memcpy(dst, record_->accepted_goods, count);
}

void Building::set_accepted_goods(const unsigned char *src, int count)
{
    if (!record_ || !src) {
        return;
    }
    if (count > RESOURCE_SLOT_COUNT) {
        count = RESOURCE_SLOT_COUNT;
    }
    memcpy(record_->accepted_goods, src, count);
}

void Building::set_primary_figure_id(unsigned int figure_id)
{
    if (record_) {
        record_->figure_id = figure_id;
    }
}

void Building::set_quaternary_figure_id(unsigned int figure_id)
{
    if (record_) {
        record_->figure_id4 = figure_id;
    }
}

int Building::max_distance_to(int x, int y) const
{
    return record_ ? calc_maximum_distance(record_->x, record_->y, x, y) : 0;
}

int Building::max_distance_to(const Building &other) const
{
    return max_distance_to(other.x(), other.y());
}

int Building::orientation() const
{
    return record_ ? record_->subtype.orientation : 0;
}

void Building::set_orientation(int orientation)
{
    if (record_) {
        record_->subtype.orientation = static_cast<short>(orientation);
    }
}

int Building::is_surface_terrain_tile() const
{
    if (!type) {
        return 0;
    }

    const building_type_registry_impl::TileKind tile_kind = type->tile().kind();
    return tile_kind == building_type_registry_impl::TileKind::Garden ||
        tile_kind == building_type_registry_impl::TileKind::Plaza ||
        type->tool().is_road() ||
        type->tool().is_highway() ||
        type->tool().is_aqueduct();
}

void Building::add_map_tiles()
{
    if (record_ && Foundation) {
        const int rotation = Foundation->definition().rotates() ? orientation() : 0;
        if (!Foundation->publish(record_->x, record_->y, rotation)) {
            return;
        }
        Building *owner = type && type->bridge().is_bridge() ?
            &dynamic_bridge_owner() :
            (Composition ? Composition->owner() : this);
        if (!owner) {
            return;
        }
        const building_type_registry_impl::BuildingGeometry geometry =
            building_type_registry_impl::BuildingGeometry::query(*owner);
        if (geometry.valid()) {
            const int close_to_water = geometry_has_water_within_range(geometry, WATER_DESIRABILITY_RANGE);
            if (owner->Composition && owner->Composition->is_owner()) {
                owner->Composition->for_each_member([close_to_water](Building &member) {
                    if (building *member_record = const_cast<building *>(member.record())) {
                        member_record->is_close_to_water = static_cast<unsigned char>(close_to_water);
                    }
                });
            } else {
                record_->is_close_to_water = static_cast<unsigned char>(close_to_water);
            }
        }
    }
}

void Building::remove_map_tiles()
{
    if (Foundation) {
        Foundation->remove();
    }
}

void Building::set_storage_id(int new_storage_id)
{
    storage_id = static_cast<unsigned int>(new_storage_id);
    if (record_) {
        record_->storage_id = static_cast<unsigned char>(new_storage_id);
    }
}

int Building::blocked_storage_permission_mask() const
{
    const building_storage *storage = record_ ? building_storage_get(record_->storage_id) : nullptr;
    return storage ? (~storage->permissions) & 0x7 : 0;
}

int Building::warehouse_flag_frame() const
{
    return record_ ? record_->data.warehouse.flag_frame : 0;
}

resource_type Building::warehouse_resource_id() const
{
    return record_ ? static_cast<resource_type>(record_->subtype.warehouse_resource_id) : RESOURCE_NONE;
}

void Building::set_warehouse_resource_id(resource_type resource)
{
    if (record_) {
        record_->subtype.warehouse_resource_id = static_cast<short>(resource);
    }
}

int Building::loads_stored() const
{
    if (!record_) {
        return 0;
    }
    int amount = 0;
    for (int resource_index = (RESOURCE_NONE + 1); resource_index < RESOURCE_SLOT_COUNT; resource_index++) {
        amount += record_->resources[resource_index];
    }
    return amount;
}

int Building::industry_has_raw_materials() const
{
    return record_ ? record_->data.industry.has_raw_materials : 0;
}

int Building::dock_accepted_route_ids() const
{
    return record_ ? record_->data.dock.accepted_route_ids : 0;
}

int Building::dock_trade_ship_id() const
{
    return record_ ? record_->data.dock.trade_ship_id : 0;
}

void Building::set_dock_trade_ship_id(int figure_id)
{
    if (record_) {
        record_->data.dock.trade_ship_id = static_cast<short>(figure_id);
    }
}

int Building::dock_num_ships() const
{
    return record_ ? record_->data.dock.num_ships : 0;
}

void Building::set_dock_num_ships(int ticks)
{
    if (record_) {
        record_->data.dock.num_ships = static_cast<unsigned char>(ticks);
    }
}

void Building::decrement_dock_num_ships()
{
    if (record_ && record_->data.dock.num_ships > 0) {
        record_->data.dock.num_ships--;
    }
}

int Building::dock_queued_docker_id() const
{
    return record_ ? record_->data.dock.queued_docker_id : 0;
}

void Building::set_dock_queued_docker_id(int figure_id)
{
    if (record_) {
        record_->data.dock.queued_docker_id = static_cast<short>(figure_id);
    }
}

int Building::dock_orientation() const
{
    return record_ ? record_->data.dock.orientation : 0;
}

int Building::dock_idle_worker_count() const
{
    return record_ ? building_dock_count_idle_dockers(*this) : 0;
}

void Building::set_has_water_access(int value)
{
    const unsigned char access = static_cast<unsigned char>(value);
    if (record_ && record_->has_water_access != access) {
        record_->has_water_access = access;
        invalidate_graphic();
    }
}

void Building::set_dock_accepted_route_ids(int has_route_ids, int route_ids)
{
    dock_has_accepted_route_ids = has_route_ids;
    if (record_) {
        record_->data.dock.has_accepted_route_ids = static_cast<unsigned char>(has_route_ids);
        record_->data.dock.accepted_route_ids = route_ids;
    }
}

const order &Building::depot_order() const
{
    static const order empty_order = {};
    return record_ ? record_->data.depot.current_order : empty_order;
}

void Building::set_depot_order(const order &value)
{
    if (record_) {
        record_->data.depot.current_order = value;
    }
}

int Building::industry_is_stockpiling() const
{
    return record_ ? record_->data.industry.is_stockpiling : 0;
}

int Building::has_required_raw_amount_for_production(resource_type resource) const
{
    return record_ && resource >= RESOURCE_NONE && resource < RESOURCE_SLOT_COUNT ?
        storage_resource_amount(resource, building_type_registry_impl::StorageRole::Input) >=
            building_get_required_raw_amount_for_production(type, resource) :
        0;
}

int Building::has_native_production() const
{
    return production_runtime_impl::get_or_create_primary(*this) ? 1 : 0;
}

int Building::native_production_has_raw_materials() const
{
    Production *production = production_runtime_impl::get_or_create_primary(*this);
    return production ? production->has_raw_materials() : 0;
}

int Building::native_production_max_progress() const
{
    Production *production = production_runtime_impl::get_or_create_primary(*this);
    return production ? production->max_progress() : 0;
}

int Building::native_production_efficiency() const
{
    Production *production = production_runtime_impl::get_or_create_primary(*this);
    return production ? production->efficiency() : -1;
}

int Building::update_native_production(int new_day, int *out_is_striking)
{
    int updated = 0;
    const size_t method_count = production_runtime_impl::get_method_count(*this);
    for (size_t i = 0; i < method_count; i++) {
        Production *production = production_runtime_impl::get_or_create(*this, i);
        if (production && production->update_daily(i == 0 ? new_day : 0, i == 0 ? out_is_striking : nullptr)) {
            updated = 1;
        }
    }
    return updated;
}

int Building::native_production_has_completed_effect() const
{
    Production *production = production_runtime_impl::get_or_create_primary(*this);
    return production ? production->has_completed_effect() : 0;
}

int Building::output_cart_capacity(resource_type resource) const
{
    return output_cart_capacity_for_definition(type, resource);
}

int Building::industry_has_fish() const
{
    return record_ ? record_->data.industry.has_fish : 0;
}

void Building::add_industry_fish(int amount)
{
    if (record_) {
        record_->data.industry.has_fish = static_cast<unsigned char>(record_->data.industry.has_fish + amount);
    }
}

void Building::add_industry_production_current_month(int amount)
{
    if (record_) {
        record_->data.industry.production_current_month =
            static_cast<short>(record_->data.industry.production_current_month + amount);
    }
}

int Building::reserve_output_storage_loads(resource_type *out_resource, int *out_loads)
{
    if (!record_ || !type) {
        return 0;
    }

    const size_t slot_count = storage_runtime_impl::get_slot_count(*this);
    for (size_t i = 0; i < slot_count; i++) {
        BuildingStorage *storage = storage_runtime_impl::get_or_create(*this, i);
        if (!storage || !storage->type() || !storage->type()->is_output()) {
            continue;
        }
        for (resource_type resource : storage->type()->resources()) {
            const int capacity = output_cart_capacity(resource);
            const int loads = storage->remove_loads(resource, capacity);
            if (loads > 0) {
                if (out_resource) {
                    *out_resource = resource;
                }
                if (out_loads) {
                    *out_loads = loads;
                }
                return 1;
            }
        }
    }
    return 0;
}

int Building::start_native_production()
{
    Production *production = production_runtime_impl::get_or_create_primary(*this);
    if (!production) {
        return 0;
    }

    production->start_new_production();
    return 1;
}

void Building::advance_native_production_stats()
{
    const size_t method_count = production_runtime_impl::get_method_count(*this);
    for (size_t i = 0; i < method_count; i++) {
        if (Production *production = production_runtime_impl::get_or_create(*this, i)) {
            production->advance_stats();
        }
    }
}

void Building::bless_native_farm()
{
    if (Production *production = production_runtime_impl::get_or_create_primary(*this)) {
        production->bless_farm();
    }
}

void Building::curse_native_farm(int big_curse)
{
    if (Production *production = production_runtime_impl::get_or_create_primary(*this)) {
        production->curse_farm(big_curse);
    }
}

void Building::bless_native_industry()
{
    if (Production *production = production_runtime_impl::get_or_create_primary(*this)) {
        production->bless_industry();
    }
}

void Building::set_industry_stockpiling(int value)
{
    if (record_) {
        record_->data.industry.is_stockpiling = static_cast<unsigned char>(value);
    }
}

void Building::set_mothballed(int value)
{
    if (record_) {
        building_mothball_set(record_, value);
    }
}

void Building::change_type(building_type new_type, const std::source_location &location)
{
    if (record_) {
        if (!building_change_type(record_, new_type)) {
            return;
        }
        this->type = building_type_registry_impl::definition_for_type(record_->type);
        if (building_record_requires_type_definition(record_) && !this->type) {
            report_missing_building_type_definition(record_, location, "change_type()");
        }
    }
}

int Building::configure_house_replacement(building_type house_type, int x, int y)
{
    if (!record_) {
        return 0;
    }
    const building_type_registry_impl::BuildingType *definition =
        building_type_registry_impl::definition_for_type(house_type);
    const auto *profile = definition ? definition->housing_def().profile : nullptr;
    const int level = profile ? profile->compatibility_level : -1;
    if (level < 0) {
        return 0;
    }
    change_type(house_type);
    record_->state = BUILDING_STATE_IN_USE;
    record_->x = static_cast<unsigned char>(x);
    record_->y = static_cast<unsigned char>(y);
    record_->grid_offset = static_cast<short>(map_grid_offset(x, y));
    record_->is_close_to_water = static_cast<unsigned char>(building_is_close_to_water(record_));
    record_->local_workforce_assigned = 0;
    record_->local_workforce_unemployed = 0;
    record_->local_workforce_validation_delay = 0;
    return 1;
}

void Building::copy_house_figure_slot_from(const Building &source, unsigned int figure_id)
{
    if (!record_ || !source.record_ || !figure_id) {
        return;
    }
    if (source.record_->figure_id == figure_id && !record_->figure_id) {
        record_->figure_id = figure_id;
    } else if (source.record_->figure_id2 == figure_id && !record_->figure_id2) {
        record_->figure_id2 = figure_id;
    } else if (source.record_->figure_id4 == figure_id && !record_->figure_id4) {
        record_->figure_id4 = figure_id;
    }
}

void Building::copy_house_data_from(const Building &source)
{
    if (!record_ || !source.record_) {
        return;
    }
    if (Housing && source.Housing) {
        Housing->state() = source.Housing->state();
    }
    record_->distance_from_entry = source.record_->distance_from_entry;
    record_->road_network_id = source.record_->road_network_id;
    record_->road_access_x = source.record_->road_access_x;
    record_->road_access_y = source.record_->road_access_y;
    record_->has_road_access = source.record_->has_road_access;
    record_->has_water_access = source.record_->has_water_access;
    record_->has_well_access = source.record_->has_well_access;
    record_->has_latrines_access = source.record_->has_latrines_access;
    record_->desirability = source.record_->desirability;
    building_runtime *target_runtime = building_runtime_impl::get_or_create_instance(record_);
    building_runtime *source_runtime = building_runtime_impl::get_or_create_instance(source.record_);
    if (target_runtime && source_runtime) {
        target_runtime->set_graphics_variant(source_runtime->graphics_variant());
    }
}

void Building::retire_replaced_house()
{
    if (!record_) {
        return;
    }
    if (Housing) {
        Housing->state().population = 0;
        Housing->state().population_room = 0;
    }
    record_->local_workforce_assigned = 0;
    record_->local_workforce_unemployed = 0;
    record_->local_workforce_validation_delay = 0;
    record_->figure_id = 0;
    record_->figure_id2 = 0;
    if (Housing) {
        Housing->remove_immigrant();
    }
    record_->figure_id4 = 0;
    for (resource_type r = RESOURCE_NONE; r < RESOURCE_SLOT_COUNT; r = static_cast<resource_type>(r + 1)) {
        record_->resources[r] = 0;
    }
    record_->state = BUILDING_STATE_DELETED_BY_GAME;
}

void Building::cleanup_figure_references_for_removal()
{
    if (!record_ || !record_->id) {
        return;
    }

    if (Housing) {
        Housing->remove_immigrant();
    }

    // Snapshot before callbacks remove or retarget figures and mutate the ledger.
    const std::vector<unsigned int> figure_ids = Figure::ids_directly_referencing_building(*this);
    for (unsigned int figure_id : Figure::ids_referencing_building(*this)) {
        if (std::find(figure_ids.begin(), figure_ids.end(), figure_id) == figure_ids.end()) {
            if (Figure *figure = Figure::get(figure_id); figure && figure->id() == figure_id) {
                figure->clear_last_destination_building_if_matches(*this);
            }
        }
    }

    record_->figure_id = 0;
    record_->figure_id2 = 0;
    record_->figure_id4 = 0;
    for (unsigned int &cartpusher_id : record_->data.distribution.cartpusher_ids) {
        cartpusher_id = 0;
    }
    record_->data.industry.fishing_boat_id = 0;
    record_->data.industry.second_fishing_boat_id = 0;

    for (unsigned int figure_id : figure_ids) {
        Figure *figure = Figure::get(figure_id);
        if (!figure || figure->id() != figure_id || !figure->state) {
            continue;
        }
        if (figure->type == FIGURE_FISHING_BOAT) {
            FishingBoat::from(*figure).sink();
        } else {
            figure->remove();
        }
    }
}

int Building::is_being_fumigated() const
{
    return record_ && record_->sickness_doctor_cure == 99;
}

int Building::fumigation_frame() const
{
    return record_ ? record_->fumigation_frame : 0;
}

void Building::set_fumigation_direction(int direction)
{
    if (record_) {
        record_->fumigation_direction = static_cast<unsigned char>(direction);
    }
}

int Building::fort_figure_type() const
{
    if (!record_) {
        return 0;
    }
    if (record_->subtype.fort_figure_type) {
        return record_->subtype.fort_figure_type;
    }
    return type ? type->military().primary_figure_type() : 0;
}

int Building::is_unfinished_monument() const
{
    return record_ ? building_monument_is_unfinished_monument(record_) : 0;
}

int Building::monument_upgrade_level() const
{
    return record_ ? record_->monument.upgrades : 0;
}

int Building::monument_phase() const
{
    return record_ ? record_->monument.phase : 0;
}

int Building::monument_secondary_frame() const
{
    return record_ ? record_->monument.secondary_frame : 0;
}

int Building::entertainment_days1() const
{
    return record_ ? record_->data.entertainment.days1 : 0;
}

int Building::entertainment_days2() const
{
    return record_ ? record_->data.entertainment.days2 : 0;
}

int Building::desirability() const
{
    return record_ ? record_->desirability : 0;
}

int building_dist(int x, int y, int w, int h, building *b)
{
    const building_type_registry_impl::BuildingType *definition = definition_for_record(b);
    const building_type_registry_impl::FoundationDef *foundation =
        definition ? definition->foundation_def() : nullptr;
    if (!b || !foundation) {
        return 10000;
    }
    const int rotation = record_foundation_rotation(*b, *foundation);
    return calc_box_distance(
        x,
        y,
        w,
        h,
        b->x,
        b->y,
        foundation->rotated_width(rotation),
        foundation->rotated_height(rotation));
}

void building_get_from_buffer(buffer *buf, int id, building *b, int includes_building_size, int save_version,
    int buffer_offset)
{
    buffer_set(buf, 0);
    int building_buf_size = BUILDING_STATE_ORIGINAL_BUFFER_SIZE;
    int buf_skip = 0;

    if (includes_building_size) {
        building_buf_size = buffer_read_i32(buf);
        buf_skip = 4;
    }
    buf_skip += buffer_offset;
    buffer_set(buf, building_buf_size * id + buf_skip);
    building_state_load_from_buffer(buf, b, building_buf_size, save_version, 1);
}

int building_count(void)
{
    return static_cast<int>(data.buildings.size());
}

void building_for_each_loaded_record(const std::function<void(building *)> &visitor)
{
    if (!visitor) {
        return;
    }

    for (size_t id = 1; id < data.buildings.size(); id++) {
        building *record = &data.buildings[id];
        if (!record->id || record->state == BUILDING_STATE_UNUSED) {
            continue;
        }
        visitor(record);
    }
}

static int composed_record_is_live(const building *b)
{
    return b && b->id > 0 &&
        (b->state == BUILDING_STATE_IN_USE ||
            b->state == BUILDING_STATE_MOTHBALLED ||
            b->state == BUILDING_STATE_CREATED);
}

static int normalized_composed_rotation(int rotation)
{
    int result = rotation % 4;
    if (result < 0) {
        result += 4;
    }
    return result;
}

static int composed_rotation_fallback(const building *main_record,
    const building_type_registry_impl::BuildingType &definition)
{
    const bool owner_stores_orientation = definition.is_warehouse() || std::any_of(
        definition.composition().children().begin(),
        definition.composition().children().end(),
        [](const building_type_registry_impl::CompositionChildDef &child) {
            return child.orientation == building_type_registry_impl::CompositionChildOrientation::InheritOwner;
        });
    if (owner_stores_orientation) {
        return normalized_composed_rotation(main_record->subtype.orientation);
    }
    return 0;
}

static int infer_loaded_composed_rotation(building *main_record,
    const building_type_registry_impl::BuildingType &definition)
{
    if (!main_record || !definition.has_composition()) {
        return 0;
    }

    int best_rotation = composed_rotation_fallback(main_record, definition);
    int best_score = -1;

    for (int rotation = 0; rotation < 4; rotation++) {
        const building_type_registry_impl::CompositionLayoutResult layout =
            building_type_registry_impl::build_composition_layout(
                &definition, definition.composition(), main_record->x, main_record->y, rotation);
        if (!layout.valid()) {
            continue;
        }
        int score = 0;
        building *previous = main_record;

        for (size_t i = 1; i < layout.members.size(); ++i) {
            if (!previous || previous->next_part_building_id <= 0) {
                break;
            }
            building *child = building_slot(previous->next_part_building_id);
            if (!composed_record_is_live(child) || child->id == previous->id) {
                break;
            }
            const building_type_registry_impl::CompositionLayoutMember &expected = layout.members[i];
            if (child->x == expected.x && child->y == expected.y) {
                score += 4;
            }
            if (expected.type && child->type == expected.type->type()) {
                score += 2;
            }
            previous = child;
        }

        if (score > best_score) {
            best_score = score;
            best_rotation = rotation;
        }
    }

    return best_score > 0 ? best_rotation : composed_rotation_fallback(main_record, definition);
}

static void publish_loaded_composed_record(building *record)
{
    if (!record || !map_grid_is_inside(record->x, record->y, 1)) {
        return;
    }
    Building *building_object = runtime_building_for_record(record);
    if (!building_object) {
        return;
    }
    building_object->add_map_tiles();
    building_object->refresh_graphic();
}

static std::vector<building_type_registry_impl::LoadedCompositionCandidate>
loaded_native_composition_chain(const building *main_record)
{
    std::vector<building_type_registry_impl::LoadedCompositionCandidate> result;
    if (!main_record) {
        return result;
    }
    int next_id = main_record->next_part_building_id;
    for (int guard = 0; next_id > 0 && guard < 64; ++guard) {
        building *candidate = building_slot(next_id);
        if (!candidate || candidate->id != static_cast<unsigned int>(next_id)) {
            break;
        }
        result.push_back(building_type_registry_impl::LoadedCompositionCandidate{
            candidate->id,
            static_cast<unsigned int>(candidate->prev_part_building_id > 0 ? candidate->prev_part_building_id : 0),
            static_cast<unsigned int>(candidate->next_part_building_id > 0 ? candidate->next_part_building_id : 0),
            building_type_registry_impl::definition_for_type(candidate->type),
            candidate->x,
            candidate->y,
            composed_record_is_live(candidate) != 0
        });
        const bool repeated = std::any_of(result.begin(), result.end() - 1,
            [candidate](const building_type_registry_impl::LoadedCompositionCandidate &previous) {
                return previous.id == candidate->id;
            });
        if (repeated) {
            break;
        }
        next_id = candidate->next_part_building_id;
    }
    return result;
}

[[noreturn]] static void report_loaded_composition_failure(
    const building *owner,
    const building_type_registry_impl::BuildingType *definition,
    const char *reason)
{
    char detail[900];
    std::snprintf(detail, sizeof(detail),
        "owner_id=%u owner_type=%s x=%d y=%d saved_prev=%d saved_next=%d reason=%s",
        owner ? owner->id : 0,
        definition ? definition->attr() : "<none>",
        owner ? owner->x : 0,
        owner ? owner->y : 0,
        owner ? owner->prev_part_building_id : 0,
        owner ? owner->next_part_building_id : 0,
        reason ? reason : "<none>");
    log_error("Unable to establish a complete loaded BuildingComposition", detail, owner ? owner->id : 0);
    error_context_report_fatal_error_dialog(
        "Savegame building composition error",
        "A saved composed building could not be converted to the native runtime graph. The game has stopped rather than continue with orphaned buildings.",
        detail);
    std::terminate();
}

static void initialize_loaded_native_composed_child(
    building *main_record,
    building *child,
    const building_type_registry_impl::CompositionLayoutMember &expected,
    int inherits_owner_orientation,
    int was_created)
{
    if (!main_record || !child) {
        return;
    }
    child->faction_id = main_record->faction_id;
    child->road_network_id = main_record->road_network_id;
    child->distance_from_entry = main_record->distance_from_entry;
    child->road_access_x = main_record->road_access_x;
    child->road_access_y = main_record->road_access_y;
    child->has_road_access = main_record->has_road_access;
    child->houses_covered = main_record->houses_covered;
    child->percentage_houses_covered = main_record->percentage_houses_covered;
    child->labor_access_score = main_record->labor_access_score;
    // subtype is a compatibility union. Warehouse bays use this member for
    // their stored resource id, so canonical children must retain the value
    // loaded from the save. Only explicitly inheriting children own it as an
    // orientation field.
    if (inherits_owner_orientation) {
        child->subtype.orientation = static_cast<short>(expected.building_orientation);
    }

    BuildingGraphicsState main_graphics_state;
    if (!building_runtime_loaded_graphics_state(main_record->id, &main_graphics_state)) {
        if (building_runtime *main_runtime = building_runtime_impl::get_or_create_instance(main_record)) {
            main_graphics_state = main_runtime->graphics_state_snapshot();
        }
    }
    building_runtime_stage_loaded_graphics_state(child->id, main_graphics_state);

    if (building_is_fort(main_record->type)) {
        child->formation_id = main_record->formation_id;
    }
    if (was_created && expected.role.find("field") == 0) {
        child->data.industry.progress = main_record->data.industry.progress;
        child->data.industry.blessing_days_left = main_record->data.industry.blessing_days_left;
        child->data.industry.curse_days_left = main_record->data.industry.curse_days_left;
        child->data.industry.has_raw_materials = main_record->data.industry.has_raw_materials;
        child->data.industry.has_fish = main_record->data.industry.has_fish;
        child->data.industry.is_stockpiling = main_record->data.industry.is_stockpiling;
        child->data.industry.production_current_month =
            static_cast<short>(main_record->data.industry.production_current_month / 5);
    }
}

static int hydrate_loaded_native_composition(
    building *main_record,
    const building_type_registry_impl::BuildingType &definition)
{
    const int rotation = infer_loaded_composed_rotation(main_record, definition);
    const building_type_registry_impl::CompositionLayoutResult layout =
        building_type_registry_impl::build_composition_layout(
            &definition, definition.composition(), main_record->x, main_record->y, rotation);
    if (!layout.valid()) {
        log_error("Unable to build loaded native composition layout", layout.detail.c_str(), 0);
        return 0;
    }

    const std::vector<building_type_registry_impl::LoadedCompositionCandidate> saved_chain =
        loaded_native_composition_chain(main_record);
    const building_type_registry_impl::CompositionHydrationPlan hydration =
        building_type_registry_impl::plan_composition_hydration(layout, main_record->id, saved_chain);
    if (!hydration.valid()) {
        log_error("Unable to hydrate loaded native composition", hydration.detail.c_str(), 0);
        return 0;
    }

    struct LoadedChild {
        building_type_registry_impl::CompositionHydrationAction action;
        Building *object = nullptr;
        building *record = nullptr;
        int created = 0;
    };
    std::vector<LoadedChild> children;
    std::vector<BuildingComposition *> child_modules;
    children.reserve(hydration.actions.size());
    child_modules.reserve(hydration.actions.size());

    for (const building_type_registry_impl::CompositionHydrationAction &action : hydration.actions) {
        Building *child_object = nullptr;
        building *child_record = nullptr;
        int created = 0;
        if (action.kind == building_type_registry_impl::CompositionHydrationActionKind::AdoptExisting) {
            child_record = building_slot(action.existing_id);
            building_runtime *runtime = building_runtime_impl::get_or_create_instance(child_record);
            child_object = runtime ? &runtime->building : nullptr;
        } else if (action.expected.type) {
            child_object = &city_building_runtime().create(
                *action.expected.type, action.expected.x, action.expected.y);
            child_record = const_cast<building *>(child_object->record());
            created = 1;
        }
        if (!child_object || !child_record || !child_object->Composition) {
            if (created && child_record) {
                child_record->state = BUILDING_STATE_DELETED_BY_GAME;
            }
            for (const LoadedChild &pending : children) {
                if (pending.created && pending.record) {
                    pending.record->state = BUILDING_STATE_DELETED_BY_GAME;
                }
            }
            log_error("Unable to materialize loaded native composition child", definition.attr(), 0);
            return 0;
        }
        children.push_back(LoadedChild{ action, child_object, child_record, created });
        child_modules.push_back(child_object->Composition);
    }

    building_runtime *main_runtime = building_runtime_impl::get_or_create_instance(main_record);
    BuildingComposition *composition = main_runtime ? main_runtime->building.Composition : nullptr;
    std::string relationship_error;
    if (!composition || !composition->attach_children(child_modules, &relationship_error)) {
        for (const LoadedChild &pending : children) {
            if (pending.created && pending.record) {
                pending.record->state = BUILDING_STATE_DELETED_BY_GAME;
            }
        }
        log_error("Unable to bind loaded native composition", relationship_error.c_str(), 0);
        return 0;
    }

    const bool owner_stores_orientation = definition.is_warehouse() || std::any_of(
        definition.composition().children().begin(),
        definition.composition().children().end(),
        [](const building_type_registry_impl::CompositionChildDef &child) {
            return child.orientation == building_type_registry_impl::CompositionChildOrientation::InheritOwner;
        });
    if (owner_stores_orientation && !building_is_fort(main_record->type)) {
        main_record->subtype.orientation = static_cast<short>(rotation);
    }
    main_record->output_resource_id = static_cast<unsigned char>(building_output_resource(&definition));
    if (definition.is_warehouse() && !main_record->storage_id) {
        main_record->storage_id = static_cast<unsigned char>(building_storage_create(main_record->id));
    }

    std::vector<unsigned int> member_ids;
    member_ids.reserve(children.size() + 1);
    member_ids.push_back(main_record->id);
    for (LoadedChild &child : children) {
        const int needs_republish = !child.created &&
            (child.record->x != child.action.expected.x ||
                child.record->y != child.action.expected.y);
        if (needs_republish) {
            if (child.object->Foundation) {
                const int child_rotation = child.object->Foundation->definition().rotates()
                    ? child.object->orientation()
                    : 0;
                child.object->Foundation->rebind(
                    child.record->x, child.record->y, child_rotation);
            }
            child.object->remove_map_tiles();
        }
        child.record->x = static_cast<unsigned char>(child.action.expected.x);
        child.record->y = static_cast<unsigned char>(child.action.expected.y);
        child.record->grid_offset = static_cast<short>(map_grid_offset(child.record->x, child.record->y));
        child.record->output_resource_id = static_cast<unsigned char>(
            building_output_resource(child.action.expected.type));
        if (child.created || !composed_record_is_live(child.record)) {
            child.record->state = main_record->state;
        }
        member_ids.push_back(child.record->id);
        const std::vector<building_type_registry_impl::CompositionChildDef> &child_definitions =
            definition.composition().children();
        const int inherits_owner_orientation =
            child.action.expected.definition_index < child_definitions.size() &&
            child_definitions[child.action.expected.definition_index].orientation ==
                building_type_registry_impl::CompositionChildOrientation::InheritOwner;
        initialize_loaded_native_composed_child(
            main_record,
            child.record,
            child.action.expected,
            inherits_owner_orientation,
            child.created);
    }

    for (unsigned int unrelated_id : hydration.unrelated_tail_ids) {
        building *unrelated = building_slot(unrelated_id);
        if (!unrelated) {
            continue;
        }
        if (std::find(member_ids.begin(), member_ids.end(),
                static_cast<unsigned int>(unrelated->prev_part_building_id)) != member_ids.end()) {
            unrelated->prev_part_building_id = 0;
        }
    }

    publish_loaded_composed_record(main_record);
    for (const LoadedChild &child : children) {
        publish_loaded_composed_record(child.record);
    }
    if (definition.is_warehouse()) {
        building_warehouse_recount_resources(main_runtime->building);
    }
    return 1;
}

void building_hydrate_loaded_compositions(void)
{
    for (size_t i = 1; i < data.buildings.size(); i++) {
        building *record = &data.buildings[i];
        if (!composed_record_is_live(record)) {
            continue;
        }
        const building_type_registry_impl::BuildingType *definition =
            building_type_registry_impl::definition_for_type(record->type);
        if (definition && definition->has_composition() &&
            !hydrate_loaded_native_composition(record, *definition)) {
            report_loaded_composition_failure(record, definition, "load hydration failed");
        }
    }

    // The bridge ends here. From this point onward every composition owner and
    // every composition-only child must have native ownership; runtime systems
    // are not allowed to repair or reinterpret this state.
    for (size_t i = 1; i < data.buildings.size(); i++) {
        building *record = &data.buildings[i];
        if (!composed_record_is_live(record)) {
            continue;
        }
        const building_type_registry_impl::BuildingType *definition =
            building_type_registry_impl::definition_for_type(record->type);
        Building *object = runtime_building_for_record(record);
        if (definition && definition->has_composition()) {
            if (!object || !object->Composition || !object->Composition->is_owner()) {
                report_loaded_composition_failure(record, definition, "owner module is missing");
            }
            object->Composition->require_complete("save load composition bridge");
            continue;
        }

        bool is_declared_child_type = false;
        for (const std::unique_ptr<building_type_registry_impl::BuildingType> &owner_definition :
            building_type_registry_impl::g_building_types) {
            if (!owner_definition || !owner_definition->has_composition()) {
                continue;
            }
            for (const building_type_registry_impl::CompositionChildDef &child :
                owner_definition->composition().children()) {
                if (definition && child.type == definition) {
                    is_declared_child_type = true;
                    break;
                }
            }
            if (is_declared_child_type) {
                break;
            }
        }
        if (is_declared_child_type &&
            (!object || !object->Composition || !object->Composition->is_child())) {
            report_loaded_composition_failure(record, definition, "orphaned composition child record");
        }
    }
}

static void fill_adjacent_types(building *b)
{
    building *first = data.first_of_type[b->type];
    building *last = data.last_of_type[b->type];
    if (!first || !last) {
        b->prev_of_type = 0;
        b->next_of_type = 0;
        data.first_of_type[b->type] = b;
        data.last_of_type[b->type] = b;
    } else if (b->id < first->id) {
        first->prev_of_type = b;
        b->next_of_type = first;
        b->prev_of_type = 0;
        data.first_of_type[b->type] = b;
    } else if (b->id > last->id) {
        last->next_of_type = b;
        b->prev_of_type = last;
        b->next_of_type = 0;
        data.last_of_type[b->type] = b;
    } else if (b != first && b != last) {
        int id = b->id - 1;
        while (id) {
            building *prev = building_slot(id);
            if (prev->state != BUILDING_STATE_UNUSED && prev->type == b->type) {
                b->prev_of_type = prev;
                b->next_of_type = prev->next_of_type;
                b->next_of_type->prev_of_type = b;
                prev->next_of_type = b;
                break;
            }
            id--;
        }
    }
}

static void remove_adjacent_types(building *b)
{
    building *first = data.first_of_type[b->type];
    building *last = data.last_of_type[b->type];
    if (b == first && b == last) {
        data.first_of_type[b->type] = 0;
        data.last_of_type[b->type] = 0;
    } else if (b == first) {
        data.first_of_type[b->type] = b->next_of_type;
        if (b->next_of_type) {
            b->next_of_type->prev_of_type = 0;
        }
    } else if (b == last) {
        data.last_of_type[b->type] = b->prev_of_type;
        if (b->prev_of_type) {
            b->prev_of_type->next_of_type = 0;
        }
    } else {
        b->prev_of_type->next_of_type = b->next_of_type;
        b->next_of_type->prev_of_type = b->prev_of_type;
    }
    b->prev_of_type = 0;
    b->next_of_type = 0;
}

building *building_create(building_type type, int x, int y)
{
    building *b = new_building_slot();
    if (!b) {
        city_warning_show_translated(WARNING_DATA_LIMIT_REACHED);
        return first_building_slot();
    }
    const building_properties *props = building_properties_for_type(type);

    b->state = BUILDING_STATE_CREATED;
    b->faction_id = 1;
    b->type = type;
    b->created_sequence = static_cast<unsigned short>(extra.created_sequence++);

    fill_adjacent_types(b);
    city_culture_add_building_module_capacity(b);

    Building *building_obj = runtime_building_for_record(b);
    if (!building_obj) {
        return b;
    }

    b->output_resource_id = static_cast<unsigned char>(building_output_resource(building_obj->type));
    if (building_obj->Housing) {
        building_obj->Housing->state().happiness = 100;
    }

    if (building_obj->type && building_obj->type->is_granary()) {
        b->resources[RESOURCE_NONE] = FULL_GRANARY;
    }

    const building_type_registry_impl::BuildingType *definition = building_obj->type;
    const building_type_registry_impl::Distribution *distribution =
        definition ? definition->distribution() : nullptr;

    // Set it as accepting all goods defined by this building's distribution.
    for (resource_type r = (RESOURCE_NONE + 1); r < RESOURCE_SLOT_COUNT; r = static_cast<resource_type>(r + 1)) {
        b->accepted_goods[r] = distribution && distribution->handles_resource(r);
    }
    if (building_obj->matches("dock")) {
        for (resource_type r = (RESOURCE_NONE + 1); r < RESOURCE_SLOT_COUNT; r = static_cast<resource_type>(r + 1)) {
            b->accepted_goods[r] = 1;
        }
    }

    // Exception for Venus temples which should never accept wine by default to prevent unwanted evolutions
    if (building_obj->type && building_obj->type->is_temple(GOD_VENUS)) {
        b->accepted_goods[resource_wine()] = 0;
    }

    if (definition && definition->bridge().is_bridge()) {
        // Dynamic bridges still use the legacy record-backed permission state.
        b->data.roadblock.exceptions = ROADBLOCK_PERMISSION_ALL;
    }
    if (definition && definition->is_warehouse() && building_obj->Foundation &&
        config_get(CONFIG_GP_CH_WAREHOUSE_DEFAULT_TO_PASS_ALL_WALKERS)) {
        building_obj->Foundation->roadblock_state().accept_all();
    }

    if (type_attr_is(b->type, "market") && config_get(CONFIG_GP_CH_MARKETS_DONT_ACCEPT)) {
        if (const building_type_registry_impl::Distribution *market_distribution =
            building_obj->type ? building_obj->type->distribution() : nullptr) {
            market_distribution->set_acceptance(*building_obj, false);
        }
    } else if (type_attr_is(b->type, "market") && !config_get(CONFIG_GP_CH_MARKETS_DONT_ACCEPT)) {
        if (const building_type_registry_impl::Distribution *market_distribution =
            building_obj->type ? building_obj->type->distribution() : nullptr) {
            market_distribution->set_acceptance(*building_obj, true);
        }
    }

    b->x = static_cast<unsigned char>(x);
    b->y = static_cast<unsigned char>(y);
    b->grid_offset = static_cast<short>(map_grid_offset(x, y));
    const int housing_figure_delay = map_random_get(b->grid_offset) & 0x7f;
    if (building_obj->Housing) {
        building_obj->Housing->state().happiness = 100;
        building_obj->Housing->state().figure_generation_delay =
            static_cast<uint8_t>(housing_figure_delay);
    }
    b->figure_roam_direction = housing_figure_delay & 6;
    b->fire_proof = static_cast<unsigned char>(
        definition && definition->flags().has_fire_proof() ? definition->flags().fire_proof() : props->fire_proof);
    b->is_close_to_water = static_cast<unsigned char>(building_is_close_to_water(b));

    return b;
}

int building_change_type(building *b, building_type type)
{
    if (!b) {
        return 0;
    }
    if (b->type == type) {
        return 1;
    }
    building_runtime *runtime = building_runtime_impl::get_or_create_instance(b);
    const building_type_registry_impl::BuildingType *current_definition = runtime ?
        runtime->building.type : building_type_registry_impl::definition_for_type(b->type);
    const building_type_registry_impl::BuildingType *target_definition =
        building_type_registry_impl::definition_for_type(type);
    building_type_registry_impl::CompositionMembership membership =
        building_type_registry_impl::CompositionMembership::Standalone;
    if (runtime && runtime->building.Composition) {
        if (runtime->building.Composition->is_child()) {
            membership = building_type_registry_impl::CompositionMembership::Child;
        } else if (runtime->building.Composition->is_owner()) {
            membership = building_type_registry_impl::CompositionMembership::Owner;
        }
    }
    const building_type_registry_impl::CompositionTypeReplacementPlan replacement =
        building_type_registry_impl::plan_composition_type_replacement(
            current_definition, target_definition, membership);
    if (!replacement.accepted()) {
        log_error("Rejected unsafe building type replacement", replacement.detail.c_str(), b->id);
        return 0;
    }
    if (replacement.action ==
        building_type_registry_impl::CompositionTypeReplacementAction::NoChange) {
        return 1;
    }
    if (runtime) {
        runtime->building.release_storage_reservations();
        if (runtime->building.Housing && (!target_definition || !target_definition->has_housing())) {
            runtime->building.Housing->remove_immigrant();
        }
        building_local_workforce::change_building(runtime->building);
    }
    city_culture_remove_building_module_capacity(b);
    remove_adjacent_types(b);
    b->type = type;
    fill_adjacent_types(b);
    city_culture_add_building_module_capacity(b);
    if (runtime) {
        runtime->rebind_definition(target_definition);
        water_access_runtime_refresh_building(&runtime->building);
    }
    return 1;
}

static void building_delete(building *b)
{
    city_culture_remove_building_module_capacity(b);
    building_clear_related_data(b);
    Building *building_object = runtime_building_for_record(b);
    water_access_runtime_remove_building(building_object);
    if (building_object && building_object->Composition) {
        building_object->Composition->clear();
    }
    if (building_object) {
        building_runtime_unregister_from_indexes(*building_object);
    }
    remove_adjacent_types(b);
    int id = b->id;
    memset(b, 0, sizeof(building));
    b->id = id;

    trim_buildings();
}

static bool rubble_record_has_map_presence(const building *record)
{
    if (!record || !record->id || !map_grid_is_valid_offset(record->grid_offset) ||
        !map_terrain_is(record->grid_offset, TERRAIN_RUBBLE)) {
        return false;
    }
    return map_building_loaded_id_at(record->grid_offset) == record->id ||
        map_building_rubble_building_id(record->grid_offset) == record->id;
}

void building_clear_related_data(building *b)
{
    if (b) {
        if (Building *building_object = runtime_building_for_record(b)) {
            building_object->release_storage_reservations();
            building_object->cleanup_figure_references_for_removal();
        }
    }
    if (b->storage_id) {
        building_storage_delete(b->storage_id);
        b->storage_id = 0;
    }
    if (building_is_fort(b->type)) {
        if (Building *fort = runtime_building_for_record(b)) {
            formation_legion_delete_for_fort(*fort);
        }
    }
    if (record_matches(b, "triumphal_arch")) {
        city_buildings_remove_triumphal_arch();
        building_menu_update();
    }
    if (building_monument_is_unfinished_monument(b)) {
        building_monument_remove_all_deliveries(b->id);
    }
}

// Restoring an undone building can immediately make it visible again, so rebuild its native runtime graphics cache here.
building *building_restore_from_undo(building *to_restore)
{
    if (to_restore->id >= data.buildings.size()) {
        resize_buildings(to_restore->id + 1);
    }
    building *b = building_slot(to_restore->id);
    memcpy(b, to_restore, sizeof(building));
    fill_adjacent_types(b);
    city_culture_add_building_module_capacity(b);
    if (b->state == BUILDING_STATE_IN_USE || b->state == BUILDING_STATE_MOTHBALLED) {
        if (Building *building = runtime_building_for_record(b)) {
            building->refresh_graphic();
        }
    }
    return b;
}

void building_trim(void)
{
    trim_buildings();
}

static int rubble_type_can_be_repaired(const building_type_registry_impl::BuildingType *type)
{
    return type &&
        !building_monument_is_limited(type->type()) &&
        !type->attr_is("aqueduct") &&
        !building_is_fort(type->type());
}

static void for_each_rubble_origin(
    const RubbleState &origin,
    const std::function<void(Building &)> &visitor)
{
    Building::for_each([&](Building *candidate) {
        const RubbleState *candidate_origin =
            candidate && candidate->Rubble ? candidate->Rubble->state() : nullptr;
        if (candidate_origin && origin.same_origin(*candidate_origin)) {
            visitor(*candidate);
        }
    });
}

int Building::rubble_is_still_burning() const
{
    if (!Rubble || !Rubble->is_burning() || !Rubble->state()) {
        return 0;
    }

    int burning = 0;
    for_each_rubble_origin(*Rubble->state(), [&](Building &candidate) {
        const building *candidate_record = candidate.record();
        if (!burning && candidate.Rubble->is_burning() && candidate_record &&
            (candidate_record->state != BUILDING_STATE_RUBBLE || map_has_figure_at(candidate_record->grid_offset))) {
            burning = 1;
        }
    });
    return burning;
}

static int repair_price(
    const building_type_registry_impl::BuildingType &type,
    const building_construction_assessment &assessment)
{
    if (assessment.placement.owner_charge_count() != 1) {
        return 0;
    }
    if (type.has_housing()) {
        const building_type_registry_impl::BuildingType *vacant_lot =
            building_construction_repair_replacement_type(type);
        const model_building *vacant_lot_model = vacant_lot ? model_get_building(vacant_lot->type()) : nullptr;
        if (!vacant_lot_model) {
            return 0;
        }
        const int lot_count = assessment.placement.unique_occupied_tiles();
        const int lot_cost_with_fee = vacant_lot_model->cost + (vacant_lot_model->cost + 19) / 20;
        return assessment.clear_cost + lot_count * lot_cost_with_fee;
    }
    const int building_cost = model_get_building(type.type())->cost;
    return assessment.clear_cost +
        assessment.placement.owner_charge_count() * (building_cost + building_cost / 20);
}

int Building::repair_cost() const
{
    const RubbleState *origin = Rubble ? Rubble->state() : nullptr;
    const building_type_registry_impl::BuildingType *original_type =
        Rubble ? Rubble->original_type() : nullptr;
    if (!origin || !rubble_type_can_be_repaired(original_type) || rubble_is_still_burning()) {
        return 0;
    }

    const building_construction_assessment assessment =
        building_construction_assess_repair(*original_type, *origin);
    return assessment.can_place ? repair_price(*original_type, assessment) : 0;
}

static void clear_rubble_identity(const Building *building_object, int fallback_grid_offset)
{
    const building_type_registry_impl::BuildingGeometry geometry = building_object ?
        building_type_registry_impl::BuildingGeometry::query(*building_object) :
        building_type_registry_impl::BuildingGeometry::from_world_cells({});
    if (geometry.valid()) {
        for (const building_type_registry_impl::BuildingGeometryCell &cell : geometry.cells()) {
            map_building_set_rubble_grid_building_id(map_grid_offset(cell.x, cell.y), 0, 1);
        }
    } else if (map_grid_is_valid_offset(fallback_grid_offset)) {
        map_building_set_rubble_grid_building_id(fallback_grid_offset, 0, 1);
    }
}

int Building::yield_rubble_to_repair(const RubbleState &origin)
{
    if (!Rubble || !Rubble->state() || !origin.same_origin(*Rubble->state())) {
        return 0;
    }
    const int offset = grid_offset();
    if (!map_building_exists_at(offset) || map_building_at(offset).record() != record_) {
        return 0;
    }
    map_building_clear_at(offset);
    map_terrain_remove(offset, TERRAIN_RUBBLE | TERRAIN_BUILDING);
    return 1;
}

void Building::restore_rubble_after_failed_repair()
{
    map_building_tiles_add_rubble(*this, x(), y());
    refresh_graphic();
}

void Building::retire_rubble_after_repair()
{
    city_culture_remove_building_module_capacity(record_);
    record_->state = BUILDING_STATE_DELETED_BY_GAME;
    if (map_building_rubble_building_id(grid_offset()) == id) {
        map_building_set_rubble_grid_building_id(grid_offset(), 0, 1);
    }
}

static int repair_plan_has_nearby_enemy(
    const building_construction::ConstructionPlacementPlan &placement)
{
    std::vector<int> occupied_cells;
    occupied_cells.reserve(static_cast<std::size_t>(placement.unique_occupied_tiles()));
    for (const building_construction::ConstructionPlacementPart &part : placement.parts()) {
        for (const building_construction::ConstructionPlacementTile &tile : part.tiles) {
            occupied_cells.push_back(tile.grid_offset);
        }
    }
    return building_construction_nearby_enemy_type(
        occupied_cells.data(), static_cast<int>(occupied_cells.size())) != FIGURE_NONE;
}

static void show_unrepairable_warning(const building_type_registry_impl::BuildingType *type)
{
    if (type && building_monument_is_limited(type->type())) {
        city_warning_show(WARNING_REPAIR_MONUMENT, translation_for_key("TR_WARNING_CANT_REPAIR_MONUMENTS"));
    } else if (type && type->attr_is("aqueduct")) {
        city_warning_show(WARNING_REPAIR_AQUEDUCT, translation_for_key("TR_WARNING_CANT_REPAIR_AQUEDUCTS"));
    } else {
        city_warning_show(WARNING_REPAIR_IMPOSSIBLE, translation_for_key("TR_WARNING_REPAIR_IMPOSSIBLE"));
    }
}

int Building::repair()
{
    const RubbleState *state = Rubble ? Rubble->state() : nullptr;
    const building_type_registry_impl::BuildingType *original_type =
        Rubble ? Rubble->original_type() : nullptr;
    if (!state || !rubble_type_can_be_repaired(original_type)) {
        show_unrepairable_warning(original_type);
        return 0;
    }
    if (rubble_is_still_burning()) {
        city_warning_show(WARNING_REPAIR_BURNING, translation_for_key("TR_WARNING_REPAIR_BURNING"));
        return 0;
    }

    const RubbleState origin = *state;
    const building_construction_assessment assessment =
        building_construction_assess_repair(*original_type, origin);
    if (!assessment.can_place) {
        show_unrepairable_warning(original_type);
        return 0;
    }
    if (repair_plan_has_nearby_enemy(assessment.placement)) {
        city_warning_show_translated(WARNING_ENEMY_NEARBY);
        return 0;
    }
    if (city_finance_out_of_money()) {
        city_warning_show_translated(WARNING_OUT_OF_MONEY);
        return 0;
    }

    const int cost = repair_price(*original_type, assessment);
    if (cost <= 0) {
        show_unrepairable_warning(original_type);
        return 0;
    }

    std::vector<Building *> rubble_parts;
    for_each_rubble_origin(origin, [&rubble_parts](Building &rubble) {
        rubble_parts.push_back(&rubble);
    });
    std::vector<Building *> yielded_rubble;
    yielded_rubble.reserve(rubble_parts.size());
    for (Building *rubble : rubble_parts) {
        if (!rubble->yield_rubble_to_repair(origin)) {
            for (Building *yielded : yielded_rubble) {
                yielded->restore_rubble_after_failed_repair();
            }
            show_unrepairable_warning(original_type);
            return 0;
        }
        yielded_rubble.push_back(rubble);
    }

    Building *repaired = building_construction_place_repaired_building(assessment, origin);
    if (!repaired) {
        for (Building *yielded : yielded_rubble) {
            yielded->restore_rubble_after_failed_repair();
        }
        show_unrepairable_warning(original_type);
        return 0;
    }

    for (Building *yielded : yielded_rubble) {
        yielded->retire_rubble_after_repair();
    }

    city_finance_process_construction(cost);
    const building_type_registry_impl::BuildingGeometry repaired_geometry =
        building_type_registry_impl::BuildingGeometry::query(*repaired);
    const building_type_registry_impl::BuildingGeometrySquareExtent explosion =
        repaired_geometry.centered_square_extent(5);
    figure_create_explosion_cloud(
        explosion.size ? explosion.x : repaired->x(),
        explosion.size ? explosion.y : repaired->y(),
        explosion.size ? explosion.size : 1,
        1);
    game_undo_disable();
    return cost;
}

void building_update_state(void)
{
    int land_recalc = 0;
    int wall_recalc = 0;
    int road_recalc = 0;
    int aqueduct_recalc = 0;
    for (size_t i = 0; i < data.buildings.size(); i++) {
        building *b = &data.buildings[i];
        if (b->state == BUILDING_STATE_CREATED) {
            b->state = BUILDING_STATE_IN_USE;
            if (Building *building_obj = runtime_building_for_record(b)) {
                water_access_runtime_refresh_building(building_obj);
                // When a created building becomes live, rebuild its cached native image-group bindings immediately.
                building_obj->refresh_graphic();
            }
            city_culture_refresh_building_module_capacity(b);
        }
        Building *state_building = runtime_building_for_record(b);
        if (b->state == BUILDING_STATE_IN_USE && state_building && state_building->Housing) {
            continue;
        }
        if (b->state == BUILDING_STATE_UNDO || b->state == BUILDING_STATE_DELETED_BY_PLAYER) {
            Building *building_obj = runtime_building_for_record(b);
            const building_type_registry_impl::BuildingType *definition = building_obj ? building_obj->type :
                definition_for_record(b);
            if (record_matches(b, "tower") || record_matches(b, "gatehouse")) {
                wall_recalc = 1;
                road_recalc = 1;
            } else if (record_matches(b, "reservoir")) {
                aqueduct_recalc = 1;
            } else {
                if (definition &&
        (definition->is_granary() || definition->bridge().is_bridge())) {
                    road_recalc = 1;
                } else if (building_monument_is_grand_temple(b->type) ||
                    record_matches(b, "pantheon") || record_matches(b, "lighthouse")) {
                    road_recalc = 1;
                }
            }
            if (building_runtime *runtime = building_runtime_impl::get_or_create_instance(b)) {
                runtime->building.remove_map_tiles();
            }
            land_recalc = 1;
            building_delete(b);
        } else if (b->state == BUILDING_STATE_RUBBLE) {
            if (!rubble_record_has_map_presence(b)) {
                building_delete(b);
                continue;
            }
            Building *rubble_building = runtime_building_for_record(b);
            if (rubble_building && rubble_building->Housing) {
                HousingState &state = rubble_building->Housing->state();
                city_population_remove_home_removed(state.population);
                state.population = 0;
            }
            if (building_is_fort(b->type) || record_matches(b, "fort_ground")) {
                b->state = BUILDING_STATE_DELETED_BY_GAME;
                clear_rubble_identity(rubble_building, b->grid_offset);
                if (building_runtime *runtime = building_runtime_impl::get_or_create_instance(b)) {
                    runtime->building.remove_map_tiles();
                }
            }
            // building_delete(b); // keep the rubbled building as a reference for reconstruction

            // monuments clear
            if (building_monument_is_limited(b->type) || building_monument_is_unfinished_monument(b)) {
                building_delete(b);
            }

        } else if (b->state == BUILDING_STATE_DELETED_BY_GAME) {
            building_delete(b);
        }
    }
    if (wall_recalc) {
        map_tiles_update_all_walls();
    }
    if (aqueduct_recalc) {
        map_tiles_update_all_aqueducts(0);
    }
    if (land_recalc) {
        Route::updateLandTerrain();
    }
    if (road_recalc) {
        map_tiles_update_all_roads();
        map_tiles_update_all_highways();
    }
}

void building_update_desirability(void)
{
    for (building &record : data.buildings) {
        if (record.state != BUILDING_STATE_IN_USE) {
            continue;
        }

        // Use wider type to prevent 8-bit overflow. Sparse and rectangular
        // objects sample only cells that belong to their rotated foundation.
        int desirability = -9999;
        const building_type_registry_impl::BuildingType *definition = definition_for_record(&record);
        const building_type_registry_impl::FoundationDef *foundation =
            definition ? definition->foundation_def() : nullptr;
        if (foundation) {
            const int rotation = record_foundation_rotation(record, *foundation);
            for (const building_type_registry_impl::RotatedFoundationCell &cell :
                foundation->rotated_cells(rotation)) {
                desirability = std::max(
                    desirability,
                    map_desirability_get(map_grid_offset(record.x + cell.x, record.y + cell.y)));
            }
        }
        if (desirability == -9999) {
            desirability = map_desirability_get(record.grid_offset);
        }

        if (record.is_close_to_water) {
            desirability += 10;
        }

        switch (map_elevation_at(record.grid_offset)) {
            case 0: break;
            case 1: desirability += 10; break;
            case 2: desirability += 12; break;
            case 3: desirability += 14; break;
            case 4: desirability += 16; break;
            default: desirability += 18; break;
        }

        // Clamp before assigning to 8-bit signed int
        if (desirability > 100) {
            desirability = 100;
        } else if (desirability < -100) {
            desirability = -100;
        }

        const int8_t value = static_cast<int8_t>(desirability);
        if (record.desirability != value) {
            record.desirability = value;
            if (Building *building_object = runtime_building_for_record(&record)) {
                building_object->invalidate_graphic();
                if (building_object->type && building_object->type->attr_is("fountain")) {
                    water_access_runtime_building_changed(building_object);
                }
            }
        }
    }
}

int building_is_active(const building *b)
{
    if (b->state != BUILDING_STATE_IN_USE) {
        return 0;
    }
    const building_type_registry_impl::BuildingType *definition =
        building_type_registry_impl::definition_for_type(b->type);
    if (definition && definition->has_housing()) {
        const Building *building_object = Building::get(b->id);
        return building_object && building_object->Housing &&
            building_object->Housing->state().population > 0;
    }
    if (building_monument_is_unfinished_monument(b)) {
        return 0;
    }
    if (type_attr_is(b->type, "reservoir")) {
        return b->has_water_access;
    }
    if (type_attr_is(b->type, "fountain")) {
        return b->has_water_access;
    }
    if (definition != nullptr) {
        if (definition->is_temple(GOD_ALL, building_type_registry_impl::ReligionTier::Oracle)) {
            if (building_monument_is_monument(b) == 0) {
                return 1;
            }
            return b->monument.phase == MONUMENT_FINISHED;
        }
    }
    if (type_attr_is(b->type, "nymphaeum")) {
        return b->monument.phase == MONUMENT_FINISHED;
    }
    if (type_attr_is(b->type, "small_mausoleum")) {
        return b->monument.phase == MONUMENT_FINISHED;
    }
    if (type_attr_is(b->type, "large_mausoleum")) {
        return b->monument.phase == MONUMENT_FINISHED;
    }
    if (type_attr_is(b->type, "wharf")) {
        Building *wharf = runtime_building_for_record(const_cast<building *>(b));
        if (wharf == nullptr) {
            return 0;
        }
        if (b->num_workers <= 0) {
            return 0;
        }
        return map_water_wharf_live_fishing_boats(*wharf) > 0;
    }
    if (type_attr_is(b->type, "dock")) {
        if (b->num_workers <= 0) {
            return 0;
        }
        return b->has_water_access != 0;
    }
    return b->num_workers > 0;
}

int building_is_fort(building_type type)
{
    return type_attr_is_any(type, {
        "fort_legionaries",
        "fort_javelin",
        "fort_mounted",
        "fort_swords",
        "fort_archers"
    });
}

int Building::building_mothball_toggle()
{
    return record_ ? ::building_mothball_toggle(record_) : 0;
}

static void set_record_mothballed(building *record, int mothball)
{
    if (!record) {
        return;
    }
    const int target_state = mothball ? BUILDING_STATE_MOTHBALLED : BUILDING_STATE_IN_USE;
    if ((record->state != BUILDING_STATE_IN_USE && record->state != BUILDING_STATE_MOTHBALLED) ||
        record->state == target_state) {
        return;
    }
    city_culture_remove_building_module_capacity(record);
    record->state = static_cast<unsigned char>(target_state);
    if (mothball) {
        record->num_workers = 0;
    }
    city_culture_add_building_module_capacity(record);
}

static int set_composition_mothballed(building *record, int mothball)
{
    if (!record) {
        return 0;
    }
    building_runtime *runtime = building_runtime_impl::get_or_create_instance(record);
    if (!runtime) {
        return record->state;
    }

    BuildingComposition *composition = runtime->building.Composition;
    Building *owner = composition ? composition->owner() : nullptr;
    if (!owner) {
        return record->state;
    }
    composition->for_each_member([mothball](Building &member) {
        building *member_record = const_cast<building *>(member.record());
        if (!member_record) {
            return;
        }
        set_record_mothballed(member_record, mothball);
        water_access_runtime_refresh_building(&member);
    });
    return owner->record() ? owner->record()->state : record->state;
}

int building_mothball_toggle(building *b)
{
    if (!b) {
        return 0;
    }
    building_runtime *runtime = building_runtime_impl::get_or_create_instance(b);
    Building *owner_object = runtime && runtime->building.Composition ?
        runtime->building.Composition->owner() : nullptr;
    const building *owner = owner_object ? owner_object->record() : b;
    if (!owner || (owner->state != BUILDING_STATE_IN_USE && owner->state != BUILDING_STATE_MOTHBALLED)) {
        return owner ? owner->state : 0;
    }
    return set_composition_mothballed(b, owner->state == BUILDING_STATE_IN_USE);
}

int building_mothball_set(building *b, int mothball)
{
    return set_composition_mothballed(b, mothball != 0);
}

unsigned char building_stockpiling_toggle(building *b)
{
    b->data.industry.is_stockpiling = b->data.industry.is_stockpiling ? 0 : 1;
    return b->data.industry.is_stockpiling;
}

int building_get_levy(const building *b)
{
    const Building *building = b ? Building::get(b->id) : nullptr;
    if (building && ((building->Composition && building->Composition->is_child()) ||
        building->is_dynamic_bridge_segment())) {
        return 0;
    }
    int levy = building && building->Housing ? building->Housing->state().monthly_levy : b->levy_amount;
    if (levy <= 0) {
        return 0;
    }
    if (building_monument_type_is_monument(b->type) && b->monument.phase != MONUMENT_FINISHED) {
        return 0;
    }
    if (b->state != BUILDING_STATE_IN_USE && levy) {
        return 0;
    }

    // Pantheon base bonus
    const building_type_registry_impl::BuildingType *definition =
        building_type_registry_impl::definition_for_type(b->type);
    if (building_monument_working(type_from_attr("pantheon")) &&
        ((definition && definition->is_temple()) || type_attr_is_any(b->type, {
                "nymphaeum",
                "small_mausoleum",
                "large_mausoleum"
            }))) {
        levy = (levy / 4) * 3;
    }

    // Mars module 1 bonus
    if (building_monument_gt_module_is_active(MARS_MODULE_1_MESS_HALL)) {
        if (building_is_fort(b->type)) {
            levy = (levy / 4) * 3;
        }
    }

    return difficulty_adjust_levies(levy);
}

int building_get_tourism(const building *b)
{
    return b->is_tourism_venue;
}

int building_get_laborers(building_type type)
{
    const model_building *model = model_get_building(type);
    int workers = model->laborers;
    // Neptune GT bonus
    if (type_attr_is(type, "fountain") && grand_temple_for_god(GOD_NEPTUNE, true)) {
        workers /= 2;
        if (workers == 0) {
            workers = 1;
        }
    }
    return workers;
}

void building_clear_all(void)
{
    building_runtime_reset();
    city_culture_clear_module_capacity_cache();
    memset(data.first_of_type, 0, sizeof(data.first_of_type));
    memset(data.last_of_type, 0, sizeof(data.last_of_type));

    data.buildings.clear();
    resize_buildings(1); // Ignore first building

    extra.created_sequence = 0;
    extra.incorrect_houses = 0;
    extra.unfixable_houses = 0;
}

void building_make_immune_cheat(void)
{
    for (building &record : data.buildings) {
        record.fire_proof = 1;
    }
}

static int building_resource_save_value(resource_type resource, int value)
{
    if (value == 0) {
        return 0;
    }
    if (resource < RESOURCE_NONE) {
        return 0;
    }
    if (resource >= RESOURCE_SLOT_COUNT) {
        return 0;
    }
    if (resource == RESOURCE_NONE) {
        return 1;
    }
    return resource_is_tradeable(resource);
}

static resource_type building_resource_save_ref(resource_type resource)
{
    if (resource == RESOURCE_NONE) {
        return RESOURCE_NONE;
    }
    return resource > RESOURCE_NONE && resource < RESOURCE_SLOT_COUNT && resource_is_tradeable(resource) ?
        resource :
        RESOURCE_NONE;
}

static int building_resource_i16_count(const short *values)
{
    int count = building_resource_save_value(RESOURCE_NONE, values[RESOURCE_NONE]);
    for (int i = 0; i < resource_loaded_count(); i++) {
        resource_type resource = resource_get_loaded(i);
        if (resource != RESOURCE_NONE && building_resource_save_value(resource, values[resource])) {
            count++;
        }
    }
    return count;
}

static int building_accepted_good_count(const building *b)
{
    int count = building_resource_save_value(
        RESOURCE_NONE, building_accepted_good_save_value(b, RESOURCE_NONE));
    for (int i = 0; i < resource_loaded_count(); i++) {
        resource_type resource = resource_get_loaded(i);
        if (resource != RESOURCE_NONE &&
            building_resource_save_value(resource, building_accepted_good_save_value(b, resource))) {
            count++;
        }
    }
    return count;
}

static void building_write_resource_i16_values(buffer *buf, const short *values)
{
    buffer_write_u32(buf, building_resource_i16_count(values));
    if (building_resource_save_value(RESOURCE_NONE, values[RESOURCE_NONE])) {
        resource_save_write_ref(buf, RESOURCE_NONE);
        buffer_write_i16(buf, values[RESOURCE_NONE]);
    }
    for (int i = 0; i < resource_loaded_count(); i++) {
        resource_type resource = resource_get_loaded(i);
        if (resource != RESOURCE_NONE && building_resource_save_value(resource, values[resource])) {
            resource_save_write_ref(buf, resource);
            buffer_write_i16(buf, values[resource]);
        }
    }
}

static void building_write_accepted_goods(buffer *buf, const building *b)
{
    buffer_write_u32(buf, building_accepted_good_count(b));
    const unsigned char none_value = building_accepted_good_save_value(b, RESOURCE_NONE);
    if (building_resource_save_value(RESOURCE_NONE, none_value)) {
        resource_save_write_ref(buf, RESOURCE_NONE);
        buffer_write_u8(buf, none_value);
    }
    for (int i = 0; i < resource_loaded_count(); i++) {
        resource_type resource = resource_get_loaded(i);
        const unsigned char value = building_accepted_good_save_value(b, resource);
        if (resource != RESOURCE_NONE && building_resource_save_value(resource, value)) {
            resource_save_write_ref(buf, resource);
            buffer_write_u8(buf, value);
        }
    }
}

static int building_uses_fetch_inventory(const building *b)
{
    if (!b) {
        return 0;
    }
    const building_type_registry_impl::BuildingType *definition = definition_for_type(b->type);
    return definition && (definition->has_distribution() || definition->is_caravanserai()) ? 1 : 0;
}

static void building_resource_state_write_payload(buffer *buf)
{
    int live_count = 0;
    for (const building &record : data.buildings) {
        if (record.state != BUILDING_STATE_UNUSED) {
            live_count++;
        }
    }

    buffer_write_u32(buf, 1);
    buffer_write_u32(buf, live_count);
    for (building &record : data.buildings) {
        building *b = &record;
        if (record.state == BUILDING_STATE_UNUSED) {
            continue;
        }

        buffer_write_u32(buf, b->id);
        resource_save_write_ref(buf, building_resource_save_ref(static_cast<resource_type>(b->output_resource_id)));
        resource_save_write_ref(buf, building_resource_save_ref(
            record_matches(b, "warehouse_space") ? static_cast<resource_type>(b->subtype.warehouse_resource_id) :
                RESOURCE_NONE));
        resource_save_write_ref(buf, building_resource_save_ref(
            building_uses_fetch_inventory(b) ?
                static_cast<resource_type>(b->data.market.fetch_inventory_id) :
                RESOURCE_NONE));
        resource_save_write_ref(buf, building_resource_save_ref(
            record_matches(b, "cart_depot") ? static_cast<resource_type>(b->data.depot.current_order.resource_type) :
                RESOURCE_NONE));
        building_write_resource_i16_values(buf, b->resources);
        building_write_accepted_goods(buf, b);
    }
}

static int building_resource_state_read_count(buffer *buf, const char *field_name)
{
    uint32_t count = buffer_read_u32(buf);
    if (count > 4096) {
        log_error("Malformed keyed building resource count in save", field_name, static_cast<int>(count));
        return 0;
    }
    return static_cast<int>(count);
}

static void building_resource_state_load_i16_values(buffer *buf, building *b, bool *loaded_resources)
{
    int count = building_resource_state_read_count(buf, "resources");
    for (int i = 0; i < count; i++) {
        resource_type resource = resource_save_read_ref(buf);
        int value = buffer_read_i16(buf);
        if (resource >= RESOURCE_NONE && resource < RESOURCE_SLOT_COUNT) {
            if (loaded_resources) {
                loaded_resources[resource] = true;
            }
            if (b) {
                b->resources[resource] = static_cast<short>(value);
            }
        }
    }
}

static void building_resource_state_load_u8_values(buffer *buf, building *b)
{
    int count = building_resource_state_read_count(buf, "accepted_goods");
    for (int i = 0; i < count; i++) {
        resource_type resource = resource_save_read_ref(buf);
        int value = buffer_read_u8(buf);
        if (b && resource >= RESOURCE_NONE && resource < RESOURCE_SLOT_COUNT) {
            building_load_accepted_good(b, resource, static_cast<unsigned char>(value));
        }
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
    if (!b || !record_matches(b, "dock") || dock_has_any_accepted_goods(b)) {
        return;
    }
    for (resource_type r = (RESOURCE_NONE + 1); r < RESOURCE_SLOT_COUNT; r = static_cast<resource_type>(r + 1)) {
        b->accepted_goods[r] = 1;
    }
}

static void restore_omitted_native_storage_resources(
    building *b, const short *flat_resources, const bool *loaded_resources)
{
    if (!b || !flat_resources || !loaded_resources) {
        return;
    }

    const building_type_registry_impl::BuildingType *definition = definition_for_record(b);
    if (!definition || !definition->has_native_storage()) {
        return;
    }

    for (const building_type_registry_impl::StorageType *storage_type : definition->storage_types()) {
        if (!storage_type) {
            continue;
        }
        for (resource_type resource : storage_type->resources()) {
            if (resource <= RESOURCE_NONE || resource >= RESOURCE_SLOT_COUNT ||
                loaded_resources[resource] || !flat_resources[resource]) {
                continue;
            }
            b->resources[resource] = flat_resources[resource];
        }
    }
}

void building_resource_state_save(buffer *buf)
{
    if (!buf) {
        return;
    }

    size_t capacity = 65536;
    std::vector<uint8_t> payload;
    buffer scratch;
    for (;;) {
        payload.assign(capacity, 0);
        buffer_init(&scratch, payload.data(), payload.size());
        building_resource_state_write_payload(&scratch);
        if (!scratch.overflow) {
            break;
        }
        capacity *= 2;
        if (capacity > 4 * 1024 * 1024) {
            log_error("Unable to save building resource state: payload is too large", 0, static_cast<int>(capacity));
            break;
        }
    }

    buffer_init_dynamic(buf, scratch.index);
    buffer_write_raw(buf, payload.data(), scratch.index);
}

void building_resource_state_load(buffer *buf)
{
    if (!buf || !buf->size) {
        return;
    }

    buffer payload = *buf;
    if (buffer_load_dynamic(&payload) < 2 * sizeof(uint32_t)) {
        log_error("Unable to load building resource state: payload is invalid", 0, 0);
        return;
    }

    uint32_t format_version = buffer_read_u32(&payload);
    if (format_version != 1) {
        log_error("Unable to load building resource state: unsupported format version", 0,
            static_cast<int>(format_version));
        return;
    }

    int building_entries = building_resource_state_read_count(&payload, "buildings");
    for (int i = 0; i < building_entries; i++) {
        uint32_t id = buffer_read_u32(&payload);
        building *b = building_slot(id);
        if (b && b->state == BUILDING_STATE_UNUSED) {
            b = nullptr;
        }

        resource_type output = resource_save_read_ref(&payload);
        resource_type warehouse_resource = resource_save_read_ref(&payload);
        resource_type fetch_inventory = resource_save_read_ref(&payload);
        resource_type depot_order_resource = resource_save_read_ref(&payload);

        short flat_resources[RESOURCE_SLOT_COUNT] = {};
        bool loaded_resources[RESOURCE_SLOT_COUNT] = {};
        if (b) {
            memcpy(flat_resources, b->resources, sizeof(flat_resources));
            b->output_resource_id = static_cast<unsigned char>(
                output >= RESOURCE_NONE && output < RESOURCE_SLOT_COUNT ? output : RESOURCE_NONE);
            if (record_matches(b, "warehouse_space")) {
                b->subtype.warehouse_resource_id =
                    warehouse_resource >= RESOURCE_NONE && warehouse_resource < RESOURCE_SLOT_COUNT ?
                        static_cast<short>(warehouse_resource) :
                        RESOURCE_NONE;
            }
            if (building_uses_fetch_inventory(b)) {
                b->data.market.fetch_inventory_id =
                    fetch_inventory >= RESOURCE_NONE && fetch_inventory < RESOURCE_SLOT_COUNT ?
                        static_cast<unsigned char>(fetch_inventory) :
                        RESOURCE_NONE;
            }
            if (record_matches(b, "cart_depot")) {
                b->data.depot.current_order.resource_type =
                    depot_order_resource >= RESOURCE_NONE && depot_order_resource < RESOURCE_SLOT_COUNT ?
                        static_cast<resource_type>(depot_order_resource) :
                        RESOURCE_NONE;
            }
            memset(b->resources, 0, sizeof(b->resources));
            memset(b->accepted_goods, 0, sizeof(b->accepted_goods));
        }

        building_resource_state_load_i16_values(&payload, b, loaded_resources);
        restore_omitted_native_storage_resources(b, flat_resources, loaded_resources);
        building_resource_state_load_u8_values(&payload, b);
        repair_dock_accepted_goods_if_empty(b);
    }
}

int building_is_close_to_water(const building *b)
{
    Building *building_object = runtime_building_for_record(const_cast<building *>(b));
    if (!b || !building_object) {
        return 0;
    }
    return geometry_has_water_within_range(
        building_type_registry_impl::BuildingGeometry::query(*building_object),
        WATER_DESIRABILITY_RANGE);
}

void building_save_state(buffer *buf, buffer *highest_id, buffer *highest_id_ever,
    buffer *sequence, buffer *corrupt_houses)
{
    int buf_size = sizeof(int32_t) + static_cast<int>(data.buildings.size()) * BUILDING_STATE_CURRENT_BUFFER_SIZE;
    uint8_t *buf_data = static_cast<uint8_t *>(malloc(buf_size));
    buffer_init(buf, buf_data, buf_size);
    buffer_write_i32(buf, BUILDING_STATE_CURRENT_BUFFER_SIZE);
    for (building &record : data.buildings) {
        building_state_save_to_buffer(buf, &record);
    }
    buffer_write_i32(highest_id, static_cast<int>(data.buildings.size()));
    buffer_write_i32(highest_id_ever, static_cast<int>(data.buildings.size()));
    buffer_skip(highest_id_ever, 4);
    buffer_write_i32(sequence, extra.created_sequence);

    buffer_write_i32(corrupt_houses, extra.incorrect_houses);
    buffer_write_i32(corrupt_houses, extra.unfixable_houses);
}

struct LegacyTilePromotionTypes {
    building_type gardens;
    building_type overgrown_gardens;
    building_type plaza;
    building_type aqueduct;
    building_type wall;
    building_type road;
    building_type highway;
    building_type burning_ruin;
};

static LegacyTilePromotionTypes legacy_tile_promotion_types()
{
    return {
        type_from_attr("gardens"),
        type_from_attr("overgrown_gardens"),
        type_from_attr("plaza"),
        type_from_attr("aqueduct"),
        type_from_attr("wall"),
        type_from_attr("road"),
        type_from_attr("highway"),
        type_from_attr("burning_ruin"),
    };
}

static bool definition_is_surface_terrain_tile(
    const building_type_registry_impl::BuildingType *definition)
{
    if (!definition) {
        return false;
    }
    const building_type_registry_impl::TileKind tile_kind = definition->tile().kind();
    return tile_kind != building_type_registry_impl::TileKind::None ||
        definition->tool().is_road() ||
        definition->tool().is_highway() ||
        definition->tool().is_aqueduct();
}

static bool loaded_record_owns_surface_tile(int grid_offset, building_type expected_type)
{
    const unsigned int id = map_building_loaded_id_at(grid_offset);
    if (!id || id >= data.buildings.size()) {
        return false;
    }
    const building &record = data.buildings[id];
    if (record.state == BUILDING_STATE_UNUSED || record.type != expected_type) {
        return false;
    }
    const int x = map_grid_offset_to_x(grid_offset);
    const int y = map_grid_offset_to_y(grid_offset);
    return record_foundation_contains(record, x, y) != 0;
}

static bool loaded_record_owns_unbound_foundation(const building &record)
{
    const building_type_registry_impl::BuildingType *definition = definition_for_record(&record);
    const building_type_registry_impl::FoundationDef *foundation =
        definition ? definition->foundation_def() : nullptr;
    if (!foundation || foundation->cells().empty() ||
        std::any_of(foundation->cells().begin(), foundation->cells().end(),
            [](const building_type_registry_impl::FoundationCellDefinition &cell) {
                return cell.binds_building != 0;
            })) {
        return false;
    }
    building_type_registry_impl::FoundationTerrainSaveState saved;
    std::vector<building_type_registry_impl::FoundationTerrainDelta> deltas;
    if (!building_runtime_loaded_foundation_state(record.id, &saved) ||
        !building_type_registry_impl::foundation_terrain_deltas_from_save(*foundation, saved, &deltas)) {
        return false;
    }
    const int rotation = record_foundation_rotation(record, *foundation);
    const std::vector<building_type_registry_impl::FoundationCellDefinition> &canonical = foundation->cells();
    bool has_owned_delta = false;
    for (const building_type_registry_impl::RotatedFoundationCell &cell : foundation->rotated_cells(rotation)) {
        if (!cell.definition || !map_grid_is_inside(record.x + cell.x, record.y + cell.y, 1)) {
            return false;
        }
        const int cell_index = static_cast<int>(cell.definition - canonical.data());
        if (cell_index < 0 || cell_index >= static_cast<int>(deltas.size())) {
            return false;
        }
        const int grid_offset = map_grid_offset(record.x + cell.x, record.y + cell.y);
        has_owned_delta = has_owned_delta || deltas[cell_index].added_terrain || deltas[cell_index].removed_terrain;
        if ((static_cast<unsigned int>(map_terrain_get(grid_offset)) & deltas[cell_index].added_terrain) !=
            deltas[cell_index].added_terrain) {
            return false;
        }
    }
    return has_owned_delta;
}

static bool bind_loaded_unbound_foundation(const building &record)
{
    const building_type_registry_impl::BuildingType *definition = definition_for_record(&record);
    const building_type_registry_impl::FoundationDef *foundation =
        definition ? definition->foundation_def() : nullptr;
    if (!foundation) {
        return false;
    }

    const int rotation = record_foundation_rotation(record, *foundation);
    for (const building_type_registry_impl::RotatedFoundationCell &cell : foundation->rotated_cells(rotation)) {
        const int grid_offset = map_grid_offset(record.x + cell.x, record.y + cell.y);
        const unsigned int existing_id = map_building_loaded_id_at(grid_offset);
        if (existing_id && existing_id != record.id && existing_id < data.buildings.size() &&
            data.buildings[existing_id].state != BUILDING_STATE_UNUSED) {
            return false;
        }
    }
    for (const building_type_registry_impl::RotatedFoundationCell &cell : foundation->rotated_cells(rotation)) {
        map_building_set_loaded_id(map_grid_offset(record.x + cell.x, record.y + cell.y), record.id);
    }
    return true;
}

static int legacy_tile_has_blocking_loaded_record(int grid_offset)
{
    if (!map_terrain_is(grid_offset, TERRAIN_BUILDING)) {
        return 0;
    }
    const unsigned int building_id = map_building_loaded_id_at(grid_offset);
    return building_id > 0 &&
        building_id < data.buildings.size() &&
        data.buildings[building_id].state != BUILDING_STATE_UNUSED;
}

static int legacy_tile_is_bridge_sprite(int grid_offset)
{
    return map_bridge_legacy_section_at(grid_offset) && map_terrain_is(grid_offset, TERRAIN_WATER);
}

static int legacy_highway_tile_is_complete_top_left(int grid_offset)
{
    if (map_grid_is_valid_offset(grid_offset) == 0) {
        return 0;
    }
    if (map_terrain_is(grid_offset, TERRAIN_HIGHWAY_TOP_LEFT) == 0) {
        return 0;
    }

    const int x = map_grid_offset_to_x(grid_offset);
    const int y = map_grid_offset_to_y(grid_offset);
    if (map_grid_is_inside(x, y, 2) == 0) {
        return 0;
    }

    const int bottom_left = map_grid_offset(x, y + 1);
    const int top_right = map_grid_offset(x + 1, y);
    const int bottom_right = map_grid_offset(x + 1, y + 1);
    if (legacy_tile_has_blocking_loaded_record(grid_offset)) {
        return 0;
    }
    if (legacy_tile_has_blocking_loaded_record(bottom_left)) {
        return 0;
    }
    if (legacy_tile_has_blocking_loaded_record(top_right)) {
        return 0;
    }
    if (legacy_tile_has_blocking_loaded_record(bottom_right)) {
        return 0;
    }
    if (map_terrain_is(bottom_left, TERRAIN_HIGHWAY_BOTTOM_LEFT) == 0) {
        return 0;
    }
    if (map_terrain_is(top_right, TERRAIN_HIGHWAY_TOP_RIGHT) == 0) {
        return 0;
    }
    return map_terrain_is(bottom_right, TERRAIN_HIGHWAY_BOTTOM_RIGHT);
}

static building_type legacy_tile_type_for_offset(int grid_offset, const LegacyTilePromotionTypes &types)
{
    if (legacy_tile_has_blocking_loaded_record(grid_offset)) {
        return BUILDING_NONE;
    }

    const int x = map_grid_offset_to_x(grid_offset);
    const int y = map_grid_offset_to_y(grid_offset);
    if (map_grid_is_inside(x, y, 1) == 0) {
        return BUILDING_NONE;
    }
    if (legacy_tile_is_bridge_sprite(grid_offset)) {
        return BUILDING_NONE;
    }

    if (types.highway != BUILDING_NONE) {
        if (legacy_highway_tile_is_complete_top_left(grid_offset)) {
            return types.highway;
        }
    }
    if (map_terrain_is(grid_offset, TERRAIN_HIGHWAY)) {
        return BUILDING_NONE;
    }
    if (types.aqueduct != BUILDING_NONE) {
        if (map_terrain_is(grid_offset, TERRAIN_AQUEDUCT)) {
            return types.aqueduct;
        }
    }
    if (types.wall != BUILDING_NONE) {
        if (map_terrain_is(grid_offset, TERRAIN_WALL)) {
            if (map_terrain_is(grid_offset, TERRAIN_GATEHOUSE) == 0) {
                return types.wall;
            }
        }
    }
    if (types.plaza != BUILDING_NONE) {
        if (map_terrain_is_superset(grid_offset, TERRAIN_ROAD | TERRAIN_GARDEN)) {
            if (map_property_is_plaza_earthquake_or_overgrown_garden(grid_offset)) {
                return types.plaza;
            }
        }
    }
    if (map_terrain_is(grid_offset, TERRAIN_GARDEN)) {
        if (map_property_is_plaza_earthquake_or_overgrown_garden(grid_offset)) {
            return types.overgrown_gardens;
        }
        return types.gardens;
    }
    if (types.road != BUILDING_NONE) {
        if (map_terrain_is(grid_offset, TERRAIN_ROAD)) {
            return types.road;
        }
    }
    if (types.burning_ruin != BUILDING_NONE) {
        if (map_terrain_is(grid_offset, TERRAIN_RUBBLE)) {
            if (map_terrain_is(grid_offset, TERRAIN_WATER) == 0) {
                return types.burning_ruin;
            }
        }
    }
    return BUILDING_NONE;
}

static int legacy_tile_promoted_state(building_type type, int grid_offset, const LegacyTilePromotionTypes &types)
{
    if (type == types.burning_ruin) {
        if (map_terrain_is(grid_offset, TERRAIN_BUILDING) == 0) {
            return BUILDING_STATE_RUBBLE;
        }
    }
    return BUILDING_STATE_IN_USE;
}

static RubbleState legacy_rubble_origin(int grid_offset, unsigned int *source_id)
{
    RubbleState origin;
    const unsigned int id = map_building_rubble_building_id(grid_offset);
    if (source_id) {
        *source_id = id;
    }
    if (!id || id >= data.buildings.size()) {
        return origin;
    }
    if (building_runtime_loaded_rubble_state(id, &origin) && origin.original_type) {
        return origin;
    }

    const building &source = data.buildings[id];
    const building_type_registry_impl::BuildingType *definition = definition_for_record(&source);
    if (!definition || definition->attr_is("burning_ruin")) {
        return origin;
    }

    origin.original_type = definition;
    origin.original_orientation = static_cast<unsigned char>(source.subtype.orientation);
    origin.original_grid_offset = static_cast<unsigned short>(map_grid_offset(source.x, source.y));
    return origin;
}

static building *append_legacy_tile_building_record(
    building_type type,
    int grid_offset,
    int state,
    const RubbleState *rubble_origin)
{
    const building_type_registry_impl::BuildingType *definition = definition_for_type(type);
    if (definition == nullptr) {
        return nullptr;
    }

    const building_properties *props = building_properties_for_type(type);
    data.buildings.resize(data.buildings.size() + 1);
    building *record = &data.buildings.back();
    memset(record, 0, sizeof(*record));

    record->id = static_cast<unsigned int>(data.buildings.size() - 1);
    record->state = static_cast<unsigned char>(state);
    record->faction_id = 1;
    record->type = type;
    record->x = static_cast<unsigned char>(map_grid_offset_to_x(grid_offset));
    record->y = static_cast<unsigned char>(map_grid_offset_to_y(grid_offset));
    record->grid_offset = static_cast<short>(grid_offset);
    record->created_sequence = static_cast<unsigned short>(extra.created_sequence++);
    if (definition->has_housing()) {
        HousingState housing_state;
        housing_state.happiness = 100;
        building_runtime_stage_loaded_housing_state(record->id, housing_state);
    }
    record->fire_proof = static_cast<unsigned char>(
        definition->flags().has_fire_proof() ? definition->flags().fire_proof() : (props ? props->fire_proof : 0));
    record->output_resource_id = static_cast<unsigned char>(building_output_resource(definition));

    if (type_attr_is(type, "burning_ruin")) {
        record->fire_duration = static_cast<short>(state == BUILDING_STATE_RUBBLE ? 0 : 1);
        record->fire_proof = static_cast<unsigned char>(1);
        building_runtime_stage_loaded_rubble_state(record->id, rubble_origin ? *rubble_origin : RubbleState{});
    }

    return record;
}

static void bind_legacy_tile_building_record_to_map(const building *record, const LegacyTilePromotionTypes &types)
{
    if (!record) {
        return;
    }

    const int grid_offset = record->grid_offset;
    const unsigned int id = record->id;
    if (record->type == types.highway) {
        const int x = record->x;
        const int y = record->y;
        map_building_set_loaded_id(map_grid_offset(x, y), id);
        map_building_set_loaded_id(map_grid_offset(x, y + 1), id);
        map_building_set_loaded_id(map_grid_offset(x + 1, y), id);
        map_building_set_loaded_id(map_grid_offset(x + 1, y + 1), id);
        return;
    }

    map_building_set_loaded_id(grid_offset, id);
    if (record->type == types.wall) {
        map_terrain_add(grid_offset, TERRAIN_WALL | TERRAIN_BUILDING);
    } else if (record->type == types.burning_ruin) {
        map_terrain_add(grid_offset, TERRAIN_RUBBLE | TERRAIN_BUILDING);
        map_building_set_rubble_grid_building_id(grid_offset, id, 1);
    }
}

static void rebuild_loaded_record_type_links()
{
    memset(data.first_of_type, 0, sizeof(data.first_of_type));
    memset(data.last_of_type, 0, sizeof(data.last_of_type));

    for (building &record : data.buildings) {
        record.prev_of_type = nullptr;
        record.next_of_type = nullptr;
    }

    for (building &record : data.buildings) {
        if (record.state != BUILDING_STATE_UNUSED) {
            fill_adjacent_types(&record);
        }
    }
}

static void discard_loaded_record(building &record)
{
    const unsigned int id = record.id;
    memset(&record, 0, sizeof(record));
    record.id = id;
}

static void normalize_loaded_surface_records()
{
    int discarded = 0;
    for (building &record : data.buildings) {
        if (!record.id || record.state == BUILDING_STATE_UNUSED ||
            !definition_is_surface_terrain_tile(definition_for_record(&record))) {
            continue;
        }

        bool has_map_presence = false;
        const building_type_registry_impl::BuildingType *definition = definition_for_record(&record);
        const building_type_registry_impl::FoundationDef *foundation =
            definition ? definition->foundation_def() : nullptr;
        if (foundation) {
            const int rotation = record_foundation_rotation(record, *foundation);
            for (const building_type_registry_impl::RotatedFoundationCell &cell :
                foundation->rotated_cells(rotation)) {
                const int x = record.x + cell.x;
                const int y = record.y + cell.y;
                if (map_grid_is_inside(x, y, 1) &&
                    map_building_loaded_id_at(map_grid_offset(x, y)) == record.id) {
                    has_map_presence = true;
                    break;
                }
            }
        }
        if (!has_map_presence && loaded_record_owns_unbound_foundation(record)) {
            has_map_presence = bind_loaded_unbound_foundation(record);
        }
        if (!has_map_presence) {
            discard_loaded_record(record);
            discarded++;
        }
    }

    if (discarded) {
        trim_buildings();
        rebuild_loaded_record_type_links();
        log_info("Discarded unbound duplicate surface building records", 0, discarded);
    }
}

static void clear_loaded_unbound_foundation_bindings()
{
    // The load bridge uses the serialized map-building grid to deduplicate
    // surface records before runtime Foundation ownership exists. Unbound
    // cells (roads, plazas, highways, and similar overlays) must not survive
    // that bridge as bound occupants: normal construction never binds them,
    // and leaving the temporary id here makes later overlays pass preview but
    // fail when their bound Foundation is published.
    int cleared = 0;
    for (const building &record : data.buildings) {
        if (!record.id || record.state == BUILDING_STATE_UNUSED) {
            continue;
        }
        const building_type_registry_impl::BuildingType *definition = definition_for_record(&record);
        const building_type_registry_impl::FoundationDef *foundation =
            definition ? definition->foundation_def() : nullptr;
        if (!foundation) {
            continue;
        }
        const int rotation = record_foundation_rotation(record, *foundation);
        for (const building_type_registry_impl::RotatedFoundationCell &cell :
            foundation->rotated_cells(rotation)) {
            if (!cell.definition || cell.definition->binds_building) {
                continue;
            }
            const int x = record.x + cell.x;
            const int y = record.y + cell.y;
            if (!map_grid_is_inside(x, y, 1)) {
                continue;
            }
            const int grid_offset = map_grid_offset(x, y);
            if (map_building_loaded_id_at(grid_offset) == record.id) {
                map_building_set_loaded_id(grid_offset, 0);
                cleared++;
            }
        }
    }
    if (cleared) {
        log_info("Cleared temporary unbound surface bindings", 0, cleared);
    }
}

static void normalize_loaded_rubble_records()
{
    const building_type rubble_type = type_from_attr("rubble");
    int discarded = 0;
    int normalized = 0;
    for (building &record : data.buildings) {
        if (!record.id || record.state == BUILDING_STATE_UNUSED) {
            continue;
        }

        const building_type_registry_impl::BuildingType *definition = definition_for_record(&record);
        const bool has_rubble_definition = definition && definition->has_rubble();
        const bool has_burning_definition =
            has_rubble_definition && definition->rubble().is_burning();
        const RubbleRecordDisposition disposition = rubble_record_disposition(
            record.state,
            has_rubble_definition,
            has_burning_definition,
            rubble_record_has_map_presence(&record));
        if (disposition == RubbleRecordDisposition::Discard) {
            discard_loaded_record(record);
            discarded++;
        } else if (disposition == RubbleRecordDisposition::NormalizeToRubble &&
            rubble_type != BUILDING_NONE) {
            record.type = rubble_type;
            record.fire_duration = 0;
            normalized++;
        }
    }
    trim_buildings();
    rebuild_loaded_record_type_links();
    if (discarded || normalized) {
        char detail[128];
        snprintf(detail, sizeof(detail), "discarded=%d normalized=%d", discarded, normalized);
        log_info("Normalized loaded rubble records", detail, 0);
    }
}

static int building_promote_legacy_tile_buildings_after_load()
{
    const LegacyTilePromotionTypes types = legacy_tile_promotion_types();
    int promoted = 0;
    bool promoted_garden_tiles = false;
    bool promoted_road_tiles = false;
    bool promoted_highway_tiles = false;
    bool promoted_plaza_tiles = false;
    bool promoted_aqueduct_tiles = false;
    bool promoted_wall_tiles = false;
    bool promoted_rubble_tiles = false;

    for (int grid_offset = 0; grid_offset < GRID_SIZE * GRID_SIZE; grid_offset++) {
        const building_type type = legacy_tile_type_for_offset(grid_offset, types);
        if (type == BUILDING_NONE) {
            continue;
        }
        if (loaded_record_owns_surface_tile(grid_offset, type)) {
            continue;
        }

        const int state = legacy_tile_promoted_state(type, grid_offset, types);
        unsigned int rubble_source_id = 0;
        const RubbleState rubble_origin = type == types.burning_ruin ?
            legacy_rubble_origin(grid_offset, &rubble_source_id) : RubbleState{};
        building *record = append_legacy_tile_building_record(
            type,
            grid_offset,
            state,
            type == types.burning_ruin ? &rubble_origin : nullptr);
        if (!record) {
            continue;
        }
        bind_legacy_tile_building_record_to_map(record, types);
        if (rubble_source_id && rubble_source_id < data.buildings.size() &&
            data.buildings[rubble_source_id].state == BUILDING_STATE_RUBBLE) {
            data.buildings[rubble_source_id].state = BUILDING_STATE_DELETED_BY_GAME;
        }
        promoted++;
        promoted_garden_tiles = promoted_garden_tiles || type == types.gardens || type == types.overgrown_gardens;
        promoted_road_tiles = promoted_road_tiles || type == types.road;
        promoted_highway_tiles = promoted_highway_tiles || type == types.highway;
        promoted_plaza_tiles = promoted_plaza_tiles || type == types.plaza;
        promoted_aqueduct_tiles = promoted_aqueduct_tiles || type == types.aqueduct;
        promoted_wall_tiles = promoted_wall_tiles || type == types.wall;
        promoted_rubble_tiles = promoted_rubble_tiles || type == types.burning_ruin;
    }

    if (promoted) {
        rebuild_loaded_record_type_links();
        if (promoted_garden_tiles) {
            map_tiles_update_all_gardens();
        }
        if (promoted_road_tiles || promoted_plaza_tiles || promoted_aqueduct_tiles) {
            map_tiles_update_all_roads();
        }
        if (promoted_highway_tiles) {
            map_tiles_update_all_highways();
        }
        if (promoted_plaza_tiles) {
            map_tiles_update_all_plazas();
        }
        if (promoted_aqueduct_tiles) {
            map_tiles_update_all_aqueducts(0);
        }
        if (promoted_wall_tiles) {
            map_tiles_update_all_walls();
        }
        if (promoted_rubble_tiles) {
            map_tiles_update_all_rubble();
        }
    }
    return promoted;
}

void building_load_state(buffer *buf, buffer *sequence, buffer *corrupt_houses, int save_version)
{
    int building_buf_size = BUILDING_STATE_ORIGINAL_BUFFER_SIZE;
    size_t buf_size = buf->size;

    if (save_version > SAVE_GAME_LAST_STATIC_VERSION) {
        building_buf_size = buffer_read_i32(buf);
        buf_size -= 4;
    }

    int buildings_to_load = (int) buf_size / building_buf_size;

    data.buildings.clear();
    resize_buildings(buildings_to_load > 0 ? buildings_to_load : 1);
    building_runtime_begin_load_bridge(buildings_to_load > 0 ? buildings_to_load : 1);

    memset(data.first_of_type, 0, sizeof(data.first_of_type));
    memset(data.last_of_type, 0, sizeof(data.last_of_type));

    int highest_id_in_use = 0;

    for (int i = 0; i < buildings_to_load; i++) {
        building *b = &data.buildings[i];
        building_state_load_from_buffer(buf, b, building_buf_size, save_version, 0);
        if (b->state != BUILDING_STATE_UNUSED) {
            highest_id_in_use = i;
            fill_adjacent_types(b);
        }
    }

    // Fix messy old hack that assigned gardens to building 0
    building *b = first_building_slot();
    if (b->state == BUILDING_STATE_UNUSED && type_attr_is(b->type, "gardens")) {
        b->type = BUILDING_NONE;
    }

    resize_buildings(highest_id_in_use + 1);

    extra.created_sequence = buffer_read_i32(sequence);

    extra.incorrect_houses = buffer_read_i32(corrupt_houses);
    extra.unfixable_houses = buffer_read_i32(corrupt_houses);
    normalize_loaded_surface_records();
    building_promote_legacy_tile_buildings_after_load();
    clear_loaded_unbound_foundation_bindings();
    normalize_loaded_rubble_records();
}
