#include "building/count.h"
#include "building/image.h"
#include "building/list.h"
#include "city/warning.h"
#include "map/building_tiles.h"

#include "building/building_record.h"
#include "building/building.h"
#include "building/building_type_registry_internal.h"
#include "building/industry.h"

#include "building/production_method.h"

#include "building/monument.h"
#include "building/properties.h"
#include "city/buildings.h"
#include "city/data_private.h"
#include "core/calc.h"
#include "core/image.h"
#include "core/random.h"
#include "game/time.h"
#include "scenario/property.h"

#include "figure/figure.h"

#include <cstring>

#define MAX_PROGRESS_VENUS_GT 400
#define DENARII_MINTED_PER_PRODUCTION 100
#define DENARII_COST_PER_GOLD 600
#define MAX_STORAGE 16
#define INFINITE 10000

#define RECORD_PRODUCTION_MONTHS 12

#define MERCURY_BLESSING_LOADS 3

namespace {

building_type runtime_type(const char *text_id)
{
    return building_type_registry_impl::type_from_attr(text_id);
}

int type_matches(building_type type, const char *text_id)
{
    return type == runtime_type(text_id);
}

building_type city_mint_type()
{
    return runtime_type("city_mint");
}

int is_city_mint_type(building_type type)
{
    return type_matches(type, "city_mint");
}

int is_wharf_type(building_type type)
{
    return type_matches(type, "wharf");
}

building_type_registry_impl::ProductionMethod *primary_native_production_method(building_type type)
{
    const std::unique_ptr<building_type_registry_impl::BuildingType> &definition =
        building_type_registry_impl::g_building_types[type];
    if (!definition || definition->production_methods().empty()) {
        return nullptr;
    }
    return definition->production_methods().front();
}

building_type_registry_impl::ProductionMethod *production_method_for_output(building_type type, resource_type output)
{
    const std::unique_ptr<building_type_registry_impl::BuildingType> &definition =
        building_type_registry_impl::g_building_types[type];
    if (!definition) {
        return nullptr;
    }
    for (building_type_registry_impl::ProductionMethod *method : definition->production_methods()) {
        if (method && method->output_resource() == output) {
            return method;
        }
    }
    return primary_native_production_method(type);
}

int has_native_production_type(building_type type)
{
    if (is_city_mint_type(type) || is_wharf_type(type)) {
        return 0;
    }
    return primary_native_production_method(type) != nullptr;
}

int is_valid_resource_slot(resource_type resource)
{
    return resource > RESOURCE_NONE && resource < RESOURCE_SLOT_COUNT;
}

} // namespace

int building_is_farm(building_type type)
{
    const building_type_registry_impl::BuildingType *definition =
        building_type_registry_impl::definition_for_type(type);
    return definition && definition->is_farm();
}

