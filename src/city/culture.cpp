#include "building/building_record.h"
#include "culture.h"

#include "building/building.h"
#include "building/culture_module.h"
#include "building/building_type_registry_internal.h"
#include "building/house.h"
#include "building/monument.h"
#include "building/religion.h"
#include "city/constants.h"
#include "city/data_private.h"
#include "city/entertainment.h"
#include "city/festival.h"
#include "city/population.h"
#include "core/calc.h"

#include <cstring>
#include <vector>

static struct {
    int theater;
    int amphitheater;
    int colosseum;
    int hippodrome;
    int hospital;
    int school;
    int academy;
    int library;
    int religion[5];
    int oracle;
    int tavern;
    int arena;
} coverage;

struct culture_module_capacity {
    int theater;
    int amphitheater;
    int colosseum;
    int colosseum_presence;
    int hippodrome;
    int hospital;
    int school;
    int academy;
    int library;
    int religion[5];
    int oracle;
    int tavern;
    int arena;
};

static culture_module_capacity module_capacity;
static std::vector<culture_module_capacity> module_capacity_by_building;

static void clear_capacity(culture_module_capacity &capacity)
{
    std::memset(&capacity, 0, sizeof(capacity));
}

static void add_capacity(culture_module_capacity &target, const culture_module_capacity &source, int sign)
{
    target.theater += sign * source.theater;
    target.amphitheater += sign * source.amphitheater;
    target.colosseum += sign * source.colosseum;
    target.colosseum_presence += sign * source.colosseum_presence;
    target.hippodrome += sign * source.hippodrome;
    target.hospital += sign * source.hospital;
    target.school += sign * source.school;
    target.academy += sign * source.academy;
    target.library += sign * source.library;
    target.oracle += sign * source.oracle;
    target.tavern += sign * source.tavern;
    target.arena += sign * source.arena;
    for (int god = GOD_CERES; god <= GOD_VENUS; god++) {
        target.religion[god] += sign * source.religion[god];
    }
}

static int is_main_building(const building *b)
{
    return b && building_main(b) == b;
}

static int is_total_counted(const building *b)
{
    return b && is_main_building(b) &&
        (b->state == BUILDING_STATE_IN_USE || b->state == BUILDING_STATE_CREATED ||
            b->state == BUILDING_STATE_MOTHBALLED);
}

static int is_upgrade_bonus_counted(const building *b)
{
    return b && is_main_building(b) &&
        (b->state == BUILDING_STATE_IN_USE || b->state == BUILDING_STATE_CREATED) &&
        b->upgrade_level > 0;
}

static int is_working_counted(const building *b)
{
    if (!b || !is_main_building(b) || b->state != BUILDING_STATE_IN_USE) {
        return 0;
    }
    if (building_monument_get_id(b->type)) {
        return building_monument_working(b->type) ? 1 : 0;
    }
    return building_is_active(b);
}

static int module_counted_for_building(const building *b, building_type_registry_impl::CultureModuleCountMode count_mode)
{
    switch (count_mode) {
        case building_type_registry_impl::CultureModuleCountMode::Total:
            return is_total_counted(b);
        case building_type_registry_impl::CultureModuleCountMode::Working:
            return is_working_counted(b);
        case building_type_registry_impl::CultureModuleCountMode::Active:
        default:
            return b && is_main_building(b) && building_is_active(b);
    }
}

static void add_religion_capacity(culture_module_capacity &capacity,
    const building_type_registry_impl::BuildingType &definition,
    int amount)
{
    const building_type_registry_impl::Religion *religion = definition.religion();
    if (!religion) {
        for (int god = GOD_CERES; god <= GOD_VENUS; god++) {
            capacity.religion[god] += amount;
        }
        return;
    }
    for (int god = GOD_CERES; god <= GOD_VENUS; god++) {
        if (religion->has_god(static_cast<god_type>(god))) {
            capacity.religion[god] += amount;
        }
    }
}

