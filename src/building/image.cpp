extern "C" {
#include "image.h"

#include "assets/assets.h"
#include "building/building_type_api.h"
#include "building/connectable.h"
#include "building/monument.h"
#include "building/properties.h"
#include "building/rotation.h"
#include "building/variant.h"
#include "city/view.h"
#include "core/direction.h"
#include "core/image.h"
#include "core/image_group.h"
#include "core/random.h"
#include "game/resource.h"
#include "map/building.h"
#include "map/random.h"
#include "map/terrain.h"
#include "scenario/property.h"
}

int building_image_get_base_farm_crop(building_type type)
{
    switch (type) {
        case BUILDING_WHEAT_FARM:
        case BUILDING_NATIVE_CROPS:
            return image_group(GROUP_BUILDING_FARM_CROPS);
        case BUILDING_VEGETABLE_FARM:
            return image_group(GROUP_BUILDING_FARM_CROPS) + 5;
        case BUILDING_FRUIT_FARM:
            return image_group(GROUP_BUILDING_FARM_CROPS) + 10;
        case BUILDING_OLIVE_FARM:
            return image_group(GROUP_BUILDING_FARM_CROPS) + 15;
        case BUILDING_VINES_FARM:
            return image_group(GROUP_BUILDING_FARM_CROPS) + 20;
        case BUILDING_PIG_FARM:
            return image_group(GROUP_BUILDING_FARM_CROPS) + 25;
        default:
            return 0;
    }
}

