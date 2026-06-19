#pragma once

#include "common.h"
#include "game/resource.h"
#include "input/mouse.h"
#include "translation/translation.h"

void window_building_draw_farm(building_info_context *c, resource_type resource);

void window_building_draw_marble_quarry(building_info_context *c);
void window_building_draw_iron_mine(building_info_context *c);
void window_building_draw_timber_yard(building_info_context *c);
void window_building_draw_clay_pit(building_info_context *c);
void window_building_draw_gold_mine(building_info_context *c);
void window_building_draw_stone_quarry(building_info_context *c);
void window_building_draw_sand_pit(building_info_context *c);

void window_building_draw_wine_workshop(building_info_context *c);
void window_building_draw_oil_workshop(building_info_context *c);
void window_building_draw_weapons_workshop(building_info_context *c);
void window_building_draw_furniture_workshop(building_info_context *c);
void window_building_draw_pottery_workshop(building_info_context *c);
void window_building_draw_brickworks(building_info_context *c);
void window_building_draw_concrete_maker(building_info_context *c);
void window_building_draw_city_mint(building_info_context *c);

void window_building_draw_shipyard(building_info_context *c);
void window_building_draw_wharf(building_info_context *c);

void window_building_draw_city_mint_foreground(building_info_context *c);
int window_building_handle_mouse_city_mint(const mouse *m, building_info_context *c);

void window_building_industry_get_tooltip(building_info_context *c, translation_key *translation);