static void add_module_capacity_by_type(culture_module_capacity &capacity,
    const building_type_registry_impl::BuildingType &definition,
    building_type_registry_impl::CultureModuleType type,
    int amount)
{
    switch (type) {
        case building_type_registry_impl::CultureModuleType::Theater:
            capacity.theater += amount;
            break;
        case building_type_registry_impl::CultureModuleType::Amphitheater:
            capacity.amphitheater += amount;
            break;
        case building_type_registry_impl::CultureModuleType::Arena:
            capacity.arena += amount;
            break;
        case building_type_registry_impl::CultureModuleType::Colosseum:
            capacity.colosseum += amount;
            break;
        case building_type_registry_impl::CultureModuleType::ColosseumPresence:
            capacity.colosseum_presence += amount;
            break;
        case building_type_registry_impl::CultureModuleType::Hippodrome:
            capacity.hippodrome += amount;
            break;
        case building_type_registry_impl::CultureModuleType::Tavern:
            capacity.tavern += amount;
            break;
        case building_type_registry_impl::CultureModuleType::School:
            capacity.school += amount;
            break;
        case building_type_registry_impl::CultureModuleType::Library:
            capacity.library += amount;
            break;
        case building_type_registry_impl::CultureModuleType::Academy:
            capacity.academy += amount;
            break;
        case building_type_registry_impl::CultureModuleType::Hospital:
            capacity.hospital += amount;
            break;
        case building_type_registry_impl::CultureModuleType::Oracle:
            capacity.oracle += amount;
            for (int god = GOD_CERES; god <= GOD_VENUS; god++) {
                capacity.religion[god] += amount;
            }
            break;
        case building_type_registry_impl::CultureModuleType::Temple:
            add_religion_capacity(capacity, definition, amount);
            break;
        case building_type_registry_impl::CultureModuleType::None:
        default:
            break;
    }
}

static culture_module_capacity calculate_building_module_capacity(const building *b)
{
    culture_module_capacity capacity = {};
    if (!b || !is_main_building(b)) {
        return capacity;
    }

    const building_type_registry_impl::BuildingType *definition =
        building_type_registry_impl::definition_for_type(b->type);
    if (!definition || !definition->has_culture_modules()) {
        return capacity;
    }

    for (const building_type_registry_impl::BuildingCultureModule &module : definition->culture_modules()) {
        if (!module.module || !module_counted_for_building(b, module.count_mode)) {
            continue;
        }
        add_module_capacity_by_type(capacity, *definition, module.module->type(), module.capacity);
        if (module.upgrade_bonus_capacity > 0 && is_upgrade_bonus_counted(b)) {
            add_module_capacity_by_type(capacity, *definition, module.module->type(), module.upgrade_bonus_capacity);
        }
    }

    return capacity;
}

int city_culture_coverage_tavern(void)
{
    return coverage.tavern;
}

int city_culture_coverage_theater(void)
{
    return coverage.theater;
}

int city_culture_coverage_amphitheater(void)
{
    return coverage.amphitheater;
}

int city_culture_coverage_arena(void)
{
    return coverage.arena;
}

int city_culture_coverage_colosseum(void)
{
    return coverage.colosseum;
}

int city_culture_coverage_hippodrome(void)
{
    return coverage.hippodrome;
}

int city_culture_coverage_average_entertainment(void)
{
    return (coverage.hippodrome + coverage.colosseum + coverage.amphitheater + coverage.theater + coverage.tavern) / 5;
}

int city_culture_coverage_religion(god_type god)
{
    return coverage.religion[god];
}

int city_culture_coverage_school(void)
{
    return coverage.school;
}

int city_culture_coverage_library(void)
{
    return coverage.library;
}

int city_culture_coverage_academy(void)
{
    return coverage.academy;
}

int city_culture_coverage_hospital(void)
{
    return coverage.hospital;
}

