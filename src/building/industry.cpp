#include "building/count.h"
#include "building/list.h"
#include "city/warning.h"

#include "building/building_record.h"
#include "building/building.h"
#include "building/building_type_registry_internal.h"
#include "building/industry.h"

#include "building/production_method.h"

#include "building/monument.h"
#include "building/properties.h"
#include "city/data_private.h"
#include "core/calc.h"
#include "core/random.h"
#include "game/time.h"
#include "scenario/property.h"

#include "figure/figure.h"

#include <algorithm>

#define MAX_PROGRESS_VENUS_GT 400
#define MAX_STORAGE 16
#define INFINITE 10000

namespace {

building_type_registry_impl::ProductionMethod *primary_native_production_method(building_type type)
{
    const std::unique_ptr<building_type_registry_impl::BuildingType> &definition =
        building_type_registry_impl::g_building_types[type];
    if (!definition || definition->production_methods().empty()) {
        return nullptr;
    }
    return definition->production_methods().front();
}

int has_native_production_type(building_type type)
{
    return primary_native_production_method(type) != nullptr;
}

int raw_material_consumer_can_receive(const building *b, resource_type raw_material, int road_network_id)
{
    if (!b || b->state != BUILDING_STATE_IN_USE || !b->has_road_access ||
        b->distance_from_entry <= 0 || b->road_network_id != road_network_id) {
        return 0;
    }
    return Building(const_cast<building *>(b)).input_storage_available_space(raw_material) >= resource_units_per_load();
}

template<typename Visitor>
int for_each_raw_material_consumer(resource_type raw_material, Visitor visitor)
{
    for (size_t index = 0; index < building_type_registry_impl::g_building_types.size(); index++) {
        const std::unique_ptr<building_type_registry_impl::BuildingType> &definition =
            building_type_registry_impl::g_building_types[index];
        if (!definition) {
            continue;
        }
        int has_input_storage = 0;
        for (const building_type_registry_impl::StorageType *storage : definition->storage_types()) {
            if (storage && storage->is_input() && storage->handles_resource(raw_material)) {
                has_input_storage = 1;
                break;
            }
        }
        if (!has_input_storage) {
            continue;
        }
        const building_type type = static_cast<building_type>(index);
        for (building *b = building_first_of_type(type); b; b = b->next_of_type) {
            if (visitor(b)) {
                return 1;
            }
        }
    }
    return 0;
}

} // namespace

int building_is_farm(building_type type)
{
    const building_type_registry_impl::BuildingType *definition =
        building_type_registry_impl::definition_for_type(type);
    if (!definition) {
        return 0;
    }
    if (definition->is_farm()) {
        return 1;
    }
    if (!definition->has_composition()) {
        return 0;
    }
    for (const building_type_registry_impl::ComposedPartDefinition &part : definition->composition().parts()) {
        const building_type_registry_impl::BuildingType *part_definition =
            building_type_registry_impl::definition_for_type(part.type);
        if (part_definition && part_definition->is_farm()) {
            return 1;
        }
    }
    return 0;
}

resource_type building_output_resource(building_type type)
{
    if (const building_type_registry_impl::ProductionMethod *method = primary_native_production_method(type)) {
        return method->output_resource();
    }
    const std::unique_ptr<building_type_registry_impl::BuildingType> &definition =
        building_type_registry_impl::g_building_types[type];
    if (definition && definition->has_composition()) {
        for (const building_type_registry_impl::ComposedPartDefinition &part : definition->composition().parts()) {
            if (const building_type_registry_impl::ProductionMethod *method =
                primary_native_production_method(part.type)) {
                return method->output_resource();
            }
        }
    }
    return RESOURCE_NONE;
}

building_type building_producer_for_resource(resource_type resource)
{
    if (resource <= RESOURCE_NONE || !resource_is_declared(resource)) {
        return BUILDING_NONE;
    }

    for (size_t index = 0; index < building_type_registry_impl::g_building_types.size(); index++) {
        const std::unique_ptr<building_type_registry_impl::BuildingType> &definition =
            building_type_registry_impl::g_building_types[index];
        if (!definition) {
            continue;
        }
        for (const building_type_registry_impl::ProductionMethod *method : definition->production_methods()) {
            if (method && method->output_resource() == resource) {
                return static_cast<building_type>(index);
            }
        }
    }

    return BUILDING_NONE;
}

int building_production_per_month(building_type type)
{
    const building_type_registry_impl::ProductionMethod *method = primary_native_production_method(type);
    return method ? method->base_monthly_production() : 0;
}

int building_default_production_per_month(building_type type)
{
    const building_type_registry_impl::ProductionMethod *method = primary_native_production_method(type);
    return method ? method->default_base_monthly_production() : 0;
}

int building_set_production_per_month(building_type type, int production)
{
    building_type_registry_impl::ProductionMethod *method = primary_native_production_method(type);
    if (!method) {
        return 0;
    }
    method->override_base_monthly_production(production);
    return 1;
}