int building_image_get_garden_gate_image(int grid_offset)
{
    building_type b_type = map_building_type_at(grid_offset);
    b_type = static_cast<building_type>(building_connectable_gate_type(b_type));

    switch (b_type) {
        case BUILDING_ROOFED_GARDEN_WALL_GATE:
            return assets_get_image_id("Aesthetics", "Garden_Gate_B") + building_connectable_get_garden_gate_offset(grid_offset);
        case BUILDING_LOOPED_GARDEN_GATE:
            return assets_get_image_id("Aesthetics", "Garden_Gate_A") + building_connectable_get_garden_gate_offset(grid_offset);
        case BUILDING_PANELLED_GARDEN_GATE:
            return assets_get_image_id("Aesthetics", "Garden_Gate_C") + building_connectable_get_garden_gate_offset(grid_offset);
        case BUILDING_HEDGE_GATE_LIGHT:
            return assets_get_image_id("Aesthetics", "L Hedge Gate") + building_connectable_get_hedge_gate_offset(grid_offset);
        case BUILDING_HEDGE_GATE_DARK:
            return assets_get_image_id("Aesthetics", "D Hedge Gate") + building_connectable_get_hedge_gate_offset(grid_offset);
        default:
            return 0;
    }
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

    switch (b->type) {
        case BUILDING_HOUSE_VACANT_LOT:
            if (b->house_population == 0) {
                return image_group(GROUP_BUILDING_HOUSE_VACANT_LOT);
            }
            [[fallthrough]];
        case BUILDING_SMALL_STATUE:
        {
            int orientation = building_rotation_get_building_orientation(b->subtype.orientation) / 2;
            return assets_get_image_id("Aesthetics", "V Small Statue") +
                (orientation % 2) * building_properties_for_type(b->type)->rotation_offset;
        }
        case BUILDING_MEDIUM_STATUE:
        {
            int orientation = building_rotation_get_building_orientation(b->subtype.orientation) / 2;
            switch (orientation % 2) {
                case 1:
                    return assets_get_image_id("Aesthetics", "Med_Statue_R");
                default:
                    return image_group(GROUP_BUILDING_STATUE) + 1;
            }
        }
        case BUILDING_PAVILION_BLUE:
            return building_variant_get_image_id_with_rotation(b->type, b->variant);
        case BUILDING_PAVILION_RED:
            return assets_get_image_id("Aesthetics", "pavilion red");
        case BUILDING_PAVILION_ORANGE:
            return assets_get_image_id("Aesthetics", "pavilion orange");
        case BUILDING_PAVILION_YELLOW:
            return assets_get_image_id("Aesthetics", "pavilion yellow");
        case BUILDING_PAVILION_GREEN:
            return assets_get_image_id("Aesthetics", "pavilion green");
        case BUILDING_OBELISK:
            return assets_get_image_id("Aesthetics", "obelisk");
        case BUILDING_CITY_MINT:
            switch (b->monument.phase) {
                case MONUMENT_START:
                    return assets_get_image_id("Monuments", "City_Mint_Construction_01");
                case 2:
                    return assets_get_image_id("Monuments", "City_Mint_Construction_02");
                default:
                    return building_variant_get_image_id_with_rotation(b->type, b->variant);
            }
        case BUILDING_GRANARY:
            return image_group(GROUP_BUILDING_GRANARY);
        case BUILDING_MISSION_POST:
            return image_group(GROUP_BUILDING_MISSION_POST);
        case BUILDING_MILITARY_ACADEMY:
            return image_group(GROUP_BUILDING_MILITARY_ACADEMY);
        case BUILDING_ROADBLOCK:
        case BUILDING_DECORATIVE_COLUMN:
            return building_variant_get_image_id_with_rotation(b->type, b->variant);
        case BUILDING_LOW_BRIDGE:
        case BUILDING_SHIP_BRIDGE:
            return 0; //shouldn't happen, this is just fallback
        case BUILDING_SHIPYARD:
            return image_group(GROUP_BUILDING_SHIPYARD) +
                ((4 + b->data.industry.orientation - city_view_orientation() / 2) % 4);
        case BUILDING_WHARF:
            return image_group(GROUP_BUILDING_WHARF) +
                ((4 + b->data.industry.orientation - city_view_orientation() / 2) % 4);
        case BUILDING_DOCK:
            switch ((4 + b->data.dock.orientation - city_view_orientation() / 2) % 4) {
                case 0:
                    return image_group(GROUP_BUILDING_DOCK_1);
                case 1:
                    return image_group(GROUP_BUILDING_DOCK_2);
                case 2:
                    return image_group(GROUP_BUILDING_DOCK_3);
                default:
                    return image_group(GROUP_BUILDING_DOCK_4);
            }
        case BUILDING_TOWER:
            return image_group(GROUP_BUILDING_TOWER);
        case BUILDING_GATEHOUSE:
        {
            int map_orientation = city_view_orientation();
            int orientation_is_top_bottom = map_orientation == DIR_0_TOP || map_orientation == DIR_4_BOTTOM;
            if (b->subtype.orientation == 1) {
                if (orientation_is_top_bottom) {
                    return image_group(GROUP_BUILDING_TOWER) + 1;
                } else {
                    return image_group(GROUP_BUILDING_TOWER) + 2;
                }
            } else {
                if (orientation_is_top_bottom) {
                    return image_group(GROUP_BUILDING_TOWER) + 2;
                } else {
                    return image_group(GROUP_BUILDING_TOWER) + 1;
                }
            }
        }
        case BUILDING_TRIUMPHAL_ARCH:
        {
            int map_orientation = city_view_orientation();
            int orientation_is_top_bottom = map_orientation == DIR_0_TOP || map_orientation == DIR_4_BOTTOM;
            if (b->subtype.orientation == 1) {
                if (orientation_is_top_bottom) {
                    return image_group(GROUP_BUILDING_TRIUMPHAL_ARCH);
                } else {
                    return image_group(GROUP_BUILDING_TRIUMPHAL_ARCH) + 2;
                }
            } else {
                if (orientation_is_top_bottom) {
                    return image_group(GROUP_BUILDING_TRIUMPHAL_ARCH) + 2;
                } else {
                    return image_group(GROUP_BUILDING_TRIUMPHAL_ARCH);
                }
            }
        }
        case BUILDING_BARRACKS:
            return image_group(GROUP_BUILDING_BARRACKS);
        case BUILDING_WAREHOUSE:
            return image_group(GROUP_BUILDING_WAREHOUSE);
        case BUILDING_WAREHOUSE_SPACE:
        {
            int resource_id = b->subtype.warehouse_resource_id;
            if (resource_id <= RESOURCE_NONE || resource_id >= RESOURCE_MAX || b->resources[resource_id] <= 0) {
                return image_group(GROUP_BUILDING_WAREHOUSE_STORAGE_EMPTY);
            } else {
                resource_type resource = static_cast<resource_type>(resource_id);
                return resource_get_data(resource)->image.storage + b->resources[resource_id] - 1;
            }
        }
        case BUILDING_HIPPODROME:
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
            } else {
                if (orientation == DIR_0_TOP || orientation == DIR_4_BOTTOM) {
                    image_id = assets_get_image_id("Monuments", "Circus NWSE 01") +
                        ((phase - 1) * phase_offset);
                } else {
                    image_id = assets_get_image_id("Monuments", "Circus NESW 01") +
                        ((phase - 1) * phase_offset);
                }
            }
            int building_part;
            if (b->prev_part_building_id == 0) {
                building_part = 0; // part 1, no previous building
            } else if (b->next_part_building_id == 0) {
                building_part = 2; // part 3, no next building
            } else {
                building_part = 1; // part 2
            }

            if (orientation == DIR_0_TOP) {
                switch (building_part) {
                    case 0: image_id += 0; break; // part 1
                    case 1: image_id += 2; break; // part 2
                    case 2: image_id += 4; break; // part 3, same for switch cases below
                }
            } else if (orientation == DIR_4_BOTTOM) {
                switch (building_part) {
                    case 0: image_id += 4; break;
                    case 1: image_id += 2; break;
                    case 2: image_id += 0; break;
                }
            } else if (orientation == DIR_6_LEFT) {
                switch (building_part) {
                    case 0: image_id += 0; break;
                    case 1: image_id += 2; break;
                    case 2: image_id += 4; break;
                }
            } else { // DIR_2_RIGHT
                switch (building_part) {
                    case 0: image_id += 4; break;
                    case 1: image_id += 2; break;
                    case 2: image_id += 0; break;
                }
            }
            return image_id;
        }
        case BUILDING_MENU_FORT: // old saves used this type as generic fort, now it's menu-only type
        case BUILDING_FORT_JAVELIN:
        case BUILDING_FORT_LEGIONARIES:
        case BUILDING_FORT_MOUNTED:
        case BUILDING_FORT_AUXILIA_INFANTRY:
        case BUILDING_FORT_ARCHERS:
            switch (scenario_property_climate()) {
                case CLIMATE_NORTHERN:
                    return assets_get_image_id("Military", "Fort_Main_North");
                case CLIMATE_DESERT:
                    return assets_get_image_id("Military", "Fort_Main_South");
                default:
                    return assets_get_image_id("Military", "Fort_Main_Central");
            }
        case BUILDING_FORT_GROUND:
            return image_group(GROUP_BUILDING_FORT) + 1;
        case BUILDING_NATIVE_HUT_ALT:
            switch (scenario_property_climate()) {
                case CLIMATE_NORTHERN:
                    return assets_get_image_id("Terrain_Maps", "Native_Hut_Northern_01") + (random_byte() & 1);
                case CLIMATE_DESERT:
                    return assets_get_image_id("Terrain_Maps", "Native_Hut_Southern_01") + (random_byte() & 1);
                default:
                    return assets_get_image_id("Terrain_Maps", "Native_Hut_Central_01") + (random_byte() & 1);
            }
        case BUILDING_NATIVE_CROPS:
            return image_group(GROUP_BUILDING_FARM_CROPS);
        case BUILDING_MESS_HALL:
            switch (scenario_property_climate()) {
                case CLIMATE_NORTHERN:
                    return assets_get_image_id("Military", "Mess ON North");
                case CLIMATE_DESERT:
                    return assets_get_image_id("Military", "Mess ON South");
                default:
                    return assets_get_image_id("Military", "Mess ON Central");
            }
        case BUILDING_GRAND_GARDEN:
            return assets_get_image_id("Engineer", "Eng Guild ON");
        case BUILDING_HORSE_STATUE:
        {
            int orientation = building_rotation_get_building_orientation(b->subtype.orientation) / 2;
            return assets_get_image_id("Aesthetics", "Eque Statue") + orientation % 2;
        }
        case BUILDING_DOLPHIN_FOUNTAIN:
            return assets_get_image_id("Engineer", "Eng Guild ON");
        case BUILDING_LEGION_STATUE:
        {
            int orientation = building_rotation_get_building_orientation(b->subtype.orientation) / 2;
            return assets_get_image_id("Aesthetics", "legio statue") +
                (orientation % 2) * building_properties_for_type(b->type)->rotation_offset;
        }
        case BUILDING_WATCHTOWER:
        {
            int image_id = 0;
            switch (scenario_property_climate()) {
                case CLIMATE_NORTHERN:
                    image_id = assets_get_image_id("Military", "Watchtower N ON");
                    break;
                case CLIMATE_DESERT:
                    image_id = assets_get_image_id("Military", "Watchtower S ON");
                    break;
                default:
                    image_id = assets_get_image_id("Military", "Watchtower C ON");
                    break;
            }

            int offset = building_variant_get_offset_with_rotation(b->type, b->variant);
            return image_id + offset;
        }
        case BUILDING_SMALL_MAUSOLEUM:
        {
            int orientation = building_rotation_get_building_orientation(b->subtype.orientation) / 2;
            switch (b->monument.phase) {
                case MONUMENT_START:
                    return assets_get_image_id("Monuments", "Mausoleum_Small_Construction_01");
                case 2:
                    return assets_get_image_id("Monuments", "Mausoleum_Small_Construction_02") + orientation % 2;
                default:
                    return assets_get_image_id("Monuments", "Mausoleum S") + orientation % 2;
            }
        }
        case BUILDING_LARGE_MAUSOLEUM:
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
        case BUILDING_NYMPHAEUM:
            switch (b->monument.phase) {
                case MONUMENT_START:
                    return assets_get_image_id("Monuments", "Pantheon_Const_00");
                case 2:
                    return assets_get_image_id("Monuments", "Nymphaeum_Construction_02");
                default:
                    return assets_get_image_id("Monuments", "Nymphaeum ON");
            }
        case BUILDING_PINE_TREE:
        case BUILDING_FIR_TREE:
        case BUILDING_OAK_TREE:
        case BUILDING_ELM_TREE:
        case BUILDING_FIG_TREE:
        case BUILDING_PLUM_TREE:
        case BUILDING_PALM_TREE:
        case BUILDING_DATE_TREE:
            return assets_get_group_id("Aesthetics") + (b->type - BUILDING_PINE_TREE);
        case BUILDING_GODDESS_STATUE:
        case BUILDING_SENATOR_STATUE:
        {
            int orientation = building_rotation_get_building_orientation(b->subtype.orientation) / 2;
            return assets_get_image_id("Aesthetics", "sml statue 2") +
                (b->type - BUILDING_GODDESS_STATUE) + (orientation % 2) *
                building_properties_for_type(b->type)->rotation_offset;
        }
        case BUILDING_HEDGE_DARK:
            return assets_get_image_id("Aesthetics", "D Hedge 01") +
                building_connectable_get_hedge_offset(b->grid_offset);
        case BUILDING_HEDGE_LIGHT:
            return assets_get_image_id("Aesthetics", "L Hedge 01") +
                building_connectable_get_hedge_offset(b->grid_offset);
        case BUILDING_COLONNADE:
            return assets_get_image_id("Aesthetics", "G Colonnade 01") +
                building_connectable_get_colonnade_offset(b->grid_offset);
        case BUILDING_LOOPED_GARDEN_WALL:
            return assets_get_image_id("Aesthetics", "C Garden Wall 01") +
                building_connectable_get_garden_wall_offset(b->grid_offset);
        case BUILDING_ROOFED_GARDEN_WALL:
            return assets_get_image_id("Aesthetics", "R Garden Wall 01") +
                building_connectable_get_garden_wall_offset(b->grid_offset);
        case BUILDING_PANELLED_GARDEN_WALL:
            return assets_get_image_id("Aesthetics", "Garden_Wall_C_01") +
                building_connectable_get_garden_wall_offset(b->grid_offset);
        case BUILDING_DATE_PATH:
        case BUILDING_ELM_PATH:
        case BUILDING_FIG_PATH:
        case BUILDING_FIR_PATH:
        case BUILDING_OAK_PATH:
        case BUILDING_PALM_PATH:
        case BUILDING_PINE_PATH:
        case BUILDING_PLUM_PATH:
        case BUILDING_GARDEN_PATH:
        {
            int image_offset = building_connectable_get_garden_path_offset(b->grid_offset,
                CONTEXT_GARDEN_PATH_INTERSECTION);
            int image_group = assets_get_image_id("Aesthetics", "Garden Path 01");
            // If path isn't an intersection, it's a straight path instead
            if (image_offset == -1) {
                image_offset = building_connectable_get_garden_path_offset(b->grid_offset,
                    b->type == BUILDING_GARDEN_PATH ? CONTEXT_GARDEN_TREELESS_PATH : CONTEXT_GARDEN_TREE_PATH);

                switch (b->type) {
                    case BUILDING_DATE_PATH:
                        image_group = assets_get_image_id("Aesthetics", "path orn date");
                        break;
                    case BUILDING_ELM_PATH:
                        image_group = assets_get_image_id("Aesthetics", "path orn elm");
                        break;
                    case BUILDING_FIG_PATH:
                        image_group = assets_get_image_id("Aesthetics", "path orn fig");
                        break;
                    case BUILDING_FIR_PATH:
                        image_group = assets_get_image_id("Aesthetics", "path orn fir");
                        break;
                    case BUILDING_OAK_PATH:
                        image_group = assets_get_image_id("Aesthetics", "path orn oak");
                        break;
                    case BUILDING_PALM_PATH:
                        image_group = assets_get_image_id("Aesthetics", "path orn palm");
                        break;
                    case BUILDING_PINE_PATH:
                        image_group = assets_get_image_id("Aesthetics", "path orn pine");
                        break;
                    case BUILDING_PLUM_PATH:
                        image_group = assets_get_image_id("Aesthetics", "path orn plum");
                        break;
                    default:
                        image_group = assets_get_image_id("Aesthetics", "garden path r");
                        break;
                }
            }
            return image_group + image_offset;
        }
        case BUILDING_BURNING_RUIN:
            if (building_was_tent(b)) {
                return image_group(GROUP_TERRAIN_RUBBLE_TENT);
            } else {
                return image_group(GROUP_TERRAIN_RUBBLE_GENERAL) + 9 * (map_random_get(b->grid_offset) & 3);
            }
        case BUILDING_ROOFED_GARDEN_WALL_GATE:
            return assets_get_image_id("Aesthetics", "Garden_Gate_B") + building_connectable_get_garden_gate_offset(b->grid_offset);
        case BUILDING_LOOPED_GARDEN_GATE:
            return assets_get_image_id("Aesthetics", "Garden_Gate_A") + building_connectable_get_garden_gate_offset(b->grid_offset);
        case BUILDING_PANELLED_GARDEN_GATE:
            return assets_get_image_id("Aesthetics", "Garden_Gate_C") + building_connectable_get_garden_gate_offset(b->grid_offset);
        case BUILDING_HEDGE_GATE_LIGHT:
            return assets_get_image_id("Aesthetics", "L Hedge Gate") + building_connectable_get_hedge_gate_offset(b->grid_offset);
        case BUILDING_HEDGE_GATE_DARK:
            return assets_get_image_id("Aesthetics", "D Hedge Gate") + building_connectable_get_hedge_gate_offset(b->grid_offset);
        case BUILDING_PALISADE:
            switch (scenario_property_climate()) {
                case CLIMATE_NORTHERN:
                    return assets_get_image_id("Military", "Pal Wall N 01") + building_connectable_get_palisade_offset(b->grid_offset);
                case CLIMATE_DESERT:
                    return assets_get_image_id("Military", "Pal Wall S 01") + building_connectable_get_palisade_offset(b->grid_offset);
                default:
                    return assets_get_image_id("Military", "Pal Wall C 01") + building_connectable_get_palisade_offset(b->grid_offset);
            }
        case BUILDING_PALISADE_GATE:
            return assets_get_image_id("Military", "Palisade_Gate") + building_connectable_get_palisade_gate_offset(b->grid_offset);
        case BUILDING_GLADIATOR_STATUE:
        {
            int orientation = building_rotation_get_building_orientation(b->subtype.orientation) / 2;
            return assets_get_image_id("Aesthetics", "Gladiator_Statue") +
                (orientation % 2) * building_properties_for_type(b->type)->rotation_offset;
        }
        case BUILDING_HIGHWAY:
            return assets_get_image_id("Admin_Logistics", "Highway_Placement");
        case BUILDING_DEPOT:
            switch (scenario_property_climate()) {
                case CLIMATE_NORTHERN:
                    return assets_get_image_id("Admin_Logistics", "Cart Depot N ON");
                case CLIMATE_DESERT:
                    return assets_get_image_id("Admin_Logistics", "Cart Depot S ON");
                default:
                    return assets_get_image_id("Admin_Logistics", "Cart Depot C ON");
            }
        case BUILDING_SHRINE_CERES:
        {
            int orientation = building_rotation_get_building_orientation(b->subtype.orientation) / 2;
            return assets_get_image_id("Health_Culture", "Altar_Ceres") + orientation % 2;
        }
        case BUILDING_SHRINE_MARS:
        {
            int orientation = building_rotation_get_building_orientation(b->subtype.orientation) / 2;
            return assets_get_image_id("Health_Culture", "Altar_Mars") + orientation % 2;
        }
        case BUILDING_SHRINE_MERCURY:
        {
            int orientation = building_rotation_get_building_orientation(b->subtype.orientation) / 2;
            return assets_get_image_id("Health_Culture", "Altar_Mercury") + orientation % 2;
        }
        case BUILDING_SHRINE_NEPTUNE:
        {
            int orientation = building_rotation_get_building_orientation(b->subtype.orientation) / 2;
            return assets_get_image_id("Health_Culture", "Altar_Neptune") + orientation % 2;
        }
        case BUILDING_SHRINE_VENUS:
        {
            int orientation = building_rotation_get_building_orientation(b->subtype.orientation) / 2;
            return assets_get_image_id("Health_Culture", "Altar_Venus") + orientation % 2;
        }
        case BUILDING_OVERGROWN_GARDENS:
            return building_properties_for_type(BUILDING_OVERGROWN_GARDENS)->image_group;

        default:
            return 0;
    }
}

int building_image_get_for_type(building_type type)
{
    building b = {};
    b.type = type;
    return building_image_get(&b);
}
