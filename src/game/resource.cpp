#include "building/building_type.h"
#include "building/industry.h"
#include "translation/translation.h"
#include "core/xml_value.h"
#include "game/mod_manager.h"
#include "game/resource_graphics.h"

#include "resource.h"

#include "building/production_method_registry.h"

#ifdef __cplusplus
extern "C" {
#endif

#include "core/crash_context.h"
#include "core/dir.h"
#include "core/file.h"
#include "core/log.h"
#include "core/xml_parser.h"
#include "game/save_version.h"
#include "scenario/allowed_building.h"
#include "scenario/property.h"

#ifdef __cplusplus
}
#endif

#include "game/resource_id_bridge.h"

#include <array>
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#define RESOURCE_ALL RESOURCE_SLOT_COUNT

static resource_data resource_info_defaults[RESOURCE_ALL];

static resource_data resource_info[RESOURCE_ALL];

static std::array<std::string, RESOURCE_ALL> resource_text_id_storage;
static std::array<std::string, RESOURCE_ALL> resource_name_key_storage;
static std::array<std::string, RESOURCE_ALL> resource_xml_attr_storage;
static std::array<int, RESOURCE_ALL> resource_declared_slots;
static std::unordered_map<std::string, resource_type> resource_text_id_lookup;
static std::vector<resource_type> loaded_resources;
static std::vector<resource_type> production_resources;

struct ResourceParseState {
    resource_type type = RESOURCE_NONE;
    resource_data data = {};
    std::array<ImageGroupEntryRef, 4> storage_images = {};
    ImageGroupEntryRef cart_single_load;
    ImageGroupEntryRef cart_multiple_loads;
    ImageGroupEntryRef cart_eight_loads;
    ImageGroupEntryRef panel_icon;
    ImageGroupEntryRef empire_icon;
    ImageGroupEntryRef editor_icon;
    ImageGroupEntryRef editor_empire_icon;
    std::string text_id;
    std::string xml_attr_name;
    int saw_root = 0;
    int error = 0;
};

static ResourceParseState resource_parse_state;

static int text_equals(const char *left, const char *right)
{
    return left && right && std::strcmp(left, right) == 0;
}

static int parse_required_int_attribute(const char *attribute_name, int *out_value)
{
    if (!xml_parser_has_attribute(attribute_name)) {
        log_error("Resource xml is missing required integer attribute", attribute_name, 0);
        resource_parse_state.error = 1;
        return 0;
    }
    const char *text = xml_parser_get_attribute_string(attribute_name);
    if (!xml_value::parse_int_strict(text ? text : "", out_value)) {
        log_error("Resource xml has invalid integer attribute", attribute_name, 0);
        resource_parse_state.error = 1;
        return 0;
    }
    return 1;
}

static int parse_optional_int_attribute(const char *attribute_name, int *out_value, int default_value)
{
    if (!xml_parser_has_attribute(attribute_name)) {
        *out_value = default_value;
        return 1;
    }
    const char *text = xml_parser_get_attribute_string(attribute_name);
    if (!xml_value::parse_int_strict(text ? text : "", out_value)) {
        log_error("Resource xml has invalid integer attribute", attribute_name, 0);
        resource_parse_state.error = 1;
        return 0;
    }
    return 1;
}

static int parse_flags_attribute(resource_flags *out_flags)
{
    *out_flags = RESOURCE_FLAG_NONE;
    if (!xml_parser_has_attribute("flags")) {
        return 1;
    }

    std::string flags_text = xml_parser_get_attribute_string("flags");
    size_t start = 0;
    while (start <= flags_text.size()) {
        const size_t end = flags_text.find(',', start);
        const std::string token = xml_value::trim_copy(
            flags_text.substr(start, end == std::string::npos ? std::string::npos : end - start));
        if (token == "food") {
            *out_flags = static_cast<resource_flags>(*out_flags | RESOURCE_FLAG_FOOD);
        } else if (token == "storable") {
            *out_flags = static_cast<resource_flags>(*out_flags | RESOURCE_FLAG_STORABLE);
        } else if (token == "inventory") {
            *out_flags = static_cast<resource_flags>(*out_flags | RESOURCE_FLAG_INVENTORY);
        } else if (token == "special") {
            *out_flags = static_cast<resource_flags>(*out_flags | RESOURCE_FLAG_SPECIAL);
        } else if (!token.empty() && token != "none") {
            log_error("Unsupported Resource flag", token.c_str(), 0);
            resource_parse_state.error = 1;
            return 0;
        }
        if (end == std::string::npos) {
            break;
        }
        start = end + 1;
    }
    return 1;
}

