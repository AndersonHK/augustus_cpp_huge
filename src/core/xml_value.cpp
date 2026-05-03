#include "core/xml_value.h"

#include <cstdlib>

namespace xml_value {

bool equals(const char *value, std::string_view expected)
{
    return value && std::string_view(value) == expected;
}

std::string trim_copy(std::string_view value)
{
    size_t start = 0;
    while (start < value.size() &&
        (value[start] == ' ' || value[start] == '\t' || value[start] == '\r' || value[start] == '\n')) {
        start++;
    }

    size_t end = value.size();
    while (end > start &&
        (value[end - 1] == ' ' || value[end - 1] == '\t' || value[end - 1] == '\r' || value[end - 1] == '\n')) {
        end--;
    }
    return std::string(value.substr(start, end - start));
}

int parse_bool(const char *value, int *out_value)
{
    if (!value || !out_value) {
        return 0;
    }

    if (equals(value, "true") || equals(value, "1") || equals(value, "yes")) {
        *out_value = 1;
        return 1;
    }
    if (equals(value, "false") || equals(value, "0") || equals(value, "no")) {
        *out_value = 0;
        return 1;
    }
    return 0;
}

int parse_int_strict(std::string_view text, int *out_value)
{
    if (!out_value) {
        return 0;
    }

    std::string value(text);
    char *end = nullptr;
    const long parsed = strtol(value.c_str(), &end, 10);
    if (!end || *end != '\0') {
        return 0;
    }
    *out_value = static_cast<int>(parsed);
    return 1;
}

} // namespace xml_value
