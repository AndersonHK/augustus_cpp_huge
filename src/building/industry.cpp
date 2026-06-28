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

Building *runtime_building_by_id(unsigned int id)
{
    if (!id) {
        return nullptr;
    }
    Building *found = nullptr;
    Building::for_each([&](Building *building) {
        if (!found && building->id == id) {
            found = building;
        }
    });
    return found;
}

int raw_material_consumer_can_receive(Building &building, resource_type raw_material, int road_network_id)
{
    if (!building.is_in_use() || !building.has_cached_road_access() ||
        building.distance_from_entry() <= 0 || building.road_network_id() != road_network_id) {
        return 0;
    }
    return building.input_storage_available_space(raw_material) >= resource_units_per_load();
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
        int found = 0;
        Building::for_each({ .BuildingType = definition.get() }, [&](Building *building) {
            if (visitor(*building)) {
                found = 1;
            }
        });
        if (found) {
            return 1;
        }
    }
    return 0;
}

} // namespace

resource_type building_output_resource(const building_type_registry_impl::BuildingType *definition)
{
    const std::vector<building_type_registry_impl::ProductionMethod *> &methods = definition->production_methods();
    if (!methods.empty() && methods.front()) {
        const building_type_registry_impl::ProductionMethod *method = methods.front();
        return method->output_resource();
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

int building_production_per_month(const building_type_registry_impl::BuildingType *definition)
{
    const building_type_registry_impl::ProductionMethod *method =
        !definition->production_methods().empty() ? definition->production_methods().front() : nullptr;
    return method ? method->base_monthly_production() : 0;
}

int building_default_production_per_month(const building_type_registry_impl::BuildingType *definition)
{
    const building_type_registry_impl::ProductionMethod *method =
        !definition->production_methods().empty() ? definition->production_methods().front() : nullptr;
    return method ? method->default_base_monthly_production() : 0;
}

int building_set_production_per_month(const building_type_registry_impl::BuildingType *definition, int production)
{
    building_type_registry_impl::ProductionMethod *method =
        !definition->production_methods().empty() ? definition->production_methods().front() : nullptr;
    if (!method) {
        return 0;
    }
    method->override_base_monthly_production(production);
    return 1;
}

int building_is_raw_resource_producer(const building_type_registry_impl::BuildingType *definition)
{
    const building_type_registry_impl::ProductionMethod *method =
        !definition->production_methods().empty() ? definition->production_methods().front() : nullptr;
    return method && method->is_workshop() && method->inputs().empty() &&
        resource_is_raw_material(method->output_resource());
}

int building_get_raw_materials_for_workshop(
    resource_supply_chain *chain,
    const building_type_registry_impl::BuildingType *definition)
{
    const building_type_registry_impl::ProductionMethod *method =
        !definition->production_methods().empty() ? definition->production_methods().front() : nullptr;
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

int building_is_workshop(const building_type_registry_impl::BuildingType *definition)
{
    const building_type_registry_impl::ProductionMethod *method =
        !definition->production_methods().empty() ? definition->production_methods().front() : nullptr;
    return method && method->is_workshop() && !method->inputs().empty();
}

static int random_industry_strikes(int num_strikes)
{
    int strikes = 0;
    int total_industries = building_list_large_size();
    if (num_strikes >= total_industries) {
        for (int i = 0; i < total_industries; i++) {
            Building *industry = runtime_building_by_id(building_list_large_item(i));
            building *b = industry ? const_cast<::building *>(industry->record()) : nullptr;
            if (!b) {
                continue;
            }
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
        Building *industry = runtime_building_by_id(building_list_large_item(index));
        building *b = industry ? const_cast<::building *>(industry->record()) : nullptr;
        int checked = 0;
        while ((!b || b->strike_duration_days > 0) && checked < total_industries) {
            if (current == total_industries) {
                current = 0;
            }
            industry = runtime_building_by_id(building_list_large_item(current));
            b = industry ? const_cast<::building *>(industry->record()) : nullptr;
            current++;
            checked++;
            if (current == index) {
                return strikes;
            }
        }
        if (!b || b->strike_duration_days > 0) {
            return strikes;
        }
        b->strike_duration_days = 48;
        strikes++;
    }
    return strikes;
}

static void force_strike(int num_strikes)
{
    building_list_large_clear();
    Building::for_each({ .hasProductionMethod = true }, [] (Building *building)
    {
        if (building->is_in_use() && building->record()->strike_duration_days == 0) {
            building_list_large_add(building->id);
        }
    });
    if (random_industry_strikes(num_strikes)) {
        city_warning_show(WARNING_SECESSION, translation_for_key("TR_CITY_WARNING_SECESSION"));
    }
}

static void update_venus_gt_production(void)
{
    Building *venus_gt = grand_temple_for_god(GOD_VENUS, false);
    if (!venus_gt || !building_monument_gt_module_is_active(VENUS_MODULE_1_DISTRIBUTE_WINE)) {
        return;
    }
    building *record = const_cast<building *>(venus_gt->record());

    record->monument.progress = static_cast<short>(record->monument.progress +
        (10 + (city_data.culture.population_with_venus_access / MAX_PROGRESS_VENUS_GT / 2)));
    if (record->monument.progress > MAX_PROGRESS_VENUS_GT) {
        if (venus_gt->resource_amount(resource_wine()) < MAX_STORAGE) {
            venus_gt->add_resource(resource_wine(), 1);
        }
        record->monument.progress = record->monument.progress - MAX_PROGRESS_VENUS_GT;
    }
}

void building_industry_update_production(int new_day)
{
    int striking_buildings = 0;

    Building::for_each({ .hasProductionMethod = true }, [&] (Building *building)
    {
        if (!building->is_in_use()) {
            return;
        }

        int native_is_striking = 0;
        if (building->update_native_production(new_day, &native_is_striking)) {
            striking_buildings += native_is_striking;
        }
    });

    if (new_day) {
        update_venus_gt_production();
        int num_strikes = city_data.building.num_striking_industries - striking_buildings;
        force_strike(num_strikes);
    }
}

void building_bless_farms(void)
{
    Building::for_each([] (Building *building)
    {
        if (building->type->is_farm() && building->is_in_use()) {
            building->bless_native_farm();
        }
    });
}

void building_bless_industry(void)
{
    Building::for_each({ .hasProductionMethod = true }, [] (Building *building)
    {
        if (building->is_in_use()) {
            building->bless_native_industry();
        }
    });
}

void building_curse_farms(int big_curse)
{
    Building::for_each([big_curse] (Building *building)
    {
        if (building->type->is_farm() && building->is_in_use()) {
            building->curse_native_farm(big_curse);
        }
    });
}

int building_get_required_raw_amount_for_production(
    const building_type_registry_impl::BuildingType *definition,
    int raw_material)
{
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
    return for_each_raw_material_consumer(raw_material, [&](Building &building) {
        return raw_material_consumer_can_receive(building, raw_material, road_network_id);
    });
}

Building *building_get_workshop_for_raw_material_with_room(int x, int y, int resource, int road_network_id, map_point *dst)
{
    resource_type raw_material = static_cast<resource_type>(resource);
    if (city_resource_is_stockpiled(raw_material)) {
        return nullptr;
    }
    int min_dist = INFINITE;
    Building *min_building = nullptr;
    for_each_raw_material_consumer(raw_material, [&](Building &building) {
        if (!raw_material_consumer_can_receive(building, raw_material, road_network_id)) {
            return 0;
        }
        int dist = calc_maximum_distance(building.x(), building.y(), x, y);
        dist += building.resource_amount(raw_material) / resource_units_per_load();
        if (dist < min_dist) {
            min_dist = dist;
            min_building = &building;
        }
        return 0;
    });
    if (min_building) {
        map_point_store_result(min_building->road_access_x(), min_building->road_access_y(), dst);
        return min_building;
    }
    return nullptr;
}

void building_industry_advance_stats(void)
{
    Building::for_each({ .hasProductionMethod = true }, [] (Building *building)
    {
        if (building->is_in_use() || building->is_mothballed()) {
            building->advance_native_production_stats();
        }
    });
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

    Building::for_each({ .hasProductionMethod = true }, [] (Building *building)
    {
        if (building->is_in_use() && building->record()->strike_duration_days == 0) {
            building_list_large_add(building->id);
        }
    });

    int strikes = random_industry_strikes(to_strike);

    city_data.building.num_striking_industries += strikes;

    if (strikes) {
        city_warning_show(WARNING_SECESSION, translation_for_key("TR_CITY_WARNING_SECESSION"));
    }
}
