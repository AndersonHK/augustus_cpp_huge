#include "city/trade_ledger.h"
#include "building/construction.h"
#include "building/construction_plan.h"
#include "building/construction_warning.h"
#include "building/count.h"
#include "building/distribution.h"
#include "building/industry.h"
#include "building/roadblock.h"
#include "building/rotation.h"
#include "building/storage.h"
#include "building/variant.h"
#include "city/warning.h"
#include "game/undo.h"
#include "map/aqueduct.h"
#include "map/building.h"
#include "map/building_tiles.h"
#include "map/orientation.h"
#include "map/tiles.h"
#include "map/water.h"
#include "map/water_navigation.h"
#include "building/water_access_runtime.h"

#include "construction_building.h"

#include "building/building.h"
#include "building/building_runtime_internal.h"
#include "building/building_type_registry_internal.h"
#include "building/religion.h"
#include "figure/formation_legion.h"

#include "assets/assets.h"
#include "building/building_record.h"
#include "building/menu.h"
#include "building/monument.h"
#include "building/properties.h"
#include "building/warehouse.h"
#include "city/buildings.h"
#include "city/monument_gifts.h"
#include "city/culture.h"
#include "city/finance.h"
#include "city/resource.h"
#include "core/config.h"
#include "core/image.h"
#include "core/log.h"
#include "core/random.h"
#include "empire/city.h"
#include "map/figure.h"
#include "map/grid.h"
#include "map/property.h"
#include "map/routing.h"
#include "figure/route.h"
#include "map/terrain.h"
#include "scenario/property.h"

#include <algorithm>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

// Repair and ordinary rubble clearing must quote the same per-tile price.
constexpr int RUBBLE_CLEAR_COST_PER_TILE = 3;

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

static building_type build_type_mess_hall()
{
    static building_type type = BUILDING_NONE;
    if (type == BUILDING_NONE) {
        type = building_type_registry_impl::type_from_attr("mess_hall");
    }
    return type;
}

