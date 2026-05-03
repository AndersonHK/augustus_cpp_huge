#include "building/building_type_id_bridge.h"

#include "building/building_type_legacy_migration.h"
#include "building/building_type_registry_internal.h"

extern "C" {
#include "core/log.h"
}

#include <array>
#include <cctype>
#include <cstdint>
#include <cstring>
#include <limits>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace {

constexpr int SAVE_TABLE_VERSION = 1;

struct BridgeState {
    std::unordered_map<std::string, uint16_t> text_to_runtime;
    std::array<std::string, BUILDING_TYPE_MAX> runtime_to_text;
    std::array<uint16_t, BUILDING_TYPE_MAX> runtime_to_save = {};
    std::vector<building_type> save_to_runtime;
    std::vector<uint8_t> save_id_missing;
    bool runtime_ready = false;
    bool save_table_ready = false;
};

BridgeState g_bridge;

std::string trim_copy(const std::string &text)
{
    size_t first = 0;
    while (first < text.size() && std::isspace(static_cast<unsigned char>(text[first]))) {
        ++first;
    }
    size_t last = text.size();
    while (last > first && std::isspace(static_cast<unsigned char>(text[last - 1]))) {
        --last;
    }
    return text.substr(first, last - first);
}

void register_text_id(building_type runtime_id, const std::string &text_id)
{
    if (runtime_id <= BUILDING_NONE || runtime_id >= BUILDING_TYPE_MAX || text_id.empty()) {
        return;
    }

    uint16_t runtime = static_cast<uint16_t>(runtime_id);
    if (g_bridge.runtime_to_text[runtime].empty()) {
        g_bridge.runtime_to_text[runtime] = text_id;
    }
    g_bridge.text_to_runtime[text_id] = runtime;
}

void register_attr_tokens(building_type runtime_id, const char *attr)
{
    if (!attr || !*attr) {
        return;
    }

    std::string list(attr);
    size_t start = 0;
    while (start <= list.size()) {
        size_t end = list.find('|', start);
        std::string token = trim_copy(list.substr(start, end == std::string::npos ? std::string::npos : end - start));
        register_text_id(runtime_id, token);
        if (end == std::string::npos) {
            break;
        }
        start = end + 1;
    }
}

void ensure_runtime_table()
{
    if (g_bridge.runtime_ready) {
        return;
    }

    g_bridge.text_to_runtime.clear();
    for (std::string &text_id : g_bridge.runtime_to_text) {
        text_id.clear();
    }

    for (uint16_t id = 1; id < BUILDING_TYPE_MAX; ++id) {
        const char *legacy_text_id = building_type_legacy_migration_text_id_for_enum(id);
        register_text_id(static_cast<building_type>(id), legacy_text_id ? legacy_text_id : "");
    }

    for (const auto &definition : building_type_registry_impl::g_building_types) {
        if (!definition) {
            continue;
        }
        register_attr_tokens(definition->type(), definition->attr());
    }

    g_bridge.runtime_ready = true;
}

void clear_save_table()
{
    g_bridge.runtime_to_save.fill(0);
    g_bridge.save_to_runtime.clear();
    g_bridge.save_id_missing.clear();
    g_bridge.save_to_runtime.push_back(BUILDING_NONE);
    g_bridge.save_id_missing.push_back(0);
}

void ensure_save_table()
{
    if (g_bridge.save_table_ready) {
        return;
    }

    building_type_id_bridge_prepare_new_save_table();
}

void append_save_id_mapping(uint16_t save_id, building_type runtime_id, bool missing)
{
    size_t index = static_cast<size_t>(save_id);
    if (g_bridge.save_to_runtime.size() <= index) {
        g_bridge.save_to_runtime.resize(index + 1, BUILDING_NONE);
        g_bridge.save_id_missing.resize(index + 1, 0);
    }
    g_bridge.save_to_runtime[index] = runtime_id;
    g_bridge.save_id_missing[index] = missing ? 1 : 0;
    if (!missing && runtime_id > BUILDING_NONE && runtime_id < BUILDING_TYPE_MAX) {
        g_bridge.runtime_to_save[static_cast<uint16_t>(runtime_id)] = save_id;
    }
}

void load_legacy_save_table()
{
    ensure_runtime_table();
    clear_save_table();

    for (uint16_t legacy_id = 1; legacy_id < BUILDING_TYPE_MAX; ++legacy_id) {
        const char *text_id = building_type_legacy_migration_text_id_for_enum(legacy_id);
        building_type runtime_id = building_type_id_bridge_runtime_from_text(text_id);
        append_save_id_mapping(legacy_id, runtime_id, runtime_id == BUILDING_NONE && text_id && *text_id);
    }

    g_bridge.save_table_ready = true;
}

}

extern "C" void building_type_id_bridge_reset_for_runtime(void)
{
    g_bridge.runtime_ready = false;
    g_bridge.save_table_ready = false;
    ensure_runtime_table();
}

