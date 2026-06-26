#include "building/building_type.h"
#include "building/housing_type.h"
#include "building/clone.h"
#include "building/construction.h"
#include "building/construction_building.h"
#include "building/construction_clear.h"
#include "building/data_transfer.h"
#include "building/distribution.h"
#include "building/image.h"
#include "building/industry.h"
#include "building/roadblock.h"
#include "building/rotation.h"
#include "building/state.h"
#include "building/storage.h"
#include "building/storage_runtime.h"
#include "building/variant.h"
#include "city/culture.h"
#include "city/warning.h"
#include "game/undo.h"
#include "map/bridge.h"
#include "map/building.h"
#include "map/building_tiles.h"
#include "map/road_access.h"
#include "map/tiles.h"
#include "map/water.h"
#include "map/water_supply.h"

#include "building.h"

#include "building/animations.h"
#include "building/building_record.h"
#include "building/building_runtime_internal.h"
#include "building/dock.h"
#include "building/building_type_registry_internal.h"
#include "building/production_method.h"
#include "building/production_runtime.h"
#include "core/crash_context.h"
#include "figure/figure.h"
#include "figure/formation_legion.h"

#include <algorithm>
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <deque>
#include <exception>
#include <initializer_list>
#include <source_location>
#include <vector>

#include "building/destruction.h"
#include "building/building_type_api.h"
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
#include "map/property.h"
#include "map/random.h"
#include "map/routing_terrain.h"
#include "map/terrain.h"

#define WATER_DESIRABILITY_RANGE 3
#define WATER_DESIRABILITY_BONUS 15

static struct {
    std::deque<building> buildings;
    building *first_of_type[BUILDING_TYPE_MAX];
    building *last_of_type[BUILDING_TYPE_MAX];
} data;

static building_type runtime_type(const char *text_id)
{
    return building_type_registry_impl::type_from_attr(text_id);
}

static int type_matches(building_type type, const char *text_id)
{
    building_type resolved = runtime_type(text_id);
    return resolved != BUILDING_NONE && type == resolved;
}

static int type_matches_any(building_type type, std::initializer_list<const char *> text_ids)
{
    for (const char *text_id : text_ids) {
        if (type_matches(type, text_id)) {
            return 1;
        }
    }
    return 0;
}

static int building_matches(const building *b, const char *text_id)
{
    return b && type_matches(b->type, text_id);
}

static int original_type_matches(const building *b, const char *text_id)
{
    return b && type_matches(static_cast<building_type>(b->data.rubble.og_type), text_id);
}

static const building_type_registry_impl::BuildingType *definition_for_type(building_type type)
{
    return building_type_registry_impl::definition_for_type(type);
}

