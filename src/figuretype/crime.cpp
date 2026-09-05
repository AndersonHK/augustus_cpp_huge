#include "building/distribution.h"
#include "building/HousingProfileDef.h"
#include "building/storage.h"
#include "city/games.h"
#include "city/sentiment.h"
#include "city/warning.h"
#include "map/building.h"
#include "map/road_access.h"

#include "crime.h"

#include "building/building.h"
#include "building/building_type_registry_internal.h"

#include "building/building_record.h"
#include "building/destruction.h"
#include "building/granary.h"
#include "building/warehouse.h"
#include "city/data_private.h"
#include "city/figures.h"
#include "city/finance.h"
#include "city/message.h"
#include "city/population.h"
#include "city/ratings.h"
#include "core/calc.h"
#include "core/image.h"
#include "core/random.h"
#include "figure/combat.h"
#include "figure/formation_enemy.h"
#include "figure/image.h"
#include "figure/figure_runtime_api.h"
#include "figure/movement.h"
#include "figure/route.h"
#include "game/tutorial.h"
#include "game/resource.h"
#include "game/time.h"
#include "map/grid.h"
#include "scenario/property.h"

#define MAX_LOOTING_DISTANCE 120

typedef struct {
    Building *building;
    resource_type resource;
} looter_destination;

static Building *target_building_at(int x, int y)
{
    const int grid_offset = map_grid_offset(x, y);
    if (!map_building_exists_at(grid_offset)) {
        return nullptr;
    }
    Building &selected = map_building_at(grid_offset);
    return selected.type && selected.type->bridge().is_bridge() ?
        &selected.dynamic_bridge_owner() :
        (selected.Composition ? selected.Composition->owner() : &selected);
}

static int get_looter_destination(Figure *f)
{
    resource_storage_info info[RESOURCE_SLOT_COUNT] = { 0 };

    // Check everything
    for (int r = (RESOURCE_NONE + 1); r < RESOURCE_SLOT_COUNT; r++) {
        info[r].needed = 1;
    }

    looter_destination possible_destinations[RESOURCE_SLOT_COUNT];
    if (!building_type_registry_impl::find_distribution_sources_for_figure(info, BUILDING_NONE, 0, f, MAX_LOOTING_DISTANCE)) {
        return 0;
    }

    resource_type resource = RESOURCE_NONE;
    int options = 0;

    for (int r = (RESOURCE_NONE + 1); r < RESOURCE_SLOT_COUNT; r++) {
        if (info[r].source) {
            possible_destinations[options].building = info[r].source;
            possible_destinations[options].resource = static_cast<resource_type>(r);
            options += 1;
        }
    }

    if (options) {
        int random_index = random_from_stdlib() % options;
        Building *storage_building = possible_destinations[random_index].building;
        building *storage = storage_building ? const_cast<building *>(storage_building->record()) : nullptr;
        if (!storage) {
            return 0;
        }
        resource = possible_destinations[random_index].resource;

        f->destination_x = static_cast<unsigned char>(storage->road_access_x);
        f->destination_y = static_cast<unsigned char>(storage->road_access_y);
    f->set_destination_building(storage_building);
        f->collecting_item_id = static_cast<unsigned char>(resource);

        return storage->id;
    } else {
        return 0;
    }
}

static void loot_storage(Figure *f, resource_type resource, Building &storage)
{
    if (storage.type && storage.type->is_granary()) {
        building_granary_try_remove_resource(storage, resource, 100);
        city_warning_show(WARNING_GRANARY_BREAKIN, translation_for_key("TR_CITY_WARNING_GRANARY_BREAKIN"));
    } else {
        building_warehouse_try_remove_resource(storage, resource, 1);
        city_warning_show(WARNING_WAREHOUSE_BREAKIN, translation_for_key("TR_CITY_WARNING_WAREHOUSE_BREAKIN"));
    }

    city_message_apply_sound_interval(MESSAGE_CAT_THEFT);
    city_message_post_with_popup_delay(MESSAGE_CAT_THEFT, MESSAGE_LOOTING,
        storage.type ? storage.type->type() : BUILDING_NONE, f->grid_offset);
}

