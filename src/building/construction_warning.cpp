#include "building/count.h"
#include "map/road_access.h"

#include "construction_warning.h"

#include "building/building.h"
#include "building/building_record.h"
#include "building/building_type_registry_internal.h"
#include "building/industry.h"
#include "building/monument.h"
#include "building/properties.h"
#include "building/religion.h"
#include "building/water_access_runtime.h"
#include "city/labor.h"
#include "city/population.h"
#include "city/resource.h"
#include "city/constants.h"
#include "city/warning.h"
#include "map/grid.h"

#include "core/calc.h"
#include "empire/city.h"
#include "map/terrain.h"
#include "scenario/property.h"

#include <initializer_list>
#include <string>
#include <string_view>

static int has_warning = 0;

static int is_vacant_lot_fill_type(building_type type)
{
    building_type vacant_lot = building_type_registry_impl::vacant_lot_fill_type();
    return vacant_lot != BUILDING_NONE && type == vacant_lot;
}

static int is_shrine_tier(building_type type)
{
    const building_type_registry_impl::BuildingType *definition = building_type_registry_impl::definition_for_type(type);
    return definition && definition->is_temple(std::nullopt, building_type_registry_impl::ReligionTier::Shrine);
}

static int active_count(std::string_view attr)
{
    building_type type = building_type_registry_impl::type_from_attr(attr);
    return type != BUILDING_NONE ? building_count_active(type) : 0;
}

void building_construction_warning_reset(void)
{
    has_warning = 0;
}

static std::string legacy_text(const uint8_t *text)
{
    return text ? reinterpret_cast<const char *>(text) : "";
}

static void replace_all(std::string &text, std::string_view placeholder, std::string_view replacement)
{
    size_t position = 0;
    while ((position = text.find(placeholder, position)) != std::string::npos) {
        text.replace(position, placeholder.size(), replacement);
        position += replacement.size();
    }
}

static void show(warning_type warning, translation_key key)
{
    city_warning_show(warning, translation_for(key));
    has_warning = 1;
}

void building_construction_warning_show_missing_resource(resource_type resource)
{
    std::string text = legacy_text(translation_for_key("TR_CITY_WARNING_TEMPLATE_MISSING_RESOURCE"));
    replace_all(text, "<resource>", building_construction_warning_resource_name(resource));
    city_warning_show(WARNING_MISSING_RESOURCE, reinterpret_cast<const uint8_t *>(text.c_str()));
    has_warning = 1;
}

std::string building_construction_warning_type_name(building_type type)
{
    const building_type_registry_impl::BuildingType *definition = building_type_registry_impl::definition_for_type(type);
    const char *name_key = definition && definition->has_identity() ? definition->identity().name_key() : nullptr;
    if (name_key) {
        std::string name = legacy_text(translation_for_key(name_key));
        if (!name.empty()) {
            return name;
        }
    }
    const char *button_key = definition ? definition->button_text_key() : nullptr;
    if (button_key) {
        std::string name = legacy_text(translation_for_key(button_key));
        if (!name.empty()) {
            return name;
        }
    }
    return "";
}

std::string building_construction_warning_resource_name(resource_type resource)
{
    resource_data *data = resource_get_data(resource);
    return data ? legacy_text(data->text) : "";
}

static void show_missing_producer(building_type producer)
{
    std::string text = legacy_text(translation_for_key("TR_CITY_WARNING_TEMPLATE_MISSING_PRODUCER"));
    replace_all(text, "<building-type>", building_construction_warning_type_name(producer));
    city_warning_show(WARNING_MISSING_PRODUCER, reinterpret_cast<const uint8_t *>(text.c_str()));
    has_warning = 1;
}

