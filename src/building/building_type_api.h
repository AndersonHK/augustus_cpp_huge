#pragma once

#include "building/building_type.h"
#include "building/properties.h"
#include "figure/type.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct building building;

enum {
    BUILDING_TYPE_TERRAIN_MEADOW = 1 << 0,
    BUILDING_TYPE_TERRAIN_ROCK = 1 << 1,
    BUILDING_TYPE_TERRAIN_TREE = 1 << 2,
    BUILDING_TYPE_TERRAIN_WATER = 1 << 3,
    BUILDING_TYPE_TERRAIN_WALL = 1 << 4,
    BUILDING_TYPE_TERRAIN_DISTANT_WATER = 1 << 5
};

enum {
    BUILDING_TYPE_HOUSING_RESIDENT_NONE = 0,
    BUILDING_TYPE_HOUSING_RESIDENT_PLEBEIAN = 1,
    BUILDING_TYPE_HOUSING_RESIDENT_PATRICIAN = 2
};

enum {
    BUILDING_TYPE_HOUSING_TRANSITION_EVOLVE_TO = 1,
    BUILDING_TYPE_HOUSING_TRANSITION_DEVOLVE_TO = 2,
    BUILDING_TYPE_HOUSING_TRANSITION_MERGE_TO = 3,
    BUILDING_TYPE_HOUSING_TRANSITION_SPLIT_TO = 4
};

const char *building_type_registry_get_building_type_path(void);

int building_type_registry_validate_mod(void);
int building_type_registry_load(void);
void building_type_registry_apply_model_overrides(void);

building_type building_type_registry_runtime_id_from_text(const char *text_id);
building_type building_type_registry_theater_type(void);
building_type building_type_registry_well_type(void);
int building_type_registry_is_theater(building_type type);
int building_type_registry_is_well(building_type type);
int building_type_registry_is_temple(building_type type);
int building_type_registry_is_warehouse(building_type type);
int building_type_registry_is_granary(building_type type);
int building_type_registry_is_mess_hall(building_type type);
int building_type_registry_is_architect_guild(building_type type);
int building_type_registry_is_caravanserai(building_type type);
int building_type_registry_is_lighthouse(building_type type);
int building_type_registry_is_armoury(building_type type);
int building_type_registry_has_native_storage(building_type type);
int building_type_registry_has_distribution(building_type type);
int building_type_registry_has_definition(building_type type);
const char *building_type_registry_get_name_key(building_type type);
int building_type_registry_get_model_size(building_type type);
const char *building_type_registry_get_foundation_policy(building_type type);
int building_type_registry_get_foundation_required_terrain(building_type type);
const char *building_type_registry_get_button_group(building_type type);
int building_type_registry_get_button_order(building_type type);
const char *building_type_registry_get_button_icon(building_type type);
const char *building_type_registry_get_button_icon_image(building_type type);
int building_type_registry_get_button_icon_image_id(building_type type);
const char *building_type_registry_get_button_text_key(building_type type);
int building_type_registry_has_labor_seeker(building_type type);
figure_type building_type_registry_get_preview_figure(building_type type);
int building_type_registry_get_sound_id(building_type type);
int building_type_registry_get_sound_mute_on_enemies(building_type type);
int building_type_registry_get_sound_always_play(building_type type);
int building_type_registry_get_sound_requires_water_access(building_type type);
int building_type_registry_has_water_access_requirements(building_type type);
int building_type_registry_get_graphics_image_id(const building *b);
int building_type_registry_has_construction(building_type type);
int building_type_registry_has_phased_construction(building_type type);
int building_type_registry_get_construction_phase_count(building_type type);
int building_type_registry_get_construction_road_update_radius(building_type type);
int building_type_registry_get_instant_construction_requirement(building_type type, int resource);
int building_type_registry_get_construction_requirement(building_type type, int resource, int phase);
int building_type_registry_has_housing(building_type type);
const model_house *building_type_registry_get_housing_model(building_type type);
int building_type_registry_get_housing_resident_class(building_type type);
int building_type_registry_housing_has_resident_class(building_type type, int resident_class);
int building_type_registry_get_housing_level(building_type type);
int building_type_registry_get_housing_capacity(building_type type);
int building_type_registry_get_housing_level_count(void);
int building_type_registry_get_housing_level_at(int index);
building_type building_type_registry_get_housing_type_for_level(int level, int footprint_size);
building_type building_type_registry_get_housing_transition(building_type type, int transition);
building_type building_type_registry_get_vacant_lot_fill_type(void);

#ifdef __cplusplus
}
#endif
