#include "construction_building.h"

extern "C" {
#include "assets/assets.h"
#include "building/building.h"
#include "building/building_runtime_api.h"
#include "building/building_type_api.h"
#include "building/construction.h"
#include "building/construction_warning.h"
#include "building/count.h"
#include "building/distribution.h"
#include "building/dock.h"
#include "building/image.h"
#include "building/menu.h"
#include "building/monument.h"
#include "building/properties.h"
#include "building/rotation.h"
#include "building/storage.h"
#include "building/variant.h"
#include "building/warehouse.h"
#include "city/buildings.h"
#include "city/finance.h"
#include "city/resource.h"
#include "city/view.h"
#include "city/warning.h"
#include "core/config.h"
#include "core/image.h"
#include "core/random.h"
#include "empire/city.h"
#include "figure/formation_legion.h"
#include "game/undo.h"
#include "map/building.h"
#include "map/building_tiles.h"
#include "map/figure.h"
#include "map/grid.h"
#include "map/orientation.h"
#include "map/property.h"
#include "map/routing.h"
#include "map/routing_terrain.h"
#include "map/terrain.h"
#include "map/tiles.h"
#include "map/water.h"
#include "map/water_supply.h"
#include "scenario/property.h"
}


static void add_fort(building_type type, building *fort)
{
    fort->prev_part_building_id = 0;
    map_building_tiles_add(fort->id, fort->x, fort->y, fort->size, building_image_get(fort), TERRAIN_BUILDING);
    if (type == BUILDING_FORT_LEGIONARIES) {
        fort->subtype.fort_figure_type = FIGURE_FORT_LEGIONARY;
    } else if (type == BUILDING_FORT_JAVELIN) {
        fort->subtype.fort_figure_type = FIGURE_FORT_JAVELIN;
    } else if (type == BUILDING_FORT_MOUNTED) {
        fort->subtype.fort_figure_type = FIGURE_FORT_MOUNTED;
    } else if (type == BUILDING_FORT_AUXILIA_INFANTRY) {
        fort->subtype.fort_figure_type = FIGURE_FORT_INFANTRY;
    } else if (type == BUILDING_FORT_ARCHERS) {
        fort->subtype.fort_figure_type = FIGURE_FORT_ARCHER;
    }

    // create parade ground
    const int offsets_x[] = { 3, -1, -4, 0 };
    const int offsets_y[] = { -1, -4, 0, 3 };
    int id = fort->id;
    building *ground = building_create(BUILDING_FORT_GROUND, fort->x + offsets_x[building_rotation_get_rotation()],
        fort->y + offsets_y[building_rotation_get_rotation()]);
    game_undo_add_building(ground);
    fort = building_get(id);
    ground->prev_part_building_id = fort->id;
    fort->next_part_building_id = ground->id;
    ground->next_part_building_id = 0;
    map_building_tiles_add(ground->id, fort->x + offsets_x[building_rotation_get_rotation()],
     fort->y + offsets_y[building_rotation_get_rotation()], 4, image_group(GROUP_BUILDING_FORT) + 1, TERRAIN_BUILDING);

    fort->formation_id = formation_legion_create_for_fort(fort);
    ground->formation_id = fort->formation_id;
}

int building_construction_prepare_terrain(grid_slice *grid_slice, clear_mode clear_mode, cost_calculation cost)
{
    int total_cost = 0;
    for (int i = 0; i < grid_slice->size; i++) {
        int g_offset = grid_slice->grid_offsets[i];
        int terrain_mask_to_remove = 0;
        switch (clear_mode) { //ugly but efficient
            case CLEAR_MODE_FORCE:
                terrain_mask_to_remove = TERRAIN_NOT_CLEAR;
                break;
            case CLEAR_MODE_RUBBLE:
                terrain_mask_to_remove = TERRAIN_RUBBLE;
                break;
            case CLEAR_MODE_TREES:
                terrain_mask_to_remove = TERRAIN_TREE | TERRAIN_SHRUB;
                break;
            case CLEAR_MODE_PLAYER:
            default:
                terrain_mask_to_remove = TERRAIN_CLEARABLE;
                break;
        }
        if (map_terrain_is(g_offset, terrain_mask_to_remove)) {
            total_cost += (cost == COST_FREE) ? 0 : 3; // base cost per tile is 50% more than regular clear
            if (cost != COST_MEASURE) {
                map_terrain_remove(g_offset, terrain_mask_to_remove);
            }
        }
    }
    if (cost == COST_PROCESS && total_cost > 0) {
        city_finance_process_construction(total_cost);
    }
    return total_cost;
}

