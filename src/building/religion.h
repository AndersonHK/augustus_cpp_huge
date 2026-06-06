#pragma once

extern "C" {
#include "city/constants.h"
}

#include <string>
#include <vector>

class God;

namespace building_type_registry_impl {

enum class ReligionTier {
    None,
    Shrine,
    Small,
    Large,
    Grand,
    Oracle,
    Pantheon
};

class Religion {
public:
    explicit Religion(std::string path);

    const char *path() const;
    void add_god(const ::God *god);
    void set_all_gods();
    const std::vector<const ::God *> &gods() const;
    int has_god(god_type god) const;
    void set_tier(ReligionTier tier);
    ReligionTier tier() const;
    int is_tier(ReligionTier tier) const;
    void set_capacity(int capacity);
    int capacity() const;

private:
    std::string path_;
    std::vector<const ::God *> gods_;
    int all_gods_ = 0;
    ReligionTier tier_ = ReligionTier::None;
    int capacity_ = 0;
};

} // namespace building_type_registry_impl
