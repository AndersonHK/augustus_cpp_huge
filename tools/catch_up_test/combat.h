#pragma once

#include "building/destruction.h"
#include "figure/figure.h"
#include "figure/movement.h"
#include "figuretype/missile.h"
#include "game/save_version.h"
#include "map/building.h"
#include "map/figure.h"
#include "map/grid.h"
#include "map/terrain.h"
#include <cstdio>
#include <memory>
#include <stdexcept>
#include <vector>

// These fixtures deliberately damage the loaded city. The caller reloads it before the soak.
inline bool run_combat_runtime_test()
{
    auto require = [](bool value, const char *message) { if (!value) throw std::runtime_error(message); };
    try {
        int x = -1, y = -1;
        for (int yy = 17; yy < map_grid_height() - 17 && x < 0; ++yy) for (int xx = 17; xx < map_grid_width() - 17; ++xx) {
            bool empty = true;
            for (int dy = -16; dy <= 16 && empty; ++dy) for (int dx = -16; dx <= 16; ++dx) {
                if (!map_grid_is_inside(xx + dx, yy + dy, 1) || map_figure_at(map_grid_offset(xx + dx, yy + dy))) { empty = false; break; }
            }
            if (empty) { x = xx; y = yy; break; }
        }
        require(x >= 0, "Projectile fixture requires an unoccupied flight area");
        auto remove = [](Figure *f) { if (f && f->id()) f->remove(); };
        using TestFigure = std::unique_ptr<Figure, decltype(remove)>;
        TestFigure launcher(Figure::create(FIGURE_BALLISTA, x, y, DIR_0_TOP), remove);
        require(launcher && launcher->id(), "Could not create ballista fixture");
        const int destinations[][2] = {{0, -15}, {15, -15}, {15, 0}, {15, 15}, {0, 15}, {-15, 15}, {-15, 0}, {-15, -15}, {3, 11}, {-11, 3}, {11, -3}, {-3, -11}};
        const figure_type missiles[] = {FIGURE_BOLT, FIGURE_JAVELIN, FIGURE_FRIENDLY_ARROW, FIGURE_ARROW, FIGURE_SPEAR, FIGURE_CATAPULT_MISSILE};
        for (const auto missile : missiles) {
            const bool hostile = missile == FIGURE_ARROW || missile == FIGURE_SPEAR || missile == FIGURE_CATAPULT_MISSILE;
            for (const auto &delta : destinations) {
                TestFigure target(Figure::create(hostile ? FIGURE_ENGINEER : FIGURE_ENEMY43_SPEAR, x + delta[0], y + delta[1], DIR_0_TOP), remove);
                require(target && target->id(), "Could not create enemy fixture");
                figure_create_missile(launcher->id(), x, y, target->x, target->y, missile);
                Figure *bolt = nullptr;
                for (unsigned int i = 1; i < Figure::count(); ++i) {
                    Figure *candidate = Figure::get(i);
                    if (candidate->state == FIGURE_STATE_ALIVE && candidate->type == missile && candidate->last_destination_id == static_cast<int>(launcher->id())) bolt = candidate;
                }
                TestFigure projectile(bolt, remove);
                require(bolt, "Ballista did not create its projectile");
                figure_projectile_action(bolt);
                require(bolt->state == FIGURE_STATE_ALIVE, "New projectile expired on its first update");
                for (int tick = 1; tick < 120 && bolt->state == FIGURE_STATE_ALIVE; ++tick) figure_projectile_action(bolt);
                if (!target->damage) fprintf(stderr, "Projectile miss: type=%d delta=%d,%d final=%d,%d cc=%d,%d remaining=%d,%d damage=%d\n", missile, delta[0], delta[1], bolt->x, bolt->y, bolt->cross_country_x, bolt->cross_country_y, bolt->cc_delta_x, bolt->cc_delta_y, target->damage);
                require(target->damage > 0, "Projectile failed to reach and damage its stationary target");
                if (missile == FIGURE_BOLT) require(target->action_state == FIGURE_ACTION_149_CORPSE, "Ballista bolt failed to kill an unarmored stationary enemy");
            }
        }
        fprintf(stdout, "Combat: all six projectile types hit in eight directions and four shallow/steep trajectories at up to 15 tiles\n");
        figure_create_explosion_cloud(x, y, 1, 0);
        int clouds = 0;
        for (unsigned int i = 1; i < Figure::count(); ++i) {
            Figure *f = Figure::get(i);
            if (f->state != FIGURE_STATE_ALIVE || f->type != FIGURE_EXPLOSION || f->x != x || f->y != y) continue;
            TestFigure cloud(f, remove);
            ++clouds;
            figure_explosion_cloud_action(f);
            require(f->state == FIGURE_STATE_ALIVE && f->progress_on_tile == 1, "Explosion cloud expired on its first update");
            for (int tick = 1; tick < 45; ++tick) figure_explosion_cloud_action(f);
            require(f->state == FIGURE_STATE_DEAD, "Explosion cloud did not expire at the end of its lifetime");
        }
        require(clouds == 16, "Explosion fixture did not create all clouds");
        fprintf(stdout, "Combat: all 16 explosion clouds retained their animation lifetime\n");

        int wall = -1;
        for (int yy = 1; yy < map_grid_height() - 1 && wall < 0; ++yy) for (int xx = 1; xx < map_grid_width() - 1; ++xx) {
            const int offset = map_grid_offset(xx, yy);
            if (map_building_exists_at(offset) && map_building_at(offset).matches("wall") && building_hit_points_at(offset) > 255) { wall = offset; break; }
        }
        require(wall >= 0, "Combat fixture requires a wall with more than 255 hit points");
        const int hp = building_hit_points_at(wall);
        map_building_damage_clear(wall);
        for (int i = 0; i < 300; ++i) map_building_damage_increase(wall);
        require(map_building_damage_at(wall) == 300, "Wall damage wrapped at the byte boundary");
        map_building_backup();
        map_building_damage_clear(wall);
        map_building_restore();
        require(map_building_damage_at(wall) == 300, "Wall damage was truncated by undo backup/restore");
        std::vector<uint8_t> buildings(GRID_SIZE * GRID_SIZE * 4), damage(buildings.size()), rubble(buildings.size());
        buffer b{}, d{}, r{};
        buffer_init(&b, buildings.data(), buildings.size()); buffer_init(&d, damage.data(), damage.size()); buffer_init(&r, rubble.data(), rubble.size());
        map_building_save_state(&b, &d, &r);
        require(!b.overflow && !d.overflow && !r.overflow, "Damage-grid save overflowed its buffers");
        buffer_reset(&b); buffer_reset(&d); buffer_reset(&r);
        map_building_load_state(&b, &d, &r, SAVE_GAME_CURRENT_VERSION);
        map_building_rebind_runtime_references();
        require(map_building_damage_at(wall) == 300, "Wall damage did not survive serialization");
        std::vector<uint8_t> legacy_damage(GRID_SIZE * GRID_SIZE);
        legacy_damage[wall] = 127;
        buffer legacy{}; buffer_init(&legacy, legacy_damage.data(), legacy_damage.size());
        buffer_reset(&b); buffer_reset(&r);
        map_building_load_state(&b, &legacy, &r, SAVE_GAME_LAST_BYTE_BUILDING_DAMAGE);
        require(map_building_damage_at(wall) == 127, "Legacy byte damage was not preserved during migration");
        buffer_reset(&b); buffer_reset(&d); buffer_reset(&r);
        map_building_load_state(&b, &d, &r, SAVE_GAME_CURRENT_VERSION);
        map_building_rebind_runtime_references();
        for (int i = 300; i <= hp; ++i) building_apply_enemy_damage(wall);
        require(!map_terrain_is(wall, TERRAIN_WALL), "Wall survived damage beyond its authored hit points");
        fprintf(stdout, "Combat: %d-HP wall accumulated damage beyond 255, survived backup/save/reload, and was destroyed\n", hp);
        return true;
    } catch (const std::exception &error) {
        fprintf(stderr, "Combat runtime test failed: %s\n", error.what());
        return false;
    }
}

