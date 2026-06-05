#include "assets/augustus_asset_extractor.h"

#include "assets/augustus_julius_template_resolver.h"
#include "assets/graphics_extractor_common.h"
#include "core/crash_context.h"

extern "C" {
#include "assets/assets.h"
#include "core/dir.h"
#include "core/file.h"
#include "core/log.h"
#include "core/png_read.h"
#include "core/xml_parser.h"
#include "game/mod_manager.h"
#include "platform/file_manager.h"
}

#include "spng/spng.h"

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace {

constexpr char kStampPrefix[] = "augustus_extract_v2:";

struct ExtractionStats {
    int source_files = 0;
    int groups_exported = 0;
    int images_exported = 0;
    int pngs_written = 0;
};

struct ExtractionPaths {
    std::string source_graphics_path;
    std::string output_graphics_path;
    std::string julius_graphics_path;
};

struct AtlasReference {
    std::string src;
    std::string group;
    std::string image;
    std::string part;
    std::string mask;
    std::string invert;
    std::string rotate;
    int src_x = 0;
    int src_y = 0;
    int x = 0;
    int y = 0;
    int width = 0;
    int height = 0;
    bool has_src_x = false;
    bool has_src_y = false;
    bool has_x = false;
    bool has_y = false;
    bool has_width = false;
    bool has_height = false;
    bool is_direct_crop = false;
};

struct AtlasAnimationFrame {
    AtlasReference reference;
};

struct AtlasAnimation {
    bool present = false;
    bool has_frames = false;
    int frames = 0;
    bool has_speed = false;
    int speed = 0;
    bool reversible = false;
    bool has_x = false;
    int x = 0;
    bool has_y = false;
    int y = 0;
    std::vector<AtlasAnimationFrame> frames_data;
};

struct AtlasImage {
    std::string id;
    bool synthetic_id = false;
    bool has_explicit_id = false;
    bool is_isometric = false;
    bool has_width = false;
    bool has_height = false;
    int width = 0;
    int height = 0;
    bool materialized = false;
    std::vector<AtlasReference> layers;
    AtlasAnimation animation;
    size_t source_index = 0;
};

struct AtlasDocument {
    std::string assetlist_name;
    std::string family_name;
    std::string xml_path;
    std::string png_path;
    std::vector<AtlasImage> images;
};

struct ParseState {
    AtlasDocument document;
    AtlasImage *current_image = nullptr;
    AtlasAnimation *current_animation = nullptr;
    int error = 0;
};

struct OutputGroup {
    std::string family_name;
    std::string group_name;
    std::string group_key;
    std::string directory_path;
    std::string xml_path;
    std::vector<std::string> alias_group_keys;
    std::vector<int> image_indices;
};

ParseState g_parse_state;

struct InferredPartCache {
    std::unordered_map<std::string, std::vector<std::string>> parts_by_image_id;
    std::unordered_map<std::string, int> resolution_state_by_image_id;
};

struct LocalReferenceTargets {
    std::unordered_map<std::string, std::string> group_key_by_image_id;
    std::unordered_set<std::string> ambiguous_image_ids;
};

const ExtractionPaths *g_active_paths = nullptr;

using graphics_extractor::append_path_component;
using graphics_extractor::append_attribute;
using graphics_extractor::append_indent;
using graphics_extractor::ensure_directory;
using graphics_extractor::load_file_to_buffer;
using graphics_extractor::make_generated_image_id;
using graphics_extractor::normalize_key;
using graphics_extractor::parse_generated_image_index;
using graphics_extractor::read_text_file;
using graphics_extractor::sanitize_component;
using graphics_extractor::write_text_file;
using graphics_extractor::without_trailing_separator;

template <typename Image, typename Visitor>
static void visit_image_references(Image &image_data, Visitor visitor)
{
    for (auto &reference : image_data.layers) {
        visitor(reference);
    }
    for (auto &frame : image_data.animation.frames_data) {
        visitor(frame.reference);
    }
}

static int reference_has_pixel_data(const AtlasReference &reference)
{
    return !reference.src.empty() || reference.is_direct_crop;
}

static int image_has_materialized_pixels(const AtlasImage &image_data)
{
    int has_pixels = 0;
    visit_image_references(image_data, [&](const AtlasReference &reference) {
        has_pixels = has_pixels || reference_has_pixel_data(reference);
    });
    return has_pixels;
}

static std::string ensure_trailing_separator(std::string path)
{
    if (!path.empty() && path.back() != '/' && path.back() != '\\') {
        path.push_back('/');
    }
    return path;
}

static std::string make_game_relative_path(const char *game_root_path, const char *relative_path)
{
    const std::string game_root = game_root_path && *game_root_path ? game_root_path : ".";
    return graphics_extractor::append_path_component(game_root, relative_path ? relative_path : "");
}

static bool build_extraction_paths(const augustus_asset_extractor_config *config, ExtractionPaths &paths)
{
    const char *game_root = config ? config->game_root_path : nullptr;
    if (config && config->source_graphics_path && *config->source_graphics_path) {
        paths.source_graphics_path = config->source_graphics_path;
    } else {
        paths.source_graphics_path = make_game_relative_path(game_root, ASSETS_DIR_NAME "/" ASSETS_IMAGE_PATH);
    }

    if (config && config->output_graphics_path && *config->output_graphics_path) {
        paths.output_graphics_path = config->output_graphics_path;
    } else {
        paths.output_graphics_path = make_game_relative_path(game_root, "Mods/Augustus/Graphics");
    }

    if (config && config->julius_graphics_path && *config->julius_graphics_path) {
        paths.julius_graphics_path = config->julius_graphics_path;
    } else {
        paths.julius_graphics_path = make_game_relative_path(game_root, "Mods/Julius/Graphics");
    }

    paths.output_graphics_path = ensure_trailing_separator(std::move(paths.output_graphics_path));
    paths.julius_graphics_path = ensure_trailing_separator(std::move(paths.julius_graphics_path));
    return !paths.source_graphics_path.empty() && !paths.output_graphics_path.empty();
}

class ScopedExtractionPaths {
public:
    explicit ScopedExtractionPaths(const ExtractionPaths *paths)
        : previous_(g_active_paths)
    {
        g_active_paths = paths;
    }

    ~ScopedExtractionPaths()
    {
        g_active_paths = previous_;
    }

private:
    const ExtractionPaths *previous_ = nullptr;
};

static void hash_bytes(uint64_t &hash, const void *data_ptr, size_t size)
{
    const unsigned char *bytes = static_cast<const unsigned char *>(data_ptr);
    const uint64_t fnv_prime = 1099511628211ull;
    for (size_t i = 0; i < size; ++i) {
        hash ^= bytes[i];
        hash *= fnv_prime;
    }
}

static void hash_string(uint64_t &hash, const char *text)
{
    if (!text) {
        return;
    }
    hash_bytes(hash, text, strlen(text));
}

static void hash_int64(uint64_t &hash, uint64_t value)
{
    hash_bytes(hash, &value, sizeof(value));
}

static std::string make_augustus_graphics_root(void)
{
    if (g_active_paths) {
        return g_active_paths->output_graphics_path;
    }
    return mod_manager_get_augustus_graphics_path();
}

static std::string make_julius_graphics_root(void)
{
    if (g_active_paths) {
        return g_active_paths->julius_graphics_path;
    }
    return mod_manager_get_julius_graphics_path();
}

static std::string make_stamp_path(void)
{
    return without_trailing_separator(make_augustus_graphics_root()) + ".graphics_extract.stamp";
}

static std::string make_source_graphics_path(void)
{
    if (g_active_paths) {
        return g_active_paths->source_graphics_path;
    }

    char path[FILE_NAME_MAX];
    const char *asset_root = platform_file_manager_get_directory_for_location(PATH_LOCATION_ASSET, 0);
    if (!append_path_component(path, sizeof(path), asset_root, ASSETS_IMAGE_PATH)) {
        return {};
    }
    return path;
}

static int stop_on_first_entry(const char *name, long modified_time)
{
    (void) name;
    (void) modified_time;
    return LIST_MATCH;
}

static bool build_expected_stamp(std::string &stamp)
{
    const std::string source_graphics_path = make_source_graphics_path();
    if (source_graphics_path.empty()) {
        return false;
    }

    uint64_t hash = 1469598103934665603ull;
    const std::string normalized_source_graphics_path = normalize_key(source_graphics_path.c_str());
    hash_string(hash, normalized_source_graphics_path.c_str());

    const dir_listing *xml_files = dir_find_files_with_extension(source_graphics_path.c_str(), "xml");
    for (int i = 0; i < xml_files->num_files; ++i) {
        hash_string(hash, xml_files->files[i].name);
        hash_int64(hash, xml_files->files[i].modified_time);
    }

    const dir_listing *png_files = dir_find_files_with_extension(source_graphics_path.c_str(), "png");
    for (int i = 0; i < png_files->num_files; ++i) {
        hash_string(hash, png_files->files[i].name);
        hash_int64(hash, png_files->files[i].modified_time);
    }

    char buffer[128];
    snprintf(buffer, sizeof(buffer), "%s%016llx", kStampPrefix, static_cast<unsigned long long>(hash));
    stamp = buffer;
    return true;
}

static bool write_png(const std::string &path, const color_t *pixels, int width, int height)
{
    if (!pixels || width <= 0 || height <= 0) {
        return false;
    }

    FILE *file = file_open(path.c_str(), "wb");
    if (!file) {
        return false;
    }

    spng_ctx *ctx = spng_ctx_new(SPNG_CTX_ENCODER);
    if (!ctx) {
        file_close(file);
        return false;
    }

    int result = spng_set_option(ctx, SPNG_IMG_COMPRESSION_LEVEL, 1);
    if (result == 0) {
        result = spng_set_png_file(ctx, file);
    }

    spng_ihdr ihdr = {};
    ihdr.width = static_cast<uint32_t>(width);
    ihdr.height = static_cast<uint32_t>(height);
    ihdr.bit_depth = 8;
    ihdr.color_type = SPNG_COLOR_TYPE_TRUECOLOR_ALPHA;

    if (result == 0) {
        result = spng_set_ihdr(ctx, &ihdr);
    }
    if (result == 0) {
        result = spng_encode_image(ctx, 0, 0, SPNG_FMT_PNG, SPNG_ENCODE_PROGRESSIVE | SPNG_ENCODE_FINALIZE);
    }

    std::vector<uint8_t> scanline(static_cast<size_t>(width) * 4);
    for (int y = 0; result == 0 && y < height; ++y) {
        uint8_t *destination = scanline.data();
        const color_t *source_row = &pixels[static_cast<size_t>(y) * width];
        for (int x = 0; x < width; ++x) {
            const color_t color = source_row[x];
            destination[0] = static_cast<uint8_t>(COLOR_COMPONENT(color, COLOR_BITSHIFT_RED));
            destination[1] = static_cast<uint8_t>(COLOR_COMPONENT(color, COLOR_BITSHIFT_GREEN));
            destination[2] = static_cast<uint8_t>(COLOR_COMPONENT(color, COLOR_BITSHIFT_BLUE));
            destination[3] = static_cast<uint8_t>(COLOR_COMPONENT(color, COLOR_BITSHIFT_ALPHA));
            destination += 4;
        }
        const int scanline_result = spng_encode_scanline(ctx, scanline.data(), scanline.size());
        if (scanline_result != SPNG_OK && scanline_result != SPNG_EOI) {
            result = scanline_result;
        }
    }

    spng_ctx_free(ctx);
    file_close(file);
    return result == 0 || result == SPNG_EOI;
}

static int xml_start_assetlist(void)
{
    const char *name = xml_parser_get_attribute_string("name");
    if (!name || !*name) {
        log_error("Augustus atlas assetlist is missing a name", 0, 0);
        g_parse_state.error = 1;
        return 0;
    }

    g_parse_state.document.assetlist_name = name;
    g_parse_state.document.family_name = sanitize_component(name);
    return 1;
}

static void push_reference(
    std::vector<AtlasReference> &destination,
    const char *path,
    const char *group,
    const char *image_id)
{
    AtlasReference reference;
    if (path && *path) {
        reference.src = path;
    }
    if (group && *group) {
        reference.group = group;
    }
    if (image_id && *image_id) {
        reference.image = image_id;
    }

    reference.has_src_x = xml_parser_has_attribute("src_x") != 0;
    reference.has_src_y = xml_parser_has_attribute("src_y") != 0;
    reference.has_x = xml_parser_has_attribute("x") != 0;
    reference.has_y = xml_parser_has_attribute("y") != 0;
    reference.has_width = xml_parser_has_attribute("width") != 0;
    reference.has_height = xml_parser_has_attribute("height") != 0;

    reference.src_x = xml_parser_get_attribute_int("src_x");
    reference.src_y = xml_parser_get_attribute_int("src_y");
    reference.x = xml_parser_get_attribute_int("x");
    reference.y = xml_parser_get_attribute_int("y");
    reference.width = xml_parser_get_attribute_int("width");
    reference.height = xml_parser_get_attribute_int("height");

    const char *part = xml_parser_get_attribute_string("part");
    if (part && *part) {
        reference.part = part;
    }
    const char *mask = xml_parser_get_attribute_string("mask");
    if (mask && *mask) {
        reference.mask = mask;
    }
    const char *invert = xml_parser_get_attribute_string("invert");
    if (invert && *invert) {
        reference.invert = invert;
    }
    const char *rotate = xml_parser_get_attribute_string("rotate");
    if (rotate && *rotate) {
        reference.rotate = rotate;
    }

    reference.is_direct_crop = reference.src.empty() && reference.group.empty();
    destination.push_back(std::move(reference));
}

static bool is_local_reference(const AtlasReference &reference);

static int translate_reference_group_key(const AtlasReference &source_reference, std::string &translated_group_key)
{
    translated_group_key.clear();
    if (source_reference.group.empty() || is_local_reference(source_reference)) {
        return 0;
    }

    return augustus_julius_template_resolver_translate_group_key(source_reference.group, translated_group_key);
}

static void set_output_group_key(OutputGroup &output_group, const std::string &group_key)
{
    output_group.group_key = group_key;

    const size_t separator = group_key.rfind('\\');
    if (separator == std::string::npos) {
        output_group.family_name.clear();
        output_group.group_name = group_key;
    } else {
        output_group.family_name = group_key.substr(0, separator);
        output_group.group_name = group_key.substr(separator + 1);
    }

    output_group.directory_path = append_path_component(make_augustus_graphics_root(), output_group.group_key);
    output_group.xml_path = append_path_component(make_augustus_graphics_root(), output_group.group_key + ".xml");
}

static void add_output_group_alias(OutputGroup &output_group, const std::string &alias_group_key)
{
    if (alias_group_key.empty() || alias_group_key == output_group.group_key) {
        return;
    }
    if (std::find(output_group.alias_group_keys.begin(), output_group.alias_group_keys.end(), alias_group_key) !=
        output_group.alias_group_keys.end()) {
        return;
    }
    output_group.alias_group_keys.push_back(alias_group_key);
}

static std::string choose_canonical_group_key(const AtlasDocument &document, const OutputGroup &output_group)
{
    std::string canonical_group_key;
    int invalid_reference = 0;

    for (int image_index : output_group.image_indices) {
        const AtlasImage &image_data = document.images[image_index];
        if (image_data.materialized) {
            continue;
        }

        auto visit_reference = [&](const AtlasReference &reference) {
            if (invalid_reference == 2) {
                return;
            }
            std::string translated_group_key;
            const int translated = translate_reference_group_key(reference, translated_group_key);
            if (translated < 0) {
                invalid_reference = 1;
                return;
            }
            if (translated == 0) {
                return;
            }
            if (canonical_group_key.empty()) {
                canonical_group_key = std::move(translated_group_key);
                return;
            }
            if (canonical_group_key != translated_group_key) {
                invalid_reference = 2;
                canonical_group_key.clear();
            }
        };

        visit_image_references(image_data, visit_reference);
        if (invalid_reference == 2) {
            break;
        }
    }

    if (invalid_reference == 1) {
        log_info(
            "Falling back to assetlist-derived Augustus output path because a referenced group key was invalid",
            output_group.group_key.c_str(),
            0);
        return {};
    }
    if (invalid_reference == 2) {
        log_info(
            "Falling back to assetlist-derived Augustus output path because wrapper references were ambiguous",
            output_group.group_key.c_str(),
            0);
        return {};
    }
    return canonical_group_key;
}

static const AtlasImage *find_output_group_image(
    const AtlasDocument &document,
    const OutputGroup &output_group,
    const std::string &image_id)
{
    for (int image_index : output_group.image_indices) {
        const AtlasImage &candidate = document.images[image_index];
        if (candidate.id == image_id) {
            return &candidate;
        }
    }
    return nullptr;
}

static int load_reference_template_parts(
    const AtlasReference &reference,
    AugustusJuliusTemplateGroup &template_group,
    std::string &template_image_id)
{
    template_image_id.clear();

    std::string translated_group_key;
    const int translated = augustus_julius_template_resolver_translate_group_image(
        make_julius_graphics_root(),
        reference.group,
        reference.image,
        translated_group_key,
        template_image_id);
    if (translated <= 0) {
        return translated;
    }

    if (!augustus_julius_template_resolver_load_group(make_julius_graphics_root(), translated_group_key, template_group)) {
        return 0;
    }

    return !template_image_id.empty();
}

static int assign_missing_isometric_parts(const AtlasImage &image_data, std::vector<AtlasReference> &output_layers)
{
    if (output_layers.empty() || !image_data.is_isometric) {
        return 1;
    }

    bool has_footprint = false;
    bool has_top = false;
    for (const AtlasReference &reference : output_layers) {
        if (reference.part == "footprint") {
            has_footprint = true;
        } else if (reference.part == "top") {
            has_top = true;
        }
    }

    for (AtlasReference &reference : output_layers) {
        if (!reference.part.empty()) {
            continue;
        }
        if (!has_footprint) {
            reference.part = "footprint";
            has_footprint = true;
        } else {
            reference.part = "top";
            has_top = true;
        }
    }

    if (!has_top && output_layers.size() > 1) {
        for (AtlasReference &reference : output_layers) {
            if (reference.part != "footprint") {
                reference.part = "top";
                has_top = true;
                break;
            }
        }
    }

    if (output_layers.size() == 1 && output_layers[0].part.empty()) {
        output_layers[0].part = "footprint";
    }

    return 1;
}

static int infer_image_parts(
    const AtlasDocument &document,
    const AtlasImage &image_data,
    const OutputGroup &output_group,
    const LocalReferenceTargets &local_targets,
    InferredPartCache &cache,
    std::vector<std::string> &parts);

static int infer_reference_parts(
    const AtlasDocument &document,
    const AtlasReference &reference,
    const OutputGroup &output_group,
    const LocalReferenceTargets &local_targets,
    InferredPartCache &cache,
    std::vector<std::string> &parts)
{
    parts.clear();

    if (!reference.part.empty()) {
        parts.push_back(reference.part);
        return 1;
    }
    if (reference.group.empty()) {
        return 0;
    }
    if (is_local_reference(reference)) {
        const std::string image_id = reference.image.empty() ? std::string("Image_0000") : reference.image;
        const AtlasImage *local_image = find_output_group_image(document, output_group, image_id);
        if (!local_image) {
            return 0;
        }
        return infer_image_parts(document, *local_image, output_group, local_targets, cache, parts);
    }

    AugustusJuliusTemplateGroup template_group;
    std::string template_image_id;
    const int loaded = load_reference_template_parts(reference, template_group, template_image_id);
    if (loaded <= 0) {
        return loaded;
    }

    auto template_it = template_group.images.find(template_image_id);
    if (template_it == template_group.images.end() || template_it->second.parts.empty()) {
        return 0;
    }

    parts = template_it->second.parts;
    return 1;
}

static int build_output_layers(
    const AtlasDocument &document,
    const AtlasImage &image_data,
    const OutputGroup &output_group,
    const LocalReferenceTargets &local_targets,
    InferredPartCache &cache,
    std::vector<AtlasReference> &output_layers)
{
    output_layers.clear();
    output_layers.reserve(image_data.layers.size());

    if (!image_data.is_isometric) {
        output_layers = image_data.layers;
        return 1;
    }

    for (const AtlasReference &reference : image_data.layers) {
        if (!reference.part.empty()) {
            output_layers.push_back(reference);
            continue;
        }

        std::vector<std::string> inherited_parts;
        const int inherited = infer_reference_parts(document, reference, output_group, local_targets, cache, inherited_parts);
        if (inherited < 0) {
            return 0;
        }
        if (inherited > 0 && !inherited_parts.empty()) {
            for (const std::string &part : inherited_parts) {
                AtlasReference duplicated = reference;
                duplicated.part = part;
                output_layers.push_back(std::move(duplicated));
            }
            continue;
        }
        output_layers.push_back(reference);
    }

    return assign_missing_isometric_parts(image_data, output_layers);
}

static int infer_image_parts(
    const AtlasDocument &document,
    const AtlasImage &image_data,
    const OutputGroup &output_group,
    const LocalReferenceTargets &local_targets,
    InferredPartCache &cache,
    std::vector<std::string> &parts)
{
    parts.clear();
    if (!image_data.is_isometric) {
        return 1;
    }

    auto cached = cache.parts_by_image_id.find(image_data.id);
    if (cached != cache.parts_by_image_id.end()) {
        parts = cached->second;
        return 1;
    }

    int &resolution_state = cache.resolution_state_by_image_id[image_data.id];
    if (resolution_state == 1) {
        log_info("Augustus extracted image detected recursive local part inference; falling back to current partial knowledge", image_data.id.c_str(), 0);
        return 0;
    }

    resolution_state = 1;
    std::vector<AtlasReference> output_layers;
    const int built = build_output_layers(document, image_data, output_group, local_targets, cache, output_layers);
    if (!built) {
        resolution_state = 0;
        return 0;
    }

    parts.reserve(output_layers.size());
    for (const AtlasReference &reference : output_layers) {
        if (!reference.part.empty()) {
            parts.push_back(reference.part);
        }
    }

    if (parts.size() == 1 &&
        parts[0] == "footprint" &&
        image_data.layers.size() == 1 &&
        image_data.layers[0].part.empty()) {
        parts.push_back("top");
    }

    cache.parts_by_image_id.emplace(image_data.id, parts);
    resolution_state = 2;
    return 1;
}

static int xml_start_image(void)
{
    AtlasImage image_data;
    const char *id = xml_parser_get_attribute_string("id");
    if (id && *id) {
        image_data.id = id;
        image_data.has_explicit_id = true;
    }
    image_data.synthetic_id = image_data.id.empty();
    image_data.is_isometric = xml_parser_get_attribute_bool("isometric") != 0;
    image_data.has_width = xml_parser_has_attribute("width") != 0;
    image_data.has_height = xml_parser_has_attribute("height") != 0;
    image_data.width = xml_parser_get_attribute_int("width");
    image_data.height = xml_parser_get_attribute_int("height");
    image_data.source_index = g_parse_state.document.images.size();

    g_parse_state.document.images.push_back(std::move(image_data));
    g_parse_state.current_image = &g_parse_state.document.images.back();
    g_parse_state.current_animation = nullptr;

    const char *path = xml_parser_get_attribute_string("src");
    const char *group = xml_parser_get_attribute_string("group");
    const char *image_id = xml_parser_get_attribute_string("image");
    if ((path && *path) || (group && *group)) {
        push_reference(g_parse_state.current_image->layers, path, group, image_id);
    }
    return 1;
}

static int xml_start_layer(void)
{
    if (!g_parse_state.current_image) {
        g_parse_state.error = 1;
        return 0;
    }

    push_reference(
        g_parse_state.current_image->layers,
        xml_parser_get_attribute_string("src"),
        xml_parser_get_attribute_string("group"),
        xml_parser_get_attribute_string("image"));
    return 1;
}

static int xml_start_animation(void)
{
    if (!g_parse_state.current_image) {
        g_parse_state.error = 1;
        return 0;
    }

    AtlasAnimation &animation = g_parse_state.current_image->animation;
    animation.present = true;
    animation.has_frames = xml_parser_has_attribute("frames") != 0;
    animation.frames = xml_parser_get_attribute_int("frames");
    animation.has_speed = xml_parser_has_attribute("speed") != 0;
    animation.speed = xml_parser_get_attribute_int("speed");
    animation.reversible = xml_parser_get_attribute_bool("reversible") != 0;
    animation.has_x = xml_parser_has_attribute("x") != 0;
    animation.x = xml_parser_get_attribute_int("x");
    animation.has_y = xml_parser_has_attribute("y") != 0;
    animation.y = xml_parser_get_attribute_int("y");
    g_parse_state.current_animation = &animation;
    return 1;
}

static int xml_start_frame(void)
{
    if (!g_parse_state.current_animation) {
        g_parse_state.error = 1;
        return 0;
    }

    AtlasAnimationFrame frame;
    std::vector<AtlasReference> frame_reference;
    push_reference(
        frame_reference,
        xml_parser_get_attribute_string("src"),
        xml_parser_get_attribute_string("group"),
        xml_parser_get_attribute_string("image"));
    if (frame_reference.empty()) {
        g_parse_state.error = 1;
        return 0;
    }
    frame.reference = std::move(frame_reference.front());
    g_parse_state.current_animation->frames_data.push_back(std::move(frame));
    return 1;
}

static void xml_end_image(void)
{
    g_parse_state.current_image = nullptr;
    g_parse_state.current_animation = nullptr;
}

static void xml_end_animation(void)
{
    g_parse_state.current_animation = nullptr;
}

static const xml_parser_element kXmlElements[] = {
    { "assetlist", xml_start_assetlist, 0 },
    { "image", xml_start_image, xml_end_image, "assetlist" },
    { "layer", xml_start_layer, 0, "image" },
    { "animation", xml_start_animation, xml_end_animation, "image" },
    { "frame", xml_start_frame, 0, "animation" }
};

static bool parse_document(const std::string &xml_path, AtlasDocument &document)
{
    CrashContextScope crash_scope("augustus_extractor.parse_document", xml_path.c_str());
    std::vector<char> buffer;
    if (!load_file_to_buffer(xml_path, buffer)) {
        log_error("Unable to open Augustus source xml", xml_path.c_str(), 0);
        return false;
    }

    g_parse_state = {};
    g_parse_state.document.xml_path = xml_path;

    if (!xml_parser_init(kXmlElements, static_cast<int>(sizeof(kXmlElements) / sizeof(kXmlElements[0])), 1)) {
        log_error("Unable to initialize Augustus extractor xml parser", xml_path.c_str(), 0);
        return false;
    }

    const int parsed = xml_parser_parse(buffer.data(), static_cast<unsigned int>(buffer.size()), 1);
    xml_parser_free();
    if (!parsed || g_parse_state.error || g_parse_state.document.assetlist_name.empty()) {
        log_error("Failed to parse Augustus source xml", xml_path.c_str(), 0);
        return false;
    }

    int materialized_counter = 0;
    int alias_counter = 0;
    for (AtlasImage &image_data : g_parse_state.document.images) {
        image_data.materialized = image_has_materialized_pixels(image_data);
        if (!image_data.id.empty()) {
            continue;
        }
        char synthetic_name[32];
        if (image_data.materialized) {
            snprintf(synthetic_name, sizeof(synthetic_name), "Image_%04d", materialized_counter++);
        } else {
            snprintf(synthetic_name, sizeof(synthetic_name), "Alias_%04d", alias_counter++);
        }
        image_data.id = synthetic_name;
        image_data.synthetic_id = true;
    }

    document = std::move(g_parse_state.document);
    return true;
}

struct DisjointSet {
    std::vector<int> parent;
    std::vector<int> rank;

    explicit DisjointSet(size_t size)
        : parent(size)
        , rank(size, 0)
    {
        for (size_t i = 0; i < size; ++i) {
            parent[i] = static_cast<int>(i);
        }
    }

    int find(int value)
    {
        if (parent[value] != value) {
            parent[value] = find(parent[value]);
        }
        return parent[value];
    }

    void unite(int a, int b)
    {
        a = find(a);
        b = find(b);
        if (a == b) {
            return;
        }
        if (rank[a] < rank[b]) {
            std::swap(a, b);
        }
        parent[b] = a;
        if (rank[a] == rank[b]) {
            rank[a]++;
        }
    }
};

static void merge_output_group_into(
    const AtlasDocument &document,
    OutputGroup &target_group,
    const OutputGroup &source_group)
{
    std::unordered_set<int> existing_indices(target_group.image_indices.begin(), target_group.image_indices.end());
    std::unordered_set<std::string> existing_image_ids;
    for (int image_index : target_group.image_indices) {
        const std::string &image_id = document.images[image_index].id;
        if (!image_id.empty()) {
            existing_image_ids.insert(image_id);
        }
    }

    for (int image_index : source_group.image_indices) {
        if (!existing_indices.insert(image_index).second) {
            continue;
        }
        const AtlasImage &image_data = document.images[image_index];
        if (!image_data.id.empty() && !existing_image_ids.insert(image_data.id).second) {
            log_info("Augustus extractor duplicate image id in merged group", image_data.id.c_str(), 0);
            continue;
        }
        target_group.image_indices.push_back(image_index);
    }

    add_output_group_alias(target_group, source_group.group_key);
    for (const std::string &alias_group_key : source_group.alias_group_keys) {
        add_output_group_alias(target_group, alias_group_key);
    }
}

static std::vector<OutputGroup> merge_output_groups_by_visible_key(
    const AtlasDocument &document,
    const std::vector<OutputGroup> &groups)
{
    if (groups.empty()) {
        return {};
    }

    DisjointSet disjoint_set(groups.size());
    std::unordered_map<std::string, int> key_owner;
    for (size_t group_index = 0; group_index < groups.size(); ++group_index) {
        const OutputGroup &group = groups[group_index];
        auto register_key = [&](const std::string &group_key) {
            if (group_key.empty()) {
                return;
            }
            const auto inserted = key_owner.emplace(group_key, static_cast<int>(group_index));
            if (!inserted.second) {
                disjoint_set.unite(static_cast<int>(group_index), inserted.first->second);
            }
        };

        register_key(group.group_key);
        for (const std::string &alias_group_key : group.alias_group_keys) {
            register_key(alias_group_key);
        }
    }

    std::vector<OutputGroup> merged_groups;
    std::unordered_map<int, size_t> root_to_group_index;
    for (size_t group_index = 0; group_index < groups.size(); ++group_index) {
        const int root = disjoint_set.find(static_cast<int>(group_index));
        const auto inserted = root_to_group_index.emplace(root, merged_groups.size());
        if (inserted.second) {
            merged_groups.push_back(groups[group_index]);
        } else {
            merge_output_group_into(document, merged_groups[inserted.first->second], groups[group_index]);
        }
    }

    return merged_groups;
}

static bool is_local_reference(const AtlasReference &reference)
{
    return reference.group == "this" || reference.group == "THIS";
}

static void connect_local_reference(
    const AtlasReference &reference,
    int image_index,
    const std::unordered_map<std::string, int> &id_lookup,
    DisjointSet &disjoint_set)
{
    if (!is_local_reference(reference) || reference.image.empty()) {
        return;
    }
    auto it = id_lookup.find(reference.image);
    if (it == id_lookup.end()) {
        return;
    }
    disjoint_set.unite(image_index, it->second);
}

static std::vector<OutputGroup> build_output_groups(const AtlasDocument &document)
{
    if (document.images.empty()) {
        return {};
    }

    std::unordered_map<std::string, int> id_lookup;
    for (size_t index = 0; index < document.images.size(); ++index) {
        id_lookup.emplace(document.images[index].id, static_cast<int>(index));
    }

    DisjointSet disjoint_set(document.images.size());
    for (size_t index = 0; index < document.images.size(); ++index) {
        const AtlasImage &image_data = document.images[index];
        visit_image_references(image_data, [&](const AtlasReference &reference) {
            connect_local_reference(reference, static_cast<int>(index), id_lookup, disjoint_set);
        });
    }

    int last_named_index = -1;
    for (size_t index = 0; index < document.images.size(); ++index) {
        const AtlasImage &image_data = document.images[index];
        if (!image_data.synthetic_id) {
            last_named_index = static_cast<int>(index);
            continue;
        }
        if (last_named_index >= 0) {
            disjoint_set.unite(static_cast<int>(index), last_named_index);
        }
    }

    std::vector<OutputGroup> grouped_output;
    std::unordered_map<int, size_t> root_to_group_index;
    for (size_t index = 0; index < document.images.size(); ++index) {
        const int root = disjoint_set.find(static_cast<int>(index));
        auto it = root_to_group_index.find(root);
        if (it == root_to_group_index.end()) {
            OutputGroup output_group;
            output_group.family_name = document.family_name;
            output_group.image_indices.push_back(static_cast<int>(index));
            root_to_group_index.emplace(root, grouped_output.size());
            grouped_output.push_back(std::move(output_group));
        } else {
            grouped_output[it->second].image_indices.push_back(static_cast<int>(index));
        }
    }

    std::unordered_map<std::string, int> family_name_counts;
    for (size_t group_index = 0; group_index < grouped_output.size(); ++group_index) {
        OutputGroup &output_group = grouped_output[group_index];

        std::string group_name;
        for (int image_index : output_group.image_indices) {
            const AtlasImage &image_data = document.images[image_index];
            if (image_data.synthetic_id) {
                continue;
            }
            group_name = sanitize_component(image_data.id.c_str());
            break;
        }
        if (group_name.empty()) {
            char unnamed_group_name[32];
            snprintf(unnamed_group_name, sizeof(unnamed_group_name), "Group_%04d", static_cast<int>(group_index));
            group_name = unnamed_group_name;
        }

        int &name_count = family_name_counts[group_name];
        if (name_count > 0) {
            char unique_name[FILE_NAME_MAX];
            snprintf(unique_name, sizeof(unique_name), "%s_%02d", group_name.c_str(), name_count + 1);
            output_group.group_name = unique_name;
        } else {
            output_group.group_name = group_name;
        }
        name_count++;

        set_output_group_key(output_group, output_group.family_name + "\\" + output_group.group_name);

        const std::string canonical_group_key = choose_canonical_group_key(document, output_group);
        if (!canonical_group_key.empty() && canonical_group_key != output_group.group_key) {
            add_output_group_alias(output_group, output_group.group_key);
            set_output_group_key(output_group, canonical_group_key);
        }

        for (int image_index : output_group.image_indices) {
            const AtlasImage &image_data = document.images[image_index];
            if (image_data.synthetic_id || image_data.id.empty()) {
                continue;
            }
            add_output_group_alias(
                output_group,
                document.family_name + "\\" + sanitize_component(image_data.id.c_str()));
        }
    }

    return merge_output_groups_by_visible_key(document, grouped_output);
}

static int resolve_crop_width(const AtlasReference &reference, const AtlasImage &image_data)
{
    if (reference.has_width && reference.width > 0) {
        return reference.width;
    }
    if (image_data.has_width && image_data.width > 0) {
        return image_data.width;
    }
    return 0;
}

static int resolve_crop_height(const AtlasReference &reference, const AtlasImage &image_data)
{
    if (reference.has_height && reference.height > 0) {
        return reference.height;
    }
    if (image_data.has_height && image_data.height > 0) {
        return image_data.height;
    }
    return 0;
}

static int count_trailing_transparent_bottom_rows(const color_t *pixels, int width, int height)
{
    if (!pixels || width <= 0 || height <= 0) {
        return 0;
    }

    for (int y = height - 1; y >= 0; --y) {
        const color_t *row = &pixels[static_cast<size_t>(y) * width];
        for (int x = 0; x < width; ++x) {
            if ((row[x] & COLOR_CHANNEL_ALPHA) != ALPHA_TRANSPARENT) {
                return height - y - 1;
            }
        }
    }

    return 0;
}

static bool measure_direct_crop_trim(
    const AtlasImage &image_data,
    const AtlasReference &reference,
    int &trimmed_bottom_rows)
{
    trimmed_bottom_rows = 0;

    const int width = resolve_crop_width(reference, image_data);
    const int height = resolve_crop_height(reference, image_data);
    if (width <= 0 || height <= 0) {
        return false;
    }

    std::vector<color_t> pixels(static_cast<size_t>(width) * height, ALPHA_TRANSPARENT);
    const int src_x = reference.has_src_x ? reference.src_x : 0;
    const int src_y = reference.has_src_y ? reference.src_y : 0;
    if (!png_read(pixels.data(), src_x, src_y, width, height, 0, 0, width, 0)) {
        return false;
    }

    trimmed_bottom_rows = count_trailing_transparent_bottom_rows(pixels.data(), width, height);
    if (trimmed_bottom_rows >= height) {
        trimmed_bottom_rows = 0;
    }
    return true;
}

static bool normalize_direct_crop_footprints(
    const AtlasImage &image_data,
    std::vector<AtlasReference> &output_layers,
    int &image_height_override)
{
    image_height_override = 0;
    if (!image_data.is_isometric || output_layers.empty()) {
        return true;
    }

    int has_top_part = 0;
    int footprint_only_direct_crop = 1;
    int saw_trimmed_footprint = 0;

    for (const AtlasReference &reference : output_layers) {
        if (reference.part == "top") {
            has_top_part = 1;
        }
        if (!reference.is_direct_crop || reference.part != "footprint") {
            footprint_only_direct_crop = 0;
        }
    }

    for (AtlasReference &reference : output_layers) {
        if (!reference.is_direct_crop || reference.part != "footprint") {
            continue;
        }

        int trimmed_bottom_rows = 0;
        if (!measure_direct_crop_trim(image_data, reference, trimmed_bottom_rows)) {
            log_error("Failed to measure Augustus extracted crop", image_data.id.c_str(), 0);
            return false;
        }
        if (trimmed_bottom_rows <= 0) {
            continue;
        }

        const int height = resolve_crop_height(reference, image_data);
        if (height <= trimmed_bottom_rows) {
            continue;
        }

        saw_trimmed_footprint = 1;
        reference.height = height - trimmed_bottom_rows;
        reference.has_height = true;
        if (has_top_part) {
            reference.y = (reference.has_y ? reference.y : 0) + trimmed_bottom_rows;
            reference.has_y = true;
        }
    }

    if (!has_top_part && footprint_only_direct_crop && saw_trimmed_footprint) {
        for (const AtlasReference &reference : output_layers) {
            const int height = resolve_crop_height(reference, image_data);
            const int y = reference.has_y ? reference.y : 0;
            image_height_override = std::max(image_height_override, std::max(0, y) + height);
        }
    }

    return true;
}

static bool write_direct_crop(
    const AtlasImage &image_data,
    const AtlasReference &reference,
    const std::string &output_path,
    ExtractionStats &stats)
{
    CrashContextScope crash_scope("augustus_extractor.write_direct_crop", image_data.id.c_str());
    const int width = resolve_crop_width(reference, image_data);
    const int height = resolve_crop_height(reference, image_data);
    if (width <= 0 || height <= 0) {
        log_error("Augustus extracted crop has invalid size", image_data.id.c_str(), 0);
        return false;
    }

    std::vector<color_t> pixels(static_cast<size_t>(width) * height, ALPHA_TRANSPARENT);
    const int src_x = reference.has_src_x ? reference.src_x : 0;
    const int src_y = reference.has_src_y ? reference.src_y : 0;
    if (!png_read(pixels.data(), src_x, src_y, width, height, 0, 0, width, 0)) {
        log_error("Failed to read Augustus atlas region", image_data.id.c_str(), 0);
        return false;
    }

    if (!write_png(output_path, pixels.data(), width, height)) {
        log_error("Failed to write Augustus extracted png", output_path.c_str(), 0);
        return false;
    }
    stats.pngs_written++;
    return true;
}

static bool translate_reference(
    const AtlasReference &source_reference,
    const OutputGroup &output_group,
    const LocalReferenceTargets &local_targets,
    AtlasReference &translated_reference)
{
    translated_reference = source_reference;
    translated_reference.src.clear();
    translated_reference.is_direct_crop = false;

    if (source_reference.group.empty()) {
        return true;
    }

    if (is_local_reference(source_reference)) {
        if (translated_reference.image.empty()) {
            translated_reference.image = "Image_0000";
        }

        auto owner = local_targets.group_key_by_image_id.find(translated_reference.image);
        if (owner != local_targets.group_key_by_image_id.end() &&
            local_targets.ambiguous_image_ids.find(translated_reference.image) == local_targets.ambiguous_image_ids.end()) {
            translated_reference.group = owner->second;
        } else {
            translated_reference.group = output_group.group_key;
        }
        return true;
    }

    std::string translated_group_key;
    std::string translated_image_id;
    if (augustus_julius_template_resolver_translate_group_image(
            make_julius_graphics_root(),
            source_reference.group,
            source_reference.image,
            translated_group_key,
            translated_image_id) <= 0) {
        log_error("Unable to translate Augustus legacy image reference", source_reference.group.c_str(), 0);
        return false;
    }

    translated_reference.group = translated_group_key;
    translated_reference.image = translated_image_id;
    return true;
}

static void append_reference_attributes(std::string &xml, const AtlasReference &reference)
{
    if (!reference.src.empty()) {
        append_attribute(xml, "src", reference.src);
    }
    if (!reference.group.empty()) {
        append_attribute(xml, "group", reference.group);
    }
    if (!reference.image.empty()) {
        append_attribute(xml, "image", reference.image);
    }
    if (reference.has_x && reference.x != 0) {
        append_attribute(xml, "x", reference.x);
    }
    if (reference.has_y && reference.y != 0) {
        append_attribute(xml, "y", reference.y);
    }
    if (!reference.part.empty()) {
        append_attribute(xml, "part", reference.part);
    }
    if (!reference.mask.empty()) {
        append_attribute(xml, "mask", reference.mask);
    }
    if (!reference.invert.empty()) {
        append_attribute(xml, "invert", reference.invert);
    }
    if (!reference.rotate.empty()) {
        append_attribute(xml, "rotate", reference.rotate);
    }
}

static bool append_reference_xml(
    std::string &xml,
    const AtlasImage &image_data,
    const AtlasReference &reference,
    const OutputGroup &output_group,
    const LocalReferenceTargets &local_targets,
    const char *tag_name,
    const char *file_stem,
    ExtractionStats &stats)
{
    AtlasReference translated_reference;
    if (reference.is_direct_crop) {
        const std::string output_path = append_path_component(output_group.directory_path, std::string(file_stem) + ".png");
        ensure_directory(output_group.directory_path);
        if (!write_direct_crop(image_data, reference, output_path, stats)) {
            return false;
        }
        translated_reference = reference;
        translated_reference.src = file_stem;
        translated_reference.group.clear();
        translated_reference.image.clear();
        translated_reference.is_direct_crop = false;
    } else if (!translate_reference(reference, output_group, local_targets, translated_reference)) {
        return false;
    }

    append_indent(xml, strcmp(tag_name, "frame") == 0 ? 3 : 2);
    xml += "<";
    xml += tag_name;
    append_reference_attributes(xml, translated_reference);
    xml += "/>\n";
    return true;
}

static bool append_image_xml(
    std::string &xml,
    const AtlasDocument &document,
    const AtlasImage &image_data,
    const OutputGroup &output_group,
    const LocalReferenceTargets &local_targets,
    InferredPartCache &part_cache,
    ExtractionStats &stats)
{
    CrashContextScope crash_scope("augustus_extractor.emit_image", image_data.id.c_str());
    const std::string image_stem = sanitize_component(image_data.id.c_str());
    std::vector<AtlasReference> output_layers;
    if (!build_output_layers(document, image_data, output_group, local_targets, part_cache, output_layers)) {
        crash_context_report_error("Augustus extractor could not infer image layer parts", image_data.id.c_str());
        return false;
    }

    int image_height_override = 0;
    if (!normalize_direct_crop_footprints(image_data, output_layers, image_height_override)) {
        crash_context_report_error("Augustus extractor could not normalize direct footprint crops", image_data.id.c_str());
        return false;
    }

    append_indent(xml, 1);
    xml += "<image";
    append_attribute(xml, "id", image_data.id);
    if (image_data.has_width && image_data.width > 0) {
        append_attribute(xml, "width", image_data.width);
    }
    if (image_height_override > 0) {
        append_attribute(xml, "height", image_height_override);
    } else if (image_data.has_height && image_data.height > 0) {
        append_attribute(xml, "height", image_data.height);
    }
    if (image_data.is_isometric) {
        append_attribute(xml, "isometric", "true");
    }
    xml += ">\n";

    for (size_t layer_index = 0; layer_index < output_layers.size(); ++layer_index) {
        char stem[FILE_NAME_MAX];
        if (output_layers.size() == 1 && !image_data.animation.present && image_stem != "unnamed") {
            snprintf(stem, sizeof(stem), "%s", image_stem.c_str());
        } else {
            snprintf(stem, sizeof(stem), "%s_Layer_%02d", image_stem.c_str(), static_cast<int>(layer_index + 1));
        }
        if (!append_reference_xml(
                xml,
                image_data,
                output_layers[layer_index],
                output_group,
                local_targets,
                "layer",
                stem,
                stats)) {
            return false;
        }
    }

    if (image_data.animation.present) {
        append_indent(xml, 2);
        xml += "<animation";
        if (image_data.animation.has_frames) {
            append_attribute(xml, "frames", image_data.animation.frames);
        }
        if (image_data.animation.has_speed) {
            append_attribute(xml, "speed", image_data.animation.speed);
        }
        if (image_data.animation.reversible) {
            append_attribute(xml, "reversible", "true");
        }
        if (image_data.animation.has_x && image_data.animation.x != 0) {
            append_attribute(xml, "x", image_data.animation.x);
        }
        if (image_data.animation.has_y && image_data.animation.y != 0) {
            append_attribute(xml, "y", image_data.animation.y);
        }
        if (image_data.animation.frames_data.empty()) {
            xml += "/>\n";
        } else {
            xml += ">\n";
            for (size_t frame_index = 0; frame_index < image_data.animation.frames_data.size(); ++frame_index) {
                char frame_stem[FILE_NAME_MAX];
                snprintf(frame_stem, sizeof(frame_stem), "%s_Frame_%02d", image_stem.c_str(), static_cast<int>(frame_index + 1));
                if (!append_reference_xml(
                        xml,
                        image_data,
                        image_data.animation.frames_data[frame_index].reference,
                        output_group,
                        local_targets,
                        "frame",
                        frame_stem,
                        stats)) {
                    return false;
                }
            }
            append_indent(xml, 2);
            xml += "</animation>\n";
        }
    }

    append_indent(xml, 1);
    xml += "</image>\n";
    stats.images_exported++;
    return true;
}

static int next_generated_image_index_after_visible_julius(const OutputGroup &output_group)
{
    const std::string julius_graphics_root = make_julius_graphics_root();
    int next_image_index = augustus_julius_template_resolver_next_generated_image_index(
        julius_graphics_root,
        output_group.group_key);
    for (const std::string &alias_group_key : output_group.alias_group_keys) {
        next_image_index = std::max(
            next_image_index,
            augustus_julius_template_resolver_next_generated_image_index(julius_graphics_root, alias_group_key));
    }
    return next_image_index;
}

static void remap_local_reference_image_id(
    AtlasReference &reference,
    const std::unordered_map<std::string, std::string> &image_id_remap)
{
    if (!is_local_reference(reference)) {
        return;
    }

    const std::string lookup_image_id = reference.image.empty() ? std::string("Image_0000") : reference.image;
    auto remapped = image_id_remap.find(lookup_image_id);
    if (remapped != image_id_remap.end()) {
        reference.image = remapped->second;
    }
}

static void offset_generated_image_ids_after_julius(AtlasDocument &document, const OutputGroup &output_group)
{
    int next_image_index = next_generated_image_index_after_visible_julius(output_group);
    if (next_image_index <= 0) {
        return;
    }

    std::unordered_set<std::string> reserved_image_ids;
    for (int image_index : output_group.image_indices) {
        const AtlasImage &image_data = document.images[image_index];
        int generated_index = 0;
        if (!image_data.synthetic_id || !parse_generated_image_index(image_data.id, generated_index)) {
            reserved_image_ids.insert(image_data.id);
        }
    }

    std::unordered_map<std::string, std::string> image_id_remap;
    for (int image_index : output_group.image_indices) {
        AtlasImage &image_data = document.images[image_index];
        int generated_index = 0;
        if (!image_data.synthetic_id || !parse_generated_image_index(image_data.id, generated_index)) {
            continue;
        }

        const std::string old_image_id = image_data.id;
        std::string new_image_id;
        do {
            new_image_id = make_generated_image_id(next_image_index++);
        } while (reserved_image_ids.find(new_image_id) != reserved_image_ids.end());

        reserved_image_ids.insert(new_image_id);
        image_data.id = new_image_id;
        if (old_image_id != new_image_id) {
            image_id_remap.emplace(old_image_id, new_image_id);
        }
    }

    if (image_id_remap.empty()) {
        return;
    }

    for (int image_index : output_group.image_indices) {
        AtlasImage &image_data = document.images[image_index];
        visit_image_references(image_data, [&](AtlasReference &reference) {
            remap_local_reference_image_id(reference, image_id_remap);
        });
    }
}

static LocalReferenceTargets build_local_reference_targets(
    const AtlasDocument &document,
    const std::vector<OutputGroup> &groups)
{
    LocalReferenceTargets targets;
    for (const OutputGroup &output_group : groups) {
        for (int image_index : output_group.image_indices) {
            if (image_index < 0 || image_index >= static_cast<int>(document.images.size())) {
                continue;
            }

            const std::string &image_id = document.images[image_index].id;
            if (image_id.empty()) {
                continue;
            }

            auto inserted = targets.group_key_by_image_id.emplace(image_id, output_group.group_key);
            if (!inserted.second && inserted.first->second != output_group.group_key) {
                targets.ambiguous_image_ids.insert(image_id);
            }
        }
    }
    return targets;
}

static bool export_group(
    const AtlasDocument &document,
    const OutputGroup &output_group,
    const LocalReferenceTargets &local_targets,
    ExtractionStats &stats)
{
    CrashContextScope crash_scope("augustus_extractor.export_group", output_group.group_key.c_str());
    InferredPartCache part_cache;

    std::string xml = "<?xml version=\"1.0\"?>\n<!DOCTYPE assetlist>\n<assetlist";
    append_attribute(xml, "name", output_group.group_key);
    xml += ">\n";

    for (int image_index : output_group.image_indices) {
        if (!append_image_xml(xml, document, document.images[image_index], output_group, local_targets, part_cache, stats)) {
            return false;
        }
    }

    xml += "</assetlist>\n";
    ensure_directory(append_path_component(make_augustus_graphics_root(), output_group.family_name));
    if (!write_text_file(output_group.xml_path, xml)) {
        log_error("Failed to write Augustus extracted xml", output_group.xml_path.c_str(), 0);
        return false;
    }

    stats.groups_exported++;
    return true;
}

static void append_alias_image_xml(std::string &xml, const AtlasImage &image_data, const std::string &target_group_key)
{
    append_indent(xml, 1);
    xml += "<image";
    append_attribute(xml, "id", image_data.id);
    if (image_data.has_width && image_data.width > 0) {
        append_attribute(xml, "width", image_data.width);
    }
    if (image_data.has_height && image_data.height > 0) {
        append_attribute(xml, "height", image_data.height);
    }
    if (image_data.is_isometric) {
        append_attribute(xml, "isometric", "true");
    }
    append_attribute(xml, "group", target_group_key);
    append_attribute(xml, "image", image_data.id);
    xml += "/>\n";
}

static bool export_alias_group(
    const AtlasDocument &document,
    const OutputGroup &target_group,
    const std::string &alias_group_key,
    ExtractionStats &stats)
{
    CrashContextScope crash_scope("augustus_extractor.export_alias_group", alias_group_key.c_str());

    OutputGroup alias_group;
    set_output_group_key(alias_group, alias_group_key);

    std::string xml = "<?xml version=\"1.0\"?>\n<!DOCTYPE assetlist>\n<assetlist";
    append_attribute(xml, "name", alias_group.group_key);
    xml += ">\n";

    for (int image_index : target_group.image_indices) {
        append_alias_image_xml(xml, document.images[image_index], target_group.group_key);
    }

    xml += "</assetlist>\n";
    ensure_directory(append_path_component(make_augustus_graphics_root(), alias_group.family_name));
    if (!write_text_file(alias_group.xml_path, xml)) {
        log_error("Failed to write Augustus extracted alias xml", alias_group.xml_path.c_str(), 0);
        return false;
    }

    stats.groups_exported++;
    return true;
}

static bool export_document(AtlasDocument &document, ExtractionStats &stats)
{
    CrashContextScope crash_scope("augustus_extractor.export_document", document.xml_path.c_str());
    if (!png_load_from_file(document.png_path.c_str(), 0)) {
        log_error("Unable to load Augustus source atlas png", document.png_path.c_str(), 0);
        return false;
    }

    std::vector<OutputGroup> groups = build_output_groups(document);
    for (const OutputGroup &output_group : groups) {
        offset_generated_image_ids_after_julius(document, output_group);
    }
    const LocalReferenceTargets local_targets = build_local_reference_targets(document, groups);

    std::unordered_set<std::string> exported_group_keys;
    for (const OutputGroup &output_group : groups) {
        exported_group_keys.insert(output_group.group_key);
    }

    for (const OutputGroup &output_group : groups) {
        if (!export_group(document, output_group, local_targets, stats)) {
            png_unload();
            return false;
        }
    }

    for (const OutputGroup &output_group : groups) {
        for (const std::string &alias_group_key : output_group.alias_group_keys) {
            if (!exported_group_keys.insert(alias_group_key).second) {
                log_info("Augustus extractor duplicate alias key after merge", alias_group_key.c_str(), 0);
                continue;
            }
            if (!export_alias_group(document, output_group, alias_group_key, stats)) {
                png_unload();
                return false;
            }
        }
    }

    png_unload();
    return true;
}

static bool extract_all_documents(ExtractionStats &stats)
{
    const std::string source_graphics_path = make_source_graphics_path();
    if (source_graphics_path.empty()) {
        log_error("Unable to resolve Augustus source graphics directory", 0, 0);
        return false;
    }

    const dir_listing *xml_files = dir_find_files_with_extension(source_graphics_path.c_str(), "xml");
    std::vector<std::string> xml_names;
    xml_names.reserve(xml_files ? xml_files->num_files : 0);
    for (int i = 0; xml_files && i < xml_files->num_files; ++i) {
        xml_names.emplace_back(xml_files->files[i].name ? xml_files->files[i].name : "");
    }

    for (const std::string &xml_name : xml_names) {
        if (xml_name.empty()) {
            continue;
        }
        const std::string xml_path = append_path_component(source_graphics_path, xml_name);

        AtlasDocument document;
        if (!parse_document(xml_path, document)) {
            return false;
        }

        document.png_path = append_path_component(source_graphics_path, document.assetlist_name + ".png");
        stats.source_files++;
        if (!export_document(document, stats)) {
            return false;
        }
    }

    return true;
}

static int run_extraction(int force, int write_stamp)
{
    const std::string graphics_root = make_augustus_graphics_root();
    const std::string stamp_path = make_stamp_path();
    if (graphics_root.empty()) {
        log_error("Unable to resolve Augustus extracted graphics directory", 0, 0);
        return 0;
    }

    std::string expected_stamp;
    if (!build_expected_stamp(expected_stamp)) {
        log_error("Failed to fingerprint Augustus source graphics", 0, 0);
        return 0;
    }

    std::string existing_stamp;
    const int has_existing_stamp = write_stamp && read_text_file(stamp_path, existing_stamp);
    const int has_current_stamp = write_stamp && has_existing_stamp && existing_stamp == expected_stamp;
    if (!force && has_current_stamp &&
        platform_file_manager_list_directory_contents(
            graphics_root.c_str(), TYPE_DIR | TYPE_FILE, 0, stop_on_first_entry) == LIST_MATCH) {
        return 1;
    }

    if (force) {
        log_info("Bootstrapping Augustus graphics because extraction was forced", 0, 0);
    } else if (!has_existing_stamp) {
        log_info("Bootstrapping Augustus graphics because no extraction stamp was found", 0, 0);
    } else if (!has_current_stamp) {
        log_info("Bootstrapping Augustus graphics because the source fingerprint or XML metadata version changed", 0, 0);
    } else {
        log_info("Bootstrapping Augustus graphics because the fallback directory is missing or empty", 0, 0);
    }

    platform_file_manager_remove_directory(graphics_root.c_str());

    ExtractionStats stats;
    augustus_julius_template_resolver_clear_cache();
    log_info("Extracting canonical Augustus graphics from packed source atlases", 0, 0);
    if (!extract_all_documents(stats)) {
        log_error("Augustus graphics extraction failed", 0, 0);
        return 0;
    }
    if (write_stamp) {
        ensure_directory(graphics_root);
        if (!write_text_file(stamp_path, expected_stamp)) {
            log_error("Failed to write Augustus extraction stamp", stamp_path.c_str(), 0);
            return 0;
        }
    }

    char summary[256];
    snprintf(
        summary,
        sizeof(summary),
        "%d source atlases, %d groups, %d images, %d pngs",
        stats.source_files,
        stats.groups_exported,
        stats.images_exported,
        stats.pngs_written);
    log_info("Augustus graphics extraction completed", summary, 0);
    return 1;
}

} // namespace

extern "C" int augustus_asset_extractor_bootstrap(void)
{
    return run_extraction(0, 1);
}

extern "C" int augustus_asset_extractor_extract_with_config(const augustus_asset_extractor_config *config)
{
    ExtractionPaths paths;
    if (!build_extraction_paths(config, paths)) {
        log_error("Invalid Augustus extraction paths", 0, 0);
        return 0;
    }

    ScopedExtractionPaths scoped_paths(&paths);
    return run_extraction(config ? config->force : 0, config ? config->write_stamp : 1);
}
