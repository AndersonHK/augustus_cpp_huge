#include "building/building_record.h"
#include "data_transfer.h"

#include "building/building.h"
#include "building/building_type_registry_internal.h"
#include "building/building_type_api.h"
#include "building/industry.h"
#include "building/roadblock.h"
#include "building/storage.h"
#include "city/warning.h"

#include <cstring>
#include <string.h>

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

static building_type type_from_text(const char *text_id)
{
    return building_type_registry_runtime_id_from_text(text_id);
}

static int type_is(building_type type, const char *text_id)
{
    return type == type_from_text(text_id);
}

static int type_is_any(building_type type, const char *const *text_ids, size_t count)
{
    for (size_t i = 0; i < count; i++) {
        if (type_is(type, text_ids[i])) {
            return 1;
        }
    }
    return 0;
}

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
    return type_is_any(type, roadblock_types, sizeof(roadblock_types) / sizeof(roadblock_types[0]));
}

static int type_is_farm_or_wharf(building_type type)
{
    static const char *const types[] = {
        "wheat_farm",
        "vegetable_farm",
        "fruit_farm",
        "olive_farm",
        "vines_farm",
        "pig_farm",
        "wharf"
    };
    return type_is_any(type, types, sizeof(types) / sizeof(types[0]));
}

static int type_is_primary_product_producer(building_type type)
{
    return resource_is_raw_material(building_output_resource(type)) || type_is_farm_or_wharf(type);
}

void building_data_transfer_clear(int backup)
{
    if (backup) {
        memset(&backup_data, 0, sizeof(backup_data));
    } else {
        memset(&data, 0, sizeof(data));
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
    memset(&backup_data, 0, sizeof(backup_data));
}

static int can_transfer_to(const Building &target, int supress_warnings)
{
    building_data_type data_type = building_data_transfer_data_type_from_building_type(target.type_id());
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

int building_data_transfer_possible(building *b, int supress_warnings)
{
    return can_transfer_to(Building(b), supress_warnings);
}

int building_data_transfer_copy(building *b, int supress_warnings)
{
    Building source(b);
    building_type copy_type = source.type_id();
    if (source.is_type(type_from_text("burning_ruin"))) {
        copy_type = source.rubble_original_type_id();
    }
    building_data_type data_type = building_data_transfer_data_type_from_building_type(copy_type);
    if (data_type == DATA_TYPE_NOT_SUPPORTED) {

        if (!supress_warnings) {
            city_warning_show(WARNING_DATA_COPY_NOT_SUPPORTED, translation_for_key("TR_CITY_WARNING_DATA_COPY_NOT_SUPPORTED"));
        }
        return 0;
    } else {
        memset(&data, 0, sizeof(data));
        data.data_type = data_type;
    }

    const building_storage *storage;
    data.mothball = source.is_mothballed() ? 1 : 0;
    switch (data_type) {
        case DATA_TYPE_ROADBLOCK:
            data.i16 = static_cast<short>(Roadblock(source.legacy_record()).exceptions());
            break;
        case DATA_TYPE_MARKET:
            source.copy_accepted_goods(data.resource, RESOURCE_SLOT_COUNT);
            break;
        case DATA_TYPE_TAVERN:
            source.copy_accepted_goods(data.resource, RESOURCE_SLOT_COUNT);
            break;
        case DATA_TYPE_GRANARY:
            storage = building_storage_get(source.storage_id());
            data.storage = *storage;
            data.i16 = static_cast<short>(Roadblock(source.legacy_record()).exceptions());
            break;
        case DATA_TYPE_WAREHOUSE:
            storage = building_storage_get(source.storage_id());
            data.storage = *storage;
            break;
        case DATA_TYPE_DOCK:
            source.copy_accepted_goods(data.resource, RESOURCE_SLOT_COUNT);
            data.i8 = static_cast<char>(source.dock_has_accepted_route_ids());
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

int building_data_transfer_paste(building *b, int supress_warnings)
{
    Building target(b);
    building_data_type data_type = building_data_transfer_data_type_from_building_type(target.type_id());

    if (!can_transfer_to(target, supress_warnings)) {
        return 0;
    }

    switch (data_type) {
        case DATA_TYPE_ROADBLOCK:
            Roadblock(target.legacy_record()).set_exceptions(data.i16);
            break;
        case DATA_TYPE_MARKET:
        case DATA_TYPE_TAVERN:
            target.set_accepted_goods(data.resource, RESOURCE_SLOT_COUNT);
            break;
        case DATA_TYPE_GRANARY:
        case DATA_TYPE_WAREHOUSE:
            building_storage_set_data(target.storage_id(), data.storage);
            Roadblock(target.legacy_record()).set_exceptions(data.i16);
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
    if (type_is_roadblock_transfer(type)) {
        return DATA_TYPE_ROADBLOCK;
    }

    if (type_is_primary_product_producer(type)) {
        return DATA_TYPE_RAW_RESOURCE_PRODUCER;
    }

    const building_type_registry_impl::BuildingType *definition =
        building_type_registry_impl::definition_for_type(type);
    if (definition && std::strcmp(definition->attr(), "dock") == 0) {
        return DATA_TYPE_DOCK;
    }
    if (type_is(type, "granary")) {
        return DATA_TYPE_GRANARY;
    }
    if (type_is(type, "warehouse") || type_is(type, "warehouse_space")) {
        return DATA_TYPE_WAREHOUSE;
    }
    if (type_is(type, "market")) {
        return DATA_TYPE_MARKET;
    }
    if (type_is(type, "tavern")) {
        return DATA_TYPE_TAVERN;
    }
    if (type_is(type, "depot")) {
        return DATA_TYPE_DEPOT;
    }
    return DATA_TYPE_NOT_SUPPORTED;
}
