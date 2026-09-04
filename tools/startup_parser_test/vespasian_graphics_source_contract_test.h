#pragma once

#include <filesystem>
#include <iosfwd>

bool validate_vespasian_graphics_source_contract(const std::filesystem::path &graphics_root, std::ostream &errors);
