#include "race_bet.h"

#include "building/building.h"
#include "city/data_private.h"
#include "city/warning.h"
#include "festival.h"
#include "figure/figure.h"

#include <algorithm>
#include <unordered_map>

namespace {

std::unordered_map<unsigned int, RaceSession> sessions;
unsigned int selected_building_id = 0;

const building_type_registry_impl::RaceDefinition *race_definition(unsigned int building_id)
{
    Building *building = Building::get(building_id);
    return building && building->type && building->type->has_race() ? &building->type->race() : nullptr;
}

void settle_bet(unsigned int building_id, int winning_team)
{
    if (!has_bet_in_progress() || (selected_building_id && selected_building_id != building_id)) return;
    const building_type_registry_impl::RaceDefinition *definition = race_definition(building_id);
    if (!definition || !definition->betting.enabled) return;
    const int chosen_team = static_cast<int>(city_data.games.chosen_horse) - 1;
    if (winning_team == chosen_team) {
        const int multiplier = city_festival_games_active() ? definition->betting.festival_multiplier : definition->betting.normal_multiplier;
        city_data.emperor.personal_savings += city_data.games.bet_amount * multiplier;
        city_warning_show(WARNING_BET_VICTORY, translation_for_key("TR_WARNING_BET_VICTORY"));
    } else {
        city_data.emperor.personal_savings = std::max(0, city_data.emperor.personal_savings - city_data.games.bet_amount);
        city_warning_show(WARNING_BET_DEFEAT, translation_for_key("TR_WARNING_BET_DEFEAT"));
    }
    city_data.games.chosen_horse = NO_BET;
    city_data.games.bet_amount = 0;
    selected_building_id = 0;
}

RaceSession *find_session(unsigned int building_id)
{
    const auto found = sessions.find(building_id);
    return found == sessions.end() ? nullptr : &found->second;
}

void invalidate_venue_graphics(unsigned int building_id)
{
    if (Building *building = Building::get(building_id)) building->invalidate_graphic();
}

int session_has_live_participant(unsigned int building_id)
{
    const building_type_registry_impl::RaceDefinition *definition = race_definition(building_id);
    if (!definition) return 0;
    for (unsigned int id = 1; id < Figure::count(); ++id) {
        Figure *figure = Figure::get(id);
        if (figure && figure->state == FIGURE_STATE_ALIVE && figure->type == definition->participant_figure &&
            figure->building && figure->building->id == building_id) return 1;
    }
    return 0;
}

int live_participant_count(unsigned int building_id)
{
    const building_type_registry_impl::RaceDefinition *definition = race_definition(building_id);
    if (!definition) return 0;
    int count = 0;
    for (unsigned int id = 1; id < Figure::count(); ++id) {
        Figure *figure = Figure::get(id);
        if (figure && figure->state == FIGURE_STATE_ALIVE && figure->type == definition->participant_figure &&
            figure->building && figure->building->id == building_id) ++count;
    }
    return count;
}

} // namespace

int has_bet_in_progress(void)
{
    return city_data.games.chosen_horse != NO_BET && city_data.games.bet_amount > 0;
}

int race_bet_can_place(void)
{
    return !has_bet_in_progress() && !race_bet_any_active();
}

int race_bet_can_place_for(unsigned int building_id)
{
    const building_type_registry_impl::RaceDefinition *definition = race_definition(building_id);
    return definition && definition->betting.enabled && !has_bet_in_progress() && !race_bet_is_active(building_id);
}

void race_bet_reset_runtime(void)
{
    for (const auto &session : sessions) invalidate_venue_graphics(session.first);
    sessions.clear();
    selected_building_id = 0;
}

void race_bet_start(unsigned int building_id, int expected_participants)
{
    if (!building_id || expected_participants < 1) return;
    sessions[building_id].start(building_id, expected_participants);
    invalidate_venue_graphics(building_id);
}

void race_bet_end(unsigned int building_id)
{
    if (sessions.erase(building_id)) invalidate_venue_graphics(building_id);
}

int race_bet_is_active(unsigned int building_id)
{
    if (!find_session(building_id)) return 0;
    if (session_has_live_participant(building_id)) return 1;
    sessions.erase(building_id);
    invalidate_venue_graphics(building_id);
    return 0;
}

int race_bet_any_active(void)
{
    for (auto session = sessions.begin(); session != sessions.end();) {
        if (!session_has_live_participant(session->first)) {
            const unsigned int building_id = session->first;
            session = sessions.erase(session);
            invalidate_venue_graphics(building_id);
        } else {
            ++session;
        }
    }
    return sessions.empty() ? 0 : 1;
}

int race_bet_register_participant(unsigned int building_id, int team_index)
{
    RaceSession *session = find_session(building_id);
    if (!session) {
        const building_type_registry_impl::RaceDefinition *definition = race_definition(building_id);
        if (!definition) return 0;
        const int saved_participants = live_participant_count(building_id);
        const int team_capacity = static_cast<int>(definition->teams.size());
        sessions[building_id].start(building_id, team_capacity, saved_participants > 0 ? saved_participants : team_capacity);
        invalidate_venue_graphics(building_id);
        if (!selected_building_id && has_bet_in_progress() && definition->betting.enabled) {
            selected_building_id = building_id;
        }
        session = find_session(building_id);
    }
    if (!session) return 0;
    const int count = session->register_participant(team_index);
    if (session->has_complete_field() && session->is_full_configured_field() && session->winner() >= 0) {
        settle_bet(building_id, session->winner());
    }
    return count;
}

int race_bet_register_finish(unsigned int building_id, int team_index)
{
    RaceSession *session = find_session(building_id);
    if (!session) return 0;
    const int position = session->register_finish(team_index);
    if (position > 0 && session->has_complete_field() && session->is_full_configured_field()) {
        settle_bet(building_id, session->winner());
    }
    return position;
}

int race_bet_winner(unsigned int building_id)
{
    RaceSession *session = find_session(building_id);
    return session ? session->winner() : -1;
}

int race_bet_finish_position(unsigned int building_id, int team_index)
{
    RaceSession *session = find_session(building_id);
    return session ? session->finish_position(team_index) : 0;
}

unsigned int race_bet_selected_building(void)
{
    return selected_building_id;
}

void race_bet_select_building(unsigned int building_id)
{
    selected_building_id = building_id;
}
