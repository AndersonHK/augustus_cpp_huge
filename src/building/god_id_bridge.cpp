#include "building/god_id_bridge.h"

#include "building/god_registry.h"
#include "city/god.h"

extern "C" {
#include "core/log.h"
}

#include <array>
#include <cstdint>
#include <limits>
#include <string>
#include <unordered_map>
#include <vector>

namespace {

constexpr int SAVE_TABLE_VERSION = 1;
constexpr int kInvalidGodRuntimeId = -1;
constexpr int kLegacyGodCount = GOD_ALL;

struct BridgeState {
    std::unordered_map<std::string, int> text_to_runtime;
    std::vector<std::string> runtime_to_text;
    std::vector<uint16_t> runtime_to_save;
    std::vector<int> save_to_runtime;
    std::vector<std::string> save_to_text;
    bool runtime_ready = false;
    bool save_table_ready = false;
};

BridgeState g_bridge;

const char *legacy_text_from_god(god_type legacy_type)
{
    switch (legacy_type) {
        case GOD_CERES:
            return "ceres";
        case GOD_NEPTUNE:
            return "neptune";
        case GOD_MERCURY:
            return "mercury";
        case GOD_MARS:
            return "mars";
        case GOD_VENUS:
            return "venus";
        case GOD_ALL:
        default:
            return nullptr;
    }
}

void clear_save_table()
{
    g_bridge.runtime_to_save.assign(g_bridge.runtime_to_text.size(), 0);
    g_bridge.save_to_runtime.clear();
    g_bridge.save_to_text.clear();
}

void ensure_runtime_table()
{
    if (g_bridge.runtime_ready) {
        return;
    }

    g_bridge.text_to_runtime.clear();
    g_bridge.runtime_to_text.clear();
    g_bridge.runtime_to_save.clear();

    const int count = building_type_registry_impl::god_definition_count();
    g_bridge.runtime_to_text.resize(static_cast<size_t>(count));
    g_bridge.runtime_to_save.resize(static_cast<size_t>(count), 0);
    for (int runtime_id = 0; runtime_id < count; ++runtime_id) {
        const God *definition = building_type_registry_impl::god_definition_at_runtime_index(runtime_id);
        if (!definition || !definition->path() || !*definition->path()) {
            continue;
        }
        g_bridge.runtime_to_text[static_cast<size_t>(runtime_id)] = definition->path();
        g_bridge.text_to_runtime[definition->path()] = runtime_id;
    }

    g_bridge.runtime_ready = true;
}

void append_save_id_mapping(uint16_t save_id, int runtime_id, const char *text_id)
{
    const size_t index = static_cast<size_t>(save_id);
    if (g_bridge.save_to_runtime.size() <= index) {
        g_bridge.save_to_runtime.resize(index + 1, kInvalidGodRuntimeId);
        g_bridge.save_to_text.resize(index + 1);
    }
    g_bridge.save_to_runtime[index] = runtime_id;
    g_bridge.save_to_text[index] = text_id ? text_id : "";
    if (runtime_id >= 0 && runtime_id < static_cast<int>(g_bridge.runtime_to_save.size())) {
        g_bridge.runtime_to_save[static_cast<size_t>(runtime_id)] = save_id;
    }
}

void load_legacy_save_table()
{
    ensure_runtime_table();
    clear_save_table();

    for (int save_id = 0; save_id < kLegacyGodCount; ++save_id) {
        const char *text_id = legacy_text_from_god(static_cast<god_type>(save_id));
        append_save_id_mapping(static_cast<uint16_t>(save_id), god_id_bridge_runtime_from_text(text_id), text_id);
    }

    g_bridge.save_table_ready = true;
}

void ensure_save_table()
{
    if (!g_bridge.save_table_ready) {
        god_id_bridge_prepare_new_save_table();
    }
}

} // namespace

extern "C" void god_id_bridge_reset_for_runtime(void)
{
    g_bridge.runtime_ready = false;
    g_bridge.save_table_ready = false;
    ensure_runtime_table();
    clear_save_table();
}

extern "C" void god_id_bridge_clear_save_table(void)
{
    clear_save_table();
    g_bridge.save_table_ready = false;
}

extern "C" const char *god_id_bridge_text_from_runtime(int runtime_id)
{
    ensure_runtime_table();
    if (runtime_id < 0 || runtime_id >= static_cast<int>(g_bridge.runtime_to_text.size()) ||
        g_bridge.runtime_to_text[static_cast<size_t>(runtime_id)].empty()) {
        return nullptr;
    }
    return g_bridge.runtime_to_text[static_cast<size_t>(runtime_id)].c_str();
}

extern "C" const char *god_id_bridge_text_from_legacy(god_type legacy_type)
{
    return legacy_text_from_god(legacy_type);
}

