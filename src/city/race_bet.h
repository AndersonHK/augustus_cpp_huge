#pragma once

#include <algorithm>
#include <vector>

// Persisted saves use one-based team ordinals; runtime definitions and sessions
// use zero-based team indices. Zero remains the historical "no bet" sentinel.
constexpr unsigned int NO_BET = 0;

class RaceSession {
public:
    void start(unsigned int building_id, int expected_participants)
    {
        start(building_id, expected_participants, expected_participants);
    }

    void start(unsigned int building_id, int team_capacity, int expected_participants)
    {
        building_id_ = building_id;
        participants_.assign(std::max(0, team_capacity), false);
        expected_participants_ = std::clamp(expected_participants, 0, static_cast<int>(participants_.size()));
        finish_order_.clear();
    }

    int register_participant(int team_index)
    {
        if (team_index < 0 || team_index >= static_cast<int>(participants_.size())) return 0;
        participants_[team_index] = true;
        return participant_count();
    }

    int register_finish(int team_index)
    {
        if (team_index < 0 || team_index >= static_cast<int>(participants_.size()) || !participants_[team_index] ||
            std::find(finish_order_.begin(), finish_order_.end(), team_index) != finish_order_.end()) {
            return finish_position(team_index);
        }
        finish_order_.push_back(team_index);
        return static_cast<int>(finish_order_.size());
    }

    int participant_count() const
    {
        return static_cast<int>(std::count(participants_.begin(), participants_.end(), true));
    }

    int finish_position(int team_index) const
    {
        const auto found = std::find(finish_order_.begin(), finish_order_.end(), team_index);
        return found == finish_order_.end() ? 0 : static_cast<int>(found - finish_order_.begin()) + 1;
    }

    int winner() const { return finish_order_.empty() ? -1 : finish_order_.front(); }
    unsigned int building_id() const { return building_id_; }
    int team_capacity() const { return static_cast<int>(participants_.size()); }
    int expected_participants() const { return expected_participants_; }
    int has_complete_field() const { return participant_count() == expected_participants_; }
    int is_full_configured_field() const { return expected_participants_ == team_capacity(); }

private:
    unsigned int building_id_ = 0;
    int expected_participants_ = 0;
    std::vector<bool> participants_;
    std::vector<int> finish_order_;
};

int has_bet_in_progress(void);
int race_bet_can_place(void);
int race_bet_can_place_for(unsigned int building_id);
void race_bet_reset_runtime(void);
void race_bet_start(unsigned int building_id, int expected_participants);
void race_bet_end(unsigned int building_id);
int race_bet_is_active(unsigned int building_id);
int race_bet_any_active(void);
int race_bet_register_participant(unsigned int building_id, int team_index);
int race_bet_register_finish(unsigned int building_id, int team_index);
int race_bet_winner(unsigned int building_id);
int race_bet_finish_position(unsigned int building_id, int team_index);
unsigned int race_bet_selected_building(void);
void race_bet_select_building(unsigned int building_id);
