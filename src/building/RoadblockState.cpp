#include "building/RoadblockState.h"

#include <limits>

namespace building_type_registry_impl {

uint16_t RoadblockState::mask_for(int permission)
{
    return permission > 0 && permission < std::numeric_limits<uint16_t>::digits
        ? static_cast<uint16_t>(1u << permission)
        : 0;
}

void RoadblockState::configure(
    const char *definition_path,
    uint16_t default_permissions,
    uint16_t configurable_permissions)
{
    const uint16_t previous_permissions = permissions_;
    const std::string new_definition_path = definition_path ? definition_path : "";
    const int same_definition = !definition_path_.empty() &&
        definition_path_ == new_definition_path &&
        default_permissions_ == default_permissions &&
        configurable_permissions_ == configurable_permissions;
    definition_path_ = new_definition_path;
    default_permissions_ = default_permissions;
    configurable_permissions_ = configurable_permissions;
    permissions_ = same_definition
        ? static_cast<uint16_t>((default_permissions_ & ~configurable_permissions_) |
            (previous_permissions & configurable_permissions_))
        : default_permissions_;
}

void RoadblockState::hydrate(uint16_t saved_permissions)
{
    if (definition_path_.empty()) {
        return;
    }
    set_permissions(saved_permissions);
}

uint16_t RoadblockState::permissions() const { return permissions_; }
uint16_t RoadblockState::configurable_permissions() const { return configurable_permissions_; }

int RoadblockState::has_permission(int permission) const
{
    if (permission == 0) {
        return 1;
    }
    const uint16_t mask = mask_for(permission);
    return mask && (permissions_ & mask) != 0;
}

int RoadblockState::can_configure(int permission) const
{
    const uint16_t mask = mask_for(permission);
    return mask && (configurable_permissions_ & mask) != 0;
}

int RoadblockState::toggle_permission(int permission)
{
    const uint16_t mask = mask_for(permission);
    if (!mask || !(configurable_permissions_ & mask)) {
        return 0;
    }
    permissions_ ^= mask;
    return 1;
}

void RoadblockState::set_permissions(uint16_t permissions)
{
    permissions_ = static_cast<uint16_t>((default_permissions_ & ~configurable_permissions_) |
        (permissions & configurable_permissions_));
}

void RoadblockState::accept_none()
{
    set_permissions(0);
}

void RoadblockState::accept_all()
{
    set_permissions(std::numeric_limits<uint16_t>::max());
}

} // namespace building_type_registry_impl
