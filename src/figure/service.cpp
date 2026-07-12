#include "building/building.h"
#include "building/distribution.h"
#include "building/house.h"
#include "figure/roamer_preview.h"
#include "figuretype/crime.h"
#include "map/building.h"

#include "service.h"

#include "building/building_type_registry_internal.h"
#include "building/religion.h"

#include "building/building_record.h"
#include "building/housing_type.h"
#include "building/monument.h"
#include "building/properties.h"
#include "city/buildings.h"
#include "city/finance.h"
#include "core/config.h"
#include "core/random.h"
#include "game/resource.h"
#include "game/time.h"
#include "map/grid.h"

#define MAX_COVERAGE 96
#define TOURISM_COOLDOWN 96

static int temple_is_tier(const building_type_registry_impl::BuildingType *definition,
    building_type_registry_impl::ReligionTier tier)
{
    return definition && definition->is_temple(std::nullopt, tier);
}

static int temple_is_basic(const building_type_registry_impl::BuildingType *definition)
{
    return temple_is_tier(definition, building_type_registry_impl::ReligionTier::Small) ||
        temple_is_tier(definition, building_type_registry_impl::ReligionTier::Large);
}

static int temple_serves_god(const building_type_registry_impl::BuildingType *definition, god_type god)
{
    return definition && definition->is_temple(god);
}

static resource_type next_resource(resource_type resource)
{
    return static_cast<resource_type>(resource + 1);
}

static Building *service_building_at(int grid_offset)
{
    return map_building_exists_at(grid_offset) ? &map_building_at(grid_offset) : nullptr;
}

static building *service_building_record(Building &building)
{
    return const_cast<::building *>(building.record());
}

static Building *service_building_for_record(building *record)
{
    if (!record) {
        return nullptr;
    }
    Building *found = nullptr;
    Building::for_each([&](Building *building) {
        if (!found && building && building->id == record->id) {
            found = building;
        }
    });
    return found;
}

static int provide_culture(int x, int y, void (*callback)(building *))
{
    int serviced = 0;
    int x_min, y_min, x_max, y_max;
    map_grid_get_area(x, y, 1, 2, &x_min, &y_min, &x_max, &y_max);
    for (int yy = y_min; yy <= y_max; yy++) {
        for (int xx = x_min; xx <= x_max; xx++) {
            int grid_offset = map_grid_offset(xx, yy);
            if (Building *building_object = service_building_at(grid_offset)) {
                building *b = service_building_record(*building_object);
                if (b->house_size && b->house_population > 0) {
                    callback(b);
                    serviced++;
                }
            }
        }
    }
    return serviced;
}

static void provide_sickness(int x, int y, void (*callback)(building *, int sickness_dest), int sickness_dest)
{
    int x_min, y_min, x_max, y_max;
    map_grid_get_area(x, y, 1, 2, &x_min, &y_min, &x_max, &y_max);
    for (int yy = y_min; yy <= y_max; yy++) {
        for (int xx = x_min; xx <= x_max; xx++) {
            int grid_offset = map_grid_offset(xx, yy);
            if (Building *building_object = service_building_at(grid_offset)) {
                building *b = service_building_record(*building_object);
                random_generate_next();
                // 1/16 chance of spreading sickness
                if (b->house_size && b->house_population > 0 && !(random_short() & 0xf)) {
                    callback(b, sickness_dest);
                }
            }
        }
    }
}

static int provide_entertainment(int x, int y, int shows, void (*callback)(building *, int))
{
    int serviced = 0;
    int x_min, y_min, x_max, y_max;
    map_grid_get_area(x, y, 1, 2, &x_min, &y_min, &x_max, &y_max);
    for (int yy = y_min; yy <= y_max; yy++) {
        for (int xx = x_min; xx <= x_max; xx++) {
            int grid_offset = map_grid_offset(xx, yy);
            if (Building *building_object = service_building_at(grid_offset)) {
                building *b = service_building_record(*building_object);
                if (b->house_size && b->house_population > 0) {
                    callback(b, shows);
                    serviced++;
                }
            }
        }
    }
    return serviced;
}

