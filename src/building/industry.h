#pragma once

#include "building/building_fwd.h"
#include "game/resource.h"
#include "map/point.h"

class Figure;

int building_is_raw_resource_producer(const building_type_registry_impl::BuildingType *type);
int building_is_workshop(const building_type_registry_impl::BuildingType *type);
resource_type building_output_resource(const building_type_registry_impl::BuildingType *type);
building_type building_producer_for_resource(resource_type resource);
int building_production_per_month(const building_type_registry_impl::BuildingType *type);
int building_default_production_per_month(const building_type_registry_impl::BuildingType *type);
int building_set_production_per_month(const building_type_registry_impl::BuildingType *type, int production);

int building_get_raw_materials_for_workshop(resource_supply_chain *chain, const building_type_registry_impl::BuildingType *type);
int building_get_required_raw_amount_for_production(const building_type_registry_impl::BuildingType *type, int resource);

void building_industry_update_production(int new_day);

void building_industry_start_new_production(building *b);

void building_bless_farms(void);
void building_curse_farms(int big_curse);
void building_bless_industry(void);

int building_workshop_add_raw_material(Building *b, int resource, int loads, Figure &figure);

Building *building_get_workshop_for_raw_material(int x, int y, int resource, int road_network_id, map_point *dst);
int building_has_workshop_for_raw_material_with_room(int resource, int road_network_id);
Building *building_get_workshop_for_raw_material_with_room(int x, int y, int resource, int road_network_id, map_point *dst);

void building_industry_advance_stats(void);
void building_industry_start_strikes(void);

