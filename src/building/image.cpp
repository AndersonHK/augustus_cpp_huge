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
#include "building/properties.h"
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

static int image_tower(const building *)
{
    return image_group(GROUP_BUILDING_TOWER);
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
    static const type_image_handler handlers[] = {
        {"tower", image_tower},
        {"triumphal_arch", image_triumphal_arch},
        {"fort_javelin", image_fort},
        {"fort_legionaries", image_fort},
        {"fort_mounted", image_fort},
        {"fort_swords", image_fort},
        {"fort_archers", image_fort},
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

    if (building->type && building->type->is_vacant_lot() && building->Housing &&
        building->Housing->state().population == 0) {
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
    BuildingGraphicsState graphics_state;
    Building building(b, definition, graphics_state);
    return building_image_get(&building);
}