static void figure_crime_steal_money(Figure *f)
{
    int treasury = city_finance_treasury();
    int money_stolen = treasury / 40;
    if (money_stolen > 400) {
        money_stolen = 400 - random_byte() / 2;
    }

    if (money_stolen < 10) {
        return;
    }
    city_message_apply_sound_interval(MESSAGE_CAT_THEFT);
    city_message_post_with_popup_delay(MESSAGE_CAT_THEFT, MESSAGE_THEFT, money_stolen, f->grid_offset);
    city_warning_show(WARNING_THEFT, translation_for_key("TR_CITY_WARNING_THEFT"));
    city_finance_process_stolen(money_stolen);
}

static void generate_striker(Building &building)
{
    ::building *b = const_cast<::building *>(building.record());
    int x_road, y_road;
    if (map_closest_road_within_radius_building(building, 2, &x_road, &y_road) && b->figure_id4 == 0) {
        Figure *f = Figure::create(FIGURE_PROTESTER, x_road, y_road, DIR_4_BOTTOM);
    f->set_home_building(&building);
        b->figure_id4 = f->id();
    }
}

static void generate_rioter(Building &building, int amount)
{
    ::building *b = const_cast<::building *>(building.record());
    int x_road, y_road;
    if (city_ratings_peace_num_rioters() > city_population() / 500) {
        return;
    }

    if (!map_closest_road_within_radius_building(building, 4, &x_road, &y_road)) {
        return;
    }

    building.Housing->state().criminal_active = 1;

    city_sentiment_add_criminal();

    for (int i = 0; i < amount; i++) {
        Figure *f = Figure::create(FIGURE_RIOTER, x_road, y_road, DIR_4_BOTTOM);
        f->action_state = FIGURE_ACTION_120_RIOTER_CREATED;
        f->roam_length = 0;
        f->wait_ticks = static_cast<short>(game_time_scale_legacy_day_ticks(10 + 4 * i));
        f->terrain_usage = TERRAIN_USAGE_ENEMY;
    }
    city_ratings_peace_record_rioter();

    tutorial_on_crime();
    city_message_apply_sound_interval(MESSAGE_CAT_RIOT);
    city_message_post_with_popup_delay(
        MESSAGE_CAT_RIOT, MESSAGE_RIOT, b->type, static_cast<short>(map_grid_offset(x_road, y_road)));
    city_sentiment_set_crime_cooldown();

}

static void generate_looter(Building &building, int amount)
{
    int x_road, y_road;
    if (!map_closest_road_within_radius_building(building, 4, &x_road, &y_road)) {
        return;
    }
    city_sentiment_add_criminal();

    for (int i = 0; i < amount; i++) {
        Figure *f = Figure::create(FIGURE_CRIMINAL_LOOTER, x_road, y_road, DIR_4_BOTTOM);
        f->terrain_usage = TERRAIN_USAGE_ANY;
        f->action_state = FIGURE_ACTION_226_CRIMINAL_LOOTER_CREATED;
        f->roam_length = 0;
        f->wait_ticks = static_cast<short>(game_time_scale_legacy_day_ticks(10 + 4 * i));
    }
    building.Housing->state().criminal_active = 1;
    city_sentiment_set_crime_cooldown();
}

static void generate_robber(Building &building, int amount)
{
    if (city_finance_treasury() < 400) {
        return;
    }

    int x_road, y_road;
    if (!map_closest_road_within_radius_building(building, 4, &x_road, &y_road)) {
        return;
    }
    city_sentiment_add_criminal();
    building.Housing->state().criminal_active = 1;

    for (int i = 0; i < amount; i++) {
        Figure *f = Figure::create(FIGURE_CRIMINAL_ROBBER, x_road, y_road, DIR_4_BOTTOM);
        f->action_state = FIGURE_ACTION_227_CRIMINAL_ROBBER_CREATED;
        f->roam_length = 0;
        f->wait_ticks = static_cast<short>(game_time_scale_legacy_day_ticks(10 + 4 * i));
    }
    city_sentiment_set_crime_cooldown();
}

static void generate_protestor(Building &building)
{
    HousingState &state = building.Housing->state();
    city_sentiment_add_protester();
    if (state.criminal_active < 1) {
        state.criminal_active = 1;
        int x_road, y_road;
        if (map_closest_road_within_radius_building(building, 2, &x_road, &y_road)) {
            Figure *f = Figure::create(FIGURE_PROTESTER, x_road, y_road, DIR_4_BOTTOM);
            f->wait_ticks =
                static_cast<short>(game_time_scale_legacy_day_ticks(10 + (state.figure_generation_delay & 0xf)));
            city_ratings_peace_record_criminal();
        }
    }
    city_sentiment_set_crime_cooldown();
}