static ImageGroupEntryRef parse_image_ref()
{
    if (!xml_parser_has_attribute("path")) {
        log_error("Resource graphics node is missing required path", xml_parser_get_current_element_name(), 0);
        resource_parse_state.error = 1;
        return ImageGroupEntryRef();
    }
    const char *path = xml_parser_get_attribute_string("path");
    const char *image = xml_parser_has_attribute("image") ? xml_parser_get_attribute_string("image") : "";
    return ImageGroupEntryRef::from_group(path ? path : "", image ? image : "");
}

static int parse_resource_root()
{
    if (resource_parse_state.saw_root) {
        log_error("Duplicate Resource root node", 0, 0);
        resource_parse_state.error = 1;
        return 0;
    }
    int slot = RESOURCE_NONE;
    if (!parse_required_int_attribute("slot", &slot) || slot < RESOURCE_NONE || slot >= RESOURCE_ALL) {
        log_error("Resource xml has unsupported slot", 0, slot);
        resource_parse_state.error = 1;
        return 0;
    }
    if (!xml_parser_has_attribute("id")) {
        log_error("Resource xml is missing required attribute 'id'", 0, 0);
        resource_parse_state.error = 1;
        return 0;
    }
    if (!xml_parser_has_attribute("name_key")) {
        log_error("Resource xml is missing required attribute 'name_key'", 0, 0);
        resource_parse_state.error = 1;
        return 0;
    }

    const std::string text_id = xml_value::trim_copy(xml_parser_get_attribute_string("id"));
    if (text_id.empty()) {
        log_error("Resource xml has empty id", 0, slot);
        resource_parse_state.error = 1;
        return 0;
    }
    if (resource_text_id_lookup.find(text_id) != resource_text_id_lookup.end()) {
        log_error("Duplicate Resource id", text_id.c_str(), slot);
        resource_parse_state.error = 1;
        return 0;
    }

    const char *name_key = xml_parser_get_attribute_string("name_key");
    const uint8_t *text = lang_get_string_by_key(name_key);
    if (!text) {
        log_error("Resource xml has unsupported name_key", name_key, 0);
        resource_parse_state.error = 1;
        return 0;
    }

    const resource_type type = static_cast<resource_type>(slot);
    if (resource_declared_slots[static_cast<size_t>(type)]) {
        log_error("Duplicate Resource slot", text_id.c_str(), slot);
        resource_parse_state.error = 1;
        return 0;
    }

    resource_parse_state.saw_root = 1;
    resource_parse_state.type = type;
    resource_parse_state.data = {};
    resource_parse_state.data.type = type;
    resource_parse_state.data.text = text;
    resource_parse_state.text_id = text_id;
    resource_parse_state.xml_attr_name.clear();
    resource_name_key_storage[static_cast<size_t>(type)] = xml_value::trim_copy(name_key);
    resource_parse_state.data.name_key = resource_name_key_storage[static_cast<size_t>(type)].c_str();
    resource_parse_state.storage_images = {};
    resource_parse_state.cart_single_load = ImageGroupEntryRef();
    resource_parse_state.cart_multiple_loads = ImageGroupEntryRef();
    resource_parse_state.cart_eight_loads = ImageGroupEntryRef();
    resource_parse_state.panel_icon = ImageGroupEntryRef();
    resource_parse_state.empire_icon = ImageGroupEntryRef();
    resource_parse_state.editor_icon = ImageGroupEntryRef();
    resource_parse_state.editor_empire_icon = ImageGroupEntryRef();
    return 1;
}

