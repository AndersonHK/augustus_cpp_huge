#pragma once

#include "core/image.h"

#include <cstdint>

namespace startup_parser {

// Stable pixel-content identity supplied by the headless validation renderer.
// This lets tests compare independently materialized references to the same image.
std::uint64_t image_resource_fingerprint(image_handle handle);

}
