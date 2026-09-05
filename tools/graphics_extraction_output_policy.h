#pragma once

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <string>

namespace graphics_extraction_output_policy {

inline std::string normalized_path_key(const std::filesystem::path &path)
{
    std::string key = std::filesystem::absolute(path).lexically_normal().generic_string();
    std::transform(key.begin(), key.end(), key.begin(), [](unsigned char value) { return static_cast<char>(std::tolower(value)); });
    while (key.size() > 1 && key.back() == '/') key.pop_back();
    return key;
}

inline bool is_within(const std::filesystem::path &path, const std::filesystem::path &root)
{
    const std::string path_key = normalized_path_key(path);
    const std::string root_key = normalized_path_key(root);
    return path_key == root_key || (path_key.size() > root_key.size() && path_key.compare(0, root_key.size(), root_key) == 0 && path_key[root_key.size()] == '/');
}

inline std::filesystem::path checkout_root_for(const std::filesystem::path &path)
{
    std::filesystem::path candidate = std::filesystem::absolute(path).lexically_normal();
    while (!candidate.empty()) {
        if (std::filesystem::exists(candidate / ".git")) return candidate;
        const std::filesystem::path parent = candidate.parent_path();
        if (parent == candidate) break;
        candidate = parent;
    }
    return {};
}

inline bool validate(const std::filesystem::path &output, std::string &failure_reason)
{
    const std::filesystem::path checkout_root = checkout_root_for(output);
    if (checkout_root.empty()) return true;
    const std::filesystem::path sample_root = checkout_root / "extracted_graphics_sample";
    if (is_within(output, sample_root)) return true;
    failure_reason = "Repository extraction output must be inside " + sample_root.string() + ", never the authored Mods or build-output trees: " + std::filesystem::absolute(output).string();
    return false;
}

}