static void check_road_access(building_type type, int x, int y, int size)
{
    if (is_vacant_lot_fill_type(type) || building_type_registry_impl::type_attr_is_any(type, {
        "well",
        "large_statue",
        "fountain",
        "reservoir",
        "roadblock",
        "small_statue",
        "goddess_statue",
        "senator_statue",
        "medium_statue",
        "small_pond",
        "large_pond",
        "pine_tree",
        "fir_tree",
        "oak_tree",
        "elm_tree",
        "fig_tree",
        "plum_tree",
        "palm_tree",
        "date_tree",
        "pavilion",
        "obelisk",
        "gatehouse",
        "triumphal_arch",
        "fort_legionaries",
        "fort_javelin",
        "fort_mounted",
        "fort_swords",
        "fort_archers",
        "horse_statue",
        "dolphin_fountain",
        "hedge_dark",
        "hedge_light",
        "looped_garden_wall",
        "legion_statue",
        "decorative_column",
        "gladiator_statue",
        "latrines",
    })) {
        return;
    }

    int has_road = 0;
    if (map_has_road_access(x, y, size, 0)) {
        has_road = 1;
    } else if (building_type_registry_impl::type_attr_is(type, "granary") && map_has_road_access_granary(x, y, 0)) {
        has_road = 1;
    } else if (building_type_registry_impl::type_attr_is(type, "warehouse")) {
        // Warehouse road access is checked after composed placement moves the tower to its XML offset.
        //TODO: a dedicated function similar as for hippodrome should be used for consistency
        has_road = 1;
    } else if (building_type_registry_impl::type_attr_is(type, "hippodrome") && map_has_road_access_hippodrome(x, y, 0)) {
        has_road = 1;
    } else if (building_type_registry_impl::type_attr_is(type, "lararium") && map_closest_road_within_radius(x, y, size, 2, 0, 0)) {
        has_road = 1;
    } else if (is_shrine_tier(type) && map_closest_road_within_radius(x, y, size, 2, 0, 0)) {
        has_road = 1;
    }

    if (!has_road) {
        show(WARNING_ROAD_ACCESS_NEEDED, "TR_CITY_WARNING_ROAD_ACCESS_NEEDED");
    }
}

static void check_water(building_type type, int x, int y, int size)
{
    const building_type_registry_impl::BuildingType *definition = building_type_registry_impl::definition_for_type(type);
    if (has_warning || building_type_registry_impl::type_attr_is(type, "reservoir") || building_type_registry_impl::type_attr_is(type, "aqueduct") ||
        !definition || !definition->water_access().has_requirements()) {
        return;
    }

    if (!water_access_runtime_building_type_has_required_access_at(definition, x, y, size)) {
        show(WARNING_WATER_PIPE_ACCESS_NEEDED, "TR_CITY_WARNING_WATER_PIPE_ACCESS_NEEDED");
    }
}

static void check_workers(building_type type)
{
    if (!has_warning && !building_type_registry_impl::type_attr_is(type, "well") && !building_is_fort(type)) {
        if (model_get_building(type)->laborers > 0 && city_labor_workers_needed() >= 10) {
            show(WARNING_WORKERS_NEEDED, "TR_CITY_WARNING_WORKERS_NEEDED");
        }
    }
}

static void check_market(building_type type)
{
    if (!has_warning && building_type_registry_impl::type_attr_is(type, "granary")) {
        if (active_count("market") <= 0) {
            show(WARNING_BUILD_MARKET, "TR_CITY_WARNING_BUILD_MARKET");
        }
    }
}

static void check_barracks(building_type type)
{
    if (!has_warning) {
        if (building_is_fort(type) && active_count("barracks") <= 0 &&
            !grand_temple_for_god(GOD_MARS, true)) {
            show(WARNING_BUILD_BARRACKS, "TR_CITY_WARNING_BUILD_BARRACKS");
        }
    }
}

static void check_armoury(building_type type)
{
    if (building_type_registry_impl::type_attr_is(type, "barracks")) {
        if (active_count("armoury") <= 0) {
            show(WARNING_NO_ARMOURY, "TR_WARNING_NO_ARMOURY");
        }
    }
}

static void check_weapons_access(building_type type)
{
    if (building_type_registry_impl::type_attr_is(type, "barracks")) {
        if (city_resource_count_warehouses_amount(resource_weapons()) <= 0) {
            show(WARNING_WEAPONS_NEEDED, "TR_CITY_WARNING_WEAPONS_NEEDED");
        }
    }
}

static void check_wall(building_type type, int x, int y, int size)
{
    if (!has_warning && building_type_registry_impl::type_attr_is(type, "tower")) {
        if (!map_terrain_is_adjacent_to_wall(x, y, size)) {
            show(WARNING_SENTRIES_NEED_WALL, "TR_CITY_WARNING_SENTRIES_NEED_WALL");
        }
    }
}

