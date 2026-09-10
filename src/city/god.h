#pragma once

#include "city/constants.h"
#include "building/religion_effects.h"

#include <string>
#include <utility>

class God {
public:
    explicit God(std::string path)
        : path_(std::move(path))
    {
    }

    std::vector<religion::Effect> effects;
    religion::FavorRules favor;
    religion::WrathRules wrath;

    const char *path() const
    {
        return path_.c_str();
    }

    void set_runtime_id(int id)
    {
        runtime_id_ = id;
    }

    int runtime_id() const
    {
        return runtime_id_;
    }

    void set_legacy_type(god_type type)
    {
        legacy_type_ = type;
    }

    god_type legacy_type() const
    {
        return legacy_type_;
    }

private:
    std::string path_;
    int runtime_id_ = -1;
    god_type legacy_type_ = GOD_ALL;
};

int city_gods_count(void);
void city_gods_reset(void);
void city_gods_reset_neptune_blessing(void);
void city_gods_update_blessings(void);

void city_gods_calculate_moods(int update_moods);

int city_gods_calculate_least_happy(void);

void city_god_change_happiness(int god_id, int amount);

void city_god_set_happiness(int god_id, int amount_set);

int city_god_happiness(int god_id);

int city_god_wrath_bolts(int god_id);

int city_god_happy_bolts(int god_id);

int city_god_months_since_festival(int god_id);

/**
 * @return God ID or -1 if no single god is the least happy
 */
int city_god_least_happy(void);

int city_god_spirit_of_mars_power(void);
void city_god_spirit_of_mars_mark_used(void);

int city_god_neptune_create_shipwreck_flotsam(void);

int city_god_venus_bonus_employment(void);

void city_god_blessing(int god_id);

void city_god_curse(int god_id, int is_major);

bool city_gods_have_effects(religion::Trigger trigger);
bool city_gods_have_effects();