static void labor_seeker_coverage(building *b)
{
    (void)b;
}

static void theater_coverage(building *b)
{
    b->data.house.theater = MAX_COVERAGE;
}

static void amphitheater_coverage(building *b, int shows)
{
    b->data.house.amphitheater_actor = MAX_COVERAGE;
    if (shows == 2) {
        b->data.house.amphitheater_gladiator = MAX_COVERAGE;
    }
}

static void colosseum_coverage(building *b, int shows)
{
    b->data.house.colosseum_gladiator = MAX_COVERAGE;
    if (shows == 2) {
        b->data.house.colosseum_lion = MAX_COVERAGE;
    }
}

static void arena_coverage(building *b, int shows)
{
    b->house_arena_gladiator = MAX_COVERAGE;
    if (shows == 2) {
        b->house_arena_lion = MAX_COVERAGE;
    }
}

static void hippodrome_coverage(building *b)
{
    b->data.house.hippodrome = MAX_COVERAGE;
}

static void tavern_coverage(building *b, int products)
{
    if (products) {
        b->house_tavern_wine_access = MAX_COVERAGE;
        if (products > 1) {
            b->house_tavern_food_access = MAX_COVERAGE;
        }
    }
}

static void bathhouse_coverage(building *b)
{
    b->data.house.bathhouse = MAX_COVERAGE;
}

static void religion_coverage_ceres(building *b)
{
    b->data.house.temple_ceres = MAX_COVERAGE;
}

static void religion_coverage_neptune(building *b)
{
    b->data.house.temple_neptune = MAX_COVERAGE;
}

static void religion_coverage_mercury(building *b)
{
    b->data.house.temple_mercury = MAX_COVERAGE;
}

static void religion_coverage_mars(building *b)
{
    b->data.house.temple_mars = MAX_COVERAGE;
}

static void religion_coverage_venus(building *b)
{
    b->data.house.temple_venus = MAX_COVERAGE;
}

static void religion_coverage_pantheon(building *b)
{
    b->house_pantheon_access = MAX_COVERAGE;
}

static void school_coverage(building *b)
{
    b->data.house.school = MAX_COVERAGE;
}

static void academy_coverage(building *b)
{
    b->data.house.academy = MAX_COVERAGE;
}

static void library_coverage(building *b)
{
    b->data.house.library = MAX_COVERAGE;
}

static void barber_coverage(building *b)
{
    b->data.house.barber = MAX_COVERAGE;
}

static void clinic_coverage(building *b)
{
    b->data.house.clinic = MAX_COVERAGE;
}

static void hospital_coverage(building *b)
{
    b->data.house.hospital = MAX_COVERAGE;
}

static void cart_pusher_sickness(building *b, int sickness_dest)
{
    if (!b->sickness_level) {
        b->sickness_level = static_cast<unsigned char>(1 + (sickness_dest / 10));
    }
}

static int provide_missionary_coverage(int x, int y)
{
    int x_min, y_min, x_max, y_max;
    map_grid_get_area(x, y, 1, 4, &x_min, &y_min, &x_max, &y_max);
    for (int yy = y_min; yy <= y_max; yy++) {
        for (int xx = x_min; xx <= x_max; xx++) {
            if (Building *native_building = service_building_at(map_grid_offset(xx, yy))) {
                building *b = service_building_record(*native_building);
                if (native_building->matches("native_hut") ||
                    native_building->matches("native_hut_alt") ||
                    native_building->matches("native_meeting") ||
                    native_building->matches("native_watchtower")) {
                    b->sentiment.native_anger = 0;
                }
            }
        }
    }
    return 1;
}

