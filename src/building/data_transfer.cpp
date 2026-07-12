#include "figure/figure.h"
#include "building/building_record.h"
#include "data_transfer.h"

#include "building/building.h"
#include "building/building_type_registry_internal.h"
#include "building/industry.h"
#include "building/roadblock.h"
#include "building/storage.h"
#include "city/warning.h"

#include <cstring>

typedef struct {
    building_data_type data_type;
    unsigned char resource[RESOURCE_SLOT_COUNT];
    building_storage storage;
    order depot_order;
    char i8;
    short i16;
    int i32;
    signed char mothball;
} transfer_data;

static transfer_data data;
static transfer_data backup_data;

static int type_is_roadblock_transfer(building_type type)
{
    static const char *const roadblock_types[] = {
        "roadblock",
        "garden_wall_gate",
        "panelled_garden_gate",
        "looped_garden_gate",
        "hedge_gate_dark",
        "hedge_gate_light",
        "palisade_gate",
        "gatehouse",
        "triumphal_arch"
    };
    return building_type_registry_impl::type_attr_is_any(
        type, roadblock_types, sizeof(roadblock_types) / sizeof(roadblock_types[0]));
}

void building_data_transfer_clear(int backup)
{
    if (backup) {
        std::memset(&backup_data, 0, sizeof(backup_data));
    } else {
        std::memset(&data, 0, sizeof(data));
    }
}
void building_data_transfer_backup(void)
{
    backup_data = data;
}

void building_data_transfer_restore(void)
{
    data = backup_data;
}

void building_data_transfer_restore_and_clear_backup(void)
{
    data = backup_data;
    std::memset(&backup_data, 0, sizeof(backup_data));
}

static int can_transfer_to(const Building &target, int supress_warnings)
{
    building_data_type data_type = building_data_transfer_data_type_from_building_type(
        target.type ? target.type->type() : BUILDING_NONE);
    if (data.data_type == DATA_TYPE_NOT_SUPPORTED || data_type == DATA_TYPE_NOT_SUPPORTED) {
        if (!supress_warnings) {
            city_warning_show(WARNING_DATA_PASTE_FAILURE, translation_for_key("TR_CITY_WARNING_DATA_PASTE_NOT_SUPPORTED"));
        }
        return 0;
    }
    if (data.data_type != data_type) {
        if (!supress_warnings) {
            city_warning_show(WARNING_DATA_PASTE_FAILURE, translation_for_key("TR_CITY_WARNING_DATA_PASTE_NOT_SUPPORTED"));
        }

        return 0;
    }
    return 1;
}

int building_data_transfer_possible(Building *b, int supress_warnings)
{
    return b ? can_transfer_to(*b, supress_warnings) : 0;
}

int building_data_transfer_copy(Building *b, int supress_warnings)
{
    if (!b) {
        return 0;
    }
    Building &source = *b;
    building_type copy_type = source.type ? source.type->type() : BUILDING_NONE;
    if (source.Rubble) {
        const building_type_registry_impl::BuildingType *original_type = source.Rubble->original_type();
        copy_type = original_type ? original_type->type() : BUILDING_NONE;
    }
    building_data_type data_type = building_data_transfer_data_type_from_building_type(copy_type);
    if (data_type == DATA_TYPE_NOT_SUPPORTED) {

        if (!supress_warnings) {
            city_warning_show(WARNING_DATA_COPY_NOT_SUPPORTED, translation_for_key("TR_CITY_WARNING_DATA_COPY_NOT_SUPPORTED"));
        }
        return 0;
    } else {
        std::memset(&data, 0, sizeof(data));
        data.data_type = data_type;
    }

    const building_storage *storage;
    data.mothball = source.is_mothballed() ? 1 : 0;
    switch (data_type) {
        case DATA_TYPE_ROADBLOCK:
            data.i16 = static_cast<short>(Roadblock(source).exceptions());
            break;
        case DATA_TYPE_MARKET:
            source.copy_accepted_goods(data.resource, RESOURCE_SLOT_COUNT);
            break;
        case DATA_TYPE_TAVERN:
            source.copy_accepted_goods(data.resource, RESOURCE_SLOT_COUNT);
            break;
        case DATA_TYPE_GRANARY:
            storage = building_storage_get(source.storage_id);
            data.storage = *storage;
            data.i16 = static_cast<short>(Roadblock(source).exceptions());
            break;
        case DATA_TYPE_WAREHOUSE:
            storage = building_storage_get(source.storage_id);
            data.storage = *storage;
            break;
        case DATA_TYPE_DOCK:
            source.copy_accepted_goods(data.resource, RESOURCE_SLOT_COUNT);
            data.i8 = static_cast<char>(source.dock_has_accepted_route_ids);
            data.i32 = source.dock_accepted_route_ids();
            break;
        case DATA_TYPE_DEPOT:
            data.depot_order = source.depot_order();
            break;
        case DATA_TYPE_RAW_RESOURCE_PRODUCER:
            data.i8 = static_cast<char>(source.industry_is_stockpiling());
            break;
        default:
            return 0;

    }
    if (!supress_warnings) {
        city_warning_show(WARNING_DATA_COPY_SUCCESS, translation_for_key("TR_CITY_WARNING_DATA_COPY_SUCCESS"));
    }
    return 1;
}

