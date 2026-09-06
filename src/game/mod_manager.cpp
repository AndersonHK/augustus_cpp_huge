#include "game/mod_manager.h"
#include "game/mod_content.h"
#include "core/crash_context.h"

#include "core/file.h"
#include "core/log.h"
#include "platform/file_manager.h"
#include "assets/assets.h"
#include "core/dir.h"
#include "core/xml_definition.h"
#include "core/xml_parser.h"
#include "core/xml_value.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <string>
#include <string_view>
#include <unordered_set>
#include <utility>
#include <vector>

namespace {

constexpr const char *kDefaultModListFileName = "mod-list";
constexpr const char *kDefaultModListXml =
    "<mod_list>\r\n"
    "  <mod name=\"Julius\" />\r\n"
    "  <mod name=\"Augustus\" />\r\n"
    "  <mod name=\"Vespasian\" />\r\n"
    "</mod_list>\r\n";

struct ModListParseState {
    std::vector<std::string> mods;
    bool error = false;
    bool saw_root = false;
};

struct ModMetadataParseState {
    mod_manager::ModMetadata metadata;
    bool error = false;
    bool saw_root = false;
    bool saw_name = false;
    bool saw_description = false;
    bool saw_version = false;
    bool saw_dependencies = false;
};

std::string g_mod_name = "Vespasian";
std::string g_mod_path = "Mods/Vespasian/";
std::string g_graphics_path = "Mods/Vespasian/Graphics/";
std::string g_augustus_graphics_path = "Mods/Augustus/Graphics/";
std::string g_julius_graphics_path = "Mods/Julius/Graphics/";
std::vector<std::string> g_mod_names = { "Julius", "Augustus", "Vespasian" };
std::vector<std::string> g_mod_paths = { "Mods/Julius/", "Mods/Augustus/", "Mods/Vespasian/" };
std::vector<std::string> g_graphics_paths = {
    "Mods/Julius/Graphics/",
    "Mods/Augustus/Graphics/",
    "Mods/Vespasian/Graphics/"
};
std::vector<mod_manager::ModMetadata> g_mod_metadata;
std::string g_failure_reason;
ModListParseState g_parse_state;
ModMetadataParseState g_metadata_parse_state;

static int stop_on_first_entry(const char *name, long unused)
{
    (void) name;
    (void) unused;
    return LIST_MATCH;
}

static int validate_directory_path(const char *path)
{
    return static_cast<int>(static_cast<bool>(
        platform_file_manager_list_directory_contents(path, TYPE_DIR | TYPE_FILE, 0, stop_on_first_entry) != LIST_ERROR));
}

static void set_failure_reason(const char *message, const char *detail = nullptr)
{
    g_failure_reason = xml_definition::format_failure_reason(message, detail);
}

static std::string build_mod_path(const std::string &mod_name)
{
    return "Mods/" + mod_name + "/";
}

static std::string build_graphics_path(const std::string &mod_name)
{
    return build_mod_path(mod_name) + "Graphics/";
}

static std::string normalized_mod_name(std::string_view name)
{
    std::string normalized(name);
    for (char &ch : normalized) {
        if (ch >= 'A' && ch <= 'Z') {
            ch = static_cast<char>(ch - 'A' + 'a');
        }
    }
    return normalized;
}

static bool valid_mod_name(std::string_view name)
{
    if (name.empty() || name == "." || name == "..") return false;
    for (const unsigned char character : name) {
        if (character < 32 || character == '/' || character == '\\' || character == ':' ||
            character == '<' || character == '>' || character == '"' || character == '|' ||
            character == '?' || character == '*') return false;
    }
    return true;
}

static void rebuild_legacy_selected_mod_paths()
{
    if (!g_mod_names.empty()) {
        g_mod_name = g_mod_names.back();
    } else if (g_mod_name.empty()) {
        g_mod_name = "Vespasian";
    }

    g_mod_path = build_mod_path(g_mod_name);
    g_graphics_path = build_graphics_path(g_mod_name);
}

static void rebuild_mod_lists()
{
    g_mod_paths.clear();
    g_graphics_paths.clear();
    for (const std::string &mod_name : g_mod_names) {
        g_mod_paths.push_back(build_mod_path(mod_name));
        g_graphics_paths.push_back(build_graphics_path(mod_name));
    }
    rebuild_legacy_selected_mod_paths();
}

static int parse_mod_list_root()
{
    if (g_parse_state.saw_root) {
        g_parse_state.error = 1;
        log_error("Duplicate mod list root node", 0, 0);
        return 0;
    }

    g_parse_state.saw_root = 1;
    return 1;
}

static int parse_mod_entry()
{
    if (!xml_parser_has_attribute("name")) {
        g_parse_state.error = 1;
        log_error("Mod list entry is missing required attribute 'name'", 0, 0);
        return 0;
    }

    std::string mod_name = xml_value::trim_copy(xml_parser_get_attribute_string("name"));
    if (!valid_mod_name(mod_name)) {
        g_parse_state.error = 1;
        log_error("Mod list entry has an invalid name", 0, 0);
        return 0;
    }

    g_parse_state.mods.push_back(std::move(mod_name));
    return 1;
}

static const xml_parser_element XML_ELEMENTS[] = {
    { "mod_list", parse_mod_list_root, nullptr, nullptr, nullptr },
    { "mod", parse_mod_entry, nullptr, "mod_list", nullptr }
};

static int parse_metadata_root()
{
    if (g_metadata_parse_state.saw_root) {
        g_metadata_parse_state.error = 1;
        log_error("Duplicate mod metadata root node", 0, 0);
        return 0;
    }
    g_metadata_parse_state.saw_root = 1;
    return 1;
}

static int parse_metadata_value()
{
    const char *element = xml_parser_get_current_element_name();
    if (!element || !xml_parser_has_attribute("value")) {
        g_metadata_parse_state.error = 1;
        log_error("Mod metadata field is missing required attribute 'value'", element, 0);
        return 0;
    }
    std::string value = xml_value::trim_copy(xml_parser_get_attribute_string("value"));
    if (value.empty()) {
        g_metadata_parse_state.error = 1;
        log_error("Mod metadata field has an empty value", element, 0);
        return 0;
    }

    bool *seen = nullptr;
    std::string *target = nullptr;
    if (strcmp(element, "name") == 0) {
        seen = &g_metadata_parse_state.saw_name;
        target = &g_metadata_parse_state.metadata.name;
    } else if (strcmp(element, "description") == 0) {
        seen = &g_metadata_parse_state.saw_description;
        target = &g_metadata_parse_state.metadata.description;
    } else if (strcmp(element, "version") == 0) {
        seen = &g_metadata_parse_state.saw_version;
        target = &g_metadata_parse_state.metadata.version;
    }
    if (!seen || !target || *seen) {
        g_metadata_parse_state.error = 1;
        log_error("Duplicate or unsupported mod metadata field", element, 0);
        return 0;
    }
    *seen = 1;
    *target = std::move(value);
    return 1;
}

static int parse_metadata_dependencies()
{
    if (g_metadata_parse_state.saw_dependencies) {
        g_metadata_parse_state.error = 1;
        log_error("Duplicate mod metadata dependencies node", 0, 0);
        return 0;
    }
    g_metadata_parse_state.saw_dependencies = 1;
    return 1;
}

static int parse_metadata_dependency()
{
    if (!xml_parser_has_attribute("name")) {
        g_metadata_parse_state.error = 1;
        log_error("Mod dependency is missing required attribute 'name'", 0, 0);
        return 0;
    }
    std::string dependency = xml_value::trim_copy(xml_parser_get_attribute_string("name"));
    if (!valid_mod_name(dependency)) {
        g_metadata_parse_state.error = 1;
        log_error("Mod dependency has an invalid name", dependency.c_str(), 0);
        return 0;
    }
    const std::string normalized = normalized_mod_name(dependency);
    for (const std::string &existing : g_metadata_parse_state.metadata.dependencies) {
        if (normalized_mod_name(existing) == normalized) {
            g_metadata_parse_state.error = 1;
            log_error("Duplicate mod dependency", dependency.c_str(), 0);
            return 0;
        }
    }
    g_metadata_parse_state.metadata.dependencies.push_back(std::move(dependency));
    return 1;
}

static const xml_parser_element MOD_METADATA_XML_ELEMENTS[] = {
    { "mod", parse_metadata_root, nullptr, nullptr, nullptr },
    { "name", parse_metadata_value, nullptr, "mod", nullptr },
    { "description", parse_metadata_value, nullptr, "mod", nullptr },
    { "version", parse_metadata_value, nullptr, "mod", nullptr },
    { "dependencies", parse_metadata_dependencies, nullptr, "mod", nullptr },
    { "mod", parse_metadata_dependency, nullptr, "dependencies", nullptr }
};

[[maybe_unused]] static int finish_metadata_parse(mod_manager::ModMetadata &metadata_out)
{
    if (g_metadata_parse_state.error || !g_metadata_parse_state.saw_root ||
        !g_metadata_parse_state.saw_name || !g_metadata_parse_state.saw_description ||
        !g_metadata_parse_state.saw_version || !g_metadata_parse_state.saw_dependencies) {
        return 0;
    }
    if (!valid_mod_name(g_metadata_parse_state.metadata.name)) {
        log_error("Mod metadata has an invalid name", g_metadata_parse_state.metadata.name.c_str(), 0);
        return 0;
    }
    const std::string own_name = normalized_mod_name(g_metadata_parse_state.metadata.name);
    for (const std::string &dependency : g_metadata_parse_state.metadata.dependencies) {
        if (normalized_mod_name(dependency) == own_name) {
            log_error("Mod metadata cannot depend on itself", dependency.c_str(), 0);
            return 0;
        }
    }
    metadata_out = std::move(g_metadata_parse_state.metadata);
    return 1;
}

static int parse_mod_metadata_file(const char *filename, mod_manager::ModMetadata &metadata_out)
{
    try {
        const auto parsed = mod_content::manifest(mod_content::utf8_path(filename));
        metadata_out = {parsed.name, parsed.description, parsed.version, parsed.dependencies};
    } catch (const std::exception &e) {
        error_context_report_error("Invalid mod metadata XML", e.what());
        return 0;
    }
    return 1;
}

static int parse_mod_list_file(const char *filename, std::vector<std::string> &mods_out)
{
    g_parse_state = {};
    const ErrorContextScope scope("Mod list XML", filename);
    const int parsed = xml_definition::parse_file(
        filename,
        "Mod list",
        XML_ELEMENTS,
        static_cast<int>(sizeof(XML_ELEMENTS) / sizeof(XML_ELEMENTS[0])));

    if (!parsed || g_parse_state.error || !g_parse_state.saw_root || g_parse_state.mods.empty()) {
        error_context_report_error("Invalid mod list XML", filename);
        set_failure_reason("Failed to load mod list.", filename);
        return 0;
    }

    mods_out = std::move(g_parse_state.mods);
    return 1;
}

static int write_default_mod_list()
{
    const char *filename = dir_append_location(kDefaultModListFileName, PATH_LOCATION_CONFIG);
    if (!filename || !*filename) {
        set_failure_reason("Failed to create default mod list.", "Config directory is unavailable.");
        return 0;
    }

    FILE *fp = file_open(filename, "wb");
    if (!fp) {
        log_error("Unable to create default mod list file", filename, 0);
        set_failure_reason("Failed to create default mod list.", filename);
        return 0;
    }

    const size_t xml_length = strlen(kDefaultModListXml);
    const size_t written = fwrite(kDefaultModListXml, 1, xml_length, fp);
    file_close(fp);

    if (written != xml_length) {
        log_error("Unable to write default mod list file", filename, 0);
        set_failure_reason("Failed to create default mod list.", filename);
        return 0;
    }

    return 1;
}

static int ensure_mod_list_file_exists()
{
    if (dir_get_file_at_location(kDefaultModListFileName, PATH_LOCATION_CONFIG)) {
        return 1;
    }

    return write_default_mod_list();
}

static int validate_loaded_mod_names(const std::vector<std::string> &mods)
{
    if (mods.empty()) {
        set_failure_reason("Mod list must contain at least one mod.");
        return 0;
    }

    std::unordered_set<std::string> normalized_names;
    for (const std::string &mod_name : mods) {
        if (!valid_mod_name(mod_name)) {
            set_failure_reason("Mod list contains an invalid mod name.", mod_name.c_str());
            return 0;
        }
        if (!normalized_names.insert(normalized_mod_name(mod_name)).second) {
            set_failure_reason("Mod list contains duplicate mods.", mod_name.c_str());
            return 0;
        }

        const std::string mod_path = build_mod_path(mod_name);
        if (!validate_directory_path(mod_path.c_str())) {
            set_failure_reason("Listed mod folder was not found.", mod_path.c_str());
            return 0;
        }
    }

    return 1;
}

static bool validate_mod_metadata_stack(
    const std::vector<std::string> &mods,
    const std::vector<mod_manager::ModMetadata> &metadata,
    std::string *failure_reason)
{
    std::unordered_set<std::string> earlier_mods;
    if (mods.size() != metadata.size()) {
        if (failure_reason) *failure_reason = "The active mod stack and metadata list have different lengths.";
        return false;
    }
    for (std::size_t index = 0; index < mods.size(); ++index) {
        if (metadata[index].name != mods[index]) {
            if (failure_reason) {
                *failure_reason = xml_definition::format_failure_reason(
                    "Mod metadata name must exactly match its mod folder and mod-list entry.", mods[index].c_str());
            }
            return false;
        }
        for (const std::string &dependency : metadata[index].dependencies) {
            if (earlier_mods.find(normalized_mod_name(dependency)) == earlier_mods.end()) {
                if (failure_reason) {
                    *failure_reason = xml_definition::format_failure_reason(
                        "Mod dependency is missing or ordered after its dependent mod.", dependency.c_str());
                }
                return false;
            }
        }
        earlier_mods.insert(normalized_mod_name(metadata[index].name));
    }
    return true;
}

static bool load_and_validate_mod_metadata(
    const std::vector<std::string> &mods,
    std::vector<mod_manager::ModMetadata> &metadata_out)
{
    std::vector<mod_manager::ModMetadata> loaded_metadata;
    loaded_metadata.reserve(mods.size());
    for (const std::string &mod_name : mods) {
        const std::string filename = build_mod_path(mod_name) + "mod.xml";
        mod_manager::ModMetadata metadata;
        if (!parse_mod_metadata_file(filename.c_str(), metadata)) {
            set_failure_reason("Failed to load required mod metadata.", filename.c_str());
            return false;
        }
        loaded_metadata.push_back(std::move(metadata));
    }

    std::string validation_failure;
    if (!validate_mod_metadata_stack(mods, loaded_metadata, &validation_failure)) {
        g_failure_reason = std::move(validation_failure);
        return false;
    }
    metadata_out = std::move(loaded_metadata);
    return true;
}

static bool names_equal_case_insensitive(std::string_view left, std::string_view right)
{
    return normalized_mod_name(left) == normalized_mod_name(right);
}

static bool select_mod_stack(std::vector<std::string> &mods, std::string_view selected_mod)
{
    const auto selected = std::find_if(mods.begin(), mods.end(), [selected_mod](const std::string &mod) {
        return names_equal_case_insensitive(mod, selected_mod);
    });
    if (selected == mods.end()) {
        const std::string selected_name(selected_mod);
        set_failure_reason("Selected mod is not present in the ordered mod list.", selected_name.c_str());
        return false;
    }
    mods.erase(selected + 1, mods.end());
    return true;
}

} // namespace