extern "C" const char *building_type_id_bridge_text_from_runtime(building_type runtime_id)
{
    ensure_runtime_table();
    if (runtime_id <= BUILDING_NONE || runtime_id >= BUILDING_TYPE_MAX ||
        g_bridge.runtime_to_text[static_cast<uint16_t>(runtime_id)].empty()) {
        return 0;
    }
    return g_bridge.runtime_to_text[static_cast<uint16_t>(runtime_id)].c_str();
}

extern "C" building_type building_type_id_bridge_runtime_from_text(const char *text_id)
{
    ensure_runtime_table();
    if (!text_id || !*text_id) {
        return BUILDING_NONE;
    }

    auto found = g_bridge.text_to_runtime.find(text_id);
    if (found == g_bridge.text_to_runtime.end()) {
        return BUILDING_NONE;
    }
    return static_cast<building_type>(found->second);
}

extern "C" void building_type_id_bridge_prepare_new_save_table(void)
{
    ensure_runtime_table();
    clear_save_table();

    for (uint16_t runtime_id = 1; runtime_id < BUILDING_TYPE_MAX; ++runtime_id) {
        if (g_bridge.runtime_to_text[runtime_id].empty()) {
            continue;
        }
        uint16_t save_id = static_cast<uint16_t>(g_bridge.save_to_runtime.size());
        append_save_id_mapping(save_id, static_cast<building_type>(runtime_id), false);
    }

    g_bridge.save_table_ready = true;
}

extern "C" void building_type_id_bridge_save_table_save_state(buffer *buf)
{
    if (!buf) {
        return;
    }

    ensure_save_table();

    size_t payload_size = sizeof(uint32_t) * 2;
    for (size_t save_id = 1; save_id < g_bridge.save_to_runtime.size(); ++save_id) {
        building_type runtime_id = g_bridge.save_to_runtime[save_id];
        const char *text_id = building_type_id_bridge_text_from_runtime(runtime_id);
        if (!text_id || !*text_id) {
            continue;
        }
        payload_size += sizeof(uint16_t) * 2 + std::strlen(text_id);
    }

    buffer_init_dynamic(buf, payload_size);
    buffer_write_u32(buf, SAVE_TABLE_VERSION);
    buffer_write_u32(buf, static_cast<uint32_t>(g_bridge.save_to_runtime.size() - 1));

    for (size_t save_id = 1; save_id < g_bridge.save_to_runtime.size(); ++save_id) {
        building_type runtime_id = g_bridge.save_to_runtime[save_id];
        const char *text_id = building_type_id_bridge_text_from_runtime(runtime_id);
        if (!text_id || !*text_id) {
            continue;
        }
        size_t text_length = std::strlen(text_id);
        if (text_length > std::numeric_limits<uint16_t>::max()) {
            log_error("Building type text id too long for save table", text_id, static_cast<int>(text_length));
            continue;
        }
        buffer_write_u16(buf, static_cast<uint16_t>(save_id));
        buffer_write_u16(buf, static_cast<uint16_t>(text_length));
        buffer_write_raw(buf, text_id, text_length);
    }
}

extern "C" void building_type_id_bridge_save_table_load_state(buffer *buf, int has_save_table)
{
    if (!has_save_table || !buf || !buf->size) {
        load_legacy_save_table();
        return;
    }

    ensure_runtime_table();
    clear_save_table();

    buffer table = *buf;
    if (buffer_load_dynamic(&table) < sizeof(uint32_t) * 2) {
        log_error("Building type save table is invalid; falling back to legacy enum migration", 0, 0);
        load_legacy_save_table();
        return;
    }

    uint32_t version = buffer_read_u32(&table);
    uint32_t count = buffer_read_u32(&table);
    if (version != SAVE_TABLE_VERSION) {
        log_error("Unsupported building type save table version", 0, static_cast<int>(version));
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

        building_type runtime_id = building_type_id_bridge_runtime_from_text(text_id.c_str());
        bool missing = runtime_id == BUILDING_NONE && !text_id.empty();
        if (missing) {
            log_error("Building type referenced by save is not available in active mod", text_id.c_str(), save_id);
        }
        append_save_id_mapping(save_id, runtime_id, missing);
    }

    g_bridge.save_table_ready = true;
}

extern "C" uint16_t building_type_id_bridge_save_id_from_runtime(building_type runtime_id)
{
    ensure_save_table();
    if (runtime_id <= BUILDING_NONE || runtime_id >= BUILDING_TYPE_MAX) {
        return 0;
    }
    return g_bridge.runtime_to_save[static_cast<uint16_t>(runtime_id)];
}

extern "C" building_type building_type_id_bridge_runtime_from_save_id(uint16_t save_id)
{
    ensure_save_table();
    if (static_cast<size_t>(save_id) >= g_bridge.save_to_runtime.size()) {
        return BUILDING_NONE;
    }
    return g_bridge.save_to_runtime[static_cast<size_t>(save_id)];
}

extern "C" int building_type_id_bridge_save_id_is_missing(uint16_t save_id)
{
    ensure_save_table();
    if (save_id == 0) {
        return 0;
    }
    if (static_cast<size_t>(save_id) >= g_bridge.save_id_missing.size()) {
        return 1;
    }
    return g_bridge.save_id_missing[static_cast<size_t>(save_id)] ? 1 : 0;
}
