#include "building/entertainment.h"

#include "building/building_record.h"
#include "building/building_runtime.h"
#include "building/monument.h"
#include "city/entertainment.h"
#include "city/games.h"
#include "city/message.h"
#include "city/race_bet.h"
#include "figure/action.h"
#include "figure/figure.h"
#include "figure/figure_runtime_api.h"
#include "figure/movement.h"
#include "map/road_access.h"

#include <string_view>

BuildingEntertainment::BuildingEntertainment(const Building &venue)
    : venue_(venue)
{}

building *BuildingEntertainment::record() const
{
    return const_cast<::building *>(venue_.record());
}

int BuildingEntertainment::is_show_venue(const Building &building)
{
    const building_type_registry_impl::BuildingType *type = building.type;
    if (!type) {
        return 0;
    }
    const std::string_view attr(type->attr());
    return attr == "theater" || attr == "amphitheater" || attr == "arena" ||
        attr == "colosseum" || attr == "hippodrome";
}

int BuildingEntertainment::has_figure_of_types(figure_type primary_type, figure_type secondary_type) const
{
    building *venue_record = record();
    if (!venue_record || venue_record->figure_id <= 0) {
        return 0;
    }

    Figure *figure = Figure::get(venue_record->figure_id);
    if (figure && !figure->is_dead() && figure->building && figure->building->id == venue_.id &&
        (figure->type == primary_type || figure->type == secondary_type)) {
        return 1;
    }

    venue_record->figure_id = 0;
    return 0;
}

int BuildingEntertainment::has_figure_of_type(figure_type type) const
{
    return has_figure_of_types(type, FIGURE_NONE);
}

void BuildingEntertainment::attach_figure_to_venue(Figure *figure) const
{
    if (figure) {
        building_runtime *runtime = venue_.runtime_instance();
    figure->set_home_building(runtime ? &runtime->building : nullptr);
    }
}

void BuildingEntertainment::copy_problem_overlay_to_hippodrome_parts() const
{
    BuildingComposition *composition = venue_.Composition;
    if (!composition || !composition->complete()) {
        return;
    }
    Building *owner = composition->owner();
    building *owner_record = owner ? const_cast<building *>(owner->record()) : nullptr;
    if (!owner_record) {
        return;
    }

    composition->for_each_member([owner_record](Building &part) {
        building *part_record = const_cast<building *>(part.record());
        if (part_record) {
            part_record->show_on_problem_overlay = owner_record->show_on_problem_overlay;
        }
    });
}

int BuildingEntertainment::find_hippodrome_road(map_point &road) const
{
    building *venue_record = record();
    return venue_record && map_has_road_access_building(venue_record->x, venue_record->y, &road);
}

int BuildingEntertainment::find_venue_road(map_point &road) const
{
    building *venue_record = record();
    return venue_record && map_has_road_access_building(venue_record->x, venue_record->y, &road);
}

void BuildingEntertainment::run_labor_phase(const map_point &road) const
{
    if (building_runtime *runtime = venue_.runtime_instance()) {
        runtime->run_labor_phase_if_defined(road);
    }
}

int BuildingEntertainment::delay_has_elapsed(
    const std::vector<building_type_registry_impl::DelayBand> &delay_bands) const
{
    building *venue_record = record();
    if (!venue_record) {
        return 0;
    }

    building_runtime *runtime = venue_.runtime_instance();
    const int spawn_delay = runtime ? runtime->evaluate_delay(delay_bands) : 0;
    if (spawn_delay <= 0) {
        return 0;
    }

    venue_record->figure_spawn_delay++;
    if (venue_record->figure_spawn_delay <= spawn_delay) {
        return 0;
    }

    venue_record->figure_spawn_delay = 0;
    return 1;
}

Figure *BuildingEntertainment::create_charioteer(
    const map_point &road,
    const char *profile_id,
    int fallback_action) const
{
    Figure *figure = figure_runtime_create_profiled(
        FIGURE_CHARIOTEER,
        road.x,
        road.y,
        DIR_0_TOP,
        venue_,
        profile_id);
    if (figure) {
        return figure;
    }

    figure = Figure::create(FIGURE_CHARIOTEER, road.x, road.y, DIR_0_TOP);
    if (!figure) {
        return nullptr;
    }

    figure->action_state = static_cast<unsigned char>(fallback_action);
    attach_figure_to_venue(figure);
    figure_movement_init_roaming(figure);
    return figure;
}

Figure *BuildingEntertainment::create_roaming_figure(const map_point &road, figure_type type) const
{
    Figure *figure = Figure::create(type, road.x, road.y, DIR_0_TOP);
    if (!figure) {
        return nullptr;
    }

    figure->action_state = FIGURE_ACTION_94_ENTERTAINER_ROAMING;
    attach_figure_to_venue(figure);
    figure_movement_init_roaming(figure);
    return figure;
}

bool BuildingEntertainment::create_race_participants() const
{
    building *venue_record = record();
    const building_type_registry_impl::RaceDefinition *race = venue_.type && venue_.type->has_race() ? &venue_.type->race() : nullptr;
    if (!venue_record || !race || race->teams.empty()) {
        return false;
    }

    std::vector<Figure *> participants(race->teams.size(), nullptr);
    for (int team = 0; team < static_cast<int>(race->teams.size()); ++team) {
        const int lane = race->teams[team].lane;
        participants[team] = Figure::create(race->participant_figure,
            venue_record->x + race->spawn_x, venue_record->y + race->spawn_y + (lane & 1), DIR_2_RIGHT);
        if (!participants[team]) {
            for (Figure *horse : participants) {
                if (horse) {
                    horse->remove();
                }
            }
            return false;
        }
        participants[team]->action_state = FIGURE_ACTION_200_HIPPODROME_HORSE_CREATED;
        attach_figure_to_venue(participants[team]);
        participants[team]->resource_id = static_cast<unsigned char>(team);
        participants[team]->speed_multiplier = static_cast<unsigned char>(race->minimum_speed);
    }
    race_bet_start(static_cast<unsigned int>(venue_record->id), static_cast<int>(race->teams.size()));
    return true;
}

