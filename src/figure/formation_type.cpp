#include "figure/formation_type.h"

#include "core/log.h"
#include "core/xml_value.h"
#include "figure/movement.h"

#include <algorithm>
#include <exception>
#include <utility>

FormationType::FormationType(std::string key)
    : key_(std::move(key))
{
}

const char *FormationType::key() const
{
    return key_.c_str();
}

bool FormationType::matches_key(const char *key) const
{
    return xml_value::equals(key_.c_str(), key);
}

void FormationType::set_grid(int width, int height, FormationStationAlignment station_alignment)
{
    width_ = width;
    height_ = height;
    station_alignment_ = station_alignment;
}

void FormationType::set_recruit_capacity(int capacity)
{
    recruit_capacity_ = capacity;
}

void FormationType::set_combat_modifiers(const FormationCombatModifiers &modifiers)
{
    combat_modifiers_ = modifiers;
}

int FormationType::capacity() const
{
    return width_ * height_;
}

int FormationType::recruit_capacity() const
{
    return recruit_capacity_ > 0 ? recruit_capacity_ : capacity();
}

bool FormationType::try_layout_position(
    const FormationLayoutDef &layout,
    int index,
    int declared_capacity,
    FormationLayoutPosition *position) const
{
    return layout.try_position(index, declared_capacity, width_, height_, position);
}

bool FormationType::layout_positions(
    const FormationLayoutDef &layout,
    int live_slot_count,
    int declared_capacity,
    std::vector<FormationLayoutPosition> *positions) const
{
    return layout.positions(live_slot_count, declared_capacity, width_, height_, positions);
}

bool FormationType::resolve_station_offset(
    const FormationLayoutDef &layout,
    int index,
    int declared_capacity,
    int area_width,
    int area_height,
    FormationSlotOffset *offset) const
{
    if (!offset || !has_grid() || station_alignment_ == FormationStationAlignment::Unspecified ||
        area_width <= 0 || area_height <= 0) {
        return false;
    }
    FormationLayoutPosition position = {};
    if (!try_layout_position(layout, index, declared_capacity, &position)) return false;
    if (station_alignment_ == FormationStationAlignment::TileAnchor) {
        offset->x = position.x * FIGURE_CROSS_COUNTRY_TILE_UNITS;
        offset->y = position.y * FIGURE_CROSS_COUNTRY_TILE_UNITS;
        return true;
    }
    const auto scale_tile_anchor = [](int coordinate, int grid_extent, int area_extent) {
        if (grid_extent <= 1 || area_extent <= 1) return 0;
        const int denominator = grid_extent - 1;
        const int numerator = coordinate * (area_extent - 1) * FIGURE_CROSS_COUNTRY_TILE_UNITS;
        return numerator >= 0 ? (numerator + denominator / 2) / denominator :
            -((-numerator + denominator / 2) / denominator);
    };
    offset->x = scale_tile_anchor(position.x, width_, area_width);
    offset->y = scale_tile_anchor(position.y, height_, area_height);
    return true;
}

int FormationType::modified_combat_value(FormationCombatStat stat, int base_value) const
{
    int modifier = 0;
    switch (stat) {
        case FormationCombatStat::MeleeAttack: modifier = combat_modifiers_.melee_attack; break;
        case FormationCombatStat::MeleeDefense: modifier = combat_modifiers_.melee_defense; break;
        case FormationCombatStat::Morale: modifier = combat_modifiers_.morale; break;
        case FormationCombatStat::MissileDamage: modifier = combat_modifiers_.missile_damage; break;
    }
    return std::max(0, base_value + modifier);
}

int FormationType::distant_battle_strength(bool trained) const
{
    if (combat_modifiers_.distant_battle_strength <= 0 || combat_modifiers_.trained_distant_battle_bonus < 0) {
        log_error("FormationType lacks explicit distant-battle combat data", key_.c_str(), 0);
        std::terminate();
    }
    return combat_modifiers_.distant_battle_strength + (trained ? combat_modifiers_.trained_distant_battle_bonus : 0);
}

int FormationType::curse_weight(int figures) const
{
    if (combat_modifiers_.curse_weight_per_figure <= 0) {
        log_error("FormationType lacks explicit curse-weight combat data", key_.c_str(), 0);
        std::terminate();
    }
    return figures * combat_modifiers_.curse_weight_per_figure;
}

bool FormationType::has_grid() const
{
    return width_ > 0 && height_ > 0;
}

bool FormationType::has_slots() const
{
    return !slots_.empty();
}

bool FormationType::has_valid_slots() const
{
    for (const FormationTypeSlot &slot : slots_) {
        if (!contains(slot.x, slot.y) || !slot.unit) {
            return false;
        }
    }
    return true;
}

bool FormationType::has_duplicate_slots() const
{
    std::vector<bool> occupied(static_cast<size_t>(capacity()), false);
    for (const FormationTypeSlot &slot : slots_) {
        if (!contains(slot.x, slot.y)) {
            return false;
        }
        const size_t index = static_cast<size_t>(slot_index(slot.x, slot.y));
        if (occupied[index]) {
            return true;
        }
        occupied[index] = true;
    }
    return false;
}

bool FormationType::can_spawn_figures_directly() const
{
    const UnitType *unit = primary_unit();
    if (!unit || static_cast<int>(slots_.size()) != capacity()) {
        return false;
    }
    return std::all_of(slots_.begin(), slots_.end(), [unit](const FormationTypeSlot &slot) {
        return slot.unit == unit;
    });
}

bool FormationType::contains_row_range(int first_row, int last_row) const
{
    return first_row >= 0 && last_row >= first_row && last_row < height_;
}

bool FormationType::contains(int x, int y) const
{
    return x >= 0 && y >= 0 && x < width_ && y < height_;
}

int FormationType::slot_index(int x, int y) const
{
    return y * width_ + x;
}

bool FormationType::add_slot(int x, int y, const char *unit_key)
{
    if (!unit_key || !*unit_key || !contains(x, y)) {
        return false;
    }

    std::string normalized_unit_key = xml_value::trim_copy(unit_key);
    const UnitType *unit = unit_type_registry_impl::find_unit_type(normalized_unit_key.c_str());
    if (!unit) {
        log_error("FormationType references unknown UnitType", normalized_unit_key.c_str(), 0);
        return false;
    }

    slots_.push_back({ x, y, unit });
    return true;
}

bool FormationType::fill_slots(const char *unit_key)
{
    if (!has_grid()) {
        return false;
    }
    for (int y = 0; y < height_; y++) {
        for (int x = 0; x < width_; x++) {
            if (!add_slot(x, y, unit_key)) {
                return false;
            }
        }
    }
    return true;
}

bool FormationType::add_slots_in_row_range(int first_row, int last_row, const char *unit_key)
{
    if (!contains_row_range(first_row, last_row)) {
        return false;
    }
    for (int y = first_row; y <= last_row; y++) {
        for (int x = 0; x < width_; x++) {
            if (!add_slot(x, y, unit_key)) {
                return false;
            }
        }
    }
    return true;
}

const UnitType *FormationType::primary_unit() const
{
    return slots_.empty() ? nullptr : slots_.front().unit;
}
