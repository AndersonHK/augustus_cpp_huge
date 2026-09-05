#include "building/building_type.h"
#include "building/BuildingGraphicsDef.h"
#include "building/industry.h"
#include "figure/figure_type_registry_internal.h"
#include "translation/translation.h"
#include "core/xml_value.h"
#include "game/ResourceGraphics.h"

#include "resource.h"

#include "building/production_method_registry.h"

#include "core/crash_context.h"
#include "core/log.h"
#include "core/xml_definition.h"
#include "core/xml_parser.h"
#include "game/mod_definition_loader.h"
#include "game/save_version.h"
#include "scenario/allowed_building.h"
#include "scenario/property.h"


#include "game/resource_id_bridge.h"

#include <array>
#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <optional>
#include <sstream>
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
static mod_definition::DefinitionOverlayTracker resource_definition_overlays;
static std::string resource_failure_reason;

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
    std::string name_key;
    std::string xml_attr_name;
    int disabled = 0;
    int saw_root = 0;
    int error = 0;
};

static ResourceParseState resource_parse_state;

struct StagedResourceDefinition {
    ResourceParseState definition;
    mod_definition::DefinitionSource source;
};

struct ResourceIdentityReservation {
    resource_type slot = RESOURCE_NONE;
    std::string text_id;
    mod_definition::DefinitionSource source;
};

struct StagedResourceDefinitions {
    std::array<std::optional<StagedResourceDefinition>, RESOURCE_ALL> winners;
    std::array<std::optional<ResourceIdentityReservation>, RESOURCE_ALL> slots;
    std::unordered_map<std::string, ResourceIdentityReservation> ids;
    mod_definition::DefinitionOverlayTracker overlays;
    std::string failure_reason;
};

static std::string source_path(const mod_definition::DefinitionSource &source)
{
    return source.full_path.empty() ? source.describe() : source.full_path;
}