void BuildingEntertainment::spawn_race_participants()
{
    building *venue_record = record();
    building_runtime *runtime = venue_.runtime_instance();
    const building_type_registry_impl::RaceDefinition *race = venue_.type && venue_.type->has_race() ? &venue_.type->race() : nullptr;
    if (!venue_record || !runtime || !race || race_bet_is_active(static_cast<unsigned int>(venue_record->id))) {
        return;
    }

    const size_t counter_index = venue_.type->spawn_groups().size() + 1;
    if (!runtime->module_delay_has_elapsed(counter_index, race->start_delay_bands)) {
        return;
    }
    if (create_race_participants() && venue_.type->attr_is("hippodrome")) {
        post_hippodrome_message_if_active();
    }
}

void BuildingEntertainment::spawn_execution_lion_tamers(const map_point &road) const
{
    if (!city_games_executions_active()) {
        return;
    }

    for (int i = 0; i < 2; i++) {
        Figure *figure = Figure::create(FIGURE_LION_TAMER, road.x, road.y, DIR_0_TOP);
        if (figure) {
            figure->action_state = FIGURE_ACTION_230_LION_TAMERS_HUNTING_ENEMIES;
        }
    }
}

void BuildingEntertainment::post_hippodrome_message_if_active() const
{
    building *venue_record = record();
    if (venue_record &&
        venue_record->data.entertainment.days1 > 0 &&
        city_entertainment_show_message_hippodrome()) {
        city_message_post(1, MESSAGE_HIPPODROME_WORKING_NEW, 0, 0);
    }
}

void BuildingEntertainment::post_colosseum_message_if_active() const
{
    building *venue_record = record();
    if (venue_record &&
        (venue_record->data.entertainment.days1 > 0 || venue_record->data.entertainment.days2 > 0) &&
        city_entertainment_show_message_colosseum()) {
        city_message_post(1, MESSAGE_COLOSSEUM_WORKING_NEW, 0, 0);
    }
}

void BuildingEntertainment::spawn_hippodrome_service()
{
    if (building_runtime *runtime = venue_.runtime_instance()) {
        runtime->check_labor_problem();
    }

    building *venue_record = record();
    if (!venue_record || (venue_.Composition && venue_.Composition->is_child())) {
        return;
    }

    copy_problem_overlay_to_hippodrome_parts();
    if (has_figure_of_type(FIGURE_CHARIOTEER)) {
        return;
    }

    map_point road;
    if (!find_hippodrome_road(road)) {
        return;
    }

    run_labor_phase(road);
    static const std::vector<building_type_registry_impl::DelayBand> kHippodromeDelays = {
        { 100, 7 },
        { 75, 15 },
        { 50, 30 },
        { 25, 50 },
        { 1, 80 }
    };
    if (!delay_has_elapsed(kHippodromeDelays)) {
        return;
    }

    Figure *charioteer = create_charioteer(road, "hippodrome_service", FIGURE_ACTION_94_ENTERTAINER_ROAMING);
    if (!charioteer) {
        return;
    }
    venue_record->figure_id = charioteer->id();

}

void BuildingEntertainment::spawn_colosseum_service()
{
    if (building_runtime *runtime = venue_.runtime_instance()) {
        runtime->check_labor_problem();
    }
    if (has_figure_of_types(FIGURE_GLADIATOR, FIGURE_LION_TAMER)) {
        return;
    }

    map_point road;
    if (!find_venue_road(road)) {
        return;
    }

    run_labor_phase(road);
    static const std::vector<building_type_registry_impl::DelayBand> kColosseumDelays = {
        { 100, 6 },
        { 75, 12 },
        { 50, 20 },
        { 25, 40 },
        { 1, 70 }
    };
    if (!delay_has_elapsed(kColosseumDelays)) {
        return;
    }

    building *venue_record = record();
    if (!venue_record) {
        return;
    }

    const figure_type spawned_type = venue_record->data.entertainment.days1 > 0 ?
        FIGURE_LION_TAMER :
        FIGURE_GLADIATOR;
    Figure *figure = create_roaming_figure(road, spawned_type);
    if (!figure) {
        return;
    }

    venue_record->figure_id = figure->id();
    spawn_execution_lion_tamers(road);
    post_colosseum_message_if_active();
}

void BuildingEntertainment::run_show_countdown()
{
    building *venue_record = record();
    if (!venue_record || venue_record->state != BUILDING_STATE_IN_USE || !is_show_venue(venue_)) {
        return;
    }
    if (building_monument_is_monument(venue_record) && venue_record->monument.phase != MONUMENT_FINISHED) {
        venue_record->data.entertainment.num_shows = 0;
        return;
    }

    int shows = 0;
    if (venue_record->data.entertainment.days1 > 0) {
        --venue_record->data.entertainment.days1;
        ++shows;
    }
    if (venue_record->data.entertainment.days2 > 0) {
        --venue_record->data.entertainment.days2;
        ++shows;
    }
    venue_record->data.entertainment.num_shows = static_cast<unsigned char>(shows);
    if (shows > 0) {
        venue_.invalidate_graphic();
    }
}

void building_entertainment_run_shows(void)
{
    Building::for_each([](Building *building) {
        const ::building *b = building->record();
        if (b->state != BUILDING_STATE_IN_USE) {
            return;
        }
        BuildingEntertainment(*building).run_show_countdown();
    });
}
