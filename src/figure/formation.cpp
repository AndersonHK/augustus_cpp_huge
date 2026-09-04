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
#include "figure/movement.h"
#include "figure/PathingMode.h"
#include "figure/properties.h"
#include "figure/route.h"
#include "game/save_version.h"
#include "game/cheats.h"
#include "map/grid.h"
#include "map/figure.h"
#include "sound/effect.h"
#include "sound/speech.h"
#include "widget/sidebar/military.h"

#include <stdio.h>
#include <cstring>
#include <deque>
#include <exception>
#include <memory>
#include <vector>


constexpr int ORIGINAL_BUFFER_SIZE_PER_FORMATION = 128;
constexpr int BUFFER_SIZE_FOR_10_LEGIONS = 128;
constexpr int LEGACY_BUFFER_SIZE_PER_FORMATION = 256;
constexpr int LEGACY_FORMATION_SAVE_SLOT_COUNT = 16;
constexpr int EXTENDED_FORMATION_ROSTER_SAVE_SLOTS = 256;
constexpr int CURRENT_BUFFER_SIZE_PER_FORMATION =
    LEGACY_BUFFER_SIZE_PER_FORMATION + sizeof(int32_t) +
    EXTENDED_FORMATION_ROSTER_SAVE_SLOTS * sizeof(int32_t);
constexpr int MAX_LEGIONS_REGALIA = 21; // +1 to keep indexing simple with 1 start
constexpr int HERD_ANIMAL_MOVE_WAIT_TICKS = 401;

// Forts retain direct pointers to their live formations. A deque keeps those
// addresses stable as enemy and herd formations are appended.
static std::deque<formation> formations;
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

static void clear_formation_destinations(void)
{
    for (formation &entry : formations) entry.standard.clear();
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
        formations.back().standard.clear();
        formations.pop_back();
    }
}

static void resize_formations_for_load(unsigned int size)
{
    clear_formation_destinations();
    formations.resize(size);
    for (unsigned int i = 0; i < size; i++) {
        reset_formation_slot(i);
    }
}

void formations_clear(void)
{
    clear_formation_destinations();
    formations.clear();
    if (!append_formation()) { // Ignore first formation
        log_error("Unable to create the formations array. The game will likely crash.", 0, 0);
    }
    data.id_last_in_use = 0;
    data.id_last_legion = 0;
    data.num_legions = 0;
    data.selected_formation = 0;
}

