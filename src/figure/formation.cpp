#include "formation.h"

#include "building/building.h"
#include "building/count.h"
#include "building/monument.h"
#include "city/data_private.h"
#include "city/military.h"
#include "core/calc.h"
#include "core/config.h"
#include "core/log.h"
#include "figure/enemy_army.h"
#include "figure/combat.h"
#include "figure/figure.h"
#include "figure/formation_enemy.h"
#include "figure/formation_herd.h"
#include "figure/formation_layout.h"
#include "figure/formation_legion.h"
#include "figure/properties.h"
#include "figure/route.h"
#include "game/save_version.h"
#include "game/cheats.h"
#include "map/grid.h"
#include "sound/effect.h"
#include "sound/speech.h"
#include "widget/sidebar/military.h"

#include <stdio.h>
#include <vector>


constexpr int ORIGINAL_BUFFER_SIZE_PER_FORMATION = 128;
constexpr int BUFFER_SIZE_FOR_10_LEGIONS = 128;
constexpr int LEGACY_BUFFER_SIZE_PER_FORMATION = 256;
constexpr int EXTENDED_FORMATION_ROSTER_SAVE_SLOTS = 256;
constexpr int CURRENT_BUFFER_SIZE_PER_FORMATION =
    LEGACY_BUFFER_SIZE_PER_FORMATION + sizeof(int32_t) +
    EXTENDED_FORMATION_ROSTER_SAVE_SLOTS * sizeof(int32_t);
constexpr int MAX_LEGIONS_REGALIA = 21; // +1 to keep indexing simple with 1 start
constexpr int HERD_ANIMAL_MOVE_WAIT_TICKS = 401;

static std::vector<formation> formations;
static struct {
    int id_last_in_use;
    int id_last_legion;
    int num_legions;
    unsigned int selected_formation;
} data;
static int8_t available_regalia[MAX_LEGIONS_REGALIA] = { 1 };

static unsigned int formation_pool_size(void)
{
    return static_cast<unsigned int>(formations.size());
}

static formation *formation_slot(unsigned int id)
{
    return id < formations.size() ? &formations[id] : nullptr;
}

static void reset_formation_slot(unsigned int id)
{
    formations[id] = {};
    formations[id].id = id;
}

static bool formation_is_active(const formation &m)
{
    return m.in_use != 0;
}

static formation *append_formation(void)
{
    formations.emplace_back();
    reset_formation_slot(formation_pool_size() - 1);
    return &formations.back();
}

static formation *new_formation_after_index(unsigned int start)
{
    while (start > formation_pool_size()) {
        append_formation();
    }
    for (unsigned int i = start; i < formation_pool_size(); i++) {
        if (!formation_is_active(formations[i])) {
            reset_formation_slot(i);
            return &formations[i];
        }
    }
    return append_formation();
}

static void trim_inactive_formations(void)
{
    while (formations.size() > 1 && !formation_is_active(formations.back())) {
        formations.pop_back();
    }
}

static void resize_formations_for_load(unsigned int size)
{
    formations.resize(size);
    for (unsigned int i = 0; i < size; i++) {
        reset_formation_slot(i);
    }
}

void formations_clear(void)
{
    formations.clear();
    if (!append_formation()) { // Ignore first formation
        log_error("Unable to create the formations array. The game will likely crash.", 0, 0);
    }
    data.id_last_in_use = 0;
    data.id_last_legion = 0;
    data.num_legions = 0;
    data.selected_formation = 0;
}

void formation_clear(int formation_id)
{
    formation_slot(formation_id)->in_use = 0;
    trim_inactive_formations();
}

int formation_assign_available_legion_regalia(formation *m, int preferred_regalia_id)
{
    if (preferred_regalia_id >= 0 && preferred_regalia_id < MAX_LEGIONS_REGALIA) {// Try preferred first
        if (available_regalia[preferred_regalia_id]) {
            available_regalia[preferred_regalia_id] = 0;
            m->legion_flag_id = widget_sidebar_military_get_standard_image(preferred_regalia_id);
            m->legion_name_id = widget_sidebar_military_get_legion_name_id(preferred_regalia_id);
            m->legion_name_group = widget_sidebar_military_get_legion_name_group(preferred_regalia_id);
            return preferred_regalia_id;
        }
    }

    for (int i = 0; i < MAX_LEGIONS_REGALIA; i++) {//if not, first available
        if (available_regalia[i]) {
            available_regalia[i] = 0;
            m->legion_flag_id = widget_sidebar_military_get_standard_image(i);
            m->legion_name_id = widget_sidebar_military_get_legion_name_id(i);
            m->legion_name_group = widget_sidebar_military_get_legion_name_group(i);
            return i;
        }
    }
    return 0;// no available regalia
}

void formation_release_legion_regalia(formation *m)
{
    int regalia_id = m->legion_id;
    if (regalia_id > 0 && regalia_id <= MAX_LEGIONS_REGALIA) {
        available_regalia[regalia_id] = 1; //mark regalia as available again
    }
}

static void formation_release_all_legion_regalia(void)
{
    for (int i = 0; i < MAX_LEGIONS_REGALIA; i++) {
        available_regalia[i] = 1;
    }
}

void formation_refresh_regalia(void)
{
    formation_release_all_legion_regalia();
    for (int i = 1; i < formation_count(); i++) {
        formation *m = formation_get(i);
        if (m->in_use && m->is_legion) {
            formation_assign_available_legion_regalia(m, m->legion_id);
        }
    }
}

void formation::bind_legion_definition_from_fort(const Building &fort)
{
    if (!is_legion || !fort.id || !fort.type) {
        return;
    }
    set_formation_type(fort.type->military().formation_type());
    const ::figure_type fort_figure_type = static_cast<::figure_type>(fort.fort_figure_type());
    if (fort_figure_type != FIGURE_NONE) {
        figure_type = fort_figure_type;
    }
}

void formation::refresh_legion_definition_from_home()
{
    if (!is_legion || building_id <= 0) {
        return;
    }
    bind_legion_definition_from_fort(Building(building_get(building_id)));
}