static int check_gatehouse_tiles(int grid_offset)
{
    grid_slice *slice = map_grid_get_grid_slice_square(grid_offset, 2);
    for (int i = 0; i < slice->size; i++) {
        if (map_terrain_is(slice->grid_offsets[i], TERRAIN_BUILDING)) {
            if (map_terrain_is(slice->grid_offsets[i], TERRAIN_WALL)) {
                continue;
            } else {
                return 0;
            }
        }
    }
    return 1;
}

static void add_hippodrome(building *b)
{
    building *part1 = b;

    int x_offset, y_offset;
    building_rotation_get_offset_with_rotation(5, building_rotation_get_rotation(), &x_offset, &y_offset);
    building *part2 = building_create(BUILDING_HIPPODROME, part1->x + x_offset, part1->y + y_offset);
    game_undo_add_building(part2);

    building_rotation_get_offset_with_rotation(10, building_rotation_get_rotation(), &x_offset, &y_offset);
    building *part3 = building_create(BUILDING_HIPPODROME, part1->x + x_offset, part1->y + y_offset);
    game_undo_add_building(part3);

    part1->prev_part_building_id = 0;
    part1->next_part_building_id = part2->id;
    part2->prev_part_building_id = part1->id;
    part2->next_part_building_id = part3->id;
    part3->prev_part_building_id = part2->id;
    part3->next_part_building_id = 0;
}

static int add_warehouse_space(int x, int y, int prev_id)
{
    building *b = building_create(BUILDING_WAREHOUSE_SPACE, x, y);
    game_undo_add_building(b);
    building *prev = building_get(prev_id);
    b->prev_part_building_id = prev->id;
    prev->next_part_building_id = b->id;
    map_building_tiles_add(b->id, x, y, 1,
        image_group(GROUP_BUILDING_WAREHOUSE_STORAGE_EMPTY), TERRAIN_BUILDING);
    return b->id;
}

static void add_warehouse(building *b, int orientation)
{
    b->storage_id = building_storage_create(b->id);
    b->prev_part_building_id = 0;
    // assert orientation. orientation points to the tower's location (0-3), out of 4 possible corners
    b->subtype.orientation = orientation;

    int x_offset[9] = { 0, 0, 1, 1, 0, 2, 1, 2, 2 };
    int y_offset[9] = { 0, 1, 0, 1, 2, 0, 2, 1, 2 };
    // can skip  building_rotation_get_rotation(), set upstream
    int tower = building_rotation_get_corner(2 * orientation);
    map_building_tiles_add(b->id, b->x + x_offset[tower], b->y + y_offset[tower], 1,
        image_group(GROUP_BUILDING_WAREHOUSE), TERRAIN_BUILDING);
    map_tiles_update_area_roads(b->x + x_offset[tower], b->y + y_offset[tower], 3);

    int id = b->id;
    int prev = id;
    for (int i = 0; i < 9; i++) {
        if (i == tower) {
            continue;
        }
        prev = add_warehouse_space(b->x + x_offset[i], b->y + y_offset[i], prev);
    }

    b = building_get(id);
    // Adjust BUILDING_WAREHOUSE to tower position
    b->x = b->x + x_offset[tower];
    b->y = b->y + y_offset[tower];
    b->grid_offset = map_grid_offset(b->x, b->y);
    game_undo_adjust_building(b);

    building_get(prev)->next_part_building_id = 0;
    map_tiles_update_area_roads(b->x, b->y, 5);
    if (!map_has_road_access_warehouse(b->x, b->y, 0)) {
        city_warning_show(WARNING_WAREHOUSE_TOWER, NEW_WARNING_SLOT);
    }
}

static void add_building(building *b)
{
    if (building_type_registry_has_construction(b->type) &&
        !building_type_registry_has_phased_construction(b->type) &&
        building_monument_type_is_monument(b->type) &&
        !b->monument.phase) {
        b->monument.phase = MONUMENT_FINISHED;
    }
    if (building_runtime_apply_graphic_if_native(b)) {
        return;
    }
    int image_id = building_image_get(b);
    if (image_id) {
        map_building_tiles_add(b->id, b->x, b->y, b->size, image_id, TERRAIN_BUILDING);
    }
}

