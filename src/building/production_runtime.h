#pragma once

#include "building/production.h"

#include <cstddef>
#include <memory>
#include <vector>

namespace production_runtime_impl {

extern std::vector<std::vector<std::unique_ptr<Production>>> g_city_productions;

void reset();
void initialize_city();
Production *get_or_create(Building building, size_t method_index);
Production *get_or_create_primary(Building building);
size_t get_method_count(Building building);

}
