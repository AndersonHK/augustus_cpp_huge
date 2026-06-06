#include "building/building_record.h"
#include "culture.h"

#include "building/building.h"
#include "building/building_type_api.h"
#include "building/count.h"
#include "building/house.h"
#include "building/monument.h"
#include "city/constants.h"
#include "city/data_private.h"
#include "city/entertainment.h"
#include "city/festival.h"
#include "city/population.h"
#include "core/calc.h"

#define LARARIUM_COVERAGE 10
#define SHRINE_COVERAGE 50
#define SMALL_TEMPLE_COVERAGE 750
#define LARGE_TEMPLE_COVERAGE 3000
#define ORACLE_COVERAGE 500
#define LARGE_ORACLE_COVERAGE 750
#define PANTHEON_COVERAGE 1500
#define GRAND_TEMPLE_COVERAGE 5000

struct religion_building_texts {
    const char *shrine;
    const char *small_temple;
    const char *large_temple;
    const char *grand_temple;
};

static building_type runtime_type(const char *text_id)
{
    if (!text_id) {
        return BUILDING_NONE;
    }
    return building_type_registry_runtime_id_from_text(text_id);
}

static int active_count(const char *text_id)
{
    building_type type = runtime_type(text_id);
    return type == BUILDING_NONE ? 0 : building_count_active(type);
}

static int total_count(const char *text_id)
{
    building_type type = runtime_type(text_id);
    return type == BUILDING_NONE ? 0 : building_count_total(type);
}

static int upgraded_count(const char *text_id)
{
    building_type type = runtime_type(text_id);
    return type == BUILDING_NONE ? 0 : building_count_upgraded(type);
}

static int working_count(const char *text_id)
{
    building_type type = runtime_type(text_id);
    return type == BUILDING_NONE ? 0 : building_monument_working(type);
}

static religion_building_texts religion_buildings_for_god(god_type god)
{
    switch (god) {
        case GOD_CERES:
            return { "shrine_ceres", "small_temple_ceres", "large_temple_ceres", "grand_temple_ceres" };
        case GOD_NEPTUNE:
            return { "shrine_neptune", "small_temple_neptune", "large_temple_neptune", "grand_temple_neptune" };
        case GOD_MERCURY:
            return { "shrine_mercury", "small_temple_mercury", "large_temple_mercury", "grand_temple_mercury" };
        case GOD_MARS:
            return { "shrine_mars", "small_temple_mars", "large_temple_mars", "grand_temple_mars" };
        case GOD_VENUS:
            return { "shrine_venus", "small_temple_venus", "large_temple_venus", "grand_temple_venus" };
        default:
            return {};
    }
}

static int religion_person_coverage(god_type god, int larariums, int oracles, int nymphaeums,
    int small_mausoleums, int large_mausoleums)
{
    religion_building_texts buildings = religion_buildings_for_god(god);
    return
        LARARIUM_COVERAGE * larariums +
        ORACLE_COVERAGE * (oracles + small_mausoleums) +
        LARGE_ORACLE_COVERAGE * (nymphaeums + large_mausoleums) +
        SHRINE_COVERAGE * total_count(buildings.shrine) +
        SMALL_TEMPLE_COVERAGE * active_count(buildings.small_temple) +
        LARGE_TEMPLE_COVERAGE * active_count(buildings.large_temple) +
        PANTHEON_COVERAGE * active_count("pantheon") +
        GRAND_TEMPLE_COVERAGE * active_count(buildings.grand_temple);
}

static int theater_active_count(void)
{
    building_type theater = building_type_registry_theater_type();
    return theater == BUILDING_NONE ? 0 : building_count_active(theater);
}

static int theater_upgraded_count(void)
{
    building_type theater = building_type_registry_theater_type();
    return theater == BUILDING_NONE ? 0 : building_count_upgraded(theater);
}

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
    coverage.tavern = top(calc_percentage(city_culture_get_tavern_person_coverage(), population));
    coverage.theater = top(calc_percentage(city_culture_get_theatre_person_coverage(), population));
    coverage.amphitheater = top(calc_percentage(city_culture_get_ampitheatre_person_coverage(), population));
    coverage.arena = top(calc_percentage(city_culture_get_arena_person_coverage(), population));

    if (working_count("hippodrome") <= 0) {
        coverage.hippodrome = 0;
    } else {
        coverage.hippodrome = 100;
    }

    if (working_count("colosseum") <= 0) {
        coverage.colosseum = 0;
    } else {
        coverage.colosseum = 100;
    }

    // religion
    int oracles = active_count("oracle");
    int larariums = total_count("lararium");
    int nymphaeums = active_count("nymphaeum");
    int small_mausoleums = active_count("small_mausoleum");
    int large_mausoleums = active_count("large_mausoleum");
    for (int god = GOD_CERES; god <= GOD_VENUS; god++) {
        coverage.religion[god] = top(calc_percentage(
            religion_person_coverage(static_cast<god_type>(god), larariums, oracles, nymphaeums,
                small_mausoleums, large_mausoleums),
            population));
    }
    coverage.oracle = top(calc_percentage(ORACLE_COVERAGE * oracles, population));

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
        city_culture_get_school_person_coverage(), city_population_school_age()));
    coverage.library = top(calc_percentage(
        city_culture_get_library_person_coverage(), population));
    coverage.academy = top(calc_percentage(
        city_culture_get_academy_person_coverage(), city_population_academy_age()));

    // health
    coverage.hospital = top(calc_percentage(
        HOSPITAL_COVERAGE * active_count("hospital"), population));
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
        if (building_house_is_active(Building(b))) {
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

int city_culture_get_theatre_person_coverage(void)
{
    return THEATER_COVERAGE * theater_active_count() + THEATER_UPGRADE_BONUS_COVERAGE * theater_upgraded_count();
}

int city_culture_get_school_person_coverage(void)
{
    return SCHOOL_COVERAGE * active_count("school") + SCHOOL_UPGRADE_BONUS_COVERAGE * upgraded_count("school");
}

int city_culture_get_library_person_coverage(void)
{
    return LIBRARY_COVERAGE * active_count("library") + LIBRARY_UPGRADE_BONUS_COVERAGE * upgraded_count("library");
}

int city_culture_get_academy_person_coverage(void)
{
    return ACADEMY_COVERAGE * active_count("academy") + ACADEMY_UPGRADE_BONUS_COVERAGE * upgraded_count("academy");
}

int city_culture_get_tavern_person_coverage(void)
{
    return TAVERN_COVERAGE * active_count("tavern") + TAVERN_UPGRADE_BONUS_COVERAGE * upgraded_count("tavern");
}

int city_culture_get_ampitheatre_person_coverage(void)
{
    return AMPHITHEATER_COVERAGE * active_count("amphitheater") + AMPHITHEATER_UPGRADE_BONUS_COVERAGE * upgraded_count("amphitheater");
}

int city_culture_get_arena_person_coverage(void)
{
    return ARENA_COVERAGE * active_count("arena") + ARENA_UPGRADE_BONUS_COVERAGE * upgraded_count("arena");
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
