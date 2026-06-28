#include "building/building_record.h"
#include "entertainment.h"

#include "building/building.h"
#include "city/data_private.h"

#include <string_view>

static void accumulate_venue_shows(std::string_view attr, int no_show_weight, int has_second_show,
    int *shows, int *no_shows_weighted)
{
    Building::for_each([&](Building *venue) {
        building *b = const_cast<building *>(venue->record());
        if (!b || b->state != BUILDING_STATE_IN_USE || !venue->type || !venue->type->attr_is(attr)) {
            return;
        }
        if (b->data.entertainment.days1) {
            (*shows)++;
        } else {
            *no_shows_weighted += no_show_weight;
        }
        if (!has_second_show) {
            return;
        }
        if (b->data.entertainment.days2) {
            (*shows)++;
        } else {
            *no_shows_weighted += no_show_weight;
        }
    });
}

int city_entertainment_theater_shows(void)
{
    return city_data.entertainment.theater_shows;
}

int city_entertainment_amphitheater_shows(void)
{
    return city_data.entertainment.amphitheater_shows;
}

int city_entertainment_arena_shows(void)
{
    return city_data.entertainment.arena_shows;
}

int city_entertainment_colosseum_shows(void)
{
    return city_data.entertainment.colosseum_shows;
}

int city_entertainment_hippodrome_shows(void)
{
    return city_data.entertainment.hippodrome_shows;
}

void city_entertainment_set_hippodrome_has_race(int has_race)
{
    city_data.entertainment.hippodrome_has_race = has_race;
}

int city_entertainment_hippodrome_has_race(void)
{
    return city_data.entertainment.hippodrome_has_race;
}

int city_entertainment_venue_needing_shows(void)
{
    return city_data.entertainment.venue_needing_shows;
}

void city_entertainment_calculate_shows(void)
{
    city_data.entertainment.theater_shows = 0;
    city_data.entertainment.theater_no_shows_weighted = 0;
    city_data.entertainment.amphitheater_shows = 0;
    city_data.entertainment.amphitheater_no_shows_weighted = 0;
    city_data.entertainment.arena_shows = 0;
    city_data.entertainment.arena_no_shows_weighted = 0;
    city_data.entertainment.colosseum_shows = 0;
    city_data.entertainment.colosseum_no_shows_weighted = 0;
    city_data.entertainment.hippodrome_shows = 0;
    city_data.entertainment.hippodrome_no_shows_weighted = 0;
    city_data.entertainment.venue_needing_shows = 0;

    accumulate_venue_shows("theater", 1, 0,
        &city_data.entertainment.theater_shows,
        &city_data.entertainment.theater_no_shows_weighted);
    accumulate_venue_shows("amphitheater", 2, 1,
        &city_data.entertainment.amphitheater_shows,
        &city_data.entertainment.amphitheater_no_shows_weighted);
    accumulate_venue_shows("arena", 3, 1,
        &city_data.entertainment.arena_shows,
        &city_data.entertainment.arena_no_shows_weighted);
    accumulate_venue_shows("colosseum", 3, 1,
        &city_data.entertainment.colosseum_shows,
        &city_data.entertainment.colosseum_no_shows_weighted);
    accumulate_venue_shows("hippodrome", 100, 0,
        &city_data.entertainment.hippodrome_shows,
        &city_data.entertainment.hippodrome_no_shows_weighted);

    int worst_shows = 0;
    if (city_data.entertainment.theater_no_shows_weighted > worst_shows) {
        worst_shows = city_data.entertainment.theater_no_shows_weighted;
        city_data.entertainment.venue_needing_shows = 1;
    }
    if (city_data.entertainment.amphitheater_no_shows_weighted > worst_shows) {
        worst_shows = city_data.entertainment.amphitheater_no_shows_weighted;
        city_data.entertainment.venue_needing_shows = 2;
    }
    if (city_data.entertainment.colosseum_no_shows_weighted > worst_shows) {
        worst_shows = city_data.entertainment.colosseum_no_shows_weighted;
        city_data.entertainment.venue_needing_shows = 3;
    }
    if (city_data.entertainment.hippodrome_no_shows_weighted > worst_shows) {
        city_data.entertainment.venue_needing_shows = 4;
    }
}

int city_entertainment_show_message_colosseum(void)
{
    if (!city_data.entertainment.colosseum_message_shown) {
        city_data.entertainment.colosseum_message_shown = 1;
        return 1;
    } else {
        return 0;
    }
}

int city_entertainment_show_message_hippodrome(void)
{
    if (!city_data.entertainment.hippodrome_message_shown) {
        city_data.entertainment.hippodrome_message_shown = 1;
        return 1;
    } else {
        return 0;
    }
}