static int parse_resource_model()
{
    if (xml_parser_has_attribute("xml_attr")) {
        resource_parse_state.xml_attr_name = xml_value::trim_copy(xml_parser_get_attribute_string("xml_attr"));
    }

    resource_flags flags = RESOURCE_FLAG_NONE;
    if (!parse_flags_attribute(&flags)) {
        return 0;
    }
    resource_parse_state.data.flags = flags;

    return 1;
}

static int parse_resource_trade()
{
    return parse_optional_int_attribute("buy", &resource_parse_state.data.default_trade_price.buy, 0) &&
        parse_optional_int_attribute("sell", &resource_parse_state.data.default_trade_price.sell, 0);
}

static int parse_resource_cart_graphic()
{
    if (!xml_parser_has_attribute("load")) {
        log_error("Resource cart graphic is missing required load", 0, 0);
        resource_parse_state.error = 1;
        return 0;
    }

    const char *load = xml_parser_get_attribute_string("load");
    ImageGroupEntryRef ref = parse_image_ref();
    if (text_equals(load, "single")) {
        resource_parse_state.cart_single_load = std::move(ref);
    } else if (text_equals(load, "multiple")) {
        resource_parse_state.cart_multiple_loads = std::move(ref);
    } else if (text_equals(load, "eight")) {
        resource_parse_state.cart_eight_loads = std::move(ref);
    } else {
        log_error("Unsupported Resource cart load", load, 0);
        resource_parse_state.error = 1;
        return 0;
    }
    return 1;
}

static int parse_resource_storage_graphic()
{
    int load = 0;
    if (!parse_required_int_attribute("load", &load) || load < 1 || load > 4) {
        log_error("Resource storage graphic has unsupported load", 0, load);
        resource_parse_state.error = 1;
        return 0;
    }

    resource_parse_state.storage_images[static_cast<size_t>(load - 1)] = parse_image_ref();
    return !resource_parse_state.error;
}

static int parse_resource_panel_icon()
{
    resource_parse_state.panel_icon = parse_image_ref();
    return !resource_parse_state.error;
}

static int parse_resource_empire_icon()
{
    resource_parse_state.empire_icon = parse_image_ref();
    return !resource_parse_state.error;
}

static int parse_resource_editor_icon()
{
    resource_parse_state.editor_icon = parse_image_ref();
    return !resource_parse_state.error;
}

static int parse_resource_editor_empire_icon()
{
    resource_parse_state.editor_empire_icon = parse_image_ref();
    return !resource_parse_state.error;
}

static void finish_resource_root()
{
    const resource_type type = resource_parse_state.type;
    resource_text_id_storage[static_cast<size_t>(type)] = resource_parse_state.text_id;
    resource_parse_state.data.text_id = resource_text_id_storage[static_cast<size_t>(type)].c_str();
    resource_xml_attr_storage[static_cast<size_t>(type)] = resource_parse_state.xml_attr_name;
    resource_parse_state.data.xml_attr_name =
        resource_xml_attr_storage[static_cast<size_t>(type)].empty() ?
            nullptr :
            resource_xml_attr_storage[static_cast<size_t>(type)].c_str();
    resource_info_defaults[type] = resource_parse_state.data;
    resource_declared_slots[static_cast<size_t>(type)] = 1;
    resource_text_id_lookup[resource_text_id_storage[static_cast<size_t>(type)]] = type;
    loaded_resources.push_back(type);

    ResourceGraphics &graphics = mutable_resource_graphics(type);
    graphics.set_storage_images(resource_parse_state.storage_images);
    graphics.set_cart_images(
        resource_parse_state.cart_single_load,
        resource_parse_state.cart_multiple_loads,
        resource_parse_state.cart_eight_loads);
    graphics.set_panel_icon(resource_parse_state.panel_icon);
    graphics.set_empire_icon(resource_parse_state.empire_icon);
    graphics.set_editor_icon(resource_parse_state.editor_icon);
    graphics.set_editor_empire_icon(resource_parse_state.editor_empire_icon);
}

