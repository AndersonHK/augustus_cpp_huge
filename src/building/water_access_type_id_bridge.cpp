#include "building/building_record.h"
#include "building/water_access_type_id_bridge.h"

#include "building/water_access_type.h"

extern "C" {
#include "core/log.h"
}

#include <array>
#include <cstdint>
#include <limits>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace {

constexpr int SAVE_TABLE_VERSION = 1;
constexpr int kMaxWaterAccessTypes = 8;
constexpr int kInvalidRuntimeId = -1;

struct BridgeState {
    std::unordered_map<std::string, uint8_t> text_to_runtime;
    std::array<std::string, kMaxWaterAccessTypes> runtime_to_text;
    std::array<uint8_t, kMaxWaterAccessTypes> runtime_to_save = {};
    std::vector<int> save_to_runtime;
    std::vector<std::string> save_to_text;
    bool runtime_ready = false;
    bool save_table_ready = false;
};

BridgeState g_bridge;

void register_text_id(uint8_t runtime_id, const std::string &text_id)
{
    if (runtime_id >= kMaxWaterAccessTypes || text_id.empty()) {
        return;
    }
    g_bridge.runtime_to_text[runtime_id] = text_id;
    g_bridge.text_to_runtime[text_id] = runtime_id;
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

    for (const std::unique_ptr<building_type_registry_impl::WaterAccessType> &definition :
         building_type_registry_impl::water_access_types()) {
        if (!definition) {
            continue;
        }
        register_text_id(definition->number_id(), definition->text_id());
    }
    g_bridge.runtime_ready = true;
}

void clear_save_table()
{
    g_bridge.runtime_to_save.fill(0);
    g_bridge.save_to_runtime.clear();
    g_bridge.save_to_text.clear();
    g_bridge.save_to_runtime.push_back(kInvalidRuntimeId);
    g_bridge.save_to_text.push_back("");
}

const char *legacy_text_from_raw_save_id(uint8_t save_id)
{
    switch (save_id) {
        case 1:
            return "well";
        case 2:
            return "fountain";
        case 3:
            return "reservoir";
        case 4:
            return "aqueduct";
        case 5:
            return "latrines";
        default:
            return nullptr;
    }
}

void load_legacy_save_table()
{
    ensure_runtime_table();
    clear_save_table();

    for (uint8_t save_id = 1; save_id <= 5; save_id++) {
        const char *text_id = legacy_text_from_raw_save_id(save_id);
        int runtime_id = water_access_type_id_bridge_runtime_from_text(text_id);
        if (g_bridge.save_to_runtime.size() <= save_id) {
            g_bridge.save_to_runtime.resize(static_cast<size_t>(save_id) + 1, kInvalidRuntimeId);
            g_bridge.save_to_text.resize(static_cast<size_t>(save_id) + 1);
        }
        g_bridge.save_to_runtime[save_id] = runtime_id;
        g_bridge.save_to_text[save_id] = text_id ? text_id : "";
        if (runtime_id >= 0 && runtime_id < kMaxWaterAccessTypes) {
            g_bridge.runtime_to_save[static_cast<uint8_t>(runtime_id)] = save_id;
        }
    }

    g_bridge.save_table_ready = true;
}

void ensure_save_table()
{
    if (!g_bridge.save_table_ready) {
        water_access_type_id_bridge_prepare_new_save_table();
    }
}

} // namespace

extern "C" void water_access_type_id_bridge_reset_for_runtime(void)
{
    g_bridge.runtime_ready = false;
    g_bridge.save_table_ready = false;
    ensure_runtime_table();
}

extern "C" const char *water_access_type_id_bridge_text_from_runtime(int runtime_id)
{
    ensure_runtime_table();
    if (runtime_id < 0 || runtime_id >= kMaxWaterAccessTypes ||
        g_bridge.runtime_to_text[static_cast<uint8_t>(runtime_id)].empty()) {
        return nullptr;
    }
    return g_bridge.runtime_to_text[static_cast<uint8_t>(runtime_id)].c_str();
}

extern "C" int water_access_type_id_bridge_runtime_from_text(const char *text_id)
{
    ensure_runtime_table();
    if (!text_id || !*text_id) {
        return kInvalidRuntimeId;
    }
    const auto found = g_bridge.text_to_runtime.find(text_id);
    return found == g_bridge.text_to_runtime.end() ? kInvalidRuntimeId : found->second;
}