void formation::initialize_legion_from_fort(const Building &fort, int assigned_legion_id)
{
    faction_id = 1;
    in_use = 1;
    is_legion = 1;
    figure_type = static_cast<::figure_type>(fort.fort_figure_type());
    building_id = fort.id;
    refresh_legion_definition_from_home();
    layout = FORMATION_DOUBLE_LINE_1;
    morale = 50;
    is_at_fort = 1;
    legion_id = assigned_legion_id;
    if (legion_id >= 20) {
        legion_id = 20;
    }

    Building fort_ground(building_get(fort.next_part_id()));
    x = standard_x = x_home = fort_ground.x();
    y = standard_y = y_home = fort_ground.y();
    target_formation_id = 0;
}

int formation::base_morale_limit() const
{
    if (has_figure_type(FIGURE_FORT_LEGIONARY)) {
        return has_military_training ? 90 : 80;
    }
    if (has_figure_type(FIGURE_ENEMY_CAESAR_LEGIONARY)) {
        return 100;
    }
    if (has_any_figure_type({
        FIGURE_FORT_JAVELIN,
        FIGURE_FORT_MOUNTED,
        FIGURE_FORT_INFANTRY,
        FIGURE_FORT_ARCHER
    })) {
        return has_military_training ? 70 : 60;
    }
    switch (enemy_type) {
        case ENEMY_0_BARBARIAN:
        case ENEMY_1_NUMIDIAN:
        case ENEMY_2_GAUL:
        case ENEMY_3_CELT:
        case ENEMY_4_GOTH:
            return 80;
        case ENEMY_8_GREEK:
        case ENEMY_10_CARTHAGINIAN:
            return 90;
        default:
            return 70;
    }
}

int formation::declared_recruit_type() const
{
    return formation_type_definition ?
        formation_type_definition->primary_recruit_type() :
        LEGION_RECRUIT_NONE;
}

int formation::legion_distant_battle_strength_factor() const
{
    if (has_military_training) {
        return has_figure_type(FIGURE_FORT_LEGIONARY) ? 3 : 2;
    }
    return has_figure_type(FIGURE_FORT_LEGIONARY) ? 2 : 1;
}

int formation::legion_curse_weight() const
{
    int weight = total_figure_count();
    if (has_figure_type(FIGURE_FORT_LEGIONARY)) {
        weight *= 2;
    }
    return weight;
}

int formation::legacy_storage_slot_count() const
{
    return LEGACY_FORMATION_ROSTER_SLOTS;
}

int formation::barracks_recruit_capacity() const
{
    const int capacity = slot_capacity();
    if (!is_legion) {
        return capacity;
    }
    return std::min(capacity, legacy_storage_slot_count());
}

int formation::barracks_recruit_overflow_count() const
{
    const int overflow = num_figures - barracks_recruit_capacity();
    return overflow > 0 ? overflow : 0;
}

void formation::ensure_roster_capacity(int capacity)
{
    if (capacity <= 0) {
        capacity = LEGACY_FORMATION_ROSTER_SLOTS;
    }
    if (static_cast<int>(figures.size()) < capacity) {
        figures.resize(static_cast<size_t>(capacity), 0);
    }
}

int formation::roster_figure_id(int slot) const
{
    if (slot < 0 || slot >= static_cast<int>(figures.size())) {
        return 0;
    }
    return figures[static_cast<size_t>(slot)];
}

void formation::write_legacy_figure_slots(buffer *buf) const
{
    for (int slot = 0; slot < legacy_storage_slot_count(); slot++) {
        buffer_write_i16(buf, roster_figure_id(slot));
    }
}

void formation::read_legacy_figure_slots(buffer *buf)
{
    ensure_roster_capacity(legacy_storage_slot_count());
    std::fill(figures.begin(), figures.end(), 0);
    for (int slot = 0; slot < legacy_storage_slot_count(); slot++) {
        figures[slot] = buffer_read_i16(buf);
    }
}

void formation::write_extended_figure_slots(buffer *buf) const
{
    const int slot_count = slot_capacity();
    const int slots_to_write = slot_count < EXTENDED_FORMATION_ROSTER_SAVE_SLOTS ?
        slot_count :
        EXTENDED_FORMATION_ROSTER_SAVE_SLOTS;
    buffer_write_i32(buf, slots_to_write);
    for (int slot = 0; slot < EXTENDED_FORMATION_ROSTER_SAVE_SLOTS; slot++) {
        buffer_write_i32(buf, slot < slots_to_write ? roster_figure_id(slot) : 0);
    }
}

void formation::read_extended_figure_slots(buffer *buf, int formation_buf_size)
{
    if (formation_buf_size <= LEGACY_BUFFER_SIZE_PER_FORMATION) {
        return;
    }
    const int extension_bytes = formation_buf_size - LEGACY_BUFFER_SIZE_PER_FORMATION;
    if (extension_bytes < static_cast<int>(sizeof(int32_t))) {
        buffer_skip(buf, extension_bytes);
        return;
    }

    const int saved_slot_count = buffer_read_i32(buf);
    const int extension_slot_capacity = (extension_bytes - static_cast<int>(sizeof(int32_t))) /
        static_cast<int>(sizeof(int32_t));
    const int saved_slots_in_current_record =
        extension_slot_capacity < EXTENDED_FORMATION_ROSTER_SAVE_SLOTS ?
            extension_slot_capacity :
            EXTENDED_FORMATION_ROSTER_SAVE_SLOTS;
    const int usable_saved_slots = saved_slot_count < saved_slots_in_current_record ?
        saved_slot_count :
        saved_slots_in_current_record;
    const int slots_to_read = usable_saved_slots > 0 ? usable_saved_slots : 0;
    ensure_roster_capacity(slots_to_read);

    for (int slot = 0; slot < saved_slots_in_current_record; slot++) {
        const int figure_id = buffer_read_i32(buf);
        if (slot < slots_to_read) {
            figures[slot] = figure_id;
        }
    }

    const int bytes_read = static_cast<int>(sizeof(int32_t)) +
        saved_slots_in_current_record * static_cast<int>(sizeof(int32_t));
    if (extension_bytes > bytes_read) {
        buffer_skip(buf, extension_bytes - bytes_read);
    }
}

