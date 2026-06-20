#include "building/construction.h"
#include "building/construction_warning.h"
#include "building/count.h"
#include "building/distribution.h"
#include "building/image.h"
#include "building/industry.h"
#include "building/roadblock.h"
#include "building/rotation.h"
#include "building/storage.h"
#include "building/variant.h"
#include "city/warning.h"
#include "game/undo.h"
#include "map/building.h"
#include "map/building_tiles.h"
#include "map/orientation.h"
#include "map/tiles.h"
#include "map/water.h"
#include "map/water_supply.h"

#include "construction_building.h"

#include "building/building.h"
#include "building/building_type_registry_internal.h"
#include "building/dock.h"
#include "building/religion.h"
#include "figure/formation_legion.h"

#include "assets/assets.h"
#include "building/building_record.h"
#include "building/building_type_api.h"
#include "building/menu.h"
#include "building/monument.h"
#include "building/properties.h"
#include "building/warehouse.h"
#include "city/buildings.h"
#include "city/finance.h"
#include "city/resource.h"
#include "city/view.h"
#include "core/config.h"
#include "core/image.h"
#include "core/random.h"
#include "empire/city.h"
#include "map/figure.h"
#include "map/grid.h"
#include "map/property.h"
#include "map/routing.h"
#include "map/routing_terrain.h"
#include "map/terrain.h"
#include "scenario/property.h"

#include <initializer_list>
#include <cstring>
#include <string>
#include <string_view>

struct PlaceWarningMessage {
    warning_type type = WARNING_NONE;
    std::string text;

    void show() const
    {
        if (type.name) {
            city_warning_show(type, reinterpret_cast<const uint8_t *>(text.c_str()));
        }
    }

    void show_when(int warnings_enabled) const
    {
        if (warnings_enabled) {
            show();
        }
    }
};

static std::string legacy_text(const uint8_t *text)
{
    return text ? reinterpret_cast<const char *>(text) : "";
}

static std::string localized_text(translation_key key)
{
    return legacy_text(translation_for(key));
}

static int building_type_attr_is(building_type type, const char *text_id)
{
    const building_type_registry_impl::BuildingType *definition =
        building_type_registry_impl::definition_for_type(type);
    return definition && definition->attr() && text_id && std::strcmp(definition->attr(), text_id) == 0;
}

static int building_type_attr_is_any(building_type type, std::initializer_list<const char *> text_ids)
{
    for (const char *text_id : text_ids) {
        if (building_type_attr_is(type, text_id)) {
            return 1;
        }
    }
    return 0;
}

static building_type building_type_from_attr(const char *text_id)
{
    if (!text_id) {
        return BUILDING_NONE;
    }
    for (int i = 1; i < BUILDING_TYPE_MAX; i++) {
        building_type type = static_cast<building_type>(i);
        const building_type_registry_impl::BuildingType *definition =
            building_type_registry_impl::definition_for_type(type);
        if (definition && definition->attr() && std::strcmp(definition->attr(), text_id) == 0) {
            return type;
        }
    }
    return BUILDING_NONE;
}

static building_type build_type_fort_ground()
{
    static building_type type = BUILDING_NONE;
    if (type == BUILDING_NONE) {
        type = building_type_from_attr("fort_ground");
    }
    return type;
}

static building_type build_type_warehouse_space()
{
    static building_type type = BUILDING_NONE;
    if (type == BUILDING_NONE) {
        type = building_type_from_attr("warehouse_space");
    }
    return type;
}

static int build_type_clear_land_cost()
{
    static int cost = -1;
    if (cost >= 0) {
        return cost;
    }
    const building_type clear_land_type = building_type_from_attr("clear_land");
    if (clear_land_type == BUILDING_NONE || !model_get_building(clear_land_type)) {
        cost = 0;
        return cost;
    }
    cost = model_get_building(clear_land_type)->cost;
    return cost;
}

static building_type build_type_mess_hall()
{
    static building_type type = BUILDING_NONE;
    if (type == BUILDING_NONE) {
        type = building_type_from_attr("mess_hall");
    }
    return type;
}

static building_type build_type_city_mint()
{
    static building_type type = BUILDING_NONE;
    if (type == BUILDING_NONE) {
        type = building_type_from_attr("city_mint");
    }
    return type;
}

