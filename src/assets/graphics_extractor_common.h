#pragma once

#include <stddef.h>

#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace vespasian::graphics::extraction {

class XmlElement {
public:
    XmlElement(std::string name, std::unordered_map<std::string, std::string> attributes, int line_number)
        : name_(std::move(name))
        , attributes_(std::move(attributes))
        , line_number_(line_number)
    {
    }

    const std::string &name() const { return name_; }
    int line_number() const { return line_number_; }
    bool has_attribute(const std::string &name) const { return attributes_.find(name) != attributes_.end(); }
    const std::string &attribute(const std::string &name) const;
    int int_attribute(const std::string &name) const;
    bool bool_attribute(const std::string &name) const;

private:
    std::string name_;
    std::unordered_map<std::string, std::string> attributes_;
    int line_number_ = 1;
};

class XmlToken {
public:
    enum class Type {
        start,
        end
    };

    XmlToken(Type type, XmlElement element, bool self_closing)
        : type_(type)
        , element_(std::move(element))
        , self_closing_(self_closing)
    {
    }

    Type type() const { return type_; }
    const XmlElement &element() const { return element_; }
    bool is_self_closing() const { return self_closing_; }

private:
    Type type_ = Type::start;
    XmlElement element_;
    bool self_closing_ = false;
};

class XmlReader {
public:
    std::vector<XmlToken> parse(const std::string &xml) const;

private:
    static std::string decode_entities(const std::string &value);
};

} // namespace vespasian::graphics::extraction

namespace graphics_extractor {

std::string sanitize_component(const char *text);
std::string normalize_key(const char *text);
std::string without_trailing_separator(std::string path);
int append_path_component(char *buffer, size_t buffer_size, const char *base_path, const char *component);
std::string append_path_component(const std::string &base_path, const std::string &component);
bool read_text_file(const std::string &path, std::string &contents);
bool write_text_file(const std::string &path, const std::string &contents);
void ensure_directory(const std::string &path);
bool parse_generated_image_index(const std::string &image_id, int &image_index);
std::string make_generated_image_id(int image_index);
void append_indent(std::string &xml, int depth);
void append_attribute(std::string &xml, const char *name, const std::string &value);
void append_attribute(std::string &xml, const char *name, int value);

} // namespace graphics_extractor