void figure_generate_criminals(void)
{
    if (city_games_executions_active()) {
        return;
    }

    Building::for_each([](Building *building) {
        ::building *b = const_cast<::building *>(building->record());
        if (b->state == BUILDING_STATE_IN_USE && b->strike_duration_days > 0) {
            generate_striker(*building);
        }
    });

    Building *min_building = nullptr;
    int min_happiness = 50;
    Building::for_each(BuildingRuntimeList::Housing, [&](Building *building) {
        if (!building->is_in_use()) {
            return;
        }
        HousingState &state = building->Housing->state();
        if (state.happiness >= 50) {
            state.criminal_active = 0;
        } else if (state.happiness < min_happiness) {
            min_happiness = state.happiness;
            min_building = building;
        }
    });
    if (city_sentiment_crime_cooldown() > 0) {
        city_sentiment_reduce_crime_cooldown();
        return;
    }
    if (min_building) {
        if (scenario_is_tutorial_1() || scenario_is_tutorial_2()) {
            return;
        }
        int sentiment = city_sentiment();
        if (random_byte() >= sentiment + 20) {
            HousingState &state = min_building->Housing->state();
            state.happiness = static_cast<int8_t>(state.happiness + 5);
            if (min_happiness <= 15) {
                int unhappy_population = city_sentiment_get_population_below_happiness(25);
                if (calc_percentage(unhappy_population, city_population()) >= 5) {
                    int amount = calc_bound(unhappy_population / 100, 1, 20);
                    generate_rioter(*min_building, amount);
                    generate_looter(*min_building, amount);
                    generate_robber(*min_building, amount);
                    city_sentiment_set_min_happiness(30);
                } else {
                    generate_looter(*min_building, 3);
                    generate_robber(*min_building, 3);
                }
            } else if (min_happiness < 30) {
                generate_looter(*min_building, min_happiness < 20 ? 2 : 1);
            } else if (min_happiness < 45) {
                generate_robber(*min_building, min_happiness < 30 ? 2 : 1);
            } else if (min_happiness < 50) {
                generate_protestor(*min_building);
            }
        }
    }
}

void figure_protestor_action(Figure *f)
{
    f->terrain_usage = TERRAIN_USAGE_ROADS_HIGHWAY;
    figure_image_increase_offset(f, 64);
    city_figures_add_protester();
    f->clear_legacy_cart_overlay_image();

    if (f->action_state == FIGURE_ACTION_149_CORPSE) {
        figure_combat_handle_corpse(f);
    }
    f->wait_ticks++;
    if (f->wait_ticks > 200 && !f->building) {
        f->state = FIGURE_STATE_DEAD;
        f->image_offset = 0;
    }
    figure_protestor_update_graphics(f);
}

void figure_protestor_update_graphics(Figure *f)
{
    if (f->action_state == FIGURE_ACTION_149_CORPSE) figure_runtime_graphics_select_corpse_entry(f, "corpse");
    else figure_runtime_graphics_select_default_entry_frame(f, "gesture", f->image_offset / 4 + 1);
}

void figure_criminal_update_graphics(Figure *f)
{
    int dir;
    if (f->direction == DIR_FIGURE_ATTACK) {
        dir = f->attack_direction;
    } else if (f->direction < 8) {
        dir = f->direction;
    } else {
        dir = f->previous_tile_direction;
    }
    dir = figure_image_normalize_direction(dir);
    if (f->action_state == FIGURE_ACTION_149_CORPSE) {
        figure_runtime_graphics_select_corpse_entry(f, "corpse");
    } else if (f->direction == DIR_FIGURE_ATTACK) {
        figure_runtime_graphics_select_default_entry_frame(f, "gesture", f->image_offset % 16 + 1);
    } else if (f->action_state == FIGURE_ACTION_121_RIOTER_MOVING || f->action_state == FIGURE_ACTION_228_CRIMINAL_GOING_TO_LOOT || f->action_state == FIGURE_ACTION_229_CRIMINAL_GOING_TO_ROB) {
        figure_runtime_graphics_select_directional_entry_frame(f, "move", dir, f->image_offset + 1);
    } else {
        figure_runtime_graphics_select_default_entry_frame(f, "gesture", f->image_offset / 2 + 1);
    }
}


