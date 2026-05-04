#include "map/road_service_history.h"

#include "core/crash_context.h"

extern "C" {
#include "map/grid.h"
}

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <vector>

namespace {

constexpr uint32_t kSaveFormatVersion = 1;
constexpr size_t kSaveHeaderSize = 2 * sizeof(uint32_t);
constexpr size_t kEffectGridCells = GRID_SIZE * GRID_SIZE;
constexpr uint32_t kLastPreReligionEffectCount = ROAD_SERVICE_EFFECT_RELIGION_CERES;
constexpr uint32_t kLastPreEntertainmentEffectCount = ROAD_SERVICE_EFFECT_ENTERTAINMENT_THEATER;
constexpr uint32_t kLastPreMarketEffectCount = ROAD_SERVICE_EFFECT_MARKET_GOODS;
constexpr uint32_t kNeverVisitedStamp = 0;
std::array<grid_u32, ROAD_SERVICE_EFFECT_MAX> g_history;
uint32_t g_last_visit_stamp = 0;

bool is_valid_effect(road_service_effect effect)
{
    return effect > ROAD_SERVICE_EFFECT_NONE && effect < ROAD_SERVICE_EFFECT_MAX;
}

void normalize_visit_stamps()
{
    std::vector<uint32_t> stamps;
    stamps.reserve(kEffectGridCells);
    for (int effect = 1; effect < ROAD_SERVICE_EFFECT_MAX; effect++) {
        const uint32_t *items = g_history[effect].items;
        const uint32_t *end = items + kEffectGridCells;
        for (const uint32_t *item = items; item != end; ++item) {
            if (*item != kNeverVisitedStamp) {
                stamps.push_back(*item);
            }
        }
    }

    if (stamps.empty()) {
        g_last_visit_stamp = 0;
        return;
    }

    std::sort(stamps.begin(), stamps.end());
    stamps.erase(std::unique(stamps.begin(), stamps.end()), stamps.end());

    for (int effect = 1; effect < ROAD_SERVICE_EFFECT_MAX; effect++) {
        uint32_t *items = g_history[effect].items;
        uint32_t *end = items + kEffectGridCells;
        for (uint32_t *item = items; item != end; ++item) {
            if (*item != kNeverVisitedStamp) {
                *item = static_cast<uint32_t>(
                    std::lower_bound(stamps.begin(), stamps.end(), *item) - stamps.begin() + 1);
            }
        }
    }
    g_last_visit_stamp = static_cast<uint32_t>(stamps.size());
}

uint32_t next_visit_stamp()
{
    // Zero means "never visited" and must stay older than every visited tile.
    // Positive values only need relative recency; compact before overflow.
    if (g_last_visit_stamp == std::numeric_limits<uint32_t>::max()) {
        normalize_visit_stamps();
    }
    return ++g_last_visit_stamp;
}

void update_last_visit_stamp_from_history(road_service_effect effect)
{
    if (!is_valid_effect(effect)) {
        return;
    }
    const uint32_t *items = g_history[effect].items;
    const uint32_t *end = items + kEffectGridCells;
    g_last_visit_stamp = std::max(g_last_visit_stamp, *std::max_element(items, end));
}

} // namespace

extern "C" void map_road_service_history_clear(void)
{
    for (int effect = 0; effect < ROAD_SERVICE_EFFECT_MAX; effect++) {
        map_grid_clear_u32(g_history[effect].items);
    }
    g_last_visit_stamp = 0;
}

extern "C" uint32_t map_road_service_history_get(road_service_effect effect, int grid_offset)
{
    if (!is_valid_effect(effect) || !map_grid_is_valid_offset(grid_offset)) {
        return 0;
    }
    return g_history[effect].items[grid_offset];
}

extern "C" void map_road_service_history_record(road_service_effect effect, int grid_offset)
{
    if (!is_valid_effect(effect) || !map_grid_is_valid_offset(grid_offset)) {
        return;
    }
    g_history[effect].items[grid_offset] = next_visit_stamp();
}

extern "C" void map_road_service_history_save_state(buffer *buf)
{
    // The payload is ordinal by road_service_effect. Append new effects only;
    // keep removed meanings as reserved ids so old saves do not shift columns.
    const size_t payload_size =
        kSaveHeaderSize +
        (static_cast<size_t>(ROAD_SERVICE_EFFECT_MAX) - 1) * kEffectGridCells * sizeof(uint32_t);
    buffer_init_dynamic(buf, payload_size);
    buffer_write_u32(buf, kSaveFormatVersion);
    buffer_write_u32(buf, static_cast<uint32_t>(ROAD_SERVICE_EFFECT_MAX));
    for (int effect = 1; effect < ROAD_SERVICE_EFFECT_MAX; effect++) {
        map_grid_save_state_u32(g_history[effect].items, buf);
    }
}

extern "C" void map_road_service_history_load_state(
    buffer *buf,
    int has_saved_state,
    int has_religion_effects,
    int has_entertainment_effects,
    int has_market_effects)
{
    map_road_service_history_clear();

    if (!has_saved_state || !buf || !buf->data || !buf->size) {
        const ErrorContextScope scope("Savegame road service history");
        error_context_report_info(
            "Savegame has no road service history; smart walker pathing history will start at zero.",
            "This is expected when loading saves created before road service history was added.");
        return;
    }

    const size_t payload_size = buffer_load_dynamic(buf);
    if (payload_size < kSaveHeaderSize) {
        const ErrorContextScope scope("Savegame road service history");
        error_context_report_warning("Invalid road service history; resetting it to zero.", 0);
        return;
    }

    const uint32_t format_version = buffer_read_u32(buf);
    const uint32_t effect_count = buffer_read_u32(buf);
    if (format_version != kSaveFormatVersion ||
        effect_count <= static_cast<uint32_t>(ROAD_SERVICE_EFFECT_NONE)) {
        const ErrorContextScope scope("Savegame road service history");
        error_context_report_warning("Unsupported road service history; resetting it to zero.", 0);
        return;
    }

    const uint32_t max_effect_count = has_market_effects ?
        static_cast<uint32_t>(ROAD_SERVICE_EFFECT_MAX) :
        (has_entertainment_effects ? kLastPreMarketEffectCount :
            (has_religion_effects ? kLastPreEntertainmentEffectCount : kLastPreReligionEffectCount));
    const int effects_to_read = static_cast<int>(std::min(effect_count, max_effect_count));
    for (int effect = 1; effect < effects_to_read; effect++) {
        map_grid_load_state_u32(g_history[effect].items, buf);
        update_last_visit_stamp_from_history(static_cast<road_service_effect>(effect));
    }

    // Older saves contain only the effect grids known to that save version. Any
    // appended religion, entertainment, or market grids stay zeroed by the initial clear.
    // This also consumes future grids so the fixed ordinal payload stays aligned.
    for (uint32_t effect = effects_to_read; effect < effect_count; effect++) {
        for (size_t i = 0; i < kEffectGridCells; i++) {
            buffer_read_u32(buf);
        }
    }
}