static building_type build_type_senate()
{
    static building_type type = BUILDING_NONE;
    if (type == BUILDING_NONE) {
        type = building_type_from_attr("senate");
    }
    return type;
}

static int type_is_basic_venus_temple(const building_type_registry_impl::BuildingType &type)
{
    return type.is_temple(GOD_VENUS, building_type_registry_impl::ReligionTier::Small) ||
        type.is_temple(GOD_VENUS, building_type_registry_impl::ReligionTier::Large);
}

static int is_roadblock_placement_type(building_type type)
{
    return building_type_attr_is_any(type, {
        "roadblock",
        "garden_wall_gate",
        "panelled_garden_gate",
        "looped_garden_gate",
        "hedge_gate_dark",
        "hedge_gate_light",
        "palisade_gate",
        "gatehouse",
        "triumphal_arch",
        "ship_bridge",
        "low_bridge",
    });
}

static int is_waterside_type(building_type type)
{
    const building_type_registry_impl::BuildingType *definition =
        building_type_registry_impl::definition_for_type(type);
    return (definition && std::strcmp(definition->attr(), "dock") == 0) ||
        building_type_attr_is_any(type, {"shipyard", "wharf"});
}

static int is_statue_with_orientation_type(building_type type)
{
    return building_type_attr_is_any(type, {
        "small_statue",
        "medium_statue",
        "horse_statue",
        "legion_statue",
        "gladiator_statue",
    });
}

static void replace_all(std::string &text, std::string_view placeholder, std::string_view replacement)
{
    size_t position = 0;
    while ((position = text.find(placeholder, position)) != std::string::npos) {
        text.replace(position, placeholder.size(), replacement);
        position += replacement.size();
    }
}

static std::string localized_text_from_key_name(const char *key_name)
{
    return key_name ? legacy_text(translation_for_key(key_name)) : "";
}

static std::string building_type_name(building_type type)
{
    std::string text = localized_text_from_key_name(building_type_registry_get_name_key(type));
    if (!text.empty()) {
        return text;
    }
    return localized_text_from_key_name(building_type_registry_get_button_text_key(type));
}

static std::string resource_name(resource_type resource)
{
    resource_data *data = resource_get_data(resource);
    return data ? legacy_text(data->text) : "";
}

static PlaceWarningMessage warning_text(warning_type type, translation_key key)
{
    return {type, localized_text(key)};
}

static PlaceWarningMessage warning_from_template(warning_type type, translation_key key, building_type building)
{
    std::string text = localized_text(key);
    replace_all(text, "<building-type>", building_type_name(building));
    return {type, text};
}

static PlaceWarningMessage warning_from_template(warning_type type, translation_key key, building_type building,
    resource_type resource)
{
    std::string text = localized_text(key);
    replace_all(text, "<building-type>", building_type_name(building));
    replace_all(text, "<resource>", resource_name(resource));
    return {type, text};
}

static PlaceWarningMessage warning_from_template(warning_type type, translation_key key, building_type building,
    building_type required_building)
{
    std::string text = localized_text(key);
    replace_all(text, "<building-type>", building_type_name(building));
    replace_all(text, "<required-building>", building_type_name(required_building));
    return {type, text};
}

static PlaceWarningMessage clear_land_needed_warning()
{
    return warning_text(WARNING_CLEAR_LAND_NEEDED, "TR_CITY_WARNING_CLEAR_LAND_NEEDED");
}

static PlaceWarningMessage dock_open_water_needed_warning()
{
    return warning_text(WARNING_DOCK_OPEN_WATER_NEEDED, "TR_CITY_WARNING_DOCK_OPEN_WATER_NEEDED");
}

static PlaceWarningMessage max_grand_temples_warning()
{
    return warning_text(WARNING_MAX_GRAND_TEMPLES, "TR_WARNING_MAX_GRAND_TEMPLES");
}

static PlaceWarningMessage max_legions_reached_warning()
{
    return warning_text(WARNING_MAX_LEGIONS_REACHED, "TR_CITY_WARNING_MAX_LEGIONS_REACHED");
}

