#include "figure/type.h"
#include <array>
#include <string>
#include <cstring>
#include <cctype>

namespace {
struct Identity { std::string name; figure_type base = FIGURE_NONE; };
std::array<Identity, FIGURE_TYPE_MAX> identities;
}
figure_type figure_type_dynamic_from_name(const char *name)
{
    for (int i = FIGURE_BUILTIN_TYPE_MAX; name && i < FIGURE_TYPE_MAX; ++i) if (identities[i].name == name) return static_cast<figure_type>(i);
    return FIGURE_NONE;
}
figure_type figure_type_register(const char *name, const char *base_name)
{
    if (!name || !*name || strlen(name) >= FIGURE_TYPE_IDENTITY_CAPACITY || !base_name) return FIGURE_NONE;
    for (const unsigned char *c = reinterpret_cast<const unsigned char *>(name); *c; ++c) if (!std::isalnum(*c) && *c != '_') return FIGURE_NONE;
    figure_type base = figure_type_from_xml_name(base_name);
    if (base <= FIGURE_NONE || base >= FIGURE_BUILTIN_TYPE_MAX) return FIGURE_NONE;
    figure_type existing = figure_type_from_xml_name(name);
    if (existing != FIGURE_NONE) return existing >= FIGURE_BUILTIN_TYPE_MAX && identities[existing].base == base ? existing : FIGURE_NONE;
    for (int i = FIGURE_BUILTIN_TYPE_MAX; i < FIGURE_TYPE_MAX; ++i) if (identities[i].name.empty()) { identities[i] = {name, base}; return static_cast<figure_type>(i); }
    return FIGURE_NONE;
}
figure_type figure_type_base(figure_type type)
{
    if (type < FIGURE_NONE || type >= FIGURE_TYPE_MAX) return FIGURE_NONE;
    return type < FIGURE_BUILTIN_TYPE_MAX ? type : identities[type].base;
}
const char *figure_type_identity(figure_type type)
{
    if (type >= FIGURE_BUILTIN_TYPE_MAX && type < FIGURE_TYPE_MAX) return identities[type].name.c_str();
    for (const auto &entry : FIGURE_TYPE_XML_NAMES) if (entry.type == type) return entry.name.data();
    return "";
}
void figure_type_identity_reset() { for (auto &id : identities) id = {}; }