int city_culture_module_capacity(building_type_registry_impl::CultureModuleType type)
{
    switch (type) {
        case building_type_registry_impl::CultureModuleType::Theater:
            return module_capacity.theater;
        case building_type_registry_impl::CultureModuleType::Amphitheater:
            return module_capacity.amphitheater;
        case building_type_registry_impl::CultureModuleType::Arena:
            return module_capacity.arena;
        case building_type_registry_impl::CultureModuleType::Colosseum:
            return module_capacity.colosseum;
        case building_type_registry_impl::CultureModuleType::ColosseumPresence:
            return module_capacity.colosseum_presence;
        case building_type_registry_impl::CultureModuleType::Hippodrome:
            return module_capacity.hippodrome;
        case building_type_registry_impl::CultureModuleType::Tavern:
            return module_capacity.tavern;
        case building_type_registry_impl::CultureModuleType::School:
            return module_capacity.school;
        case building_type_registry_impl::CultureModuleType::Library:
            return module_capacity.library;
        case building_type_registry_impl::CultureModuleType::Academy:
            return module_capacity.academy;
        case building_type_registry_impl::CultureModuleType::Hospital:
            return module_capacity.hospital;
        case building_type_registry_impl::CultureModuleType::Oracle:
            return module_capacity.oracle;
        case building_type_registry_impl::CultureModuleType::Temple:
        case building_type_registry_impl::CultureModuleType::None:
        default:
            return 0;
    }
}

void city_culture_clear_module_capacity_cache(void)
{
    clear_capacity(module_capacity);
    module_capacity_by_building.clear();
}

void city_culture_remove_building_module_capacity(const building *b)
{
    if (!b || b->id <= 0 || b->id >= static_cast<int>(module_capacity_by_building.size())) {
        return;
    }

    add_capacity(module_capacity, module_capacity_by_building[b->id], -1);
    clear_capacity(module_capacity_by_building[b->id]);
}

void city_culture_add_building_module_capacity(const building *b)
{
    if (!b || b->id <= 0) {
        return;
    }

    if (b->id >= static_cast<int>(module_capacity_by_building.size())) {
        module_capacity_by_building.resize(b->id + 1);
    }

    city_culture_remove_building_module_capacity(b);
    module_capacity_by_building[b->id] = calculate_building_module_capacity(b);
    add_capacity(module_capacity, module_capacity_by_building[b->id], 1);
}

void city_culture_refresh_building_module_capacity(const building *b)
{
    city_culture_add_building_module_capacity(b);
}

void city_culture_rebuild_module_capacity_cache(void)
{
    city_culture_clear_module_capacity_cache();
    for (int id = 1; id < building_count(); id++) {
        city_culture_add_building_module_capacity(building_get(id));
    }
}

int city_culture_average_education(void)
{
    return city_data.culture.average_education;
}

int city_culture_average_entertainment(void)
{
    return city_data.culture.average_entertainment;
}

int city_culture_average_health(void)
{
    return city_data.culture.average_health;
}

static int top(int input)
{
    return input > 100 ? 100 : input;
}

