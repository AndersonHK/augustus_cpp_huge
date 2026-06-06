#include "building/building_record.h"
#include "entertainment.h"

#include "building/building.h"
#include "building/building_type_api.h"
#include "city/data_private.h"

static building_type runtime_type(const char *text_id)
{
    if (!text_id) {
        return BUILDING_NONE;
    }
    return building_type_registry_runtime_id_from_text(text_id);
}

static void accumulate_venue_shows(const char *text_id, int no_show_weight, int has_second_show,
    int *shows, int *no_shows_weighted)
{
    building_type type = runtime_type(text_id);
    if (type == BUILDING_NONE) {
        return;
    }
    for (building *b = building_first_of_type(type); b; b = b->next_of_type) {
        if (b->state != BUILDING_STATE_IN_USE) {
            continue;
        }
        if (b->data.entertainment.days1) {
            (*shows)++;
        } else {
            *no_shows_weighted += no_show_weight;
        }
        if (!has_second_show) {
            continue;
        }
        if (b->data.entertainment.days2) {
            (*shows)++;
        } else {
            *no_shows_weighted += no_show_weight;
        }
    }
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

    building_type theater = building_type_registry_theater_type();
    if (theater != BUILDING_NONE) {
        for (building *b = building_first_of_type(theater); b; b = b->next_of_type) {
            if (b->state != BUILDING_STATE_IN_USE) {
                continue;
            }
            if (b->data.entertainment.days1) {
                city_data.entertainment.theater_shows++;
            } else {
                city_data.entertainment.theater_no_shows_weighted++;
            }
        }
    }
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