std::vector<int> formation::layout_grid_offsets() const
{
    const FormationLayoutFootprint footprint = formation_type_definition ?
        formation_type_definition->layout_footprint() :
        formation_layout_legacy_footprint();
    const std::vector<FormationLayoutPosition> positions =
        formation_layout_positions(layout, figure_count(), declared_capacity(), footprint);
    if (positions.empty()) {
        return {};
    }

    const int base_offset = map_grid_offset(positions[0].x, positions[0].y);
    std::vector<int> offsets(positions.size(), 0);
    for (size_t index = 1; index < positions.size(); index++) {
        offsets[index] = map_grid_offset(positions[index].x, positions[index].y) - base_offset;
    }
    return offsets;
}

static bool figure_is_combat_locked(const Figure &f)
{
    return f.action_state == FIGURE_ACTION_149_CORPSE ||
        f.action_state == FIGURE_ACTION_150_ATTACK;
}

Figure *formation::first_figure() const
{
    const int figure_id = first_figure_id();
    return figure_id ? Figure::get(figure_id) : nullptr;
}

Figure *formation::first_alive_figure() const
{
    Figure *figure = first_figure();
    return figure && figure->state == FIGURE_STATE_ALIVE ? figure : nullptr;
}

int formation::count_alive_figures() const
{
    int alive_figures = 0;
    for_each_figure_id([&](int figure_id, int) {
        if (!Figure::get(figure_id)->is_dead()) {
            alive_figures++;
        }
    });
    return alive_figures;
}

int formation::count_figures_in_action(int action_state) const
{
    int matching_figures = 0;
    for_each_figure_id([&](int figure_id, int) {
        if (Figure::get(figure_id)->action_state == action_state) {
            matching_figures++;
        }
    });
    return matching_figures;
}

bool formation::has_figure_in_action(int action_state) const
{
    return count_figures_in_action(action_state) > 0;
}

bool formation::has_figure_attacking_live_legion() const
{
    return any_figure_id([](int figure_id, int) {
        Figure *f = Figure::get(figure_id);
        if (f->action_state == FIGURE_ACTION_150_ATTACK) {
            Figure *opponent = f->opponent.save_id() ? &f->opponent.get() : nullptr;
            return opponent && !opponent->is_dead() && opponent->is_legion();
        }
        return false;
    });
}

bool formation::is_fully_in_city() const
{
    return !any_figure_id([](int figure_id, int) {
        Figure *f = Figure::get(figure_id);
        return f->state != FIGURE_STATE_DEAD && f->is_ghost;
    });
}

void formation::kill_figures() const
{
    for_each_figure_id([](int figure_id, int) {
        Figure::get(figure_id)->state = FIGURE_STATE_DEAD;
    });
}

int formation::kill_alive_figures(int limit) const
{
    int killed = 0;
    for_each_figure_id([&](int figure_id, int) {
        if (killed >= limit) {
            return;
        }
        Figure *f = Figure::get(figure_id);
        if (!f->is_dead()) {
            f->state = FIGURE_STATE_DEAD;
            killed++;
        }
    });
    return killed;
}

void formation::set_all_figures_action(int action_state) const
{
    for_each_figure_id([&](int figure_id, int) {
        Figure::get(figure_id)->action_state = action_state;
    });
}

void formation::set_alive_figures_action(int action_state, bool mark_at_rest) const
{
    for_each_figure_id([&](int figure_id, int) {
        Figure *f = Figure::get(figure_id);
        if (!f->is_dead()) {
            f->action_state = action_state;
            if (mark_at_rest) {
                f->formation_at_rest = 1;
            }
        }
    });
}

void formation::set_non_combat_figures_action(int action_state, bool remove_route) const
{
    for_each_figure_id([&](int figure_id, int) {
        Figure *f = Figure::get(figure_id);
        if (figure_is_combat_locked(*f)) {
            return;
        }
        if (f->action_state == action_state) {
            return;
        }
        f->action_state = action_state;
        if (remove_route) {
            Route::remove(f);
        }
    });
}

void formation::reset_non_combat_figures_action(int action_state) const
{
    for_each_figure_id([&](int figure_id, int) {
        Figure *f = Figure::get(figure_id);
        if (figure_is_combat_locked(*f)) {
            return;
        }
        f->action_state = action_state;
        f->wait_ticks = 0;
    });
}

void formation::move_herd_animals(int attacking_animals) const
{
    for_each_figure_id([&](int figure_id, int) {
        Figure *f = Figure::get(figure_id);
        if (figure_is_combat_locked(*f)) {
            return;
        }
        f->wait_ticks = HERD_ANIMAL_MOVE_WAIT_TICKS;
        if (attacking_animals) {
            int target_id = figure_combat_get_target_for_wolf(f->x, f->y, 6);
            if (target_id) {
                Figure *target = Figure::get(target_id);
                f->action_state = FIGURE_ACTION_199_WOLF_ATTACKING;
                f->destination_x = target->x;
                f->destination_y = target->y;
                f->target_figure.retarget(*target);
                target->targeted_by_figure.retarget(*f);
                f->target_figure_created_sequence = target->created_sequence;
                Route::remove(f);
            } else {
                f->action_state = FIGURE_ACTION_196_HERD_ANIMAL_AT_REST;
            }
        } else {
            f->action_state = FIGURE_ACTION_196_HERD_ANIMAL_AT_REST;
        }
    });
}

void formation::mark_barracks_recruit_overflow_returning() const
{
    int too_many = barracks_recruit_overflow_count();
    for_each_figure_id_reverse([&](int figure_id, int) {
        if (too_many <= 0) {
            return;
        }
        Figure::get(figure_id)->action_state = FIGURE_ACTION_82_SOLDIER_RETURNING_TO_BARRACKS;
        too_many--;
    });
}