static int is_statue_with_orientation_type(building_type type)
{
    return building_type_registry_impl::type_attr_is_any(type, {
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

static PlaceWarningMessage warning_text(warning_type type, translation_key key)
{
    return {type, localized_text(key)};
}

static PlaceWarningMessage warning_from_template(warning_type type, translation_key key, building_type building)
{
    std::string text = localized_text(key);
    replace_all(text, "<building-type>", building_construction_warning_type_name(building));
    return {type, text};
}

static PlaceWarningMessage warning_from_template(warning_type type, translation_key key, building_type building,
    resource_type resource)
{
    std::string text = localized_text(key);
    replace_all(text, "<building-type>", building_construction_warning_type_name(building));
    replace_all(text, "<resource>", building_construction_warning_resource_name(resource));
    return {type, text};
}

static PlaceWarningMessage warning_from_template(warning_type type, translation_key key, building_type building,
    building_type required_building)
{
    std::string text = localized_text(key);
    replace_all(text, "<building-type>", building_construction_warning_type_name(building));
    replace_all(text, "<required-building>", building_construction_warning_type_name(required_building));
    return {type, text};
}

static PlaceWarningMessage max_count_warning_from_template(warning_type type, translation_key key, building_type building,
    int max_count)
{
    std::string text = localized_text(key);
    replace_all(text, "<building-type>", building_construction_warning_type_name(building));
    replace_all(text, "<max-count>", std::to_string(max_count));
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

static PlaceWarningMessage max_building_count_warning(building_type building, int max_count)
{
    if (max_count == 1) {
        return one_building_of_type_warning(building);
    }
    return max_count_warning_from_template(WARNING_ONE_BUILDING_OF_TYPE, "TR_CITY_WARNING_TEMPLATE_MAX_BUILDING_COUNT_REACHED",
        building, max_count);
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
            total_cost += (cost == COST_FREE) ? 0 : RUBBLE_CLEAR_COST_PER_TILE;
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

static void add_building(building *b)
{
    building_runtime *runtime = building_runtime_impl::get_or_create_instance(b);
    if (!runtime) {
        return;
    }
    Building &building_obj = runtime->building;
    const building_type_registry_impl::BuildingType *definition = building_obj.type;
    if (definition && definition->has_construction() && !definition->has_phased_construction() &&
        building_monument_type_is_monument(b->type) &&
        !b->monument.phase) {
        b->monument.phase = MONUMENT_FINISHED;
    }
    building_obj.add_map_tiles();
    building_obj.refresh_graphic();
}

static void refresh_placed_tile_regions(const building_construction::ConstructionPlacementPlan &placement)
{
    for (const building_construction::ConstructionPlacementPart &part : placement.parts()) {
        if (!part.definition->has_tile()) {
            continue;
        }
        map_tiles_update_region_tile(part.x, part.y, part.x + part.width - 1, part.y + part.height - 1,
            part.definition->tile());
    }
}

static const building_construction::ConstructionPlacementPart *placement_owner_part(
    const building_construction::ConstructionPlacementPlan &placement)
{
    for (const building_construction::ConstructionPlacementPart &part : placement.parts()) {
        if (part.is_owner) {
            return &part;
        }
    }
    return nullptr;
}

static void initialize_composed_child(
    building *main_record,
    Building &child_object,
    const building_construction::ConstructionPlacementPart &part,
    unsigned char graphics_variant)
{
    building *child = const_cast<building *>(child_object.record());
    if (!main_record || !child) {
        return;
    }
    child->subtype.orientation = static_cast<short>(part.building_orientation);
    child->road_network_id = main_record->road_network_id;
    child->distance_from_entry = main_record->distance_from_entry;
    child->road_access_x = main_record->road_access_x;
    child->road_access_y = main_record->road_access_y;
    child->has_road_access = main_record->has_road_access;
    child->houses_covered = main_record->houses_covered;
    child->percentage_houses_covered = main_record->percentage_houses_covered;
    child->labor_access_score = main_record->labor_access_score;
    if (building_runtime *child_runtime = child_object.runtime_instance()) {
        child_runtime->set_graphics_variant(graphics_variant);
    }
}

static void abandon_unpublished_composition(
    building *main_record,
    const std::vector<Building *> &children)
{
    if (main_record) {
        if (building_runtime *runtime = building_runtime_impl::get_or_create_instance(main_record);
            runtime && runtime->building.Composition) {
            runtime->building.Composition->clear();
        }
    }
    if (main_record) {
        main_record->state = BUILDING_STATE_DELETED_BY_GAME;
    }
    for (Building *child_object : children) {
        building *child = child_object ? const_cast<building *>(child_object->record()) : nullptr;
        if (!child) {
            continue;
        }
        child->state = BUILDING_STATE_DELETED_BY_GAME;
    }
}

static bool add_native_composed_building(
    building *main_record,
    const building_type_registry_impl::BuildingType &definition,
    const building_construction::ConstructionPlacementPart &main_part,
    const building_construction::ConstructionPlacementPlan &placement)
{
    building_runtime *main_runtime = building_runtime_impl::get_or_create_instance(main_record);
    Building *main_object = main_runtime ? &main_runtime->building : nullptr;
    BuildingComposition *composition = main_object ? main_object->Composition : nullptr;
    const std::vector<building_type_registry_impl::CompositionChildDef> &child_definitions =
        definition.composition().children();

    if (!placement.can_place() || placement.owner_charge_count() != 1 || !main_part.is_owner) {
        log_error("Native composition construction received an invalid placement transaction",
            definition.attr(), 0);
        abandon_unpublished_composition(main_record, {});
        return false;
    }

    std::vector<const building_construction::ConstructionPlacementPart *> child_parts;
    child_parts.reserve(child_definitions.size());
    for (const building_construction::ConstructionPlacementPart &part : placement.parts()) {
        if (!part.is_owner) {
            child_parts.push_back(&part);
        }
    }
    if (!composition || !composition->is_owner() || child_parts.size() != child_definitions.size()) {
        log_error("Native composition construction has an incomplete placement layout", definition.attr(), 0);
        abandon_unpublished_composition(main_record, {});
        return false;
    }
    for (std::size_t index = 0; index < child_parts.size(); ++index) {
        if (!child_parts[index] || child_parts[index]->definition != child_definitions[index].type) {
            log_error("Native composition placement order does not match CompositionDef", definition.attr(), 0);
            abandon_unpublished_composition(main_record, {});
            return false;
        }
    }

    if (main_record->x != main_part.x || main_record->y != main_part.y) {
        main_record->x = static_cast<unsigned char>(main_part.x);
        main_record->y = static_cast<unsigned char>(main_part.y);
        main_record->grid_offset = static_cast<short>(map_grid_offset(main_record->x, main_record->y));
    }

    std::vector<Building *> child_objects;
    std::vector<BuildingComposition *> child_modules;
    child_objects.reserve(child_parts.size());
    child_modules.reserve(child_parts.size());
    for (const building_construction::ConstructionPlacementPart *part : child_parts) {
        Building &child_object = city_building_runtime().create(*part->definition, part->x, part->y);
        initialize_composed_child(main_record, child_object, *part, main_runtime->graphics_variant());
        child_objects.push_back(&child_object);
        if (!child_object.Composition) {
            log_error("Native composition child has no BuildingComposition module", definition.attr(), 0);
            abandon_unpublished_composition(main_record, child_objects);
            return false;
        }
        child_modules.push_back(child_object.Composition);
    }

    std::string relationship_error;
    if (!composition->attach_children(child_modules, &relationship_error)) {
        log_error("Unable to bind native building composition", relationship_error.c_str(), 0);
        abandon_unpublished_composition(main_record, child_objects);
        return false;
    }
    if (!composition->complete(&relationship_error)) {
        log_error("Native building composition is incomplete before map publication",
            relationship_error.c_str(), 0);
        abandon_unpublished_composition(main_record, child_objects);
        return false;
    }

    // Creation and relationship binding are complete before the first map cell
    // is published. Layout validation guarantees member Foundations do not overlap.
    add_building(main_record);
    for (Building *child_object : child_objects) {
        building *child = const_cast<building *>(child_object->record());
        game_undo_add_building(child);
        add_building(child);
    }
    return true;
}

static bool add_composed_building(
    building *main_record,
    const building_construction::ConstructionPlacementPlan &placement)
{
    if (!main_record) {
        return false;
    }
    const building_construction::ConstructionPlacementPart *main_part =
        placement_owner_part(placement);
    const building_type_registry_impl::BuildingType *definition =
        main_part ? main_part->definition : building_type_registry_impl::definition_for_type(main_record->type);
    if (!definition) {
        abandon_unpublished_composition(main_record, {});
        return false;
    }
    if (!main_part || main_part->type != main_record->type) {
        log_error("Composition placement has no owner part", definition->attr(), 0);
        abandon_unpublished_composition(main_record, {});
        return false;
    }
    return add_native_composed_building(main_record, *definition, *main_part, placement);
}

static void set_monument_phase_for_parts(building *main_record, int phase)
{
    if (building_runtime *runtime = building_runtime_impl::get_or_create_instance(main_record)) {
        BuildingComposition *composition = runtime->building.Composition;
        if (!composition) {
            return;
        }
        composition->for_each_member([phase](Building &part) {
            building_monument_set_phase(const_cast<building *>(part.record()), phase);
        });
    }
}

static void assign_fort_formation_to_parts(building *main_record)
{
    building_runtime *runtime = building_runtime_impl::get_or_create_instance(main_record);
    if (!runtime) {
        return;
    }
    Building &fort_object = runtime->building;
    const int formation_id = formation_legion_create_for_fort(fort_object);
    if (!fort_object.Composition || !fort_object.Composition->is_owner() ||
        !fort_object.Composition->complete()) {
        log_error("Fort is missing its native BuildingComposition", fort_object.type->attr(), fort_object.id);
        return;
    }
    fort_object.Composition->for_each_member([formation_id](Building &part) {
        part.set_formation_id(formation_id);
    });
}

static void add_depot(building *b)
{
    b->data.depot.current_order.condition.condition_type = ORDER_CONDITION_ALWAYS;
    add_building(b);
}

static void add_granary(building *b)
{
    if (building_runtime *runtime = building_runtime_impl::get_or_create_instance(b)) {
        runtime->building.set_storage_id(building_storage_create(b->id));
    }
    add_building(b);
    map_update_building_internal_roads(b);
    map_tiles_update_area_roads(b->x, b->y, 5);
}

static bool add_to_map(
    building_type type,
    building *b,
    const building_construction::ConstructionPlacementPlan &placement)
{
    building_runtime *runtime = building_runtime_impl::get_or_create_instance(b);
    if (!runtime) {
        return false;
    }
    Building &building_obj = runtime->building;
    const building_type_registry_impl::BuildingType &definition = *building_obj.type;
    const building_construction::ConstructionPlacementPart *owner_part =
        placement_owner_part(placement);
    if (!owner_part || owner_part->definition != &definition) {
        log_error("Building construction placement has no matching owner part", definition.attr(), 0);
        return false;
    }
    if (definition.has_rotated_placement_geometry() && !building_is_fort(type)) {
        b->subtype.orientation = static_cast<short>(owner_part->building_orientation);
    }
    if (definition.attr_is("dock")) {
        b->data.dock.orientation = static_cast<signed char>(owner_part->foundation_rotation);
    }
    if (building_variant_has_variants(b->type)) {
        if (runtime) {
            runtime->set_graphics_variant(
                building_rotation_get_rotation_with_limit(building_variant_get_number_of_variants(b->type)));
        }
    }
    // Native graphics options share the saved variant byte, so XML-owned
    // buildings seed it after legacy variant/rotation setup has run.
    if (runtime) {
        runtime->assign_graphic_variant(1);
    }
    if (definition.is_temple(GOD_VENUS, building_type_registry_impl::ReligionTier::Small) ||
        definition.is_temple(GOD_VENUS, building_type_registry_impl::ReligionTier::Large)) {
        if (const building_type_registry_impl::Distribution *distribution = definition.distribution()) {
            distribution->set_acceptance(building_obj, false);
        }
    }
    if (definition.has_composition()) {
        if (definition.is_warehouse()) {
            building_obj.set_storage_id(building_storage_create(b->id));
        } else if (building_is_fort(type)) {
            b->subtype.fort_figure_type =
                static_cast<short>(building_count_forts_get_figure_type_from_building(type));
        }
        if (!add_composed_building(b, placement)) {
            if (b->storage_id) {
                building_storage_delete(b->storage_id);
                b->storage_id = 0;
            }
            return false;
        }
        if (definition.has_phased_construction()) {
            int road_update_radius = definition.construction().road_update_radius();
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
            if (!map_has_road_access_building(b->x, b->y, 0)) {
                warehouse_tower_road_access_warning().show();
            }
        } else if (building_is_fort(type)) {
            assign_fort_formation_to_parts(b);
        }
    } else if (definition.has_phased_construction()) {
        add_building(b);
        int road_update_radius = definition.construction().road_update_radius();
        if (road_update_radius > 0) {
            map_tiles_update_area_roads(b->x, b->y, road_update_radius);
        }
        if (definition.is_temple(GOD_MARS, building_type_registry_impl::ReligionTier::Grand)) {
            b->accepted_goods[resource_weapons()] = 1;
            b->accepted_goods[RESOURCE_NONE] = 1;
        }
        building_monument_set_phase(b, MONUMENT_START);
    } else if (definition.is_farm()) {
        add_building(b);
    } else if (definition.is_granary()) {
        add_granary(b);
    } else if (definition.is_temple(GOD_VENUS, building_type_registry_impl::ReligionTier::Small)) {
        add_building(b);
        if (const building_type_registry_impl::Distribution *distribution = definition.distribution()) {
            distribution->set_acceptance(building_obj, false);
        }
    } else if (definition.attr_is("large_mausoleum") || definition.attr_is("nymphaeum")) {
        add_building(b);
        map_tiles_update_area_roads(b->x, b->y, 5);
        building_monument_set_phase(b, MONUMENT_START);
    } else if (definition.attr_is("gatehouse")) {
        add_building(b);
        map_orientation_update_buildings();
        map_tiles_update_area_roads(b->x, b->y, 5);
        map_tiles_update_area_highways(b->x, b->y, 3);
        map_tiles_update_all_plazas();
        map_tiles_update_area_walls(b->x, b->y, 5);
    } else if (definition.attr_is("triumphal_arch")) {
        add_building(b);
        map_orientation_update_buildings();
        map_tiles_update_area_roads(b->x, b->y, 5);
        map_tiles_update_all_plazas();
        if (definition.has_phased_construction()) building_monument_set_phase(b, MONUMENT_START);
        building_menu_update();
        building_construction_clear_type();
    } else if (definition.is_mess_hall()) {
        b->data.market.is_mess_hall = 1;
        add_building(b);
    } else if (is_statue_with_orientation_type(type)) {
        b->subtype.orientation = static_cast<short>(placement.rotation());
        add_building(b);
    } else if (definition.attr_is("small_mausoleum")) {
        b->subtype.orientation = static_cast<short>(placement.rotation());
        map_tiles_update_area_roads(b->x, b->y, 4);
        building_monument_set_phase(b, MONUMENT_START);
    } else if (definition.attr_is("cart_depot")) {
        add_depot(b);
    } else if (definition.is_temple(std::nullopt, building_type_registry_impl::ReligionTier::Shrine)) {
        b->subtype.orientation = static_cast<short>(placement.rotation());
        add_building(b);
    } else if (definition.attr_is("barracks")) {
        b->accepted_goods[resource_weapons()] = 1;
        b->accepted_goods[RESOURCE_NONE] = 1;
        add_building(b);
    } else {
        add_building(b);
    }
    refresh_placed_tile_regions(placement);
    Route::updateLandTerrain();
    Route::updateWallTerrain();
    return true;
}

int building_construction_fill_vacant_lots(grid_slice *area)
{
    int items_placed = 0;
    building_type vacant_lot_type = building_type_registry_impl::vacant_lot_fill_type();
    for (int i = 0; i < area->size; i++) {
        int grid_offset = area->grid_offsets[i];
        int x = map_grid_offset_to_x(grid_offset);
        int y = map_grid_offset_to_y(grid_offset);
        int success = building_construction_place_building(vacant_lot_type, x, y, 1);
        if (!success) {
            continue;
        }
        building *b = const_cast<building *>(map_building_at(grid_offset).record());
        game_undo_add_building(b);
        items_placed++;
    }
    if (items_placed > 0) {
        building_construction_warning_check_food_stocks(vacant_lot_type);
        Route::updateLandTerrain();
    }
    return items_placed;
}

enum {
    FORCE_PLACE_CLEARABLE_TERRAIN = TERRAIN_TREE | TERRAIN_SHRUB | TERRAIN_ROAD
};

struct force_place_check {
    int active = 0;
    int perform_clear = 0;
    int clear_cost = 0;
    std::vector<int> clear_offsets;
};

static int instant_building_has_required_resources(
    const building_type_registry_impl::BuildingType &definition,
    int emit_warnings)
{
    for (resource_type resource = (RESOURCE_NONE + 1); resource < RESOURCE_SLOT_COUNT; resource = static_cast<resource_type>(resource + 1)) {
        int amount = definition.construction().instant_requirement_amount(resource);
        if (amount > 0 && city_resource_count_warehouses_amount(resource) < amount) {
            building_needs_resource_warning(definition.type(), resource).show_when(emit_warnings);
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
    if (!city_monument_gift_available(type)) return 0;
    const int max_count = construction.max_count();
    if (max_count > 0 &&
        !construction_config_flag_enabled(construction.max_count_unless_config()) &&
        building_count_total(type) >= max_count) {
        max_building_count_warning(type, max_count).show_when(emit_warnings);
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

static int building_construction_global_rules_allow_placement(
    const building_type_registry_impl::BuildingType &definition,
    int emit_warnings)
{
    const building_type type = definition.type();
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
    if (!instant_building_has_required_resources(definition, emit_warnings)) {
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
    const building_type_registry_impl::BuildingType *definition =
        building_type_registry_impl::definition_for_type(type);
    if (!definition) {
        return;
    }

    for (resource_type resource = (RESOURCE_NONE + 1); resource < RESOURCE_SLOT_COUNT; resource = static_cast<resource_type>(resource + 1)) {
        int amount = definition->construction().instant_requirement_amount(resource);
        if (amount > 0) {
            const int remaining = building_warehouses_remove_resource(resource, amount);
            city_trade_ledger_consumed(resource, (amount - remaining) * resource_units_per_load());
        }
    }
}

static void force_place_copy_plan_offsets(
    force_place_check *check,
    const building_construction::ConstructionPlacementPlan &plan)
{
    if (!check || !check->active) {
        return;
    }
    check->clear_offsets = plan.clear_offsets();
    check->clear_cost = plan.clear_cost();
}

static void force_place_clear_offsets(force_place_check *check)
{
    if (!check || !check->active || !check->perform_clear || check->clear_offsets.empty()) {
        return;
    }

    int x_min = map_grid_offset_to_x(check->clear_offsets.front());
    int x_max = x_min;
    int y_min = map_grid_offset_to_y(check->clear_offsets.front());
    int y_max = y_min;

    for (int grid_offset : check->clear_offsets) {
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

static int building_construction_validate_local_placement_plan(
    const building_construction::ConstructionPlacementPlan &placement,
    force_place_check *force_check,
    int emit_warnings)
{
    const building_type type = placement.type();
    const int x = placement.origin_x();
    const int y = placement.origin_y();
    const int size = placement.placement_size();
    if (!placement.can_place()) {
        if (placement.failure_reason() == building_construction::PlacementFailureReason::Proximity) {
            warning_text(WARNING_CLEAR_LAND_NEEDED, "TR_CITY_WARNING_FOUNDATION_PROXIMITY").show_when(emit_warnings);
        } else if (placement.has_open_water_failure()) {
            dock_open_water_needed_warning().show_when(emit_warnings);
        } else {
            clear_land_needed_warning().show_when(emit_warnings);
        }
        return 0;
    }
    force_place_copy_plan_offsets(force_check, placement);
    for (int slot = RESOURCE_NONE + 1; slot < RESOURCE_SLOT_COUNT; ++slot) {
        const auto resource = static_cast<resource_type>(slot);
        const int amount = placement.support_resource_amount(resource) + placement.definition().construction().instant_requirement_amount(resource);
        if (amount > 0 && city_resource_count_warehouses_amount(resource) < amount) {
            building_needs_resource_warning(type, resource).show_when(emit_warnings);
            return 0;
        }
    }

    PlaceWarningMessage terrain_warning;
    if (!terrain_requirement_allows_placement(x, y, &terrain_warning)) {
        terrain_warning.show_when(emit_warnings);
        return 0;
    }
    if (emit_warnings) {
        building_construction_warning_check_all(type, x, y, size);
    }
    return 1;
}

static int building_construction_validate_placement_plan(
    const building_construction::ConstructionPlacementPlan &placement,
    force_place_check *force_check,
    int emit_warnings)
{
    if (!building_construction_global_rules_allow_placement(placement.definition(), emit_warnings)) {
        return 0;
    }
    return building_construction_validate_local_placement_plan(
        placement,
        force_check,
        emit_warnings);
}

class PlacementSupersession {
public:
    PlacementSupersession() = default;

    explicit PlacementSupersession(
        const building_construction::ConstructionPlacementPlan &placement)
    {
        stage(placement);
    }

    void stage(const building_construction::ConstructionPlacementPlan &placement)
    {
        for (const building_construction::ConstructionPlacementSupersession &supersession :
                placement.supersessions()) {
            const unsigned int removed = static_cast<unsigned int>(map_terrain_get(supersession.grid_offset)) &
                supersession.generated_terrain;
            if (removed) {
                terrain_.push_back(Terrain{
                    supersession.grid_offset,
                    removed,
                    supersession.replacement_terrain
                });
                map_terrain_remove(supersession.grid_offset, static_cast<int>(removed));
            }
            if (!supersession.building_id) {
                continue;
            }
            const auto existing = std::find_if(records_.begin(), records_.end(),
                [&supersession](const Record &saved) {
                    return saved.id == supersession.building_id;
                });
            if (existing != records_.end()) {
                continue;
            }
            Building *replaced = Building::get(supersession.building_id);
            building *record = replaced ? const_cast<::building *>(replaced->record()) : nullptr;
            if (!record) {
                continue;
            }
            records_.push_back(Record{
                supersession.building_id,
                record->state,
                record->is_deleted,
                *record
            });
            if (replaced->Foundation) {
                replaced->Foundation->detach_unbound_ownership();
            }
            record->state = BUILDING_STATE_DELETED_BY_PLAYER;
            record->is_deleted = 1;
            const building_type_registry_impl::FoundationDef *foundation =
                replaced->type ? replaced->type->foundation_def() : nullptr;
            if (foundation) {
                const int rotation = foundation->rotates() ? replaced->orientation() : 0;
                for (const building_type_registry_impl::RotatedFoundationCell &cell :
                        foundation->rotated_cells(rotation)) {
                    const int x = record->x + cell.x;
                    const int y = record->y + cell.y;
                    if (!map_grid_is_inside(x, y, 1)) {
                        continue;
                    }
                    const int grid_offset = map_grid_offset(x, y);
                    if (map_building_exists_at(grid_offset) &&
                        map_building_at(grid_offset).record() == record) {
                        bindings_.push_back(Binding{ grid_offset, supersession.building_id });
                        map_building_clear_at(grid_offset);
                    }
                }
            }
        }
        active_ = !terrain_.empty() || !records_.empty();
    }

    ~PlacementSupersession()
    {
        if (!active_) {
            return;
        }
        for (const Record &saved : records_) {
            Building *surface = Building::get(saved.id);
            building *record = surface ? const_cast<::building *>(surface->record()) : nullptr;
            if (record) {
                record->state = saved.state;
                record->is_deleted = saved.is_deleted;
                if (surface->Foundation) {
                    surface->Foundation->restore_unbound_ownership();
                }
            }
        }
        for (const Binding &saved : bindings_) {
            if (Building *surface = Building::get(saved.id)) {
                map_building_set(saved.grid_offset, *surface);
            }
        }
        for (const Terrain &saved : terrain_) {
            map_terrain_add(saved.grid_offset, static_cast<int>(saved.removed_terrain));
        }
    }

    void commit()
    {
        for (Record &saved : records_) {
            game_undo_add_replaced_building(&saved.undo_snapshot);
        }
        for (const Terrain &saved : terrain_) {
            if ((saved.removed_terrain & TERRAIN_AQUEDUCT) &&
                !(saved.replacement_terrain & TERRAIN_AQUEDUCT)) {
                map_aqueduct_remove(saved.grid_offset);
            }
        }
        active_ = false;
    }

private:
    struct Record {
        unsigned int id;
        unsigned char state;
        unsigned char is_deleted;
        building undo_snapshot;
    };
    struct Binding {
        int grid_offset;
        unsigned int id;
    };
    struct Terrain {
        int grid_offset;
        unsigned int removed_terrain;
        unsigned int replacement_terrain;
    };

    std::vector<Record> records_;
    std::vector<Binding> bindings_;
    std::vector<Terrain> terrain_;
    bool active_ = false;
};

static int building_construction_place_building_internal(building_type type, int x, int y,
    int exact_coordinates, force_place_check *force_check, int emit_warnings, int place_building)
{
    const building_type_registry_impl::BuildingType &definition =
        *building_type_registry_impl::definition_for_type(type);
    building_construction::ConstructionPlacementPlan placement(
        definition,
        x,
        y,
        exact_coordinates,
        force_check && force_check->active);

    if (!building_construction_validate_placement_plan(placement, force_check, emit_warnings)) {
        return 0;
    }

    if (!place_building) {
        return 1;
    }
    force_place_clear_offsets(force_check);

    // Validation records every definition-authorized replacement. Remove its
    // owned terrain just long enough for the new FoundationState to acquire
    // the same bits, restoring the old record and terrain if publication fails.
    PlacementSupersession supersession(placement);

    // phew, checks done!
    Building &building_obj = city_building_runtime().create(
        definition,
        placement.origin_x(),
        placement.origin_y());
    building *b = const_cast<building *>(building_obj.record());

    if (!add_to_map(type, b, placement)) {
        if (b) {
            b->state = BUILDING_STATE_DELETED_BY_GAME;
            b->is_deleted = 1;
        }
        return 0;
    }
    if (building_obj.Foundation && !building_obj.Foundation->state().is_published()) {
        b->state = BUILDING_STATE_DELETED_BY_GAME;
        b->is_deleted = 1;
        return 0;
    }
    supersession.commit();
    game_undo_add_building(b);
    instant_building_remove_required_resources(type);
    for (int slot = RESOURCE_NONE + 1; slot < RESOURCE_SLOT_COUNT; ++slot) {
        const auto resource = static_cast<resource_type>(slot);
        const int amount = placement.support_resource_amount(resource);
        if (amount > 0) {
            const int remaining = building_warehouses_remove_resource(resource, amount);
            game_undo_add_resource_cost(resource, amount - remaining);
            city_trade_ledger_consumed(resource, (amount - remaining) * resource_units_per_load());
        }
    }
    water_access_runtime_refresh_building(&building_obj);
    if (definition.attr_is("dock")) {
        water_navigation::invalidate_dock_endpoints();
    }
    return 1;
}

int building_construction_place_building(building_type type, int x, int y, int exact_coordinates)
{
    return building_construction_place_building_internal(type, x, y, exact_coordinates, 0, 1, 1);
}

int building_construction_publish_placement_batch(
    const std::vector<building_construction::ConstructionPlacementPlan> &placements)
{
    if (placements.empty()) {
        return 1;
    }
    PlacementSupersession supersession;
    for (const building_construction::ConstructionPlacementPlan &placement : placements) {
        if (!placement.can_place()) {
            return 0;
        }
        supersession.stage(placement);
    }

    std::vector<Building *> published;
    published.reserve(placements.size());
    for (const building_construction::ConstructionPlacementPlan &placement : placements) {
        Building &building_object = city_building_runtime().create(
            placement.definition(), placement.origin_x(), placement.origin_y());
        building *record = const_cast<::building *>(building_object.record());
        if (!record || !add_to_map(placement.type(), record, placement) ||
            (building_object.Foundation && !building_object.Foundation->state().is_published())) {
            if (record) {
                record->state = BUILDING_STATE_DELETED_BY_GAME;
                record->is_deleted = 1;
            }
            for (auto it = published.rbegin(); it != published.rend(); ++it) {
                Building *created = *it;
                building *created_record = created ? const_cast<::building *>(created->record()) : nullptr;
                if (created && created->Foundation) {
                    created->Foundation->remove();
                }
                if (created_record) {
                    created_record->state = BUILDING_STATE_DELETED_BY_GAME;
                    created_record->is_deleted = 1;
                }
            }
            return 0;
        }
        published.push_back(&building_object);
    }

    supersession.commit();
    for (Building *created : published) {
        building *record = created ? const_cast<::building *>(created->record()) : nullptr;
        if (record) {
            game_undo_add_created_building(record);
        }
        if (created && created->matches("dock")) {
            water_navigation::invalidate_dock_endpoints();
        }
    }
    return 1;
}

building_construction_assessment building_construction_assess_repair(
    const building_type_registry_impl::BuildingType &definition,
    const RubbleState &origin)
{
    force_place_check check;
    const int origin_x = map_grid_offset_to_x(origin.original_grid_offset);
    const int origin_y = map_grid_offset_to_y(origin.original_grid_offset);
    building_construction::ConstructionPlacementPlan placement(
        definition,
        origin_x,
        origin_y,
        1,
        0,
        origin.original_orientation,
        &origin);
    const int global_can_place = building_construction_global_rules_allow_placement(definition, 0);
    const int local_can_place = building_construction_validate_local_placement_plan(placement, &check, 0);
    const int can_place = global_can_place && local_can_place;
    const int clear_cost =
        placement.replaceable_rubble_tiles() * RUBBLE_CLEAR_COST_PER_TILE;
    return {std::move(placement), can_place, !global_can_place, clear_cost};
}

const building_type_registry_impl::BuildingType *building_construction_repair_replacement_type(
    const building_type_registry_impl::BuildingType &original_type)
{
    if (!original_type.has_housing()) {
        return &original_type;
    }
    return building_type_registry_impl::definition_for_type(
        building_type_registry_impl::vacant_lot_fill_type());
}

static Building *publish_repaired_plan(
    const building_type_registry_impl::BuildingType &definition,
    const building_construction::ConstructionPlacementPlan &placement)
{
    Building &building_object = city_building_runtime().create(
        definition,
        placement.origin_x(),
        placement.origin_y());
    ::building *record = const_cast<::building *>(building_object.record());
    if (!add_to_map(definition.type(), record, placement)) {
        if (record) {
            record->state = BUILDING_STATE_DELETED_BY_GAME;
        }
        return nullptr;
    }
    bool fully_published = true;
    if (building_object.Composition && building_object.Composition->is_owner()) {
        building_object.Composition->for_each_member([&fully_published](Building &member) {
            if (!member.Foundation || !member.Foundation->state().is_published()) {
                fully_published = false;
            }
        });
    } else if (!building_object.Foundation || !building_object.Foundation->state().is_published()) {
        fully_published = false;
    }
    if (!fully_published) {
        std::vector<Building *> members;
        if (building_object.Composition && building_object.Composition->is_owner()) {
            building_object.Composition->for_each_member([&members](Building &member) {
                members.push_back(&member);
            });
        } else {
            members.push_back(&building_object);
        }
        for (auto it = members.rbegin(); it != members.rend(); ++it) {
            Building *member = *it;
            building *member_record = const_cast<::building *>(member->record());
            city_culture_remove_building_module_capacity(member_record);
            member->remove_map_tiles();
            building_clear_related_data(member_record);
            member_record->state = BUILDING_STATE_DELETED_BY_GAME;
        }
        if (building_object.Composition) {
            building_object.Composition->clear();
        }
        return nullptr;
    }
    return &building_object;
}

static void finalize_repaired_plan(
    Building &building_object,
    const building_type_registry_impl::BuildingType &definition)
{
    game_undo_add_building(const_cast<::building *>(building_object.record()));
    instant_building_remove_required_resources(definition.type());
    water_access_runtime_refresh_building(&building_object);
}

static void rollback_repaired_plans(const std::vector<Building *> &placed)
{
    for (auto it = placed.rbegin(); it != placed.rend(); ++it) {
        Building *building_object = *it;
        building *record = building_object ? const_cast<::building *>(building_object->record()) : nullptr;
        if (!record) {
            continue;
        }
        city_culture_remove_building_module_capacity(record);
        building_object->remove_map_tiles();
        building_clear_related_data(record);
        record->state = BUILDING_STATE_DELETED_BY_GAME;
    }
}

static Building *place_repaired_housing(
    const building_construction::ConstructionPlacementPlan &original_placement,
    const RubbleState &origin)
{
    const building_type_registry_impl::BuildingType *vacant_lot =
        building_construction_repair_replacement_type(original_placement.definition());
    if (!vacant_lot) {
        return nullptr;
    }

    std::vector<building_construction::ConstructionPlacementPlan> lots;
    for (const building_construction::ConstructionPlacementPart &part : original_placement.parts()) {
        for (const building_construction::ConstructionPlacementTile &tile : part.tiles) {
            lots.emplace_back(*vacant_lot, tile.x, tile.y, 1, 0, 0, &origin);
            if (!lots.back().can_place()) {
                return nullptr;
            }
        }
    }

    std::vector<Building *> placed_lots;
    placed_lots.reserve(lots.size());
    for (const building_construction::ConstructionPlacementPlan &lot : lots) {
        Building *placed = publish_repaired_plan(*vacant_lot, lot);
        if (!placed) {
            rollback_repaired_plans(placed_lots);
            return nullptr;
        }
        placed_lots.push_back(placed);
    }
    for (Building *placed : placed_lots) {
        finalize_repaired_plan(*placed, *vacant_lot);
    }
    return placed_lots.empty() ? nullptr : placed_lots.front();
}

Building *building_construction_place_repaired_building(
    const building_construction_assessment &assessment,
    const RubbleState &origin)
{
    if (!assessment.can_place) {
        return nullptr;
    }

    const building_construction::ConstructionPlacementPlan &placement = assessment.placement;
    const building_type_registry_impl::BuildingType &definition = placement.definition();
    if (definition.has_housing()) {
        return place_repaired_housing(placement, origin);
    }
    Building *repaired = publish_repaired_plan(definition, placement);
    if (repaired) {
        finalize_repaired_plan(*repaired, definition);
    }
    return repaired;
}

building_construction_assessment building_construction_assess_placement(
    const building_type_registry_impl::BuildingType &definition,
    int x,
    int y,
    int exact_coordinates,
    int force_place)
{
    force_place_check check;
    check.active = force_place ? 1 : 0;
    building_construction::ConstructionPlacementPlan placement(
        definition,
        x,
        y,
        exact_coordinates,
        check.active);
    const int global_can_place = building_construction_global_rules_allow_placement(definition, 0);
    const int local_can_place = building_construction_validate_local_placement_plan(placement, &check, 0);
    const int can_place = global_can_place ? local_can_place : 0;
    return {std::move(placement), can_place, !global_can_place, can_place ? check.clear_cost : 0};
}

int building_construction_show_placement_warning(
    const building_type_registry_impl::BuildingType &definition,
    int x,
    int y,
    int exact_coordinates,
    int force_place)
{
    force_place_check check;
    check.active = force_place ? 1 : 0;
    building_construction::ConstructionPlacementPlan placement(
        definition,
        x,
        y,
        exact_coordinates,
        check.active);
    return building_construction_validate_placement_plan(placement, &check, 1);
}

int building_construction_force_place_building(building_type type, int x, int y, int exact_coordinates, int *clear_cost)
{
    force_place_check check;
    check.active = 1;
    check.perform_clear = 1;
    int success = building_construction_place_building_internal(type, x, y, exact_coordinates, &check, 1, 1);
    if (clear_cost) {
        *clear_cost = success ? check.clear_cost : 0;
    }
    return success;
}
