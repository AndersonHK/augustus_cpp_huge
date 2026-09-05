#include "graphics/runtime_overlay_images.h"

#include "graphics/image.h"

#include "core/log.h"
#include "graphics/color.h"
#include "graphics/renderer.h"

#include <array>
#include <cmath>
#include <cstring>

namespace {

constexpr int kOverlayWidth = FOOTPRINT_WIDTH;
constexpr int kOverlayHeight = FOOTPRINT_HEIGHT;
constexpr const char *kFootprintPayloadKey = "Graphics/RuntimeOverlay/Footprint/Base";
Image *g_footprint_image = nullptr;

void generate_footprint_pixels(std::array<color_t, kOverlayWidth * kOverlayHeight> &pixels)
{
    pixels.fill(ALPHA_TRANSPARENT);

    // All placement, range and cursor overlays share neutral pixels; only the
    // caller's ARGB color controls their tint and opacity.
    constexpr float center_x = (kOverlayWidth - 1) / 2.0f;
    constexpr float center_y = (kOverlayHeight - 1) / 2.0f;
    constexpr float half_width = kOverlayWidth / 2.0f;
    constexpr float half_height = kOverlayHeight / 2.0f;

    for (int y = 0; y < kOverlayHeight; y++) {
        for (int x = 0; x < kOverlayWidth; x++) {
            const float distance =
                std::fabs((x - center_x) / half_width) +
                std::fabs((y - center_y) / half_height);
            if (distance <= 1.0f) {
                pixels[y * kOverlayWidth + x] = COLOR_WHITE;
            }
        }
    }
}

int upload_overlay(const color_t *pixels, int width, int height)
{
    image_manager().release(kFootprintPayloadKey);

    image metadata = {};
    metadata.width = width;
    metadata.height = height;
    metadata.original.width = width;
    metadata.original.height = height;
    metadata.is_isometric = 1;

    g_footprint_image = image_manager().load_pixels(kFootprintPayloadKey, metadata, pixels, width, height);
    if (!g_footprint_image) {
        log_error("Runtime overlay image upload failed", kFootprintPayloadKey, 0);
        return 0;
    }
    return 1;
}

} // namespace

int runtime_overlay_images_init_or_reload(void)
{
    std::array<color_t, kOverlayWidth * kOverlayHeight> pixels;
    generate_footprint_pixels(pixels);
    return upload_overlay(pixels.data(), kOverlayWidth, kOverlayHeight);
}

void runtime_overlay_images_reset(void)
{
    image_manager().release(kFootprintPayloadKey);
    g_footprint_image = nullptr;
}

const Image *runtime_footprint_overlay_image()
{
    return g_footprint_image;
}