static int tourist_visit(int x, int y, Figure *f, void (*callback)(building *, Figure *))
{
    int serviced = 0;
    int x_min, y_min, x_max, y_max;
    map_grid_get_area(x, y, 1, 2, &x_min, &y_min, &x_max, &y_max);
    for (int yy = y_min; yy <= y_max; yy++) {
        for (int xx = x_min; xx <= x_max; xx++) {
            int grid_offset = map_grid_offset(xx, yy);
            if (Building *building_object = service_building_at(grid_offset)) {
                building *b = service_building_record(*building_object);
                callback(b, f);
            }
        }
    }
    return serviced;
}

static void tourist_spend(building *b, Figure *f)
{
    int can_pay = 0;
    if (!b->is_tourism_venue || b->tourism_disabled) {
        return;
    }

    Building *building = service_building_for_record(b);
    if (building && building->matches("hippodrome")) {
        b = building_main(b);
    }
    for (int i = 0; i <= 12; ++i) {
        if (f->tourist.visited_building_type_ids[i]) {
            if (f->tourist.visited_building_type_ids[i] == b->type) {
                if (f->tourist.ticks_since_last_visited_id[i] >= TOURISM_COOLDOWN) {
                    can_pay = 1;
                    f->tourist.ticks_since_last_visited_id[i] = 0;
                }
                break;
            }
        } else {
            f->tourist.visited_building_type_ids[i] = b->type;
            can_pay = 1;
            break;
        }
    }

    if (can_pay) {
        int amount = b->tourism_income;
        f->tourist.tourist_money_spent = static_cast<unsigned short>(f->tourist.tourist_money_spent + amount);
        b->tourism_income_this_year = static_cast<unsigned char>(b->tourism_income_this_year + amount);
        city_finance_treasury_add_miscellaneous(amount);
    }
}

static int provide_service(int x, int y, int *data, void (*callback)(building *, int *))
{
    int serviced = 0;
    int x_min, y_min, x_max, y_max;
    map_grid_get_area(x, y, 1, 2, &x_min, &y_min, &x_max, &y_max);
    for (int yy = y_min; yy <= y_max; yy++) {
        for (int xx = x_min; xx <= x_max; xx++) {
            int grid_offset = map_grid_offset(xx, yy);
            if (Building *building_object = service_building_at(grid_offset)) {
                building *b = service_building_record(*building_object);
                callback(b, data);
                if (b->house_size && b->house_population > 0) {
                    serviced++;
                }
            }
        }
    }
    return serviced;
}

static void engineer_coverage(building *b, int *max_damage_seen)
{
    Building *building = service_building_for_record(b);
    if (building && building->matches("hippodrome")) {
        b = building_main(b);
    }
    if (b->damage_risk > *max_damage_seen) {
        *max_damage_seen = b->damage_risk;
    }
    b->damage_risk = 0;
}

static void prefect_coverage(building *b, int *min_happiness_seen)
{
    Building *building = service_building_for_record(b);
    if (building && building->matches("hippodrome")) {
        b = building_main(b);
    }
    b->fire_risk = 0;
    if (b->sentiment.house_happiness < *min_happiness_seen) {
        *min_happiness_seen = b->sentiment.house_happiness;
    }
}

static void tax_collector_coverage(building *b, int *max_tax_multiplier)
{
    if (b->house_size && b->house_population > 0) {
        Building *house = service_building_for_record(b);
        const model_house *house_model = house ? building_house_get_model(*house) : nullptr;
        int tax_multiplier = house_model ? house_model->tax_multiplier : 0;
        if (tax_multiplier > *max_tax_multiplier) {
            *max_tax_multiplier = tax_multiplier;
        }
        b->house_tax_coverage = 50;
    }
}

static void distribute_good(building *b, building *market, int stock_wanted, resource_type resource)
{
    Building *market_object = service_building_for_record(market);
    if (!market_object || !market_object->accepts_good(resource)) {
        return;
    }
    int amount_wanted = stock_wanted - b->resources[resource];
    if (market->resources[resource] > 0 && amount_wanted > 0) {
        if (amount_wanted <= market->resources[resource]) {
            b->resources[resource] = static_cast<short>(b->resources[resource] + amount_wanted);
            market->resources[resource] = static_cast<short>(market->resources[resource] - amount_wanted);
        } else {
            b->resources[resource] += market->resources[resource];
            market->resources[resource] = 0;
        }
    }
}

