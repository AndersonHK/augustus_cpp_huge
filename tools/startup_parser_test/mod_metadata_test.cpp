#include "mod_metadata_test.h"

#include "game/mod_manager.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <ostream>
#include <string>
#include <unordered_map>
#include <unordered_set>

namespace {

std::string normalized_name(const std::string &name)
{
    std::string normalized = name;
    for (char &character : normalized) {
        if (character >= 'A' && character <= 'Z') character = static_cast<char>(character - 'A' + 'a');
    }
    return normalized;
}

bool validate_source_schema(std::ostream &errors)
{
    constexpr const char *valid_source =
        "<mod>"
        "<name value=\"Elven Tavern\"/>"
        "<description value=\"Replaces the Roman tavern.\"/>"
        "<version value=\"1.2.3\"/>"
        "<dependencies><mod name=\"Julius\"/><mod name=\"Augustus\"/></dependencies>"
        "</mod>";
    mod_manager::ModMetadata metadata;
    if (!mod_manager::parse_metadata_source_for_test(valid_source, metadata) ||
        metadata.name != "Elven Tavern" || metadata.description != "Replaces the Roman tavern." ||
        metadata.version != "1.2.3" || metadata.dependencies.size() != 2 ||
        metadata.dependencies[0] != "Julius" || metadata.dependencies[1] != "Augustus") {
        errors << "Valid mod metadata did not preserve its declared fields.\n";
        return false;
    }

    constexpr const char *missing_description =
        "<mod><name value=\"Example\"/><version value=\"1\"/><dependencies/></mod>";
    constexpr const char *duplicate_dependency =
        "<mod><name value=\"Example\"/><description value=\"Example\"/><version value=\"1\"/>"
        "<dependencies><mod name=\"Julius\"/><mod name=\"julius\"/></dependencies></mod>";
    constexpr const char *self_dependency =
        "<mod><name value=\"Example\"/><description value=\"Example\"/><version value=\"1\"/>"
        "<dependencies><mod name=\"Example\"/></dependencies></mod>";
    constexpr const char *unknown_element =
        "<mod><name value=\"Example\"/><description value=\"Example\"/><version value=\"1\"/>"
        "<dependencies/><author value=\"Unknown\"/></mod>";
    if (mod_manager::parse_metadata_source_for_test(missing_description, metadata) ||
        mod_manager::parse_metadata_source_for_test(duplicate_dependency, metadata) ||
        mod_manager::parse_metadata_source_for_test(self_dependency, metadata) ||
        mod_manager::parse_metadata_source_for_test(unknown_element, metadata)) {
        errors << "Invalid mod metadata was accepted.\n";
        return false;
    }
    return true;
}

std::string normalized_xml_source(const std::filesystem::path &path)
{
    std::ifstream stream(path, std::ios::binary);
    if (!stream) return {};
    const std::string source{std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>()};
    std::string normalized;
    normalized.reserve(source.size());
    bool quoted = false;
    char quote = 0;
    for (std::size_t index = 0; index < source.size();) {
        if (!quoted && source.compare(index, 2, "<?") == 0) {
            const std::size_t end = source.find("?>", index + 2);
            if (end == std::string::npos) return {};
            index = end + 2;
            continue;
        }
        if (!quoted && source.compare(index, 4, "<!--") == 0) {
            const std::size_t end = source.find("-->", index + 4);
            if (end == std::string::npos) return {};
            index = end + 3;
            continue;
        }
        const char character = source[index++];
        if (character == '"' || character == '\'') {
            if (!quoted) {
                quoted = true;
                quote = character;
            } else if (quote == character) {
                quoted = false;
            }
            normalized.push_back(character);
            continue;
        }
        if (!quoted && std::isspace(static_cast<unsigned char>(character))) {
            while (index < source.size() && std::isspace(static_cast<unsigned char>(source[index]))) index++;
            const char previous = normalized.empty() ? 0 : normalized.back();
            const char next = index < source.size() ? source[index] : 0;
            if (previous && previous != '>' && previous != '<' && next && next != '<' && next != '>') {
                normalized.push_back(' ');
            }
            continue;
        }
        normalized.push_back(character);
    }
    return normalized;
}

bool definitions_are_equivalent(const std::filesystem::path &left, const std::filesystem::path &right)
{
    const std::string left_source = normalized_xml_source(left);
    return !left_source.empty() && left_source == normalized_xml_source(right);
}

bool is_authored_definition_file(const std::filesystem::path &relative_path)
{
    if (relative_path.empty()) return false;
    const std::string first_component = normalized_name(relative_path.begin()->string());
    return first_component != "graphics" && normalized_name(relative_path.filename().string()) != "mod.xml";
}

bool validate_sparse_active_mod_trees(std::ostream &errors)
{
    std::unordered_map<std::string, std::filesystem::path> inherited_files;
    const auto &paths = mod_manager::mod_paths();
    for (std::size_t layer = 0; layer < paths.size(); ++layer) {
        const std::filesystem::path root(paths[layer]);
        std::error_code error;
        if (!std::filesystem::is_directory(root, error)) continue;

        std::filesystem::recursive_directory_iterator iterator(root, error);
        const std::filesystem::recursive_directory_iterator end;
        while (!error && iterator != end) {
            if (iterator->is_regular_file(error)) {
                const std::filesystem::path relative_path = iterator->path().lexically_relative(root);
                if (is_authored_definition_file(relative_path)) {
                    const std::string key = normalized_name(relative_path.generic_string());
                    const auto lower = inherited_files.find(key);
                    if (lower != inherited_files.end() && definitions_are_equivalent(lower->second, iterator->path())) {
                        errors << "Redundant definition duplicates its inherited winner: "
                               << iterator->path().generic_string() << " duplicates "
                               << lower->second.generic_string() << "\n";
                        return false;
                    }
                    inherited_files[key] = iterator->path();
                }
            }
            iterator.increment(error);
        }
        if (error) {
            errors << "Unable to inspect active mod tree " << root.generic_string() << ": " << error.message() << "\n";
            return false;
        }
    }
    return true;
}

} // namespace

