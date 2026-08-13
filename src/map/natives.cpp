#include "figure/figure.h"
#include "building/building_record.h"
#include "natives.h"

#include "building/building.h"
#include "building/building_runtime_internal.h"
#include "building/building_type_registry_internal.h"
#include "building/roadblock.h"
#include "city/buildings.h"
#include "city/military.h"
#include "core/calc.h"
#include "core/image.h"
#include "core/image_group_editor.h"
#include "map/building.h"
#include "map/building_tiles.h"
#include "map/data.h"
#include "map/grid.h"
#include "map/image.h"
#include "map/property.h"
#include "map/random.h"
#include "map/terrain.h"
#include "scenario/data.h" // TODO remove this dependency
#include "scenario/property.h"

#include <algorithm>
#include <vector>

struct native_types {
    building_type hut = BUILDING_NONE;
    building_type alt_hut = BUILDING_NONE;
    building_type decor = BUILDING_NONE;
    building_type monument = BUILDING_NONE;
    building_type watchtower = BUILDING_NONE;
    building_type meeting = BUILDING_NONE;
    building_type crops = BUILDING_NONE;
};

static native_types resolve_native_types()
{
    return {
        building_type_registry_impl::type_from_attr("native_hut"),
        building_type_registry_impl::type_from_attr("native_hut_alt"),
        building_type_registry_impl::type_from_attr("native_decor"),
        building_type_registry_impl::type_from_attr("native_monument"),
        building_type_registry_impl::type_from_attr("native_watchtower"),
        building_type_registry_impl::type_from_attr("native_meeting"),
        building_type_registry_impl::type_from_attr("native_crops"),
    };
}

static void mark_native_land(int x, int y, int size, int radius)
{
    int x_min, y_min, x_max, y_max;
    map_grid_get_area(x, y, size, radius, &x_min, &y_min, &x_max, &y_max);
    for (int yy = y_min; yy <= y_max; yy++) {
        for (int xx = x_min; xx <= x_max; xx++) {
            map_property_mark_native_land(map_grid_offset(xx, yy));
        }
    }
}

static int has_building_on_native_land(int x, int y, int size, int radius)
{
    int x_min, y_min, x_max, y_max;
    map_grid_get_area(x, y, size, radius, &x_min, &y_min, &x_max, &y_max);
    for (int yy = y_min; yy <= y_max; yy++) {
        for (int xx = x_min; xx <= x_max; xx++) {
            int grid_offset = map_grid_offset(xx, yy);
            if (map_building_exists_at(grid_offset)) {
                Building existing = map_building_at(grid_offset);
                building_type type = existing.type ? existing.type->type() : BUILDING_NONE;
                static const char *const native_land_allowed[] = {
                    "mission_post",
                    "native_hut",
                    "native_hut_alt",
                    "native_meeting",
                    "native_crops",
                    "native_decor",
                    "native_monument",
                    "native_watchtower",
                };
                static const char *const roadblock_pass_allowed[] = {
                    "palisade_gate",
                    "triumphal_arch",
                    "gatehouse",
                };
                const int is_storage = existing.type && existing.type->is_storage();
                if (!building_type_registry_impl::type_attr_is_any(type, native_land_allowed,
                        sizeof(native_land_allowed) / sizeof(native_land_allowed[0])) &&
                    (Roadblock(existing).kind() == ROADBLOCK_NONE ||
                        building_type_registry_impl::type_attr_is_any(type, roadblock_pass_allowed,
                            sizeof(roadblock_pass_allowed) / sizeof(roadblock_pass_allowed[0])) ||
                        is_storage)) {
                    return 1;
                }
            } else if (map_terrain_is(map_grid_offset(xx, yy), TERRAIN_AQUEDUCT | TERRAIN_WALL | TERRAIN_GARDEN)) {
                return 1;
            }
        }
    }
    return 0;
}

static void determine_meeting_center(const native_types &types)
{
    std::vector<Building *> meetings;
    if (types.meeting != BUILDING_NONE) {
        for (Building &meeting : Building::of_type(types.meeting)) {
            meetings.push_back(&meeting);
        }
    }

    const building_type hut_types[] = { types.hut, types.alt_hut };
    for (building_type hut_type : hut_types) {
        if (hut_type == BUILDING_NONE) {
            continue;
        }
        // Determine closest meeting center for hut
        for (Building &hut : Building::of_type(hut_type)) {
            building *record = const_cast<building *>(hut.record());
            if (!record || !hut.is_in_use()) {
                continue;
            }
            int min_dist = 1000;
            int min_meeting_id = 0;
            for (const Building *meeting : meetings) {
                int dist = calc_maximum_distance(hut.x(), hut.y(), meeting->x(), meeting->y());
                if (dist < min_dist) {
                    min_dist = dist;
                    min_meeting_id = meeting->id;
                }
            }
            record->subtype.native_meeting_center_id = static_cast<short>(min_meeting_id);
        }
    }
}

struct native_tile_match {
    building_type type = BUILDING_NONE;
    int graphics_variant = 0;
};

struct native_tile_context {
    native_types types;
};

static native_tile_context make_native_tile_context()
{
    native_tile_context context;
    context.types = resolve_native_types();
    return context;
}