int building_is_raw_resource_producer(building_type type)
{
    const building_type_registry_impl::ProductionMethod *method = primary_native_production_method(type);
    return method && method->is_workshop() && method->inputs().empty() &&
        resource_is_raw_material(method->output_resource());
}

int building_get_raw_materials_for_workshop(resource_supply_chain *chain, building_type type)
{
    const building_type_registry_impl::ProductionMethod *method = primary_native_production_method(type);
    if (!method) {
        return 0;
    }
    const std::vector<building_type_registry_impl::ProductionResourceAmount> &inputs = method->inputs();
    if (chain) {
        for (size_t i = 0; i < inputs.size(); i++) {
            chain[i].good = method->output_resource();
            chain[i].raw_material = inputs[i].resource;
            chain[i].raw_amount = method->scaled_input_amount(inputs[i]);
        }
    }
    return static_cast<int>(inputs.size());
}

int building_is_workshop(building_type type)
{
    const building_type_registry_impl::ProductionMethod *method = primary_native_production_method(type);
    return method && method->is_workshop() && !method->inputs().empty();
}

int building_get_efficiency(const building *b)
{
    return has_native_production_type(b->type) ?
        Building(const_cast<building *>(b)).native_production_efficiency() : -1;
}

int building_industry_get_max_progress(const building *b)
{
    return has_native_production_type(b->type) ?
        Building(const_cast<building *>(b)).native_production_max_progress() : 0;
}

static int random_industry_strikes(int num_strikes)
{
    int strikes = 0;
    int total_industries = building_list_large_size();
    if (num_strikes >= total_industries) {
        for (int i = 0; i < total_industries; i++) {
            building *b = building_get(building_list_large_item(i));
            if (b->strike_duration_days == 0) {
                b->strike_duration_days = 48;
                strikes++;
            }
        }
        return strikes;
    }

    for (int i = 0; i < num_strikes; i++) {
        int index = random_from_stdlib() % total_industries;

        // Prevent the same building from being selected twice
        int current = index + 1;
        building *b = building_get(building_list_large_item(index));
        while (b->strike_duration_days > 0) {
            if (current == total_industries) {
                current = 0;
            }
            b = building_get(building_list_large_item(current));
            current++;
            if (current == index) {
                return strikes;
            }
        }
        b->strike_duration_days = 48;
        strikes++;
    }
    return strikes;
}

static void force_strike(int num_strikes)
{
    building_list_large_clear();
    for (size_t index = 0; index < building_type_registry_impl::g_building_types.size(); index++) {
        building_type type = static_cast<building_type>(index);
        if (!has_native_production_type(type)) {
            continue;
        }
        for (building *b = building_first_of_type(type); b; b = b->next_of_type) {
            if (b->state == BUILDING_STATE_IN_USE && b->strike_duration_days == 0) {
                building_list_large_add(b->id);
            }
        }
    }
    if (random_industry_strikes(num_strikes)) {
        city_warning_show(WARNING_SECESSION, translation_for_key("TR_CITY_WARNING_SECESSION"));
    }
}

static void update_venus_gt_production(void)
{
    int venus_gt_id = building_monument_get_grand_temple_for_god(GOD_VENUS);
    building *venus_gt = venus_gt_id ? building_get(venus_gt_id) : nullptr;
    if (!venus_gt || !building_monument_gt_module_is_active(VENUS_MODULE_1_DISTRIBUTE_WINE)) {
        return;
    }

    venus_gt->monument.progress += (10 + (city_data.culture.population_with_venus_access /
        MAX_PROGRESS_VENUS_GT / 2));
    if (venus_gt->monument.progress > MAX_PROGRESS_VENUS_GT) {
        if (venus_gt->resources[resource_wine()] < MAX_STORAGE) {
            venus_gt->resources[resource_wine()] += 1;
        }
        venus_gt->monument.progress = venus_gt->monument.progress - MAX_PROGRESS_VENUS_GT;
    }
}

int building_industry_has_raw_materials_for_production(const building *b)
{
    return has_native_production_type(b->type) ?
        Building(const_cast<building *>(b)).native_production_has_raw_materials() : 1;
}

void building_industry_update_production(int new_day)
{
    int striking_buildings = 0;

    for (size_t index = 0; index < building_type_registry_impl::g_building_types.size(); index++) {
        building_type type = static_cast<building_type>(index);
        if (!has_native_production_type(type)) {
            continue;
        }
        for (building *b = building_first_of_type(type); b; b = b->next_of_type) {
            if (b->state != BUILDING_STATE_IN_USE) {
                continue;
            }

            int native_is_striking = 0;
            if (Building(b).update_native_production(new_day, &native_is_striking)) {
                striking_buildings += native_is_striking;
                continue;
            }
        }
    }

    if (new_day) {
        update_venus_gt_production();
        int num_strikes = city_data.building.num_striking_industries - striking_buildings;
        force_strike(num_strikes);
    }
}

int building_stockpiling_enabled(building *b)
{
    return b->data.industry.is_stockpiling;
}