static const xml_parser_element RESOURCE_XML_ELEMENTS[] = {
    { "resource", parse_resource_root, finish_resource_root, nullptr, nullptr },
    { "model", parse_resource_model, nullptr, "resource", nullptr },
    { "trade", parse_resource_trade, nullptr, "resource", nullptr },
    { "graphics", nullptr, nullptr, "resource", nullptr },
    { "cart", parse_resource_cart_graphic, nullptr, "graphics", nullptr },
    { "storage", parse_resource_storage_graphic, nullptr, "graphics", nullptr },
    { "panel_icon", parse_resource_panel_icon, nullptr, "graphics", nullptr },
    { "empire_icon", parse_resource_empire_icon, nullptr, "graphics", nullptr },
    { "editor_icon", parse_resource_editor_icon, nullptr, "graphics", nullptr },
    { "editor_empire_icon", parse_resource_editor_empire_icon, nullptr, "graphics", nullptr },
};

static int load_file_to_buffer(const char *filename, std::vector<char> &buffer)
{
    FILE *fp = file_open(filename, "rb");
    if (!fp) {
        log_error("Unable to open Resource xml", filename, 0);
        return 0;
    }

    if (fseek(fp, 0, SEEK_END) != 0) {
        file_close(fp);
        log_error("Unable to seek Resource xml", filename, 0);
        return 0;
    }

    const long size = ftell(fp);
    if (size < 0) {
        file_close(fp);
        log_error("Unable to size Resource xml", filename, 0);
        return 0;
    }
    rewind(fp);

    buffer.resize(static_cast<size_t>(size));
    const size_t read = fread(buffer.data(), 1, buffer.size(), fp);
    file_close(fp);
    if (read != buffer.size()) {
        log_error("Unable to read Resource xml", filename, 0);
        return 0;
    }
    return 1;
}

static int parse_resource_definition_file(const char *filename)
{
    const ErrorContextScope error_scope("Resource XML", filename);

    std::vector<char> buffer;
    if (!load_file_to_buffer(filename, buffer)) {
        return 0;
    }

    resource_parse_state = {};
    if (!xml_parser_init(RESOURCE_XML_ELEMENTS, static_cast<int>(sizeof(RESOURCE_XML_ELEMENTS) / sizeof(RESOURCE_XML_ELEMENTS[0])), 1)) {
        log_error("Unable to initialize Resource xml parser", filename, 0);
        return 0;
    }

    const int parsed = xml_parser_parse(buffer.data(), static_cast<unsigned int>(buffer.size()), 1);
    xml_parser_free();
    if (!parsed || resource_parse_state.error || !resource_parse_state.saw_root) {
        log_error("Unable to parse Resource xml", filename, 0);
        return 0;
    }
    return 1;
}

static int load_resource_definitions()
{
    std::memset(resource_info_defaults, 0, sizeof(resource_info_defaults));
    for (std::string &text_id : resource_text_id_storage) {
        text_id.clear();
    }
    for (std::string &name_key : resource_name_key_storage) {
        name_key.clear();
    }
    for (std::string &xml_attr : resource_xml_attr_storage) {
        xml_attr.clear();
    }
    resource_declared_slots = {};
    resource_text_id_lookup.clear();
    loaded_resources.clear();
    production_resources.clear();

    const std::string resource_path = std::string(mod_manager_get_mod_path()) + "Resources/";
    const dir_listing *files = dir_find_files_with_extension(resource_path.c_str(), "xml");
    if (!files || files->num_files <= 0) {
        log_error("No Resource xml files found in", resource_path.c_str(), 0);
        error_context_report_error("No Resource xml files found.", resource_path.c_str());
        return 0;
    }

    for (int i = 0; i < files->num_files; i++) {
        char full_path[FILE_NAME_MAX];
        snprintf(full_path, FILE_NAME_MAX, "%s%s", resource_path.c_str(), files->files[i].name);
        if (!parse_resource_definition_file(full_path)) {
            error_context_report_error("Unable to parse Resource xml.", full_path);
            return 0;
        }
    }
    std::sort(loaded_resources.begin(), loaded_resources.end());
    loaded_resources.erase(std::unique(loaded_resources.begin(), loaded_resources.end()), loaded_resources.end());
    for (resource_type resource : loaded_resources) {
        if (resource != RESOURCE_NONE &&
            (resource_info_defaults[resource].flags & RESOURCE_FLAG_SPECIAL) != RESOURCE_FLAG_SPECIAL) {
            production_resources.push_back(resource);
        }
    }
    return 1;
}