bool formation::prepare_legion_to_move()
{
    if (months_very_low_morale || months_low_morale > 1) {
        return false;
    }
    if (months_low_morale == 1) {
        formation_change_morale(this, 10);
    }
    return true;
}

void formation::send_non_combat_figures_to_standard()
{
    for_each_figure_id([&](int figure_id, int) {
        Figure *f = Figure::get(figure_id);
        if (figure_is_combat_locked(*f)) {
            return;
        }
        if (prepare_legion_to_move()) {
            f->alternative_location_index = 0;
            f->action_state = FIGURE_ACTION_83_SOLDIER_GOING_TO_STANDARD;
            Route::remove(f);
        }
    });
}

void formation::send_non_combat_figures_to_fort()
{
    for_each_figure_id([&](int figure_id, int) {
        Figure *f = Figure::get(figure_id);
        if (figure_is_combat_locked(*f)) {
            return;
        }
        if (prepare_legion_to_move()) {
            f->action_state = FIGURE_ACTION_81_SOLDIER_GOING_TO_FORT;
            Route::remove(f);
            f->formation_at_rest = 1;
        }
    });
}

formation *formation_create_legion(const Building &fort)
{
    formation *m;
    m = new_formation_after_index(1);
    if (!m) {
        return formation_slot(0);
    }
    m->initialize_legion_from_fort(fort, data.num_legions + 1);

    data.num_legions++;
    if (m->id > (unsigned int) data.id_last_in_use) {
        data.id_last_in_use = m->id;
    }

    //standards and name
    formation_assign_available_legion_regalia(m, m->legion_id);
    return m;
}

static formation *formation_create(figure_type type, int layout, int orientation, int x, int y)
{
    formation *f;
    f = new_formation_after_index(20);
    if (!f) {
        return 0;
    }
    f->faction_id = 0;
    f->x = x;
    f->y = y;
    f->in_use = 1;
    f->is_legion = 0;
    f->figure_type = type;
    f->legion_id = 0; //legions created separately
    f->morale = 100;
    if (layout == FORMATION_ENEMY_DOUBLE_LINE) {
        if (orientation == DIR_0_TOP || orientation == DIR_4_BOTTOM) {
            f->layout = FORMATION_DOUBLE_LINE_1;
        } else {
            f->layout = FORMATION_DOUBLE_LINE_2;
        }
    } else {
        f->layout = layout;
    }
    f->target_formation_id = 0;
    return f;
}

int formation_create_herd(figure_type type, int x, int y, int num_animals)
{
    formation *f = formation_create(type, FORMATION_HERD, 0, x, y);
    if (!f) {
        return 0;
    }
    f->is_herd = 1;
    f->wait_ticks = 24;
    f->max_figures = num_animals;
    return f->id;
}

int formation_create_enemy(figure_type type, int x, int y, int layout, int orientation,
    int enemy_type, int attack_type, int invasion_id, int invasion_sequence)
{
    formation *f = formation_create(type, layout, orientation, x, y);
    if (!f) {
        return 0;
    }
    f->attack_type = attack_type;
    f->orientation = orientation;
    f->enemy_type = enemy_type;
    f->invasion_id = invasion_id;
    f->invasion_sequence = invasion_sequence;
    return f->id;
}

formation *formation_get(int formation_id)
{
    return formation_slot(formation_id);
}

int formation_count(void)
{
    return static_cast<int>(formations.size());
}

unsigned int formation_get_selected(void)
{
    return data.selected_formation;
}

void formation_set_selected(int formation_id)
{
    data.selected_formation = formation_id;
}

void formation_toggle_empire_service(int formation_id)
{
    formation_slot(formation_id)->empire_service ^= 1;
}

void formation_record_missile_fired(formation *m)
{
    m->missile_fired = 6;
}

void formation_record_missile_attack(formation *m, int from_formation_id)
{
    m->missile_attack_timeout = 6;
    m->missile_attack_formation_id = from_formation_id;
}

void formation_record_fight(formation *m)
{
    m->recent_fight = 6;
}

int formation_is_halted(const formation *m)
{
    return m->is_halted;
}

int formation_is_moving(const formation *m)
{
    return m->is_moving;
}

int formation_is_charging(const formation *m)
{
    return m->is_charging;
}

int formation_update_halted_state(formation *m)
{
    const bool has_moving_figure = m->any_figure_id([](int figure_id, int) {
        return Figure::get(figure_id)->direction != DIR_8_NONE;
    });

    m->is_halted = !has_moving_figure;
    return m->is_halted;
}


int formation_update_movement_state(formation *m)
{
    int is_moving_now = (m->standard_x != m->x_home || m->standard_y != m->y_home);
    int current_offset = map_grid_offset(m->x_home, m->y_home);

    if (is_moving_now && !m->is_moving) {
        // Just started moving
        m->is_moving = 1;
        m->started_moving_from_grid_offset = current_offset;
        m->traveled_tiles = 0;
        return 1;
    }

    if (is_moving_now && m->is_moving) {
        // Still moving
        m->traveled_tiles = map_grid_chess_distance(m->started_moving_from_grid_offset, current_offset);
        return 1;
    }

    if (!is_moving_now && m->is_moving) {
        // Just stopped moving
        m->traveled_tiles = map_grid_chess_distance(m->started_moving_from_grid_offset, current_offset);
        m->halted_at_grid_offset = current_offset;
        m->is_moving = 0;
        return 0;
    }

    // Not moving
    return 0;
}

int formation_update_charge_state(formation *m)
{
    int is_charging = 0;

    if (m->is_moving) {
        is_charging = (m->traveled_tiles >= 5);
    } else if (m->is_halted && m->traveled_tiles >= 5) {
        if (m->halted_for_months >= 2) {
            is_charging = (m->recent_fight >= 4);
        } else {
            is_charging = 1;
        }
    }

    m->is_charging = is_charging;
    return is_charging;
}

