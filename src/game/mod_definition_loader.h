#pragma once

#include <cstddef>
#include <functional>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace mod_definition {

struct DefinitionLayer {
    std::string mod_name;
    std::string root_path;
};

struct LayeredFileSource {
    std::size_t layer_index = 0;
    std::string mod_name;
    std::string mod_root;
    std::string relative_path;
    std::string full_path;
};

struct DefinitionSource {
    std::size_t layer_index = 0;
    std::string mod_name;
    std::string mod_root;
    std::string category;
    std::string file_name;
    std::string full_path;
    std::string normalized_definition_path;
    std::string registry_relative_path;

    std::string describe() const;
};

struct DefinitionEnumerationSummary {
    std::size_t layers = 0;
    std::size_t directories = 0;
    std::size_t files = 0;
};

using DefinitionSourceVisitor = std::function<bool(const DefinitionSource &)>;

bool configured_layers(std::vector<DefinitionLayer> &out, std::string *failure_reason = nullptr);

// Selects one complete singleton file from a lower-to-upper layer list. Use
// identity-based enumeration for files that contain registry definitions.
bool find_nearest_file(
    const std::vector<DefinitionLayer> &layers,
    const std::string &relative_path,
    LayeredFileSource *out,
    std::string *failure_reason = nullptr);

bool find_nearest_configured_file(
    const std::string &relative_path,
    LayeredFileSource *out,
    std::string *failure_reason = nullptr);

bool for_each_definition_file(
    const std::vector<DefinitionLayer> &layers,
    const std::vector<std::string> &categories,
    const char *label,
    bool require_files,
    const DefinitionSourceVisitor &visitor,
    DefinitionEnumerationSummary *summary = nullptr,
    std::string *failure_reason = nullptr);

bool for_each_configured_definition_file(
    const std::vector<std::string> &categories,
    const char *label,
    bool require_files,
    const DefinitionSourceVisitor &visitor,
    DefinitionEnumerationSummary *summary = nullptr,
    std::string *failure_reason = nullptr);

enum class DefinitionOverlayAction {
    Added,
    Replaced,
    Suppressed,
    Restored
};

struct DefinitionOverlayEntry {
    std::string stable_id;
    DefinitionSource source;
    bool disabled = false;
};

struct DefinitionOverlayChange {
    DefinitionOverlayAction action = DefinitionOverlayAction::Added;
    std::optional<DefinitionOverlayEntry> previous;
    DefinitionOverlayEntry winner;
};

class DefinitionOverlayTracker {
public:
    bool apply(
        const std::string &stable_id,
        bool disabled,
        const DefinitionSource &source,
        DefinitionOverlayChange *change = nullptr);

    const DefinitionOverlayEntry *find(const std::string &stable_id) const;
    const std::map<std::string, DefinitionOverlayEntry> &entries() const;
    std::size_t active_count() const;
    std::size_t suppressed_count() const;
    const std::string &failure_reason() const;
    void clear();

private:
    std::map<std::string, DefinitionOverlayEntry> entries_;
    std::string failure_reason_;
};

} // namespace mod_definition