static void collect_offerings_from_house(building *house, building *temple)
{
    // offerings are generated, not removed from house stores
    if (house->days_since_offering >= MARS_OFFERING_FREQUENCY) {
        for (resource_type r = (RESOURCE_NONE + 1); r < RESOURCE_SLOT_COUNT; r = next_resource(r)) {
            if (!resource_is_inventory(r)) {
                continue;
            }
            if (house->resources[r]) {
                if (house->house_size == 1 && house->house_is_merged) {
                    temple->resources[r] += 2;
                } else {
                    temple->resources[r] += house->house_size;
                }
            }
            if (temple->resources[r] > 400) {
                temple->resources[r] = 400;
            }
        }
        house->days_since_offering = 0;
    }
}

static const model_house *house_evolution_target_model(Building house)
{
    building_type evolve_to = house.type ?
        house.type->housing_transition_type(building_type_registry_impl::HousingTransitionKind::EvolveTo) :
        BUILDING_NONE;
    if (evolve_to != BUILDING_NONE) {
        const building_type_registry_impl::BuildingType *evolve_definition =
            building_type_registry_impl::definition_for_type(evolve_to);
        if (evolve_definition && evolve_definition->housing_type()) {
            return &evolve_definition->housing_type()->model();
        }
    }

    int level = building_house_legacy_level(house);
    if (level < HOUSE_MIN) {
        level = HOUSE_MIN;
    } else if (level < HOUSE_LUXURY_PALACE) {
        level++;
    }
    return model_get_house(static_cast<house_level>(level));
}

static void distribute_market_resources(building *b, building *market)
{
    Building *house = service_building_for_record(b);
    Building *market_object = service_building_for_record(market);
    if (!house || !market_object) {
        return;
    }
    int max_food_stocks = 4 * b->house_highest_population;
    int food_types_stored_max = 0;
    for (resource_type r = (RESOURCE_NONE + 1); r < RESOURCE_SLOT_COUNT; r = next_resource(r)) {
        if (!resource_is_food(r)) {
            continue;
        }
        if (b->resources[r] >= max_food_stocks) {
            food_types_stored_max++;
        }
    }
    const model_house *model = house_evolution_target_model(*house);
    if (model->food_types) {
        for (resource_type r = (RESOURCE_NONE + 1); r < RESOURCE_SLOT_COUNT; r = next_resource(r)) {
            if (!resource_is_food(r) || b->resources[r] >= max_food_stocks ||
                !market_object->accepts_good(r)) {
                continue;
            }
            if (market->resources[r] >= max_food_stocks) {
                b->resources[r] = static_cast<short>(b->resources[r] + max_food_stocks);
                market->resources[r] = static_cast<short>(market->resources[r] - max_food_stocks);
                break;
            } else if (market->resources[r]) {
                b->resources[r] += market->resources[r];
                market->resources[r] = 0;
                break;
            }
        }
    }

    int goods_no = 8;

    // Venus base stockpile bonus
    if (grand_temple_for_god(GOD_VENUS, true)) {
        goods_no = 12;
    }

    if (model->pottery && market->accepted_goods[resource_pottery()]) {
        building_set_distribution_demand(market, resource_pottery(), 10);
        distribute_good(b, market, goods_no * model->pottery, resource_pottery());
    }
    if (model->furniture && market->accepted_goods[resource_furniture()]) {
        building_set_distribution_demand(market, resource_furniture(), 10);
        distribute_good(b, market, goods_no * model->furniture, resource_furniture());
    }
    if (model->oil && market->accepted_goods[resource_oil()]) {
        building_set_distribution_demand(market, resource_oil(), 10);
        distribute_good(b, market, goods_no * model->oil, resource_oil());
    }
    if (model->wine && market->accepted_goods[resource_wine()]) {
        building_set_distribution_demand(market, resource_wine(), 10);
        distribute_good(b, market, goods_no * model->wine, resource_wine());
    }
}

