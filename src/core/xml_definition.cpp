#include "core/xml_definition.h"

#include "core/xml_value.h"

#include "core/file.h"
#include "core/log.h"

#include <cstdio>
#include <cstring>

namespace xml_definition {

static int equals_ignore_case_ascii(char left, char right)
{
    if (left >= 'A' && left <= 'Z') {
        left = static_cast<char>(left - 'A' + 'a');
    }
    if (right >= 'A' && right <= 'Z') {
        right = static_cast<char>(right - 'A' + 'a');
    }
    return left == right;
}

static int ends_with_ignore_case_ascii(const std::string &value, const char *suffix)
{
    if (!suffix) {
        return 0;
    }

    const size_t suffix_length = std::strlen(suffix);
    if (value.size() < suffix_length) {
        return 0;
    }

    const size_t start = value.size() - suffix_length;
    for (size_t i = 0; i < suffix_length; i++) {
        if (!equals_ignore_case_ascii(value[start + i], suffix[i])) {
            return 0;
        }
    }
    return 1;
}

std::string normalize_path(const char *value)
{
    std::string normalized = xml_value::trim_copy(value ? value : "");
    if (normalized.empty()) {
        return std::string();
    }

    for (char &ch : normalized) {
        if (ch == '/') {
            ch = '\\';
        }
    }

    std::string collapsed;
    collapsed.reserve(normalized.size());
    char previous = '\0';
    for (char ch : normalized) {
        if (ch == '\\' && previous == '\\') {
            continue;
        }
        collapsed.push_back(ch);
        previous = ch;
    }

    if (!collapsed.empty() && (collapsed.front() == '\\' || collapsed.back() == '\\')) {
        return std::string();
    }
    if (ends_with_ignore_case_ascii(collapsed, ".xml")) {
        collapsed.resize(collapsed.size() - 4);
    }
    return collapsed;
}

int load_file_to_buffer(const char *filename, std::vector<char> &buffer, const char *label)
{
    FILE *fp = file_open(filename, "rb");
    if (!fp) {
        log_error("Unable to open xml definition", label ? label : filename, 0);
        return 0;
    }

    if (std::fseek(fp, 0, SEEK_END) != 0) {
        file_close(fp);
        log_error("Unable to seek xml definition", label ? label : filename, 0);
        return 0;
    }

    long size = std::ftell(fp);
    if (size < 0) {
        file_close(fp);
        log_error("Unable to size xml definition", label ? label : filename, 0);
        return 0;
    }
    std::rewind(fp);

    buffer.resize(static_cast<size_t>(size));
    const size_t read = std::fread(buffer.data(), 1, buffer.size(), fp);
    file_close(fp);
    if (read != buffer.size()) {
        log_error("Unable to read xml definition", label ? label : filename, 0);
        return 0;
    }
    return 1;
}

} // namespace xml_definition
