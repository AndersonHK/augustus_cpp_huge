#include "game/mod_definition_loader.h"

#include "core/log.h"
#include "core/xml_definition.h"
#include "game/mod_manager.h"

#include <algorithm>
#include <filesystem>
#include <sstream>
#include <utility>

namespace mod_definition {
namespace {

void set_failure(std::string *failure_reason, const std::string &message)
{
    if (failure_reason) {
        *failure_reason = message;
    }
}

std::string trim_category(const std::string &category)
{
    std::size_t first = 0;
    while (first < category.size() && (category[first] == '/' || category[first] == '\\')) {
        ++first;
    }

    std::size_t last = category.size();
    while (last > first && (category[last - 1] == '/' || category[last - 1] == '\\')) {
        --last;
    }

    std::string result = category.substr(first, last - first);
    for (char &ch : result) {
        if (ch == '/') {
            ch = '\\';
        }
    }
    return result;
}

bool category_is_valid(const std::string &category)
{
    if (category.empty() || category.find(':') != std::string::npos) {
        return false;
    }

    std::size_t start = 0;
    while (start <= category.size()) {
        const std::size_t end = category.find('\\', start);
        const std::string segment = category.substr(
            start, end == std::string::npos ? std::string::npos : end - start);
        if (segment.empty() || segment == "." || segment == "..") {
            return false;
        }
        if (end == std::string::npos) {
            break;
        }
        start = end + 1;
    }
    return true;
}

std::string directory_path(const DefinitionLayer &layer, const std::string &category)
{
    std::string result = layer.root_path;
    if (!result.empty() && result.back() != '/' && result.back() != '\\') {
        result.push_back('/');
    }
    result += category;
    result.push_back('/');
    return result;
}

bool normalize_relative_path(const std::string &path, std::string &out)
{
    out = path;
    for (char &ch : out) {
        if (ch == '\\') {
            ch = '/';
        }
    }

    if (out.empty() || out.front() == '/' || out.find(':') != std::string::npos) {
        return false;
    }

    std::size_t start = 0;
    while (start <= out.size()) {
        const std::size_t end = out.find('/', start);
        const std::string segment = out.substr(
            start, end == std::string::npos ? std::string::npos : end - start);
        if (segment.empty() || segment == "." || segment == "..") {
            return false;
        }
        if (end == std::string::npos) {
            break;
        }
        start = end + 1;
    }
    return true;
}

std::string file_path(const DefinitionLayer &layer, const std::string &relative_path)
{
    std::string result = layer.root_path;
    if (!result.empty() && result.back() != '/' && result.back() != '\\') {
        result.push_back('/');
    }
    result += relative_path;
    return result;
}

std::string source_name(const DefinitionSource &source)
{
    if (!source.full_path.empty()) {
        return source.full_path;
    }
    if (!source.registry_relative_path.empty()) {
        return source.registry_relative_path;
    }
    return "<unknown definition source>";
}

} // namespace

std::string DefinitionSource::describe() const
{
    std::ostringstream description;
    if (!mod_name.empty()) {
        description << mod_name << ':';
    }
    description << (!registry_relative_path.empty() ? registry_relative_path : source_name(*this));
    return description.str();
}

bool configured_layers(std::vector<DefinitionLayer> &out, std::string *failure_reason)
{
    out.clear();
    if (failure_reason) {
        failure_reason->clear();
    }

    const std::vector<std::string> &names = mod_manager::mod_names();
    const std::vector<std::string> &paths = mod_manager::mod_paths();
    if (names.size() != paths.size()) {
        set_failure(failure_reason, "Configured mod names and paths have different lengths.");
        return false;
    }

    out.reserve(names.size());
    for (std::size_t i = 0; i < names.size(); ++i) {
        out.push_back({names[i], paths[i]});
    }
    return true;
}

bool find_nearest_file(
    const std::vector<DefinitionLayer> &layers,
    const std::string &relative_path,
    LayeredFileSource *out,
    std::string *failure_reason)
{
    if (out) {
        *out = {};
    }
    if (failure_reason) {
        failure_reason->clear();
    }

    std::string normalized_path;
    if (!normalize_relative_path(relative_path, normalized_path)) {
        set_failure(failure_reason, "Invalid layered file path: " + relative_path);
        return false;
    }

    for (std::size_t i = layers.size(); i > 0; --i) {
        const std::size_t layer_index = i - 1;
        const DefinitionLayer &layer = layers[layer_index];
        const std::string full_path = file_path(layer, normalized_path);
        std::error_code error;
        if (!std::filesystem::is_regular_file(std::filesystem::path(full_path), error)) {
            continue;
        }

        if (out) {
            out->layer_index = layer_index;
            out->mod_name = layer.mod_name;
            out->mod_root = layer.root_path;
            out->relative_path = normalized_path;
            out->full_path = full_path;
        }
        return true;
    }

    set_failure(failure_reason, "Layered file was not found in the configured mod stack: " + normalized_path);
    return false;
}

bool find_nearest_configured_file(
    const std::string &relative_path,
    LayeredFileSource *out,
    std::string *failure_reason)
{
    std::vector<DefinitionLayer> layers;
    if (!configured_layers(layers, failure_reason)) {
        return false;
    }
    return find_nearest_file(layers, relative_path, out, failure_reason);
}

bool for_each_definition_file(
    const std::vector<DefinitionLayer> &layers,
    const std::vector<std::string> &categories,
    const char *label,
    bool require_files,
    const DefinitionSourceVisitor &visitor,
    DefinitionEnumerationSummary *summary,
    std::string *failure_reason)
{
    if (summary) {
        *summary = {};
        summary->layers = layers.size();
    }
    if (failure_reason) {
        failure_reason->clear();
    }
    if (categories.empty()) {
        set_failure(failure_reason, "Layered definition enumeration requires at least one category.");
        return false;
    }

    std::vector<std::string> normalized_categories;
    normalized_categories.reserve(categories.size());
    for (const std::string &category : categories) {
        std::string normalized = trim_category(category);
        if (!category_is_valid(normalized)) {
            set_failure(failure_reason, "Invalid layered definition category: " + category);
            return false;
        }
        if (std::find(normalized_categories.begin(), normalized_categories.end(), normalized) !=
            normalized_categories.end()) {
            set_failure(failure_reason, "Duplicate layered definition category: " + normalized);
            return false;
        }
        normalized_categories.push_back(std::move(normalized));
    }

    std::size_t files_found = 0;
    for (std::size_t layer_index = 0; layer_index < layers.size(); ++layer_index) {
        const DefinitionLayer &layer = layers[layer_index];
        for (const std::string &category : normalized_categories) {
            const std::string directory = directory_path(layer, category);
            if (!xml_definition::directory_exists(directory.c_str())) {
                continue;
            }
            if (summary) {
                ++summary->directories;
            }

            std::vector<DefinitionSource> sources;
            if (!xml_definition::for_each_definition_file(
                    directory,
                    label,
                    false,
                    [&](const xml_definition::DefinitionFile &file, const std::string &normalized_path) {
                        DefinitionSource source;
                        source.layer_index = layer_index;
                        source.mod_name = layer.mod_name;
                        source.mod_root = layer.root_path;
                        source.category = category;
                        source.file_name = file.name;
                        source.full_path = file.full_path;
                        source.normalized_definition_path = normalized_path;
                        source.registry_relative_path = category + "\\" + normalized_path;
                        sources.push_back(std::move(source));
                        return true;
                    })) {
                set_failure(failure_reason, "Unable to enumerate " + std::string(label ? label : "definition") +
                    " files in " + directory);
                return false;
            }

            std::sort(sources.begin(), sources.end(), [](const DefinitionSource &left, const DefinitionSource &right) {
                return left.file_name < right.file_name;
            });
            for (const DefinitionSource &source : sources) {
                ++files_found;
                if (summary) {
                    ++summary->files;
                }
                if (visitor && !visitor(source)) {
                    // Registry visitors often have a more specific parser or
                    // overlay diagnostic. Preserve it instead of replacing it
                    // with an enumeration-level failure.
                    if (!failure_reason || failure_reason->empty()) {
                        set_failure(failure_reason, "Definition visitor rejected " + source.describe());
                    }
                    return false;
                }
            }
        }
    }

    if (require_files && files_found == 0) {
        const std::string message = "No " + std::string(label ? label : "definition") +
            " xml files found in the configured mod layers.";
        set_failure(failure_reason, message);
        log_error(message.c_str(), 0, 0);
        return false;
    }
    return true;
}

bool for_each_configured_definition_file(
    const std::vector<std::string> &categories,
    const char *label,
    bool require_files,
    const DefinitionSourceVisitor &visitor,
    DefinitionEnumerationSummary *summary,
    std::string *failure_reason)
{
    std::vector<DefinitionLayer> layers;
    if (!configured_layers(layers, failure_reason)) {
        return false;
    }
    return for_each_definition_file(
        layers, categories, label, require_files, visitor, summary, failure_reason);
}

bool DefinitionOverlayTracker::apply(
    const std::string &stable_id,
    bool disabled,
    const DefinitionSource &source,
    DefinitionOverlayChange *change)
{
    failure_reason_.clear();
    if (stable_id.empty()) {
        failure_reason_ = "Definition stable id must not be empty (source " + source_name(source) + ").";
        return false;
    }

    const auto existing = entries_.find(stable_id);
    if (existing != entries_.end() && source.layer_index <= existing->second.source.layer_index) {
        std::ostringstream failure;
        if (source.layer_index == existing->second.source.layer_index) {
            failure << "Duplicate definition '" << stable_id << "' in mod layer " << source.layer_index
                << ": " << source_name(existing->second.source) << " and " << source_name(source) << '.';
        } else {
            failure << "Definition '" << stable_id << "' from lower layer " << source.layer_index
                << " was applied after layer " << existing->second.source.layer_index << ": "
                << source_name(source) << '.';
        }
        failure_reason_ = failure.str();
        return false;
    }

    DefinitionOverlayEntry winner{stable_id, source, disabled};
    DefinitionOverlayChange applied;
    applied.winner = winner;
    if (existing == entries_.end()) {
        applied.action = disabled ? DefinitionOverlayAction::Suppressed : DefinitionOverlayAction::Added;
        entries_.emplace(stable_id, winner);
    } else {
        applied.previous = existing->second;
        if (disabled) {
            applied.action = DefinitionOverlayAction::Suppressed;
        } else if (existing->second.disabled) {
            applied.action = DefinitionOverlayAction::Restored;
        } else {
            applied.action = DefinitionOverlayAction::Replaced;
        }
        existing->second = winner;
    }

    if (change) {
        *change = std::move(applied);
    }
    return true;
}

const DefinitionOverlayEntry *DefinitionOverlayTracker::find(const std::string &stable_id) const
{
    const auto found = entries_.find(stable_id);
    return found == entries_.end() ? nullptr : &found->second;
}

const std::map<std::string, DefinitionOverlayEntry> &DefinitionOverlayTracker::entries() const
{
    return entries_;
}

std::size_t DefinitionOverlayTracker::active_count() const
{
    return static_cast<std::size_t>(std::count_if(
        entries_.begin(), entries_.end(), [](const auto &entry) { return !entry.second.disabled; }));
}

std::size_t DefinitionOverlayTracker::suppressed_count() const
{
    return entries_.size() - active_count();
}

const std::string &DefinitionOverlayTracker::failure_reason() const
{
    return failure_reason_;
}

void DefinitionOverlayTracker::clear()
{
    entries_.clear();
    failure_reason_.clear();
}

} // namespace mod_definition