static void add_depot(building *b)
{
    b->data.depot.current_order.condition.condition_type = ORDER_CONDITION_ALWAYS;
    add_building(b);
}

static void add_granary(building *b)
{
    b->storage_id = building_storage_create(b->id);
    add_building(b);
    map_update_granary_internal_roads(b);
    map_tiles_update_area_roads(b->x, b->y, 5);
}

static void add_to_map(building_type type, building *b, int size, int orientation, int waterside_orientation_abs)
{
    if (building_variant_has_variants(b->type)) {
        b->variant = building_rotation_get_rotation_with_limit(building_variant_get_number_of_variants(b->type));
    }
    // Native graphics options share the saved variant byte, so XML-owned
    // buildings seed it after legacy variant/rotation setup has run.
    building_runtime_assign_graphic_variant(b, 1);
    if (type == BUILDING_LARGE_TEMPLE_VENUS) {
        building_distribution_unaccept_all_goods(b);
    }
    if (building_type_registry_has_phased_construction(type)) {
        int road_update_radius = building_type_registry_get_construction_road_update_radius(type);
        if (road_update_radius > 0) {
            map_tiles_update_area_roads(b->x, b->y, road_update_radius);
        }
        if (type == BUILDING_GRAND_TEMPLE_MARS) {
            b->accepted_goods[RESOURCE_WEAPONS] = 1;
            b->accepted_goods[RESOURCE_NONE] = 1;
        }
        building_monument_set_phase(b, MONUMENT_START);
    } else switch (type) {
        default:
            add_building(b);
            break;
            // entertainment
        case BUILDING_COLOSSEUM:
            map_tiles_update_area_roads(b->x, b->y, 5);
            building_monument_set_phase(b, MONUMENT_START);
            break;
            // farms
        case BUILDING_WHEAT_FARM:
        case BUILDING_VEGETABLE_FARM:
        case BUILDING_FRUIT_FARM:
        case BUILDING_OLIVE_FARM:
        case BUILDING_VINES_FARM:
        case BUILDING_PIG_FARM:
            map_building_tiles_add_farm(b->id, b->x, b->y, building_image_get_base_farm_crop(type), 0);
            break;
            // distribution
        case BUILDING_GRANARY:
            add_granary(b);
            break;
            // Don't autodistribute wine for new Venus temples
        case BUILDING_SMALL_TEMPLE_VENUS:
            add_building(b);
            building_distribution_unaccept_all_goods(b);
            break;
        case BUILDING_LARGE_MAUSOLEUM:
        case BUILDING_NYMPHAEUM:
        case BUILDING_CITY_MINT:
            map_tiles_update_area_roads(b->x, b->y, 5);
            building_monument_set_phase(b, MONUMENT_START);
            break;
        case BUILDING_ROADBLOCK:
            add_building(b);
            map_terrain_add_roadblock_road(b->x, b->y);
            map_tiles_update_area_roads(b->x, b->y, 5);
            map_tiles_update_all_plazas();
            break;
        case BUILDING_SHIPYARD:
        case BUILDING_WHARF:
            b->data.industry.orientation = waterside_orientation_abs;
            map_water_add_building(b->id, b->x, b->y, 2);
            break;
        case BUILDING_DOCK:
            b->data.dock.orientation = waterside_orientation_abs;
            map_water_add_building(b->id, b->x, b->y, size);
            break;
        case BUILDING_TOWER:
            map_terrain_remove_with_radius(b->x, b->y, 2, 0, TERRAIN_WALL);
            map_building_tiles_add(b->id, b->x, b->y, size, building_image_get(b),
                TERRAIN_BUILDING | TERRAIN_GATEHOUSE);
            map_tiles_update_area_walls(b->x, b->y, 5);
            break;
        case BUILDING_GATEHOUSE:
            b->subtype.orientation = orientation;
            map_building_tiles_add_remove(b->id, b->x, b->y, size,
                building_image_get(b), TERRAIN_BUILDING | TERRAIN_GATEHOUSE, TERRAIN_CLEARABLE & ~TERRAIN_HIGHWAY);
            map_orientation_update_buildings();
            map_terrain_add_gatehouse_roads(b->x, b->y, orientation);
            map_tiles_update_area_roads(b->x, b->y, 5);
            map_tiles_update_area_highways(b->x, b->y, 3);
            map_tiles_update_all_plazas();
            map_tiles_update_area_walls(b->x, b->y, 5);
            break;
        case BUILDING_TRIUMPHAL_ARCH:
            b->subtype.orientation = orientation;
            add_building(b);
            map_orientation_update_buildings();
            map_terrain_add_triumphal_arch_roads(b->x, b->y, orientation);
            map_tiles_update_area_roads(b->x, b->y, 5);
            map_tiles_update_all_plazas();
            city_buildings_build_triumphal_arch();
            building_menu_update();
            building_construction_clear_type();
            break;
        case BUILDING_WAREHOUSE:
            add_warehouse(b, orientation);
            break;
        case BUILDING_HIPPODROME: {
            add_hippodrome(b);
            building_monument_set_phase(b, MONUMENT_START);
            building *b2 = building_get(b->next_part_building_id);
            building_monument_set_phase(b2, MONUMENT_START);
            building *b3 = building_get(b2->next_part_building_id);
            building_monument_set_phase(b3, MONUMENT_START);
            break;
        }
        case BUILDING_FORT_LEGIONARIES:
        case BUILDING_FORT_JAVELIN:
        case BUILDING_FORT_MOUNTED:
        case BUILDING_FORT_AUXILIA_INFANTRY:
        case BUILDING_FORT_ARCHERS:
            add_fort(type, b);
            break;
        case BUILDING_PANTHEON:
            map_tiles_update_area_roads(b->x, b->y, 9);
            building_monument_set_phase(b, MONUMENT_START);
            break;
        case BUILDING_MESS_HALL:
            b->data.market.is_mess_hall = 1;
            add_building(b);
            break;
        case BUILDING_SMALL_STATUE:
        case BUILDING_MEDIUM_STATUE:
        case BUILDING_HORSE_STATUE:
        case BUILDING_LEGION_STATUE:
        case BUILDING_GLADIATOR_STATUE:
            b->subtype.orientation = building_rotation_get_rotation();
            add_building(b);
            break;
        case BUILDING_SMALL_MAUSOLEUM:
            b->subtype.orientation = building_rotation_get_rotation();
            map_tiles_update_area_roads(b->x, b->y, 4);
            building_monument_set_phase(b, MONUMENT_START);
            break;
        case BUILDING_HIGHWAY:
            add_building(b);
            break;
        case BUILDING_DEPOT:
            add_depot(b);
            break;
        case BUILDING_SHRINE_CERES:
        case BUILDING_SHRINE_MARS:
        case BUILDING_SHRINE_MERCURY:
        case BUILDING_SHRINE_NEPTUNE:
        case BUILDING_SHRINE_VENUS:
            b->subtype.orientation = building_rotation_get_rotation();
            add_building(b);
            break;
        case BUILDING_BARRACKS:
            b->accepted_goods[RESOURCE_WEAPONS] = 1;
            b->accepted_goods[RESOURCE_NONE] = 1;
            add_building(b);
            break;
    }
    map_routing_update_land();
    map_routing_update_walls();
}