extern "C" int god_id_bridge_runtime_from_text(const char *text_id)
{
    ensure_runtime_table();
    if (!text_id || !*text_id) {
        return kInvalidGodRuntimeId;
    }
    const auto found = g_bridge.text_to_runtime.find(text_id);
    return found == g_bridge.text_to_runtime.end() ? kInvalidGodRuntimeId : found->second;
}

extern "C" int god_id_bridge_runtime_from_legacy(god_type legacy_type)
{
    return god_id_bridge_runtime_from_text(legacy_text_from_god(legacy_type));
}

extern "C" void god_id_bridge_prepare_new_save_table(void)
{
    ensure_runtime_table();
    clear_save_table();

    for (int runtime_id = 0; runtime_id < static_cast<int>(g_bridge.runtime_to_text.size()); ++runtime_id) {
        const std::string &text_id = g_bridge.runtime_to_text[static_cast<size_t>(runtime_id)];
        if (text_id.empty()) {
            continue;
        }
        uint16_t save_id = static_cast<uint16_t>(g_bridge.save_to_runtime.size());
        append_save_id_mapping(save_id, runtime_id, text_id.c_str());
    }
    g_bridge.save_table_ready = true;
}

extern "C" void god_id_bridge_save_table_save_state(buffer *buf)
{
    if (!buf) {
        return;
    }

    ensure_save_table();

    size_t payload_size = sizeof(uint32_t) * 2;
    for (size_t save_id = 0; save_id < g_bridge.save_to_text.size(); ++save_id) {
        const std::string &text_id = g_bridge.save_to_text[save_id];
        if (text_id.empty()) {
            continue;
        }
        payload_size += sizeof(uint16_t) * 2 + text_id.size();
    }

    buffer_init_dynamic(buf, payload_size);
    buffer_write_u32(buf, SAVE_TABLE_VERSION);
    buffer_write_u32(buf, static_cast<uint32_t>(g_bridge.save_to_text.size()));

    for (size_t save_id = 0; save_id < g_bridge.save_to_text.size(); ++save_id) {
        const std::string &text_id = g_bridge.save_to_text[save_id];
        if (text_id.empty()) {
            continue;
        }
        if (save_id > std::numeric_limits<uint16_t>::max() ||
            text_id.size() > std::numeric_limits<uint16_t>::max()) {
            log_error("God text id too long for save table", text_id.c_str(), static_cast<int>(text_id.size()));
            continue;
        }
        buffer_write_u16(buf, static_cast<uint16_t>(save_id));
        buffer_write_u16(buf, static_cast<uint16_t>(text_id.size()));
        buffer_write_raw(buf, text_id.c_str(), text_id.size());
    }
}

extern "C" void god_id_bridge_save_table_load_state(buffer *buf, int has_save_table)
{
    if (!has_save_table || !buf || !buf->size) {
        load_legacy_save_table();
        return;
    }

    ensure_runtime_table();
    clear_save_table();

    buffer table = *buf;
    if (buffer_load_dynamic(&table) < sizeof(uint32_t) * 2) {
        log_error("God save table is invalid; falling back to legacy ids", 0, 0);
        load_legacy_save_table();
        return;
    }

    uint32_t version = buffer_read_u32(&table);
    uint32_t count = buffer_read_u32(&table);
    if (version != SAVE_TABLE_VERSION) {
        log_error("Unsupported god save table version", 0, static_cast<int>(version));
        load_legacy_save_table();
        return;
    }

    for (uint32_t i = 0; i < count && !buffer_at_end(&table); ++i) {
        uint16_t save_id = buffer_read_u16(&table);
        uint16_t text_length = buffer_read_u16(&table);
        std::string text_id(text_length, '\0');
        if (text_length) {
            buffer_read_raw(&table, &text_id[0], text_length);
        }
        int runtime_id = god_id_bridge_runtime_from_text(text_id.c_str());
        if (runtime_id < 0 && !text_id.empty()) {
            log_error("God referenced by save is not available in active mod", text_id.c_str(), save_id);
        }
        append_save_id_mapping(save_id, runtime_id, text_id.c_str());
    }

    g_bridge.save_table_ready = true;
}

extern "C" uint16_t god_id_bridge_save_id_from_runtime(int runtime_id)
{
    ensure_save_table();
    if (runtime_id < 0 || runtime_id >= static_cast<int>(g_bridge.runtime_to_save.size())) {
        return 0;
    }
    return g_bridge.runtime_to_save[static_cast<size_t>(runtime_id)];
}

extern "C" int god_id_bridge_runtime_from_save_id(uint16_t save_id)
{
    ensure_save_table();
    if (static_cast<size_t>(save_id) < g_bridge.save_to_runtime.size()) {
        return g_bridge.save_to_runtime[static_cast<size_t>(save_id)];
    }
    return kInvalidGodRuntimeId;
}
