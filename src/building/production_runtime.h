#pragma once

#include "building/production.h"

#include <cstddef>

namespace production_runtime_impl {

Production *get_or_create(const Building &building, size_t method_index);
Production *get_or_create_primary(const Building &building);
size_t get_method_count(const Building &building);

}