static PlaceWarningMessage shore_needed_warning()
{
    return warning_text(WARNING_SHORE_NEEDED, "TR_CITY_WARNING_SHORE_NEEDED");
}

static PlaceWarningMessage wall_needed_warning()
{
    return warning_text(WARNING_WALL_NEEDED, "TR_CITY_WARNING_WALL_NEEDED");
}

static PlaceWarningMessage warehouse_tower_road_access_warning()
{
    return warning_text(WARNING_WAREHOUSE_TOWER, "TR_WARNING_NO_WAREHOUSE_TOWER_ROAD_ACCESS");
}

static PlaceWarningMessage building_needs_materials_warning(building_type building)
{
    return warning_from_template(WARNING_RESOURCES_NOT_AVAILABLE, "TR_CITY_WARNING_TEMPLATE_BUILDING_NEEDS_MATERIALS",
        building);
}

static PlaceWarningMessage building_needs_resource_warning(building_type building, resource_type resource)
{
    return warning_from_template(WARNING_MISSING_RESOURCE, "TR_CITY_WARNING_TEMPLATE_BUILDING_NEEDS_RESOURCE", building,
        resource);
}

static PlaceWarningMessage no_mess_hall_warning()
{
    return warning_from_template(WARNING_NO_MESS_HALL, "TR_CITY_WARNING_TEMPLATE_MISSING_PRODUCER",
        build_type_mess_hall());
}

static PlaceWarningMessage one_building_of_type_warning(building_type building)
{
    return warning_from_template(WARNING_ONE_BUILDING_OF_TYPE, "TR_CITY_WARNING_TEMPLATE_ONE_BUILDING_OF_TYPE", building);
}

static PlaceWarningMessage city_mint_needs_senate_warning()
{
    return warning_from_template(WARNING_SENATE_NEEDED, "TR_CITY_WARNING_TEMPLATE_BUILDING_NEEDS_BUILDING",
        build_type_city_mint(), build_type_senate());
}

static PlaceWarningMessage build_senate_warning()
{
    return warning_from_template(WARNING_BUILD_SENATE, "TR_CITY_WARNING_TEMPLATE_MISSING_PRODUCER",
        build_type_senate());
}

