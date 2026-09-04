#include "vespasian_graphics_source_contract_test.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <iterator>
#include <ostream>
#include <set>
#include <string>
#include <vector>

namespace {

std::string lower(std::string value)
{
    std::transform(value.begin(), value.end(), value.begin(),
        [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return value;
}

bool canonical_name(const std::string &value, bool allow_underscores)
{
    size_t start = 0;
    while (start < value.size()) {
        const size_t end = allow_underscores ? value.find('_', start) : std::string::npos;
        const std::string word = value.substr(start, end == std::string::npos ? end : end - start);
        if (word.empty() || !std::isupper(static_cast<unsigned char>(word.front())) ||
            !std::all_of(word.begin(), word.end(), [](unsigned char c) { return std::isalnum(c); })) return false;
        if (end == std::string::npos) return true;
        start = end + 1;
    }
    return false;
}

bool forbidden_artifact_name(const std::string &name)
{
    const std::string folded = lower(name);
    if (folded == "source" || folded == "sequences" || folded.find("manifest") != std::string::npos ||
        folded.find("stamp") != std::string::npos || folded.find("atlas") != std::string::npos) return true;
    return folded.rfind("group_", 0) == 0 && folded.size() > 6 &&
        std::all_of(folded.begin() + 6, folded.end(), [](unsigned char c) { return std::isdigit(c); });
}

} // namespace

bool validate_vespasian_graphics_source_contract(const std::filesystem::path &graphics_root, std::ostream &errors)
{
    std::error_code error;
    if (!std::filesystem::is_directory(graphics_root, error) || error) {
        errors << "Vespasian graphics source root is missing: " << graphics_root.string() << ".\n";
        return false;
    }

    bool valid = true;
    std::set<std::string> group_keys;
    std::set<std::string> declared_group_keys;
    std::vector<std::filesystem::path> files;
    for (std::filesystem::recursive_directory_iterator current(graphics_root, error), end;
         !error && current != end; current.increment(error)) {
        const std::filesystem::path relative = std::filesystem::relative(current->path(), graphics_root, error);
        if (error) break;
        for (const std::filesystem::path &component : relative) {
            if (forbidden_artifact_name(component.string())) {
                errors << "Forbidden generated graphics path component: " << relative.string() << ".\n";
                valid = false;
            }
        }
        if (current->is_regular_file(error)) files.push_back(current->path());
    }
    if (error) {
        errors << "Unable to enumerate Vespasian graphics source: " << error.message() << ".\n";
        return false;
    }
    std::sort(files.begin(), files.end());

    for (const std::filesystem::path &file : files) {
        const std::filesystem::path relative = std::filesystem::relative(file, graphics_root, error);
        if (error) {
            errors << "Unable to normalize Vespasian graphics path: " << file.string() << ".\n";
            return false;
        }
        const std::string extension = file.extension().string();
        if (extension == ".xml") {
            if (static_cast<size_t>(std::distance(relative.begin(), relative.end())) != 2 ||
                !canonical_name(relative.begin()->string(), false) || !canonical_name(file.stem().string(), true)) {
                errors << "Graphics XML must be Category/Semantic_Image_Group.xml: " << relative.string() << ".\n";
                valid = false;
                continue;
            }
            const std::string key = lower(relative.parent_path().string() + "\\" + file.stem().string());
            if (!group_keys.insert(key).second) {
                errors << "Duplicate case-insensitive graphics image-group key: " << relative.string() << ".\n";
                valid = false;
            }

            std::ifstream input(file, std::ios::binary);
            const std::string xml((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
            const size_t assetlist = xml.find("<assetlist");
            const size_t tag_end = assetlist == std::string::npos ? std::string::npos : xml.find('>', assetlist);
            const size_t name_start = assetlist == std::string::npos ? std::string::npos : xml.find("name=\"", assetlist);
            const size_t value_start = name_start == std::string::npos ? std::string::npos : name_start + 6;
            const size_t value_end = value_start == std::string::npos ? std::string::npos : xml.find('\"', value_start);
            const std::string expected_name = relative.parent_path().string() + "\\" + file.stem().string();
            if (!input || tag_end == std::string::npos || value_end == std::string::npos || name_start > tag_end) {
                errors << "Graphics XML has no readable assetlist name: " << relative.string() << ".\n";
                valid = false;
            } else {
                const std::string declared_name = xml.substr(value_start, value_end - value_start);
                if (declared_name != expected_name) {
                    errors << "Graphics assetlist name must match its Category\\Image_Group path: " << relative.string() << ".\n";
                    valid = false;
                }
                if (!declared_group_keys.insert(lower(declared_name)).second) {
                    errors << "Duplicate case-insensitive declared assetlist name: " << declared_name << ".\n";
                    valid = false;
                }
            }
            const std::string category = relative.begin()->string();
            if ((category == "Environment" || category == "Walkers" || category == "Warriors") &&
                xml.substr(assetlist, tag_end - assetlist).find("logical_units_per_source_pixel=\"96\"") == std::string::npos) {
                errors << "Vespasian movable graphics must use the 0.8-scale 96/120 logical-size ratio: "
                       << relative.string() << ".\n";
                valid = false;
            }
            continue;
        }
        if (extension != ".png" || static_cast<size_t>(std::distance(relative.begin(), relative.end())) != 3) {
            errors << "Authored graphics payload must be Category/Image_Group/file.png: " << relative.string() << ".\n";
            valid = false;
            continue;
        }
        const std::filesystem::path group_directory = file.parent_path();
        const std::filesystem::path sibling_xml = group_directory.parent_path() /
            (group_directory.filename().string() + ".xml");
        if (!std::filesystem::is_regular_file(sibling_xml, error) || error) {
            errors << "Graphics payload directory has no sibling image-group XML: " << relative.string() << ".\n";
            valid = false;
            error.clear();
        }
    }

    const std::string infantry = "warriors\\auxiliary_infantry";
    const std::string archer = "warriors\\auxiliary_archer";
    if (group_keys.count(infantry) != 1 || group_keys.count(archer) != 1) {
        errors << "Vespasian must declare exactly one Auxiliary_Infantry and one Auxiliary_Archer image group.\n";
        valid = false;
    }
    for (const std::string &key : group_keys) {
        if (key.rfind(infantry + "_", 0) == 0 || key.rfind(archer + "_", 0) == 0) {
            errors << "Auxiliary animation was split into per-action/per-frame XML: " << key << ".\n";
            valid = false;
        }
    }
    return valid;
}
