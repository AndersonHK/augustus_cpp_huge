#include "properties.h"

#include "building/building_type_api.h"

#include <stdint.h>
#include <stdlib.h>

static model_building buildings[BUILDING_TYPE_MAX];
static building_properties properties[BUILDING_TYPE_MAX];

static const model_building NO_BUILDING_MODEL = {
    .cost = 0,
    .desirability_value = 0,
    .desirability_step = 0,
    .desirability_step_size = 0,
    .desirability_range = 0,
    .laborers = 0
};

static const model_house NO_HOUSE_MODEL = {
    .devolve_desirability = 0,
    .evolve_desirability = 0,
    .entertainment = 0,
    .water = 0,
    .religion = 0,
    .education = 0,
    .barber = 0,
    .bathhouse = 0,
    .health = 0,
    .food_types = 0,
    .pottery = 0,
    .oil = 0,
    .furniture = 0,
    .wine = 0,
    .prosperity = 0,
    .tax_multiplier = 0
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
    properties[type] = (building_properties) {0};
}

void building_properties_apply_xml_model_size(building_type type, int size)
{
    if (!valid_building_type(type)) {
        return;
    }
    properties[type].size = size;
}

void building_properties_apply_xml_event_attr(building_type type, const char *attr)
{
    if (!valid_building_type(type)) {
        return;
    }
    properties[type].event_data.attr = attr;
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
    for (building_type type = BUILDING_NONE; type < BUILDING_TYPE_MAX; type = (building_type) (type + 1)) {
        buildings[type] = NO_BUILDING_MODEL;
    }
}

void model_save_model_data(buffer *buf)
{
    int buf_size = sizeof(model_building) * BUILDING_TYPE_MAX;
    uint8_t *buf_data = malloc(buf_size);

    buffer_init(buf, buf_data, buf_size);
    buffer_write_raw(buf, buildings, buf_size);
}

void model_load_model_data(buffer *buf)
{
    int buf_size = sizeof(model_building) * BUILDING_TYPE_MAX;
    buffer_read_raw(buf, buildings, buf_size);
}

const model_house *model_get_house(house_level level)
{
    building_type type = building_type_registry_get_housing_type_for_level(level, 1);
    const model_house *model = building_type_registry_get_housing_model(type);
    return model ? model : &NO_HOUSE_MODEL;
}

model_building *model_get_building(building_type type)
{
    if (!valid_building_type(type)) {
        return (model_building *) &NO_BUILDING_MODEL;
    }
    return &buildings[type];
}

int model_house_uses_inventory(house_level level, resource_type inventory)
{
    const model_house *house = model_get_house(level);
    if (inventory == resource_wine()) {
        return house->wine;
    }
    if (inventory == resource_oil()) {
        return house->oil;
    }
    if (inventory == resource_furniture()) {
        return house->furniture;
    }
    if (inventory == resource_pottery()) {
        return house->pottery;
    }
    return 0;
}