int building_construction_is_granary_cross_tile(int tile_no)
{
    return  tile_no == 1 ||
        tile_no == 2 ||
        tile_no == 3 ||
        tile_no == 6 ||
        tile_no == 7;
}

int building_construction_is_warehouse_corner(int tile_no)
{

    int building_rotation = building_rotation_get_rotation();
    int view_rotation = city_view_orientation() / 2;
    int effective_rotation = (building_rotation + view_rotation) % 4;
    int corner = building_rotation_get_corner(2 * effective_rotation);

    return tile_no == corner;
}

int building_construction_fill_vacant_lots(grid_slice *area)
{
    int items_placed = 0;
    building_type vacant_lot_type = building_type_registry_get_vacant_lot_fill_type();
    for (int i = 0; i < area->size; i++) {
        int grid_offset = area->grid_offsets[i];
        int x = map_grid_offset_to_x(grid_offset);
        int y = map_grid_offset_to_y(grid_offset);
        int success = building_construction_place_building(vacant_lot_type, x, y, 1);
        if (!success) {
            continue;
        }
        building *b = building_get(map_building_at(grid_offset));
        game_undo_add_building(b);
        items_placed++;
    }
    if (items_placed > 0) {
        building_construction_warning_check_food_stocks(vacant_lot_type);
        map_routing_update_land();
    }
    return items_placed;
}

