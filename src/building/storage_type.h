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

private:
    std::string path_;
    std::vector<resource_type> resources_;
    int capacity_ = 0;
    StorageRole role_ = StorageRole::None;
};

} // namespace building_type_registry_impl
