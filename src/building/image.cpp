#include "building/connectable.h"
#include "building/rotation.h"
#include "building/variant.h"
#include "game/resource_graphics.h"
#include "map/building.h"

#include "building/building_record.h"
#include "building/building.h"

extern "C" {
#include "image.h"

#include "assets/assets.h"
#include "building/monument.h"
#include "building/properties.h"
#include "city/view.h"
#include "core/direction.h"
#include "core/image.h"
#include "core/image_group.h"
#include "core/random.h"
#include "game/resource.h"
#include "map/random.h"
#include "map/terrain.h"
#include "scenario/property.h"
}

#include "building/building_type_api.h"

struct type_image_handler {
    const char *text_id;
    int (*image)(const building *b);
};

struct typed_asset {
    const char *text_id;
    const char *group;
    const char *image;
};

static building_type xml_type(const char *text_id)
{
    return building_type_registry_runtime_id_from_text(text_id);
}

static int type_matches(building_type type, const char *text_id)
{
    building_type resolved = xml_type(text_id);
    return resolved != BUILDING_NONE && type == resolved;
}

static int type_index(building_type type, const char *const *text_ids, int count)
{
    for (int i = 0; i < count; i++) {
        if (type_matches(type, text_ids[i])) {
            return i;
        }
    }
    return -1;
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

static int climate_asset(const char *northern, const char *desert, const char *central)
{
    switch (scenario_property_climate()) {
        case CLIMATE_NORTHERN:
            return assets_get_image_id("Military", northern);
        case CLIMATE_DESERT:
            return assets_get_image_id("Military", desert);
        default:
            return assets_get_image_id("Military", central);
    }
}

static int image_group_only(int group)
{
    return image_group(group);
}

static int image_building_variant_with_rotation(const building *b)
{
    return building_variant_get_image_id_with_rotation(b->type, b->variant);
}

static int image_small_statue(const building *b)
{
    return assets_get_image_id("Aesthetics", "V Small Statue") +
        orientation_pair_offset(b) * building_properties_for_type(b->type)->rotation_offset;
}

static int image_medium_statue(const building *b)
{
    if (orientation_pair_offset(b)) {
        return assets_get_image_id("Aesthetics", "Med_Statue_R");
    }
    return image_group(GROUP_BUILDING_STATUE) + 1;
}

static int image_pavilion_red(const building *b)
{
    return assets_get_image_id("Aesthetics", "pavilion red");
}

static int image_pavilion_orange(const building *b)
{
    return assets_get_image_id("Aesthetics", "pavilion orange");
}

static int image_pavilion_yellow(const building *b)
{
    return assets_get_image_id("Aesthetics", "pavilion yellow");
}

static int image_pavilion_green(const building *b)
{
    return assets_get_image_id("Aesthetics", "pavilion green");
}

static int image_obelisk(const building *b)
{
    return assets_get_image_id("Aesthetics", "obelisk");
}

static int image_city_mint(const building *b)
{
    switch (b->monument.phase) {
        case MONUMENT_START:
            return assets_get_image_id("Monuments", "City_Mint_Construction_01");
        case 2:
            return assets_get_image_id("Monuments", "City_Mint_Construction_02");
        default:
            return building_variant_get_image_id_with_rotation(b->type, b->variant);
    }
}

static int image_granary(const building *b)
{
    return image_group_only(GROUP_BUILDING_GRANARY);
}

static int image_mission_post(const building *b)
{
    return image_group_only(GROUP_BUILDING_MISSION_POST);
}

static int image_military_academy(const building *b)
{
    return image_group_only(GROUP_BUILDING_MILITARY_ACADEMY);
}

static int image_no_fallback(const building *b)
{
    return 0;
}

static int image_shipyard(const building *b)
{
    return image_group(GROUP_BUILDING_SHIPYARD) +
        city_view_adjusted_orientation(b->data.industry.orientation);
}

static int image_wharf(const building *b)
{
    return image_group(GROUP_BUILDING_WHARF) +
        city_view_adjusted_orientation(b->data.industry.orientation);
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

static int image_tower(const building *b)
{
    return image_group_only(GROUP_BUILDING_TOWER);
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

static int image_barracks(const building *b)
{
    return image_group_only(GROUP_BUILDING_BARRACKS);
}

static int image_warehouse(const building *b)
{
    return image_group_only(GROUP_BUILDING_WAREHOUSE);
}

static int image_warehouse_space(const building *b)
{
    int resource_id = b->subtype.warehouse_resource_id;
    int loads = 0;
    resource_type resource = RESOURCE_NONE;
    if (resource_id > RESOURCE_NONE && resource_id < RESOURCE_SLOT_COUNT) {
        loads = b->resources[resource_id];
        if (loads > 0) {
            resource = static_cast<resource_type>(resource_id);
        }
    }
    return resource_graphics(resource).storage_image(loads).image_id();
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
        image_id = assets_get_image_id("Monuments", "Circus NWSE 01") +
            ((phase - 1) * phase_offset);
    } else {
        image_id = assets_get_image_id("Monuments", "Circus NESW 01") +
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

    if (orientation == DIR_0_TOP) {
        static const int offsets[] = {0, 2, 4};
        image_id += offsets[building_part];
    } else if (orientation == DIR_4_BOTTOM) {
        static const int offsets[] = {4, 2, 0};
        image_id += offsets[building_part];
    } else if (orientation == DIR_6_LEFT) {
        static const int offsets[] = {0, 2, 4};
        image_id += offsets[building_part];
    } else {
        static const int offsets[] = {4, 2, 0};
        image_id += offsets[building_part];
    }
    return image_id;
}

static int image_fort(const building *b)
{
    return climate_asset("Fort_Main_North", "Fort_Main_South", "Fort_Main_Central");
}

static int image_fort_ground(const building *b)
{
    return image_group(GROUP_BUILDING_FORT) + 1;
}

static int image_native_hut_alt(const building *b)
{
    switch (scenario_property_climate()) {
        case CLIMATE_NORTHERN:
            return assets_get_image_id("Terrain_Maps", "Native_Hut_Northern_01") + (random_byte() & 1);
        case CLIMATE_DESERT:
            return assets_get_image_id("Terrain_Maps", "Native_Hut_Southern_01") + (random_byte() & 1);
        default:
            return assets_get_image_id("Terrain_Maps", "Native_Hut_Central_01") + (random_byte() & 1);
    }
}

static int image_native_crops(const building *b)
{
    return image_group_only(GROUP_BUILDING_FARM_CROPS);
}

static int image_mess_hall(const building *b)
{
    return climate_asset("Mess ON North", "Mess ON South", "Mess ON Central");
}

static int image_grand_garden(const building *b)
{
    return assets_get_image_id("Engineer", "Eng Guild ON");
}

static int image_horse_statue(const building *b)
{
    return assets_get_image_id("Aesthetics", "Eque Statue") + orientation_pair_offset(b);
}

static int image_dolphin_fountain(const building *b)
{
    return assets_get_image_id("Engineer", "Eng Guild ON");
}

static int image_legion_statue(const building *b)
{
    return assets_get_image_id("Aesthetics", "legio statue") +
        orientation_pair_offset(b) * building_properties_for_type(b->type)->rotation_offset;
}

static int image_watchtower(const building *b)
{
    int image_id = climate_asset("Watchtower N ON", "Watchtower S ON", "Watchtower C ON");
    return image_id + building_variant_get_offset_with_rotation(b->type, b->variant);
}

static int image_small_mausoleum(const building *b)
{
    switch (b->monument.phase) {
        case MONUMENT_START:
            return assets_get_image_id("Monuments", "Mausoleum_Small_Construction_01");
        case 2:
            return assets_get_image_id("Monuments", "Mausoleum_Small_Construction_02") + orientation_pair_offset(b);
        default:
            return assets_get_image_id("Monuments", "Mausoleum S") + orientation_pair_offset(b);
    }
}

static int image_large_mausoleum(const building *b)
{
    int offset = building_variant_get_offset_with_rotation(b->type, b->variant);
    switch (b->monument.phase) {
        case MONUMENT_START:
            return assets_get_image_id("Monuments", "Mausoleum L Cons");
        case 2:
            return assets_get_image_id("Monuments", "Mausoleum_Large_Construction_02") + offset;
        default:
            return assets_get_image_id("Monuments", "Mausoleum L") + offset;
    }
}

static int image_nymphaeum(const building *b)
{
    switch (b->monument.phase) {
        case MONUMENT_START:
            return assets_get_image_id("Monuments", "Pantheon_Const_00");
        case 2:
            return assets_get_image_id("Monuments", "Nymphaeum_Construction_02");
        default:
            return assets_get_image_id("Monuments", "Nymphaeum ON");
    }
}

static int image_tree(const building *b)
{
    static const char *const tree_types[] = {
        "pine_tree",
        "fir_tree",
        "oak_tree",
        "elm_tree",
        "fig_tree",
        "plum_tree",
        "palm_tree",
        "date_tree",
    };
    int index = type_index(b->type, tree_types, sizeof(tree_types) / sizeof(tree_types[0]));
    return index >= 0 ? assets_get_group_id("Aesthetics") + index : 0;
}

static int image_small_alt_statue(const building *b)
{
    static const char *const statue_types[] = {
        "goddess_statue",
        "senator_statue",
    };
    int index = type_index(b->type, statue_types, sizeof(statue_types) / sizeof(statue_types[0]));
    if (index < 0) {
        return 0;
    }
    return assets_get_image_id("Aesthetics", "sml statue 2") + index +
        orientation_pair_offset(b) * building_properties_for_type(b->type)->rotation_offset;
}

static int image_hedge_dark(const building *b)
{
    return assets_get_image_id("Aesthetics", "D Hedge 01") +
        building_connectable_get_hedge_offset(b->grid_offset);
}

static int image_hedge_light(const building *b)
{
    return assets_get_image_id("Aesthetics", "L Hedge 01") +
        building_connectable_get_hedge_offset(b->grid_offset);
}

static int image_colonnade(const building *b)
{
    return assets_get_image_id("Aesthetics", "G Colonnade 01") +
        building_connectable_get_colonnade_offset(b->grid_offset);
}

static int image_looped_garden_wall(const building *b)
{
    return assets_get_image_id("Aesthetics", "C Garden Wall 01") +
        building_connectable_get_garden_wall_offset(b->grid_offset);
}

static int image_roofed_garden_wall(const building *b)
{
    return assets_get_image_id("Aesthetics", "R Garden Wall 01") +
        building_connectable_get_garden_wall_offset(b->grid_offset);
}

static int image_panelled_garden_wall(const building *b)
{
    return assets_get_image_id("Aesthetics", "Garden_Wall_C_01") +
        building_connectable_get_garden_wall_offset(b->grid_offset);
}

static const typed_asset *path_asset_for_type(building_type type)
{
    static const typed_asset path_assets[] = {
        {"date_path", "Aesthetics", "path orn date"},
        {"elm_path", "Aesthetics", "path orn elm"},
        {"fig_path", "Aesthetics", "path orn fig"},
        {"fir_path", "Aesthetics", "path orn fir"},
        {"oak_path", "Aesthetics", "path orn oak"},
        {"palm_path", "Aesthetics", "path orn palm"},
        {"pine_path", "Aesthetics", "path orn pine"},
        {"plum_path", "Aesthetics", "path orn plum"},
        {"garden_path", "Aesthetics", "garden path r"},
    };
    for (const typed_asset &asset : path_assets) {
        if (type_matches(type, asset.text_id)) {
            return &asset;
        }
    }
    return nullptr;
}

static int image_garden_path(const building *b)
{
    int image_offset = building_connectable_get_garden_path_offset(b->grid_offset,
        CONTEXT_GARDEN_PATH_INTERSECTION);
    int image_group_id = assets_get_image_id("Aesthetics", "Garden Path 01");
    if (image_offset == -1) {
        int context = type_matches(b->type, "garden_path") ? CONTEXT_GARDEN_TREELESS_PATH : CONTEXT_GARDEN_TREE_PATH;
        image_offset = building_connectable_get_garden_path_offset(b->grid_offset, context);
        const typed_asset *asset = path_asset_for_type(b->type);
        image_group_id = asset ? assets_get_image_id(asset->group, asset->image) :
            assets_get_image_id("Aesthetics", "garden path r");
    }
    return image_group_id + image_offset;
}

static int image_burning_ruin(const building *b)
{
    if (building_was_tent(b)) {
        return image_group(GROUP_TERRAIN_RUBBLE_TENT);
    }
    return image_group(GROUP_TERRAIN_RUBBLE_GENERAL) + 9 * (map_random_get(b->grid_offset) & 3);
}

static int image_roofed_garden_gate(const building *b)
{
    return assets_get_image_id("Aesthetics", "Garden_Gate_B") +
        building_connectable_get_garden_gate_offset(b->grid_offset);
}

static int image_looped_garden_gate(const building *b)
{
    return assets_get_image_id("Aesthetics", "Garden_Gate_A") +
        building_connectable_get_garden_gate_offset(b->grid_offset);
}

static int image_panelled_garden_gate(const building *b)
{
    return assets_get_image_id("Aesthetics", "Garden_Gate_C") +
        building_connectable_get_garden_gate_offset(b->grid_offset);
}

static int image_hedge_gate_light(const building *b)
{
    return assets_get_image_id("Aesthetics", "L Hedge Gate") +
        building_connectable_get_hedge_gate_offset(b->grid_offset);
}

static int image_hedge_gate_dark(const building *b)
{
    return assets_get_image_id("Aesthetics", "D Hedge Gate") +
        building_connectable_get_hedge_gate_offset(b->grid_offset);
}

static int image_palisade(const building *b)
{
    switch (scenario_property_climate()) {
        case CLIMATE_NORTHERN:
            return assets_get_image_id("Military", "Pal Wall N 01") +
                building_connectable_get_palisade_offset(b->grid_offset);
        case CLIMATE_DESERT:
            return assets_get_image_id("Military", "Pal Wall S 01") +
                building_connectable_get_palisade_offset(b->grid_offset);
        default:
            return assets_get_image_id("Military", "Pal Wall C 01") +
                building_connectable_get_palisade_offset(b->grid_offset);
    }
}

static int image_palisade_gate(const building *b)
{
    return assets_get_image_id("Military", "Palisade_Gate") +
        building_connectable_get_palisade_gate_offset(b->grid_offset);
}

static int image_gladiator_statue(const building *b)
{
    return assets_get_image_id("Aesthetics", "Gladiator_Statue") +
        orientation_pair_offset(b) * building_properties_for_type(b->type)->rotation_offset;
}

static int image_highway(const building *b)
{
    return assets_get_image_id("Admin_Logistics", "Highway_Placement");
}

static int image_shrine(const building *b)
{
    static const typed_asset shrine_assets[] = {
        {"shrine_ceres", "Health_Culture", "Altar_Ceres"},
        {"shrine_mars", "Health_Culture", "Altar_Mars"},
        {"shrine_mercury", "Health_Culture", "Altar_Mercury"},
        {"shrine_neptune", "Health_Culture", "Altar_Neptune"},
        {"shrine_venus", "Health_Culture", "Altar_Venus"},
    };
    for (const typed_asset &asset : shrine_assets) {
        if (type_matches(b->type, asset.text_id)) {
            return assets_get_image_id(asset.group, asset.image) + orientation_pair_offset(b);
        }
    }
    return 0;
}

static int image_overgrown_gardens(const building *b)
{
    return building_properties_for_type(b->type)->image_group;
}

static int type_handler_image(const building *b, int *image_id)
{
    static const type_image_handler handlers[] = {
        {"roadblock", image_building_variant_with_rotation},
        {"small_statue", image_small_statue},
        {"medium_statue", image_medium_statue},
        {"pavilion_blue", image_building_variant_with_rotation},
        {"pavilion_red", image_pavilion_red},
        {"pavilion_orange", image_pavilion_orange},
        {"pavilion_yellow", image_pavilion_yellow},
        {"pavilion_green", image_pavilion_green},
        {"obelisk", image_obelisk},
        {"city_mint", image_city_mint},
        {"granary", image_granary},
        {"mission_post", image_mission_post},
        {"military_academy", image_military_academy},
        {"decorative_column", image_building_variant_with_rotation},
        {"low_bridge", image_no_fallback},
        {"ship_bridge", image_no_fallback},
        {"shipyard", image_shipyard},
        {"wharf", image_wharf},
        {"dock", image_dock},
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
        {"native_hut_alt", image_native_hut_alt},
        {"native_crops", image_native_crops},
        {"mess_hall", image_mess_hall},
        {"grand_garden", image_grand_garden},
        {"horse_statue", image_horse_statue},
        {"dolphin_fountain", image_dolphin_fountain},
        {"legion_statue", image_legion_statue},
        {"watchtower", image_watchtower},
        {"small_mausoleum", image_small_mausoleum},
        {"large_mausoleum", image_large_mausoleum},
        {"nymphaeum", image_nymphaeum},
        {"pine_tree", image_tree},
        {"fir_tree", image_tree},
        {"oak_tree", image_tree},
        {"elm_tree", image_tree},
        {"fig_tree", image_tree},
        {"plum_tree", image_tree},
        {"palm_tree", image_tree},
        {"date_tree", image_tree},
        {"goddess_statue", image_small_alt_statue},
        {"senator_statue", image_small_alt_statue},
        {"hedge_dark", image_hedge_dark},
        {"hedge_light", image_hedge_light},
        {"colonnade", image_colonnade},
        {"looped_garden_wall", image_looped_garden_wall},
        {"roofed_garden_wall", image_roofed_garden_wall},
        {"panelled_garden_wall", image_panelled_garden_wall},
        {"date_path", image_garden_path},
        {"elm_path", image_garden_path},
        {"fig_path", image_garden_path},
        {"fir_path", image_garden_path},
        {"oak_path", image_garden_path},
        {"palm_path", image_garden_path},
        {"pine_path", image_garden_path},
        {"plum_path", image_garden_path},
        {"garden_path", image_garden_path},
        {"burning_ruin", image_burning_ruin},
        {"garden_wall_gate", image_roofed_garden_gate},
        {"roofed_garden_wall_gate", image_roofed_garden_gate},
        {"looped_garden_gate", image_looped_garden_gate},
        {"panelled_garden_gate", image_panelled_garden_gate},
        {"hedge_gate_light", image_hedge_gate_light},
        {"hedge_gate_dark", image_hedge_gate_dark},
        {"palisade", image_palisade},
        {"palisade_gate", image_palisade_gate},
        {"gladiator_statue", image_gladiator_statue},
        {"highway", image_highway},
        {"shrine_ceres", image_shrine},
        {"shrine_mars", image_shrine},
        {"shrine_mercury", image_shrine},
        {"shrine_neptune", image_shrine},
        {"shrine_venus", image_shrine},
        {"overgrown_gardens", image_overgrown_gardens},
    };

    for (const type_image_handler &handler : handlers) {
        if (type_matches(b->type, handler.text_id)) {
            *image_id = handler.image(b);
            return 1;
        }
    }
    return 0;
}

int building_image_get_base_farm_crop(building_type type)
{
    static const char *const crop_types[] = {
        "wheat_farm",
        "native_crops",
        "vegetable_farm",
        "fruit_farm",
        "olive_farm",
        "vines_farm",
        "pig_farm",
    };
    int index = type_index(type, crop_types, sizeof(crop_types) / sizeof(crop_types[0]));
    if (index < 0) {
        return 0;
    }
    if (index <= 1) {
        return image_group(GROUP_BUILDING_FARM_CROPS);
    }
    return image_group(GROUP_BUILDING_FARM_CROPS) + (index - 1) * 5;
}

int building_image_get_garden_gate_image(int grid_offset)
{
    building_type type = map_building_type_at(grid_offset);
    building_type gate_type = static_cast<building_type>(building_connectable_gate_type(type));
    building fake_gate = {};
    fake_gate.type = gate_type;
    fake_gate.grid_offset = grid_offset;

    int image_id = 0;
    if (gate_type != BUILDING_NONE && type_handler_image(&fake_gate, &image_id)) {
        return image_id;
    }
    return 0;
}

int building_image_get(const building *b)
{
    if (!b) {
        return 0;
    }

    int xml_image_id = building_type_registry_get_graphics_image_id(b);
    if (xml_image_id) {
        return xml_image_id;
    }

    if (b->type == building_type_registry_get_vacant_lot_fill_type() && b->house_population == 0) {
        return image_group(GROUP_BUILDING_HOUSE_VACANT_LOT);
    }

    int image_id = 0;
    if (type_handler_image(b, &image_id)) {
        return image_id;
    }
    return 0;
}

int building_image_get_for_type(building_type type)
{
    building b = {};
    b.type = type;
    return building_image_get(&b);
}