bool validate_mod_metadata_contract(std::ostream &errors)
{
    if (!validate_source_schema(errors)) return false;

    const std::vector<std::string> sparse_stack_names = {
        "Julius", "Augustus", "Vespasian", "Elven Tavern"
    };
    std::vector<mod_manager::ModMetadata> sparse_stack = {
        {"Julius", "Base", "1", {}},
        {"Augustus", "Gameplay", "1", {"Julius"}},
        {"Vespasian", "Gameplay", "1", {"Augustus"}},
        {"Elven Tavern", "Sparse patch", "1", {"Vespasian"}}
    };
    std::string stack_failure;
    if (!mod_manager::metadata_stack_is_valid_for_test(sparse_stack_names, sparse_stack, &stack_failure)) {
        errors << "A valid four-mod dependency stack was rejected: " << stack_failure << "\n";
        return false;
    }
    sparse_stack.back().dependencies = {"Missing Mod"};
    if (mod_manager::metadata_stack_is_valid_for_test(sparse_stack_names, sparse_stack, &stack_failure) ||
        stack_failure.find("Missing Mod") == std::string::npos) {
        errors << "A sparse fourth mod was allowed to omit its declared prerequisite.\n";
        return false;
    }

    mod_manager::set_mod_name("Vespasian");
    if (!mod_manager::load_mod_list()) {
        errors << "Unable to load installed mod metadata: " << mod_manager::failure_reason() << "\n";
        return false;
    }

    const auto &names = mod_manager::mod_names();
    const auto &metadata = mod_manager::metadata();
    if (metadata.size() != names.size() || metadata.empty() ||
        !mod_manager::selected_metadata() || mod_manager::selected_metadata() != &metadata.back()) {
        errors << "Loaded mod metadata does not align with the active mod stack.\n";
        return false;
    }

    std::unordered_set<std::string> earlier_mods;
    for (std::size_t i = 0; i < metadata.size(); ++i) {
        if (metadata[i].name != names[i] || metadata[i].description.empty() || metadata[i].version.empty()) {
            errors << "Loaded mod metadata has missing or mismatched identity fields.\n";
            return false;
        }
        for (const std::string &dependency : metadata[i].dependencies) {
            if (earlier_mods.find(normalized_name(dependency)) == earlier_mods.end()) {
                errors << "Loaded mod metadata has an unsatisfied dependency.\n";
                return false;
            }
        }
        earlier_mods.insert(normalized_name(metadata[i].name));
    }
    return validate_sparse_active_mod_trees(errors);
}