void figure_rioter_action(Figure *f)
{
    city_figures_add_rioter(!f->targeted_by_figure.save_id());
    f->terrain_usage = TERRAIN_USAGE_ENEMY;
    f->max_roam_length = 480;
    f->clear_legacy_cart_overlay_image();
    f->is_ghost = 0;

    switch (f->action_state) {
        case FIGURE_ACTION_150_ATTACK:
            figure_combat_handle_attack(f);
            break;
        case FIGURE_ACTION_149_CORPSE:
            figure_combat_handle_corpse(f);
            break;
        case FIGURE_ACTION_120_RIOTER_CREATED:
        {
            figure_image_increase_offset(f, 32);
            f->wait_ticks++;
            if (f->wait_ticks >= 160) {
                f->action_state = FIGURE_ACTION_121_RIOTER_MOVING;
                f->image_offset = 0;
                int x_tile, y_tile, resource = 0, target_building_id;
                target_building_id = formation_rioter_get_target_building(&x_tile, &y_tile);

                if (target_building_id) {
                    f->destination_x = static_cast<unsigned char>(x_tile);
                    f->destination_y = static_cast<unsigned char>(y_tile);
    f->set_destination_building(target_building_at(x_tile, y_tile));
                    f->collecting_item_id = static_cast<unsigned char>(resource);
                    Route::remove(f);
                } else {
                    f->state = FIGURE_STATE_DEAD;
                    Route::remove(f);
                }
            }
            break;
        }
        case FIGURE_ACTION_121_RIOTER_MOVING:
            figure_image_increase_offset(f, 12);
            figure_movement_move_ticks(f, 1);
            if (f->direction == DIR_FIGURE_AT_DESTINATION) {
                figure_rioter_collapse_building(f);

                int x_tile, y_tile;
                int building_id = formation_rioter_get_target_building(&x_tile, &y_tile);
                if (building_id) {
                    f->destination_x = static_cast<unsigned char>(x_tile);
                    f->destination_y = static_cast<unsigned char>(y_tile);
    f->set_destination_building(target_building_at(x_tile, y_tile));
                    Route::remove(f);
                } else {
                    f->type = FIGURE_CRIMINAL;
                    f->action_state = FIGURE_ACTION_120_RIOTER_CREATED;
                    Route::remove(f);
                }
            } else if (f->direction == DIR_FIGURE_REROUTE || f->direction == DIR_FIGURE_LOST) {
                f->state = FIGURE_STATE_DEAD;
            } else if (f->direction == DIR_FIGURE_ATTACK) {
                if (f->image_offset > 12) {
                    f->image_offset = 0;
                }
            }
            break;
    }

    figure_criminal_update_graphics(f);
}

void figure_robber_action(Figure *f)
{
    city_figures_add_robber(!f->targeted_by_figure.save_id());
    f->terrain_usage = TERRAIN_USAGE_PREFER_ROADS_HIGHWAY;
    f->max_roam_length = 480;
    f->clear_legacy_cart_overlay_image();
    f->is_ghost = 0;

    switch (f->action_state) {
        case FIGURE_ACTION_150_ATTACK:
            figure_combat_handle_attack(f);
            break;
        case FIGURE_ACTION_149_CORPSE:
            figure_combat_handle_corpse(f);
            break;
        case FIGURE_ACTION_227_CRIMINAL_ROBBER_CREATED:
        {
            figure_image_increase_offset(f, 32);
            f->wait_ticks++;
            if (f->wait_ticks >= 160) {
                int x_tile, y_tile, resource = 0, target_building_id;
                target_building_id = formation_rioter_get_target_building_for_robbery(f->x, f->y, &x_tile, &y_tile);

                if (target_building_id) {
                    f->destination_x = static_cast<unsigned char>(x_tile);
                    f->destination_y = static_cast<unsigned char>(y_tile);
    f->set_destination_building(target_building_at(x_tile, y_tile));
                    f->collecting_item_id = static_cast<unsigned char>(resource);
                f->action_state = FIGURE_ACTION_229_CRIMINAL_GOING_TO_ROB;
                f->image_offset = 0;
                    Route::remove(f);
                } else {
                    f->state = FIGURE_STATE_DEAD;
                }
            }
            break;
        }
        case FIGURE_ACTION_229_CRIMINAL_GOING_TO_ROB:
            figure_image_increase_offset(f, 12);
            figure_movement_move_ticks(f, 1);
            if (f->direction == DIR_FIGURE_AT_DESTINATION) {
                figure_crime_steal_money(f);
                f->state = FIGURE_STATE_DEAD;
            } else if (f->direction == DIR_FIGURE_REROUTE) {
                Route::remove(f);
            } else if (f->direction == DIR_FIGURE_LOST) {
                f->state = FIGURE_STATE_DEAD;
            }
            break;
    }

    figure_criminal_update_graphics(f);
}