static void add_fort(building_type type, building *fort)
{
    fort->prev_part_building_id = 0;
    map_building_tiles_add(fort->id, fort->x, fort->y, fort->size, building_image_get(fort), TERRAIN_BUILDING);
    fort->subtype.fort_figure_type = building_count_forts_get_figure_type_from_building(type);

    // create parade ground
    const int offsets_x[] = { 3, -1, -4, 0 };
    const int offsets_y[] = { -1, -4, 0, 3 };
    int id = fort->id;
    if (build_type_fort_ground() == BUILDING_NONE) {
        return;
    }
    building *ground = building_create(build_type_fort_ground(), fort->x + offsets_x[building_rotation_get_rotation()],
        fort->y + offsets_y[building_rotation_get_rotation()]);
    game_undo_add_building(ground);
    fort = building_get(id);
    ground->prev_part_building_id = fort->id;
    fort->next_part_building_id = ground->id;
    ground->next_part_building_id = 0;
    map_building_tiles_add(ground->id, fort->x + offsets_x[building_rotation_get_rotation()],
     fort->y + offsets_y[building_rotation_get_rotation()], 4, image_group(GROUP_BUILDING_FORT) + 1, TERRAIN_BUILDING);

    Building fort_object(*fort);
    Building ground_object(*ground);
    fort_object.set_formation_id(formation_legion_create_for_fort(fort_object));
    ground_object.set_formation_id(fort_object.formation_id());
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
    building *part2 = building_create(part1->type, part1->x + x_offset, part1->y + y_offset);
    game_undo_add_building(part2);

    building_rotation_get_offset_with_rotation(10, building_rotation_get_rotation(), &x_offset, &y_offset);
    building *part3 = building_create(part1->type, part1->x + x_offset, part1->y + y_offset);
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
    if (prev_id <= 0) {
        return 0;
    }
    building_type type = build_type_warehouse_space();
    if (type == BUILDING_NONE) {
        return prev_id;
    }
    building *b = building_create(type, x, y);
    if (!b || b->id <= 0) {
        return prev_id;
    }

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
    // Adjust warehouse to tower position
    b->x = b->x + x_offset[tower];
    b->y = b->y + y_offset[tower];
    b->grid_offset = map_grid_offset(b->x, b->y);
    game_undo_adjust_building(b);

    building_get(prev)->next_part_building_id = 0;
    map_tiles_update_area_roads(b->x, b->y, 5);
    if (!map_has_road_access_warehouse(b->x, b->y, 0)) {
        warehouse_tower_road_access_warning().show();
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
    if (Building(b).refresh_graphic_if_native()) {
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
    Building building_obj(b);
    if (building_variant_has_variants(b->type)) {
        b->variant = building_rotation_get_rotation_with_limit(building_variant_get_number_of_variants(b->type));
    }
    // Native graphics options share the saved variant byte, so XML-owned
    // buildings seed it after legacy variant/rotation setup has run.
    building_obj.assign_graphic_variant(1);
    if (building_obj.type && type_is_basic_venus_temple(*building_obj.type)) {
        if (const building_type_registry_impl::Distribution *distribution = building_obj.type->distribution()) {
            distribution->set_acceptance(building_obj, 0);
        }
    }
    if (building_type_registry_has_phased_construction(type)) {
        int road_update_radius = building_type_registry_get_construction_road_update_radius(type);
        if (road_update_radius > 0) {
            map_tiles_update_area_roads(b->x, b->y, road_update_radius);
        }
        if (building_obj.type &&
            building_obj.type->is_temple(GOD_MARS, building_type_registry_impl::ReligionTier::Grand)) {
            b->accepted_goods[resource_weapons()] = 1;
            b->accepted_goods[RESOURCE_NONE] = 1;
        }
        building_monument_set_phase(b, MONUMENT_START);
    } else if (building_type_attr_is(type, "colosseum")) {
        map_tiles_update_area_roads(b->x, b->y, 5);
        building_monument_set_phase(b, MONUMENT_START);
    } else if (building_is_farm(type)) {
        map_building_tiles_add_farm(b->id, b->x, b->y, building_image_get_base_farm_crop(type), 0);
    } else if (building_type_attr_is(type, "granary")) {
        add_granary(b);
    } else if (building_obj.type &&
        building_obj.type->is_temple(GOD_VENUS, building_type_registry_impl::ReligionTier::Small)) {
        add_building(b);
        if (const building_type_registry_impl::Distribution *distribution = building_obj.type->distribution()) {
            distribution->set_acceptance(building_obj, 0);
        }
    } else if (building_type_attr_is_any(type, {"large_mausoleum", "nymphaeum", "city_mint"})) {
        map_tiles_update_area_roads(b->x, b->y, 5);
        building_monument_set_phase(b, MONUMENT_START);
    } else if (building_type_attr_is(type, "roadblock")) {
        add_building(b);
        map_terrain_add_roadblock_road(b->x, b->y);
        map_tiles_update_area_roads(b->x, b->y, 5);
        map_tiles_update_all_plazas();
    } else if (building_type_attr_is_any(type, {"shipyard", "wharf"})) {
        b->subtype.orientation = waterside_orientation_abs;
        map_water_add_building(b->id, b->x, b->y, 2);
    } else if (building_obj.type && building_obj.type->attr() &&
        std::strcmp(building_obj.type->attr(), "dock") == 0) {
        b->subtype.orientation = waterside_orientation_abs;
        b->data.dock.orientation = waterside_orientation_abs;
        map_water_add_building(b->id, b->x, b->y, size);
    } else if (building_type_attr_is(type, "tower")) {
        map_terrain_remove_with_radius(b->x, b->y, 2, 0, TERRAIN_WALL);
        map_building_tiles_add(b->id, b->x, b->y, size, building_image_get(b),
            TERRAIN_BUILDING | TERRAIN_GATEHOUSE);
        map_tiles_update_area_walls(b->x, b->y, 5);
    } else if (building_type_attr_is(type, "gatehouse")) {
        b->subtype.orientation = orientation;
        map_building_tiles_add_remove(b->id, b->x, b->y, size,
            building_image_get(b), TERRAIN_BUILDING | TERRAIN_GATEHOUSE, TERRAIN_CLEARABLE & ~TERRAIN_HIGHWAY);
        map_orientation_update_buildings();
        map_terrain_add_gatehouse_roads(b->x, b->y, orientation);
        map_tiles_update_area_roads(b->x, b->y, 5);
        map_tiles_update_area_highways(b->x, b->y, 3);
        map_tiles_update_all_plazas();
        map_tiles_update_area_walls(b->x, b->y, 5);
    } else if (building_type_attr_is(type, "triumphal_arch")) {
        b->subtype.orientation = orientation;
        add_building(b);
        map_orientation_update_buildings();
        map_terrain_add_triumphal_arch_roads(b->x, b->y, orientation);
        map_tiles_update_area_roads(b->x, b->y, 5);
        map_tiles_update_all_plazas();
        city_buildings_build_triumphal_arch();
        building_menu_update();
        building_construction_clear_type();
    } else if (building_type_attr_is(type, "warehouse")) {
        add_warehouse(b, orientation);
    } else if (building_type_attr_is(type, "hippodrome")) {
        add_hippodrome(b);
        building_monument_set_phase(b, MONUMENT_START);
        building *b2 = building_get(b->next_part_building_id);
        building_monument_set_phase(b2, MONUMENT_START);
        building *b3 = building_get(b2->next_part_building_id);
        building_monument_set_phase(b3, MONUMENT_START);
    } else if (building_is_fort(type)) {
        add_fort(type, b);
    } else if (building_type_attr_is(type, "pantheon")) {
        map_tiles_update_area_roads(b->x, b->y, 9);
        building_monument_set_phase(b, MONUMENT_START);
    } else if (building_type_attr_is(type, "mess_hall")) {
        b->data.market.is_mess_hall = 1;
        add_building(b);
    } else if (is_statue_with_orientation_type(type)) {
        b->subtype.orientation = building_rotation_get_rotation();
        add_building(b);
    } else if (building_type_attr_is(type, "small_mausoleum")) {
        b->subtype.orientation = building_rotation_get_rotation();
        map_tiles_update_area_roads(b->x, b->y, 4);
        building_monument_set_phase(b, MONUMENT_START);
    } else if (building_type_attr_is(type, "depot")) {
        add_depot(b);
    } else if (building_obj.type &&
        building_obj.type->is_temple_tier(building_type_registry_impl::ReligionTier::Shrine)) {
        b->subtype.orientation = building_rotation_get_rotation();
        add_building(b);
    } else if (building_type_attr_is(type, "barracks")) {
        b->accepted_goods[resource_weapons()] = 1;
        b->accepted_goods[RESOURCE_NONE] = 1;
        add_building(b);
    } else {
        add_building(b);
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

static int instant_building_has_required_resources(building_type type, int emit_warnings)
{
    for (resource_type resource = (RESOURCE_NONE + 1); resource < RESOURCE_SLOT_COUNT; resource = static_cast<resource_type>(resource + 1)) {
        int amount = building_type_registry_get_instant_construction_requirement(type, resource);
        if (amount > 0 && city_resource_count_warehouses_amount(resource) < amount) {
            building_needs_resource_warning(type, resource).show_when(emit_warnings);
            return 0;
        }
    }
    return 1;
}

static int terrain_requirement_allows_placement(int x, int y, PlaceWarningMessage *warning)
{
    warning_type type = WARNING_NONE;
    translation_key text_key = "TR_CITY_WARNING_CLEAR_LAND_NEEDED";
    if (building_construction_can_place_on_terrain(x, y, &type, &text_key)) {
        return 1;
    }
    if (warning) {
        *warning = warning_text(type, text_key);
    }
    return 0;
}

static void instant_building_remove_required_resources(building_type type)
{
    for (resource_type resource = (RESOURCE_NONE + 1); resource < RESOURCE_SLOT_COUNT; resource = static_cast<resource_type>(resource + 1)) {
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
    check->clear_cost += build_type_clear_land_cost();
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
    int exact_coordinates, force_place_check *force_check, int emit_warnings, int place_building)
{
    int grid_offset = map_grid_offset(x, y);

    int terrain_mask = TERRAIN_ALL;
    if (is_roadblock_placement_type(type) ||
        (config_get(CONFIG_GP_CH_WAREHOUSES_GRANARIES_OVER_ROAD_PLACEMENT) &&
        building_type_attr_is_any(type, {"granary", "warehouse"}))) {
        terrain_mask = building_type_attr_is(type, "gatehouse") ? ~TERRAIN_WALL & ~TERRAIN_ROAD &
            ~TERRAIN_HIGHWAY & ~TERRAIN_BUILDING : ~TERRAIN_ROAD & ~TERRAIN_HIGHWAY;
        //allow building gatehouses over walls and roads, other non-bridge roadblocks over roads and highways
    } else if (building_type_attr_is(type, "tower")) {
        terrain_mask = ~TERRAIN_WALL & ~TERRAIN_BUILDING;
    } else if (building_type_attr_is_any(type, {"reservoir", "draggable_reservoir"})) {
        terrain_mask = ~TERRAIN_AQUEDUCT;
    }
    int size = building_properties_for_type(type)->size;
    if (building_type_attr_is(type, "warehouse")) {
        size = 3;
    }
    // Do not check for a figure when build a roadblock of single tile size
    // TODO: do not check for figures on tiles that are citizen passable in general
    int check_figure = building_type_attr_is(type, "roadblock") && size == 1 ? 0 : 1;
    int building_orientation = 0;
    if (building_type_attr_is_any(type, {"gatehouse", "warehouse"})) {
        //check if there's a preset orientation from old building
        building *old_b = building_main(building_get(map_building_rubble_building_id(grid_offset)));
        if (old_b && building_type_attr_is_any(old_b->type, {"gatehouse", "warehouse", "warehouse_space"})) {
            building_orientation = old_b->subtype.orientation;
        } else if (building_type_attr_is(type, "gatehouse")) {
            building_orientation = map_orientation_for_gatehouse(x, y);
        } else if (building_type_attr_is(type, "warehouse")) {
            building_orientation = building_rotation_get_rotation();
        }
    } else if (building_type_attr_is(type, "triumphal_arch")) {
        building_orientation = map_orientation_for_triumphal_arch(x, y);
    }
    if (!exact_coordinates) {
        building_construction_offset_start_from_orientation(&x, &y, size);
    }
    // extra checks
    if (building_type_attr_is(type, "tower")) {
        if (!map_terrain_all_tiles_in_radius_are(x, y, size, 0, TERRAIN_WALL)) {
            clear_land_needed_warning().show_when(emit_warnings);
            return 0;
        }
        if (!map_terrain_all_tiles_in_radius_are(x, y, 2, 0, TERRAIN_BUILDING)) {
            wall_needed_warning().show_when(emit_warnings);
            return 0;
        }
        if (!building_orientation) {
            building_orientation = building_rotation_get_rotation() + 1;
            if (building_orientation > 4) {
                building_orientation = 1;
            }
        }
    }
    if (building_type_attr_is(type, "gatehouse")) {
        if (!tiles_are_clear_or_force_clearable(x, y, size, terrain_mask, check_figure, force_check)) {
            clear_land_needed_warning().show_when(emit_warnings);
            return 0;
        }
        if (!check_gatehouse_tiles(grid_offset)) { //helper to make sure all building tiles are on walls
            clear_land_needed_warning().show_when(emit_warnings);
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
    if (building_type_attr_is(type, "roadblock")) {
        if (map_tiles_are_clear(x, y, size, TERRAIN_ROAD, check_figure)) {
            return 0;
        }
    }
    if (building_type_attr_is(type, "triumphal_arch")) {
        if (!tiles_are_clear_or_force_clearable(x, y, size, terrain_mask, check_figure, force_check)) {
            clear_land_needed_warning().show_when(emit_warnings);
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

    if (is_waterside_type(type)) {
        waterside_orientation_abs = -1;
        waterside_orientation_rel = -1;
        int blocked_tiles = map_water_determine_orientation(x, y, building_properties_for_type(type)->size, 0,
            &waterside_orientation_abs, &waterside_orientation_rel, 0, 0);
        if (waterside_orientation_abs < 0) {
            shore_needed_warning().show_when(emit_warnings);
            return 0;
        }
        const waterside_tile_loop *loop =
            map_water_get_waterside_tile_loop(waterside_orientation_abs, building_properties_for_type(type)->size);
        if (!map_water_has_water_in_front(x, y, 0, loop, 0)) {
            shore_needed_warning().show_when(emit_warnings);
            return 0;
        }
        if (blocked_tiles &&
            !tiles_are_clear_or_force_clearable(x, y, building_properties_for_type(type)->size,
                TERRAIN_ALL & ~TERRAIN_WATER, check_figure, force_check)) {
            clear_land_needed_warning().show_when(emit_warnings);
            return 0;
        }
        const building_type_registry_impl::BuildingType *definition =
            building_type_registry_impl::definition_for_type(type);
        if (definition && std::strcmp(definition->attr(), "dock") == 0 &&
            !building_dock_is_connected_to_open_water(x, y)) {
            dock_open_water_needed_warning().show_when(emit_warnings);
            return 0;
        }
    } else {
        if ((exact_coordinates || force_check) &&
            !tiles_are_clear_or_force_clearable(x, y, size, terrain_mask, check_figure, force_check)) {
            clear_land_needed_warning().show_when(emit_warnings);
            return 0;
        }
        PlaceWarningMessage terrain_warning;
        if ((exact_coordinates || force_check) && !terrain_requirement_allows_placement(x, y, &terrain_warning)) {
            terrain_warning.show_when(emit_warnings);
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
            clear_land_needed_warning().show_when(emit_warnings);
            return 0;
        }
        if (formation_get_num_legions_cached() >= formation_get_max_legions()) {
            max_legions_reached_warning().show_when(emit_warnings);
            return 0;
        }
        if (!city_buildings_has_mess_hall()) {
            no_mess_hall_warning().show_when(emit_warnings);
            return 0;
        }
    }

    if (!building_monument_has_required_resources_to_build(type)) {
        building_needs_materials_warning(type).show_when(emit_warnings);
        return 0;
    }
    if (!instant_building_has_required_resources(type, emit_warnings)) {
        return 0;
    }

    if (building_monument_get_id(type) && !building_monument_type_is_mini_monument(type)) {
        one_building_of_type_warning(type).show_when(emit_warnings);
        return 0;
    }

    if (building_monument_is_grand_temple(type) &&
        building_monument_count_grand_temples() >= config_get(CONFIG_GP_CH_MAX_GRAND_TEMPLES)) {
        max_grand_temples_warning().show_when(emit_warnings);
        return 0;
    }
    if (building_type_attr_is(type, "colosseum")) {
        if (building_count_total(type)) {
            one_building_of_type_warning(type).show_when(emit_warnings);
            return 0;
        }
    }
    if (building_type_attr_is(type, "hippodrome")) {
        if (city_buildings_has_hippodrome()) {
            one_building_of_type_warning(type).show_when(emit_warnings);
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
            clear_land_needed_warning().show_when(emit_warnings);
            return 0;
        }
    }
    if (building_type_attr_is(type, "senate") && city_buildings_has_senate()) {
        one_building_of_type_warning(type).show_when(emit_warnings);
        return 0;
    }
    if (building_type_attr_is(type, "city_mint")) {
        if (city_buildings_has_city_mint()) {
            one_building_of_type_warning(type).show_when(emit_warnings);
            return 0;
        }
        if (!city_buildings_has_senate()) {
            city_mint_needs_senate_warning().show_when(emit_warnings);
            build_senate_warning().show_when(emit_warnings);
            return 0;
        }
    }
    if (building_type_attr_is(type, "lighthouse") && city_buildings_has_lighthouse()) {
        one_building_of_type_warning(type).show_when(emit_warnings);
        return 0;
    }
    if (building_type_attr_is(type, "caravanserai") && city_buildings_has_caravanserai()) {
        one_building_of_type_warning(type).show_when(emit_warnings);
        return 0;
    }
    if (building_type_attr_is(type, "barracks") && city_buildings_has_barracks() && !config_get(CONFIG_GP_CH_MULTIPLE_BARRACKS)) {
        one_building_of_type_warning(type).show_when(emit_warnings);
        return 0;
    }
    if (building_type_attr_is(type, "mess_hall") && city_buildings_has_mess_hall()) {
        one_building_of_type_warning(type).show_when(emit_warnings);
        return 0;
    }
    if (emit_warnings) {
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