enum {
    FORCE_PLACE_MAX_CLEAR_TILES = 128,
    FORCE_PLACE_CLEARABLE_TERRAIN = TERRAIN_TREE | TERRAIN_SHRUB | TERRAIN_ROAD
};

struct force_place_check {
    int active;
    int perform_clear;
    int clear_cost;
    int clear_offset_count;
    int clear_offsets[FORCE_PLACE_MAX_CLEAR_TILES];
};

static void show_place_warning(int show_warnings, int warning_id)
{
    if (show_warnings) {
        city_warning_show(static_cast<warning_type>(warning_id), NEW_WARNING_SLOT);
    }
}

static int instant_resource_warning_for_type(building_type type, resource_type resource)
{
    if (resource == RESOURCE_MARBLE) {
        if (type == BUILDING_ORACLE) {
            return WARNING_MARBLE_NEEDED_ORACLE;
        }
        if (type >= BUILDING_LARGE_TEMPLE_CERES && type <= BUILDING_LARGE_TEMPLE_VENUS) {
            return WARNING_MARBLE_NEEDED_LARGE_TEMPLE;
        }
    }
    return WARNING_RESOURCES_NOT_AVAILABLE;
}

static int instant_building_has_required_resources(building_type type, int show_warnings)
{
    for (resource_type resource = RESOURCE_MIN; resource < RESOURCE_MAX; resource = static_cast<resource_type>(resource + 1)) {
        int amount = building_type_registry_get_instant_construction_requirement(type, resource);
        if (amount > 0 && city_resource_count_warehouses_amount(resource) < amount) {
            show_place_warning(show_warnings, instant_resource_warning_for_type(type, resource));
            return 0;
        }
    }
    return 1;
}

static void instant_building_remove_required_resources(building_type type)
{
    for (resource_type resource = RESOURCE_MIN; resource < RESOURCE_MAX; resource = static_cast<resource_type>(resource + 1)) {
        int amount = building_type_registry_get_instant_construction_requirement(type, resource);
        if (amount > 0) {
            building_warehouses_remove_resource(resource, amount);
        }
    }
}

static int force_place_can_clear_terrain(int terrain)
{
    return terrain && !(terrain & ~FORCE_PLACE_CLEARABLE_TERRAIN);
}

static void force_place_add_clear_offset(force_place_check *check, int grid_offset)
{
    if (!check || !check->active) {
        return;
    }
    for (int i = 0; i < check->clear_offset_count; i++) {
        if (check->clear_offsets[i] == grid_offset) {
            return;
        }
    }
    if (check->clear_offset_count >= FORCE_PLACE_MAX_CLEAR_TILES) {
        return;
    }
    check->clear_offsets[check->clear_offset_count++] = grid_offset;
    check->clear_cost += model_get_building(BUILDING_CLEAR_LAND)->cost;
}

static int tiles_are_clear_or_force_clearable(int x, int y, int size, int disallowed_terrain,
    int check_figure, force_place_check *force_check)
{
    if (!force_check || !force_check->active) {
        return map_tiles_are_clear(x, y, size, disallowed_terrain, check_figure);
    }
    if (!map_grid_is_inside(x, y, size)) {
        return 0;
    }
    for (int dy = 0; dy < size; dy++) {
        for (int dx = 0; dx < size; dx++) {
            int grid_offset = map_grid_offset(x + dx, y + dy);
            if ((check_figure || force_check->active) && map_has_figure_at(grid_offset)) {
                return 0;
            }
            int blocked_terrain = map_terrain_get(grid_offset) & TERRAIN_NOT_CLEAR & disallowed_terrain;
            if (!blocked_terrain) {
                continue;
            }
            if (!force_place_can_clear_terrain(blocked_terrain)) {
                return 0;
            }
            force_place_add_clear_offset(force_check, grid_offset);
        }
    }
    return 1;
}