static int definition_registers_water_footprint(const building_type_registry_impl::BuildingType *definition)
{
    return definition && definition->foundation().policy_type() == building_type_registry_impl::FoundationPolicy::Shoreline;
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
    for (const building_type_registry_impl::ComposedPartDefinition &part : definition->composition().parts()) {
        const building_type_registry_impl::BuildingType *part_definition = definition_for_type(part.type);
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

static building *first_building_slot()
{
    return data.buildings.empty() ? nullptr : &data.buildings.front();
}

static int building_slot_is_active(const building &record)
{
    return record.state != BUILDING_STATE_UNUSED || game_undo_contains_building(record.id);
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
        "runtime_save_id=%u legacy_text_id=%s registry_has_definition=%d x=%d y=%d grid_offset=%d size=%d "
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
        building_type_registry_has_definition(runtime_type),
        record ? record->x : 0,
        record ? record->y : 0,
        record ? record->grid_offset : 0,
        record ? record->size : 0,
        record ? record->prev_part_building_id : 0,
        record ? record->next_part_building_id : 0,
        record ? record->is_deleted : 0);

    error_context_report_fatal_error_dialog(
        "Building runtime error",
        "Building has no type definition.",
        detail);
    std::terminate();
}

Building::Building(::building *record, const std::source_location &location)
    : Building(record, record ? building_type_registry_impl::definition_for_type(record->type) : nullptr, location)
{}

Building::Building(::building &record, const std::source_location &location)
    : Building(&record, location)
{}

Building::Building(
    ::building *record,
    const building_type_registry_impl::BuildingType *type_definition,
    const std::source_location &location)
    : type(type_definition),
    record_(record)
{
    if (building_record_requires_type_definition(record_) && !type) {
        report_missing_building_type_definition(record_, location, "constructor");
    }
}

Building::Building(
    ::building &record,
    const building_type_registry_impl::BuildingType *type_definition,
    const std::source_location &location)
    : Building(&record, type_definition, location)
{}

Building::TypeRange::iterator::iterator(::building *record)
    : record_(record)
{}

Building Building::TypeRange::iterator::operator*() const
{
    return Building(record_);
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
    return iterator(building_first_of_type(type_));
}

Building::TypeRange::iterator Building::TypeRange::end() const
{
    return iterator(nullptr);
}

Building::TypeRange Building::of_type(building_type type)
{
    return TypeRange(type);
}

Building Building::first_of_type(building_type type)
{
    return Building(building_first_of_type(type));
}

Building Building::create(building_type type, int x, int y)
{
    return Building(building_create(type, x, y));
}

int Building::count()
{
    return building_count();
}

unsigned int Building::id() const
{
    return record_ ? record_->id : 0;
}

const ::building *Building::record() const
{
    return record_;
}

Building Building::main() const
{
    if (!record_) {
        return Building(nullptr);
    }
    if (!record_->id) {
        return Building(record_, type);
    }
    return Building(building_main(record_));
}

Building Building::main_part() const
{
    return main();
}

Building Building::next() const
{
    return Building(record_ ? building_next(record_) : nullptr);
}

void Building::for_each_part(const std::function<void(Building)> &visitor) const
{
    Building part = main_part();
    for (int guard = 0; part.id() && guard < 64; guard++) {
        visitor(part);
        if (!part.next_part_id()) {
            break;
        }
        part = part.next();
    }
}

Building Building::next_of_type() const
{
    return Building(record_ ? record_->next_of_type : nullptr);
}

int Building::grid_offset() const
{
    return record_ ? record_->grid_offset : 0;
}

int Building::x() const
{
    return record_ ? record_->x : 0;
}

int Building::y() const
{
    return record_ ? record_->y : 0;
}

int Building::size() const
{
    return record_ ? record_->size : 0;
}

int Building::previous_part_id() const
{
    return record_ ? record_->prev_part_building_id : 0;
}

int Building::next_part_id() const
{
    return record_ ? record_->next_part_building_id : 0;
}

int Building::is_main_part() const
{
    return !previous_part_id();
}

int Building::road_network_id() const
{
    return record_ ? record_->road_network_id : 0;
}

int Building::distance_from_entry() const
{
    return record_ ? record_->distance_from_entry : 0;
}

void Building::set_distance_from_entry(int value)
{
    if (record_) {
        record_->distance_from_entry = value;
    }
}

int Building::road_access_x() const
{
    return record_ ? record_->road_access_x : 0;
}

int Building::road_access_y() const
{
    return record_ ? record_->road_access_y : 0;
}

int Building::state_id() const
{
    return record_ ? record_->state : BUILDING_STATE_UNUSED;
}

int Building::formation_id() const
{
    return record_ ? record_->formation_id : 0;
}

void Building::set_formation_id(int formation_id)
{
    if (record_) {
        record_->formation_id = formation_id;
    }
}

int Building::is_deleted() const
{
    return record_ ? record_->is_deleted : 0;
}

int Building::is_in_use() const
{
    return record_ && record_->state == BUILDING_STATE_IN_USE;
}

int Building::is_mothballed() const
{
    return record_ && record_->state == BUILDING_STATE_MOTHBALLED;
}

int Building::has_plague() const
{
    return record_ && record_->has_plague;
}

int Building::has_cached_road_access() const
{
    return record_ && record_->has_road_access;
}

int Building::is_close_to_water() const
{
    return record_ ? building_is_close_to_water(record_) : 0;
}

int Building::has_house_size() const
{
    return record_ && record_->house_size;
}

int Building::house_population() const
{
    return record_ ? record_->house_population : 0;
}

void Building::set_house_population(int value)
{
    if (record_) {
        record_->house_population = value;
    }
}

int Building::house_population_room() const
{
    return record_ ? record_->house_population_room : 0;
}

void Building::set_house_population_room(int value)
{
    if (record_) {
        record_->house_population_room = value;
    }
}

unsigned int Building::immigrant_figure_id() const
{
    return record_ ? record_->immigrant_figure_id : 0;
}

void Building::set_immigrant_figure_id(unsigned int id)
{
    if (record_) {
        record_->immigrant_figure_id = id;
    }
}

int Building::house_figure_generation_delay() const
{
    return record_ ? record_->house_figure_generation_delay : 0;
}

building_runtime *Building::runtime_instance() const
{
    return record_ && record_->id ? building_runtime_impl::get_or_create_instance(record_) : nullptr;
}

building_type_registry_impl::BuildingAnimation Building::animate()
{
    return building_type_registry_impl::BuildingAnimation(*this);
}

int Building::draw_footprint(const BuildingDrawContext &ctx)
{
    return building_record_requires_type_definition(record_) && type ? type->graphics().draw_footprint(*this, ctx) : 0;
}

int Building::draw_top(const BuildingDrawContext &ctx)
{
    return building_record_requires_type_definition(record_) && type ? type->graphics().draw_top(*this, ctx) : 0;
}

int Building::draw_animation(const BuildingDrawContext &ctx)
{
    return building_record_requires_type_definition(record_) && type ? type->graphics().draw_animation(*this, ctx) : 0;
}

void Building::refresh_graphic()
{
    if (record_) {
        if (building_runtime *instance = building_runtime_impl::get_or_create_instance(record_)) {
            instance->set_building_graphic();
        }
    }
}

int Building::refresh_graphic_if_native()
{
    if (!record_) {
        return 0;
    }
    if (building_runtime *instance = building_runtime_impl::get_or_create_instance(record_)) {
        if (instance->uses_new_graphics() &&
            (record_->state == BUILDING_STATE_CREATED ||
                record_->state == BUILDING_STATE_IN_USE ||
                record_->state == BUILDING_STATE_MOTHBALLED)) {
            instance->set_building_graphic();
            return 1;
        }
    }
    return 0;
}

void Building::assign_graphic_variant(int force_reseed)
{
    if (record_) {
        if (building_runtime *instance = building_runtime_impl::get_or_create_instance(record_)) {
            instance->assign_graphic_variant(force_reseed);
        }
    }
}

void Building::spawn_figure()
{
    Building owner = main();
    if (owner.record_) {
        if (building_runtime *instance = building_runtime_impl::get_or_create_instance(owner.record_)) {
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

    int workers = 0;
    int farm_part_count = 0;
    for_each_part([&](Building part) {
        if (!part.record_) {
            return;
        }
        if (part.id() == main_part().id()) {
            workers += part.record_->num_workers;
            return;
        }
        if (part.type && part.type->is_farm()) {
            workers += part.record_->num_workers;
            farm_part_count++;
        }
    });
    return farm_part_count > 0 ? workers : record_->num_workers;
}

int Building::employment_required_workers() const
{
    if (!record_ || !type) {
        return 0;
    }

    int workers = 0;
    int farm_part_count = 0;
    for_each_part([&](Building part) {
        if (!part.type) {
            return;
        }
        if (part.id() == main_part().id()) {
            workers += part.type->required_workers();
            return;
        }
        if (part.type->is_farm()) {
            workers += part.type->required_workers();
            farm_part_count++;
        }
    });
    return farm_part_count > 0 ? workers : type->required_workers();
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

static int composed_record_uses_union_road_access(const building *owner)
{
    const building_type_registry_impl::BuildingType *definition =
        owner ? building_type_registry_impl::definition_for_type(owner->type) : nullptr;
    return definition && definition->has_composition() &&
        !definition->is_warehouse() &&
        !definition->composition().child_inherits_orientation() &&
        !building_is_fort(owner->type);
}

static int composed_record_road_access_area(const building *owner, int *x, int *y, int *size)
{
    if (!owner || !x || !y || !size || !composed_record_uses_union_road_access(owner)) {
        return 0;
    }

    int min_x = owner->x;
    int min_y = owner->y;
    int max_x = owner->x + owner->size;
    int max_y = owner->y + owner->size;
    int saw_part = 0;
    const building *part = owner;
    for (int guard = 0; part && guard < 64; guard++) {
        if (part != owner) {
            saw_part = 1;
        }
        if (part->x < min_x) {
            min_x = part->x;
        }
        if (part->y < min_y) {
            min_y = part->y;
        }
        if (part->x + part->size > max_x) {
            max_x = part->x + part->size;
        }
        if (part->y + part->size > max_y) {
            max_y = part->y + part->size;
        }
        if (part->next_part_building_id <= 0) {
            break;
        }
        part = building_get(part->next_part_building_id);
    }

    if (!saw_part) {
        const building_type_registry_impl::BuildingType *definition =
            building_type_registry_impl::definition_for_type(owner->type);
        const building_type_registry_impl::ComposedBuildingDefinition &composition = definition->composition();
        int rotation = owner->subtype.orientation % 4;
        if (rotation < 0) {
            rotation += 4;
        }
        const building_type_registry_impl::ComposedPartOffset main_offset =
            composition.main_offset_for_rotation(rotation);
        min_x = owner->x - main_offset.x;
        min_y = owner->y - main_offset.y;
        const int width = rotation % 2 ? composition.footprint_height() : composition.footprint_width();
        const int height = rotation % 2 ? composition.footprint_width() : composition.footprint_height();
        max_x = min_x + width;
        max_y = min_y + height;
    }

    const int width = max_x - min_x;
    const int height = max_y - min_y;
    *x = min_x;
    *y = min_y;
    *size = width > height ? width : height;
    return *size > 0;
}

int Building::has_road_access(map_point *road) const
{
    building *owner = record_ ? building_main(record_) : nullptr;
    int access_x = 0;
    int access_y = 0;
    int access_size = 0;
    if (composed_record_road_access_area(owner, &access_x, &access_y, &access_size)) {
        return map_has_road_access(access_x, access_y, access_size, road);
    }
    return owner && map_has_road_access(owner->x, owner->y, owner->size, road);
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

    const building_type_registry_impl::BuildingType *definition = type;
    if (!definition) {
        return worker_count() > 0;
    }
    if (definition->required_workers() > 0 && !worker_count()) {
        return 0;
    }
    if (definition->water_access().has_requirements() && !has_water_access()) {
        return 0;
    }
    return 1;
}

int Building::is_merged_house() const
{
    return record_ ? record_->house_is_merged : 0;
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

unsigned int Building::distribution_cartpusher_id(int index) const
{
    if (!record_ || index < 0 || index >= 3) {
        return 0;
    }
    return record_->data.distribution.cartpusher_ids[index];
}

int Building::resource_amount(resource_type resource) const
{
    return record_ && resource >= RESOURCE_NONE && resource < RESOURCE_SLOT_COUNT ? record_->resources[resource] : 0;
}

void Building::add_resource(resource_type resource, int amount)
{
    if (record_ && resource >= RESOURCE_NONE && resource < RESOURCE_SLOT_COUNT) {
        record_->resources[resource] += amount;
    }
}

void Building::set_resource_amount(resource_type resource, int amount)
{
    if (record_ && resource >= RESOURCE_NONE && resource < RESOURCE_SLOT_COUNT) {
        record_->resources[resource] = static_cast<short>(amount);
    }
}

int Building::add_storage_resource(
    resource_type resource, int amount, building_type_registry_impl::StorageRole role)
{
    if (!record_ || resource == RESOURCE_NONE || resource < RESOURCE_NONE || resource >= RESOURCE_SLOT_COUNT) {
        return 0;
    }

    const size_t slot_count = storage_runtime_impl::get_slot_count(record_);
    for (size_t i = 0; i < slot_count; i++) {
        BuildingStorage *storage = storage_runtime_impl::get_or_create(record_, i);
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
    const size_t slot_count = storage_runtime_impl::get_slot_count(record_);
    for (size_t i = 0; i < slot_count; i++) {
        BuildingStorage *storage = storage_runtime_impl::get_or_create(record_, i);
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
    const size_t slot_count = storage_runtime_impl::get_slot_count(record_);
    for (size_t i = 0; i < slot_count; i++) {
        BuildingStorage *storage = storage_runtime_impl::get_or_create(record_, i);
        if (storage && storage->type() && storage->type()->is_input()) {
            space += storage->available_space(resource);
        }
    }
    return space;
}

int Building::reserve_input_storage_load(resource_type resource, unsigned int figure_id)
{
    if (!record_ || resource == RESOURCE_NONE || resource < RESOURCE_NONE || resource >= RESOURCE_SLOT_COUNT) {
        return 0;
    }

    const size_t slot_count = storage_runtime_impl::get_slot_count(record_);
    for (size_t i = 0; i < slot_count; i++) {
        BuildingStorage *storage = storage_runtime_impl::get_or_create(record_, i);
        if (storage && storage->reserve_inbound_load(resource, figure_id)) {
            return 1;
        }
    }
    return 0;
}

void Building::release_input_storage_reservation(unsigned int figure_id)
{
    if (!record_ || !figure_id) {
        return;
    }

    const size_t slot_count = storage_runtime_impl::get_slot_count(record_);
    for (size_t i = 0; i < slot_count; i++) {
        if (BuildingStorage *storage = storage_runtime_impl::get_or_create(record_, i)) {
            storage->release_inbound(figure_id);
        }
    }
}

int Building::receive_input_storage_loads(resource_type resource, int loads, unsigned int figure_id)
{
    if (!record_ || resource == RESOURCE_NONE || resource < RESOURCE_NONE || resource >= RESOURCE_SLOT_COUNT) {
        return 0;
    }

    const size_t slot_count = storage_runtime_impl::get_slot_count(record_);
    for (size_t i = 0; i < slot_count; i++) {
        BuildingStorage *storage = storage_runtime_impl::get_or_create(record_, i);
        if (storage && storage->handles_resource(resource)) {
            const int received = storage->receive_inbound_loads(resource, loads, figure_id);
            if (received > 0) {
                return received;
            }
        }
    }
    return 0;
}

int Building::house_happiness() const
{
    return record_ ? record_->sentiment.house_happiness : 0;
}

void Building::set_house_happiness(int value)
{
    if (record_) {
        record_->sentiment.house_happiness = static_cast<signed char>(value);
    }
}

resource_type Building::fetch_inventory_id() const
{
    return record_ ? static_cast<resource_type>(record_->data.market.fetch_inventory_id) : RESOURCE_NONE;
}

void Building::set_fetch_inventory_id(resource_type resource)
{
    if (record_) {
        record_->data.market.fetch_inventory_id = static_cast<unsigned char>(resource);
    }
}

int Building::accepts_good(resource_type resource) const
{
    return record_ && resource >= RESOURCE_NONE && resource < RESOURCE_SLOT_COUNT ? record_->accepted_goods[resource] : 0;
}

void Building::set_accepted_good(resource_type resource, int value)
{
    if (record_ && resource >= RESOURCE_NONE && resource < RESOURCE_SLOT_COUNT) {
        record_->accepted_goods[resource] = static_cast<unsigned char>(value);
    }
}

void Building::toggle_accepted_good(resource_type resource)
{
    if (record_ && resource >= RESOURCE_NONE && resource < RESOURCE_SLOT_COUNT) {
        record_->accepted_goods[resource] ^= 1;
    }
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

void Building::set_primary_figure_id(unsigned int id)
{
    if (record_) {
        record_->figure_id = id;
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
        record_->subtype.orientation = orientation;
    }
}

int Building::variant() const
{
    return record_ ? record_->variant : 0;
}

void Building::set_variant(int variant)
{
    if (record_) {
        record_->variant = static_cast<unsigned char>(variant);
    }
}

int Building::image_id() const
{
    return record_ ? building_image_get(record_) : 0;
}

void Building::add_map_tiles(int image_id) const
{
    if (record_) {
        const char *attr = type ? type->attr() : nullptr;
        if (definition_registers_water_footprint(type)) {
            map_water_add_building(record_->id, record_->x, record_->y, record_->size, image_id);
            return;
        }
        int terrain = TERRAIN_BUILDING;
        if (attr && !std::strcmp(attr, "wall")) {
            terrain |= TERRAIN_WALL;
        }
        if (type && type->roadblock().kind() != building_type_registry_impl::RoadblockKind::None &&
            type->roadblock().kind() != building_type_registry_impl::RoadblockKind::Bridge) {
            terrain |= TERRAIN_ROAD;
        }
        map_building_tiles_add(record_->id, record_->x, record_->y, record_->size, image_id, terrain);
    }
}

int Building::storage_id() const
{
    return record_ ? record_->storage_id : 0;
}

void Building::set_storage_id(int storage_id)
{
    if (record_) {
        record_->storage_id = storage_id;
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
    return record_ ? building_loads_stored(record_) : 0;
}

int Building::industry_has_raw_materials() const
{
    return record_ ? record_->data.industry.has_raw_materials : 0;
}

int Building::dock_has_accepted_route_ids() const
{
    return record_ ? record_->data.dock.has_accepted_route_ids : 0;
}

int Building::dock_accepted_route_ids() const
{
    return record_ ? record_->data.dock.accepted_route_ids : 0;
}

int Building::dock_trade_ship_id() const
{
    return record_ ? record_->data.dock.trade_ship_id : 0;
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
    if (record_) {
        record_->has_water_access = static_cast<unsigned char>(value);
    }
}

void Building::set_dock_accepted_route_ids(int has_route_ids, int route_ids)
{
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
            building_get_required_raw_amount_for_production(record_->type, resource) :
        0;
}

int Building::has_native_production() const
{
    return production_runtime_impl::get_or_create_primary(*this) ? 1 : 0;
}

int Building::native_production_method_count() const
{
    return static_cast<int>(production_runtime_impl::get_method_count(*this));
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

int Building::reserve_output_storage_loads(resource_type *out_resource, int *out_loads)
{
    if (!record_ || !type) {
        return 0;
    }

    const size_t slot_count = storage_runtime_impl::get_slot_count(record_);
    for (size_t i = 0; i < slot_count; i++) {
        BuildingStorage *storage = storage_runtime_impl::get_or_create(record_, i);
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

void Building::change_type(building_type type, const std::source_location &location)
{
    if (record_) {
        building_change_type(record_, type);
        this->type = building_type_registry_impl::definition_for_type(record_->type);
        if (building_record_requires_type_definition(record_) && !this->type) {
            report_missing_building_type_definition(record_, location, "change_type()");
        }
    }
}

int Building::configure_house_replacement(building_type type, int x, int y, int size, int merged)
{
    if (!record_ || size <= 0) {
        return 0;
    }
    const building_type_registry_impl::BuildingType *definition =
        building_type_registry_impl::definition_for_type(type);
    const int level = definition && definition->has_housing() ? definition->housing_type()->level() : -1;
    if (level < 0) {
        return 0;
    }
    change_type(type);
    record_->subtype.house_level = level;
    record_->state = BUILDING_STATE_IN_USE;
    record_->x = static_cast<unsigned char>(x);
    record_->y = static_cast<unsigned char>(y);
    record_->grid_offset = map_grid_offset(x, y);
    record_->size = record_->house_size = static_cast<unsigned char>(size);
    record_->house_is_merged = static_cast<unsigned char>(merged);
    record_->is_close_to_water = building_is_close_to_water(record_);
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
    record_->data.house = source.record_->data.house;
    record_->sentiment.house_happiness = source.record_->sentiment.house_happiness;
    record_->distance_from_entry = source.record_->distance_from_entry;
    record_->road_network_id = source.record_->road_network_id;
    record_->road_access_x = source.record_->road_access_x;
    record_->road_access_y = source.record_->road_access_y;
    record_->has_road_access = source.record_->has_road_access;
    record_->has_water_access = source.record_->has_water_access;
    record_->has_well_access = source.record_->has_well_access;
    record_->house_tax_coverage = source.record_->house_tax_coverage;
    record_->house_tavern_wine_access = source.record_->house_tavern_wine_access;
    record_->house_tavern_food_access = source.record_->house_tavern_food_access;
    record_->house_arena_gladiator = source.record_->house_arena_gladiator;
    record_->house_arena_lion = source.record_->house_arena_lion;
    record_->has_latrines_access = source.record_->has_latrines_access;
    record_->house_days_without_food = source.record_->house_days_without_food;
    record_->desirability = source.record_->desirability;
    record_->house_figure_generation_delay = source.record_->house_figure_generation_delay;
    record_->variant = source.record_->variant;
}

void Building::retire_replaced_house()
{
    if (!record_) {
        return;
    }
    record_->house_population = 0;
    record_->house_population_room = 0;
    record_->local_workforce_assigned = 0;
    record_->local_workforce_unemployed = 0;
    record_->local_workforce_validation_delay = 0;
    record_->figure_id = 0;
    record_->figure_id2 = 0;
    record_->immigrant_figure_id = 0;
    record_->figure_id4 = 0;
    for (resource_type r = RESOURCE_NONE; r < RESOURCE_SLOT_COUNT; r = static_cast<resource_type>(r + 1)) {
        record_->resources[r] = 0;
    }
    record_->state = BUILDING_STATE_DELETED_BY_GAME;
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
        record_->fumigation_direction = direction;
    }
}

int Building::fort_figure_type() const
{
    return record_ ? record_->subtype.fort_figure_type : 0;
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

std::uint64_t Building::graphics_state_signature(int selected_graphics_option) const
{
    if (!record_) {
        return 0;
    }

    std::uint64_t hash = 1469598103934665603ull;
    const auto mix = [&hash](std::uint64_t value) {
        hash ^= value;
        hash *= 1099511628211ull;
    };

    mix(static_cast<std::uint64_t>(record_->state));
    mix(static_cast<std::uint64_t>(record_->num_workers));
    mix(static_cast<std::uint64_t>(record_->has_water_access));
    mix(static_cast<std::uint64_t>(record_->desirability));
    mix(static_cast<std::uint64_t>(record_->strike_duration_days));
    mix(static_cast<std::uint64_t>(record_->data.industry.progress));
    mix(static_cast<std::uint64_t>(record_->data.industry.has_raw_materials));
    mix(static_cast<std::uint64_t>(record_->output_resource_id));
    mix(static_cast<std::uint64_t>(record_->figure_id));
    mix(static_cast<std::uint64_t>(record_->figure_id2));
    mix(static_cast<std::uint64_t>(record_->figure_id4));
    mix(static_cast<std::uint64_t>(record_->monument.phase));
    mix(static_cast<std::uint64_t>(record_->monument.upgrades));
    mix(static_cast<std::uint64_t>(record_->variant));
    if (selected_graphics_option >= 0) {
        mix(static_cast<std::uint64_t>(selected_graphics_option));
    }

    for (int i = 0; i < RESOURCE_SLOT_COUNT; i++) {
        mix(static_cast<std::uint64_t>(record_->resources[i]));
    }

    return hash;
}

building *building_get(unsigned int id)
{
    return building_slot(id);
}

int building_can_repair_type(building_type type)
{
    if (building_monument_is_limited(type) || type_matches(type, "aqueduct") || building_is_fort(type)) {
        return 0; // limited monuments and aqueducts cannot be repaired at the moment, aqueducts require a rework,
    }   //and limited monuments are too complex to easily repair, and arent a common occurrence
    // forts have the complexity of holding formations, so are also currently excluded
    building_type repair_type = building_clone_type_from_building_type(type);
    if (repair_type == BUILDING_NONE) {
        return 0;
    } else {
        return 1;
    }
}
int building_dist(int x, int y, int w, int h, building *b)
{
    int size = building_properties_for_type(b->type)->size;
    int dist = calc_box_distance(x, y, w, h, b->x, b->y, size, size);
    return dist;
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

int building_find(building_type type)
{
    for (building *b = data.first_of_type[type]; b; b = b->next_of_type) {
        if (b->state == BUILDING_STATE_IN_USE) {
            return b->id;
        }
    }
    return 0;
}

int building_find_with_mothballed(building_type type)
{
    for (building *b = data.first_of_type[type]; b; b = b->next_of_type) {
        if (b->state == BUILDING_STATE_IN_USE || b->state == BUILDING_STATE_MOTHBALLED) {
            return b->id;
        }
    }
    return 0;
}

building *building_first_of_type(building_type type)
{
    return data.first_of_type[type];
}

building *building_main(const building *b)
{
    if (!b) {
        return nullptr;
    }
    building *part = building_get(b->id);
    for (int guard = 0; guard < 64; guard++) {
        if (!part || part->prev_part_building_id <= 0) {
            return part;
        }
        part = building_get(part->prev_part_building_id);
    }
    return first_building_slot();
}

building *building_next(building *b)
{
    return building_get(b->next_part_building_id);
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

static building *chain_child_after(building *previous)
{
    if (!previous || previous->next_part_building_id <= 0) {
        return nullptr;
    }
    building *child = building_get(previous->next_part_building_id);
    if (!composed_record_is_live(child) || child->id == previous->id) {
        return nullptr;
    }
    return child;
}

static int composed_rotation_fallback(const building *main_record,
    const building_type_registry_impl::BuildingType &definition)
{
    if (definition.is_warehouse() || definition.composition().child_inherits_orientation()) {
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

    const building_type_registry_impl::ComposedBuildingDefinition &composition = definition.composition();
    int best_rotation = composed_rotation_fallback(main_record, definition);
    int best_score = -1;

    for (int rotation = 0; rotation < 4; rotation++) {
        const building_type_registry_impl::ComposedPartOffset main_offset =
            composition.main_offset_for_rotation(rotation);
        const int origin_x = static_cast<int>(main_record->x) - main_offset.x;
        const int origin_y = static_cast<int>(main_record->y) - main_offset.y;
        int score = 0;
        building *previous = main_record;

        for (const building_type_registry_impl::ComposedPartDefinition &part : composition.parts()) {
            building *child = chain_child_after(previous);
            if (!child) {
                break;
            }

            const building_type_registry_impl::ComposedPartOffset offset = part.offset_for_rotation(rotation);
            if (offset.has_value &&
                child->x == origin_x + offset.x &&
                child->y == origin_y + offset.y) {
                score += 4;
            }
            if (child->type == part.type) {
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

static void initialize_loaded_composed_child(building *main_record, building *child,
    const building_type_registry_impl::BuildingType &main_definition,
    const building_type_registry_impl::ComposedPartDefinition &part, int was_created)
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
    child->variant = main_record->variant;

    if (main_definition.composition().child_inherits_orientation()) {
        child->subtype.orientation = main_record->subtype.orientation;
    }
    if (building_is_fort(main_record->type)) {
        child->formation_id = main_record->formation_id;
    }
    if (was_created && part.role.find("field") == 0) {
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

static void publish_loaded_composed_record(building *record)
{
    if (!record || !map_grid_is_inside(record->x, record->y, record->size)) {
        return;
    }
    Building building_object(record);
    if (!building_object.refresh_graphic_if_native()) {
        building_object.add_map_tiles(building_image_get(record));
    }
}

static void normalize_loaded_composed_main(building *main_record,
    const building_type_registry_impl::BuildingType &definition, int origin_x, int origin_y,
    const building_type_registry_impl::ComposedPartOffset &main_offset)
{
    if (!main_record) {
        return;
    }
    const building_properties *props = building_properties_for_type(main_record->type);
    if (!props) {
        return;
    }

    const int expected_x = origin_x + main_offset.x;
    const int expected_y = origin_y + main_offset.y;
    const int old_x = main_record->x;
    const int old_y = main_record->y;
    const int needs_republish = main_record->x != expected_x ||
        main_record->y != expected_y ||
        main_record->size != props->size;

    if (needs_republish) {
        map_building_tiles_remove(main_record->id, old_x, old_y);
    }

    main_record->size = props->size;
    main_record->x = static_cast<unsigned char>(expected_x);
    main_record->y = static_cast<unsigned char>(expected_y);
    main_record->grid_offset = map_grid_offset(expected_x, expected_y);
    main_record->house_size = building_type_registry_has_housing(main_record->type) ? props->size : 0;
    main_record->output_resource_id = building_output_resource(main_record->type);
    main_record->prev_part_building_id = 0;

    if (definition.is_warehouse() && !main_record->storage_id) {
        main_record->storage_id = building_storage_create(main_record->id);
    }
}

static building *repair_loaded_composed_child(building *main_record, building *previous,
    const building_type_registry_impl::BuildingType &main_definition,
    const building_type_registry_impl::ComposedPartDefinition &part, int expected_x, int expected_y)
{
    const building_properties *props = part.type == BUILDING_NONE ? nullptr : building_properties_for_type(part.type);
    if (!main_record || !previous || !props ||
        !map_grid_is_inside(expected_x, expected_y, props->size)) {
        return previous;
    }

    building *child = chain_child_after(previous);
    int was_created = 0;
    if (!child) {
        child = building_create(part.type, expected_x, expected_y);
        was_created = 1;
    }
    if (!composed_record_is_live(child)) {
        return previous;
    }

    const int old_x = child->x;
    const int old_y = child->y;
    const int needs_republish = child->type != part.type ||
        child->x != expected_x ||
        child->y != expected_y ||
        child->size != props->size;

    if (!was_created && needs_republish) {
        map_building_tiles_remove(child->id, old_x, old_y);
    }

    if (child->type != part.type) {
        building_change_type(child, part.type);
    }
    child->size = props->size;
    child->x = static_cast<unsigned char>(expected_x);
    child->y = static_cast<unsigned char>(expected_y);
    child->grid_offset = map_grid_offset(expected_x, expected_y);
    child->house_size = 0;
    child->output_resource_id = building_output_resource(part.type);
    if (was_created) {
        child->state = main_record->state;
    } else if (child->state != BUILDING_STATE_IN_USE &&
        child->state != BUILDING_STATE_MOTHBALLED &&
        child->state != BUILDING_STATE_CREATED) {
        child->state = main_record->state;
    }
    child->prev_part_building_id = previous->id;
    previous->next_part_building_id = child->id;

    initialize_loaded_composed_child(main_record, child, main_definition, part, was_created);
    publish_loaded_composed_record(child);
    return child;
}

static void repair_loaded_composed_main(building *main_record)
{
    if (!composed_record_is_live(main_record) || main_record->prev_part_building_id > 0) {
        return;
    }

    const building_type_registry_impl::BuildingType *definition =
        building_type_registry_impl::definition_for_type(main_record->type);
    if (!definition || !definition->has_composition()) {
        return;
    }

    const int rotation = infer_loaded_composed_rotation(main_record, *definition);
    const building_type_registry_impl::ComposedBuildingDefinition &composition = definition->composition();
    const building_type_registry_impl::ComposedPartOffset main_offset =
        composition.main_offset_for_rotation(rotation);
    const int origin_x = static_cast<int>(main_record->x) - main_offset.x;
    const int origin_y = static_cast<int>(main_record->y) - main_offset.y;

    normalize_loaded_composed_main(main_record, *definition, origin_x, origin_y, main_offset);
    publish_loaded_composed_record(main_record);

    building *previous = main_record;
    for (const building_type_registry_impl::ComposedPartDefinition &part : composition.parts()) {
        const building_type_registry_impl::ComposedPartOffset offset = part.offset_for_rotation(rotation);
        if (!offset.has_value) {
            continue;
        }
        previous = repair_loaded_composed_child(main_record, previous, *definition, part,
            origin_x + offset.x, origin_y + offset.y);
    }
    previous->next_part_building_id = 0;

    Building main_object(main_record);
    if (main_object.type && main_object.type->is_warehouse()) {
        building_warehouse_recount_resources(main_object);
    }
}

void building_repair_loaded_compositions(void)
{
    for (size_t i = 1; i < data.buildings.size(); i++) {
        repair_loaded_composed_main(&data.buildings[i]);
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
            building *prev = building_get(id);
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
    b->size = props->size;
    b->created_sequence = extra.created_sequence++;
    b->sentiment.house_happiness = 100;

    fill_adjacent_types(b);
    city_culture_add_building_module_capacity(b);

    // house size
    if (building_type_registry_has_housing(type)) {
        b->house_size = props->size;
    }

    // subtype
    if (building_is_house(type)) {
        int level = building_type_registry_get_housing_level(type);
        b->subtype.house_level = static_cast<short>(level);
    }

    b->output_resource_id = building_output_resource(type);

    Building building_obj(b);
    if (building_obj.type && building_obj.type->is_granary()) {
        b->resources[RESOURCE_NONE] = FULL_GRANARY;
    }

    const building_type_registry_impl::BuildingType *definition =
        building_type_registry_impl::definition_for_type(type);
    const building_type_registry_impl::Distribution *distribution =
        definition ? definition->distribution() : nullptr;

    // Set it as accepting all goods defined by this building's distribution.
    for (resource_type r = (RESOURCE_NONE + 1); r < RESOURCE_SLOT_COUNT; r = static_cast<resource_type>(r + 1)) {
        b->accepted_goods[r] = distribution && distribution->handles_resource(r);
    }
    if (building_obj.type && building_obj.type->attr() && !std::strcmp(building_obj.type->attr(), "dock")) {
        for (resource_type r = (RESOURCE_NONE + 1); r < RESOURCE_SLOT_COUNT; r = static_cast<resource_type>(r + 1)) {
            b->accepted_goods[r] = 1;
        }
    }

    // Exception for Venus temples which should never accept wine by default to prevent unwanted evolutions
    if (building_is_venus_temple(type)) {
        b->accepted_goods[resource_wine()] = 0;
    }

    if (building_obj.type &&
        (building_obj.type->is_warehouse() || building_obj.type->composition().child_inherits_orientation())) {
        b->subtype.orientation = building_rotation_get_rotation();
    }

    // Most roadblock-like buildings should allow everything by default
    if (Roadblock(b).kind() != ROADBLOCK_NONE &&
        Roadblock(b).kind() != ROADBLOCK_STANDARD &&
        !type_matches(b->type, "gatehouse") &&
        !type_matches(b->type, "palisade_gate") &&
        (!building_obj.type || !building_obj.type->is_granary()) &&
        (!building_obj.type || !building_obj.type->is_warehouse()) &&
        config_get(CONFIG_GP_CH_GATES_DEFAULT_TO_PASS_ALL_WALKERS)) {
        b->data.roadblock.exceptions = ROADBLOCK_PERMISSION_ALL;
    }
    if (building_type_is_bridge(b->type)) {
        // Bridges should allow all walkers by default.
        b->data.roadblock.exceptions = ROADBLOCK_PERMISSION_ALL;
    }
    if (building_obj.type && building_obj.type->is_granary() &&
        !config_get(CONFIG_GP_CH_GRANARY_DEFAULT_TO_PASS_ALL_WALKERS)) {
        b->data.roadblock.exceptions = ROADBLOCK_PERMISSION_ALL;
    }
    if (building_obj.type && building_obj.type->is_warehouse() &&
        !config_get(CONFIG_GP_CH_WAREHOUSE_DEFAULT_TO_PASS_ALL_WALKERS)) {
        b->data.roadblock.exceptions = ROADBLOCK_PERMISSION_ALL;
    }

    if (type_matches(b->type, "market") && config_get(CONFIG_GP_CH_MARKETS_DONT_ACCEPT)) {
        if (const building_type_registry_impl::Distribution *market_distribution =
            building_obj.type ? building_obj.type->distribution() : nullptr) {
            market_distribution->set_acceptance(building_obj, 0);
        }
    } else if (type_matches(b->type, "market") && !config_get(CONFIG_GP_CH_MARKETS_DONT_ACCEPT)) {
        if (const building_type_registry_impl::Distribution *market_distribution =
            building_obj.type ? building_obj.type->distribution() : nullptr) {
            market_distribution->set_acceptance(building_obj, 1);
        }
    }

    b->x = x;
    b->y = y;
    b->grid_offset = map_grid_offset(x, y);
    b->house_figure_generation_delay = map_random_get(b->grid_offset) & 0x7f;
    b->figure_roam_direction = b->house_figure_generation_delay & 6;
    b->fire_proof = props->fire_proof;
    b->is_close_to_water = building_is_close_to_water(b);

    return b;
}

void building_change_type(building *b, building_type type)
{
    if (b->type == type) {
        return;
    }
    city_culture_remove_building_module_capacity(b);
    remove_adjacent_types(b);
    b->type = type;
    fill_adjacent_types(b);
    city_culture_add_building_module_capacity(b);
}

static void building_delete(building *b)
{
    city_culture_remove_building_module_capacity(b);
    building_clear_related_data(b);
    remove_adjacent_types(b);
    int id = b->id;
    memset(b, 0, sizeof(building));
    b->id = id;

    trim_buildings();
}

void building_clear_related_data(building *b)
{
    if (b->storage_id) {
        building_storage_delete(b->storage_id);
        b->storage_id = 0;
    }
    if (building_is_fort(b->type)) {
        Building fort(*b);
        formation_legion_delete_for_fort(fort);
    }
    if (building_matches(b, "triumphal_arch")) {
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
    building *b = building_get(to_restore->id);
    memcpy(b, to_restore, sizeof(building));
    fill_adjacent_types(b);
    city_culture_add_building_module_capacity(b);
    if (b->state == BUILDING_STATE_IN_USE || b->state == BUILDING_STATE_MOTHBALLED) {
        Building(b).refresh_graphic();
    }
    return b;
}

void building_trim(void)
{
    trim_buildings();
}

int building_was_tent(const building *b)
{
    if (!b) {
        return 0;
    }
    int level = building_type_registry_get_housing_level(static_cast<building_type>(b->data.rubble.og_type));
    return level >= HOUSE_SMALL_TENT && level <= HOUSE_LARGE_TENT;
}

int building_is_storage(building_type b_type)
{
    const building_type_registry_impl::BuildingType *definition = definition_for_type(b_type);
    return definition && (definition->is_granary() || definition->is_warehouse());
}

int building_is_still_burning(building *b)
{
    int hot = building_matches(b, "burning_ruin");
    int grid_offset = hot ? b->data.rubble.og_grid_offset : b->grid_offset;
    int size = hot ? b->data.rubble.og_size : b->size;
    grid_slice *b_area = map_grid_get_grid_slice_square(grid_offset, size);
    for (int i = 0; i < b_area->size; i++) {
        int offset = b_area->grid_offsets[i];
        if (map_has_figure_at(offset)) {  // also check for prefects on the tile - their presence prevents rebuilding
            return 1;
        }
        building *tile_building = building_get(map_building_at(offset));
        if (building_matches(tile_building, "burning_ruin")) {
            if (tile_building->state == BUILDING_STATE_RUBBLE) {
                continue; // extinguished tile
            }
            return 1;
        }
    }
    return 0;
}

int building_can_repair(building *b)
{
    if (!b) {
        return 0;
    }
    if (building_matches(b, "burning_ruin")) {
        if (building_is_still_burning(b)) {
            return 0;
        }
        if (!building_can_repair_type(static_cast<building_type>(b->data.rubble.og_type))) {
            return 0;
        } else {
            return 1;
        }
    } else {
        if (b->state != BUILDING_STATE_RUBBLE) {
            return 0;
        } else {
            return building_can_repair_type(b->type);
        }
    }
}

int building_repair_cost(building *b)
{
    int og_grid_offset = 0, og_size = 0;
    building_type og_type = BUILDING_NONE;
    if (!b || !building_can_repair(b)) {
        return 0;
    }
    Building building_obj(b);
    int is_ruin = building_matches(b, "burning_ruin") || // ruins and collapsed warehouse parts all use rubble data
        building_matches(b, "warehouse_space") || (building_obj.type && building_obj.type->is_warehouse());

    og_grid_offset = is_ruin ? b->data.rubble.og_grid_offset : b->grid_offset;
    og_size = is_ruin ? b->data.rubble.og_size : b->size;
    og_type = static_cast<building_type>(is_ruin ? b->data.rubble.og_type : b->type);

    if (building_is_house(og_type)) {
        grid_slice *house_slice = map_grid_get_grid_slice_house(b->id, 1);
        int clear_cost = house_slice->size * (11 + 3); // 10.5 per new house tile + 3 per rubble tile to clear
        return clear_cost;
    }
    if (building_matches(b, "warehouse_space")) {
        og_size = 1; // dont charge for clearing the whole warehouse, just the collapsed part, otherwise its *9
    }
    grid_slice *grid_slice = map_grid_get_grid_slice_square(og_grid_offset, og_size); // wont work correctly for hippo
    int clear_cost = building_construction_prepare_terrain(grid_slice, CLEAR_MODE_RUBBLE, COST_MEASURE);
    int placement_cost = model_get_building(og_type)->cost;
    const building_type_registry_impl::BuildingType *original_definition = definition_for_type(og_type);
    if (original_definition && original_definition->is_warehouse() && building_matches(b, "warehouse_space")) {
        placement_cost = 0; // collapsed warehouse parts only need clearing cost, no placement cost
    }
    return clear_cost + placement_cost + placement_cost / 20; // +5% fee on a building price
}

static int get_rubble_data(building *b, int *og_size, int *og_grid_offset, int *og_orientation, building_type *og_type)
{
    if (!b) {
        return 0;
    }

    int has = b->data.rubble.og_size || b->data.rubble.og_grid_offset ||
        b->data.rubble.og_orientation || b->data.rubble.og_type;

    if (!has) {
        return 0;
    } else {
        if (og_size) { *og_size = b->data.rubble.og_size; }
        if (og_grid_offset) { *og_grid_offset = b->data.rubble.og_grid_offset; }
        if (og_orientation) { *og_orientation = b->data.rubble.og_orientation; }
        if (og_type) { *og_type = static_cast<building_type>(b->data.rubble.og_type); }
        return 1;
    }
}

static int is_warehouse_ruin(building *b)
{
    int is_warehouse = 0;
    Building building_obj(b);
    if ((building_obj.type && building_obj.type->is_warehouse()) || building_matches(b, "warehouse_space")) {
        is_warehouse = 1;
    } else if (building_matches(b, "burning_ruin")) { // shouldnt happen - wh are fireproof - but just in case
        building_type original_type = static_cast<building_type>(b->data.rubble.og_type);
        const building_type_registry_impl::BuildingType *original_definition = definition_for_type(original_type);
        if ((original_definition && original_definition->is_warehouse()) ||
            original_type_matches(b, "warehouse_space")) {
            is_warehouse = 1;
        }
    }
    return is_warehouse;
}

static int warehouse_repair(building *b)
{
    building *main_warehouse = building_main(b); // should point to the warehouse tower
    building *new_building = NULL; // placeholder
    int og_size = 3, og_grid_offset = 0, og_orientation = 0, success = 0;
    get_rubble_data(main_warehouse, 0, &og_grid_offset, &og_orientation, 0); // no og_type or size we know it should be warehouse, 3
    int og_storage_id = main_warehouse->storage_id; //store the original storage id before clearing it
    building_data_transfer_backup();
    building_data_transfer_copy(main_warehouse, 1);
    int standard_grid_offset = building_warehouse_get_main_grid_offset(Building(b));
    int std_x = map_grid_offset_to_x(standard_grid_offset);
    int std_y = map_grid_offset_to_y(standard_grid_offset);
    grid_slice *grid_slice = map_grid_get_grid_slice_square(standard_grid_offset, og_size);
    if (building_construction_nearby_enemy_type(grid_slice) != FIGURE_NONE) {
        city_warning_show_translated(WARNING_ENEMY_NEARBY);
        building_data_transfer_restore_and_clear_backup();
        return 0;
    }
    map_terrain_backup(); // backup the terrain in case of failure
    int cleared = building_construction_prepare_terrain(grid_slice, CLEAR_MODE_RUBBLE, COST_PROCESS);
    building_type warehouse_type = runtime_type("warehouse");
    if (cleared) {
        success = building_construction_place_building(warehouse_type, std_x, std_y, 1);
        new_building = building_main(building_get(map_building_at(map_grid_offset(std_x, std_y))));//inception
    }

    if (!success || !cleared) {
        map_terrain_restore(); // restore terrain on failure
        city_finance_process_construction(-cleared); // refund clearing cost
        city_warning_show(WARNING_REPAIR_IMPOSSIBLE, translation_for_key("TR_WARNING_REPAIR_IMPOSSIBLE"));
        return 0;
    }

    if (new_building->storage_id != og_storage_id) {
        building_storage_delete(new_building->storage_id);
        building_storage_change_building(og_storage_id, new_building->id);
        // above does `new_building->storage_id = og_storage_id`
        b->storage_id = 0; // remove reference to the storage we just nuked
    }

    int placement_cost = model_get_building(warehouse_type)->cost * success;
    int full_cost = (placement_cost + placement_cost / 20);// +5%

    city_finance_process_construction(full_cost);
    new_building->subtype.orientation = og_orientation;
    map_building_set_rubble_grid_building_id(standard_grid_offset, 0, 3); // remove rubble marker
    building_data_transfer_paste(new_building, 1);
    new_building->state = BUILDING_STATE_CREATED;
    building_data_transfer_restore_and_clear_backup();
    figure_create_explosion_cloud(
        map_grid_offset_to_x(standard_grid_offset), map_grid_offset_to_y(standard_grid_offset), 3, 1);

    city_culture_remove_building_module_capacity(b);
    b->state = BUILDING_STATE_DELETED_BY_GAME; // mark old building as deleted
    game_undo_disable(); // not accounting for undoing repairs
    return full_cost;
}

int building_repair(building *b)
{
    if (!b) {
        return 0;
    }
    if (building_matches(b, "burning_ruin") && building_is_still_burning(b)) {
        city_warning_show(WARNING_REPAIR_BURNING, translation_for_key("TR_WARNING_REPAIR_BURNING"));
        return 0;
    }
    if (!building_can_repair_type(b->type) && !building_can_repair_type(static_cast<building_type>(b->data.rubble.og_type))) {
        if (building_monument_is_limited(b->type) ||
            building_monument_is_limited(static_cast<building_type>(b->data.rubble.og_type))) {
            city_warning_show(WARNING_REPAIR_MONUMENT, translation_for_key("TR_WARNING_CANT_REPAIR_MONUMENTS"));
        } else if (building_matches(b, "aqueduct") || original_type_matches(b, "aqueduct")) {
            city_warning_show(WARNING_REPAIR_AQUEDUCT, translation_for_key("TR_WARNING_CANT_REPAIR_AQUEDUCTS"));
        } else {
            city_warning_show(WARNING_REPAIR_IMPOSSIBLE, translation_for_key("TR_WARNING_REPAIR_IMPOSSIBLE"));
        }
        return 0;
    }
    if (is_warehouse_ruin(b)) { // use helper for warehouse repairs
        return warehouse_repair(b);
    }
    // flags and placeholders
    int og_size = 0, og_grid_offset = 0, og_orientation = 0, og_storage_id = 0, wall = 0, is_house_lot = 0, success = 0;
    building_type og_type = BUILDING_NONE;
    get_rubble_data(b, &og_size, &og_grid_offset, &og_orientation, &og_type);
    //  Backup building data
    building_data_transfer_backup();
    building_data_transfer_copy(b, 1);
    //  Resolve placement data
    int grid_offset = og_grid_offset ? og_grid_offset : b->grid_offset;
    int x = map_grid_offset_to_x(grid_offset);
    int y = map_grid_offset_to_y(grid_offset);
    int size = og_size ? og_size : b->size;
    building_type type = og_type ? og_type : static_cast<building_type>(b->type);
    building_type type_to_place = og_type ? og_type : static_cast<building_type>(b->type);

    if (building_is_house(type) || type == 1) {
        is_house_lot = 1;
        building_change_type(b, building_type_registry_get_vacant_lot_fill_type());
    }
    int placement_cost = 0;
    og_storage_id = b->storage_id; //store the original storage id before clearing it
    // Clear terrain and place building
    grid_slice *grid_slice = map_grid_get_grid_slice_square(grid_offset, size);
    if (building_construction_nearby_enemy_type(grid_slice) != FIGURE_NONE) {
        city_warning_show_translated(WARNING_ENEMY_NEARBY);
        building_data_transfer_restore_and_clear_backup();
        return 0;
    }
    map_terrain_backup(); // backup the terrain in case of failure
    int cleared = building_construction_prepare_terrain(grid_slice, CLEAR_MODE_RUBBLE, COST_PROCESS);
    if (is_house_lot) {
        success = building_construction_fill_vacant_lots(grid_slice);
    } else if (type_matches_any(type_to_place, {"wall", "tower"})) {
        wall = 1;
        building_type wall_type = runtime_type("wall");
        for (int i = 0; i < grid_slice->size; i++) {
            success = building_construction_place_wall(grid_slice->grid_offsets[i]);
            placement_cost += model_get_building(wall_type)->cost * success;
            if (!success) {
                break; // force failure if any wall/tower placement failed
            }
        }
        if (type_matches(type_to_place, "tower")) {
            map_tiles_update_all_walls(); // towers affect wall connections
            success = building_construction_place_building(type_to_place, x, y, 1);
        }

    } else {
        if (type_matches(type_to_place, "gatehouse")) {
            wall = 1;
        }
        success = building_construction_place_building(type_to_place, x, y, 1);
    }
    building *new_building = building_get(map_building_at(map_grid_offset(x, y)));
    if (!success || !cleared) {
        map_terrain_restore(); // restore terrain on failure
        city_finance_process_construction(-cleared); // refund clearing cost
        city_warning_show(WARNING_REPAIR_IMPOSSIBLE, translation_for_key("TR_WARNING_REPAIR_IMPOSSIBLE"));
        return 0;
    }
    if (building_is_storage(type_to_place) && b->storage_id) {
        if (new_building->storage_id != og_storage_id) {
            building_storage_delete(new_building->storage_id);
            building_storage_change_building(og_storage_id, new_building->id);
            // above does `new_building->storage_id = og_storage_id`
            b->storage_id = 0; // remove reference to the storage we just nuked
        }
    }
    placement_cost += model_get_building(type_to_place)->cost * success;
    int full_cost = (placement_cost + placement_cost / 20);// +5%

    city_finance_process_construction(full_cost);
    new_building->subtype.orientation = og_orientation;
    map_building_set_rubble_grid_building_id(grid_offset, 0, size); // remove rubble marker
    building_data_transfer_paste(new_building, 1);
    new_building->state = BUILDING_STATE_CREATED;
    building_data_transfer_restore_and_clear_backup();
    figure_create_explosion_cloud(new_building->x, new_building->y, og_size, 1);
    if (wall) {
        map_tiles_update_all_walls(); // towers affect wall connections
    }
    city_culture_remove_building_module_capacity(b);
    b->state = BUILDING_STATE_DELETED_BY_GAME; // mark old building as deleted
    game_undo_disable(); // not accounting for undoing repairs
    return full_cost;
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
            map_water_supply_refresh_building(b);
            // When a created building becomes live, rebuild its cached native image-group bindings immediately.
            Building(b).refresh_graphic();
            city_culture_refresh_building_module_capacity(b);
        }
        if (b->state == BUILDING_STATE_IN_USE && b->house_size) {
            continue;
        }
        if (b->state == BUILDING_STATE_UNDO || b->state == BUILDING_STATE_DELETED_BY_PLAYER) {
            if (building_matches(b, "tower") || building_matches(b, "gatehouse")) {
                wall_recalc = 1;
                road_recalc = 1;
            } else if (building_matches(b, "reservoir")) {
                aqueduct_recalc = 1;
            } else {
                Building building_obj(b);
                if ((building_obj.type && building_obj.type->is_granary()) || building_type_is_bridge(b->type)) {
                    road_recalc = 1;
                } else if (building_monument_is_grand_temple(b->type) ||
                    building_matches(b, "pantheon") || building_matches(b, "lighthouse")) {
                    road_recalc = 1;
                }
            }
            map_building_tiles_remove(b->id, b->x, b->y);
            if (Roadblock(b).kind() != ROADBLOCK_NONE && b->size == 1 && !building_type_is_bridge(b->type)) {
                // Leave the road behind the deleted roadblock
                // except for bridges - they are coded as size 1 too
                map_tiles_set_road(b->x, b->y);
                road_recalc = 1;
            }
            land_recalc = 1;
            building_delete(b);
        } else if (b->state == BUILDING_STATE_RUBBLE) {
            if (b->house_size) {
                city_population_remove_home_removed(b->house_population);
                b->house_population = 0;
            }
            if (building_is_fort(b->type) || building_matches(b, "fort_ground")) {
                b->state = BUILDING_STATE_DELETED_BY_GAME;
                map_building_tiles_remove(b->id, b->x, b->y);
                map_building_set_rubble_grid_building_id(b->grid_offset, 0, b->size);
            }
            // building_delete(b); // keep the rubbled building as a reference for reconstruction

            // monuments clear
            if (building_monument_is_limited(b->type) || building_monument_is_unfinished_monument(b)) {
                building_delete(b);
            }

        } else if (b->state == BUILDING_STATE_DELETED_BY_GAME) {
            building_delete(b);
        } else if (b->immigrant_figure_id) {
            const Figure *f = Figure::get(b->immigrant_figure_id);
            if (!f || f->state != FIGURE_STATE_ALIVE || f->destination_building.id() != b->id) {
                b->immigrant_figure_id = 0;
            }
        }
    }
    if (wall_recalc) {
        map_tiles_update_all_walls();
    }
    if (aqueduct_recalc) {
        map_tiles_update_all_aqueducts(0);
    }
    if (land_recalc) {
        map_routing_update_land();
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

        // Use wider type to prevent 8-bit overflow
        int desirability = map_desirability_get_max(record.x, record.y, record.size);

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

        record.desirability = (int8_t) desirability;
    }
}

int building_is_active(const building *b)
{
    if (b->state != BUILDING_STATE_IN_USE) {
        return 0;
    }
    if (building_is_house(b->type)) {
        return b->house_size > 0 && b->house_population > 0;
    }
    if (building_monument_is_unfinished_monument(b)) {
        return 0;
    }
    if (building_matches(b, "reservoir") || building_matches(b, "fountain")) {
        return b->has_water_access;
    }
    const building_type_registry_impl::BuildingType *definition =
        building_type_registry_impl::definition_for_type(b->type);
    if (definition && definition->is_oracle()) {
        return !building_monument_is_monument(b) || b->monument.phase == MONUMENT_FINISHED;
    }
    if (building_matches(b, "nymphaeum") || building_matches(b, "small_mausoleum") ||
        building_matches(b, "large_mausoleum")) {
        return b->monument.phase == MONUMENT_FINISHED;
    }
    if (building_matches(b, "wharf")) {
        return b->num_workers > 0 && map_water_wharf_live_fishing_boats(Building(const_cast<building *>(b))) > 0;
    }
    if (definition && std::strcmp(definition->attr(), "dock") == 0) {
        return b->num_workers > 0 && b->has_water_access;
    }
    return b->num_workers > 0;
}

int building_is_primary_product_producer(building_type type)
{
    return building_is_raw_resource_producer(type) || building_is_farm(type);
}

int building_is_house(building_type type)
{
    return building_type_registry_has_housing(type);
}

int building_get_house_group(building_type type)
{
    int legacy_level = building_type_registry_get_housing_level(type);
    if (legacy_level < 0) {
        return 0;
    }
    if (legacy_level <= HOUSE_LARGE_TENT) {
        return HOUSE_GROUP_TENT;
    }
    if (legacy_level <= HOUSE_LARGE_SHACK) {
        return HOUSE_GROUP_SHACK;
    }
    if (legacy_level <= HOUSE_LARGE_HOVEL) {
        return HOUSE_GROUP_HOVEL;
    }
    if (legacy_level <= HOUSE_LARGE_CASA) {
        return HOUSE_GROUP_CASA;
    }
    if (legacy_level <= HOUSE_GRAND_INSULA) {
        return HOUSE_GROUP_INSULA;
    }
    if (legacy_level <= HOUSE_GRAND_VILLA) {
        return HOUSE_GROUP_VILLA;
    }
    return legacy_level <= HOUSE_LUXURY_PALACE ? HOUSE_GROUP_PALACE : 0;
}

int building_is_house_group(house_groups group, building_type type)
{
    return building_get_house_group(type) == group;
}

// For Venus GT base bonus
int building_is_statue_garden_temple(building_type type)
{
    const building_properties *props = building_properties_for_type(type);
    return props->venus_gt_bonus;
}

int building_is_ceres_temple(building_type type)
{
    const building_type_registry_impl::BuildingType *definition = definition_for_type(type);
    return definition && definition->is_ceres_temple();
}

int building_is_neptune_temple(building_type type)
{
    const building_type_registry_impl::BuildingType *definition = definition_for_type(type);
    return definition && definition->is_neptune_temple();
}

int building_is_mercury_temple(building_type type)
{
    const building_type_registry_impl::BuildingType *definition = definition_for_type(type);
    return definition && definition->is_mercury_temple();
}

int building_is_mars_temple(building_type type)
{
    const building_type_registry_impl::BuildingType *definition = definition_for_type(type);
    return definition && definition->is_mars_temple();
}

int building_is_venus_temple(building_type type)
{
    const building_type_registry_impl::BuildingType *definition = definition_for_type(type);
    return definition && definition->is_venus_temple();
}

int building_has_supplier_inventory(building_type type)
{
    const building_type_registry_impl::BuildingType *definition =
        building_type_registry_impl::definition_for_type(type);
    return definition && definition->distribution();
}

int building_is_fort(building_type type)
{
    return type_matches_any(type, {
        "fort_legionaries",
        "fort_javelin",
        "fort_mounted",
        "fort_swords",
        "fort_archers"
    });
}

int building_mothball_toggle(building *b)
{
    if (b->state == BUILDING_STATE_IN_USE) {
        city_culture_remove_building_module_capacity(b);
        b->state = BUILDING_STATE_MOTHBALLED;
        b->num_workers = 0;
        city_culture_add_building_module_capacity(b);
    } else if (b->state == BUILDING_STATE_MOTHBALLED) {
        city_culture_remove_building_module_capacity(b);
        b->state = BUILDING_STATE_IN_USE;
        city_culture_add_building_module_capacity(b);
    }
    return b->state;
}

int building_mothball_set(building *b, int mothball)
{
    if (mothball) {
        if (b->state == BUILDING_STATE_IN_USE) {
            city_culture_remove_building_module_capacity(b);
            b->state = BUILDING_STATE_MOTHBALLED;
            b->num_workers = 0;
            city_culture_add_building_module_capacity(b);
        }
    } else if (b->state == BUILDING_STATE_MOTHBALLED) {
        city_culture_remove_building_module_capacity(b);
        b->state = BUILDING_STATE_IN_USE;
        city_culture_add_building_module_capacity(b);
    }
    return b->state;

}

unsigned char building_stockpiling_toggle(building *b)
{
    b->data.industry.is_stockpiling = !b->data.industry.is_stockpiling;
    return b->data.industry.is_stockpiling;
}

int building_get_levy(const building *b)
{
    int levy = b->monthly_levy;
    if (levy <= 0) {
        return 0;
    }
    if (building_monument_type_is_monument(b->type) && b->monument.phase != MONUMENT_FINISHED) {
        return 0;
    }
    if (b->state != BUILDING_STATE_IN_USE && levy && !b->prev_part_building_id) {
        return 0;
    }
    if (b->prev_part_building_id) {
        return 0;
    }

    // Pantheon base bonus
    const building_type_registry_impl::BuildingType *definition =
        building_type_registry_impl::definition_for_type(b->type);
    if (building_monument_working(runtime_type("pantheon")) &&
        ((definition && definition->is_temple()) || type_matches_any(b->type, {
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
    if (type_matches(type, "fountain") && building_monument_working_grand_temple_for_god(GOD_NEPTUNE)) {
        workers /= 2;
        if (workers == 0) {
            workers = 1;
        }
    }
    return workers;
}

void building_totals_add_corrupted_house(int unfixable)
{
    extra.incorrect_houses++;
    if (unfixable) {
        extra.unfixable_houses++;
    }
}

void building_clear_all(void)
{
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
    if (!value || resource < RESOURCE_NONE || resource >= RESOURCE_SLOT_COUNT) {
        return 0;
    }
    return resource == RESOURCE_NONE || resource_is_tradeable(resource);
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

static int building_resource_u8_count(const unsigned char *values)
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

static void building_write_resource_u8_values(buffer *buf, const unsigned char *values)
{
    buffer_write_u32(buf, building_resource_u8_count(values));
    if (building_resource_save_value(RESOURCE_NONE, values[RESOURCE_NONE])) {
        resource_save_write_ref(buf, RESOURCE_NONE);
        buffer_write_u8(buf, values[RESOURCE_NONE]);
    }
    for (int i = 0; i < resource_loaded_count(); i++) {
        resource_type resource = resource_get_loaded(i);
        if (resource != RESOURCE_NONE && building_resource_save_value(resource, values[resource])) {
            resource_save_write_ref(buf, resource);
            buffer_write_u8(buf, values[resource]);
        }
    }
}

static int building_uses_fetch_inventory(const building *b)
{
    if (!b) {
        return 0;
    }
    const building_type_registry_impl::BuildingType *definition = definition_for_type(b->type);
    return definition && (definition->has_distribution() || definition->is_caravanserai());
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
            building_matches(b, "warehouse_space") ?
                static_cast<resource_type>(b->subtype.warehouse_resource_id) :
                RESOURCE_NONE));
        resource_save_write_ref(buf, building_resource_save_ref(
            building_uses_fetch_inventory(b) ?
                static_cast<resource_type>(b->data.market.fetch_inventory_id) :
                RESOURCE_NONE));
        resource_save_write_ref(buf, building_resource_save_ref(
            building_matches(b, "cart_depot") ?
                static_cast<resource_type>(b->data.depot.current_order.resource_type) :
                RESOURCE_NONE));
        building_write_resource_i16_values(buf, b->resources);
        building_write_resource_u8_values(buf, b->accepted_goods);
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

static void building_resource_state_load_i16_values(buffer *buf, building *b)
{
    int count = building_resource_state_read_count(buf, "resources");
    for (int i = 0; i < count; i++) {
        resource_type resource = resource_save_read_ref(buf);
        int value = buffer_read_i16(buf);
        if (b && resource >= RESOURCE_NONE && resource < RESOURCE_SLOT_COUNT) {
            b->resources[resource] = static_cast<short>(value);
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
            b->accepted_goods[resource] = static_cast<unsigned char>(value);
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
    if (!b || !building_matches(b, "dock") || dock_has_any_accepted_goods(b)) {
        return;
    }
    for (resource_type r = (RESOURCE_NONE + 1); r < RESOURCE_SLOT_COUNT; r = static_cast<resource_type>(r + 1)) {
        b->accepted_goods[r] = 1;
    }
}

void building_resource_state_save(buffer *buf)
{
    if (!buf) {
        return;
    }

    size_t capacity = 65536;
    std::vector<uint8_t> data;
    buffer scratch;
    for (;;) {
        data.assign(capacity, 0);
        buffer_init(&scratch, data.data(), data.size());
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
    buffer_write_raw(buf, data.data(), scratch.index);
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
        building *b = id < static_cast<uint32_t>(data.buildings.size()) ? building_get(id) : nullptr;
        if (b && b->state == BUILDING_STATE_UNUSED) {
            b = nullptr;
        }

        resource_type output = resource_save_read_ref(&payload);
        resource_type warehouse_resource = resource_save_read_ref(&payload);
        resource_type fetch_inventory = resource_save_read_ref(&payload);
        resource_type depot_order_resource = resource_save_read_ref(&payload);

        if (b) {
            b->output_resource_id = static_cast<unsigned char>(
                output >= RESOURCE_NONE && output < RESOURCE_SLOT_COUNT ? output : RESOURCE_NONE);
            if (building_matches(b, "warehouse_space")) {
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
            if (building_matches(b, "cart_depot")) {
                b->data.depot.current_order.resource_type =
                    depot_order_resource >= RESOURCE_NONE && depot_order_resource < RESOURCE_SLOT_COUNT ?
                        static_cast<resource_type>(depot_order_resource) :
                        RESOURCE_NONE;
            }
            memset(b->resources, 0, sizeof(b->resources));
            memset(b->accepted_goods, 0, sizeof(b->accepted_goods));
        }

        building_resource_state_load_i16_values(&payload, b);
        building_resource_state_load_u8_values(&payload, b);
        repair_dock_accepted_goods_if_empty(b);
    }
}

int building_is_close_to_water(const building *b)
{
    return map_terrain_exists_tile_in_radius_with_type(b->x, b->y, b->size, WATER_DESIRABILITY_RANGE, TERRAIN_WATER);
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
    if (b->state == BUILDING_STATE_UNUSED && type_matches(b->type, "gardens")) {
        b->type = BUILDING_NONE;
    }

    resize_buildings(highest_id_in_use + 1);

    extra.created_sequence = buffer_read_i32(sequence);

    extra.incorrect_houses = buffer_read_i32(corrupt_houses);
    extra.unfixable_houses = buffer_read_i32(corrupt_houses);
    city_culture_rebuild_module_capacity_cache();
}
