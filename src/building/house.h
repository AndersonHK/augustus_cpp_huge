#pragma once

#include "building/building.h"
#include "building/properties.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Native-housing-aware helpers for C and C++ callers that still need legacy
 * house facts while the save/runtime compatibility fields are being retired.
 */
int building_house_is_active(const building *house);
int building_house_legacy_level(const building *house);
const model_house *building_house_get_model(const building *house);
int building_house_has_plebeian_residents(const building *house);
int building_house_has_patrician_residents(const building *house);

void building_house_change_to(building *house, building_type type);
void building_house_vacant_lot_mark_draw(int building_id);
void building_house_change_to_vacant_lot(building *house);

void building_house_merge(building *house);

int building_house_can_expand(building *house, int num_tiles);

void building_house_expand_to_large_insula(building *house);
void building_house_expand_to_large_villa(building *house);
void building_house_expand_to_large_palace(building *house);
int building_house_expand_to_type(building *house, building_type type);

void building_house_devolve_from_large_insula(building *house);
void building_house_devolve_from_large_villa(building *house);
void building_house_devolve_from_large_palace(building *house);
void building_house_devolve_to_type(building *house, building_type type);

void building_house_desize_patrician(building *house);

void building_house_check_for_corruption(building *house);

void building_house_restore_population_after_undo(building *house);

#ifdef __cplusplus
}
#endif