int resource_is_food(resource_type resource)
{
    resource_data *data = resource_get_data(resource);
    return data && (data->flags & RESOURCE_FLAG_FOOD) == RESOURCE_FLAG_FOOD;
}

int resource_is_raw_material(resource_type resource)
{
    return resource != RESOURCE_NONE && !resource_is_food(resource) &&
        resource_get_supply_chain_for_good(0, resource) == 0;
}

int resource_is_inventory(resource_type resource)
{
    resource_data *data = resource_get_data(resource);
    return data && (data->flags & RESOURCE_FLAG_INVENTORY) == RESOURCE_FLAG_INVENTORY;
}

int resource_is_storable(resource_type resource)
{
    resource_data *data = resource_get_data(resource);
    return data && (data->flags & RESOURCE_FLAG_STORABLE) == RESOURCE_FLAG_STORABLE;
}

int resource_is_special(resource_type resource)
{
    resource_data *data = resource_get_data(resource);
    return data && (data->flags & RESOURCE_FLAG_SPECIAL) == RESOURCE_FLAG_SPECIAL;
}

int resource_is_declared(resource_type resource)
{
    return resource >= RESOURCE_NONE &&
        resource < RESOURCE_ALL &&
        resource_declared_slots[static_cast<size_t>(resource)];
}

int resource_is_tradeable(resource_type resource)
{
    return resource != RESOURCE_NONE && resource_is_declared(resource) && !resource_is_special(resource);
}

int resource_get_supply_chain_for_good(resource_supply_chain *chain, resource_type good)
{
    if (!resource_is_declared(good)) {
        return 0;
    }
    return production_method_registry_supply_chain_for_good(chain, good, RESOURCE_SUPPLY_CHAIN_MAX_SIZE);
}

int resource_get_supply_chain_for_raw_material(resource_supply_chain *chain, resource_type raw_material)
{
    if (!resource_is_declared(raw_material)) {
        return 0;
    }
    return production_method_registry_supply_chain_for_raw_material(chain, raw_material, RESOURCE_SUPPLY_CHAIN_MAX_SIZE);
}

void resource_init(void)
{
    resource_graphics_reset();
    production_method_registry_reset_production_overrides();
    load_resource_definitions();
    std::memcpy(resource_info, resource_info_defaults, sizeof(resource_info_defaults));
    resource_id_bridge_reset_for_runtime();
}

resource_data *resource_get_data(resource_type resource)
{
    if (!resource_is_declared(resource)) {
        return 0;
    }
    return &resource_info[resource];
}

const char *resource_text_id(resource_type resource)
{
    resource_data *data = resource_get_data(resource);
    return data ? data->text_id : 0;
}

resource_type resource_type_from_text_id(const char *text_id)
{
    if (!text_id || !*text_id) {
        return RESOURCE_NONE;
    }
    const auto found = resource_text_id_lookup.find(text_id);
    return found == resource_text_id_lookup.end() ? RESOURCE_NONE : found->second;
}

void resource_save_write_ref(buffer *buf, resource_type resource)
{
    buffer_write_u16(buf, resource_id_bridge_save_id_from_runtime(resource));
}

resource_type resource_save_read_ref(buffer *buf)
{
    return resource_remap(buffer_read_u16(buf));
}

int resource_matches_text_id(resource_type resource, const char *text_id)
{
    const char *resource_id = resource_text_id(resource);
    return resource_id && text_id && std::strcmp(resource_id, text_id) == 0;
}