static int provide_market_goods(building *market, int x, int y)
{
    int serviced = 0;
    int x_min, y_min, x_max, y_max;
    map_grid_get_area(x, y, 1, 2, &x_min, &y_min, &x_max, &y_max);
    for (int yy = y_min; yy <= y_max; yy++) {
        for (int xx = x_min; xx <= x_max; xx++) {
            int grid_offset = map_grid_offset(xx, yy);
            if (Building *building_object = service_building_at(grid_offset)) {
                building *b = service_building_record(*building_object);
                if (b->house_size && b->house_population > 0) {
                    distribute_market_resources(b, market);
                    serviced++;
                }

            }
        }
    }
    return serviced;
}

static int provide_venus_wine_to_taverns(building *market, int x, int y)
{
    int serviced = 0;
    int x_min, y_min, x_max, y_max;
    map_grid_get_area(x, y, 1, 2, &x_min, &y_min, &x_max, &y_max);
    for (int yy = y_min; yy <= y_max; yy++) {
        for (int xx = x_min; xx <= x_max; xx++) {
            int grid_offset = map_grid_offset(xx, yy);
            if (Building *building_object = service_building_at(grid_offset)) {
                building *b = service_building_record(*building_object);
                if (building_object->matches("tavern")) {
                    int amount_wanted = 200 - b->resources[resource_wine()];
                    if (market->resources[resource_wine()] > 0 && amount_wanted > 0) {
                        if (amount_wanted <= market->resources[resource_wine()]) {
                            b->resources[resource_wine()] =
                                static_cast<short>(b->resources[resource_wine()] + amount_wanted);
                            market->resources[resource_wine()] =
                                static_cast<short>(market->resources[resource_wine()] - amount_wanted);
                        } else {
                            b->resources[resource_wine()] += market->resources[resource_wine()];
                            market->resources[resource_wine()] = 0;
                        }
                    }
                    serviced++;
                }
            }
        }
    }
    return serviced;
}

static int collect_offerings(building *market, int x, int y)
{
    int serviced = 0;
    int x_min, y_min, x_max, y_max;
    map_grid_get_area(x, y, 1, 2, &x_min, &y_min, &x_max, &y_max);
    for (int yy = y_min; yy <= y_max; yy++) {
        for (int xx = x_min; xx <= x_max; xx++) {
            int grid_offset = map_grid_offset(xx, yy);
            if (Building *building_object = service_building_at(grid_offset)) {
                building *b = service_building_record(*building_object);
                if (b->house_size && b->house_population > 0) {
                    collect_offerings_from_house(b, market);
                    serviced++;
                }
            }
        }
    }
    return serviced;
}

static Building *get_entertainment_building(const Figure *f)
{
    if (f->action_state == FIGURE_ACTION_94_ENTERTAINER_ROAMING ||
        f->action_state == FIGURE_ACTION_95_ENTERTAINER_RETURNING) {
        return f->building;
    }
    return f->destination_building;
}

