#include "assets/graphics_extractor_common.h"

extern "C" {
#include "core/file.h"
#include "platform/file_manager.h"
}

#include <cstdio>
#include <cstdlib>
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

std::string without_trailing_separator(std::string path)
{
    while (!path.empty() && (path.back() == '/' || path.back() == '\\')) {
        path.pop_back();
    }
    return path;
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
    std::vector<char> buffer;
    if (!load_file_to_buffer(path, buffer)) {
        contents.clear();
        return false;
    }
    contents.assign(buffer.begin(), buffer.end());
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

bool parse_generated_image_index(const std::string &image_id, int &image_index)
{
    image_index = 0;
    if (image_id.size() <= 6 || image_id.compare(0, 6, "Image_") != 0) {
        return false;
    }

    char *end = nullptr;
    const long value = strtol(image_id.c_str() + 6, &end, 10);
    if (!end || *end != '\0' || value < 0) {
        return false;
    }
    image_index = static_cast<int>(value);
    return true;
}

std::string make_generated_image_id(int image_index)
{
    char buffer[32];
    snprintf(buffer, sizeof(buffer), "Image_%04d", image_index);
    return buffer;
}

void append_indent(std::string &xml, int depth)
{
    xml.append(static_cast<size_t>(depth) * 4, ' ');
}

void append_attribute(std::string &xml, const char *name, const std::string &value)
{
    xml += " ";
    xml += name;
    xml += "=\"";
    xml += value;
    xml += "\"";
}

void append_attribute(std::string &xml, const char *name, int value)
{
    char buffer[32];
    snprintf(buffer, sizeof(buffer), "%d", value);
    append_attribute(xml, name, std::string(buffer));
}

} // namespace graphics_extractor