static int stage_resource_definition(
    StagedResourceDefinitions &staged,
    ResourceParseState definition,
    const mod_definition::DefinitionSource &source)
{
    const resource_type slot = definition.type;
    const std::string &text_id = definition.text_id;
    const std::optional<ResourceIdentityReservation> &slot_identity =
        staged.slots[static_cast<std::size_t>(slot)];
    if (slot_identity && slot_identity->text_id != text_id) {
        std::ostringstream failure;
        failure << "Resource slot " << slot << " is already owned by id '" << slot_identity->text_id
            << "' from " << source_path(slot_identity->source) << "; id '" << text_id
            << "' in " << source_path(source) << " cannot replace it.";
        staged.failure_reason = failure.str();
        return 0;
    }

    const auto id_identity = staged.ids.find(text_id);
    if (id_identity != staged.ids.end() && id_identity->second.slot != slot) {
        std::ostringstream failure;
        failure << "Resource id '" << text_id << "' is already assigned to slot " << id_identity->second.slot
            << " by " << source_path(id_identity->second.source) << "; " << source_path(source)
            << " cannot move it to slot " << slot << ".";
        staged.failure_reason = failure.str();
        return 0;
    }

    mod_definition::DefinitionOverlayChange change;
    if (!staged.overlays.apply(text_id, definition.disabled != 0, source, &change)) {
        staged.failure_reason = staged.overlays.failure_reason();
        return 0;
    }

    if (!slot_identity) {
        ResourceIdentityReservation identity{slot, text_id, source};
        staged.slots[static_cast<std::size_t>(slot)] = identity;
        staged.ids.emplace(text_id, std::move(identity));
    }
    staged.winners[static_cast<std::size_t>(slot)] = StagedResourceDefinition{
        std::move(definition), source};
    return 1;
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

static int parse_enabled_content(const char *element)
{
    if (!resource_parse_state.saw_root || resource_parse_state.disabled) {
        log_error("Disabled Resource definition must contain only its root identity", element, 0);
        resource_parse_state.error = 1;
        return 0;
    }
    return 1;
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
    const std::string text_id = xml_value::trim_copy(xml_parser_get_attribute_string("id"));
    if (text_id.empty()) {
        log_error("Resource xml has empty id", 0, slot);
        resource_parse_state.error = 1;
        return 0;
    }
    const resource_type type = static_cast<resource_type>(slot);
    int disabled = 0;
    if (xml_parser_has_attribute("disabled") &&
        !xml_value::parse_bool(xml_parser_get_attribute_string("disabled"), &disabled)) {
        log_error("Resource xml has invalid Boolean attribute 'disabled'", text_id.c_str(), slot);
        resource_parse_state.error = 1;
        return 0;
    }

    resource_parse_state.saw_root = 1;
    resource_parse_state.type = type;
    resource_parse_state.disabled = disabled;
    resource_parse_state.data = {};
    resource_parse_state.data.type = type;
    resource_parse_state.text_id = text_id;
    resource_parse_state.name_key.clear();
    resource_parse_state.xml_attr_name.clear();
    resource_parse_state.storage_images = {};
    resource_parse_state.cart_single_load = ImageGroupEntryRef();
    resource_parse_state.cart_multiple_loads = ImageGroupEntryRef();
    resource_parse_state.cart_eight_loads = ImageGroupEntryRef();
    resource_parse_state.panel_icon = ImageGroupEntryRef();
    resource_parse_state.empire_icon = ImageGroupEntryRef();
    resource_parse_state.editor_icon = ImageGroupEntryRef();
    resource_parse_state.editor_empire_icon = ImageGroupEntryRef();

    if (disabled) {
        if (xml_parser_has_attribute("name_key")) {
            log_error("Disabled Resource definition must not declare name_key", text_id.c_str(), slot);
            resource_parse_state.error = 1;
            return 0;
        }
        return 1;
    }

    if (!xml_parser_has_attribute("name_key")) {
        log_error("Resource xml is missing required attribute 'name_key'", text_id.c_str(), slot);
        resource_parse_state.error = 1;
        return 0;
    }
    resource_parse_state.name_key = xml_value::trim_copy(xml_parser_get_attribute_string("name_key"));
    if (resource_parse_state.name_key.empty()) {
        log_error("Resource xml has empty name_key", text_id.c_str(), slot);
        resource_parse_state.error = 1;
        return 0;
    }
    return 1;
}

static int parse_resource_model()
{
    if (!parse_enabled_content("model")) {
        return 0;
    }
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
    if (!parse_enabled_content("trade")) {
        return 0;
    }
    return parse_optional_int_attribute("buy", &resource_parse_state.data.default_trade_price.buy, 0) &&
        parse_optional_int_attribute("sell", &resource_parse_state.data.default_trade_price.sell, 0);
}

static int parse_resource_cart_graphic()
{
    if (!parse_enabled_content("cart")) {
        return 0;
    }
    if (!xml_parser_has_attribute("load")) {
        log_error("Resource cart graphic is missing required load", 0, 0);
        resource_parse_state.error = 1;
        return 0;
    }

    const char *load = xml_parser_get_attribute_string("load");
    ImageGroupEntryRef ref = parse_image_ref();
    if (load && std::strcmp(load, "single") == 0) {
        resource_parse_state.cart_single_load = std::move(ref);
    } else if (load && std::strcmp(load, "multiple") == 0) {
        resource_parse_state.cart_multiple_loads = std::move(ref);
    } else if (load && std::strcmp(load, "eight") == 0) {
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
    if (!parse_enabled_content("storage")) {
        return 0;
    }
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
    if (!parse_enabled_content("panel_icon")) {
        return 0;
    }
    resource_parse_state.panel_icon = parse_image_ref();
    return !resource_parse_state.error;
}

static int parse_resource_empire_icon()
{
    if (!parse_enabled_content("empire_icon")) {
        return 0;
    }
    resource_parse_state.empire_icon = parse_image_ref();
    return !resource_parse_state.error;
}

static int parse_resource_editor_icon()
{
    if (!parse_enabled_content("editor_icon")) {
        return 0;
    }
    resource_parse_state.editor_icon = parse_image_ref();
    return !resource_parse_state.error;
}

static int parse_resource_editor_empire_icon()
{
    if (!parse_enabled_content("editor_empire_icon")) {
        return 0;
    }
    resource_parse_state.editor_empire_icon = parse_image_ref();
    return !resource_parse_state.error;
}

static int parse_resource_graphics()
{
    return parse_enabled_content("graphics");
}

static const xml_parser_element RESOURCE_XML_ELEMENTS[] = {
    { "resource", parse_resource_root, nullptr, nullptr, nullptr },
    { "model", parse_resource_model, nullptr, "resource", nullptr },
    { "trade", parse_resource_trade, nullptr, "resource", nullptr },
    { "graphics", parse_resource_graphics, nullptr, "resource", nullptr },
    { "cart", parse_resource_cart_graphic, nullptr, "graphics", nullptr },
    { "storage", parse_resource_storage_graphic, nullptr, "graphics", nullptr },
    { "panel_icon", parse_resource_panel_icon, nullptr, "graphics", nullptr },
    { "empire_icon", parse_resource_empire_icon, nullptr, "graphics", nullptr },
    { "editor_icon", parse_resource_editor_icon, nullptr, "graphics", nullptr },
    { "editor_empire_icon", parse_resource_editor_empire_icon, nullptr, "graphics", nullptr },
};

static int parse_resource_definition_buffer(
    const char *filename,
    const std::vector<char> &buffer,
    ResourceParseState *out_definition)
{
    const ErrorContextScope error_scope("Resource XML", filename);

    resource_parse_state = {};
    const int parsed = xml_definition::parse_buffer(
        filename,
        "Resource",
        RESOURCE_XML_ELEMENTS,
        static_cast<int>(sizeof(RESOURCE_XML_ELEMENTS) / sizeof(RESOURCE_XML_ELEMENTS[0])),
        buffer);
    if (!parsed || resource_parse_state.error || !resource_parse_state.saw_root) {
        log_error("Unable to parse Resource xml", filename, 0);
        return 0;
    }
    if (out_definition) {
        *out_definition = std::move(resource_parse_state);
    }
    return 1;
}

static int parse_resource_definition_file(const char *filename, ResourceParseState *out_definition)
{
    std::vector<char> buffer;
    if (!xml_definition::load_file_to_buffer(filename, buffer, "Resource")) {
        return 0;
    }
    return parse_resource_definition_buffer(filename, buffer, out_definition);
}

static int validate_staged_resource_definitions(StagedResourceDefinitions &staged)
{
    if (staged.overlays.active_count() == 0) {
        staged.failure_reason = "No enabled Resource definitions remain after applying the configured mod layers.";
        return 0;
    }

    for (std::optional<StagedResourceDefinition> &winner : staged.winners) {
        if (!winner || winner->definition.disabled) {
            continue;
        }
        winner->definition.data.text = lang_get_string_by_key(winner->definition.name_key.c_str());
        if (!winner->definition.data.text) {
            staged.failure_reason = "Resource '" + winner->definition.text_id +
                "' has unsupported name_key '" + winner->definition.name_key +
                "' in " + source_path(winner->source) + ".";
            return 0;
        }
    }
    return 1;
}

static void publish_resource_definitions(const StagedResourceDefinitions &staged)
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
    resource_graphics_reset();
    building_type_registry_impl::BuildingGraphicsDef::reset_resource_storage_images();
    figure_type_registry_impl::FigureGraphics::reset_resource_cart_images();

    for (std::size_t slot = 0; slot < staged.winners.size(); ++slot) {
        const std::optional<StagedResourceDefinition> &winner = staged.winners[slot];
        if (!winner || winner->definition.disabled) {
            continue;
        }
        const ResourceParseState &definition = winner->definition;
        const resource_type type = definition.type;
        resource_text_id_storage[slot] = definition.text_id;
        resource_name_key_storage[slot] = definition.name_key;
        resource_xml_attr_storage[slot] = definition.xml_attr_name;

        resource_data data = definition.data;
        data.text_id = resource_text_id_storage[slot].c_str();
        data.name_key = resource_name_key_storage[slot].c_str();
        data.xml_attr_name = resource_xml_attr_storage[slot].empty() ?
            nullptr : resource_xml_attr_storage[slot].c_str();
        resource_info_defaults[type] = data;
        resource_declared_slots[slot] = 1;
        resource_text_id_lookup[resource_text_id_storage[slot]] = type;
        loaded_resources.push_back(type);

        building_type_registry_impl::BuildingGraphicsDef::set_resource_storage_images(
            type, definition.storage_images);
        figure_type_registry_impl::FigureGraphics::set_resource_cart_images(
            type,
            definition.cart_single_load,
            definition.cart_multiple_loads,
            definition.cart_eight_loads);
        ResourceGraphics &graphics = mutable_resource_graphics(type);
        graphics.set_panel_icon(definition.panel_icon);
        graphics.set_empire_icon(definition.empire_icon);
        graphics.set_editor_icon(definition.editor_icon);
        graphics.set_editor_empire_icon(definition.editor_empire_icon);

        if (type != RESOURCE_NONE &&
            (data.flags & RESOURCE_FLAG_SPECIAL) != RESOURCE_FLAG_SPECIAL) {
            production_resources.push_back(type);
        }
    }
    resource_definition_overlays = staged.overlays;
}

static int load_resource_definitions()
{
    StagedResourceDefinitions staged;
    std::string enumeration_failure;
    if (!mod_definition::for_each_configured_definition_file(
            {"Resources"},
            "Resource",
            true,
            [&](const mod_definition::DefinitionSource &source) {
                ResourceParseState definition;
                if (!parse_resource_definition_file(source.full_path.c_str(), &definition)) {
                    staged.failure_reason = "Unable to parse Resource xml: " + source.full_path;
                    return false;
                }
                return stage_resource_definition(staged, std::move(definition), source) != 0;
            },
            nullptr,
            &enumeration_failure)) {
        resource_failure_reason = staged.failure_reason.empty() ? enumeration_failure : staged.failure_reason;
        error_context_report_error("Unable to load layered Resource definitions.", resource_failure_reason.c_str());
        return 0;
    }
    if (!validate_staged_resource_definitions(staged)) {
        resource_failure_reason = staged.failure_reason;
        error_context_report_error("Unable to validate layered Resource definitions.", resource_failure_reason.c_str());
        return 0;
    }

    publish_resource_definitions(staged);
    resource_failure_reason.clear();
    return 1;
}

int resource_is_food(resource_type resource)
{
    resource_data *data = resource_get_data(resource);
    return data && (data->flags & RESOURCE_FLAG_FOOD) == RESOURCE_FLAG_FOOD;
}

int resource_is_raw_material(resource_type resource)
{
    return resource_is_declared(resource) && !resource_is_food(resource) &&
        resource_get_supply_chain_for_good(0, resource) == 0;
}

int resource_is_inventory(resource_type resource)
{
    resource_data *data = resource_get_data(resource);
    return data && (data->flags & RESOURCE_FLAG_INVENTORY) == RESOURCE_FLAG_INVENTORY;
}

int resource_is_inventory_good(resource_type resource)
{
    return resource_is_inventory(resource) && !resource_is_food(resource);
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

int resource_init(void)
{
    if (!load_resource_definitions()) {
        return 0;
    }
    production_method_registry_reset_production_overrides();
    std::memcpy(resource_info, resource_info_defaults, sizeof(resource_info_defaults));
    resource_id_bridge_reset_for_runtime();
    return 1;
}

const char *resource_get_failure_reason(void)
{
    return resource_failure_reason.c_str();
}

const char *resource_definition_source_path(const char *text_id)
{
    if (!text_id || !*text_id) {
        return nullptr;
    }
    const mod_definition::DefinitionOverlayEntry *entry = resource_definition_overlays.find(text_id);
    return entry ? entry->source.full_path.c_str() : nullptr;
}

int resource_definition_is_suppressed(const char *text_id)
{
    if (!text_id || !*text_id) {
        return 0;
    }
    const mod_definition::DefinitionOverlayEntry *entry = resource_definition_overlays.find(text_id);
    return entry && entry->disabled;
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
        buffer_write_u16(buf, static_cast<uint16_t>(std::clamp(production, 0, 0xffff)));
    }
}

void production_rates_load(buffer *buf)
{
    for (int i = 0; i < resource_production_count(); i++) {
        resource_type resource = resource_get_production(i);
        production_method_registry_set_production_per_month_for_resource(resource, buffer_read_u16(buf));
    }
}

#ifdef STARTUP_PARSER_TEST
int resource_layered_definition_buffers_are_valid_for_test(
    const resource_layer_test_input *inputs,
    int input_count,
    const char *query_id,
    resource_layer_test_result *result)
{
    if (!inputs || input_count < 0) {
        return 0;
    }

    StagedResourceDefinitions staged;
    for (int index = 0; index < input_count; ++index) {
        const resource_layer_test_input &input = inputs[index];
        const char *xml = input.xml ? input.xml : "";
        std::vector<char> buffer(xml, xml + std::strlen(xml));
        mod_definition::DefinitionSource source;
        source.layer_index = input.layer_index < 0 ? 0 : static_cast<std::size_t>(input.layer_index);
        source.mod_name = input.mod_name ? input.mod_name : "";
        source.full_path = input.source_path ? input.source_path : "ResourceLayerTest.xml";
        source.file_name = source.full_path;
        source.category = "Resources";
        source.registry_relative_path = "Resources\\" + source.file_name;

        ResourceParseState definition;
        if (!parse_resource_definition_buffer(source.full_path.c_str(), buffer, &definition) ||
            !stage_resource_definition(staged, std::move(definition), source)) {
            return 0;
        }
    }
    if (!validate_staged_resource_definitions(staged)) {
        return 0;
    }

    if (result) {
        *result = {};
        result->active_count = static_cast<int>(staged.overlays.active_count());
        result->suppressed_count = static_cast<int>(staged.overlays.suppressed_count());
        result->queried_slot = -1;
        result->queried_source_layer = -1;

        const std::string id = query_id ? query_id : "";
        const auto identity = staged.ids.find(id);
        const mod_definition::DefinitionOverlayEntry *overlay = staged.overlays.find(id);
        if (identity != staged.ids.end() && overlay) {
            const resource_type slot = identity->second.slot;
            result->queried_slot = slot;
            result->queried_disabled = overlay->disabled ? 1 : 0;
            result->queried_source_layer = static_cast<int>(overlay->source.layer_index);
            const std::optional<StagedResourceDefinition> &winner =
                staged.winners[static_cast<std::size_t>(slot)];
            if (winner && !winner->definition.disabled) {
                result->queried_buy = winner->definition.data.default_trade_price.buy;
                result->queried_sell = winner->definition.data.default_trade_price.sell;
            }
        }
    }
    return 1;
}
#endif
