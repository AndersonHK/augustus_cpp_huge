#include "formation_legion.h"

#include "building/building.h"
#include "building/building_type_registry_internal.h"
#include "city/figures.h"
#include "city/games.h"
#include "city/military.h"
#include "city/warning.h"
#include "core/calc.h"
#include "figure/enemy_army.h"
#include "figure/figure.h"
#include "figure/route.h"
#include "map/building.h"
#include "map/figure.h"
#include "map/grid.h"
#include "map/routing.h"
#include "scenario/distant_battle.h"

int formation_legion_create_for_fort(Building &fort)
{
    formation_calculate_legion_totals();

    formation *m = formation_create_legion(fort);
    if (!m->id) {
        return 0;
    }
    Figure *standard = Figure::create(FIGURE_FORT_STANDARD, 0, 0, DIR_0_TOP);
    standard->set_home_building(&fort);
    standard->formation_id = m->id;
    m->standard_figure_id = standard->id();

    return m->id;
}

void formation_legion_delete_for_fort(const Building &fort)
{
    if (formation *m = fort.formation_object()) {
        if (m->in_use) {
            if (m->standard_figure_id) {
                Figure::get(m->standard_figure_id)->remove();
            }
            m->kill_figures();
            formation_clear(static_cast<int>(m->id));
            formation_calculate_legion_totals();
        }
    }
}

int formation_legion_recruits_needed(void)
{
    for (int i = 1; i < formation_count(); i++) {
        formation *m = formation_get(i);
        if (m->in_use && m->is_legion && m->legion_recruit_type != LEGION_RECRUIT_NONE &&
            m->num_figures < m->barracks_recruit_capacity()) {
            return 1;
        }
    }
    return 0;
}

void formation_legion_update_recruit_status(const Building &fort)
{
    formation *m = fort.formation_object();
    if (!m) {
        return;
    }
    m->legion_recruit_type = LEGION_RECRUIT_NONE;
    if (!m->is_at_fort || m->cursed_by_mars) {
        return;
    }
    if (m->num_figures < m->barracks_recruit_capacity()) {
        m->legion_recruit_type = m->declared_recruit_type();
    } else if (m->barracks_recruit_overflow_count() > 0) {
        m->mark_barracks_recruit_overflow_returning();
        formation_calculate_figures();
    }
}

void formation_legion_change_layout(formation *m, const char *layout_key)
{
    const FormationLayoutDef *definition = formation_layout_registry_impl::find_layout(layout_key);
    if (!m || !definition) {
        return;
    }
    if (definition->matches_key("mop_up") && !m->uses_layout("mop_up")) {
        m->prev.layout_definition = m->layout_type();
    }
    m->layout_definition = definition;
}

void formation_legion_restore_layout(formation *m)
{
    if (m->uses_layout("mop_up")) {
        m->layout_definition = m->prev.layout_definition ?
            m->prev.layout_definition :
            formation_layout_registry_impl::find_layout("column");
    }
}

void formation_legion_move_to(formation *m, const map_tile *tile)
{
    const Route::DistanceQuery route = Route::DistanceQuery::fromPoint({ m->x_home, m->y_home });
    if (!route.distanceTo(tile->grid_offset)) {
        return; // unable to route there
    }
    if (tile->x == m->x_home && tile->y == m->y_home) {
        return; // use formation_legion_return_home
    }
    if (m->cursed_by_mars) {
        return;
    }
    m->standard_x = tile->x;
    m->standard_y = tile->y;
    m->is_at_fort = 0;
    m->target_formation_id = 0;

    int figure_id = map_figure_at(tile->grid_offset);
    while (figure_id) {
        Figure *f = Figure::get(figure_id);
        if (f->formation_id) {
            formation *l = formation_get(f->formation_id);
            if (!l->is_legion) {
                m->target_formation_id = l->id;
                break;
            }
        }

        figure_id = f->next_figure_id_on_same_tile;
    }

    if (m->morale <= 20) {
        city_warning_show(WARNING_LEGION_MORALE_TOO_LOW, translation_for_key("TR_CITY_WARNING_LEGION_MORALE_TOO_LOW"));
    }
    m->send_non_combat_figures_to_standard();
}

void formation_legion_return_home(formation *m)
{
    const Route::DistanceQuery route = Route::DistanceQuery::fromPoint({ m->x_home, m->y_home });
    if (!route.distanceTo(map_grid_offset(m->x, m->y))) {
        return; // unable to route home
    }
    if (m->cursed_by_mars) {
        return;
    }
    m->is_at_fort = 1;
    formation_legion_restore_layout(m);
    m->send_non_combat_figures_to_fort();
}

void formation_legion_return_home_all(void)
{
    for (int i = 1; i < formation_count(); i++) {
        formation *m = formation_get(i);
        if (m->in_use && m->is_legion && !m->is_at_fort && !m->in_distant_battle) {
            formation_legion_return_home(m);
        }
    }
}

static int dispatch_soldiers(formation *m)
{
    m->in_distant_battle = 1;
    m->is_at_fort = 0;
    const int figure_count = m->figure_count();
    m->set_alive_figures_action(FIGURE_ACTION_87_SOLDIER_GOING_TO_DISTANT_BATTLE);
    int strength_factor = m->legion_distant_battle_strength_factor();
    if (city_games_naval_battle_distant_battle_bonus_active()) {
        strength_factor += 1;
    }
    return strength_factor * figure_count;
}

