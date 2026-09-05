#include "properties.h"

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
    for (building_type type = BUILDING_NONE; type < BUILDING_TYPE_MAX; type = (building_type) (type + 1)) {
        buildings[type] = NO_BUILDING_MODEL;
    }
}

void model_save_model_data(buffer *buf)
{
    int buf_size = sizeof(model_building) * BUILDING_TYPE_MAX;
    auto *buf_data = static_cast<uint8_t *>(malloc(buf_size));

    buffer_init(buf, buf_data, buf_size);
    buffer_write_raw(buf, buildings, buf_size);
}

void model_load_model_data(buffer *buf)
{
    int buf_size = sizeof(model_building) * BUILDING_TYPE_MAX;
    buffer_read_raw(buf, buildings, buf_size);
}

model_building *model_get_building(building_type type)
{
    if (!valid_building_type(type)) {
        return (model_building *) &NO_BUILDING_MODEL;
    }
    return &buildings[type];
}
