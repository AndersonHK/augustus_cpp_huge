#include "building/building_record.h"
#include "native.h"

#include "building/building.h"
#include "building/building_type_registry_internal.h"
#include "building/image.h"
#include "city/population.h"
#include "core/image.h"
#include "city/figures.h"
#include "city/military.h"
#include "figure/combat.h"
#include "figure/formation.h"
#include "figure/image.h"
#include "figure/movement.h"
#include "figure/route.h"
#include "game/time.h"
#include "map/image.h"
#include "map/random.h"
#include "map/road_access.h"
#include "map/terrain.h"
#include "sound/speech.h"

#define NATIVE_ATTACK_SOUND_DELAY 60000

static time_millis native_attack_last_played = 0;

building *NativeBuildingSpawner::record() const
{
    return const_cast<building *>(owner_.record());
}

bool NativeBuildingSpawner::has_primary_figure(figure_type type) const
{
    building *b = record();
    if (!b || !b->figure_id) {
        return false;
    }
    Figure *figure = Figure::get(b->figure_id);
    if (figure && !figure->is_dead() && figure->type == type &&
        figure->home_building_id() == static_cast<unsigned int>(owner_.id)) {
        return true;
    }
    b->figure_id = 0;
    return false;
}

Figure *NativeBuildingSpawner::create_primary_figure(
    figure_type type,
    int x,
    int y,
    int action_state,
    bool roaming)
{
    building *b = record();
    if (!b) {
        return nullptr;
    }
    Figure *figure = Figure::create(type, x, y, DIR_0_TOP);
    if (!figure) {
        return nullptr;
    }
    figure->action_state = static_cast<unsigned char>(action_state);
    if (!figure->set_home_building(&owner_)) {
        figure->remove();
        return nullptr;
    }
    b->figure_id = figure->id();
    if (roaming) {
        figure_movement_init_roaming(figure);
    }
    return figure;
}

void NativeBuildingSpawner::spawn_hut_resident()
{
    building *b = record();
    if (!b) {
        return;
    }
    if (owner_.matches("native_hut")) {
        map_image_set(b->grid_offset, image_group(GROUP_BUILDING_NATIVE) + (map_random_get(b->grid_offset) & 1));
    } else {
        map_image_set(b->grid_offset, building_image_get(&owner_));
    }
    if (has_primary_figure(FIGURE_INDIGENOUS_NATIVE)) {
        return;
    }
    int x_out, y_out;
    if (b->subtype.native_meeting_center_id > 0 &&
        map_terrain_get_adjacent_road_or_clear_land(owner_, &x_out, &y_out)) {
        b->figure_spawn_delay++;
        if (b->figure_spawn_delay > game_time_scale_legacy_day_ticks(4)) {
            b->figure_spawn_delay = 0;
            create_primary_figure(
                FIGURE_INDIGENOUS_NATIVE, x_out, y_out, FIGURE_ACTION_158_NATIVE_CREATED, false);
        }
    }
}

void NativeBuildingSpawner::spawn_meeting_trader()
{
    building *b = record();
    if (!b) {
        return;
    }
    owner_.add_map_tiles(building_image_get(&owner_));
    if (b->native_anger >= 100 || has_primary_figure(FIGURE_NATIVE_TRADER)) {
        return;
    }
    int x_out, y_out;
    if (map_terrain_get_adjacent_road_or_clear_land(owner_, &x_out, &y_out)) {
        b->figure_spawn_delay++;
        if (b->figure_spawn_delay > game_time_scale_legacy_day_ticks(8)) {
            b->figure_spawn_delay = 0;
            create_primary_figure(
                FIGURE_NATIVE_TRADER, x_out, y_out, FIGURE_ACTION_162_NATIVE_TRADER_CREATED, false);
        }
    }
}

void NativeBuildingSpawner::spawn_missionary()
{
    building *b = record();
    if (!b || has_primary_figure(FIGURE_MISSIONARY)) {
        return;
    }
    map_point road;
    if (map_has_road_access_building(b->x, b->y, &road) && city_population() > 0) {
        b->figure_spawn_delay++;
        if (b->figure_spawn_delay > game_time_scale_legacy_day_ticks(1)) {
            b->figure_spawn_delay = 0;
            create_primary_figure(FIGURE_MISSIONARY, road.x, road.y, FIGURE_ACTION_125_ROAMING, true);
        }
    }
}

void NativeBuildingSpawner::advance_crop_graphics()
{
    building *b = record();
    const int option_count = owner_.type ? owner_.type->graphics().production_progress_option_count() : 0;
    if (!b || option_count <= 0) {
        return;
    }
    b->data.industry.progress++;
    if (b->data.industry.progress >= option_count) {
        b->data.industry.progress = 0;
    }
    owner_.refresh_graphic_if_native();
}