resource_type resource_get_loaded(int index)
{
    if (index < 0 || index >= static_cast<int>(loaded_resources.size())) {
        return RESOURCE_NONE;
    }
    return loaded_resources[static_cast<size_t>(index)];
}

int resource_loaded_count(void)
{
    return static_cast<int>(loaded_resources.size());
}

resource_type resource_get_production(int index)
{
    if (index < 0 || index >= static_cast<int>(production_resources.size())) {
        return RESOURCE_NONE;
    }
    return production_resources[static_cast<size_t>(index)];
}

int resource_production_count(void)
{
    return static_cast<int>(production_resources.size());
}

int resource_units_per_load(void)
{
    return 100;
}

resource_type resource_wheat(void) { return resource_type_from_text_id("wheat"); }
resource_type resource_vegetables(void) { return resource_type_from_text_id("vegetables"); }
resource_type resource_fruit(void) { return resource_type_from_text_id("fruit"); }
resource_type resource_meat(void) { return resource_type_from_text_id("meat"); }
resource_type resource_fish(void) { return resource_type_from_text_id("fish"); }
resource_type resource_clay(void) { return resource_type_from_text_id("clay"); }
resource_type resource_timber(void) { return resource_type_from_text_id("timber"); }
resource_type resource_olives(void) { return resource_type_from_text_id("olives"); }
resource_type resource_vines(void) { return resource_type_from_text_id("vines"); }
resource_type resource_iron(void) { return resource_type_from_text_id("iron"); }
resource_type resource_marble(void) { return resource_type_from_text_id("marble"); }
resource_type resource_gold(void) { return resource_type_from_text_id("gold"); }
resource_type resource_sand(void) { return resource_type_from_text_id("sand"); }
resource_type resource_stone(void) { return resource_type_from_text_id("stone"); }
resource_type resource_pottery(void) { return resource_type_from_text_id("pottery"); }
resource_type resource_furniture(void) { return resource_type_from_text_id("furniture"); }
resource_type resource_oil(void) { return resource_type_from_text_id("oil"); }
resource_type resource_wine(void) { return resource_type_from_text_id("wine"); }
resource_type resource_weapons(void) { return resource_type_from_text_id("weapons"); }
resource_type resource_concrete(void) { return resource_type_from_text_id("concrete"); }
resource_type resource_bricks(void) { return resource_type_from_text_id("bricks"); }
resource_type resource_denarii(void) { return resource_type_from_text_id("denarii"); }
resource_type resource_troops(void) { return resource_type_from_text_id("troops"); }

resource_type resource_type_from_xml_attr(const char *name)
{
    if (!name || !*name) {
        return RESOURCE_NONE;
    }

    const std::string normalized_name = xml_value::trim_copy(name);
    if (normalized_name.empty()) {
        return RESOURCE_NONE;
    }

    for (resource_type type : loaded_resources) {
        resource_data *data = resource_get_data(type);
        if (!data || !data->xml_attr_name) {
            continue;
        }
        if (xml_parser_compare_multiple(data->xml_attr_name, normalized_name.c_str())) {
            return type;
        }
    }
    return RESOURCE_NONE;
}

void production_rates_save(buffer *buf)
{
    int buf_size = sizeof(uint16_t) * resource_production_count();
    uint8_t *buf_data = static_cast<uint8_t *>(std::malloc(buf_size));
    buffer_init(buf, buf_data, buf_size);
    
    for (int i = 0; i < resource_production_count(); i++) {
        resource_type resource = resource_get_production(i);
        const int production = production_method_registry_production_per_month_for_resource(resource);
        buffer_write_u16(buf, std::clamp(production, 0, 0xffff));
    }
}

void production_rates_load(buffer *buf)
{
    for (int i = 0; i < resource_production_count(); i++) {
        resource_type resource = resource_get_production(i);
        production_method_registry_set_production_per_month_for_resource(resource, buffer_read_u16(buf));
    }
}
