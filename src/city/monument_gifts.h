#pragma once
#include "building/building_type.h"
#include "core/buffer.h"
#include <string_view>

class Building;
void city_monument_gifts_reset();
void city_monument_gifts_award(std::string_view event, int count = 1);
void city_monument_gifts_grant(building_type type, int count);
bool city_monument_gift_available(building_type type);
void city_monument_gifts_save(buffer *buf);
void city_monument_gifts_load(buffer *buf);
void city_monument_gifts_import_legacy();
void city_monument_gift_supply(Building &building, bool materials, bool workers, int loads_per_delivery);