void figure_looter_action(Figure *f)
{
    city_figures_add_looter(!f->targeted_by_figure.save_id());
    f->terrain_usage = TERRAIN_USAGE_PREFER_ROADS_HIGHWAY;
    f->max_roam_length = 480;
    f->clear_legacy_cart_overlay_image();
    f->is_ghost = 0;

    switch (f->action_state) {
        case FIGURE_ACTION_150_ATTACK:
            figure_combat_handle_attack(f);
            break;
        case FIGURE_ACTION_149_CORPSE:
            figure_combat_handle_corpse(f);
            break;
        case FIGURE_ACTION_226_CRIMINAL_LOOTER_CREATED:
        {
            figure_image_increase_offset(f, 32);
            f->wait_ticks++;
            if (f->wait_ticks >= 160) {
                int target_building_id = get_looter_destination(f);

                if (target_building_id) {
                f->action_state = FIGURE_ACTION_228_CRIMINAL_GOING_TO_LOOT;
                f->image_offset = 0;
                    Route::remove(f);
                } else {
                    f->state = FIGURE_STATE_DEAD;
                    Route::remove(f);
                }
            }
            break;
        }
        case FIGURE_ACTION_228_CRIMINAL_GOING_TO_LOOT:
            figure_image_increase_offset(f, 12);
            figure_movement_move_ticks(f, 1);
            if (f->direction == DIR_FIGURE_AT_DESTINATION) {
                if (f->destination_building) {
                    loot_storage(f, static_cast<resource_type>(f->collecting_item_id), *f->destination_building);
                }
                f->state = FIGURE_STATE_DEAD;
            } else if (f->direction == DIR_FIGURE_REROUTE) {
                Route::remove(f);
            } else if (f->direction == DIR_FIGURE_LOST) {
                f->state = FIGURE_STATE_DEAD;
            }
            break;
    }

    figure_criminal_update_graphics(f);
}


int figure_rioter_collapse_building(Figure *f)
{
    static const char * const non_collapsible_types[] = {
        "warehouse_space",
        "warehouse",
        "fort_archers",
        "fort_legionaries",
        "fort_javelin",
        "fort_mounted",
        "fort_auxilia_infantry",
        "fort_ground",
        "burning_ruin",
        "native_crops",
        "native_hut",
        "native_hut_alt",
        "native_meeting",
        "native_decor",
        "native_watchtower",
        "native_monument",
        "reservoir",
        "fountain",
        "market",
        "granary",
        "forum",
        "senate",
        0
    };

    for (int dir = 0; dir < 8; dir += 2) {
        int grid_offset = f->grid_offset + map_grid_direction_delta(dir);
        if (!map_building_exists_at(grid_offset)) {
            continue;
        }
        Building building = map_building_at(grid_offset);
        ::building *b = const_cast<::building *>(building.record());
        if (building.type && building.type->is_well()) {
            continue;
        }
        if (building_type_registry_impl::type_attr_is_any(
            b->type,
            non_collapsible_types,
            (sizeof(non_collapsible_types) / sizeof(non_collapsible_types[0])) - 1)) {
            continue;
        }
        const auto *profile = building.Housing ? building.Housing->definition().profile : nullptr;
        int house_level = profile ? profile->compatibility_level : -1;
        if (building.Housing &&
            house_level >= HOUSE_MIN && house_level < HOUSE_SMALL_CASA) {
            continue;
        }
        if (b->fire_proof) {
            continue;
        }
        city_message_apply_sound_interval(MESSAGE_CAT_RIOT_COLLAPSE);
        city_message_post(0, MESSAGE_DESTROYED_BUILDING, b->type, f->grid_offset);
        city_message_increase_category_count(MESSAGE_CAT_RIOT_COLLAPSE);
        building.destroy_by_fire();
        f->action_state = FIGURE_ACTION_120_RIOTER_CREATED;
        f->wait_ticks = 0;
        f->direction = static_cast<signed char>(dir);
        return 1;
    }
    return 0;
}
