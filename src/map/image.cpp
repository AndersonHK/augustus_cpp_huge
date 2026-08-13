#include "image.h"

#include "building/building.h"
#include "building/BuildingGeometry.h"
#include "building/building_type.h"
#include "figure/figure.h"
#include "building/building_record.h"
#include "core/image.h"
#include "core/image_group.h"
#include "map/grid.h"
#include "map/orientation.h"
#include "map/tiles.h"

static grid_u32 images;
static grid_u32 images_backup;

unsigned int map_image_at(int grid_offset)
{
    return images.items[grid_offset];
}

void map_image_set(int grid_offset, int image_id)
{
    images.items[grid_offset] = image_id;
}

void map_image_backup(void)
{
    map_grid_copy_u32(images.items, images_backup.items);
}

void map_image_restore(void)
{
    map_grid_copy_u32(images_backup.items, images.items);
}

void map_image_restore_at(int grid_offset)
{
    images.items[grid_offset] = images_backup.items[grid_offset];
}

void map_image_clear(void)
{
    map_grid_clear_u32(images.items);
}

void map_image_init_edges(void)
{
    int width, height;
    map_grid_size(&width, &height);
    for (int x = 1; x < width; x++) {
        images.items[map_grid_offset(x, height)] = 1;
    }
    for (int y = 1; y < height; y++) {
        images.items[map_grid_offset(width, y)] = 2;
    }
    images.items[map_grid_offset(0, height)] = 3;
    images.items[map_grid_offset(width, 0)] = 4;
    images.items[map_grid_offset(width, height)] = 5;
}

static int building_is_drawn_by_terrain_tiles(const Building &building)
{
    if (!building.type || building.type->has_graphic()) {
        return 0;
    }
    const building_type_registry_impl::ConstructionToolDefinition &tool = building.type->tool();
    return tool.is_road() || tool.is_highway() || tool.is_aqueduct() || building.type->has_tile();
}

void map_image_update_all(void)
{
    map_tiles_update_all();
    Building::for_each([](Building *building) {
        if (!building) {
            return;
        }

        Building &b = *building;
        if (b.state_id() != BUILDING_STATE_IN_USE && b.state_id() != BUILDING_STATE_MOTHBALLED &&
            b.state_id() != BUILDING_STATE_CREATED) {
            return;
        }
    if ((b.type && b.type->bridge().is_bridge()) || b.matches("wall")) {
            return; //bridges are drawn as a part of terrain drawing, and their image shouldnt be fetched.
        }
        if (building_is_drawn_by_terrain_tiles(b)) {
            return;
        }
        if (b.refresh_graphic_if_native()) {
            return;
        }
        int image_id = b.image_id();

        const building_type_registry_impl::BuildingGeometry geometry =
            building_type_registry_impl::BuildingGeometry::query(b);
        for (const building_type_registry_impl::BuildingGeometryCell &cell : geometry.cells()) {
            map_image_set(map_grid_offset(cell.x, cell.y), image_id);
        }
    });
}

void map_image_save_state_legacy(buffer *buf)
{
    map_grid_save_state_u32_to_u16(images.items, buf);
}

void map_image_load_state_legacy(buffer *buf)
{
    map_grid_load_state_u16_to_u32(images.items, buf);
}
