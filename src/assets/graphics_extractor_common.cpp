#include "assets/graphics_extractor_common.h"

extern "C" {
#include "core/file.h"
#include "platform/file_manager.h"
}

#include <cstdio>
#include <cstring>

namespace graphics_extractor {

std::string sanitize_component(const char *text)
{
    if (!text || !*text) {
        return "unnamed";
    }

    std::string sanitized;
    sanitized.reserve(strlen(text));
    for (const char *cursor = text; *cursor; ++cursor) {
        const char value = *cursor;
        const bool is_alpha = (value >= 'a' && value <= 'z') || (value >= 'A' && value <= 'Z');
        const bool is_digit = value >= '0' && value <= '9';
        if (is_alpha || is_digit || value == '_' || value == '-') {
            sanitized.push_back(value);
        } else {
            sanitized.push_back('_');
        }
    }

    while (!sanitized.empty() && sanitized.back() == '_') {
        sanitized.pop_back();
    }
    return sanitized.empty() ? std::string("unnamed") : sanitized;
}

std::string normalize_key(const char *text)
{
    std::string normalized = text ? text : "";
    while (!normalized.empty() &&
        (normalized.front() == ' ' || normalized.front() == '\t' || normalized.front() == '\r' || normalized.front() == '\n')) {
        normalized.erase(normalized.begin());
    }
    while (!normalized.empty() &&
        (normalized.back() == ' ' || normalized.back() == '\t' || normalized.back() == '\r' || normalized.back() == '\n')) {
        normalized.pop_back();
    }
    for (char &value : normalized) {
        if (value == '/') {
            value = '\\';
        }
    }
    return normalized;
}

int append_path_component(char *buffer, size_t buffer_size, const char *base_path, const char *component)
{
    if (!buffer || buffer_size == 0 || !base_path || !*base_path || !component || !*component) {
        return 0;
    }

    const size_t base_length = strlen(base_path);
    const int has_separator = base_length > 0 &&
        (base_path[base_length - 1] == '/' || base_path[base_length - 1] == '\\');
    return snprintf(buffer, buffer_size, has_separator ? "%s%s" : "%s/%s", base_path, component) <
        static_cast<int>(buffer_size);
}

std::string append_path_component(const std::string &base_path, const std::string &component)
{
    char buffer[FILE_NAME_MAX];
    if (!append_path_component(buffer, sizeof(buffer), base_path.c_str(), component.c_str())) {
        return {};
    }
    return buffer;
}

bool load_file_to_buffer(const std::string &path, std::vector<char> &buffer)
{
    FILE *file = file_open(path.c_str(), "rb");
    if (!file) {
        return false;
    }

    if (fseek(file, 0, SEEK_END) != 0) {
        file_close(file);
        return false;
    }

    long file_size = ftell(file);
    if (file_size < 0) {
        file_close(file);
        return false;
    }
    rewind(file);

    buffer.resize(static_cast<size_t>(file_size));
    if (!buffer.empty()) {
        const size_t bytes_read = fread(buffer.data(), 1, buffer.size(), file);
        if (bytes_read != buffer.size()) {
            file_close(file);
            return false;
        }
    }

    file_close(file);
    return true;
}

bool read_text_file(const std::string &path, std::string &contents)
{
    FILE *file = file_open(path.c_str(), "rb");
    if (!file) {
        contents.clear();
        return false;
    }

    char buffer[256];
    const size_t bytes_read = fread(buffer, 1, sizeof(buffer) - 1, file);
    buffer[bytes_read] = '\0';
    contents.assign(buffer);
    file_close(file);
    return true;
}

bool write_text_file(const std::string &path, const std::string &contents)
{
    FILE *file = file_open(path.c_str(), "wb");
    if (!file) {
        return false;
    }

    const size_t bytes_written = fwrite(contents.data(), 1, contents.size(), file);
    file_close(file);
    return bytes_written == contents.size();
}

void ensure_directory(const std::string &path)
{
    if (!path.empty()) {
        platform_file_manager_create_directory(path.c_str(), 0, 1);
    }
}

} // namespace graphics_extractor