void formation_update_movement_all_states(formation *m)
{
    formation_update_halted_state(m);
    formation_update_movement_state(m);
    formation_update_charge_state(m);
}


int formation_grid_offset_for_invasion(int invasion_sequence)
{
    for (formation &entry : formations) {
        formation *m = &entry;
        if (m->in_use == 1 && !m->is_legion && !m->is_herd && m->invasion_sequence == invasion_sequence) {
            if (m->x_home > 0 || m->y_home > 0) {
                return map_grid_offset(m->x_home, m->y_home);
            } else {
                return 0;
            }
        }
    }
    return 0;
}

void formation_caesar_pause(void)
{
    for (formation &entry : formations) {
        formation *m = &entry;
        if (m->in_use == 1 && m->has_figure_type(FIGURE_ENEMY_CAESAR_LEGIONARY)) {
            m->wait_ticks = 20;
        }
    }
}

void formation_caesar_retreat(void)
{
    for (formation &entry : formations) {
        formation *m = &entry;
        if (m->in_use == 1 && m->has_figure_type(FIGURE_ENEMY_CAESAR_LEGIONARY)) {
            m->months_low_morale = 1;
        }
    }
}

int formation_has_low_morale(formation *m)
{
    return m->months_low_morale || m->months_very_low_morale;
}

int formation_get_num_legions_cached(void)
{
    return data.num_legions;
}

void formation_calculate_legion_totals(void)
{
    data.id_last_legion = 0;
    data.num_legions = 0;
    city_military_clear_legionary_legions();
    for (unsigned int i = 1; i < formations.size(); i++) {
        formation *m = formation_get(i);
        if (m->in_use) {
            if (m->is_legion) {
                data.id_last_legion = i;
                data.num_legions++;
                if (m->has_figure_type(FIGURE_FORT_LEGIONARY)) {
                    city_military_add_legionary_legion();
                }
            }
            if (m->missile_attack_timeout <= 0) {
                if (Figure *f = m->first_alive_figure()) {
                    formation_set_home(m, f->x, f->y);
                }
            }
        }
    }
    formation_refresh_regalia();
}

int formation_get_num_legions(void)
{
    int total = 0;
    for (formation &entry : formations) {
        formation *m = &entry;
        if (m->in_use && m->is_legion) {
            total++;
        }
    }
    return total;
}

int formation_get_max_legions(void)
{
    // Mars base bonus
    if (game_cheat_extra_legions()) {
        return MAX_LEGIONS + 14;
    } else if (building_monument_working_grand_temple_for_god(GOD_MARS)) {
        return MAX_LEGIONS + 4;
    } else {
        return MAX_LEGIONS;
    }
}

int formation_for_legion(int legion_index)
{
    int index = 1;
    for (formation &entry : formations) {
        formation *m = &entry;
        if (m->in_use && m->is_legion) {
            if (index++ == legion_index) {
                return m->id;
            }
        }
    }
    return 0;
}

void formation_change_morale(formation *m, int amount)
{
    m->morale = calc_bound(
        m->morale + amount,
        0,
        m->base_morale_limit() + m->mess_hall_max_morale_modifier);
}

void formation_update_morale_after_death(formation *m)
{
    formation_calculate_figures();
    int pct_dead = calc_percentage(1, m->total_figure_count() + 1);
    int morale;
    if (pct_dead < 8) {
        morale = -4;
    } else if (pct_dead < 10) {
        morale = -6;
    } else if (pct_dead < 14) {
        morale = -8;
    } else if (pct_dead < 20) {
        morale = -10;
    } else if (pct_dead < 30) {
        morale = -12;
    } else {
        morale = -16;
    }
    formation_change_morale(m, morale);
}

static void change_all_morale(int legion, int enemy)
{
    for (unsigned int i = 1; i < formations.size(); i++) {
        formation *m = formation_get(i);
        if (m->in_use && !m->is_herd) {
            if (m->is_legion) {
                formation_change_morale(m, legion);
            } else {
                formation_change_morale(m, enemy);
            }
        }
    }
}

void formation_update_monthly_morale_deployed(void)
{
    for (formation &entry : formations) {
        formation *f = &entry;
        if (f->in_use != 1 || f->is_herd) {
            continue;
        }
        if (f->is_legion) {
            if (!f->is_at_fort && !f->in_distant_battle) {
                if (f->morale <= 20 && !f->months_low_morale && !f->months_very_low_morale) {
                    change_all_morale(-5, 10);
                }
                if (f->morale <= 10) {
                    f->months_very_low_morale++;
                } else if (f->morale <= 20) {
                    f->months_low_morale++;
                }
            }
        } else { // enemy
            if (f->morale <= 20 && !f->months_low_morale && !f->months_very_low_morale) {
                change_all_morale(5, -10);
            }
            if (f->morale <= 10) {
                f->months_very_low_morale++;
            } else if (f->morale <= 20) {
                f->months_low_morale++;
            }
        }
    }
}

static void update_morale_for_mess_hall(void)
{
    for (unsigned int i = 1; i < formations.size(); i++) {
        formation *f = formation_get(i);
        int max_morale = 0;
        if (f->in_use != 1 || !f->is_legion) {
            continue;
        }

        // food types bonus
        max_morale += calc_bound(((city_data.mess_hall.food_types - 1) * 5), 0, 10);

        // well-fed bonus
        if (city_data.mess_hall.food_stress_cumulative < 3) {
            max_morale += 5;
        }

        // hungry soldiers morale penalty
        if (city_data.mess_hall.food_stress_cumulative > 20) {
            max_morale -= city_data.mess_hall.food_stress_cumulative / 3;
        }

        // apply
        f->mess_hall_max_morale_modifier = calc_bound(max_morale, -30, 15);
    }
}

