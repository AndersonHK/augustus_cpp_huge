#pragma once

#include <stdint.h>

#include <string>

namespace building_type_registry_impl {

// Mutable walker permissions owned by a building's foundation runtime state.
// Permission values are bit indices; index zero means an unmanaged walker.
class RoadblockState {
public:
    void configure(const char *definition_path, uint16_t default_permissions, uint16_t configurable_permissions);
    void hydrate(uint16_t saved_permissions);

    uint16_t permissions() const;
    uint16_t configurable_permissions() const;
    int has_permission(int permission) const;
    int can_configure(int permission) const;
    int toggle_permission(int permission);
    void set_permissions(uint16_t permissions);
    void accept_none();
    void accept_all();

private:
    static uint16_t mask_for(int permission);

    uint16_t permissions_ = 0;
    uint16_t default_permissions_ = 0;
    uint16_t configurable_permissions_ = 0;
    std::string definition_path_;
};

} // namespace building_type_registry_impl