static void check_actor_access(building_type type)
{
    if (!has_warning && building_type_registry_impl::type_attr_is(type, "theater")) {
        if (active_count("actor_colony") <= 0) {
            show(WARNING_BUILD_ACTOR_COLONY, "TR_CITY_WARNING_BUILD_ACTOR_COLONY");
        }
    }
}

static void check_gladiator_access(building_type type)
{
    if (!has_warning && building_type_registry_impl::type_attr_is_any(type, {"amphitheater", "colosseum", "arena"})) {
        if (active_count("gladiator_school") <= 0) {
            show(WARNING_BUILD_GLADIATOR_SCHOOL, "TR_CITY_WARNING_BUILD_GLADIATOR_SCHOOL");
        }
    }
}

static void check_lion_access(building_type type)
{
    if (!has_warning && building_type_registry_impl::type_attr_is_any(type, {"colosseum", "arena"})) {
        if (active_count("lion_house") <= 0) {
            show(WARNING_BUILD_LION_HOUSE, "TR_CITY_WARNING_BUILD_LION_HOUSE");
        }
    }
}

static void check_charioteer_access(building_type type)
{
    if (!has_warning && building_type_registry_impl::type_attr_is(type, "hippodrome")) {
        if (active_count("chariot_maker") <= 0) {
            show(WARNING_BUILD_CHARIOT_MAKER, "TR_CITY_WARNING_BUILD_CHARIOT_MAKER");
        }
    }
}

static void check_raw_material_access(const building_type_registry_impl::BuildingType *definition)
{
    if (!definition) {
        return;
    }
    resource_type good = building_output_resource(definition);
    if (good == RESOURCE_NONE) {
        return;
    }
    resource_supply_chain chain[RESOURCE_SUPPLY_CHAIN_MAX_SIZE];
    int num_resources = resource_get_supply_chain_for_good(chain, good);
    for (int i = 0; i < num_resources; i++) {
        building_type producer = building_producer_for_resource(chain[i].raw_material);
        if (building_count_active(producer) <= 0) {
            if (city_resource_count_warehouses_amount(good) <= 0 && city_resource_count_warehouses_amount(chain[i].raw_material) <= 0) {
                building_construction_warning_show_missing_resource(chain[i].raw_material);
                if (empire_can_produce_resource(chain[i].raw_material)) {
                    show_missing_producer(producer);
                } else if (!empire_can_import_resource(chain[i].raw_material)) {
                    show(WARNING_OPEN_TRADE_TO_IMPORT, "TR_CITY_WARNING_OPEN_TRADE_TO_IMPORT");
                } else if (!(city_resource_trade_status(chain[i].raw_material) & TRADE_STATUS_IMPORT)) {
                    show(WARNING_TRADE_IMPORT_RESOURCE, "TR_CITY_WARNING_TRADE_IMPORT_RESOURCE");
                }
            }
        }
    }
}

void building_construction_warning_check_all(building_type type, int x, int y, int size)
{
    const building_type_registry_impl::BuildingType *definition =
        building_type_registry_impl::definition_for_type(type);

    building_construction_warning_check_food_stocks(type);
    check_workers(type);
    check_market(type);
    check_actor_access(type);
    check_gladiator_access(type);
    check_lion_access(type);
    check_charioteer_access(type);

    check_barracks(type);
    check_weapons_access(type);
    check_armoury(type);

    check_wall(type, x, y, size);
    check_water(type, x, y, size);

    check_raw_material_access(definition);

    check_road_access(type, x, y, size);
}

void building_construction_warning_check_food_stocks(building_type type)
{
    if (!has_warning && is_vacant_lot_fill_type(type)) {
        if (city_population() >= 200 && !scenario_property_rome_supplies_wheat()) {
            if (city_resource_food_percentage_produced() <= 95) {
                show(WARNING_MORE_FOOD_NEEDED, "TR_CITY_WARNING_MORE_FOOD_NEEDED");
            }
        }
    }
}

void building_construction_warning_check_reservoir(building_type type)
{
    if (!has_warning && building_type_registry_impl::type_attr_is(type, "reservoir")) {
        if (building_count_active(type)) {
            show(WARNING_CONNECT_TO_RESERVOIR, "TR_CITY_WARNING_CONNECT_TO_RESERVOIR");
        } else {
            show(WARNING_PLACE_RESERVOIR_NEXT_TO_WATER, "TR_CITY_WARNING_PLACE_RESERVOIR_NEXT_TO_WATER");
        }
    }
}
