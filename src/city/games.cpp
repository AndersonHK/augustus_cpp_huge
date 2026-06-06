#include "games.h"

#include "building/building_type_api.h"
#include "building/granary.h"
#include "building/warehouse.h"
#include "core/calc.h"
#include "city/constants.h"
#include "city/data_private.h"
#include "city/emperor.h"
#include "city/finance.h"
#include "city/message.h"
#include "city/sentiment.h"
#include "core/config.h"
#include "game/time.h"

#define POPULATION_SCALING_FACTOR 1200
#define BASE_RESOURCE_REQUIREMENT 3

typedef enum {
    G_PLANNING,
    G_STARTING,
    G_ENDING
} games_messages;

static void naval_battle_start(void);
static void executions_start(void);
static void imperial_games_start(void);

static games_type make_game(
    int id,
    translation_key header_key,
    translation_key description_key,
    int message_planning,
    int cost_base,
    int cost_scaling,
    int delay_months,
    int duration_days,
    int bonus_duration,
    const char *building_text_id_required,
    int water_access_required,
    resource_type resource_1,
    int cost_1,
    resource_type resource_2,
    int cost_2,
    void (*games_start_function)(void))
{
    games_type game = {};
    game.id = id;
    game.header_key = header_key;
    game.description_key = description_key;
    game.message_planning = message_planning;
    game.cost_base = cost_base;
    game.cost_scaling = cost_scaling;
    game.delay_months = delay_months;
    game.duration_days = duration_days;
    game.bonus_duration = bonus_duration;
    game.building_id_required = building_type_registry_runtime_id_from_text(building_text_id_required);
    game.water_access_required = water_access_required;
    if (resource_1 != RESOURCE_NONE) {
        game.resource_cost[resource_1] = cost_1;
    }
    if (resource_2 != RESOURCE_NONE) {
        game.resource_cost[resource_2] = cost_2;
    }
    game.games_start_function = games_start_function;
    return game;
}

static games_type *all_games()
{
    static games_type games[MAX_GAMES];
    static int initialized = 0;
    if (!initialized) {
        games[0] = make_game(
            1, TR_WINDOW_GAMES_OPTION_1, TR_WINDOW_GAMES_OPTION_1_DESC, MESSAGE_NG_GAMES_PLANNED, 1500, 100, 1, 32, 12,
            "colosseum", 1, resource_wine(), 1, resource_timber(), 1, naval_battle_start
        );
        games[1] = make_game(
            2, TR_WINDOW_GAMES_OPTION_5, TR_WINDOW_GAMES_OPTION_5_DESC, MESSAGE_IG_GAMES_PLANNED, 800, 150, 1, 32, 12,
            "colosseum", 0, resource_wheat(), 2, resource_oil(), 1, imperial_games_start
        );
        games[2] = make_game(
            3, TR_WINDOW_GAMES_OPTION_2, TR_WINDOW_GAMES_OPTION_2_DESC, MESSAGE_AN_GAMES_PLANNED, 800, 150, 1, 32, 12,
            "colosseum", 0, resource_meat(), 2, RESOURCE_NONE, 0, executions_start
        );
        initialized = 1;
    }
    return games;
}


games_type *city_games_get_game_type(int id)
{
    games_type *games = all_games();
    for (int i = 0; i < MAX_GAMES; ++i) {
        if (games[i].id == id) {
            return &games[i];
        }
    }
    return 0;
}

int city_games_money_cost(int game_type_id)
{
    games_type *game = city_games_get_game_type(game_type_id);
    if (!game) {
        return 0;
    }
    int cost = game->cost_base + game->cost_scaling * (city_data.population.population / POPULATION_SCALING_FACTOR);
    return cost;
}