void formation_update_monthly_morale_at_rest(void)
{
    for (unsigned int i = 1; i < formations.size(); i++) {
        formation *m = formation_get(i);
        if (m->in_use != 1 || m->is_herd) {
            continue;
        }
        if (m->is_legion) {
            if (m->is_at_fort) {
                m->months_from_home = 0;
                m->months_very_low_morale = 0;
                m->months_low_morale = 0;
                formation_change_morale(m, 5);
                formation_legion_restore_layout(m);
            } else if (!m->recent_fight) {
                m->months_from_home++;
                if (m->months_from_home > 3) {
                    if (m->months_from_home > 100) {
                        m->months_from_home = 100;
                    }
                    formation_change_morale(m, -5);
                }
            }
            update_morale_for_mess_hall();
        } else {
            formation_change_morale(m, 0);
        }
    }
}

void formation_change_all_legions_morale(int amount)
{
    for (unsigned int i = 1; i < formations.size(); i++) {
        formation *m = formation_get(i);
        if (!m->is_legion) {
            continue;
        }

        formation_change_morale(m, amount);
    }
}

void formation_decrease_monthly_counters(formation *m)
{
    if (m->is_legion) {
        if (m->cursed_by_mars) {
            m->cursed_by_mars--;
        }
    }
    if (m->missile_fired) {
        m->missile_fired--;
    }
    if (m->missile_attack_timeout) {
        m->missile_attack_timeout--;
    }
    if (m->recent_fight) {
        m->recent_fight--;
    }
    // this one increases
    if (m->is_halted) {
        m->halted_for_months++;
    }
}

void formation_clear_monthly_counters(formation *m)
{
    m->missile_fired = 0;
    m->missile_attack_timeout = 0;
    m->recent_fight = 0;
}

void formation_set_destination(formation *m, int x, int y)
{
    m->destination_x = x;
    m->destination_y = y;
}

void formation_set_destination_building(formation *m, int x, int y, const Building *building)
{
    m->destination_x = x;
    m->destination_y = y;
    m->destination_building_id = building ? building->id : 0;
}

void formation_set_home(formation *m, int x, int y)
{
    m->x_home = x;
    m->y_home = y;
}

void formation_retreat(formation *m)
{
    m->months_low_morale = 1;
}

static void clear_figures(void)
{
    for (unsigned int i = 1; i < formations.size(); i++) {
        formation *f = formation_get(i);
        f->clear_roster();
        f->is_at_fort = 1;
    }
}

int formation_legion_count_alive_soldiers(int formation_id)
{
    formation *m = formation_get(formation_id);
    return m->count_alive_figures();
}

int formation_legion_count_alive_soldiers_by_type(figure_type type)
{
    int totals = 0;
    for (formation &entry : formations) {
        formation *m = &entry;
        if (m->in_use && m->counts_for_legion_type(type)) {
            // fort_standard used to count all types
            totals += formation_legion_count_alive_soldiers(m->id);
        }
    }
    return totals;
}


void formation_move_herds_away(int x, int y)
{
    for (unsigned int i = 1; i < formations.size(); i++) {
        formation *f = formation_get(i);
        if (f->in_use != 1 || f->is_legion || !f->is_herd || !f->has_figures()) {
            continue;
        }
        if (calc_maximum_distance(x, y, f->x_home, f->y_home) <= 6) {
            f->wait_ticks = 50;
            f->herd_direction = calc_general_direction(x, y, f->x_home, f->y_home);
        }
    }
}

void formation_calculate_figures(void)
{
    clear_figures();
    for (unsigned int i = 1; i < Figure::count(); i++) {
        Figure *f = Figure::get(i);
        if (f->state != FIGURE_STATE_ALIVE) {
            continue;
        }
        if (!f->is_legion() && !f->is_enemy() && !f->is_herd()) {
            continue;
        }
        if (f->type == FIGURE_ENEMY54_GLADIATOR) {
            continue;
        }
        formation *m = formation_get(f->formation_id);
        int index = m->add_figure_to_roster(i,
            f->formation_at_rest != 1, f->damage,
            figure_properties_for_type(static_cast<figure_type>(f->type))->max_damage
        );
        f->index_in_formation = index;
    }

    enemy_army_totals_clear();
    for (unsigned int i = 1; i < formations.size(); i++) {
        formation *m = formation_get(i);
        if (m->in_use && !m->is_herd) {
            if (m->is_legion) {
                if (m->has_figures()) {
                    int was_halted = m->is_halted;
                    int was_charging = m->is_charging;
                    formation_update_movement_all_states(m);
                    if (!was_charging && m->is_charging && !m->is_at_fort) {
                        if (m->has_figure_type(FIGURE_FORT_MOUNTED)) {
                            sound_effect_play(SOUND_EFFECT_HORSE_MOVING);//CHAAARGE!
                        } else if (m->has_figure_type(FIGURE_FORT_INFANTRY)) {
                            sound_speech_play_file("wavs/horn3.wav"); //unused horn sound
                        }
                    }
                    if (!was_halted && m->is_halted) { // formation stopped
                        m->halted_at_grid_offset = map_grid_offset(m->x_home, m->y_home);
                        m->started_moving_from_grid_offset = 0;
                        if (m->has_figure_type(FIGURE_FORT_LEGIONARY)) {
                            sound_effect_play(SOUND_EFFECT_FORMATION_SHIELD);
                        }
                    }
                }
            } else {
                // enemy
                if (!m->has_figures()) {
                    formation_clear(m->id);
                } else {
                    enemy_army_totals_add_enemy_formation(m->total_figure_count());
                }
            }
        }
    }

    city_military_update_totals();
}