resource_type building_output_resource(building_type type)
{
    if (const building_type_registry_impl::ProductionMethod *method = primary_native_production_method(type)) {
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
    if (const building_type_registry_impl::ProductionMethod *method = primary_native_production_method(type)) {
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

    resource_type good = building_output_resource(type);
    if (good == RESOURCE_NONE) {
        return 0;
    }
    return resource_get_supply_chain_for_good(chain, good);
}

int building_is_workshop(building_type type)
{
    const building_type_registry_impl::ProductionMethod *method = primary_native_production_method(type);
    return method && method->is_workshop() && !method->inputs().empty();
}

int building_get_efficiency(const building *b)
{
    if (has_native_production_type(b->type)) {
        return Building(const_cast<building *>(b)).native_production_efficiency();
    }

    if (b->state == BUILDING_STATE_MOTHBALLED) {
        return -1;
    }
    resource_type resource = static_cast<resource_type>(b->output_resource_id);
    if (b->data.industry.age_months == 0 || !resource) {
        return -1;
    }
    const building_type_registry_impl::ProductionMethod *method = production_method_for_output(b->type, resource);
    if (!method) {
        return -1;
    }
    const int production_for_resource = method->effective_monthly_production();
    if (production_for_resource <= 0) {
        return -1;
    }

    int percentage = calc_percentage(b->data.industry.average_production_per_month, production_for_resource);
    return calc_bound(percentage, 0, 100);
}

int building_industry_get_max_progress(const building *b)
{
    if (has_native_production_type(b->type)) {
        return Building(const_cast<building *>(b)).native_production_max_progress();
    }

    const building_type_registry_impl::ProductionMethod *method =
        production_method_for_output(b->type, static_cast<resource_type>(b->output_resource_id));
    if (!method) {
        return 0;
    }
    const int monthly_production = method->effective_monthly_production();
    if (monthly_production <= 0) {
        return 0;
    }
    return calc_percentage(GAME_TIME_DAYS_PER_MONTH * 2 * model_get_building(b->type)->laborers, monthly_production);
}

static void update_farm_image(const building *b)
{
    map_building_tiles_add_farm(b->id, b->x, b->y, building_image_get_base_farm_crop(b->type),
        calc_percentage(b->data.industry.progress, building_industry_get_max_progress(b)));
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
    for (int resource_index = (RESOURCE_NONE + 1); resource_index < RESOURCE_SLOT_COUNT; resource_index++) {
        resource_type resource = static_cast<resource_type>(resource_index);
        building_type type = building_producer_for_resource(resource);
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

static void update_city_mint_production(int new_day)
{
    if (building_count_active(runtime_type("senate")) == 0) {
        return;
    }

    building *b = building_first_of_type(city_mint_type());
    if (!b || b->state != BUILDING_STATE_IN_USE) {
        return;
    }

    b->data.industry.has_raw_materials = 0;
    if (b->houses_covered <= 0 || b->num_workers <= 0) {
        return;
    }

    if (b->resources[resource_gold()] < BUILDING_INDUSTRY_CITY_MINT_GOLD_PER_COIN &&
        b->output_resource_id == resource_denarii()) {
        return;
    }

    if (b->data.industry.curse_days_left && new_day) {
        b->data.industry.curse_days_left--;
        return;
    }

    if (b->output_resource_id == resource_gold()) {
        if (b->data.industry.progress == 0) {
            if (city_finance_out_of_money()) {
                return;
            }
            city_finance_process_sundry(DENARII_COST_PER_GOLD);
        }
        b->data.industry.progress += b->num_workers;

        int max = building_industry_get_max_progress(b);
        if (b->data.industry.progress > max) {
            b->data.industry.progress = max;
        }
        return;
    }

    b->data.industry.progress += b->num_workers;

    int max = building_industry_get_max_progress(b);
    if (b->data.industry.progress > max) {
        b->data.industry.production_current_month += 100;
        b->data.industry.progress = 0;
        int minted_personal_funds = 0;
        if (city_buildings_has_governor_house()) {
            int personal_salary = city_emperor_salary_amount();
            if (personal_salary > 5) {
                minted_personal_funds = DENARII_MINTED_PER_PRODUCTION / 10;
                minted_personal_funds = calc_adjust_with_percentage(minted_personal_funds, personal_salary);
                if (minted_personal_funds == 0) {
                    minted_personal_funds = 1;
                }
            }
        }
        city_data.emperor.personal_savings += minted_personal_funds;
        city_finance_treasury_add_miscellaneous(DENARII_MINTED_PER_PRODUCTION - minted_personal_funds);
        if (b->resources[resource_gold()] >= BUILDING_INDUSTRY_CITY_MINT_GOLD_PER_COIN) {
            b->resources[resource_gold()] -= BUILDING_INDUSTRY_CITY_MINT_GOLD_PER_COIN;
                b->data.industry.has_raw_materials = 1;
        }
    }
}

int building_industry_has_raw_materials_for_production(const building *b)
{
    if (has_native_production_type(b->type)) {
        return Building(const_cast<building *>(b)).native_production_has_raw_materials();
    }

    resource_supply_chain chain[RESOURCE_SUPPLY_CHAIN_MAX_SIZE];
    int num_raw_materials = building_get_raw_materials_for_workshop(chain, b->type);
    for (int i = 0; i < num_raw_materials; i++) {
        if (!is_valid_resource_slot(chain[i].raw_material) ||
            b->resources[chain[i].raw_material] < chain[i].raw_amount) {
            return 0;
        }
    }
    return 1;
}

void building_industry_update_production(int new_day)
{
    int striking_buildings = 0;

    for (int resource_index = (RESOURCE_NONE + 1); resource_index < RESOURCE_SLOT_COUNT; resource_index++) {
        resource_type resource = static_cast<resource_type>(resource_index);
        building_type type = building_producer_for_resource(resource);
        int is_storable = resource_is_storable(resource);
        for (building *b = building_first_of_type(type); b; b = b->next_of_type) {
            if (b->state != BUILDING_STATE_IN_USE) {
                continue;
            }

            int native_is_striking = 0;
            if (Building(b).update_native_production(new_day, &native_is_striking)) {
                striking_buildings += native_is_striking;
                continue;
            }

            if (b->strike_duration_days > 0) {
                striking_buildings++;
                if (new_day) {
                    b->strike_duration_days--;
                    if (city_data.sentiment.value > 50) {
                        b->strike_duration_days -= 3;
                    }
                    if (city_data.sentiment.value > 65) {
                        b->strike_duration_days = 0;
                    }
                }
                if (b->strike_duration_days == 0) {
                    city_data.building.num_striking_industries--;
                    striking_buildings--;
                    // remove striker walker
                    if (Figure *striker = Figure::get(b->figure_id4)) {
                        striker->remove();
                    }
                }
            }

            b->data.industry.has_raw_materials = 0;
            if (b->houses_covered <= 0 || b->num_workers <= 0 || b->strike_duration_days > 0) {
                continue;
            }

            if (!building_industry_has_raw_materials_for_production(b)) {
                continue;
            }

            if (!is_storable && b->data.industry.progress == 0 &&
                !building_has_workshop_for_raw_material_with_room(resource, b->road_network_id) &&
                !building_monument_get_monument(b->x, b->y, resource, b->road_network_id, 0)) {
                continue;
            }

            if (b->data.industry.curse_days_left) {
                if (new_day) {
                    b->data.industry.curse_days_left--;
                }
                continue;
            }
            if (b->data.industry.blessing_days_left && new_day) {
                b->data.industry.blessing_days_left--;
            }
            int progress = b->num_workers;
            if (b->data.industry.blessing_days_left && building_is_farm(b->type)) {
                progress += b->num_workers;
            }
            b->data.industry.progress += progress;

            int max = building_industry_get_max_progress(b);
            if (b->data.industry.progress > max) {
                b->data.industry.progress = max;
            }
            if (building_is_farm(b->type)) {
                update_farm_image(b);
            }
        }
    }

    update_city_mint_production(new_day);

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

int building_industry_has_produced_resource(building *b)
{
    if (has_native_production_type(b->type)) {
        return Building(b).native_production_has_produced_resource();
    }

    if (is_city_mint_type(b->type)) {
        if (b->output_resource_id != resource_gold()) {
            return 0;
        }
        if (b->resources[resource_gold()] >= resource_units_per_load()) {
            return 1;
        }
    }
    return b->data.industry.progress >= building_industry_get_max_progress(b);
}

void building_industry_start_new_production(building *b)
{
    if (has_native_production_type(b->type)) {
        Building(b).start_native_production();
        return;
    }

    if (is_city_mint_type(b->type) && b->output_resource_id == resource_gold() &&
        b->resources[resource_gold()] >= resource_units_per_load()) {
        b->resources[resource_gold()] -= resource_units_per_load();
        return;
    }
    if (b->data.industry.progress >= building_industry_get_max_progress(b)) {
        b->data.industry.production_current_month += 100;
        b->data.industry.progress = 0;
    }
    resource_supply_chain chain[RESOURCE_SUPPLY_CHAIN_MAX_SIZE];
    int num_raw_materials = building_get_raw_materials_for_workshop(chain, b->type);
    int has_raw_materials = building_industry_has_raw_materials_for_production(b);
    if (has_raw_materials) {
        for (int i = 0; i < num_raw_materials; i++) {
            if (is_valid_resource_slot(chain[i].raw_material)) {
                b->resources[chain[i].raw_material] -= chain[i].raw_amount;
            }
        }
    }
    b->data.industry.has_raw_materials = has_raw_materials;
    if (building_is_farm(b->type)) {
        update_farm_image(b);
    }
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
    for (int resource_index = (RESOURCE_NONE + 1); resource_index < RESOURCE_SLOT_COUNT; resource_index++) {
        resource_type resource = static_cast<resource_type>(resource_index);
        building_type type = building_producer_for_resource(resource);
        if (!building_is_farm(type)) {
            continue;
        }
        for (building *b = building_first_of_type(type); b; b = b->next_of_type) {
            if (b->state == BUILDING_STATE_IN_USE) {
                Building building_obj(b);
                if (building_obj.has_native_production()) {
                    building_obj.bless_native_farm();
                    continue;
                }
                b->data.industry.progress = building_industry_get_max_progress(b);
                b->data.industry.curse_days_left = 0;
                b->data.industry.blessing_days_left = 16;
                update_farm_image(b);
            }
        }
    }
}

void building_bless_industry(void)
{
    for (int resource_index = (RESOURCE_NONE + 1); resource_index <= resource_denarii(); resource_index++) {
        resource_type resource = static_cast<resource_type>(resource_index);
        building_type type = building_producer_for_resource(resource);
        resource_supply_chain chain[RESOURCE_SUPPLY_CHAIN_MAX_SIZE];
        int num_resources = resource_get_supply_chain_for_good(chain, resource);
        if (num_resources == 0) {
            continue;
        }
        for (building *b = building_first_of_type(type); b; b = b->next_of_type) {
            if (b->state != BUILDING_STATE_IN_USE || b->output_resource_id != resource) {
                continue;
            }
            Building building_obj(b);
            if (building_obj.has_native_production()) {
                building_obj.bless_native_industry();
                continue;
            }
            if (b->num_workers <= 0) {
                continue;
            }
            for (int i = 0; i < num_resources; i++) {
                if (!is_valid_resource_slot(chain[i].raw_material)) {
                    continue;
                }
                if (b->resources[chain[i].raw_material] > 0 &&
                    b->resources[chain[i].raw_material] < MERCURY_BLESSING_LOADS * chain[i].raw_amount) {
                    b->resources[chain[i].raw_material] = MERCURY_BLESSING_LOADS * chain[i].raw_amount;
                }
            }
            if (b->data.industry.progress) {
                b->data.industry.progress = building_industry_get_max_progress(b);
            }
        }
    }
}

void building_curse_farms(int big_curse)
{
    for (int resource_index = (RESOURCE_NONE + 1); resource_index < RESOURCE_SLOT_COUNT; resource_index++) {
        resource_type resource = static_cast<resource_type>(resource_index);
        building_type type = building_producer_for_resource(resource);
        if (!building_is_farm(type)) {
            continue;
        }
        for (building *b = building_first_of_type(type); b; b = b->next_of_type) {
            if (b->state == BUILDING_STATE_IN_USE) {
                Building building_obj(b);
                if (building_obj.has_native_production()) {
                    building_obj.curse_native_farm(big_curse);
                    continue;
                }
                b->data.industry.progress = 0;
                b->data.industry.blessing_days_left = 0;
                b->data.industry.curse_days_left = big_curse ? 48 : 4;
                update_farm_image(b);
            }
        }
    }
    building *city_mint = building_first_of_type(city_mint_type());
    if (city_mint && city_mint->state == BUILDING_STATE_IN_USE) {
        city_mint->data.industry.progress = 0;
        city_mint->data.industry.curse_days_left = big_curse ? 48 : 4;
    }
}

int building_get_required_raw_amount_for_production(building_type type, int raw_material)
{
    if (const building_type_registry_impl::ProductionMethod *method = primary_native_production_method(type)) {
        for (const building_type_registry_impl::ProductionResourceAmount &input : method->inputs()) {
            if (input.resource == static_cast<resource_type>(raw_material)) {
                return method->scaled_input_amount(input);
            }
        }
        return 0;
    }

    resource_type good = building_output_resource(type);
    if (good == RESOURCE_NONE) {
        return 0;
    }
    resource_supply_chain chain[RESOURCE_SUPPLY_CHAIN_MAX_SIZE];
    int num_resources = resource_get_supply_chain_for_good(chain, good);
    for (int i = 0; i < num_resources; i++) {
        if (chain[i].raw_material == raw_material) {
            return chain[i].raw_amount;
        }
    }
    return 0;
}

void building_workshop_add_raw_material(building *b, int resource)
{
    if (!b->id) {
        return;
    }
    if (resource > RESOURCE_NONE && resource < RESOURCE_SLOT_COUNT &&
        building_get_required_raw_amount_for_production(b->type, resource) > 0) {
        b->resources[resource] += resource_units_per_load();
    }
}

int building_has_workshop_for_raw_material_with_room(int resource, int road_network_id)
{
    resource_supply_chain chain[RESOURCE_SUPPLY_CHAIN_MAX_SIZE];
    resource_type raw_material = static_cast<resource_type>(resource);
    int num_goods = resource_get_supply_chain_for_raw_material(chain, raw_material);

    for (int i = 0; i < num_goods; i++) {
        building_type type = building_producer_for_resource(chain[i].good);
        for (building *b = building_first_of_type(type); b; b = b->next_of_type) {
            const int desired_stock = has_native_production_type(type) ?
                2 * building_get_required_raw_amount_for_production(type, resource) : 2 * resource_units_per_load();
            if (b->state == BUILDING_STATE_IN_USE && b->has_road_access && b->distance_from_entry > 0 &&
                b->road_network_id == road_network_id && b->resources[resource] < desired_stock) {
                if (is_city_mint_type(type)) {
                    if (b->monument.phase != MONUMENT_FINISHED || b->output_resource_id == resource_gold()) {
                        continue;
                    }
                }
                return 1;
            }
        }
    }
    return 0;
}

int building_get_workshop_for_raw_material_with_room(int x, int y, int resource, int road_network_id, map_point *dst)
{
    resource_type raw_material = static_cast<resource_type>(resource);
    if (city_resource_is_stockpiled(raw_material)) {
        return 0;
    }
    resource_supply_chain chain[RESOURCE_SUPPLY_CHAIN_MAX_SIZE];
    int num_goods = resource_get_supply_chain_for_raw_material(chain, raw_material);

    if (num_goods == 0) {
        return 0;
    }
    int min_dist = INFINITE;
    building *min_building = 0;
    for (int i = 0; i < num_goods; i++) {
        building_type type = building_producer_for_resource(chain[i].good);
        for (building *b = building_first_of_type(type); b; b = b->next_of_type) {
            const int desired_stock = has_native_production_type(type) ?
                2 * building_get_required_raw_amount_for_production(type, resource) : 2 * resource_units_per_load();
            if (b->state != BUILDING_STATE_IN_USE || !b->has_road_access || b->distance_from_entry <= 0 ||
                b->road_network_id != road_network_id || b->resources[resource] >= desired_stock) {
                continue;
            }
            if (is_city_mint_type(type)) {
                if (b->monument.phase != MONUMENT_FINISHED || b->output_resource_id == resource_gold()) {
                    continue;
                }
            }
            int dist = calc_maximum_distance(b->x, b->y, x, y);
            if (b->resources[resource] > 0) {
                dist += 20;
            }
            if (dist < min_dist) {
                min_dist = dist;
                min_building = b;
            }
        }
    }
    if (min_building) {
        map_point_store_result(min_building->road_access_x, min_building->road_access_y, dst);
        return min_building->id;
    }
    return 0;
}

int building_get_workshop_for_raw_material(int x, int y, int resource, int road_network_id, map_point *dst)
{
    resource_type raw_material = static_cast<resource_type>(resource);
    if (city_resource_is_stockpiled(raw_material)) {
        return 0;
    }
    resource_supply_chain chain[RESOURCE_SUPPLY_CHAIN_MAX_SIZE];
    int num_goods = resource_get_supply_chain_for_raw_material(chain, raw_material);

    if (num_goods == 0) {
        return 0;
    }
    int min_dist = INFINITE;
    building *min_building = 0;
    for (int i = 0; i < num_goods; i++) {
        building_type type = building_producer_for_resource(chain[i].good);
        for (building *b = building_first_of_type(type); b; b = b->next_of_type) {
            if (b->state != BUILDING_STATE_IN_USE ||
                !b->has_road_access || b->distance_from_entry <= 0 || b->road_network_id != road_network_id) {
                continue;
            }
            if (is_city_mint_type(type)) {
                if (b->monument.phase != MONUMENT_FINISHED || b->output_resource_id == resource_gold()) {
                    continue;
                }
            }
            int dist = b->resources[resource] +
                calc_maximum_distance(b->x, b->y, x, y);
            if (dist < min_dist) {
                min_dist = dist;
                min_building = b;
            }
        }
    }
    if (min_building) {
        map_point_store_result(min_building->road_access_x, min_building->road_access_y, dst);
        return min_building->id;
    }
    return 0;
}

static void update_stats_for_type(building_type type)
{
    for (building *b = building_first_of_type(type); b; b = b->next_of_type) {
        if (b->state != BUILDING_STATE_IN_USE && b->state != BUILDING_STATE_MOTHBALLED) {
            continue;
        }
        Building building_obj(b);
        if (building_obj.has_native_production()) {
            building_obj.advance_native_production_stats();
            continue;
        }
        if (b->data.industry.age_months < RECORD_PRODUCTION_MONTHS) {
            b->data.industry.age_months++;
        }
        int sum_months = b->data.industry.average_production_per_month * (b->data.industry.age_months - 1);
        int pending_production_percentage = is_wharf_type(b->type) ?
            0 : calc_percentage(b->data.industry.progress, building_industry_get_max_progress(b));
        pending_production_percentage = calc_bound(pending_production_percentage, 0, 100);
        sum_months += b->data.industry.production_current_month + pending_production_percentage;
        b->data.industry.average_production_per_month = sum_months / b->data.industry.age_months;
        int leftover_from_average = sum_months % b->data.industry.age_months;
        b->data.industry.production_current_month = leftover_from_average - pending_production_percentage;
    }
}

void building_industry_advance_stats(void)
{
    for (int resource_index = (RESOURCE_NONE + 1); resource_index < RESOURCE_SLOT_COUNT; resource_index++) {
        update_stats_for_type(building_producer_for_resource(static_cast<resource_type>(resource_index)));
    }
    update_stats_for_type(city_mint_type());
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

    for (int resource_index = (RESOURCE_NONE + 1); resource_index < RESOURCE_SLOT_COUNT; resource_index++) {
        building_type type = building_producer_for_resource(static_cast<resource_type>(resource_index));
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
