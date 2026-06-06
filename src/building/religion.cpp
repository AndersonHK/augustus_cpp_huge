#include "building/religion.h"

#include "city/god.h"

#include <utility>

namespace building_type_registry_impl {

Religion::Religion(std::string path)
    : path_(std::move(path))
{
}

const char *Religion::path() const
{
    return path_.c_str();
}

void Religion::add_god(const ::God *god)
{
    if (god) {
        gods_.push_back(god);
    }
}

void Religion::set_all_gods()
{
    all_gods_ = 1;
}

const std::vector<const ::God *> &Religion::gods() const
{
    return gods_;
}

int Religion::has_god(god_type god) const
{
    if (all_gods_) {
        return 1;
    }
    for (const ::God *current : gods_) {
        if (current && current->legacy_type() == god) {
            return 1;
        }
    }
    return 0;
}

void Religion::set_tier(ReligionTier tier)
{
    tier_ = tier;
}

ReligionTier Religion::tier() const
{
    return tier_;
}

int Religion::is_tier(ReligionTier tier) const
{
    return tier_ == tier;
}

void Religion::set_capacity(int capacity)
{
    capacity_ = capacity;
}

int Religion::capacity() const
{
    return capacity_;
}

} // namespace building_type_registry_impl