static void force_place_clear_offsets(force_place_check *check)
{
    if (!check || !check->active || !check->perform_clear || !check->clear_offset_count) {
        return;
    }

    int x_min = map_grid_offset_to_x(check->clear_offsets[0]);
    int x_max = x_min;
    int y_min = map_grid_offset_to_y(check->clear_offsets[0]);
    int y_max = y_min;

    for (int i = 0; i < check->clear_offset_count; i++) {
        int grid_offset = check->clear_offsets[i];
        int x = map_grid_offset_to_x(grid_offset);
        int y = map_grid_offset_to_y(grid_offset);
        if (x < x_min) { x_min = x; }
        if (x > x_max) { x_max = x; }
        if (y < y_min) { y_min = y; }
        if (y > y_max) { y_max = y; }

        if (map_terrain_is(grid_offset, TERRAIN_ROAD)) {
            map_property_clear_plaza_earthquake_or_overgrown_garden(grid_offset);
        }
        map_terrain_remove(grid_offset, FORCE_PLACE_CLEARABLE_TERRAIN);
    }

    int radius = x_max - x_min <= y_max - y_min ? y_max - y_min + 3 : x_max - x_min + 3;
    map_tiles_update_region_empty_land(x_min, y_min, x_max, y_max);
    map_tiles_update_region_meadow(x_min, y_min, x_max, y_max);
    map_tiles_update_area_roads(x_min, y_min, radius);
    map_tiles_update_all_plazas();
}

