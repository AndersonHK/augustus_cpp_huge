#pragma once

#include "game/resource.h"

#include <string>
#include <vector>

namespace building_type_registry_impl {

enum class StorageRole {
    None,
    Input,
    Output
};

class StorageType {
public:
    explicit StorageType(std::string path);

    const char *path() const;

    void add_resource(resource_type resource);
    const std::vector<resource_type> &resources() const;
    int handles_resource(resource_type resource) const;

    void set_capacity(int capacity);
    int capacity() const;

    void set_role(StorageRole role);
    StorageRole role() const;
    int is_input() const;
    int is_output() const;
    void set_respect_orders(bool value) { respect_orders_ = value; }
    bool respect_orders() const { return respect_orders_; }

private:
    std::string path_;
    std::vector<resource_type> resources_;
    int capacity_ = 0;
    StorageRole role_ = StorageRole::None;
    bool respect_orders_ = false;
};

} // namespace building_type_registry_impl
