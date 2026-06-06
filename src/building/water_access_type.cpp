#include "building/building_record.h"
#include "building/water_access_type.h"

#include "core/crash_context.h"
#include "core/xml_value.h"

extern "C" {
#include "core/dir.h"
#include "core/file.h"
#include "core/log.h"
#include "core/xml_parser.h"
#include "game/mod_manager.h"
}

#include <array>
#include <cstdio>
#include <cstring>
#include <string>
#include <utility>
#include <vector>

namespace building_type_registry_impl {

namespace {

constexpr int kMaxWaterAccessTypes = 8;

std::string g_water_access_type_path;
std::vector<std::unique_ptr<WaterAccessType>> g_water_access_types;
std::array<const WaterAccessType *, kMaxWaterAccessTypes> g_water_access_by_number = {};
uint8_t g_defined_mask = 0;

struct ParseState {
    std::unique_ptr<WaterAccessType> definition;
    int error = 0;
};

ParseState g_parse_state;

int compare_text(const char *left, const char *right)
{
    if (!left || !right) {
        return left == right ? 0 : (left ? 1 : -1);
    }
    while (*left && *right && *left == *right) {
        ++left;
        ++right;
    }
    return static_cast<unsigned char>(*left) - static_cast<unsigned char>(*right);
}

int load_file_to_buffer(const char *filename, std::vector<char> &buffer)
{
    FILE *fp = file_open(filename, "rb");
    if (!fp) {
        log_error("Unable to open WaterAccessType xml", filename, 0);
        return 0;
    }

    if (fseek(fp, 0, SEEK_END) != 0) {
        file_close(fp);
        log_error("Unable to seek WaterAccessType xml", filename, 0);
        return 0;
    }

    long size = ftell(fp);
    if (size < 0) {
        file_close(fp);
        log_error("Unable to size WaterAccessType xml", filename, 0);
        return 0;
    }
    rewind(fp);

    buffer.resize(static_cast<size_t>(size));
    size_t read = fread(buffer.data(), 1, buffer.size(), fp);
    file_close(fp);
    if (read != buffer.size()) {
        log_error("Unable to read WaterAccessType xml", filename, 0);
        return 0;
    }
    return 1;
}

int parse_root()
{
    if (!xml_parser_has_attribute("text_id") || !xml_parser_has_attribute("number_id")) {
        log_error("WaterAccessType xml is missing required attributes", 0, 0);
        g_parse_state.error = 1;
        return 0;
    }

    const char *text_id = xml_parser_get_attribute_string("text_id");
    std::string normalized_text_id = xml_value::trim_copy(text_id ? text_id : "");
    int number_id = -1;
    if (normalized_text_id.empty() ||
        !xml_value::parse_int_strict(xml_parser_get_attribute_string("number_id"), &number_id) ||
        number_id < 0 || number_id >= kMaxWaterAccessTypes) {
        log_error("Unsupported WaterAccessType id", text_id, number_id);
        g_parse_state.error = 1;
        return 0;
    }

    g_parse_state.definition = std::make_unique<WaterAccessType>(
        std::move(normalized_text_id),
        static_cast<uint8_t>(number_id));
    return 1;
}

const xml_parser_element XML_ELEMENTS[] = {
    { "water_access_type", parse_root, nullptr, nullptr, nullptr }
};

int parse_definition_file(const char *filename)
{
    ErrorContextScope error_scope("water_access_type_registry.parse_definition", filename);

    std::vector<char> buffer;
    if (!load_file_to_buffer(filename, buffer)) {
        return 0;
    }

    g_parse_state = {};
    if (!xml_parser_init(XML_ELEMENTS, static_cast<int>(sizeof(XML_ELEMENTS) / sizeof(XML_ELEMENTS[0])), 1)) {
        log_error("Unable to initialize WaterAccessType xml parser", filename, 0);
        return 0;
    }

    int parsed = xml_parser_parse(buffer.data(), static_cast<unsigned int>(buffer.size()), 1);
    xml_parser_free();
    if (!parsed || g_parse_state.error || !g_parse_state.definition) {
        log_error("Unable to parse WaterAccessType xml", filename, 0);
        return 0;
    }

    const WaterAccessType *duplicate_number = water_access_type_from_number_id(g_parse_state.definition->number_id());
    if (duplicate_number) {
        char detail[256];
        snprintf(
            detail,
            sizeof(detail),
            "number_id=%u first=%s second=%s",
            static_cast<unsigned int>(g_parse_state.definition->number_id()),
            duplicate_number->text_id(),
            g_parse_state.definition->text_id());
        log_error("Duplicate WaterAccessType number_id", detail, 0);
        return 0;
    }
    if (find_water_access_type(g_parse_state.definition->text_id())) {
        log_error("Duplicate WaterAccessType text_id", g_parse_state.definition->text_id(), 0);
        return 0;
    }
    if (g_water_access_types.size() >= kMaxWaterAccessTypes) {
        log_error("Too many WaterAccessType definitions", filename, kMaxWaterAccessTypes);
        return 0;
    }

    const uint8_t number_id = g_parse_state.definition->number_id();
    const uint8_t mask = g_parse_state.definition->mask();
    g_water_access_types.push_back(std::move(g_parse_state.definition));
    g_water_access_by_number[number_id] = g_water_access_types.back().get();
    g_defined_mask |= mask;
    return 1;
}

} // namespace

WaterAccessType::WaterAccessType(std::string text_id, uint8_t number_id)
    : text_id_(std::move(text_id))
    , number_id_(number_id)
    , mask_(static_cast<uint8_t>(1u << number_id))
{
}

const char *WaterAccessType::text_id() const
{
    return text_id_.c_str();
}

uint8_t WaterAccessType::number_id() const
{
    return number_id_;
}

uint8_t WaterAccessType::mask() const
{
    return mask_;
}

const WaterAccessType *find_water_access_type(const char *text_id)
{
    if (!text_id || !*text_id) {
        return nullptr;
    }
    for (const std::unique_ptr<WaterAccessType> &definition : g_water_access_types) {
        if (definition && compare_text(definition->text_id(), text_id) == 0) {
            return definition.get();
        }
    }
    return nullptr;
}

const WaterAccessType *water_access_type_from_number_id(uint8_t number_id)
{
    if (number_id >= kMaxWaterAccessTypes) {
        return nullptr;
    }
    return g_water_access_by_number[number_id];
}

uint8_t water_access_mask_from_text(const char *text_id)
{
    const WaterAccessType *definition = find_water_access_type(text_id);
    return definition ? definition->mask() : 0;
}

uint8_t water_access_defined_mask(void)
{
    return g_defined_mask;
}

const std::vector<std::unique_ptr<WaterAccessType>> &water_access_types(void)
{
    return g_water_access_types;
}

} // namespace building_type_registry_impl

