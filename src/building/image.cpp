#include "image.h"

#include "assets/assets.h"
#include "building/BuildingGraphicsState.h"
#include "building/building.h"
#include "building/building_record.h"
#include "building/building_runtime_internal.h"
#include "building/building_runtime_graphics.h"
#include "building/building_type.h"
#include "building/building_type_registry_internal.h"
#include "building/connectable.h"
#include "building/monument.h"
#include "building/properties.h"
#include "building/rotation.h"
#include "city/view.h"
#include "core/direction.h"
#include "core/image.h"
#include "core/image_group.h"
#include "map/building.h"
#include "map/random.h"
#include "map/terrain.h"

struct type_image_handler {
    const char *text_id;
    int (*image)(const building *b);
};

static const ::building *record_for(const Building *building)
{
    return building ? building->record() : nullptr;
}

static unsigned char graphics_variant_for_image_lookup(const Building *building)
{
    const ::building *record = record_for(building);
    if (!record || !record->id) {
        return 0;
    }
    BuildingGraphicsState loaded_graphics_state;
    if (building_runtime_loaded_graphics_state(record->id, &loaded_graphics_state)) {
        return loaded_graphics_state.variant();
    }
    if (building_runtime *runtime = building_runtime_impl::get_city_building(const_cast<::building *>(record))) {
        return runtime->graphics_variant();
    }
    return 0;
}

static int orientation_pair(const building *b)
{
    return building_rotation_get_building_orientation(b->subtype.orientation) / 2;
}

static int orientation_pair_offset(const building *b)
{
    return orientation_pair(b) % 2;
}

static int city_view_adjusted_orientation(int orientation)
{
    return (4 + orientation - city_view_orientation() / 2) % 4;
}

static int image_small_statue(const building *b)
{
    return orientation_pair_offset(b) ?
        assets_get_image_id("Aesthetics\\V_Small_Statue_R", "V Small Statue R") :
        assets_get_image_id("Aesthetics\\V_Small_Statue", "V Small Statue");
}

static int image_medium_statue(const building *b)
{
    if (orientation_pair_offset(b)) {
        return assets_get_image_id("Aesthetics\\Med_Statue_R", "Med_Statue_R");
    }
    return image_group(GROUP_BUILDING_STATUE) + 1;
}

static int image_no_fallback(const building *)
{
    return 0;
}

static int image_mission_post(const building *)
{
    return image_group(GROUP_BUILDING_MISSION_POST);
}

static int image_military_academy(const building *)
{
    return image_group(GROUP_BUILDING_MILITARY_ACADEMY);
}

static int image_tower(const building *)
{
    return image_group(GROUP_BUILDING_TOWER);
}

static int image_barracks(const building *)
{
    return image_group(GROUP_BUILDING_BARRACKS);
}

static int image_warehouse(const building *)
{
    return image_group(GROUP_BUILDING_WAREHOUSE);
}

static int image_fort_ground(const building *)
{
    return image_group(GROUP_BUILDING_FORT) + 1;
}

static int image_dock(const building *b)
{
    switch (city_view_adjusted_orientation(b->data.dock.orientation)) {
        case 0:
            return image_group(GROUP_BUILDING_DOCK_1);
        case 1:
            return image_group(GROUP_BUILDING_DOCK_2);
        case 2:
            return image_group(GROUP_BUILDING_DOCK_3);
        default:
            return image_group(GROUP_BUILDING_DOCK_4);
    }
}

static int image_gatehouse(const building *b)
{
    int map_orientation = city_view_orientation();
    int orientation_is_top_bottom = map_orientation == DIR_0_TOP || map_orientation == DIR_4_BOTTOM;
    if (b->subtype.orientation == 1) {
        return image_group(GROUP_BUILDING_TOWER) + (orientation_is_top_bottom ? 1 : 2);
    }
    return image_group(GROUP_BUILDING_TOWER) + (orientation_is_top_bottom ? 2 : 1);
}

static int image_triumphal_arch(const building *b)
{
    int map_orientation = city_view_orientation();
    int orientation_is_top_bottom = map_orientation == DIR_0_TOP || map_orientation == DIR_4_BOTTOM;
    if (b->subtype.orientation == 1) {
        return image_group(GROUP_BUILDING_TRIUMPHAL_ARCH) + (orientation_is_top_bottom ? 0 : 2);
    }
    return image_group(GROUP_BUILDING_TRIUMPHAL_ARCH) + (orientation_is_top_bottom ? 2 : 0);
}

static int image_warehouse_space(const building *b)
{
    (void) b;
    return image_group(GROUP_BUILDING_WAREHOUSE_STORAGE_EMPTY);
}