static int building_construction_place_building_internal(building_type type, int x, int y,
    int exact_coordinates, force_place_check *force_check, int show_warnings, int place_building)
{
    int grid_offset = map_grid_offset(x, y);

    int terrain_mask = TERRAIN_ALL;
    if ((building_type_is_roadblock(type) && !(type == BUILDING_GRANARY || type == BUILDING_WAREHOUSE)) ||
        (config_get(CONFIG_GP_CH_WAREHOUSES_GRANARIES_OVER_ROAD_PLACEMENT) &&
        (type == BUILDING_GRANARY || type == BUILDING_WAREHOUSE))) {
        terrain_mask = type == BUILDING_GATEHOUSE ? ~TERRAIN_WALL & ~TERRAIN_ROAD &
            ~TERRAIN_HIGHWAY & ~TERRAIN_BUILDING : ~TERRAIN_ROAD & ~TERRAIN_HIGHWAY;
        //allow building gatehouses over walls and roads, other non-bridge roadblocks over roads and highways
    } else if (type == BUILDING_TOWER) {
        terrain_mask = ~TERRAIN_WALL & ~TERRAIN_BUILDING;
    } else if (type == BUILDING_RESERVOIR || type == BUILDING_DRAGGABLE_RESERVOIR) {
        terrain_mask = ~TERRAIN_AQUEDUCT;
    }
    //allow building granaries and warehouses over all road, BUT,
    //the building ghost is set up to SUGGEST placing it over crossroads only

    int size = building_properties_for_type(type)->size;
    if (type == BUILDING_WAREHOUSE) {
        size = 3;
    }
    // Do not check for a figure when build a roadblock of single tile size
    // TODO: do not check for figures on tiles that are citizen passable in general
    int check_figure = type == BUILDING_ROADBLOCK && size == 1 ? 0 : 1;
    int building_orientation = 0;
    if (type == BUILDING_GATEHOUSE || type == BUILDING_WAREHOUSE) {
        //check if there's a preset orientation from old building
        building *old_b = building_main(building_get(map_building_rubble_building_id(grid_offset)));
        if (old_b && (old_b->type == BUILDING_GATEHOUSE ||
            old_b->type == BUILDING_WAREHOUSE || old_b->type == BUILDING_WAREHOUSE_SPACE)) {
            building_orientation = old_b->subtype.orientation;
        } else if (type == BUILDING_GATEHOUSE) {
            building_orientation = map_orientation_for_gatehouse(x, y);
        } else if (type == BUILDING_WAREHOUSE) {
            building_orientation = building_rotation_get_rotation();
        }
    } else if (type == BUILDING_TRIUMPHAL_ARCH) {
        building_orientation = map_orientation_for_triumphal_arch(x, y);
    }
    if (!exact_coordinates) {
        building_construction_offset_start_from_orientation(&x, &y, size);
    }
    // extra checks
    if (type == BUILDING_TOWER) {
        if (!map_terrain_all_tiles_in_radius_are(x, y, size, 0, TERRAIN_WALL)) {
            show_place_warning(show_warnings, WARNING_CLEAR_LAND_NEEDED);
            return 0;
        }
        if (!map_terrain_all_tiles_in_radius_are(x, y, 2, 0, TERRAIN_BUILDING)) {
            show_place_warning(show_warnings, WARNING_WALL_NEEDED);
            return 0;
        }
        if (!building_orientation) {
            building_orientation = building_rotation_get_rotation() + 1;
            if (building_orientation > 4) {
                building_orientation = 1;
            }
        }
    }
    if (type == BUILDING_GATEHOUSE) {
        if (!tiles_are_clear_or_force_clearable(x, y, size, terrain_mask, check_figure, force_check)) {
            show_place_warning(show_warnings, WARNING_CLEAR_LAND_NEEDED);
            return 0;
        }
        if (!check_gatehouse_tiles(grid_offset)) { //helper to make sure all building tiles are on walls
            show_place_warning(show_warnings, WARNING_CLEAR_LAND_NEEDED);
            return 0;
        }
        if (!building_orientation) {
            if (building_rotation_get_road_orientation() == 1) {
                building_orientation = 1;
            } else {
                building_orientation = 2;
            }
        }
    }
    if (type == BUILDING_ROADBLOCK) {
        if (map_tiles_are_clear(x, y, size, TERRAIN_ROAD, check_figure)) {
            return 0;
        }
    }
    if (type == BUILDING_TRIUMPHAL_ARCH) {
        if (!tiles_are_clear_or_force_clearable(x, y, size, terrain_mask, check_figure, force_check)) {
            show_place_warning(show_warnings, WARNING_CLEAR_LAND_NEEDED);
            return 0;
        }
        if (!building_orientation) {
            if (building_rotation_get_road_orientation() == 1) {
                building_orientation = 1;
            } else {
                building_orientation = 2;
            }
        }
    }
    int waterside_orientation_abs = 0, waterside_orientation_rel = 0;

    if (type == BUILDING_SHIPYARD || type == BUILDING_WHARF || type == BUILDING_DOCK) {
        if (map_water_determine_orientation(x, y, building_properties_for_type(type)->size, 0,
            &waterside_orientation_abs, &waterside_orientation_rel, 1, 0)) {
            show_place_warning(show_warnings, WARNING_SHORE_NEEDED);
            return 0;
        }
        if (type == BUILDING_DOCK && !building_dock_is_connected_to_open_water(x, y)) {
            show_place_warning(show_warnings, WARNING_DOCK_OPEN_WATER_NEEDED);
            return 0;
        }
    } else {
        if (!tiles_are_clear_or_force_clearable(x, y, size, terrain_mask, check_figure, force_check)) {
            show_place_warning(show_warnings, WARNING_CLEAR_LAND_NEEDED);
            return 0;
        }
        int warning_id;
        if (!building_construction_can_place_on_terrain(x, y, &warning_id)) {
            show_place_warning(show_warnings, warning_id);
            return 0;
        }
    }
    if (building_is_fort(type)) {
        const int offsets_x[] = { 3, -1, -4, 0 };
        const int offsets_y[] = { -1, -4, 0, 3 };
        int orient_index = building_rotation_get_rotation();
        int x_offset = offsets_x[orient_index];
        int y_offset = offsets_y[orient_index];
        if (!tiles_are_clear_or_force_clearable(x + x_offset, y + y_offset, 4, terrain_mask, 0, force_check)) {
            // ignore figures on fort grounds
            show_place_warning(show_warnings, WARNING_CLEAR_LAND_NEEDED);
            return 0;
        }
        if (formation_get_num_legions_cached() >= formation_get_max_legions()) {
            show_place_warning(show_warnings, WARNING_MAX_LEGIONS_REACHED);
            return 0;
        }
        if (!city_buildings_has_mess_hall()) {
            show_place_warning(show_warnings, WARNING_NO_MESS_HALL);
            return 0;
        }
    }

    if (!building_monument_has_required_resources_to_build(type)) {
        show_place_warning(show_warnings, WARNING_RESOURCES_NOT_AVAILABLE);
        return 0;
    }
    if (!instant_building_has_required_resources(type, show_warnings)) {
        return 0;
    }

    if (building_monument_get_id(type) && !building_monument_type_is_mini_monument(type)) {
        show_place_warning(show_warnings, WARNING_ONE_BUILDING_OF_TYPE);
        return 0;
    }

    if (building_monument_is_grand_temple(type) &&
        building_monument_count_grand_temples() >= config_get(CONFIG_GP_CH_MAX_GRAND_TEMPLES)) {
        show_place_warning(show_warnings, WARNING_MAX_GRAND_TEMPLES);
        return 0;
    }
    if (type == BUILDING_COLOSSEUM) {
        if (building_count_total(BUILDING_COLOSSEUM)) {
            show_place_warning(show_warnings, WARNING_ONE_BUILDING_OF_TYPE);
            return 0;
        }
    }
    if (type == BUILDING_HIPPODROME) {
        if (city_buildings_has_hippodrome()) {
            show_place_warning(show_warnings, WARNING_ONE_BUILDING_OF_TYPE);
            return 0;
        }
        int x_offset_1, y_offset_1;
        building_rotation_get_offset_with_rotation(5, building_rotation_get_rotation(), &x_offset_1, &y_offset_1);
        int x_offset_2, y_offset_2;
        building_rotation_get_offset_with_rotation(10, building_rotation_get_rotation(), &x_offset_2, &y_offset_2);
        if (!tiles_are_clear_or_force_clearable(x + x_offset_1, y + y_offset_1, 5, terrain_mask, check_figure,
                force_check) ||
            !tiles_are_clear_or_force_clearable(x + x_offset_2, y + y_offset_2, 5, terrain_mask, check_figure,
                force_check)) {
            show_place_warning(show_warnings, WARNING_CLEAR_LAND_NEEDED);
            return 0;
        }
    }
    if (type == BUILDING_SENATE && city_buildings_has_senate()) {
        show_place_warning(show_warnings, WARNING_ONE_BUILDING_OF_TYPE);
        return 0;
    }
    if (type == BUILDING_CITY_MINT) {
        if (city_buildings_has_city_mint()) {
            show_place_warning(show_warnings, WARNING_ONE_BUILDING_OF_TYPE);
            return 0;
        }
        if (!city_buildings_has_senate()) {
            show_place_warning(show_warnings, WARNING_SENATE_NEEDED);
            show_place_warning(show_warnings, WARNING_BUILD_SENATE);
            return 0;
        }
    }
    if (type == BUILDING_LIGHTHOUSE && city_buildings_has_lighthouse()) {
        show_place_warning(show_warnings, WARNING_ONE_BUILDING_OF_TYPE);
        return 0;
    }
    if (type == BUILDING_CARAVANSERAI && city_buildings_has_caravanserai()) {
        show_place_warning(show_warnings, WARNING_ONE_BUILDING_OF_TYPE);
        return 0;
    }
    if (type == BUILDING_BARRACKS && city_buildings_has_barracks() && !config_get(CONFIG_GP_CH_MULTIPLE_BARRACKS)) {
        show_place_warning(show_warnings, WARNING_ONE_BUILDING_OF_TYPE);
        return 0;
    }
    if (type == BUILDING_MESS_HALL && city_buildings_has_mess_hall()) {
        show_place_warning(show_warnings, WARNING_ONE_BUILDING_OF_TYPE);
        return 0;
    }
    if (show_warnings) {
        building_construction_warning_check_all(type, x, y, size);
    }

    if (!place_building) {
        return 1;
    }
    force_place_clear_offsets(force_check);


    // phew, checks done!
    building *b;
    b = building_create(type, x, y);

    game_undo_add_building(b);
    if (b->id <= 0) {
        return 0;
    }
    add_to_map(type, b, size, building_orientation, waterside_orientation_abs);
    instant_building_remove_required_resources(type);
    map_water_supply_refresh_building(b);
    return 1;
}

int building_construction_place_building(building_type type, int x, int y, int exact_coordinates)
{
    return building_construction_place_building_internal(type, x, y, exact_coordinates, 0, 1, 1);
}

int building_construction_force_place_assess(building_type type, int x, int y, int exact_coordinates, int *clear_cost)
{
    force_place_check check = { 1, 0, 0, 0, { 0 } };
    int success = building_construction_place_building_internal(type, x, y, exact_coordinates, &check, 0, 0);
    if (clear_cost) {
        *clear_cost = success ? check.clear_cost : 0;
    }
    return success;
}

int building_construction_force_place_building(building_type type, int x, int y, int exact_coordinates, int *clear_cost)
{
    force_place_check check = { 1, 1, 0, 0, { 0 } };
    int success = building_construction_place_building_internal(type, x, y, exact_coordinates, &check, 1, 1);
    if (clear_cost) {
        *clear_cost = success ? check.clear_cost : 0;
    }
    return success;
}