extern "C" int water_access_type_registry_load(void)
{
    using namespace building_type_registry_impl;

    g_water_access_type_path = std::string(mod_manager_get_mod_path()) + "WaterAccessType/";
    g_water_access_types.clear();
    g_water_access_by_number.fill(nullptr);
    g_defined_mask = 0;

    const dir_listing *files = dir_find_files_with_extension(g_water_access_type_path.c_str(), "xml");
    if (!files || files->num_files <= 0) {
        log_error("No WaterAccessType xml files found in", g_water_access_type_path.c_str(), 0);
        return 0;
    }

    for (int i = 0; i < files->num_files; i++) {
        char full_path[FILE_NAME_MAX];
        snprintf(full_path, FILE_NAME_MAX, "%s%s", g_water_access_type_path.c_str(), files->files[i].name);
        if (!parse_definition_file(full_path)) {
            log_error("Unable to parse WaterAccessType xml", full_path, 0);
            return 0;
        }
    }
    return 1;
}

extern "C" const char *water_access_type_text_from_number_id(uint8_t number_id)
{
    const building_type_registry_impl::WaterAccessType *definition =
        building_type_registry_impl::water_access_type_from_number_id(number_id);
    return definition ? definition->text_id() : nullptr;
}

extern "C" uint8_t water_access_type_mask_from_text_id(const char *text_id)
{
    return building_type_registry_impl::water_access_mask_from_text(text_id);
}

extern "C" uint8_t water_access_type_defined_mask_c(void)
{
    return building_type_registry_impl::water_access_defined_mask();
}