struct CombatEncounterObservation {
    unsigned int figure_id;
    unsigned short created_sequence;
    int damage;
    int wall;
    int wall_damage;
};

inline std::vector<CombatEncounterObservation> observe_wall_attackers()
{
    std::vector<CombatEncounterObservation> result;
    for (unsigned int i = 1; i < Figure::count(); ++i) {
        Figure *f = Figure::get(i);
        if (f->state != FIGURE_STATE_ALIVE || !f->is_enemy() || f->direction != DIR_FIGURE_ATTACK || f->attack_direction < 0 || f->attack_direction >= 8) continue;
        const int wall = f->grid_offset + map_grid_direction_delta(f->attack_direction);
        if (!map_terrain_is(wall, TERRAIN_WALL)) continue;
        result.push_back({i, f->created_sequence, f->damage, wall, map_building_damage_at(wall)});
        fprintf(stdout, "Combat encounter before: enemy=%u x=%d y=%d damage=%d wall=%d wall_damage=%d hp=%d\n", i, f->x, f->y, f->damage, wall, map_building_damage_at(wall), building_hit_points_at(wall));
    }
    return result;
}

inline bool validate_combat_encounter(const std::vector<CombatEncounterObservation> &before)
{
    for (const auto &observation : before) {
        const Figure *f = Figure::get(observation.figure_id);
        const bool killed = f->state != FIGURE_STATE_ALIVE || f->created_sequence != observation.created_sequence || f->action_state == FIGURE_ACTION_149_CORPSE;
        const bool wall_destroyed = !map_terrain_is(observation.wall, TERRAIN_WALL);
        const int damage = map_building_damage_at(observation.wall);
        fprintf(stdout, "Combat encounter after: enemy=%u killed=%d damage=%d wall_destroyed=%d wall_damage=%d\n", observation.figure_id, killed, f->damage, wall_destroyed, damage);
        if (!killed && !wall_destroyed && f->damage <= observation.damage && damage <= observation.wall_damage) {
            fprintf(stderr, "Combat encounter made no damage progress for enemy %u\n", observation.figure_id);
            return false;
        }
    }
    return true;
}