int city_games_resource_cost(int game_type_id, resource_type resource)
{
    games_type *game = city_games_get_game_type(game_type_id);
    if (!game) {
        return 0;
    }
    if (game_type_id == 3) { //for animal games, check if player has more fish or meat
        int player_meat_count = city_resource_get_amount_including_granaries(resource_meat(), 2, 0, 1);
        int player_fish_count = city_resource_get_amount_including_granaries(resource_fish(), 2, 0, 1);

        resource_type dominant_resource = (player_meat_count >= player_fish_count) ? resource_meat() : resource_fish();
        for (int i = 0; i < RESOURCE_SLOT_COUNT; ++i) {
            // Reset all costs to 0 before reassigignment
            game->resource_cost[i] = 0;
        }
        game->resource_cost[dominant_resource] = 2;
    }
    int cost = game->resource_cost[resource] * (BASE_RESOURCE_REQUIREMENT + city_data.population.population / POPULATION_SCALING_FACTOR);

    return cost;
}


static void post_games_message(int type)
{
    games_type *game = city_games_get_game_type(city_data.games.selected_games_id);
    city_message_post(1, game->message_planning + type, 0, 0);
}

static void begin_games(void)
{
    games_type *game = city_games_get_game_type(city_data.games.selected_games_id);

    city_data.games.months_to_go = 0;
    city_data.games.games_is_active = 1;
    city_data.games.remaining_duration = game->duration_days;
    game->games_start_function();

    post_games_message(G_STARTING);
}

static void end_games(void)
{
    city_data.games.games_is_active = 0;
    city_data.games.remaining_duration = 0;

    city_data.games.months_since_last = 0;

    post_games_message(G_ENDING);
}

void city_games_schedule(int game_id)
{
    games_type *game = city_games_get_game_type(game_id);
    city_emperor_decrement_personal_savings(city_games_money_cost(game_id));

    for (int resource_id = (RESOURCE_NONE + 1); resource_id < RESOURCE_SLOT_COUNT; resource_id++) {
        resource_type resource = static_cast<resource_type>(resource_id);
        int resource_cost = city_games_resource_cost(game_id, resource);
        if (resource_cost) {
            resource_cost = building_warehouses_remove_resource(resource, resource_cost);
            if (resource_cost > 0 && resource_is_food(resource)) {
                building_granaries_remove_resource(resource, resource_cost);
            }
        }
    }

    city_data.games.months_to_go = game->delay_months;
    post_games_message(G_PLANNING);
}

void city_games_decrement_month_counts(void)
{
    city_data.games.months_since_last++;

    if (city_data.games.months_to_go) {
        city_data.games.months_to_go--;
        if (city_data.games.months_to_go == 0) {
            begin_games();
        }
    }
    if (city_data.games.naval_battle_bonus_months) {
        city_data.games.naval_battle_bonus_months--;
    }
    if (city_data.games.executions_bonus_months) {
        city_data.games.executions_bonus_months--;
    }
    if (city_data.games.imperial_games_bonus_months) {
        city_data.games.imperial_games_bonus_months--;
    }
    if (city_data.games.games_4_bonus_months) {
        city_data.games.games_4_bonus_months--;
    }
}

void city_games_decrement_duration(void)
{
    if (city_data.games.remaining_duration && city_data.games.games_is_active) {
        city_data.games.remaining_duration--;
        if (city_data.games.remaining_duration == 0) {
            end_games();
        }
    }
}

int city_games_naval_battle_active(void)
{
    return city_data.games.naval_battle_bonus_months;
}

int city_games_executions_active(void)
{
    return city_data.games.executions_bonus_months;
}

int city_games_imperial_festival_active(void)
{
    return city_data.games.imperial_games_bonus_months;
}

int city_games_naval_battle_distant_battle_bonus_active(void)
{
    return city_data.games.naval_battle_distant_battle_bonus;
}

void city_games_remove_naval_battle_distant_battle_bonus(void)
{
    city_data.games.naval_battle_distant_battle_bonus = 0;
}

static void naval_battle_start(void)
{
    games_type *game = city_games_get_game_type(city_data.games.selected_games_id);
    city_data.games.naval_battle_bonus_months = game->bonus_duration;
    city_data.games.naval_battle_distant_battle_bonus = 1;
}

static void executions_start(void)
{
    games_type *game = city_games_get_game_type(city_data.games.selected_games_id);
    city_data.games.executions_bonus_months = game->bonus_duration;
}

static void imperial_games_start(void)
{
    games_type *game = city_games_get_game_type(city_data.games.selected_games_id);
    city_data.games.imperial_games_bonus_months = game->bonus_duration;
}