static int provide_priest_service(Figure *f, int x, int y)
{
    Building *owner = f ? f->building : nullptr;
    building *owner_record = owner ? const_cast<building *>(owner->record()) : nullptr;
    const building_type_registry_impl::BuildingType *type = owner ? owner->type : nullptr;
    if (!type || !type->is_temple()) {
        return 0;
    }

        if (type->is_temple(GOD_ALL, building_type_registry_impl::ReligionTier::Grand)) {
        int houses_serviced = provide_culture(x, y, religion_coverage_ceres);
        provide_culture(x, y, religion_coverage_neptune);
        provide_culture(x, y, religion_coverage_mercury);
        provide_culture(x, y, religion_coverage_mars);
        provide_culture(x, y, religion_coverage_venus);
        provide_culture(x, y, religion_coverage_pantheon);
        return houses_serviced;
    }

    const int basic_temple = temple_is_basic(type);
    if (temple_serves_god(type, GOD_CERES)) {
        int houses_serviced = provide_culture(x, y, religion_coverage_ceres);
        if (basic_temple) {
            provide_market_goods(owner_record, x, y);
        }
        return houses_serviced;
    }
    if (temple_serves_god(type, GOD_NEPTUNE)) {
        return provide_culture(x, y, religion_coverage_neptune);
    }
    if (temple_serves_god(type, GOD_MERCURY)) {
        return provide_culture(x, y, religion_coverage_mercury);
    }
    if (temple_serves_god(type, GOD_MARS)) {
        if (basic_temple && building_monument_gt_module_is_active(MARS_MODULE_1_MESS_HALL) &&
            city_buildings_get_mess_hall()) {
            collect_offerings(owner_record, x, y);
        }
        return provide_culture(x, y, religion_coverage_mars);
    }
    if (temple_serves_god(type, GOD_VENUS)) {
        int houses_serviced = provide_culture(x, y, religion_coverage_venus);
        if (basic_temple) {
            provide_market_goods(owner_record, x, y);
            if (building_monument_gt_module_is_active(VENUS_MODULE_1_DISTRIBUTE_WINE)) {
                provide_venus_wine_to_taverns(owner_record, x, y);
            }
        }
        return houses_serviced;
    }
    return 0;
}