int building_loads_stored(const building *b)
{
    int amount = 0;
    for (int resource_index = (RESOURCE_NONE + 1); resource_index < RESOURCE_SLOT_COUNT; resource_index++) {
        amount += b->resources[resource_index];
    }
    return amount;
}

void building_bless_farms(void)
{
    Building::for_each([] (Building *building)
    {
        if (building_is_farm(building->type->type()) && building->is_in_use()) {
            building->bless_native_farm();
        }
    });
}

void building_bless_industry(void)
{
    Building::for_each([] (Building *building)
    {
        if (has_native_production_type(building->type->type()) && building->is_in_use()) {
            building->bless_native_industry();
        }
    });
}

void building_curse_farms(int big_curse)
{
    Building::for_each([big_curse] (Building *building)
    {
        if (building_is_farm(building->type->type()) && building->is_in_use()) {
            building->curse_native_farm(big_curse);
        }
    });
}

int building_get_required_raw_amount_for_production(building_type type, int raw_material)
{
    const std::unique_ptr<building_type_registry_impl::BuildingType> &definition =
        building_type_registry_impl::g_building_types[type];
    if (!definition) {
        return 0;
    }
    int amount = 0;
    for (const building_type_registry_impl::ProductionMethod *method : definition->production_methods()) {
        if (!method) {
            continue;
        }
        for (const building_type_registry_impl::ProductionResourceAmount &input : method->inputs()) {
            if (input.resource == static_cast<resource_type>(raw_material)) {
                amount = std::max(amount, method->scaled_input_amount(input));
            }
        }
    }
    return amount;
}

int building_workshop_add_raw_material(Building *b, int resource, int loads, unsigned int figure_id)
{
    if (!b || !b->id || loads <= 0 || resource <= RESOURCE_NONE || resource >= RESOURCE_SLOT_COUNT) {
        return 0;
    }
    return b->receive_input_storage_loads(static_cast<resource_type>(resource), loads, figure_id);
}

int building_has_workshop_for_raw_material_with_room(int resource, int road_network_id)
{
    resource_type raw_material = static_cast<resource_type>(resource);
    return for_each_raw_material_consumer(raw_material, [&](building *b) {
        return raw_material_consumer_can_receive(b, raw_material, road_network_id);
    });
}

int building_get_workshop_for_raw_material_with_room(int x, int y, int resource, int road_network_id, map_point *dst)
{
    resource_type raw_material = static_cast<resource_type>(resource);
    if (city_resource_is_stockpiled(raw_material)) {
        return 0;
    }
    int min_dist = INFINITE;
    building *min_building = 0;
    for_each_raw_material_consumer(raw_material, [&](building *b) {
        if (!raw_material_consumer_can_receive(b, raw_material, road_network_id)) {
            return 0;
        }
        int dist = calc_maximum_distance(b->x, b->y, x, y);
        dist += b->resources[resource] / resource_units_per_load();
        if (dist < min_dist) {
            min_dist = dist;
            min_building = b;
        }
        return 0;
    });
    if (min_building) {
        map_point_store_result(min_building->road_access_x, min_building->road_access_y, dst);
        return min_building->id;
    }
    return 0;
}

int building_get_workshop_for_raw_material(int x, int y, int resource, int road_network_id, map_point *dst)
{
    return building_get_workshop_for_raw_material_with_room(x, y, resource, road_network_id, dst);
}

static void update_stats_for_type(building_type type)
{
    Building::for_each([type] (Building *building)
    {
        if (building->type->type() == type && (building->is_in_use() || building->is_mothballed())) {
            building->advance_native_production_stats();
        }
    });
}

void building_industry_advance_stats(void)
{
    for (size_t index = 0; index < building_type_registry_impl::g_building_types.size(); index++) {
        building_type type = static_cast<building_type>(index);
        if (has_native_production_type(type)) {
            update_stats_for_type(type);
        }
    }
}

void building_industry_start_strikes(void)
{
    if (city_data.sentiment.value >= 55) {
        return;
    }
    int base_chance = 60 - city_data.sentiment.value;
    int strike_chance = base_chance * base_chance / 36;
    if (random_from_stdlib() % 100 > strike_chance) {
        return;
    }
    int to_strike = calc_bound(city_data.population.population / 2000, 1, 12);

    building_list_large_clear();

    for (size_t index = 0; index < building_type_registry_impl::g_building_types.size(); index++) {
        building_type type = static_cast<building_type>(index);
        if (!has_native_production_type(type)) {
            continue;
        }
        for (building *b = building_first_of_type(type); b; b = b->next_of_type) {
            if (b->state == BUILDING_STATE_IN_USE && b->strike_duration_days == 0) {
                building_list_large_add(b->id);
            }
        }
    }

    int strikes = random_industry_strikes(to_strike);

    city_data.building.num_striking_industries += strikes;

    if (strikes) {
        city_warning_show(WARNING_SECESSION, translation_for_key("TR_CITY_WARNING_SECESSION"));
    }
}
