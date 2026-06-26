#include "building/construction.h"
#include "building/construction_plan.h"
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
#include <utility>

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

static int building_definition_attr_is(
    const building_type_registry_impl::BuildingType &definition,
    const char *text_id)
{
    return std::strcmp(definition.attr(), text_id) == 0;
}

static int building_definition_attr_is_any(
    const building_type_registry_impl::BuildingType &definition,
    std::initializer_list<const char *> text_ids)
{
    for (const char *text_id : text_ids) {
        if (building_definition_attr_is(definition, text_id)) {
            return 1;
        }
    }
    return 0;
}

static int building_type_roadblock_passage_is(
    building_type type,
    building_type_registry_impl::RoadblockPassageType passage)
{
    const building_type_registry_impl::BuildingType *definition =
        building_type_registry_impl::definition_for_type(type);
    return definition && definition->roadblock().passage_type() == passage;
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

static const building_type_registry_impl::BuildingType *definition_for_placement_type(building_type type)
{
    return building_type_registry_impl::definition_for_type(type);
}

static int building_registers_water_footprint(const building_type_registry_impl::BuildingType &definition)
{
    return definition.foundation().policy_type() == building_type_registry_impl::FoundationPolicy::Shoreline;
}

static int building_uses_wall_foundation(const building_type_registry_impl::BuildingType &definition)
{
    return definition.foundation().policy_type() == building_type_registry_impl::FoundationPolicy::Wall;
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

static int building_is_single_tile_roadblock(
    const building_type_registry_impl::BuildingType &definition,
    int size)
{
    return size == 1 &&
        definition.roadblock().kind() != building_type_registry_impl::RoadblockKind::None &&
        definition.roadblock().kind() != building_type_registry_impl::RoadblockKind::Bridge;
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

static PlaceWarningMessage building_requires_building_warning(building_type building, building_type required_building)
{
    return warning_from_template(WARNING_SENATE_NEEDED, "TR_CITY_WARNING_TEMPLATE_BUILDING_NEEDS_BUILDING",
        building, required_building);
}

static PlaceWarningMessage build_required_building_warning(building_type required_building)
{
    return warning_from_template(WARNING_BUILD_SENATE, "TR_CITY_WARNING_TEMPLATE_MISSING_PRODUCER",
        required_building);
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

static void register_single_tile_building_terrain(building *b, int terrain)
{
    int grid_offset = map_grid_offset(b->x, b->y);
    map_terrain_remove(grid_offset, TERRAIN_CLEARABLE);
    map_terrain_add(grid_offset, terrain);
    map_building_set(grid_offset, b->id);
    map_property_clear_constructing(grid_offset);
    map_property_set_multi_tile_size(grid_offset, 1);
    map_property_set_multi_tile_xy(grid_offset, 0, 0, 1);
}

static void refresh_placed_tile_regions(const building_construction::ConstructionPlacementPlan &placement)
{
    for (const building_construction::ConstructionPlacementPart &part : placement.parts()) {
        if (!part.definition->has_tile()) {
            continue;
        }
        map_tiles_update_region_tile(part.x, part.y, part.x + part.size - 1, part.y + part.size - 1,
            part.definition->tile());
    }
}

static const building_construction::ConstructionPlacementPart *find_placement_part(
    const building_construction::ConstructionPlacementPlan &placement,
    building_type type)
{
    for (const building_construction::ConstructionPlacementPart &part : placement.parts()) {
        if (part.type == type) {
            return &part;
        }
    }
    return nullptr;
}

static void add_composed_building(
    building *main_record,
    const building_construction::ConstructionPlacementPlan &placement)
{
    if (!main_record) {
        return;
    }
    const building_construction::ConstructionPlacementPart *main_part =
        find_placement_part(placement, main_record->type);
    const building_type_registry_impl::BuildingType *definition =
        main_part ? main_part->definition : definition_for_placement_type(main_record->type);
    if (!main_part || !definition || !definition->has_composition()) {
        add_building(main_record);
        return;
    }

    main_record->prev_part_building_id = 0;
    main_record->next_part_building_id = 0;
    if (main_part && (main_record->x != main_part->x || main_record->y != main_part->y)) {
        main_record->x = static_cast<unsigned char>(main_part->x);
        main_record->y = static_cast<unsigned char>(main_part->y);
        main_record->grid_offset = map_grid_offset(main_record->x, main_record->y);
        game_undo_adjust_building(main_record);
    }
    add_building(main_record);

    int previous_id = main_record->id;
    for (const building_construction::ConstructionPlacementPart &part : placement.parts()) {
        if (&part == main_part || part.type == BUILDING_NONE || previous_id <= 0) {
            continue;
        }

        building *child = building_create(part.type, part.x, part.y);
        if (!child || child->id <= 0) {
            continue;
        }
        game_undo_add_building(child);
        child->prev_part_building_id = previous_id;
        child->next_part_building_id = 0;
        if (definition->composition().child_inherits_orientation()) {
            child->subtype.orientation = main_record->subtype.orientation;
        }
        child->road_network_id = main_record->road_network_id;
        child->distance_from_entry = main_record->distance_from_entry;
        child->road_access_x = main_record->road_access_x;
        child->road_access_y = main_record->road_access_y;
        child->has_road_access = main_record->has_road_access;
        child->houses_covered = main_record->houses_covered;
        child->percentage_houses_covered = main_record->percentage_houses_covered;
        child->labor_access_score = main_record->labor_access_score;
        child->variant = main_record->variant;
        building_get(previous_id)->next_part_building_id = child->id;
        add_building(child);
        previous_id = child->id;
    }
}

static void set_monument_phase_for_parts(building *main_record, int phase)
{
    for (building *part = main_record; part && part->id > 0; part = building_get(part->next_part_building_id)) {
        building_monument_set_phase(part, phase);
        if (!part->next_part_building_id) {
            break;
        }
    }
}

static void assign_fort_formation_to_parts(building *main_record)
{
    if (!main_record) {
        return;
    }
    Building fort_object(main_record);
    const int formation_id = formation_legion_create_for_fort(fort_object);
    for (Building part(main_record); part.id(); part = part.next()) {
        part.set_formation_id(formation_id);
        if (!part.next_part_id()) {
            break;
        }
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

static void add_to_map(
    building_type type,
    building *b,
    int orientation,
    const building_construction::ConstructionPlacementPlan &placement)
{
    int size = placement.placement_size();
    int waterside_orientation_abs = placement.waterside_orientation_absolute();
    Building building_obj(b);
    const building_type_registry_impl::BuildingType &definition = *building_obj.type;
    if (building_variant_has_variants(b->type)) {
        b->variant = building_rotation_get_rotation_with_limit(building_variant_get_number_of_variants(b->type));
    }
    // Native graphics options share the saved variant byte, so XML-owned
    // buildings seed it after legacy variant/rotation setup has run.
    building_obj.assign_graphic_variant(1);
    if (type_is_basic_venus_temple(definition)) {
        if (const building_type_registry_impl::Distribution *distribution = definition.distribution()) {
            distribution->set_acceptance(building_obj, 0);
        }
    }
    if (definition.has_composition()) {
        if (definition.is_warehouse()) {
            b->storage_id = building_storage_create(b->id);
            b->subtype.orientation = orientation;
        } else if (building_is_fort(type)) {
            b->subtype.fort_figure_type = building_count_forts_get_figure_type_from_building(type);
        } else if (definition.composition().child_inherits_orientation()) {
            b->subtype.orientation = building_rotation_get_rotation();
        }
        add_composed_building(b, placement);
        if (building_type_registry_has_phased_construction(type)) {
            int road_update_radius = building_type_registry_get_construction_road_update_radius(type);
            if (road_update_radius > 0) {
                map_tiles_update_area_roads(b->x, b->y, road_update_radius);
            }
            if (definition.is_temple(GOD_MARS, building_type_registry_impl::ReligionTier::Grand)) {
                b->accepted_goods[resource_weapons()] = 1;
                b->accepted_goods[RESOURCE_NONE] = 1;
            }
            set_monument_phase_for_parts(b, MONUMENT_START);
        }
        if (definition.is_warehouse()) {
            map_tiles_update_area_roads(b->x, b->y, 5);
            if (!map_has_road_access_warehouse(b->x, b->y, 0)) {
                warehouse_tower_road_access_warning().show();
            }
        } else if (building_is_fort(type)) {
            assign_fort_formation_to_parts(b);
        }
    } else if (building_type_registry_has_phased_construction(type)) {
        int road_update_radius = building_type_registry_get_construction_road_update_radius(type);
        if (road_update_radius > 0) {
            map_tiles_update_area_roads(b->x, b->y, road_update_radius);
        }
        if (definition.is_temple(GOD_MARS, building_type_registry_impl::ReligionTier::Grand)) {
            b->accepted_goods[resource_weapons()] = 1;
            b->accepted_goods[RESOURCE_NONE] = 1;
        }
        building_monument_set_phase(b, MONUMENT_START);
    } else if (building_is_farm(type)) {
        add_building(b);
    } else if (definition.is_granary()) {
        add_granary(b);
    } else if (definition.is_temple(GOD_VENUS, building_type_registry_impl::ReligionTier::Small)) {
        add_building(b);
        if (const building_type_registry_impl::Distribution *distribution = definition.distribution()) {
            distribution->set_acceptance(building_obj, 0);
        }
    } else if (building_definition_attr_is_any(definition, {"large_mausoleum", "nymphaeum"})) {
        map_tiles_update_area_roads(b->x, b->y, 5);
        building_monument_set_phase(b, MONUMENT_START);
    } else if (building_is_single_tile_roadblock(definition, size)) {
        add_building(b);
        register_single_tile_building_terrain(b, TERRAIN_BUILDING);
        map_terrain_add_roadblock_road(b->x, b->y);
        map_tiles_update_area_roads(b->x, b->y, 5);
    } else if (building_registers_water_footprint(definition)) {
        b->subtype.orientation = waterside_orientation_abs;
        b->data.dock.orientation = waterside_orientation_abs;
        map_water_add_building(b->id, b->x, b->y, size);
    } else if (building_uses_wall_foundation(definition)) {
        map_terrain_remove_with_radius(b->x, b->y, 2, 0, TERRAIN_WALL);
        map_building_tiles_add(b->id, b->x, b->y, size, building_image_get(b),
            TERRAIN_BUILDING | TERRAIN_GATEHOUSE);
        map_tiles_update_area_walls(b->x, b->y, 5);
    } else if (definition.roadblock().is_wall_gate()) {
        b->subtype.orientation = orientation;
        map_building_tiles_add_remove(b->id, b->x, b->y, size,
            building_image_get(b), TERRAIN_BUILDING | TERRAIN_GATEHOUSE, TERRAIN_CLEARABLE & ~TERRAIN_HIGHWAY);
        map_orientation_update_buildings();
        map_terrain_add_gatehouse_roads(b->x, b->y, orientation);
        map_tiles_update_area_roads(b->x, b->y, 5);
        map_tiles_update_area_highways(b->x, b->y, 3);
        map_tiles_update_all_plazas();
        map_tiles_update_area_walls(b->x, b->y, 5);
    } else if (definition.roadblock().has_center_road_passage()) {
        b->subtype.orientation = orientation;
        add_building(b);
        map_orientation_update_buildings();
        map_terrain_add_triumphal_arch_roads(b->x, b->y, orientation);
        map_tiles_update_area_roads(b->x, b->y, 5);
        map_tiles_update_all_plazas();
        city_buildings_build_triumphal_arch();
        building_menu_update();
        building_construction_clear_type();
    } else if (definition.is_mess_hall()) {
        b->data.market.is_mess_hall = 1;
        add_building(b);
    } else if (is_statue_with_orientation_type(type)) {
        b->subtype.orientation = building_rotation_get_rotation();
        add_building(b);
    } else if (building_definition_attr_is(definition, "small_mausoleum")) {
        b->subtype.orientation = building_rotation_get_rotation();
        map_tiles_update_area_roads(b->x, b->y, 4);
        building_monument_set_phase(b, MONUMENT_START);
    } else if (building_definition_attr_is(definition, "cart_depot")) {
        add_depot(b);
    } else if (definition.is_temple_tier(building_type_registry_impl::ReligionTier::Shrine)) {
        b->subtype.orientation = building_rotation_get_rotation();
        add_building(b);
    } else if (building_definition_attr_is(definition, "barracks")) {
        b->accepted_goods[resource_weapons()] = 1;
        b->accepted_goods[RESOURCE_NONE] = 1;
        add_building(b);
    } else {
        add_building(b);
    }
    refresh_placed_tile_regions(placement);
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

static int construction_config_flag_enabled(building_type_registry_impl::ConstructionConfigFlag flag)
{
    switch (flag) {
        case building_type_registry_impl::ConstructionConfigFlag::MultipleBarracks:
            return config_get(CONFIG_GP_CH_MULTIPLE_BARRACKS);
        case building_type_registry_impl::ConstructionConfigFlag::None:
        default:
            return 0;
    }
}

static int construction_rules_allow_placement(
    const building_type_registry_impl::BuildingType &definition,
    int emit_warnings)
{
    const building_type type = definition.type();
    const building_type_registry_impl::ConstructionDefinition &construction =
        definition.construction();
    const int max_count = construction.max_count();
    if (max_count > 0 &&
        !construction_config_flag_enabled(construction.max_count_unless_config()) &&
        building_count_total(type) >= max_count) {
        one_building_of_type_warning(type).show_when(emit_warnings);
        return 0;
    }

    const building_type required_building = construction.required_building_type();
    if (required_building != BUILDING_NONE && building_count_total(required_building) <= 0) {
        building_requires_building_warning(type, required_building).show_when(emit_warnings);
        build_required_building_warning(required_building).show_when(emit_warnings);
        return 0;
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

static void force_place_copy_plan_offsets(
    force_place_check *check,
    const building_construction::ConstructionPlacementPlan &plan)
{
    if (!check || !check->active) {
        return;
    }
    for (int grid_offset : plan.clear_offsets()) {
        force_place_add_clear_offset(check, grid_offset);
    }
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

static int initial_building_orientation(
    const building_type_registry_impl::BuildingType &definition,
    int x,
    int y)
{
    int grid_offset = map_grid_offset(x, y);
    int building_orientation = 0;
    if (definition.roadblock().is_wall_gate() || definition.is_warehouse()) {
        //check if there's a preset orientation from old building
        building *old_b = building_main(building_get(map_building_rubble_building_id(grid_offset)));
        if (old_b && (building_type_roadblock_passage_is(
                old_b->type,
                building_type_registry_impl::RoadblockPassageType::WallGate) ||
                building_type_attr_is_any(old_b->type, {"warehouse", "warehouse_space"}))) {
            building_orientation = old_b->subtype.orientation;
        } else if (definition.roadblock().is_wall_gate()) {
            building_orientation = map_orientation_for_gatehouse(x, y);
        } else if (definition.is_warehouse()) {
            building_orientation = building_rotation_get_rotation();
        }
    } else if (definition.roadblock().has_center_road_passage()) {
        building_orientation = map_orientation_for_triumphal_arch(x, y);
    }
    return building_orientation;
}

static int building_construction_validate_placement_plan(
    const building_construction::ConstructionPlacementPlan &placement,
    force_place_check *force_check,
    int emit_warnings,
    int *building_orientation)
{
    const building_type type = placement.type();
    const building_type_registry_impl::BuildingType &definition = placement.definition();
    const int x = placement.origin_x();
    const int y = placement.origin_y();
    const int grid_offset = map_grid_offset(placement.cursor_x(), placement.cursor_y());
    const int size = placement.placement_size();

    // extra checks
    if (building_uses_wall_foundation(definition)) {
        if (!map_terrain_all_tiles_in_radius_are(x, y, 2, 0, TERRAIN_BUILDING)) {
            wall_needed_warning().show_when(emit_warnings);
            return 0;
        }
        if (!*building_orientation) {
            *building_orientation = building_rotation_get_rotation() + 1;
            if (*building_orientation > 4) {
                *building_orientation = 1;
            }
        }
    }
    if (definition.roadblock().is_wall_gate()) {
        constexpr int gatehouse_disallowed_terrain =
            ~TERRAIN_WALL & ~TERRAIN_ROAD & ~TERRAIN_HIGHWAY & ~TERRAIN_BUILDING;
        if (!tiles_are_clear_or_force_clearable(x, y, size, gatehouse_disallowed_terrain, 1, force_check)) {
            clear_land_needed_warning().show_when(emit_warnings);
            return 0;
        }
        if (!check_gatehouse_tiles(grid_offset)) { //helper to make sure all building tiles are on walls
            clear_land_needed_warning().show_when(emit_warnings);
            return 0;
        }
        if (!*building_orientation) {
            if (building_rotation_get_road_orientation() == 1) {
                *building_orientation = 1;
            } else {
                *building_orientation = 2;
            }
        }
    }
    if (definition.roadblock().has_center_road_passage()) {
        if (!*building_orientation) {
            if (building_rotation_get_road_orientation() == 1) {
                *building_orientation = 1;
            } else {
                *building_orientation = 2;
            }
        }
    }
    if (!placement.can_place()) {
        if (placement.has_open_water_failure()) {
            dock_open_water_needed_warning().show_when(emit_warnings);
        } else if (placement.has_shoreline_failure()) {
            shore_needed_warning().show_when(emit_warnings);
        } else {
            clear_land_needed_warning().show_when(emit_warnings);
        }
        return 0;
    }
    force_place_copy_plan_offsets(force_check, placement);

    PlaceWarningMessage terrain_warning;
    if (!terrain_requirement_allows_placement(x, y, &terrain_warning)) {
        terrain_warning.show_when(emit_warnings);
        return 0;
    }
    if (building_is_fort(type)) {
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
    if (!construction_rules_allow_placement(definition, emit_warnings)) {
        return 0;
    }

    if (building_monument_is_grand_temple(type) &&
        building_monument_count_grand_temples() >= config_get(CONFIG_GP_CH_MAX_GRAND_TEMPLES)) {
        max_grand_temples_warning().show_when(emit_warnings);
        return 0;
    }
    if (emit_warnings) {
        building_construction_warning_check_all(type, x, y, size);
    }
    return 1;
}

static int building_construction_place_building_internal(building_type type, int x, int y,
    int exact_coordinates, force_place_check *force_check, int emit_warnings, int place_building)
{
    const building_type_registry_impl::BuildingType &definition = *definition_for_placement_type(type);
    int building_orientation = initial_building_orientation(definition, x, y);
    building_construction::ConstructionPlacementPlan placement(
        definition,
        x,
        y,
        exact_coordinates,
        force_check && force_check->active);

    if (!building_construction_validate_placement_plan(
        placement, force_check, emit_warnings, &building_orientation)) {
        return 0;
    }

    if (!place_building) {
        return 1;
    }
    force_place_clear_offsets(force_check);


    // phew, checks done!
    building *b;
    b = building_create(type, placement.origin_x(), placement.origin_y());

    game_undo_add_building(b);
    if (b->id <= 0) {
        return 0;
    }
    add_to_map(type, b, building_orientation, placement);
    instant_building_remove_required_resources(type);
    map_water_supply_refresh_building(b);
    return 1;
}

int building_construction_place_building(building_type type, int x, int y, int exact_coordinates)
{
    return building_construction_place_building_internal(type, x, y, exact_coordinates, 0, 1, 1);
}

building_construction_assessment building_construction_assess_placement(
    const building_type_registry_impl::BuildingType &definition,
    int x,
    int y,
    int exact_coordinates,
    int force_place)
{
    force_place_check check = { force_place ? 1 : 0, 0, 0, 0, { 0 } };
    int building_orientation = initial_building_orientation(definition, x, y);
    building_construction::ConstructionPlacementPlan placement(
        definition,
        x,
        y,
        exact_coordinates,
        check.active);
    const int can_place = building_construction_validate_placement_plan(
        placement, &check, 0, &building_orientation);
    return {std::move(placement), can_place, can_place ? check.clear_cost : 0};
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
