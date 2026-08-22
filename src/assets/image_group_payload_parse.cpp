#include "assets/image_group_payload_internal.h"

#include "core/crash_context.h"

#include "core/file.h"
#include "core/log.h"
#include "core/xml_parser.h"
#include "core/xml_value.h"

#include <utility>

namespace image_group_payload_internal {

namespace {

struct ParseState {
    std::string requested_key;
    std::string xml_path;
    xml_asset_source source = XML_ASSET_SOURCE_AUTO;
    std::unique_ptr<ImageGroupDoc> doc;
    ImageEntryDef *current_entry = nullptr;
    int error = 0;
};

ParseState g_parse_state;

static const char *INVERT_VALUES[3] = { "horizontal", "vertical", "both" };
static const char *ROTATE_VALUES[3] = { "90", "180", "270" };
static const char *PART_VALUES[2] = { "footprint", "top" };
static const char *MASK_VALUES[2] = { "grayscale", "alpha" };

// Input: one XML file path and an output byte buffer.
// Output: the file contents in memory so the XML parser can work on a stable buffer.
int read_file_to_buffer(const char *filename, std::vector<char> &buffer)
{
    FILE *file = file_open(filename, "rb");
    if (!file) {
        log_error("Unable to open image group xml", filename, 0);
        return 0;
    }

    if (fseek(file, 0, SEEK_END) != 0) {
        file_close(file);
        log_error("Unable to seek image group xml", filename, 0);
        return 0;
    }

    long size = ftell(file);
    if (size < 0) {
        file_close(file);
        log_error("Unable to size image group xml", filename, 0);
        return 0;
    }
    rewind(file);

    buffer.resize(static_cast<size_t>(size));
    if (!buffer.empty()) {
        const size_t bytes_read = fread(buffer.data(), 1, buffer.size(), file);
        if (bytes_read != buffer.size()) {
            file_close(file);
            log_error("Unable to read image group xml", filename, 0);
            return 0;
        }
    }

    file_close(file);
    return 1;
}

// Input: parser state positioned on the opening <assetlist> tag.
// Output: a new source-local document shell whose key/source must match the requested cache key.
int xml_start_assetlist(void)
{
    const char *name = xml_parser_get_attribute_string("name");
    const std::string normalized_name = normalize_path_key(name);
    if (normalized_name.empty() || normalized_name != g_parse_state.requested_key) {
        crash_context_report_error("ImageGroup assetlist name does not match requested key", name ? name : "");
        g_parse_state.error = 1;
        return 0;
    }

    g_parse_state.doc = std::make_unique<ImageGroupDoc>();
    g_parse_state.doc->key = normalized_name;
    g_parse_state.doc->xml_path = g_parse_state.xml_path;
    g_parse_state.doc->source = g_parse_state.source;
    return 1;
}

int xml_start_figure_graphics(void)
{
    if (!g_parse_state.doc || g_parse_state.doc->figure_graphics_contract.present) {
        crash_context_report_error("ImageGroup contains duplicate or misplaced figure_graphics metadata", g_parse_state.requested_key.c_str());
        g_parse_state.error = 1;
        return 0;
    }
    ImageGroupFigureGraphicsContract &contract = g_parse_state.doc->figure_graphics_contract;
    const char *default_image = xml_parser_get_attribute_string("default");
    const char *corpse_image = xml_parser_get_attribute_string("corpse");
    const char *action_image = xml_parser_get_attribute_string("action");
    const char *action_state = xml_parser_get_attribute_string("action_state");
    const char *runtime_selected_source = xml_parser_get_attribute_string("runtime_selected_source");
    contract.present = 1;
    if (xml_parser_has_attribute("runtime_selected_image") &&
        !xml_value::parse_bool(xml_parser_get_attribute_string("runtime_selected_image"), &contract.runtime_selected_image)) {
        crash_context_report_error("ImageGroup figure_graphics runtime_selected_image is invalid", g_parse_state.requested_key.c_str());
        g_parse_state.error = 1;
        return 0;
    }
    if (xml_parser_has_attribute("runtime_selected_empty_is_hidden") &&
        !xml_value::parse_bool(xml_parser_get_attribute_string("runtime_selected_empty_is_hidden"), &contract.runtime_selected_empty_is_hidden)) {
        crash_context_report_error("ImageGroup figure_graphics runtime_selected_empty_is_hidden is invalid", g_parse_state.requested_key.c_str());
        g_parse_state.error = 1;
        return 0;
    }
    contract.runtime_selected_source = runtime_selected_source ? runtime_selected_source : "";
    contract.default_image_pattern = default_image ? default_image : "";
    contract.default_frame_count = xml_parser_get_attribute_int("frames");
    contract.corpse_image_pattern = corpse_image ? corpse_image : "";
    contract.corpse_frame_count = xml_parser_get_attribute_int("corpse_frames");
    contract.action_image_pattern = action_image ? action_image : "";
    contract.action_state = action_state ? action_state : "";
    contract.action_frame_count = xml_parser_get_attribute_int("action_frames");
    contract.action_min_wait_ticks = xml_parser_get_attribute_int("action_min_wait_ticks");
    const bool has_action = !contract.action_image_pattern.empty();
    const bool runtime_selected_is_complete = contract.runtime_selected_image && !contract.runtime_selected_source.empty() &&
        contract.default_image_pattern.empty() && contract.default_frame_count <= 0 && contract.corpse_image_pattern.empty() &&
        contract.corpse_frame_count <= 0 && !has_action && contract.action_state.empty() && contract.action_frame_count <= 0;
    const bool frame_contract_is_complete = !contract.runtime_selected_image && !contract.runtime_selected_empty_is_hidden && !contract.default_image_pattern.empty() &&
        contract.default_frame_count > 0 && contract.corpse_image_pattern.empty() == (contract.corpse_frame_count <= 0) &&
        has_action == !contract.action_state.empty() && has_action == (contract.action_frame_count > 0);
    if ((!runtime_selected_is_complete && !frame_contract_is_complete) || contract.action_min_wait_ticks < 0) {
        crash_context_report_error("ImageGroup figure_graphics metadata is incomplete or invalid", g_parse_state.requested_key.c_str());
        g_parse_state.error = 1;
        return 0;
    }
    return 1;
}

// Input: parser state positioned on a <layer> or <frame> tag plus default part/mask fallbacks.
// Output: a raw symbolic layer definition with all XML attributes captured but no raster work performed.
RawLayerDef parse_raw_layer(layer_isometric_part default_part, layer_mask default_mask)
{
    RawLayerDef layer;
    const char *path = xml_parser_get_attribute_string("src");
    const char *group = xml_parser_get_attribute_string("group");
    const char *image_id = xml_parser_get_attribute_string("image");

    if (path && *path) {
        layer.reference.type = RawReferenceType::PNG_PATH;
        layer.reference.path = normalize_path_key(path);
    } else if (group && *group) {
        layer.reference.type = RawReferenceType::GROUP_IMAGE;
        layer.reference.group_key = normalize_group_reference_key(group, g_parse_state.requested_key);
        layer.reference.image_id = image_id ? image_id : "";
    }

    layer.src_x = xml_parser_get_attribute_int("src_x");
    layer.src_y = xml_parser_get_attribute_int("src_y");
    layer.x_offset = xml_parser_get_attribute_int("x");
    layer.y_offset = xml_parser_get_attribute_int("y");
    layer.width = xml_parser_get_attribute_int("width");
    layer.height = xml_parser_get_attribute_int("height");
    layer.invert = static_cast<layer_invert_type>(
        xml_parser_get_attribute_enum("invert", INVERT_VALUES, 3, INVERT_HORIZONTAL));
    layer.rotate = static_cast<layer_rotate_type>(
        xml_parser_get_attribute_enum("rotate", ROTATE_VALUES, 3, ROTATE_90_DEGREES));
    layer.part = static_cast<layer_isometric_part>(
        xml_parser_get_attribute_enum("part", PART_VALUES, 2, PART_FOOTPRINT));
    if (layer.part == PART_NONE) {
        layer.part = default_part;
    }
    layer.mask = static_cast<layer_mask>(
        xml_parser_get_attribute_enum("mask", MASK_VALUES, 2, LAYER_MASK_GRAYSCALE));
    if (!xml_parser_has_attribute("mask")) {
        layer.mask = default_mask;
    }
    return layer;
}

// Input: parser state positioned on an <image> tag.
// Output: a new source-local entry definition appended to the current document order.
int xml_start_image(void)
{
    if (!g_parse_state.doc) {
        g_parse_state.error = 1;
        return 0;
    }

    const char *id = xml_parser_get_attribute_string("id");
    if (!id || !*id) {
        crash_context_report_error("ImageGroup image is missing id", g_parse_state.requested_key.c_str());
        g_parse_state.error = 1;
        return 0;
    }

    ImageEntryDef entry;
    entry.id = id;
    entry.source = g_parse_state.source;
    entry.local_order = static_cast<int>(g_parse_state.doc->ordered_ids.size());
    entry.width = xml_parser_get_attribute_int("width");
    entry.height = xml_parser_get_attribute_int("height");
    const int has_logical_width = xml_parser_has_attribute("logical_width");
    const int has_logical_height = xml_parser_has_attribute("logical_height");
    if (has_logical_width != has_logical_height) {
        crash_context_report_error("ImageGroup logical size requires both logical_width and logical_height", entry.id.c_str());
        g_parse_state.error = 1;
        return 0;
    }
    if (has_logical_width) {
        entry.fixed_logical_size.width = xml_parser_get_attribute_int("logical_width");
        entry.fixed_logical_size.height = xml_parser_get_attribute_int("logical_height");
        if (entry.fixed_logical_size.width <= 0 || entry.fixed_logical_size.height <= 0) {
            crash_context_report_error("ImageGroup logical size requires positive fixed-point logical units", entry.id.c_str());
            g_parse_state.error = 1;
            return 0;
        }
    }
    entry.draw_offset_x = xml_parser_get_attribute_int("x");
    entry.draw_offset_y = xml_parser_get_attribute_int("y");
    entry.is_isometric = xml_parser_get_attribute_bool("isometric");

    const char *path = xml_parser_get_attribute_string("src");
    const char *group = xml_parser_get_attribute_string("group");
    const char *image_id = xml_parser_get_attribute_string("image");
    if (group && *group) {
        entry.has_full_image_ref = 1;
        entry.full_image_ref.type = RawReferenceType::GROUP_IMAGE;
        entry.full_image_ref.group_key = normalize_group_reference_key(group, g_parse_state.requested_key);
        entry.full_image_ref.image_id = image_id ? image_id : "";
    } else if (path && *path) {
        RawLayerDef base_layer;
        base_layer.reference.type = RawReferenceType::PNG_PATH;
        base_layer.reference.path = normalize_path_key(path);
        entry.layers.push_back(std::move(base_layer));
    }

    g_parse_state.doc->ordered_ids.push_back(entry.id);
    auto inserted = g_parse_state.doc->entries.emplace(entry.id, std::move(entry));
    g_parse_state.current_entry = &inserted.first->second;
    return 1;
}

// Input: parser state positioned on a <layer> tag inside one image.
// Output: one additional symbolic layer appended to the current entry.
int xml_start_layer(void)
{
    if (!g_parse_state.current_entry) {
        g_parse_state.error = 1;
        return 0;
    }
    g_parse_state.current_entry->layers.push_back(parse_raw_layer(PART_BOTH, LAYER_MASK_NONE));
    return 1;
}

// Input: parser state positioned on an <animation> tag inside one image.
// Output: animation metadata recorded on the current entry without resolving any frames yet.
int xml_start_animation(void)
{
    if (!g_parse_state.current_entry) {
        g_parse_state.error = 1;
        return 0;
    }
    RawAnimationDef &animation = g_parse_state.current_entry->animation;
    animation.present = 1;
    animation.metadata.num_sprites = xml_parser_get_attribute_int("frames");
    animation.metadata.speed_id = xml_parser_get_attribute_int("speed");
    animation.metadata.can_reverse = xml_parser_get_attribute_bool("reversible");
    animation.metadata.sprite_offset_x = xml_parser_get_attribute_int("x");
    animation.metadata.sprite_offset_y = xml_parser_get_attribute_int("y");
    animation.implicit_frame_count = animation.metadata.num_sprites;
    return 1;
}

// Input: parser state positioned on a <frame> tag inside one animation.
// Output: one explicit frame layer definition appended to the current animation.
int xml_start_frame(void)
{
    if (!g_parse_state.current_entry || !g_parse_state.current_entry->animation.present) {
        g_parse_state.error = 1;
        return 0;
    }
    g_parse_state.current_entry->animation.explicit_frames.push_back(parse_raw_layer(PART_BOTH, LAYER_MASK_NONE));
    return 1;
}

static const xml_parser_element kXmlElements[] = {
    { "assetlist", xml_start_assetlist, nullptr },
    { "figure_graphics", xml_start_figure_graphics, nullptr, "assetlist" },
    { "image", xml_start_image, nullptr, "assetlist" },
    { "layer", xml_start_layer, nullptr, "image" },
    { "animation", xml_start_animation, nullptr, "image" },
    { "frame", xml_start_frame, nullptr, "animation" }
};

// Input: one XML path plus the requested group key/source identity.
// Output: a fully parsed source-local document or null when the XML is malformed or mismatched.
std::unique_ptr<ImageGroupDoc> parse_group_doc(const char *xml_path, const std::string &path_key, xml_asset_source source)
{
    CrashContextScope crash_scope("image_group.parse_group_doc", xml_path);
    std::vector<char> buffer;
    if (!read_file_to_buffer(xml_path, buffer)) {
        return nullptr;
    }

    g_parse_state = {};
    g_parse_state.requested_key = path_key;
    g_parse_state.xml_path = xml_path;
    g_parse_state.source = source;

    if (!xml_parser_init(kXmlElements, static_cast<int>(sizeof(kXmlElements) / sizeof(kXmlElements[0])), 1)) {
        crash_context_report_error("Unable to initialize image group parser", xml_path);
        return nullptr;
    }

    const int parsed = xml_parser_parse(buffer.data(), static_cast<unsigned int>(buffer.size()), 1);
    xml_parser_free();

    if (!parsed || g_parse_state.error || !g_parse_state.doc) {
        return nullptr;
    }
    return std::move(g_parse_state.doc);
}

} // namespace

// Input: a source-local XML document path, requested group key, and source.
// Output: a parsed document that contains unresolved symbolic entry data only, or null on parse failure.
const ImageGroupDoc *load_group_doc(const std::string &group_key, xml_asset_source source)
{
    if (const ImageGroupDoc *existing = find_group_doc(group_key, source)) {
        return existing;
    }

    const std::string cache_key = make_source_doc_cache_key(group_key, source);
    if (g_failed_group_docs.find(cache_key) != g_failed_group_docs.end()) {
        return nullptr;
    }

    char xml_path[FILE_NAME_MAX] = { 0 };
    xml_asset_source resolved_source = XML_ASSET_SOURCE_AUTO;
    if (!xml_resolve_assetlist_path(xml_path, group_key.c_str(), source, &resolved_source) || resolved_source != source) {
        g_failed_group_docs.insert(cache_key);
        return nullptr;
    }

    std::unique_ptr<ImageGroupDoc> doc = parse_group_doc(xml_path, group_key, source);
    if (!doc) {
        g_failed_group_docs.insert(cache_key);
        return nullptr;
    }

    const ImageGroupDoc *doc_ptr = doc.get();
    g_group_docs.emplace(cache_key, std::move(doc));
    return doc_ptr;
}

} // namespace image_group_payload_internal
