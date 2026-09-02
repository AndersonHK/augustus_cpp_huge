#include "city/data_private.h"
#include "city/entertainment.h"
#include "city/warning.h"
#include "festival.h"
#include "race_bet.h"

namespace {

HippodromeRace race;

void settle_bet(bet_horse winner)
{
    if (!has_bet_in_progress()) {
        return;
    }
    if (winner == city_data.games.chosen_horse) {
        city_data.emperor.personal_savings += city_data.games.bet_amount * (city_festival_games_active() ? 4 : 2);
        city_warning_show(WARNING_BET_VICTORY, translation_for_key("TR_WARNING_BET_VICTORY"));
    } else {
        if (city_data.emperor.personal_savings > city_data.games.bet_amount) {
            city_data.emperor.personal_savings -= city_data.games.bet_amount;
        } else {
            city_data.emperor.personal_savings = 0;
        }
        city_warning_show(WARNING_BET_DEFEAT, translation_for_key("TR_WARNING_BET_DEFEAT"));
    }
    city_data.games.chosen_horse = NO_BET;
    city_data.games.bet_amount = 0;
}

} // namespace

int has_bet_in_progress(void)
{
    return city_data.games.chosen_horse != NO_BET && city_data.games.bet_amount;
}

int race_bet_can_place(void)
{
    return !has_bet_in_progress() && !city_entertainment_hippodrome_has_race();
}

void race_bet_reset_runtime(void)
{
    race.start(0);
}

void race_bet_start(unsigned int hippodrome_id)
{
    race.start(hippodrome_id);
    city_entertainment_set_hippodrome_has_race(1);
}

int race_bet_register_participant(unsigned int hippodrome_id, bet_horse horse)
{
    const int participant_count = race.register_participant(hippodrome_id, horse);
    if (participant_count == 4 && race.winner() != NO_BET) settle_bet(race.winner());
    return participant_count;
}

int race_bet_register_finish(unsigned int hippodrome_id, bet_horse horse)
{
    const bet_horse winner_before = race.hippodrome_id() == hippodrome_id ? race.winner() : NO_BET;
    const int position = race.register_finish(hippodrome_id, horse);
    if (position == 1 && winner_before == NO_BET && race.participant_count() == 4) {
        settle_bet(horse);
    }
    return position;
}

bet_horse race_bet_winner(void)
{
    return race.winner();
}

int race_bet_finish_position(bet_horse horse)
{
    return race.finish_position(horse);
}