void figure_indigenous_native_action(Figure *f)
{
    time_millis now = time_get_millis();

    building *b = f && f->building ? const_cast<building *>(f->building->record()) : nullptr;
    if (!b) {
        f->state = FIGURE_STATE_DEAD;
        return;
    }
    f->terrain_usage = TERRAIN_USAGE_ANY;
    f->use_cross_country = 0;
    f->max_roam_length = 800;
    if (b->state != BUILDING_STATE_IN_USE || (unsigned int) b->figure_id != f->id()) {
        f->state = FIGURE_STATE_DEAD;
    }
    figure_image_increase_offset(f, 12);
    switch (f->action_state) {
        case FIGURE_ACTION_150_ATTACK:
            figure_combat_handle_attack(f);
            break;
        case FIGURE_ACTION_149_CORPSE:
            figure_combat_handle_corpse(f);
            break;
        case FIGURE_ACTION_156_NATIVE_GOING_TO_MEETING_CENTER:
            figure_movement_move_ticks(f, 1);
            if (f->direction == DIR_FIGURE_AT_DESTINATION) {
                f->action_state = FIGURE_ACTION_157_NATIVE_RETURNING_FROM_MEETING;
                f->destination_x = f->source_x;
                f->destination_y = f->source_y;
            } else if (f->direction == DIR_FIGURE_REROUTE || f->direction == DIR_FIGURE_LOST) {
                f->state = FIGURE_STATE_DEAD;
            }
            break;
        case FIGURE_ACTION_157_NATIVE_RETURNING_FROM_MEETING:
            figure_movement_move_ticks(f, 1);
            if (f->direction == DIR_FIGURE_AT_DESTINATION ||
                f->direction == DIR_FIGURE_REROUTE ||
                f->direction == DIR_FIGURE_LOST) {
                f->state = FIGURE_STATE_DEAD;
            }
            break;
        case FIGURE_ACTION_158_NATIVE_CREATED:
            f->image_offset = 0;
            f->wait_ticks++;
            if ((unsigned int) f->wait_ticks > 10 + (f->id() & 3)) {
                f->wait_ticks = 0;
                const formation *m = formation_get(NATIVE_FORMATION);
                if (!city_military_is_native_attack_active() || m->months_low_morale > 0) {
                    int x_tile, y_tile;
                    Building *meeting_building =
                        Building::get(static_cast<unsigned int>(b->subtype.native_meeting_center_id));
                    if (!meeting_building) {
                        f->state = FIGURE_STATE_DEAD;
                        break;
                    }
                    if (map_terrain_get_adjacent_road_or_clear_land(
                            *meeting_building, &x_tile, &y_tile)) {
                        f->action_state = FIGURE_ACTION_156_NATIVE_GOING_TO_MEETING_CENTER;
                        f->destination_x = static_cast<unsigned char>(x_tile);
                        f->destination_y = static_cast<unsigned char>(y_tile);
                    }
                } else {
                    f->action_state = FIGURE_ACTION_159_NATIVE_ATTACKING;
                    f->destination_x = static_cast<unsigned char>(m->destination_x);
                    f->destination_y = static_cast<unsigned char>(m->destination_y);
                    Building *destination = Building::get(static_cast<unsigned int>(m->destination_building_id));
                    f->set_destination_building(destination);
                    if (native_attack_last_played == 0 || (now - native_attack_last_played) > NATIVE_ATTACK_SOUND_DELAY) {
                        sound_speech_play_file("wavs/barbarian_war_cry.wav");
                        native_attack_last_played = now;
                    }
                }
                Route::remove(f);
            }
            break;
        case FIGURE_ACTION_159_NATIVE_ATTACKING:
            city_figures_add_attacking_native();
            f->terrain_usage = TERRAIN_USAGE_ENEMY;
            figure_movement_move_ticks(f, 1);
            if (f->direction == DIR_FIGURE_AT_DESTINATION ||
                f->direction == DIR_FIGURE_REROUTE ||
                f->direction == DIR_FIGURE_LOST) {
                f->action_state = FIGURE_ACTION_158_NATIVE_CREATED;
            }
            break;
    }
    int dir;
    if (f->action_state == FIGURE_ACTION_150_ATTACK || f->direction == DIR_FIGURE_ATTACK) {
        dir = f->attack_direction;
    } else if (f->direction < 8) {
        dir = f->direction;
    } else {
        dir = f->previous_tile_direction;
    }
    dir = figure_image_normalize_direction(dir);

    f->is_enemy_image = 1;
    if (f->action_state == FIGURE_ACTION_150_ATTACK) {
        const int frame_offset = f->attack_image_offset >= 12 ? (f->attack_image_offset - 12) / 2 : 0;
        f->select_legacy_directional_frame_image(393, dir, frame_offset);
    } else if (f->action_state == FIGURE_ACTION_149_CORPSE) {
        f->select_legacy_corpse_image(441);
    } else if (f->direction == DIR_FIGURE_ATTACK) {
        f->select_legacy_directional_frame_image(393, dir, f->image_offset / 2);
    } else if (f->action_state == FIGURE_ACTION_159_NATIVE_ATTACKING) {
        f->select_legacy_directional_frame_image(297, dir, f->image_offset);
    } else {
        f->select_legacy_directional_frame_image(201, dir, f->image_offset);
    }
}