static native_tile_match native_tile_match_for_image(
    int source_image_id,
    const native_tile_context &context)
{
    if (source_image_id == scenario.native_images.hut) {
        return { context.types.hut, 0 };
    }
    if (source_image_id == scenario.native_images.hut + 1) {
        return { context.types.hut, 1 };
    }
    if (scenario.native_images.alt_hut != 0 && source_image_id == scenario.native_images.alt_hut) {
        return { context.types.alt_hut, 0 };
    }
    if (scenario.native_images.alt_hut != 0 && source_image_id == scenario.native_images.alt_hut + 1) {
        return { context.types.alt_hut, 1 };
    }
    if (scenario.native_images.decoration != 0 && source_image_id == scenario.native_images.decoration) {
        return { context.types.decor };
    }
    if (scenario.native_images.monument != 0 && source_image_id == scenario.native_images.monument) {
        return { context.types.monument };
    }
    if (scenario.native_images.watchtower != 0 && source_image_id == scenario.native_images.watchtower) {
        return { context.types.watchtower };
    }
    if (source_image_id == scenario.native_images.meeting) {
        return { context.types.meeting };
    }
    if (source_image_id == scenario.native_images.crops) {
        return { context.types.crops };
    }
    return {};
}

void map_natives_init(void)
{
    const native_tile_context context = make_native_tile_context();
    int grid_offset = map_data.start_offset;
    for (int y = 0; y < map_data.height; y++, grid_offset += map_data.border_size) {
        for (int x = 0; x < map_data.width; x++, grid_offset++) {
            if (!map_terrain_is(grid_offset, TERRAIN_BUILDING) || map_building_exists_at(grid_offset)) {
                continue;
            }

            int random_bit = map_random_get(grid_offset) & 1;
            native_tile_match match = native_tile_match_for_image(
                map_image_at(grid_offset),
                context);
            building_type type = match.type;
            if (type == BUILDING_NONE) {
                map_legacy_building_tiles_remove(x, y);
                continue;
            }
            const building_type_registry_impl::BuildingType *definition =
                building_type_registry_impl::definition_for_type(type);
            if (!definition) {
                continue;
            }
            Building &building_obj = city_building_runtime().create(*definition, x, y);
            building *b = const_cast<building *>(building_obj.record());
            b->state = BUILDING_STATE_IN_USE;
            if (type == context.types.crops) {
                b->data.industry.progress = static_cast<short>(random_bit);
            } else if (type == context.types.meeting) {
                b->native_anger = 100;
                mark_native_land(b->x, b->y, 2, 6);
            } else {
                if (type == context.types.hut || type == context.types.alt_hut ||
                    type == context.types.watchtower) {
                    b->native_anger = 100;
                    b->figure_spawn_delay = static_cast<unsigned char>(random_bit);
                    mark_native_land(b->x, b->y, 1, 3);
                }
            }
            building_obj.add_map_tiles();
            building_obj.Graphics().set_variant(match.graphics_variant);
            building_obj.refresh_graphic();
        }
    }

    determine_meeting_center(context.types);
}

void map_natives_init_editor(void)
{
    const native_tile_context context = make_native_tile_context();

    int grid_offset = map_data.start_offset;
    for (int y = 0; y < map_data.height; y++, grid_offset += map_data.border_size) {
        for (int x = 0; x < map_data.width; x++, grid_offset++) {
            if (!map_terrain_is(grid_offset, TERRAIN_BUILDING) || map_building_exists_at(grid_offset)) {
                continue;
            }

            native_tile_match match = native_tile_match_for_image(
                map_image_at(grid_offset),
                context);
            building_type type = match.type;
            if (type == BUILDING_NONE) {
                map_legacy_building_tiles_remove(x, y);
                continue;
            }
            const building_type_registry_impl::BuildingType *definition =
                building_type_registry_impl::definition_for_type(type);
            if (!definition) {
                continue;
            }
            Building &building_obj = city_building_runtime().create(*definition, x, y);
            building *b = const_cast<building *>(building_obj.record());
            b->state = BUILDING_STATE_IN_USE;
            building_obj.add_map_tiles();
            building_obj.Graphics().set_variant(match.graphics_variant);
            building_obj.refresh_graphic();
        }
    }
}

void map_natives_check_land(int update_behavior)
{
    map_property_clear_all_native_land();
    if (update_behavior) {
        city_military_decrease_native_attack_duration();
    }

    const native_types types = resolve_native_types();
    const building_type native_buildings[] = {
        types.hut,
        types.alt_hut,
        types.meeting,
        types.watchtower,
    };

    for (building_type type : native_buildings) {
        if (type == BUILDING_NONE) {
            continue;
        }
        const building_type_registry_impl::BuildingType *definition =
            building_type_registry_impl::definition_for_type(type);
        const building_type_registry_impl::FoundationDef *foundation =
            definition ? definition->foundation_def() : nullptr;
        const int size = foundation ? std::max(foundation->width(), foundation->height()) : 0;
        if (size <= 0) {
            continue;
        }
        int radius = size * 3;
        for (Building &native : Building::of_type(type)) {
            building *record = const_cast<building *>(native.record());
            if (!record || !native.is_in_use()) {
                continue;
            }
            if (record->native_anger >= 100) {
                mark_native_land(native.x(), native.y(), size, radius);
                if (!city_military_natives_are_retreating() && update_behavior && 
                    has_building_on_native_land(native.x(), native.y(), size, radius)) {
                    city_military_start_native_attack();
                }
            } else if (update_behavior) {
                record->native_anger++;
            }
        }
    }
}
