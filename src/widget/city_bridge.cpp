#include "building/construction.h"
#include "building/building_type_registry_internal.h"
#include "graphics/image.h"
#include "map/bridge.h"

#include "city_bridge.h"

#include "building/building_type_api.h"
#include "map/property.h"
#include "map/sprite.h"
#include "map/terrain.h"

#include <cstring>

static int building_type_attr_is(building_type type, const char *text_id)
{
    const building_type_registry_impl::BuildingType *definition =
        building_type_registry_impl::definition_for_type(type);
    return definition && std::strcmp(definition->attr(), text_id) == 0;
}

void city_draw_bridge(int x, int y, float scale, int grid_offset)
{
    if (!map_terrain_is(grid_offset, TERRAIN_WATER)) {
        map_sprite_clear_tile(grid_offset);
        return;
    }
    if (map_terrain_is(grid_offset, TERRAIN_BUILDING) && !map_is_bridge(grid_offset)) {
        return;
    }
    color_t color_mask = 0;
    if (map_property_is_deleted(grid_offset) && !building_type_attr_is(building_construction_type(), "clear_trees")) {
        color_mask = COLOR_MASK_RED;
    }
    city_draw_bridge_tile(x, y, scale, map_sprite_bridge_at(grid_offset), color_mask);
}

void city_draw_bridge_tile(int x, int y, float scale, int bridge_sprite_id, color_t color_mask)
{
    int image_id = Image::group(GROUP_BUILDING_BRIDGE);
    switch (bridge_sprite_id) {
        case 1:
            Image::from_id(image_id + 5).draw(x, y - 20, color_mask, scale);
            break;
        case 2:
            Image::from_id(image_id).draw(x - 1, y - 8, color_mask, scale);
            break;
        case 3:
            Image::from_id(image_id + 3).draw(x, y - 8, color_mask, scale);
            break;
        case 4:
            Image::from_id(image_id + 2).draw(x + 7, y - 20, color_mask, scale);
            break;
        case 5:
            Image::from_id(image_id + 4).draw(x, y - 21, color_mask, scale);
            break;
        case 6:
            Image::from_id(image_id + 1).draw(x + 5, y - 21, color_mask, scale);
            break;
        case 7:
            Image::from_id(image_id + 11).draw(x - 3, y - 50, color_mask, scale);
            break;
        case 8:
            Image::from_id(image_id + 6).draw(x - 1, y - 12, color_mask, scale);
            break;
        case 9:
            Image::from_id(image_id + 9).draw(x - 30, y - 12, color_mask, scale);
            break;
        case 10:
            Image::from_id(image_id + 8).draw(x - 23, y - 53, color_mask, scale);
            break;
        case 11:
            Image::from_id(image_id + 10).draw(x, y - 37, color_mask, scale);
            break;
        case 12:
            Image::from_id(image_id + 7).draw(x + 7, y - 38, color_mask, scale);
            break;
            // Note: no nr 13 //note: could've noted why.
        case 14:
            Image::from_id(image_id + 13).draw(x, y - 38, color_mask, scale);
            break;
        case 15:
            Image::from_id(image_id + 12).draw(x + 7, y - 38, color_mask, scale);
            break;
    }
}