static void update_direction(int formation_id, int first_figure_direction)
{
    formation *f = formation_get(formation_id);
    if (f->unknown_fired) {
        f->unknown_fired--;
    } else if (f->missile_fired) {
        f->direction = first_figure_direction;
    } else if (f->layout == FORMATION_DOUBLE_LINE_1 || f->layout == FORMATION_SINGLE_LINE_1) {
        if (f->y_home < f->prev.y_home) {
            f->direction = DIR_0_TOP;
        } else if (f->y_home > f->prev.y_home) {
            f->direction = DIR_4_BOTTOM;
        }
    } else if (f->layout == FORMATION_DOUBLE_LINE_2 || f->layout == FORMATION_SINGLE_LINE_2) {
        if (f->x_home < f->prev.x_home) {
            f->direction = DIR_6_LEFT;
        } else if (f->x_home > f->prev.x_home) {
            f->direction = DIR_2_RIGHT;
        }
    } else if (f->layout == FORMATION_TORTOISE || f->layout == FORMATION_COLUMN) {
        int dx = (f->x_home < f->prev.x_home) ? (f->prev.x_home - f->x_home) : (f->x_home - f->prev.x_home);
        int dy = (f->y_home < f->prev.y_home) ? (f->prev.y_home - f->y_home) : (f->y_home - f->prev.y_home);
        if (dx > dy) {
            if (f->x_home < f->prev.x_home) {
                f->direction = DIR_6_LEFT;
            } else if (f->x_home > f->prev.x_home) {
                f->direction = DIR_2_RIGHT;
            }
        } else {
            if (f->y_home < f->prev.y_home) {
                f->direction = DIR_0_TOP;
            } else if (f->y_home > f->prev.y_home) {
                f->direction = DIR_4_BOTTOM;
            }
        }
    }
    f->prev.x_home = f->x_home;
    f->prev.y_home = f->y_home;
}

static void update_directions(void)
{
    for (formation &entry : formations) {
        formation *m = &entry;
        if (m->in_use && !m->is_herd) {
            if (Figure *f = m->first_figure()) {
                update_direction(m->id, f->direction);
            }
        }
    }
}

static void set_legion_max_figures(void)
{
    for (formation &entry : formations) {
        formation *m = &entry;
        if (m->in_use && m->is_legion) {
            m->refresh_legion_definition_from_home();
            m->max_figures = m->declared_capacity();
        }
    }
}

void formation_refresh_runtime_definitions(void)
{
    for (formation &entry : formations) {
        entry.refresh_legion_definition_from_home();
    }
}

void formation_update_all(int second_time)
{
    formation_calculate_legion_totals();
    formation_calculate_figures();
    update_directions();
    formation_legion_decrease_damage();
    if (!second_time) {
        formation_update_monthly_morale_deployed();
    }
    set_legion_max_figures();
    formation_legion_update();
    formation_enemy_update();
    formation_herd_update();
}

void formations_save_state(buffer *buf, buffer *totals)
{
    const size_t buf_size = sizeof(int32_t) + formations.size() * CURRENT_BUFFER_SIZE_PER_FORMATION;
    uint8_t *buf_data = static_cast<uint8_t *>(malloc(buf_size));
    memset(buf_data, 0, buf_size);
    buffer_init(buf, buf_data, buf_size);
    buffer_write_i32(buf, CURRENT_BUFFER_SIZE_PER_FORMATION);

    for (unsigned int i = 0; i < formations.size(); i++) {
        formation *f = formation_get(i);
        buffer_write_u8(buf, f->in_use);
        buffer_write_u8(buf, f->faction_id);
        buffer_write_u8(buf, f->legion_id);
        buffer_write_u8(buf, f->is_at_fort);
        buffer_write_i16(buf, f->figure_type);
        buffer_write_i16(buf, f->building_id);
        f->write_legacy_figure_slots(buf);
        buffer_write_u8(buf, f->num_figures);
        buffer_write_u8(buf, f->max_figures);
        buffer_write_i16(buf, f->layout);
        buffer_write_i16(buf, f->morale);
        buffer_write_u8(buf, f->x_home);
        buffer_write_u8(buf, f->y_home);
        buffer_write_u8(buf, f->standard_x);
        buffer_write_u8(buf, f->standard_y);
        buffer_write_u8(buf, f->x);
        buffer_write_u8(buf, f->y);
        buffer_write_u8(buf, f->destination_x);
        buffer_write_u8(buf, f->destination_y);
        buffer_write_i16(buf, f->destination_building_id);
        buffer_write_i16(buf, f->standard_figure_id);
        buffer_write_u8(buf, f->is_legion);
        buffer_write_u8(buf, f->mess_hall_max_morale_modifier);
        buffer_write_i32(buf, f->legion_flag_id);
        buffer_write_i32(buf, f->legion_name_id);
        buffer_write_i32(buf, f->legion_name_group);
        buffer_write_i16(buf, f->attack_type);
        buffer_write_i16(buf, f->legion_recruit_type);
        buffer_write_i16(buf, f->has_military_training);
        buffer_write_i16(buf, f->total_damage);
        buffer_write_i16(buf, f->max_total_damage);
        buffer_write_i16(buf, f->wait_ticks);
        buffer_write_i16(buf, f->recent_fight);
        buffer_write_i16(buf, f->enemy_state.duration_advance);
        buffer_write_i16(buf, f->enemy_state.duration_regroup);
        buffer_write_i16(buf, f->enemy_state.duration_halt);
        buffer_write_i16(buf, f->enemy_legion_index);
        buffer_write_i16(buf, f->is_halted);
        buffer_write_i16(buf, f->missile_fired);
        buffer_write_i16(buf, f->missile_attack_timeout);
        buffer_write_i16(buf, f->missile_attack_formation_id);
        buffer_write_i16(buf, f->prev.layout);
        buffer_write_i16(buf, f->cursed_by_mars);
        buffer_write_u8(buf, f->months_low_morale);
        buffer_write_u8(buf, f->empire_service);
        buffer_write_u8(buf, f->in_distant_battle);
        buffer_write_u8(buf, f->is_herd);
        buffer_write_u8(buf, f->enemy_type);
        buffer_write_u8(buf, f->direction);
        buffer_write_u8(buf, f->prev.x_home);
        buffer_write_u8(buf, f->prev.y_home);
        buffer_write_u8(buf, f->unknown_fired);
        buffer_write_u8(buf, f->orientation);
        buffer_write_u8(buf, f->months_from_home);
        buffer_write_u8(buf, f->months_very_low_morale);
        buffer_write_u8(buf, f->invasion_id);
        buffer_write_u8(buf, f->herd_wolf_spawn_delay);
        buffer_write_u8(buf, f->herd_direction);
        buffer_write_i32(buf, f->target_formation_id);
        buffer_skip(buf, 13);
        buffer_write_i16(buf, f->invasion_sequence);
        f->write_extended_figure_slots(buf);
    }
    buffer_write_i32(totals, data.id_last_in_use);
    buffer_write_i32(totals, data.id_last_legion);
    buffer_write_i32(totals, data.num_legions);
}

