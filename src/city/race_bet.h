#pragma once

#include <array>


typedef enum {
    NO_BET,
    BLUE_HORSE,
    RED_HORSE,
    WHITE_HORSE,
    GREEN_HORSE
} bet_horse;

class HippodromeRace {
public:
    void start(unsigned int hippodrome_id)
    {
        hippodrome_id_ = hippodrome_id;
        participants_.fill(false);
        finish_order_.fill(NO_BET);
        finish_count_ = 0;
    }

    int register_participant(unsigned int hippodrome_id, bet_horse horse)
    {
        if (!hippodrome_id || horse < BLUE_HORSE || horse > GREEN_HORSE) return 0;
        if (hippodrome_id_ != hippodrome_id) start(hippodrome_id);
        participants_[horse - BLUE_HORSE] = true;
        return participant_count();
    }

    int register_finish(unsigned int hippodrome_id, bet_horse horse)
    {
        if (!hippodrome_id || horse < BLUE_HORSE || horse > GREEN_HORSE) return 0;
        if (hippodrome_id_ != hippodrome_id) start(hippodrome_id);
        for (int i = 0; i < finish_count_; ++i) {
            if (finish_order_[i] == horse) return i + 1;
        }
        if (finish_count_ >= static_cast<int>(finish_order_.size())) return 0;
        finish_order_[finish_count_++] = horse;
        return finish_count_;
    }

    bet_horse winner() const { return finish_count_ ? finish_order_[0] : NO_BET; }
    unsigned int hippodrome_id() const { return hippodrome_id_; }

    int participant_count() const
    {
        int count = 0;
        for (bool participant : participants_) count += participant;
        return count;
    }

    int finish_position(bet_horse horse) const
    {
        for (int i = 0; i < finish_count_; ++i) {
            if (finish_order_[i] == horse) return i + 1;
        }
        return 0;
    }

private:
    unsigned int hippodrome_id_ = 0;
    std::array<bool, 4> participants_ = { false, false, false, false };
    std::array<bet_horse, 4> finish_order_ = { NO_BET, NO_BET, NO_BET, NO_BET };
    int finish_count_ = 0;
};

int has_bet_in_progress(void);
int race_bet_can_place(void);
void race_bet_reset_runtime(void);
void race_bet_start(unsigned int hippodrome_id);
int race_bet_register_participant(unsigned int hippodrome_id, bet_horse horse);
int race_bet_register_finish(unsigned int hippodrome_id, bet_horse horse);
bet_horse race_bet_winner(void);
int race_bet_finish_position(bet_horse horse);