int building_data_transfer_paste(Building *b, int supress_warnings)
{
    if (!b) {
        return 0;
    }
    Building &target = *b;
    building_data_type data_type = building_data_transfer_data_type_from_building_type(
        target.type ? target.type->type() : BUILDING_NONE);

    if (!can_transfer_to(target, supress_warnings)) {
        return 0;
    }

    switch (data_type) {
        case DATA_TYPE_ROADBLOCK:
            Roadblock(target).set_exceptions(data.i16);
            break;
        case DATA_TYPE_MARKET:
        case DATA_TYPE_TAVERN:
            target.set_accepted_goods(data.resource, RESOURCE_SLOT_COUNT);
            break;
        case DATA_TYPE_GRANARY:
        case DATA_TYPE_WAREHOUSE:
            building_storage_set_data(target.storage_id, data.storage);
            Roadblock(target).set_exceptions(data.i16);
            break;
        case DATA_TYPE_DOCK:
            target.set_accepted_goods(data.resource, RESOURCE_SLOT_COUNT);
            target.set_dock_accepted_route_ids(data.i8, data.i32);
            break;
        case DATA_TYPE_DEPOT:
            target.set_depot_order(data.depot_order);
            break;
        case DATA_TYPE_RAW_RESOURCE_PRODUCER:
            target.set_industry_stockpiling(data.i8);
            break;
        default:
            return 0;
    }
    target.set_mothballed(data.mothball);
    if (!supress_warnings) {
        city_warning_show(WARNING_DATA_PASTE_SUCCESS, translation_for_key("TR_CITY_WARNING_DATA_PASTE_SUCCESS"));
    }
    return 1;

}

building_data_type building_data_transfer_data_type_from_building_type(building_type type)
{
    const building_type_registry_impl::BuildingType *definition =
        building_type_registry_impl::definition_for_type(type);

    if (type_is_roadblock_transfer(type)) {
        return DATA_TYPE_ROADBLOCK;
    }

    if (definition) {
        const resource_type output = building_output_resource(definition);
        if (resource_is_raw_material(output) || resource_is_food(output)) {
            return DATA_TYPE_RAW_RESOURCE_PRODUCER;
        }
    }

    if (building_type_registry_impl::type_attr_is(type, "dock")) {
        return DATA_TYPE_DOCK;
    }
    if (building_type_registry_impl::type_attr_is(type, "granary")) {
        return DATA_TYPE_GRANARY;
    }
    if (building_type_registry_impl::type_attr_is_any(type, {"warehouse", "warehouse_space"})) {
        return DATA_TYPE_WAREHOUSE;
    }
    if (building_type_registry_impl::type_attr_is(type, "market")) {
        return DATA_TYPE_MARKET;
    }
    if (building_type_registry_impl::type_attr_is(type, "tavern")) {
        return DATA_TYPE_TAVERN;
    }
    if (building_type_registry_impl::type_attr_is(type, "cart_depot")) {
        return DATA_TYPE_DEPOT;
    }
    return DATA_TYPE_NOT_SUPPORTED;
}