void formation::remove()
{
    standard.clear();
    if (!in_use) {
        return;
    }

    disconnect_relationships(RelationshipDisconnectReason::EndpointRemoved);
    for (unsigned int figure_id = 1; figure_id < Figure::count(); figure_id++) {
        Figure *figure = Figure::get(figure_id);
        if (figure && figure->formation_id == id) {
            figure->formation_id = 0;
        }
    }
    for (formation &dependent : formations) {
        if (dependent.missile_attack_formation_id == static_cast<int>(id)) {
            dependent.missile_attack_formation_id = 0;
            dependent.missile_attack_timeout = 0;
        }
    }
    if (invasion_id >= 0 && invasion_id < MAX_ENEMY_ARMIES) {
        enemy_army *army = enemy_army_get_editable(invasion_id);
        if (army->formation_id == static_cast<int>(id)) {
            army->formation_id = 0;
        }
    }
    if (data.selected_formation == id) {
        data.selected_formation = 0;
    }
    unbind_from_fort();
    formation_type_definition = nullptr;
    layout_definition = nullptr;
    figures.clear();
    member_movement_plan = {};
    num_figures = 0;
    in_use = 0;
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

bool formation::bind_to_fort(Building &fort, const char **failure_reason)
{
    const FormationType *declared_type =
        fort.type && fort.type->has_military() ? fort.type->military().formation_type() : nullptr;
    const FormationFortLinkFailure failure = validate_formation_fort_link({
        id,
        fort.id,
        fort.is_in_use() || fort.is_mothballed() || fort.state_id() == BUILDING_STATE_CREATED,
        in_use,
        is_legion,
        declared_type != nullptr,
        fort.formation_id(),
        building_id
    });
    if (failure != FormationFortLinkFailure::None) {
        if (failure_reason) {
            *failure_reason = formation_fort_link_failure_text(failure);
        }
        return false;
    }

    if (fort.Formation && fort.Formation != this) {
        if (failure_reason) {
            *failure_reason = "fort is already bound to another live formation";
        }
        return false;
    }

    bind_definition(*declared_type);
    const ::figure_type fort_figure_type = static_cast<::figure_type>(fort.fort_figure_type());
    if (fort_figure_type != FIGURE_NONE) {
        figure_type = fort_figure_type;
    }
    building_id = static_cast<int>(fort.id);
    fort.set_formation_id(static_cast<int>(id));
    fort.Formation = this;
    if (failure_reason) {
        *failure_reason = nullptr;
    }
    return true;
}

Building *formation::fort() const
{
    if (!is_legion || building_id <= 0) {
        return nullptr;
    }
    Building *home = Building::get(static_cast<unsigned int>(building_id));
    return home && (home->is_in_use() || home->is_mothballed() ||
        home->state_id() == BUILDING_STATE_CREATED) ? home : nullptr;
}

void formation::unbind_from_fort()
{
    Building *home = building_id > 0 ? Building::get(static_cast<unsigned int>(building_id)) : nullptr;
    if (home && home->Formation == this) {
        home->Formation = nullptr;
    }
}

bool formation::refresh_legion_definition_from_home()
{
    if (!is_legion) {
        return true;
    }
    Building *home = fort();
    int matching_forts = 0;
    if (!home && building_id <= 0) {
        // Some legacy records can retain only the fort-side formation id.
        // Recover only an unambiguous military owner; composition children do
        // not declare MilitaryDefinition and therefore cannot be adopted.
        Building::for_each([&](Building *candidate) {
            if (candidate->type && candidate->type->has_military() &&
                candidate->formation_id() == static_cast<int>(id)) {
                home = candidate;
                matching_forts++;
            }
        });
        if (matching_forts != 1) {
            home = nullptr;
        }
    }
    if (!home) {
        log_error(
            matching_forts > 1 ?
                "Legion formation matches more than one fort building" :
                "Legion formation references missing fort building",
            0,
            static_cast<int>(id));
        return false;
    }
    const char *failure = nullptr;
    if (!bind_to_fort(*home, &failure)) {
        log_error("Unable to bind legion formation to fort", failure, static_cast<int>(id));
        return false;
    }
    return true;
}

bool formation::reconcile_loaded_legion_command()
{
    Building *home = fort();
    if (!home || !home->Composition) {
        log_error("Loaded fort formation has no data-defined mustering ground", 0, static_cast<int>(id));
        return false;
    }
    Building &ground = home->Composition->require_child_for_role(
        "mustering_ground", "formation::reconcile_loaded_legion_command");
    const int saved_standard_x = standard_x;
    const int saved_standard_y = standard_y;
    const int saved_is_at_fort = is_at_fort;
    const int saved_direction = direction;
    int repaired_actions = 0;
    int live_members = 0;
    int home_command_members = 0;
    int deployed_command_members = 0;
    int stationed_home_members = 0;
    int facing_counts[DIR_8_NONE] = {};

    for_each_alive_figure([&](Figure &member, int) {
        if (member.action_state == FIGURE_ACTION_149_CORPSE) return;
        live_members++;
        if (member.action_state == FIGURE_ACTION_80_SOLDIER_AT_REST ||
            member.action_state == FIGURE_ACTION_81_SOLDIER_GOING_TO_FORT) {
            home_command_members++;
        } else if (member.action_state == FIGURE_ACTION_83_SOLDIER_GOING_TO_STANDARD ||
            member.action_state == FIGURE_ACTION_84_SOLDIER_AT_STANDARD) {
            deployed_command_members++;
        }
        if (member.action_state == FIGURE_ACTION_80_SOLDIER_AT_REST && member.formation_at_rest == 1) {
            stationed_home_members++;
        }
        int member_facing = member.previous_tile_direction;
        if (member_facing < DIR_0_TOP || member_facing >= DIR_8_NONE) member_facing = member.direction;
        if (member_facing >= DIR_0_TOP && member_facing < DIR_8_NONE) facing_counts[member_facing]++;
    });

    bool command_is_home = standard_x == ground.x() && standard_y == ground.y();
    if (live_members && home_command_members == live_members) {
        command_is_home = true;
    } else if (live_members && deployed_command_members == live_members) {
        command_is_home = false;
    }
    if (command_is_home) set_station_origin(ground.x(), ground.y());

    for_each_alive_figure([&](Figure &member, int) {
        if (member.action_state == FIGURE_ACTION_149_CORPSE) return;
        if (command_is_home) {
            if (member.action_state == FIGURE_ACTION_83_SOLDIER_GOING_TO_STANDARD ||
                member.action_state == FIGURE_ACTION_84_SOLDIER_AT_STANDARD) {
                member.action_state = FIGURE_ACTION_81_SOLDIER_GOING_TO_FORT;
                member.formation_at_rest = 0;
                Route::remove(&member);
                repaired_actions++;
            }
        } else {
            if (member.action_state == FIGURE_ACTION_80_SOLDIER_AT_REST ||
                member.action_state == FIGURE_ACTION_81_SOLDIER_GOING_TO_FORT) {
                member.action_state = FIGURE_ACTION_83_SOLDIER_GOING_TO_STANDARD;
                Route::remove(&member);
                repaired_actions++;
            }
            member.formation_at_rest = 0;
        }
    });

    is_at_fort = command_is_home && stationed_home_members == live_members;
    int recovered_facing = direction;
    if (!command_is_home && (recovered_facing < DIR_0_TOP || recovered_facing >= DIR_8_NONE)) {
        int best_count = 0;
        for (int candidate = DIR_0_TOP; candidate < DIR_8_NONE; candidate++) {
            if (facing_counts[candidate] > best_count) {
                recovered_facing = candidate;
                best_count = facing_counts[candidate];
            }
        }
        if (best_count == 0 && layout_definition &&
            layout_definition->stationary_facing >= DIR_0_TOP &&
            layout_definition->stationary_facing < DIR_8_NONE) {
            recovered_facing = layout_definition->stationary_facing;
        }
        if (recovered_facing < DIR_0_TOP || recovered_facing >= DIR_8_NONE) {
            const FormationLayoutDef *at_rest = formation_layout_registry_impl::find_layout("at_rest");
            if (!at_rest || at_rest->stationary_facing < DIR_0_TOP || at_rest->stationary_facing >= DIR_8_NONE) {
                log_error("Loaded deployed legion has no recoverable stationary facing", 0, static_cast<int>(id));
                return false;
            }
            recovered_facing = at_rest->stationary_facing;
        }
        direction = recovered_facing;
    }

    if (saved_standard_x != standard_x || saved_standard_y != standard_y ||
        saved_is_at_fort != is_at_fort || saved_direction != direction || repaired_actions) {
        char detail[256];
        snprintf(detail, sizeof(detail),
            "formation_id=%u saved_anchor=(%d,%d) canonical_anchor=(%d,%d) home=(%d,%d) saved_at_fort=%d canonical_at_fort=%d saved_facing=%d canonical_facing=%d repaired_actions=%d",
            id, saved_standard_x, saved_standard_y, standard_x, standard_y, ground.x(), ground.y(), saved_is_at_fort, is_at_fort,
            saved_direction, direction, repaired_actions);
        log_warning("Repairing contradictory loaded legion command state", detail, 0);
    }
    set_station_origin(standard_x, standard_y);
    return true;
}

bool formation::initialize_legion_from_fort(Building &fort, int assigned_legion_id)
{
    faction_id = 1;
    in_use = 1;
    is_legion = 1;
    figure_type = static_cast<::figure_type>(fort.fort_figure_type());
    const char *failure = nullptr;
    if (!bind_to_fort(fort, &failure)) {
        log_error("Unable to create legion formation for fort", failure, static_cast<int>(fort.id));
        return false;
    }
    if (!set_layout("double_line_1")) {
        log_error("Unable to create legion formation without FormationLayout", "double_line_1", 0);
        return false;
    }
    morale = 50;
    is_at_fort = 1;
    legion_id = assigned_legion_id;
    if (legion_id >= 20) {
        legion_id = 20;
    }

    if (!fort.Composition) {
        log_error("Fort formation has no data-defined mustering ground", fort.type ? fort.type->attr() : "unknown", static_cast<int>(fort.id));
        std::terminate();
    }
    Building &fort_ground = fort.Composition->require_child_for_role(
        "mustering_ground", "formation::initialize_legion_from_fort");
    x = x_home = fort_ground.x();
    y = y_home = fort_ground.y();
    set_station_origin(fort_ground.x(), fort_ground.y());
    target_formation.clear();
    return true;
}

bool formation::owns_figure(const Figure &figure) const
{
    if (!in_use) {
        return false;
    }
    require_definition("owns_figure");
    if (figure.formation_id != id) {
        return false;
    }
    const ::figure_type type = static_cast<::figure_type>(figure.type);
    if (is_legion) {
        return has_figure_type(type);
    }
    return formation_type_definition->spawn.role != FormationSpawnRole::None && has_figure_type(type);
}

bool formation::is_roster_member(const Figure &figure) const
{
    return owns_figure(figure);
}

int formation::modified_combat_value(FormationCombatStat stat, int base_value) const
{
    require_definition("modified_combat_value");
    return formation_type_definition->modified_combat_value(stat, base_value);
}

bool formation::recruit_requires_weapon() const
{
    require_definition("recruit_requires_weapon");
    return formation_type_definition->primary_unit()->requires_weapon();
}

int formation::base_morale_limit() const
{
    const UnitType *unit = unit_type_registry_impl::find_unit_type(figure_type_id());
    if (unit && unit->has_morale()) {
        require_definition("base_morale_limit");
        return formation_type_definition->modified_combat_value(FormationCombatStat::Morale, unit->combat_stats().morale) +
            (has_military_training ? 10 : 0);
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
    require_definition("declared_recruit_type");
    return formation_type_definition->primary_unit()->recruit_type();
}

int formation::distant_battle_strength() const
{
    require_definition("distant_battle_strength");
    return formation_type_definition->distant_battle_strength(has_military_training != 0);
}

int formation::curse_weight() const
{
    require_definition("curse_weight");
    return formation_type_definition->curse_weight(num_figures);
}

int formation::barracks_recruit_capacity() const
{
    require_definition("barracks_recruit_capacity");
    return is_legion ? formation_type_definition->recruit_capacity() : formation_type_definition->capacity();
}

int formation::barracks_recruit_overflow_count() const
{
    const int overflow = num_figures - barracks_recruit_capacity();
    return overflow > 0 ? overflow : 0;
}

void formation::ensure_roster_capacity(int capacity)
{
    if (capacity <= 0) {
        return;
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

void formation::prepare_roster_refresh()
{
    ensure_roster_capacity(declared_capacity());
    num_figures = 0;
    total_damage = 0;
    max_total_damage = 0;
    is_at_fort = !is_legion || (!in_distant_battle && standard_x == x && standard_y == y);

    for (int slot = 0; slot < static_cast<int>(figures.size()); slot++) {
        const int figure_id = figures[slot];
        if (!figure_id || static_cast<unsigned int>(figure_id) >= Figure::count()) {
            member_movement_plan.release_slot(slot);
            figures[slot] = 0;
            continue;
        }
        Figure *figure = Figure::get(figure_id);
        if (!figure || figure->state != FIGURE_STATE_ALIVE || !is_roster_member(*figure)) {
            member_movement_plan.release_slot(slot);
            figures[slot] = 0;
            continue;
        }
        figure->index_in_formation = static_cast<unsigned char>(slot);
    }
}

int formation::publish_figure(Figure &figure)
{
    require_definition("publish_figure");
    if (!is_roster_member(figure)) {
        log_error("Formation cannot publish a figure it does not own", "formation", static_cast<int>(id));
        std::terminate();
    }

    int slot = -1;
    for (int candidate = 0; candidate < static_cast<int>(figures.size()); candidate++) {
        if (figures[candidate] == static_cast<int>(figure.id())) {
            if (slot >= 0) {
                log_error("Formation roster contains a duplicate figure", "formation", static_cast<int>(id));
                std::terminate();
            }
            slot = candidate;
        }
    }
    if (slot < 0) {
        for (int candidate = 0; candidate < static_cast<int>(figures.size()); candidate++) {
            if (!figures[candidate]) {
                figures[candidate] = static_cast<int>(figure.id());
                slot = candidate;
                break;
            }
        }
    }
    if (slot < 0 || slot >= declared_capacity()) {
        log_error("Formation has no free declared roster slot", "formation", static_cast<int>(id));
        std::terminate();
    }

    figure.index_in_formation = static_cast<unsigned char>(slot);
    return slot;
}

void formation::refresh_published_figure(Figure &figure)
{
    publish_figure(figure);
    num_figures++;
    total_damage += figure.damage;
    max_total_damage += figure_damage_limit_for_type(static_cast<::figure_type>(figure.type));
    if (figure.formation_at_rest != 1) {
        is_at_fort = 0;
    }
}

void formation::unpublish_figure(Figure &figure)
{
    for (int slot = 0; slot < static_cast<int>(figures.size()); slot++) {
        if (figures[slot] == static_cast<int>(figure.id())) {
            member_movement_plan.release_member(slot, figure);
            figures[slot] = 0;
            break;
        }
    }
    figure.formation_id = 0;
    figure.index_in_formation = 0;
}

void formation::write_legacy_figure_slots(buffer *buf) const
{
    for (int slot = 0; slot < LEGACY_FORMATION_SAVE_SLOT_COUNT; slot++) {
        buffer_write_i16(buf, static_cast<int16_t>(roster_figure_id(slot)));
    }
}

void formation::read_legacy_figure_slots(buffer *buf)
{
    ensure_roster_capacity(LEGACY_FORMATION_SAVE_SLOT_COUNT);
    std::fill(figures.begin(), figures.end(), 0);
    for (int slot = 0; slot < LEGACY_FORMATION_SAVE_SLOT_COUNT; slot++) {
        figures[slot] = buffer_read_i16(buf);
    }
    invalidate_member_movement_plan();
}

void formation::write_extended_figure_slots(buffer *buf) const
{
    const int slot_count = static_cast<int>(figures.size());
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
    invalidate_member_movement_plan();
}

std::vector<int> formation::layout_grid_offsets() const
{
    if (num_figures <= 0) {
        return {};
    }
    require_definition("layout_grid_offsets");
    if (!layout_definition) {
        log_error("Live formation has no FormationLayout", formation_type_definition->key(), static_cast<int>(id));
        std::terminate();
    }
    std::vector<FormationLayoutPosition> positions;
    const bool positioned = formation_type_definition->layout_positions(
        *layout_definition, num_figures, declared_capacity(), &positions);
    if (!positioned || positions.empty()) {
        log_error("Formation cannot position its roster without a declared FormationType grid", "formation", id);
        std::terminate();
    }

    const int base_offset = map_grid_offset(positions[0].x, positions[0].y);
    std::vector<int> offsets(positions.size(), 0);
    for (size_t index = 1; index < positions.size(); index++) {
        offsets[index] = map_grid_offset(positions[index].x, positions[index].y) - base_offset;
    }
    return offsets;
}

FormationLayoutPosition formation::layout_position(int index, const FormationLayoutDef *layout_override) const
{
    require_definition("layout_position");
    const FormationLayoutDef *layout = layout_override ? layout_override : layout_definition;
    if (!layout) {
        log_error("Live formation has no FormationLayout", formation_type_definition->key(), static_cast<int>(id));
        std::terminate();
    }
    FormationLayoutPosition position = {};
    if (!formation_type_definition->try_layout_position(*layout, index, declared_capacity(), &position)) {
        log_error("Formation cannot resolve a declared layout position", formation_type_definition->key(), index);
        std::terminate();
    }
    return position;
}

void formation::rebuild_member_movement_plan(
    FormationMemberDestination destination,
    const FormationLayoutDef &layout,
    int origin_x,
    int origin_y,
    int area_width,
    int area_height)
{
    require_definition("rebuild_member_movement_plan");
    const FormationMemberMovementPlan previous_plan = member_movement_plan;
    const int capacity = declared_capacity();
    std::vector<FigureMovementDestination> ideals(static_cast<size_t>(capacity));
    for (int slot = 0; slot < capacity; slot++) {
        FormationSlotOffset offset = {};
        if (!formation_type_definition->resolve_station_offset(
            layout, slot, capacity, area_width, area_height, &offset)) {
            log_error("Formation cannot resolve a declared world-space station", "formation", static_cast<int>(id));
            std::terminate();
        }
        ideals[static_cast<size_t>(slot)] = {
            figure_movement_tile_to_cross_country(origin_x) + offset.x,
            figure_movement_tile_to_cross_country(origin_y) + offset.y,
            FigureMovementPlane::Ground
        };
    }
    const FormationMovementBounds bounds = {
        0,
        0,
        map_grid_width() * FIGURE_CROSS_COUNTRY_TILE_UNITS - 1,
        map_grid_height() * FIGURE_CROSS_COUNTRY_TILE_UNITS - 1
    };
    if (!member_movement_plan.configure(
        destination, layout, origin_x, origin_y, area_width, area_height, bounds, ideals)) {
        log_error("Formation declared invalid world-space stations", "formation", static_cast<int>(id));
        std::terminate();
    }

    std::vector<FormationMemberStationRequest> requests;
    std::vector<Figure *> members(static_cast<size_t>(capacity), nullptr);
    for (int slot = 0; slot < capacity; slot++) {
        const int figure_id = roster_figure_id(slot);
        Figure *member = figure_id ? Figure::get(figure_id) : nullptr;
        if (!member || member->state != FIGURE_STATE_ALIVE || !is_roster_member(*member)) continue;
        members[static_cast<size_t>(slot)] = member;
        requests.push_back({slot, member});
    }
    int queried_slot = -1;
    std::unique_ptr<Route::DistanceQuery> route;
    const auto reachable = [this, &members, &queried_slot, &route](
        int slot, const FigureMovementDestination &candidate) {
        if (slot != queried_slot) {
            queried_slot = slot;
            route = std::make_unique<Route::DistanceQuery>(
                Route::DistanceQuery::fromFigure(*members[static_cast<size_t>(slot)]));
        }
        return member_station_route_distance(*route, candidate);
    };
    if (!member_movement_plan.assign_members(requests, reachable)) {
        log_error("Formation could not allocate its stable member stations", "formation", static_cast<int>(id));
        std::terminate();
    }

    for (int slot = 0; slot < capacity; slot++) {
        Figure *member = members[static_cast<size_t>(slot)];
        if (!member) continue;
        FigureMovementDestination previous_destination = {};
        FigureMovementDestination resolved_destination = {};
        FormationStationState previous_state = FormationStationState::Unplaced;
        FormationStationState resolved_state = FormationStationState::Unplaced;
        const bool had_previous = previous_plan.resolve_member(
            slot, *member, &previous_destination, &previous_state);
        const bool has_resolved = member_movement_plan.resolve_member(
            slot, *member, &resolved_destination, &resolved_state);
        const bool destination_changed = !had_previous || !has_resolved || previous_state != resolved_state ||
            previous_destination.x != resolved_destination.x || previous_destination.y != resolved_destination.y ||
            previous_destination.plane != resolved_destination.plane;
        if (destination_changed) Route::remove(member);
    }
}

int formation::member_station_route_distance(
    const Route::DistanceQuery &route,
    const FigureMovementDestination &candidate) const
{
    const int tile_x = figure_movement_cross_country_to_tile(candidate.x);
    const int tile_y = figure_movement_cross_country_to_tile(candidate.y);
    if (!map_grid_is_inside(tile_x, tile_y, 1)) return 0;
    return route.distanceTo(map_grid_offset(tile_x, tile_y));
}

void formation::resolve_member_movement_station(Figure &figure)
{
    const int slot = figure.index_in_formation;
    if (slot < 0 || slot >= declared_capacity() || roster_figure_id(slot) != static_cast<int>(figure.id())) {
        log_error("Formation cannot assign a station to a non-roster member", "formation", static_cast<int>(id));
        std::terminate();
    }
    const Route::DistanceQuery route = Route::DistanceQuery::fromFigure(figure);
    const auto reachable = [this, &route](int, const FigureMovementDestination &candidate) {
        return member_station_route_distance(route, candidate);
    };
    if (!member_movement_plan.assign_member(slot, figure, reachable)) {
        log_error("Formation could not reserve a stable member station", "formation", static_cast<int>(id));
        std::terminate();
    }
}

FormationMemberMovementResult formation::move_member_to_slot(
    Figure &figure,
    FormationMemberDestination destination,
    int num_ticks,
    int tick_percentage)
{
    require_definition("move_member_to_slot");
    Building *home = fort();
    if (!home || !home->Composition) {
        log_error("Legion cannot place a member without its fort composition", "formation", static_cast<int>(id));
        std::terminate();
    }
    Building &ground = home->Composition->require_child_for_role(
        "mustering_ground", "formation::move_member_to_slot");
    if (!ground.Foundation || !ground.Foundation->state().is_published()) {
        log_error("Legion mustering ground has no published Foundation", "formation", static_cast<int>(id));
        std::terminate();
    }
    const bool at_fort = destination == FormationMemberDestination::MusteringGround;
    const FormationLayoutDef *layout = at_fort ? formation_layout_registry_impl::find_layout("at_rest") : layout_definition;
    const building_type_registry_impl::FoundationState &foundation = ground.Foundation->state();
    const int width = ground.Foundation->width(foundation.rotation());
    const int height = ground.Foundation->height(foundation.rotation());
    if (!layout) {
        log_error("Legion member has no declared FormationLayout", "formation", static_cast<int>(id));
        std::terminate();
    }
    const int origin_x = at_fort ? foundation.origin_x() : standard_x;
    const int origin_y = at_fort ? foundation.origin_y() : standard_y;
    const bool plan_matches = member_movement_plan.matches(
        destination, *layout, origin_x, origin_y, width, height, declared_capacity());
    if (!plan_matches) rebuild_member_movement_plan(destination, *layout, origin_x, origin_y, width, height);

    const int slot = figure.index_in_formation;
    FigureMovementDestination movement_destination = {};
    FormationStationState station_state = FormationStationState::Unplaced;
    const bool had_station = member_movement_plan.resolve_member(
        slot, figure, &movement_destination, &station_state);
    if (!had_station) {
        resolve_member_movement_station(figure);
        Route::remove(&figure);
    }
    if (!member_movement_plan.resolve_member(slot, figure, &movement_destination, &station_state)) {
        log_error("Formation member has no stable station relationship", "formation", static_cast<int>(id));
        std::terminate();
    }
    if (station_state == FormationStationState::Unplaced) {
        return settle_member(figure, destination, *layout, FormationMemberMovementResult::SettledUnplaced);
    }

    const int tile_x = figure_movement_cross_country_to_tile(movement_destination.x);
    const int tile_y = figure_movement_cross_country_to_tile(movement_destination.y);
    if (!map_grid_is_inside(tile_x, tile_y, 1)) {
        log_error("Legion member destination is outside the live map", "formation", static_cast<int>(id));
        std::terminate();
    }
    figure.formation_position_x.soldier = static_cast<unsigned char>(tile_x);
    figure.formation_position_y.soldier = static_cast<unsigned char>(tile_y);
    const FigureMovementResult result = figure.move_ticks_to(movement_destination, num_ticks, tick_percentage);
    if (result == FigureMovementResult::Moving) return FormationMemberMovementResult::Moving;
    if (result == FigureMovementResult::Arrived) {
        return settle_member(figure, destination, *layout, FormationMemberMovementResult::Stationed);
    }

    member_movement_plan.mark_unplaced(slot, figure);
    return settle_member(figure, destination, *layout, FormationMemberMovementResult::SettledUnplaced);
}

FormationMemberMovementResult formation::settle_member(
    Figure &figure,
    FormationMemberDestination destination,
    const FormationLayoutDef &layout,
    FormationMemberMovementResult result) const
{
    figure.formation_position_x.soldier = static_cast<unsigned char>(figure.x);
    figure.formation_position_y.soldier = static_cast<unsigned char>(figure.y);
    const int stationary_facing = destination == FormationMemberDestination::MusteringGround ?
        layout.stationary_facing : direction;
    if (stationary_facing < DIR_0_TOP || stationary_facing >= DIR_8_NONE) {
        log_error("Formation has no valid stationary facing for its active layout", layout.key(), static_cast<int>(id));
        std::terminate();
    }
    figure.previous_tile_direction = static_cast<signed char>(stationary_facing);
    figure.direction = DIR_8_NONE;
    return result;
}

void formation::require_definition(const char *operation) const
{
    if (!formation_type_definition) {
        char detail[256];
        snprintf(detail, sizeof(detail),
            "operation=%s formation_id=%u in_use=%u legion=%u herd=%u figure_type=%u fort_id=%u figures=%d",
            operation, id, in_use, is_legion, is_herd, static_cast<unsigned int>(figure_type),
            static_cast<unsigned int>(building_id), num_figures);
        log_error("Live formation is missing its FormationType definition", detail, static_cast<int>(id));
        std::terminate();
    }
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
    Figure *first_alive = nullptr;
    for_each_alive_figure([&](Figure &figure, int) {
        if (!first_alive) {
            first_alive = &figure;
        }
    });
    return first_alive;
}

void formation::for_each_figure(const std::function<void(Figure &, int)> &visitor) const
{
    if (!visitor) {
        return;
    }
    for_each_figure_id([&](int figure_id, int slot) {
        visitor(*Figure::get(figure_id), slot);
    });
}

void formation::for_each_alive_figure(const std::function<void(Figure &, int)> &visitor) const
{
    for_each_figure([&](Figure &figure, int slot) {
        if (!figure.is_dead()) {
            visitor(figure, slot);
        }
    });
}

int formation::count_alive_figures() const
{
    int alive_figures = 0;
    for_each_alive_figure([&](Figure &, int) {
        alive_figures++;
    });
    return alive_figures;
}

int formation::count_figures_in_action(int action_state) const
{
    int matching_figures = 0;
    for_each_figure([&](Figure &figure, int) {
        if (figure.action_state == action_state) {
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
    for_each_figure([](Figure &figure, int) {
        figure.state = FIGURE_STATE_DEAD;
    });
}

int formation::kill_alive_figures(int limit) const
{
    int killed = 0;
    for_each_alive_figure([&](Figure &figure, int) {
        if (killed >= limit) {
            return;
        }
        figure.state = FIGURE_STATE_DEAD;
        killed++;
    });
    return killed;
}

void formation::set_all_figures_action(int action_state) const
{
    for_each_figure([&](Figure &figure, int) {
        figure.action_state = static_cast<unsigned char>(action_state);
    });
}

void formation::set_alive_figures_action(int action_state) const
{
    for_each_alive_figure([&](Figure &figure, int) {
        figure.action_state = static_cast<unsigned char>(action_state);
        figure.formation_at_rest = 0;
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
        f->action_state = static_cast<unsigned char>(action_state);
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
        f->action_state = static_cast<unsigned char>(action_state);
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
            int target_id = figure_combat_get_target_for_aggressive_herd(f->x, f->y, 6);
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
        change_morale(10);
    }
    return true;
}

void formation::send_non_combat_figures_to_standard()
{
    invalidate_member_movement_plan();
    for_each_figure_id([&](int figure_id, int) {
        Figure *f = Figure::get(figure_id);
        if (figure_is_combat_locked(*f)) {
            return;
        }
        if (prepare_legion_to_move()) {
            f->action_state = FIGURE_ACTION_83_SOLDIER_GOING_TO_STANDARD;
            Route::remove(f);
        }
    });
}

void formation::send_non_combat_figures_to_fort()
{
    invalidate_member_movement_plan();
    for_each_figure_id([&](int figure_id, int) {
        Figure *f = Figure::get(figure_id);
        if (figure_is_combat_locked(*f)) {
            return;
        }
        if (prepare_legion_to_move()) {
            f->action_state = FIGURE_ACTION_81_SOLDIER_GOING_TO_FORT;
            Route::remove(f);
            f->formation_at_rest = 0;
        }
    });
}

formation *formation_create_legion(Building &fort)
{
    formation *m;
    m = new_formation_after_index(1);
    if (!m) {
        return formation_slot(0);
    }
    if (!m->initialize_legion_from_fort(fort, data.num_legions + 1)) {
        m->remove();
        return formation_slot(0);
    }

    data.num_legions++;
    if (m->id > (unsigned int) data.id_last_in_use) {
        data.id_last_in_use = m->id;
    }

    //standards and name
    formation_assign_available_legion_regalia(m, m->legion_id);
    return m;
}

static formation *formation_create(
    figure_type type,
    const FormationType &definition,
    const char *layout_key,
    int orientation,
    int x,
    int y)
{
    formation *f;
    f = new_formation_after_index(20);
    if (!f) {
        return 0;
    }
    f->bind_definition(definition);
    f->faction_id = 0;
    f->x = x;
    f->y = y;
    f->in_use = 1;
    f->is_legion = 0;
    f->figure_type = type;
    f->legion_id = 0; //legions created separately
    f->morale = 100;
    if (layout_key && !strcmp(layout_key, "enemy_double_line")) {
        const char *resolved_layout = orientation == DIR_0_TOP || orientation == DIR_4_BOTTOM ?
            "double_line_1" : "double_line_2";
        if (!f->set_layout(resolved_layout)) {
            log_error("Unable to create formation without FormationLayout", resolved_layout, 0);
            f->remove();
            return nullptr;
        }
    } else {
        if (!f->set_layout(layout_key)) {
            log_error("Unable to create formation without FormationLayout", layout_key, 0);
            f->remove();
            return nullptr;
        }
    }
    f->target_formation.clear();
    return f;
}

int formation_create_herd(const FormationType &definition, int x, int y)
{
    if (definition.spawn.role != FormationSpawnRole::Herd || definition.spawn.initial_count <= 0) {
        log_error("Unable to create herd from invalid FormationType spawn definition", 0, 0);
        return 0;
    }
    formation *f = formation_create(definition.primary_unit()->figure_type_id(), definition, "herd", 0, x, y);
    if (!f) {
        return 0;
    }
    f->wait_ticks = 24;
    return f->id;
}

int formation_create_enemy(figure_type type, int x, int y, int layout, int orientation,
    int enemy_type, int attack_type, int invasion_id, int invasion_sequence)
{
    const FormationLayoutDef *layout_definition = formation_layout_registry_impl::find_layout_by_legacy_id(layout);
    if (!layout_definition) {
        log_error("Unable to create enemy formation from unknown legacy FormationLayout id", 0, layout);
        return 0;
    }
    const FormationType *formation_definition = formation_type_registry_impl::find_spawn_formation(type);
    if (!formation_definition || formation_definition->spawn.role != FormationSpawnRole::Enemy) {
        log_error("Unable to create enemy without a data-defined FormationType", 0, static_cast<int>(type));
        return 0;
    }
    formation *f = formation_create(
        type,
        *formation_definition,
        layout_definition->key(),
        orientation,
        x,
        y);
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

void formation::record_missile_attack(int from_formation_id)
{
    missile_attack_timeout = 6;
    missile_attack_formation_id = from_formation_id;
}

void formation::pursue_target()
{
    formation *target = target_formation.get_ptr();
    if (!target) return;
    Figure *focus = target->in_use ? target->first_alive_figure() : nullptr;
    if (!focus) {
        target_formation.clear();
        return;
    }
    if (standard_x == focus->x && standard_y == focus->y) return;
    set_station_origin(focus->x, focus->y);
    is_at_fort = 0;
    send_non_combat_figures_to_standard();
}

void formation::update_movement_states()
{
    is_halted = !any_figure_id([](int figure_id, int) {
        return Figure::get(figure_id)->direction != DIR_8_NONE;
    });

    const bool is_moving_now = standard_x != x_home || standard_y != y_home;
    const int current_offset = map_grid_offset(x_home, y_home);
    if (is_moving_now && !is_moving) {
        is_moving = 1;
        started_moving_from_grid_offset = current_offset;
        traveled_tiles = 0;
    } else if (is_moving_now) {
        traveled_tiles = map_grid_chess_distance(started_moving_from_grid_offset, current_offset);
    } else if (is_moving) {
        traveled_tiles = map_grid_chess_distance(started_moving_from_grid_offset, current_offset);
        halted_at_grid_offset = current_offset;
        is_moving = 0;
    }

    is_charging = is_moving ? traveled_tiles >= 5 :
        is_halted && traveled_tiles >= 5 && (halted_for_months < 2 || recent_fight >= 4);
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
                    m->set_home(f->x, f->y);
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
    } else if (grand_temple_for_god(GOD_MARS, true)) {
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

void formation::change_morale(int amount)
{
    morale = calc_bound(morale + amount, 0, base_morale_limit() + mess_hall_max_morale_modifier);
}

void formation::update_morale_after_death()
{
    formation_calculate_figures();
    int pct_dead = calc_percentage(1, num_figures + 1);
    int morale_delta;
    if (pct_dead < 8) {
        morale_delta = -4;
    } else if (pct_dead < 10) {
        morale_delta = -6;
    } else if (pct_dead < 14) {
        morale_delta = -8;
    } else if (pct_dead < 20) {
        morale_delta = -10;
    } else if (pct_dead < 30) {
        morale_delta = -12;
    } else {
        morale_delta = -16;
    }
    change_morale(morale_delta);
}

static void change_all_morale(int legion, int enemy)
{
    for (unsigned int i = 1; i < formations.size(); i++) {
        formation *m = formation_get(i);
        if (m->in_use && !m->is_herd) {
            if (m->is_legion) {
                m->change_morale(legion);
            } else {
                m->change_morale(enemy);
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
                m->change_morale(5);
                formation_legion_restore_layout(m);
            } else if (!m->recent_fight) {
                m->months_from_home++;
                if (m->months_from_home > 3) {
                    if (m->months_from_home > 100) {
                        m->months_from_home = 100;
                    }
                    m->change_morale(-5);
                }
            }
            update_morale_for_mess_hall();
        } else {
            m->change_morale(0);
        }
    }
}

void formation::decrease_monthly_counters()
{
    if (is_legion) {
        if (cursed_by_mars) {
            cursed_by_mars--;
        }
    }
    if (missile_fired) {
        missile_fired--;
    }
    if (missile_attack_timeout) {
        missile_attack_timeout--;
    }
    if (recent_fight) {
        recent_fight--;
    }
    // this one increases
    if (is_halted) {
        halted_for_months++;
    }
}

void formation::clear_monthly_counters()
{
    missile_fired = 0;
    missile_attack_timeout = 0;
    recent_fight = 0;
}

void formation::set_destination(int tile_x, int tile_y)
{
    destination_x = tile_x;
    destination_y = tile_y;
}

void formation::set_destination(int tile_x, int tile_y, const Building *building)
{
    set_destination(tile_x, tile_y);
    destination_building_id = building ? building->id : 0;
}

void formation::set_station_origin(int tile_x, int tile_y)
{
    const bool changed = standard_x != tile_x || standard_y != tile_y;
    standard_x = tile_x;
    standard_y = tile_y;
    if (is_legion && in_use && !in_distant_battle) standard.place(tile_x, tile_y);
    else standard.clear();
    if (changed) invalidate_member_movement_plan();
}

void formation::set_distant_battle(int value)
{
    in_distant_battle = value ? 1 : 0;
    if (!is_legion || !in_use || in_distant_battle) standard.clear();
    else standard.place(standard_x, standard_y);
}

static void prepare_rosters_for_refresh(void)
{
    for (unsigned int i = 1; i < formations.size(); i++) {
        formation *f = formation_get(i);
        if (!f->in_use) {
            continue;
        }
        f->prepare_roster_refresh();
    }
}

int formation_legion_count_alive_soldiers_by_type(figure_type type)
{
    int totals = 0;
    for (formation &entry : formations) {
        formation *m = &entry;
        if (m->in_use && m->matches_player_troop_query(type)) {
            totals += m->count_alive_figures();
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
    prepare_rosters_for_refresh();
    for (unsigned int i = 1; i < Figure::count(); i++) {
        Figure *f = Figure::get(i);
        if (f->state != FIGURE_STATE_ALIVE || !f->formation_id) {
            continue;
        }
        formation *m = formation_get(f->formation_id);
        if (!m || !m->owns_figure(*f)) {
            char detail[192];
            snprintf(detail, sizeof(detail), "figure_id=%u figure_type=%u formation_id=%u",
                f->id(), static_cast<unsigned int>(f->type), f->formation_id);
            log_error("Live figure has an invalid formation relationship", detail, 0);
            std::terminate();
        }
        if (!m->is_roster_member(*f)) {
            continue;
        }
        m->refresh_published_figure(*f);
    }

    enemy_army_totals_clear();
    for (unsigned int i = 1; i < formations.size(); i++) {
        formation *m = formation_get(i);
        if (m->in_use && !m->is_herd) {
            if (m->is_legion) {
                if (m->has_figures()) {
                    int was_halted = m->is_halted;
                    int was_charging = m->is_charging;
                    m->update_movement_states();
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
                    m->remove();
                } else {
                    enemy_army_totals_add_enemy_formation(m->num_figures);
                }
            }
        }
    }

    city_military_update_totals();
}

void formation::update_direction(const Figure &first_figure)
{
    if (unknown_fired) {
        unknown_fired--;
    } else if (missile_fired) {
        const int firing_direction = first_figure.direction < DIR_8_NONE ?
            first_figure.direction : first_figure.previous_tile_direction;
        if (firing_direction >= DIR_0_TOP && firing_direction < DIR_8_NONE) direction = firing_direction;
    } else if (uses_layout("double_line_1") || uses_layout("single_line_1")) {
        if (y_home < prev.y_home) {
            direction = DIR_0_TOP;
        } else if (y_home > prev.y_home) {
            direction = DIR_4_BOTTOM;
        }
    } else if (uses_layout("double_line_2") || uses_layout("single_line_2")) {
        if (x_home < prev.x_home) {
            direction = DIR_6_LEFT;
        } else if (x_home > prev.x_home) {
            direction = DIR_2_RIGHT;
        }
    } else if (uses_layout("tortoise") || uses_layout("column")) {
        int dx = (x_home < prev.x_home) ? (prev.x_home - x_home) : (x_home - prev.x_home);
        int dy = (y_home < prev.y_home) ? (prev.y_home - y_home) : (y_home - prev.y_home);
        if (dx > dy) {
            if (x_home < prev.x_home) {
                direction = DIR_6_LEFT;
            } else if (x_home > prev.x_home) {
                direction = DIR_2_RIGHT;
            }
        } else {
            if (y_home < prev.y_home) {
                direction = DIR_0_TOP;
            } else if (y_home > prev.y_home) {
                direction = DIR_4_BOTTOM;
            }
        }
    }
    prev.x_home = x_home;
    prev.y_home = y_home;
}

int formation_refresh_runtime_definitions(void)
{
    Building::for_each([](Building *building) {
        building->Formation = nullptr;
    });

    int valid = 1;
    for (formation &entry : formations) {
        if (!entry.in_use) {
            continue;
        }
        if (entry.is_legion) {
            entry.formation_type_definition = nullptr;
            if (!entry.refresh_legion_definition_from_home()) {
                valid = 0;
            }
            continue;
        }
        const int serialized_is_herd = entry.is_herd;
        const FormationType *definition = formation_type_registry_impl::find_spawn_formation(entry.figure_type_id());
        if (!definition) {
            log_error("Unable to hydrate formation without a data-defined FormationType", 0, static_cast<int>(entry.id));
            valid = 0;
            continue;
        }
        entry.bind_definition(*definition);
        if (serialized_is_herd != entry.is_herd) {
            char detail[160];
            snprintf(detail, sizeof(detail), "formation_id=%u saved_is_herd=%d canonical_is_herd=%d definition=%s",
                entry.id, serialized_is_herd, entry.is_herd, definition->key());
            log_warning("Repairing serialized formation role from its FormationType", detail, 0);
        }
    }
    if (!valid) {
        return 0;
    }
    for (unsigned int figure_id = 1; figure_id < Figure::count(); figure_id++) {
        Figure *figure = Figure::get(figure_id);
        if (!figure || figure->state != FIGURE_STATE_ALIVE || !figure->formation_id) {
            continue;
        }
        if (figure->type == FIGURE_FORT_STANDARD) continue;
        formation *owner = formation_slot(figure->formation_id);
        if (owner && owner->owns_figure(*figure)) {
            continue;
        }
        char detail[256];
        snprintf(detail, sizeof(detail),
            "figure_id=%u figure_type=%u saved_formation_id=%u formation_in_use=%u",
            figure->id(), static_cast<unsigned int>(figure->type), figure->formation_id,
            owner ? owner->in_use : 0);
        log_warning("Discarding loaded combat figure with an invalid formation relationship", detail, 0);
        figure->remove();
    }
    return valid;
}

int formation_finish_load_bridge(void)
{
    for (unsigned int figure_id = Figure::count(); figure_id-- > 1;) {
        Figure *figure = Figure::get(figure_id);
        if (!figure || figure->state != FIGURE_STATE_ALIVE || figure->type != FIGURE_FORT_STANDARD) continue;
        char detail[128];
        snprintf(detail, sizeof(detail), "figure_id=%u saved_formation_id=%u", figure->id(), figure->formation_id);
        log_warning("Migrating a serialized fort standard into its formation-owned destination", detail, 0);
        figure->remove();
    }

    for (formation &entry : formations) {
        if (!entry.in_use || !entry.is_legion) continue;
        if (!entry.formation_type_definition) {
            log_error("Loaded legion has no FormationType after the load bridge", 0, static_cast<int>(entry.id));
            return 0;
        }
        if (!entry.reconcile_loaded_legion_command()) return 0;
    }

    for (unsigned int figure_id = 1; figure_id < Figure::count(); figure_id++) {
        const Figure *figure = Figure::get(figure_id);
        if (figure && figure->state == FIGURE_STATE_ALIVE && figure->type == FIGURE_FORT_STANDARD) {
            log_error("Load bridge left a live fort standard figure in runtime", 0, static_cast<int>(figure_id));
            return 0;
        }
    }
    return 1;
}

void formation_update_all(int second_time)
{
    formation_calculate_legion_totals();
    formation_calculate_figures();
    for (formation &entry : formations) {
        if (!entry.in_use || entry.is_herd) continue;
        if (Figure *first = entry.first_alive_figure()) entry.update_direction(*first);
    }
    formation_legion_decrease_damage();
    if (!second_time) {
        formation_update_monthly_morale_deployed();
    }
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
        buffer_write_u8(buf, static_cast<uint8_t>(f->in_use));
        buffer_write_u8(buf, static_cast<uint8_t>(f->faction_id));
        buffer_write_u8(buf, static_cast<uint8_t>(f->legion_id));
        buffer_write_u8(buf, static_cast<uint8_t>(f->is_at_fort));
        buffer_write_i16(buf, static_cast<int16_t>(f->figure_type));
        buffer_write_i16(buf, static_cast<int16_t>(f->building_id));
        f->write_legacy_figure_slots(buf);
        buffer_write_u8(buf, static_cast<uint8_t>(f->num_figures));
        buffer_write_u8(buf, static_cast<uint8_t>(f->max_figures));
        buffer_write_i16(buf, static_cast<int16_t>(formation_layout_to_legacy_id(f->layout_definition)));
        buffer_write_i16(buf, static_cast<int16_t>(f->morale));
        buffer_write_u8(buf, static_cast<uint8_t>(f->x_home));
        buffer_write_u8(buf, static_cast<uint8_t>(f->y_home));
        buffer_write_u8(buf, static_cast<uint8_t>(f->standard_x));
        buffer_write_u8(buf, static_cast<uint8_t>(f->standard_y));
        buffer_write_u8(buf, static_cast<uint8_t>(f->x));
        buffer_write_u8(buf, static_cast<uint8_t>(f->y));
        buffer_write_u8(buf, static_cast<uint8_t>(f->destination_x));
        buffer_write_u8(buf, static_cast<uint8_t>(f->destination_y));
        buffer_write_i16(buf, static_cast<int16_t>(f->destination_building_id));
        buffer_write_i16(buf, 0); // Reserved legacy fort-standard figure id. Runtime destinations are formation-owned.
        buffer_write_u8(buf, static_cast<uint8_t>(f->is_legion));
        buffer_write_u8(buf, static_cast<uint8_t>(f->mess_hall_max_morale_modifier));
        buffer_write_i32(buf, f->legion_flag_id);
        buffer_write_i32(buf, f->legion_name_id);
        buffer_write_i32(buf, f->legion_name_group);
        buffer_write_i16(buf, static_cast<int16_t>(f->attack_type));
        buffer_write_i16(buf, static_cast<int16_t>(f->legion_recruit_type));
        buffer_write_i16(buf, static_cast<int16_t>(f->has_military_training));
        buffer_write_i16(buf, static_cast<int16_t>(f->total_damage));
        buffer_write_i16(buf, static_cast<int16_t>(f->max_total_damage));
        buffer_write_i16(buf, static_cast<int16_t>(f->wait_ticks));
        buffer_write_i16(buf, static_cast<int16_t>(f->recent_fight));
        buffer_write_i16(buf, static_cast<int16_t>(f->enemy_state.duration_advance));
        buffer_write_i16(buf, static_cast<int16_t>(f->enemy_state.duration_regroup));
        buffer_write_i16(buf, static_cast<int16_t>(f->enemy_state.duration_halt));
        buffer_write_i16(buf, static_cast<int16_t>(f->enemy_legion_index));
        buffer_write_i16(buf, static_cast<int16_t>(f->is_halted));
        buffer_write_i16(buf, static_cast<int16_t>(f->missile_fired));
        buffer_write_i16(buf, static_cast<int16_t>(f->missile_attack_timeout));
        buffer_write_i16(buf, static_cast<int16_t>(f->missile_attack_formation_id));
        buffer_write_i16(buf, static_cast<int16_t>(formation_layout_to_legacy_id(f->prev.layout_definition)));
        buffer_write_i16(buf, static_cast<int16_t>(f->cursed_by_mars));
        buffer_write_u8(buf, static_cast<uint8_t>(f->months_low_morale));
        buffer_write_u8(buf, static_cast<uint8_t>(f->empire_service));
        buffer_write_u8(buf, static_cast<uint8_t>(f->in_distant_battle));
        buffer_write_u8(buf, static_cast<uint8_t>(f->is_herd));
        buffer_write_u8(buf, static_cast<uint8_t>(f->enemy_type));
        buffer_write_u8(buf, static_cast<uint8_t>(f->direction));
        buffer_write_u8(buf, static_cast<uint8_t>(f->prev.x_home));
        buffer_write_u8(buf, static_cast<uint8_t>(f->prev.y_home));
        buffer_write_u8(buf, static_cast<uint8_t>(f->unknown_fired));
        buffer_write_u8(buf, static_cast<uint8_t>(f->orientation));
        buffer_write_u8(buf, static_cast<uint8_t>(f->months_from_home));
        buffer_write_u8(buf, static_cast<uint8_t>(f->months_very_low_morale));
        buffer_write_u8(buf, static_cast<uint8_t>(f->invasion_id));
        buffer_write_u8(buf, static_cast<uint8_t>(f->herd_spawn_delay));
        buffer_write_u8(buf, static_cast<uint8_t>(f->herd_direction));
        buffer_write_i32(buf, f->target_formation.save_id());
        buffer_skip(buf, 13);
        buffer_write_i16(buf, static_cast<int16_t>(f->invasion_sequence));
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

    std::vector<unsigned int> loaded_target_formation_ids(static_cast<size_t>(formations_to_load));
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
        f->set_layout_from_legacy_id(buffer_read_i16(buf));
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
        buffer_read_i16(buf); // Consume the legacy fort-standard figure id at the save boundary.
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
        f->prev.layout_definition = formation_layout_from_legacy_id(buffer_read_i16(buf));
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
        f->herd_spawn_delay = buffer_read_u8(buf);
        f->herd_direction = buffer_read_u8(buf);
        loaded_target_formation_ids[static_cast<size_t>(i)] = buffer_read_i32(buf);
        buffer_skip(buf, 13);
        f->invasion_sequence = buffer_read_i16(buf);
        f->read_extended_figure_slots(buf, formation_buf_size);

        if (f->in_use) {
            highest_id_in_use = i;
        }
    }

    // Reduce number of available formations to improve performance
    formations.resize(highest_id_in_use + 1);

    // The load bridge repairs legacy ids into live object relationships after every formation exists.
    for (unsigned int i = 0; i < formations.size(); i++) {
        formation *f = formation_slot(i);
        const unsigned int target_id = loaded_target_formation_ids[i];
        formation *target = formation_slot(target_id);
        if (!target_id) continue;
        if (!f->in_use || target_id == i || !target || !target->in_use) {
            char detail[128];
            snprintf(detail, sizeof(detail), "formation_id=%u target_formation_id=%u", i, target_id);
            log_warning("Clearing invalid saved formation target relationship", detail, 0);
            continue;
        }
        f->target_formation.retarget(*target);
    }
}