void city_culture_update_coverage(void)
{
    int population = city_data.population.population;

    // entertainment
    coverage.tavern = top(calc_percentage(module_capacity.tavern, population));
    coverage.theater = top(calc_percentage(module_capacity.theater, population));
    coverage.amphitheater = top(calc_percentage(module_capacity.amphitheater, population));
    coverage.arena = top(calc_percentage(module_capacity.arena, population));

    if (module_capacity.hippodrome <= 0) {
        coverage.hippodrome = 0;
    } else {
        coverage.hippodrome = 100;
    }

    if (module_capacity.colosseum_presence > 0) {
        coverage.colosseum = 100;
    } else if (module_capacity.colosseum <= 0) {
        coverage.colosseum = 0;
    } else {
        coverage.colosseum = top(calc_percentage(module_capacity.colosseum, population));
    }

    // religion
    for (int god = GOD_CERES; god <= GOD_VENUS; god++) {
        coverage.religion[god] = top(calc_percentage(module_capacity.religion[god], population));
    }
    coverage.oracle = top(calc_percentage(module_capacity.oracle, population));

    city_data.culture.religion_coverage =
        coverage.religion[GOD_CERES] +
        coverage.religion[GOD_NEPTUNE] +
        coverage.religion[GOD_MERCURY] +
        coverage.religion[GOD_MARS] +
        coverage.religion[GOD_VENUS];
    city_data.culture.religion_coverage /= 5;

    // education
    city_population_calculate_educational_age();

    coverage.school = top(calc_percentage(
        module_capacity.school, city_population_school_age()));
    coverage.library = top(calc_percentage(
        module_capacity.library, population));
    coverage.academy = top(calc_percentage(
        module_capacity.academy, city_population_academy_age()));

    // health
    coverage.hospital = top(calc_percentage(module_capacity.hospital, population));
}

void city_culture_calculate(void)
{
    city_data.culture.average_entertainment = 0;
    city_data.culture.average_religion = 0;
    city_data.culture.average_education = 0;
    city_data.culture.average_health = 0;
    city_data.culture.average_desirability = 0;
    city_data.culture.population_with_venus_access = 0; //venus

    int num_houses = 0;
    for (int i = 1; i < building_count(); i++) {
        building *b = building_get(i);
        const Building house(b);
        if (building_house_is_active(house)) {
            num_houses++;
            city_data.culture.average_entertainment += b->data.house.entertainment;
            city_data.culture.average_religion += b->data.house.num_gods;
            city_data.culture.average_education += b->data.house.education;
            city_data.culture.average_health += b->data.house.health;
            city_data.culture.average_desirability += b->desirability;
            if (b->data.house.temple_venus) {
                city_data.culture.population_with_venus_access += b->house_population;
            }
        }
    }
    if (num_houses) {
        city_data.culture.average_entertainment /= num_houses;
        city_data.culture.average_religion /= num_houses;
        city_data.culture.average_education /= num_houses;
        city_data.culture.average_health /= num_houses;
        city_data.culture.average_desirability /= num_houses;
    }

    city_entertainment_calculate_shows();
    city_festival_calculate_costs();
}

void city_culture_save_state(buffer *buf)
{
    // Yes, hospital is saved twice
    buffer_write_i32(buf, coverage.theater);
    buffer_write_i32(buf, coverage.amphitheater);
    buffer_write_i32(buf, coverage.colosseum);
    buffer_write_i32(buf, coverage.hospital);
    buffer_write_i32(buf, coverage.hippodrome);
    for (int i = GOD_CERES; i <= GOD_VENUS; i++) {
        buffer_write_i32(buf, coverage.religion[i]);
    }
    buffer_write_i32(buf, coverage.oracle);
    buffer_write_i32(buf, coverage.school);
    buffer_write_i32(buf, coverage.library);
    buffer_write_i32(buf, coverage.academy);
    buffer_write_i32(buf, coverage.hospital);
}

void city_culture_load_state(buffer *buf)
{
    // Yes, hospital is saved twice
    coverage.theater = buffer_read_i32(buf);
    coverage.amphitheater = buffer_read_i32(buf);
    coverage.colosseum = buffer_read_i32(buf);
    coverage.hospital = buffer_read_i32(buf);
    coverage.hippodrome = buffer_read_i32(buf);
    for (int i = GOD_CERES; i <= GOD_VENUS; i++) {
        coverage.religion[i] = buffer_read_i32(buf);
    }
    coverage.oracle = buffer_read_i32(buf);
    coverage.school = buffer_read_i32(buf);
    coverage.library = buffer_read_i32(buf);
    coverage.academy = buffer_read_i32(buf);
    coverage.hospital = buffer_read_i32(buf);
}