void formation_legions_dispatch_to_distant_battle(void)
{
    int num_legions = 0;
    int roman_strength = 0;
    for (int i = 1; i < formation_count(); i++) {
        formation *m = formation_get(i);
        if (m->in_use && m->is_legion && m->empire_service && m->has_figures()) {
            roman_strength += dispatch_soldiers(m);
            num_legions++;
        }
    }
    // Protect from overflow -> only stores 1 unsigned byte
    if (roman_strength > 255) {
        roman_strength = 255;
    }
    if (num_legions > 0) {
        city_games_remove_naval_battle_distant_battle_bonus();
        city_military_dispatch_to_distant_battle(roman_strength);
    }
}

static void kill_soldiers(formation *m, int kill_percentage)
{
    formation_change_morale(m, -75);
    int soldiers_total = m->count_alive_figures();
    int soldiers_to_kill = calc_adjust_with_percentage(soldiers_total, kill_percentage);
    if (soldiers_to_kill >= soldiers_total) {
        m->is_at_fort = 1;
        m->in_distant_battle = 0;
    }
    m->kill_alive_figures(soldiers_to_kill);
}

void formation_legions_kill_in_distant_battle(int kill_percentage)
{
    for (int i = 1; i < formation_count(); i++) {
        formation *m = formation_get(i);
        if (m->in_use && m->is_legion && m->in_distant_battle) {
            kill_soldiers(m, kill_percentage);
        }
    }
}

static void return_soldiers(formation *m)
{
    m->in_distant_battle = 0;
    m->set_alive_figures_action(FIGURE_ACTION_88_SOLDIER_RETURNING_FROM_DISTANT_BATTLE, true);
}

void formation_legions_return_from_distant_battle(void)
{
    for (int i = 1; i < formation_count(); i++) {
        formation *m = formation_get(i);
        if (m->in_use && m->is_legion && m->in_distant_battle) {
            return_soldiers(m);
        }
    }
}

int formation_legion_curse(void)
{
    formation *best_legion = 0;
    int best_legion_weight = 0;
    for (int i = 1; i < formation_count(); i++) {
        formation *m = formation_get(i);
        if (m->in_use == 1 && m->is_legion) {
            int weight = m->legion_curse_weight();
            if (weight > best_legion_weight) {
                best_legion_weight = weight;
                best_legion = m;
            }
        }
    }
    if (!best_legion) {
        return 0;
    }
    best_legion->set_all_figures_action(FIGURE_ACTION_82_SOLDIER_RETURNING_TO_BARRACKS);
    best_legion->cursed_by_mars = 96;
    formation_calculate_figures();
    return 1;
}

static int is_legion(Figure *f)
{
    if (f->is_legion() || f->type == FIGURE_FORT_STANDARD) {
        return f->formation_id;
    }
    return 0;
}

static int is_herd(Figure *f)
{
    if (f->is_herd()) {
        return f->formation_id;
    }
    return 0;
}

int formation_legion_at_grid_offset(int grid_offset)
{
    return map_figure_foreach_until(grid_offset, is_legion);
}

int formation_legion_or_herd_at_grid_offset(int grid_offset)
{
    int formation_id = map_figure_foreach_until(grid_offset, is_legion);
    if (formation_id) {
        return formation_id;
    }
    return map_figure_foreach_until(grid_offset, is_herd);
}

int formation_legion_at_building(int grid_offset)
{
    if (map_building_exists_at(grid_offset)) {
        Building &b = map_building_at(grid_offset);
        const building_type fort_ground = building_type_registry_impl::runtime_id_from_text("fort_ground");
        const building_type type = b.type ? b.type->type() : BUILDING_NONE;
        if (b.is_in_use() && (building_is_fort(type) ||
                (fort_ground != BUILDING_NONE && type == fort_ground))) {
            const formation *m = b.formation_object();
            return m ? static_cast<int>(m->id) : 0;
        }
    }
    return 0;
}

void formation_legion_update(void)
{
    for (int i = 1; i < formation_count(); i++) {
        formation *m = formation_get(i);
        if (m->in_use != 1 || !m->is_legion) {
            continue;
        }
        formation_decrease_monthly_counters(m);
        if (city_figures_enemies() <= 0) {
            formation_clear_monthly_counters(m);
        }
        if (m->has_figure_in_action(FIGURE_ACTION_150_ATTACK)) {
            formation_record_fight(m);
        }
        if (formation_has_low_morale(m)) {
            // flee back to fort
            m->set_non_combat_figures_action(FIGURE_ACTION_148_FLEEING, true);
        } else if (m->uses_layout("mop_up")) {
            if (enemy_army_total_enemy_formations() +
                city_figures_rioters() +
                city_figures_attacking_natives() > 0) {
                m->set_non_combat_figures_action(FIGURE_ACTION_86_SOLDIER_MOPPING_UP);
            } else {
                formation_legion_restore_layout(m);
            }
        }
    }
}

void formation_legion_decrease_damage(void)
{
    for (int i = 1; i < formation_count(); i++) {
        formation *m = formation_get(i);
        if (!m || !m->in_use || !m->is_legion) {
            continue;
        }
        m->for_each_alive_figure([](Figure &figure, int) {
            if (figure.action_state == FIGURE_ACTION_80_SOLDIER_AT_REST && figure.damage) {
                figure.damage--;
            }
        });
    }
}