int figure_service_provide_coverage(Figure *f)
{
    int houses_serviced = 0;
    int x = f->x;
    int y = f->y;
    building *b;
    building *owner_record = f->building ? const_cast<building *>(f->building->record()) : nullptr;
    building *destination_record = f->destination_building ?
        const_cast<building *>(f->destination_building->record()) :
        nullptr;
    switch (f->type) {
        case FIGURE_LABOR_SEEKER:
            houses_serviced = provide_culture(x, y, labor_seeker_coverage);
            break;
        case FIGURE_TAX_COLLECTOR:
        {
            int max_tax_rate = 0;
            houses_serviced = provide_service(x, y, &max_tax_rate, tax_collector_coverage);
            f->min_max_seen = static_cast<unsigned char>(max_tax_rate);
            break;
        }
        case FIGURE_MARKET_TRADER:
            houses_serviced = provide_market_goods(owner_record, x, y);
            break;
        case FIGURE_MARKET_SUPPLIER:
            if (!config_get(CONFIG_GP_CH_NO_SUPPLIER_DISTRIBUTION)) {
                houses_serviced = provide_market_goods(owner_record, x, y);
            }
            break;
        case FIGURE_BATHHOUSE_WORKER:
            houses_serviced = provide_culture(x, y, bathhouse_coverage);
            break;
        case FIGURE_SCHOOL_CHILD:
            houses_serviced = provide_culture(x, y, school_coverage);
            break;
        case FIGURE_TEACHER:
            houses_serviced = provide_culture(x, y, academy_coverage);
            break;
        case FIGURE_LIBRARIAN:
            houses_serviced = provide_culture(x, y, library_coverage);
            break;
        case FIGURE_BARBER:
            houses_serviced = provide_culture(x, y, barber_coverage);
            break;
        case FIGURE_DOCTOR:
            houses_serviced = provide_culture(x, y, clinic_coverage);
            break;
        case FIGURE_SURGEON:
            houses_serviced = provide_culture(x, y, hospital_coverage);
            break;
        case FIGURE_WAREHOUSEMAN:
        case FIGURE_DOCKER:
        case FIGURE_CART_PUSHER:
        case FIGURE_DEPOT_CART_PUSHER:
        case FIGURE_NATIVE_TRADER:
        case FIGURE_LIGHTHOUSE_SUPPLIER:
        {
            b = owner_record;
            building *dest_b = destination_record;
            if (!b || !dest_b) {
                break;
            }

            if (b->sickness_level || dest_b->sickness_level) {
                provide_sickness(x, y, cart_pusher_sickness, dest_b->sickness_level);
            }
            break;
        }
        case FIGURE_MISSIONARY:
            houses_serviced = provide_missionary_coverage(x, y);
            break;
        case FIGURE_PRIEST:
            houses_serviced = provide_priest_service(f, x, y);
            break;
        case FIGURE_ACTOR:
        {
            Building *venue = get_entertainment_building(f);
            b = venue ? const_cast<building *>(venue->record()) : nullptr;
            if (venue && venue->type && venue->type->is_theater()) {
                houses_serviced = provide_culture(x, y, theater_coverage);
            } else if (venue && venue->matches("amphitheater") && b) {
                houses_serviced = provide_entertainment(x, y,
                    b->data.entertainment.days1 ? 2 : 1, amphitheater_coverage);
            }
            break;
        }
        case FIGURE_GLADIATOR:
        {
            Building *venue = get_entertainment_building(f);
            b = venue ? const_cast<building *>(venue->record()) : nullptr;
            if (venue && venue->matches("amphitheater") && b) {
                houses_serviced = provide_entertainment(x, y,
                    b->data.entertainment.days2 ? 2 : 1, amphitheater_coverage);
            } else if (venue && venue->matches("colosseum") && b) {
                houses_serviced = provide_entertainment(x, y,
                    b->data.entertainment.days1 ? 2 : 1, colosseum_coverage);
            } else if (venue && venue->matches("arena") && b) {
                houses_serviced = provide_entertainment(x, y,
                    b->data.entertainment.days1 ? 2 : 1, arena_coverage);
            }
            break;
        }
        case FIGURE_LION_TAMER:
        {
            Building *venue = get_entertainment_building(f);
            b = venue ? const_cast<building *>(venue->record()) : nullptr;
            if (venue && venue->matches("arena") && b) {
                houses_serviced = provide_entertainment(x, y,
                    b->data.entertainment.days1 ? 2 : 1, arena_coverage);
            } else if (b) {
                houses_serviced = provide_entertainment(x, y,
                    b->data.entertainment.days2 ? 2 : 1, colosseum_coverage);
            }
            break;
        }
        case FIGURE_CHARIOTEER:
            houses_serviced = provide_culture(x, y, hippodrome_coverage);
            break;
        case FIGURE_BARKEEP:
        {
            b = owner_record;
            if (!b) {
                break;
            }
            int tavern_goods = 0;
            if (b->resources[resource_wine()]) {
                tavern_goods = 1;
                if (b->resources[resource_meat()] || b->resources[resource_fish()]) {
                    tavern_goods = 2;
                }
            }
            houses_serviced = provide_entertainment(x, y, tavern_goods, tavern_coverage);
            break;
        }
        case FIGURE_ENGINEER:
        case FIGURE_WORK_CAMP_ARCHITECT:
        {
            int max_damage = 0;
            houses_serviced = provide_service(x, y, &max_damage, engineer_coverage);
            if (max_damage > f->min_max_seen) {
                f->min_max_seen = static_cast<unsigned char>(max_damage);
            } else if (f->min_max_seen <= 10) {
                f->min_max_seen = 0;
            } else {
                f->min_max_seen -= 10;
            }
            break;
        }
        case FIGURE_PREFECT:
        {
            int min_happiness = 100;
            houses_serviced = provide_service(x, y, &min_happiness, prefect_coverage);
            f->min_max_seen = static_cast<unsigned char>(min_happiness);
            break;
        }
        case FIGURE_RIOTER:
            if (f->terrain_usage == TERRAIN_USAGE_ENEMY) {
                if (figure_rioter_collapse_building(f) == 1) {
                    return 1;
                }
            }
            break;
        case FIGURE_TOURIST:
            tourist_visit(x, y, f, tourist_spend);
            break;
    }
    if (houses_serviced > 0 && owner_record) {
        owner_record->houses_covered = static_cast<short>(owner_record->houses_covered + houses_serviced);
        if (owner_record->houses_covered > 300) {
            owner_record->houses_covered = 300;
        }
    }
    return 0;
}
