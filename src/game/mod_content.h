#pragma once

#include <filesystem>
#include <map>
#include <set>
#include <string>
#include <vector>

// Shared by the native launcher and runtime. No renderer, global XML parser or game state dependencies.
namespace mod_content {

struct Node {
    std::string name;
    std::map<std::string, std::string> attributes;
    std::vector<Node> children;
    std::string text;
    std::set<std::string> settings;
    std::map<std::string, std::set<std::string>> attribute_settings;
    std::map<std::string, std::pair<std::size_t, std::size_t>> attribute_ranges;
    std::string origin;
    std::size_t begin = 0, end = 0;
    std::string attribute(const std::string &key, const std::string &fallback = {}) const;
    const Node *child(const std::string &key) const;
};

Node parse(const std::string &xml);
std::string serialize(const Node &node);
std::string read(const std::filesystem::path &path);
void write_atomic(const std::filesystem::path &path, const std::string &text);
std::string path_key(const std::filesystem::path &path);
std::string path_text(const std::filesystem::path &path);
std::filesystem::path utf8_path(const std::string &path);
Node overlay(Node base, const Node &upper);

struct Setting {
    std::string mod, id, name, category, description;
    bool boolean = true;
    int minimum = 0, maximum = 1, default_value = 0, value = 0;
    bool effective = false;
    std::string disabled_reason;
    std::string key() const { return mod + ":" + id; }
};

struct Manifest {
    std::string name, description, version;
    std::vector<std::string> dependencies;
    std::vector<Setting> settings;
};

Manifest manifest(const std::filesystem::path &path);
std::vector<std::string> read_list(const std::filesystem::path &path);
void write_list(const std::filesystem::path &path, const std::vector<std::string> &mods);
bool valid_name(const std::string &name);
std::string lower(std::string value);

struct Layer { std::string name; std::filesystem::path root; };

class Session {
public:
    // Builds a fresh snapshot; callers publish only after the whole stack compiles.
    void load(const std::vector<Layer> &layers, const std::filesystem::path &values_file, bool require_manifests = true);
    void compile();
    void set(const std::string &key, int value);
    void save() const;
    const std::vector<Setting> &settings() const { return settings_; }
    const std::vector<Layer> &layers() const { return layers_; }
    const std::map<std::string, std::string> &files() const { return files_; }
    const std::string *file(const std::filesystem::path &path) const;
    std::string expand(const std::string &source, const std::string &mod) const;
private:
    std::vector<Layer> layers_;
    std::vector<Setting> settings_;
    std::map<std::string, int> saved_values_;
    std::filesystem::path values_file_;
    std::map<std::string, std::string> files_;
};

// Runtime snapshot used by the normal registry readers. Launcher owns a separate Session.
Session &runtime();
const std::string *resolved_file(const char *path);
}
