#include "assets/graphics_extractor_common.h"

#include "core/file.h"
#include "platform/file_manager.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

#include <cctype>
#include <utility>

namespace vespasian::graphics::extraction {

namespace {

const std::string kEmptyXmlAttribute;

bool is_name_char(char value)
{
    return std::isalnum(static_cast<unsigned char>(value)) || value == '_' || value == '-' || value == ':' || value == '\\';
}

void skip_whitespace(const std::string &xml, size_t &index, int &line_number)
{
    while (index < xml.size() && std::isspace(static_cast<unsigned char>(xml[index]))) {
        if (xml[index] == '\n') {
            line_number++;
        }
        index++;
    }
}

std::string parse_name(const std::string &xml, size_t &index)
{
    const size_t start = index;
    while (index < xml.size() && is_name_char(xml[index])) {
        index++;
    }
    return xml.substr(start, index - start);
}

void skip_until(const std::string &xml, size_t &index, const char *terminator, int &line_number)
{
    const std::string marker = terminator;
    while (index < xml.size()) {
        if (xml.compare(index, marker.size(), marker) == 0) {
            index += marker.size();
            return;
        }
        if (xml[index] == '\n') {
            line_number++;
        }
        index++;
    }
}

} // namespace

const std::string &XmlElement::attribute(const std::string &name) const
{
    auto it = attributes_.find(name);
    return it == attributes_.end() ? kEmptyXmlAttribute : it->second;
}

int XmlElement::int_attribute(const std::string &name) const
{
    const std::string &value = attribute(name);
    return value.empty() ? 0 : std::atoi(value.c_str());
}

bool XmlElement::bool_attribute(const std::string &name) const
{
    const std::string &value = attribute(name);
    if (value.empty()) {
        return false;
    }
    std::string lower = value;
    for (char &character : lower) {
        character = static_cast<char>(std::tolower(static_cast<unsigned char>(character)));
    }
    return lower == "true" || lower == "1" || lower == "yes" || lower == "y" || lower == name;
}

std::string XmlReader::decode_entities(const std::string &value)
{
    std::string decoded;
    decoded.reserve(value.size());
    for (size_t index = 0; index < value.size(); ++index) {
        if (value[index] != '&') {
            decoded.push_back(value[index]);
            continue;
        }
        const size_t semicolon = value.find(';', index + 1);
        if (semicolon == std::string::npos) {
            decoded.push_back(value[index]);
            continue;
        }
        const std::string entity = value.substr(index + 1, semicolon - index - 1);
        if (entity == "amp") {
            decoded.push_back('&');
        } else if (entity == "lt") {
            decoded.push_back('<');
        } else if (entity == "gt") {
            decoded.push_back('>');
        } else if (entity == "quot") {
            decoded.push_back('"');
        } else if (entity == "apos") {
            decoded.push_back('\'');
        } else {
            decoded += "&";
            decoded += entity;
            decoded += ";";
        }
        index = semicolon;
    }
    return decoded;
}

std::vector<XmlToken> XmlReader::parse(const std::string &xml) const
{
    std::vector<XmlToken> tokens;
    int line_number = 1;
    size_t index = 0;
    while (index < xml.size()) {
        if (xml[index] != '<') {
            if (xml[index] == '\n') {
                line_number++;
            }
            index++;
            continue;
        }

        const int token_line = line_number;
        index++;
        if (index >= xml.size()) {
            break;
        }
        if (xml.compare(index, 3, "!--") == 0) {
            index += 3;
            skip_until(xml, index, "-->", line_number);
            continue;
        }
        if (xml[index] == '?') {
            index++;
            skip_until(xml, index, "?>", line_number);
            continue;
        }
        if (xml[index] == '!') {
            index++;
            skip_until(xml, index, ">", line_number);
            continue;
        }

        if (xml[index] == '/') {
            index++;
            skip_whitespace(xml, index, line_number);
            std::string name = parse_name(xml, index);
            skip_until(xml, index, ">", line_number);
            if (!name.empty()) {
                tokens.emplace_back(XmlToken::Type::end, XmlElement(std::move(name), {}, token_line), false);
            }
            continue;
        }

        std::string name = parse_name(xml, index);
        std::unordered_map<std::string, std::string> attributes;
        bool self_closing = false;
        while (index < xml.size()) {
            skip_whitespace(xml, index, line_number);
            if (index >= xml.size()) {
                break;
            }
            if (xml[index] == '/') {
                self_closing = true;
                index++;
                skip_whitespace(xml, index, line_number);
                if (index < xml.size() && xml[index] == '>') {
                    index++;
                }
                break;
            }
            if (xml[index] == '>') {
                index++;
                break;
            }

            std::string attribute_name = parse_name(xml, index);
            skip_whitespace(xml, index, line_number);
            std::string attribute_value;
            if (index < xml.size() && xml[index] == '=') {
                index++;
                skip_whitespace(xml, index, line_number);
                if (index < xml.size() && (xml[index] == '"' || xml[index] == '\'')) {
                    const char quote = xml[index++];
                    const size_t value_start = index;
                    while (index < xml.size() && xml[index] != quote) {
                        if (xml[index] == '\n') {
                            line_number++;
                        }
                        index++;
                    }
                    attribute_value = decode_entities(xml.substr(value_start, index - value_start));
                    if (index < xml.size()) {
                        index++;
                    }
                }
            }
            if (!attribute_name.empty()) {
                attributes.emplace(std::move(attribute_name), std::move(attribute_value));
            }
        }

        if (!name.empty()) {
            tokens.emplace_back(XmlToken::Type::start, XmlElement(std::move(name), std::move(attributes), token_line), self_closing);
        }
    }
    return tokens;
}

} // namespace vespasian::graphics::extraction

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

static bool load_file_to_buffer(const std::string &path, std::vector<char> &buffer)
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