namespace mod_manager {

void set_mod_name(std::string_view mod_name)
{
    if (!mod_name.empty()) {
        g_mod_name = mod_name;
    } else {
        g_mod_name = "Vespasian";
    }
    g_mod_path = build_mod_path(g_mod_name);
    g_graphics_path = build_graphics_path(g_mod_name);
}

bool load_mod_list()
{
    g_failure_reason.clear();

    if (!ensure_mod_list_file_exists()) {
        return false;
    }

    const char *filename = dir_get_file_at_location(kDefaultModListFileName, PATH_LOCATION_CONFIG);
    if (!filename || !*filename) {
        set_failure_reason("Failed to load mod list.", "Config mod-list file was not found.");
        return false;
    }

    std::vector<std::string> loaded_mods;
    if (!parse_mod_list_file(filename, loaded_mods)) {
        return false;
    }

    if (!select_mod_stack(loaded_mods, g_mod_name) || !validate_loaded_mod_names(loaded_mods)) {
        return false;
    }

    std::vector<ModMetadata> loaded_metadata;
    if (!load_and_validate_mod_metadata(loaded_mods, loaded_metadata)) {
        return false;
    }

    try {
        std::vector<mod_content::Layer> layers;
        for (const auto &name : loaded_mods) layers.push_back({name, mod_content::utf8_path(build_mod_path(name))});
        mod_content::Session compiled;
        compiled.load(layers, mod_content::utf8_path(dir_append_location("mod-settings.xml", PATH_LOCATION_CONFIG)));
        mod_content::runtime() = std::move(compiled);
    } catch (const std::exception &e) {
        set_failure_reason("Failed to resolve mod settings and fields.", e.what());
        return false;
    }

    g_mod_names = std::move(loaded_mods);
    g_mod_metadata = std::move(loaded_metadata);
    rebuild_mod_lists();
    return true;
}

const std::string &failure_reason()
{
    return g_failure_reason;
}

const std::string &mod_name()
{
    return g_mod_name;
}

const std::string &mod_path()
{
    return g_mod_path;
}

const std::string &graphics_path()
{
    return g_graphics_path;
}

const std::string &augustus_graphics_path()
{
    return g_augustus_graphics_path;
}

const std::string &julius_graphics_path()
{
    return g_julius_graphics_path;
}

const std::vector<std::string> &mod_names()
{
    return g_mod_names;
}

const std::vector<std::string> &mod_paths()
{
    return g_mod_paths;
}

const std::vector<std::string> &graphics_paths()
{
    return g_graphics_paths;
}

const std::vector<ModMetadata> &metadata()
{
    return g_mod_metadata;
}

const ModMetadata *selected_metadata()
{
    return g_mod_metadata.empty() ? nullptr : &g_mod_metadata.back();
}

bool validate_mod_path()
{
    return validate_directory_path(g_mod_path.c_str());
}

bool validate_graphics_path()
{
    for (const std::string &graphics_path : g_graphics_paths) {
        if (validate_directory_path(graphics_path.c_str())) {
            return true;
        }
    }
    return false;
}

#ifdef STARTUP_PARSER_TEST
bool parse_metadata_source_for_test(const char *source, ModMetadata &metadata_out)
{
    g_metadata_parse_state = {};
    if (!source || !*source || !xml_parser_init(
        MOD_METADATA_XML_ELEMENTS,
        static_cast<int>(sizeof(MOD_METADATA_XML_ELEMENTS) / sizeof(MOD_METADATA_XML_ELEMENTS[0])),
        1)) {
        return false;
    }
    const int parsed = xml_parser_parse(source, static_cast<unsigned int>(strlen(source)), 1);
    xml_parser_free();
    return parsed && finish_metadata_parse(metadata_out);
}

bool metadata_stack_is_valid_for_test(
    const std::vector<std::string> &mod_names,
    const std::vector<ModMetadata> &metadata,
    std::string *failure_reason)
{
    if (failure_reason) failure_reason->clear();
    return validate_mod_metadata_stack(mod_names, metadata, failure_reason);
}
#endif

}