static int image_hippodrome(const building *b)
{
    int phase = b->monument.phase;
    if (!phase) {
        phase = MONUMENT_FINISHED;
    }
    int phase_offset = 6;
    int orientation = building_rotation_get_building_orientation(b->subtype.orientation);
    int image_id;

    if (phase == MONUMENT_FINISHED) {
        if (orientation == DIR_0_TOP || orientation == DIR_4_BOTTOM) {
            image_id = image_group(GROUP_BUILDING_HIPPODROME_2);
        } else {
            image_id = image_group(GROUP_BUILDING_HIPPODROME_1);
        }
    } else if (orientation == DIR_0_TOP || orientation == DIR_4_BOTTOM) {
        image_id = assets_get_image_id("Monuments\\Circus_NWSE_01", "Circus NWSE 01") +
            ((phase - 1) * phase_offset);
    } else {
        image_id = assets_get_image_id("Monuments\\Circus_NESW_01", "Circus NESW 01") +
            ((phase - 1) * phase_offset);
    }

    int building_part;
    if (b->prev_part_building_id == 0) {
        building_part = 0;
    } else if (b->next_part_building_id == 0) {
        building_part = 2;
    } else {
        building_part = 1;
    }

    if (orientation == DIR_0_TOP || orientation == DIR_6_LEFT) {
        static const int offsets[] = { 0, 2, 4 };
        image_id += offsets[building_part];
    } else {
        static const int offsets[] = { 4, 2, 0 };
        image_id += offsets[building_part];
    }
    return image_id;
}

static int image_fort(const building *)
{
    switch (city_view_orientation() / 2) {
        case 0:
        case 2:
            return assets_get_image_id("Military\\Fort_Main_Central", "Fort_Main_Central");
        default:
            return assets_get_image_id("Military\\Fort_Main_Central", "Fort_Main_North");
    }
}

static int image_burning_ruin(const building *b)
{
    if (building_was_tent(b)) {
        return image_group(GROUP_TERRAIN_RUBBLE_TENT);
    }
    return image_group(GROUP_TERRAIN_RUBBLE_GENERAL) + 9 * (map_random_get(b->grid_offset) & 3);
}

static int type_handler_image(const Building *building, int *image_id)
{
    const ::building *record = record_for(building);
    if (!building || !building->type || !record) {
        return 0;
    }
    if (building->type->attr_is("dock")) {
        *image_id = image_dock(record);
        return 1;
    }

    static const type_image_handler handlers[] = {
        {"small_statue", image_small_statue},
        {"medium_statue", image_medium_statue},
        {"mission_post", image_mission_post},
        {"military_academy", image_military_academy},
        {"low_bridge", image_no_fallback},
        {"ship_bridge", image_no_fallback},
        {"tower", image_tower},
        {"gatehouse", image_gatehouse},
        {"triumphal_arch", image_triumphal_arch},
        {"barracks", image_barracks},
        {"warehouse", image_warehouse},
        {"warehouse_space", image_warehouse_space},
        {"hippodrome", image_hippodrome},
        {"fort_javelin", image_fort},
        {"fort_legionaries", image_fort},
        {"fort_mounted", image_fort},
        {"fort_swords", image_fort},
        {"fort_archers", image_fort},
        {"fort_ground", image_fort_ground},
        {"burning_ruin", image_burning_ruin},
    };

    for (const type_image_handler &handler : handlers) {
        if (building->type->attr_is(handler.text_id)) {
            *image_id = handler.image(record);
            return 1;
        }
    }
    return 0;
}

int building_image_get_garden_gate_image(int grid_offset)
{
    building_type type = map_building_type_at(grid_offset);
    building_type gate_type = static_cast<building_type>(building_connectable_gate_type(type));
    if (gate_type == BUILDING_NONE) {
        return 0;
    }

    building fake_gate = {};
    fake_gate.type = gate_type;
    fake_gate.state = BUILDING_STATE_IN_USE;
    fake_gate.grid_offset = static_cast<short>(grid_offset);
    fake_gate.size = 1;
    BuildingGraphicsState graphics_state;
    Building gate(fake_gate, building_type_registry_impl::definition_for_type(gate_type), graphics_state);
    return building_runtime_graphics_image_id(gate, 0);
}

int building_image_get(const Building *building)
{
    const ::building *record = record_for(building);
    if (!building || !record) {
        return 0;
    }

    int xml_image_id = building_runtime_graphics_image_id(*building, graphics_variant_for_image_lookup(building));
    if (xml_image_id) {
        return xml_image_id;
    }

    if (building->type && building->type->is_vacant_lot() && record->house_population == 0) {
        return image_group(GROUP_BUILDING_HOUSE_VACANT_LOT);
    }

    int image_id = 0;
    if (type_handler_image(building, &image_id)) {
        return image_id;
    }
    return 0;
}

int building_image_get_for_type(const building_type_registry_impl::BuildingType *definition)
{
    if (!definition) {
        return 0;
    }
    building b = {};
    b.type = definition->type();
    b.state = BUILDING_STATE_IN_USE;
    const int declared_size = definition->declared_model_size();
    b.size = static_cast<unsigned char>(declared_size > 0 ? declared_size : 1);
    BuildingGraphicsState graphics_state;
    Building building(b, definition, graphics_state);
    return building_image_get(&building);
}
