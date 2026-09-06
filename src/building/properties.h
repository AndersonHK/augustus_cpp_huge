#pragma once

#include "building/building_type.h"
#include "city/resource.h"
#include "core/buffer.h"


// MODEL DATA

 /**
  * Building model
  */
typedef struct {
    int cost; /**< Cost of structure or of one tile of a structure (for walls) */
    int desirability_value; /**< Initial desirability value */
    int desirability_step; /**< Desirability step (in tiles) */
    int desirability_step_size; /**< Desirability step size */
    int desirability_range; /**< Max desirability range */
    int laborers; /**< Number of people a building employs (max) */
} model_building;

enum {
    MODEL_COST,
    MODEL_DESIRABILITY_VALUE,
    MODEL_DESIRABILITY_STEP,
    MODEL_DESIRABILITY_STEP_SIZE,
    MODEL_DESIRABILITY_RANGE,
    MODEL_LABORERS,
};

/**
 * Resets model data from properties
 */
void model_reset(void);
void model_capture_mod_defaults(void);
const model_building *model_get_mod_defaults(building_type type);
void model_mark_scenario_override(building_type type, int field);

void model_save_model_data(buffer *buf);
int model_load_model_data(buffer *buf, bool keyed = false);

/**
 * Gets the model for a building
 * @param type Building type
 * @return Read-only model
 */
model_building *model_get_building(building_type type);
int model_get_construction_cost(building_type type);

// PROPERTIES

typedef struct {
    int fire_proof;
    int image_group;
    int image_offset;
    int rotation_offset;
    int sound_id;
    int draw_desirability_range;
    int venus_gt_bonus; // indicator of whether building is part of the 'garden/statue/temple' group
    struct {
        const char *group;
        const char *id;
    } custom_asset;
    model_building building_model_data;
} building_properties;

void building_properties_init(void);
void building_properties_clear_xml_runtime_fields(building_type type);
void building_properties_apply_xml_sound_id(building_type type, int sound_id);
void building_properties_apply_xml_fire_proof(building_type type, int fire_proof);
void building_properties_apply_xml_draw_desirability_range(building_type type, int draw_desirability_range);
void building_properties_apply_xml_venus_gt_bonus(building_type type, int venus_gt_bonus);
const building_properties *building_properties_for_type(building_type type);

