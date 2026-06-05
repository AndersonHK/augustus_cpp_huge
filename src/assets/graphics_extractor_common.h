#pragma once

#include <stddef.h>

#include <string>
#include <vector>

namespace graphics_extractor {

std::string sanitize_component(const char *text);
std::string normalize_key(const char *text);
std::string without_trailing_separator(std::string path);
int append_path_component(char *buffer, size_t buffer_size, const char *base_path, const char *component);
std::string append_path_component(const std::string &base_path, const std::string &component);
bool load_file_to_buffer(const std::string &path, std::vector<char> &buffer);
bool read_text_file(const std::string &path, std::string &contents);
bool write_text_file(const std::string &path, const std::string &contents);
void ensure_directory(const std::string &path);
bool parse_generated_image_index(const std::string &image_id, int &image_index);
std::string make_generated_image_id(int image_index);
void append_indent(std::string &xml, int depth);
void append_attribute(std::string &xml, const char *name, const std::string &value);
void append_attribute(std::string &xml, const char *name, int value);

} // namespace graphics_extractor