void formations_load_state(buffer *buf, buffer *totals, int version)
{
    data.id_last_in_use = buffer_read_i32(totals);
    data.id_last_legion = buffer_read_i32(totals);
    data.num_legions = buffer_read_i32(totals);
    data.selected_formation = 0;

    int formation_buf_size = ORIGINAL_BUFFER_SIZE_PER_FORMATION;
    size_t buf_size = buf->size;
    if (version <= SAVE_GAME_LAST_10_LEGIONS_MAX) {
        formation_buf_size = BUFFER_SIZE_FOR_10_LEGIONS;
    }
    if (version > SAVE_GAME_LAST_STATIC_VERSION) {
        formation_buf_size = buffer_read_i32(buf);
        buf_size -= 4;
    }


    int formations_to_load = (int) buf_size / formation_buf_size;

    resize_formations_for_load(formations_to_load);

    // Reduce number of used formations. Improves performance
    int highest_id_in_use = 0;

    for (int i = 0; i < formations_to_load; i++) {
        formation *f = formation_slot(i);
        f->in_use = buffer_read_u8(buf);
        f->faction_id = buffer_read_u8(buf);
        f->legion_id = buffer_read_u8(buf);
        f->is_at_fort = buffer_read_u8(buf);
        f->figure_type = buffer_read_i16(buf);
        f->building_id = buffer_read_i16(buf);
        f->read_legacy_figure_slots(buf);
        f->num_figures = buffer_read_u8(buf);
        f->max_figures = buffer_read_u8(buf);
        f->layout = buffer_read_i16(buf);
        f->morale = buffer_read_i16(buf);
        f->x_home = buffer_read_u8(buf);
        f->y_home = buffer_read_u8(buf);
        f->standard_x = buffer_read_u8(buf);
        f->standard_y = buffer_read_u8(buf);
        f->x = buffer_read_u8(buf);
        f->y = buffer_read_u8(buf);
        f->destination_x = buffer_read_u8(buf);
        f->destination_y = buffer_read_u8(buf);
        f->destination_building_id = buffer_read_i16(buf);
        f->standard_figure_id = buffer_read_i16(buf);
        f->is_legion = buffer_read_u8(buf);
        f->mess_hall_max_morale_modifier = buffer_read_u8(buf);
        if (version <= SAVE_GAME_LAST_10_LEGIONS_MAX) {
            if (f->is_legion) {
                f->legion_id = f->legion_id + f->is_legion; // 1-based index for legions
            }
            // after increasing number of legions, switched to 1-based index
            f->legion_flag_id = widget_sidebar_military_get_standard_image(f->legion_id);
            f->legion_name_id = widget_sidebar_military_get_legion_name_id(f->legion_id);
            f->legion_name_group = widget_sidebar_military_get_legion_name_group(f->legion_id);
        } else {
            f->legion_flag_id = buffer_read_i32(buf);
            f->legion_name_id = buffer_read_i32(buf);
            f->legion_name_group = buffer_read_i32(buf);
        }
        f->attack_type = buffer_read_i16(buf);
        f->legion_recruit_type = buffer_read_i16(buf);
        f->has_military_training = buffer_read_i16(buf);
        f->total_damage = buffer_read_i16(buf);
        f->max_total_damage = buffer_read_i16(buf);
        f->wait_ticks = buffer_read_i16(buf);
        f->recent_fight = buffer_read_i16(buf);
        f->enemy_state.duration_advance = buffer_read_i16(buf);
        f->enemy_state.duration_regroup = buffer_read_i16(buf);
        f->enemy_state.duration_halt = buffer_read_i16(buf);
        f->enemy_legion_index = buffer_read_i16(buf);
        f->is_halted = buffer_read_i16(buf);
        f->missile_fired = buffer_read_i16(buf);
        f->missile_attack_timeout = buffer_read_i16(buf);
        f->missile_attack_formation_id = buffer_read_i16(buf);
        f->prev.layout = buffer_read_i16(buf);
        f->cursed_by_mars = buffer_read_i16(buf);
        f->months_low_morale = buffer_read_u8(buf);
        f->empire_service = buffer_read_u8(buf);
        f->in_distant_battle = buffer_read_u8(buf);
        f->is_herd = buffer_read_u8(buf);
        f->enemy_type = buffer_read_u8(buf);
        f->direction = buffer_read_u8(buf);
        f->prev.x_home = buffer_read_u8(buf);
        f->prev.y_home = buffer_read_u8(buf);
        f->unknown_fired = buffer_read_u8(buf);
        f->orientation = buffer_read_u8(buf);
        f->months_from_home = buffer_read_u8(buf);
        f->months_very_low_morale = buffer_read_u8(buf);
        f->invasion_id = buffer_read_u8(buf);
        f->herd_wolf_spawn_delay = buffer_read_u8(buf);
        f->herd_direction = buffer_read_u8(buf);
        f->target_formation_id = buffer_read_i32(buf);
        buffer_skip(buf, 13);
        f->invasion_sequence = buffer_read_i16(buf);
        f->read_extended_figure_slots(buf, formation_buf_size);

        if (f->in_use) {
            highest_id_in_use = i;
        }
    }

    // Reduce number of available formations to improve performance
    formations.resize(highest_id_in_use + 1);

    // old saves did not write formations to a zeroed out buffer, so check for invalid target_formation_ids
    for (unsigned int i = 0; i < formations.size(); i++) {
        formation *f = formation_slot(i);
        if (f->target_formation_id >= formations.size()) {
            f->target_formation_id = 0;
        }
    }
}