extern "C" void water_access_type_id_bridge_prepare_new_save_table(void)
{
    ensure_runtime_table();
    clear_save_table();

    for (uint8_t runtime_id = 0; runtime_id < kMaxWaterAccessTypes; runtime_id++) {
        if (g_bridge.runtime_to_text[runtime_id].empty()) {
            continue;
        }
        uint8_t save_id = static_cast<uint8_t>(g_bridge.save_to_runtime.size());
        g_bridge.save_to_runtime.push_back(runtime_id);
        g_bridge.save_to_text.push_back(g_bridge.runtime_to_text[runtime_id]);
        g_bridge.runtime_to_save[runtime_id] = save_id;
    }
    g_bridge.save_table_ready = true;
}

extern "C" void water_access_type_id_bridge_save_table_save_state(buffer *buf)
{
    if (!buf) {
        return;
    }

    ensure_save_table();

    size_t payload_size = sizeof(uint32_t) * 2;
    for (size_t save_id = 1; save_id < g_bridge.save_to_runtime.size(); save_id++) {
        payload_size += sizeof(uint8_t) + sizeof(uint16_t) + g_bridge.save_to_text[save_id].size();
    }

    buffer_init_dynamic(buf, payload_size);
    buffer_write_u32(buf, SAVE_TABLE_VERSION);
    buffer_write_u32(buf, static_cast<uint32_t>(g_bridge.save_to_runtime.size() - 1));

    for (size_t save_id = 1; save_id < g_bridge.save_to_runtime.size(); save_id++) {
        const std::string &text_id = g_bridge.save_to_text[save_id];
        if (text_id.size() > std::numeric_limits<uint16_t>::max()) {
            log_error("Water access type text id too long for save table", text_id.c_str(), static_cast<int>(text_id.size()));
            continue;
        }
        buffer_write_u8(buf, static_cast<uint8_t>(save_id));
        buffer_write_u16(buf, static_cast<uint16_t>(text_id.size()));
        buffer_write_raw(buf, text_id.c_str(), text_id.size());
    }
}

extern "C" void water_access_type_id_bridge_save_table_load_state(buffer *buf, int has_save_table)
{
    if (!has_save_table || !buf || !buf->size) {
        load_legacy_save_table();
        return;
    }

    ensure_runtime_table();
    clear_save_table();

    buffer table = *buf;
    if (buffer_load_dynamic(&table) < sizeof(uint32_t) * 2) {
        log_error("Water access type save table is invalid; falling back to legacy ids", 0, 0);
        load_legacy_save_table();
        return;
    }

    uint32_t version = buffer_read_u32(&table);
    uint32_t count = buffer_read_u32(&table);
    if (version != SAVE_TABLE_VERSION) {
        log_error("Unsupported water access type save table version", 0, static_cast<int>(version));
        load_legacy_save_table();
        return;
    }

    for (uint32_t i = 0; i < count && !buffer_at_end(&table); i++) {
        uint8_t save_id = buffer_read_u8(&table);
        uint16_t text_length = buffer_read_u16(&table);
        std::string text_id(text_length, '\0');
        if (text_length) {
            buffer_read_raw(&table, &text_id[0], text_length);
        }

        if (g_bridge.save_to_runtime.size() <= save_id) {
            g_bridge.save_to_runtime.resize(static_cast<size_t>(save_id) + 1, kInvalidRuntimeId);
            g_bridge.save_to_text.resize(static_cast<size_t>(save_id) + 1);
        }
        int runtime_id = water_access_type_id_bridge_runtime_from_text(text_id.c_str());
        if (runtime_id < 0 && !text_id.empty()) {
            log_error("Water access type referenced by save is not available in active mod", text_id.c_str(), save_id);
        }
        g_bridge.save_to_runtime[save_id] = runtime_id;
        g_bridge.save_to_text[save_id] = text_id;
        if (runtime_id >= 0 && runtime_id < kMaxWaterAccessTypes) {
            g_bridge.runtime_to_save[static_cast<uint8_t>(runtime_id)] = save_id;
        }
    }
    g_bridge.save_table_ready = true;
}

extern "C" uint8_t water_access_type_id_bridge_save_id_from_runtime(int runtime_id)
{
    ensure_save_table();
    if (runtime_id < 0 || runtime_id >= kMaxWaterAccessTypes) {
        return 0;
    }
    return g_bridge.runtime_to_save[static_cast<uint8_t>(runtime_id)];
}

extern "C" int water_access_type_id_bridge_runtime_from_save_id(uint8_t save_id)
{
    ensure_save_table();
    if (static_cast<size_t>(save_id) < g_bridge.save_to_runtime.size()) {
        return g_bridge.save_to_runtime[save_id];
    }
    return water_access_type_id_bridge_runtime_from_text(legacy_text_from_raw_save_id(save_id));
}
