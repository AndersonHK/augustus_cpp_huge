#include "game/mod_content.h"
#include "sxml/sxml.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <charconv>
#include <cctype>
#include <fstream>
#include <functional>
#include <limits>
#include <stdexcept>
#include <system_error>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace mod_content {
namespace {
[[noreturn]] void fail(const std::string &message) { throw std::runtime_error(message); }

std::string escaped(const std::string &value)
{
    std::string out;
    for (char c : value) {
        switch (c) {
        case '&': out += "&amp;"; break;
        case '<': out += "&lt;"; break;
        case '>': out += "&gt;"; break;
        case '"': out += "&quot;"; break;
        default: out += c; break;
        }
    }
    return out;
}

std::string decoded(std::string value)
{
    std::string out;
    for (std::size_t i = 0; i < value.size(); ++i) {
        if (value[i] != '&') { out += value[i]; continue; }
        const auto end = value.find(';', i);
        if (end == std::string::npos) fail("Unterminated XML entity");
        const auto key = value.substr(i + 1, end - i - 1);
        if (key == "amp") out += '&';
        else if (key == "lt") out += '<';
        else if (key == "gt") out += '>';
        else if (key == "quot") out += '"';
        else if (key == "apos") out += '\'';
        else if (!key.empty() && key[0] == '#') {
            unsigned int cp = 0;
            const bool hex = key.size() > 1 && key[1] == 'x';
            const char *start = key.data() + (hex ? 2 : 1);
            auto result = std::from_chars(start, key.data() + key.size(), cp, hex ? 16 : 10);
            if (result.ec != std::errc() || result.ptr != key.data() + key.size() || !cp || cp > 0x10ffff || (cp >= 0xd800 && cp <= 0xdfff)) fail("Invalid XML character reference");
            if (cp < 0x80) out += static_cast<char>(cp);
            else if (cp < 0x800) { out += static_cast<char>(0xc0 | (cp >> 6)); out += static_cast<char>(0x80 | (cp & 63)); }
            else if (cp < 0x10000) { out += static_cast<char>(0xe0 | (cp >> 12)); out += static_cast<char>(0x80 | ((cp >> 6) & 63)); out += static_cast<char>(0x80 | (cp & 63)); }
            else { out += static_cast<char>(0xf0 | (cp >> 18)); out += static_cast<char>(0x80 | ((cp >> 12) & 63)); out += static_cast<char>(0x80 | ((cp >> 6) & 63)); out += static_cast<char>(0x80 | (cp & 63)); }
        } else fail("Unsupported XML entity: " + key);
        i = end;
    }
    return out;
}

int integer(const std::string &value)
{
    int out = 0;
    auto r = std::from_chars(value.data(), value.data() + value.size(), out);
    if (r.ec != std::errc() || r.ptr != value.data() + value.size()) fail("Expected integer, got: " + value);
    return out;
}

bool identifier(const std::string &id)
{
    if (id.empty() || !(std::isalpha(static_cast<unsigned char>(id[0])) || id[0] == '_')) return false;
    return std::all_of(id.begin(), id.end(), [](unsigned char c) { return std::isalnum(c) || c == '_'; });
}

std::string field_key(const Node &node)
{
    // Named tables (e.g. calendars) are independent fields; anonymous repetitions form one field.
    return node.name + (node.attributes.count("id") ? ":" + node.attribute("id") : "");
}

struct Reference { std::size_t begin, end; std::string key; };

std::string expand_source(const std::string &source, const std::string &mod, const std::vector<Setting> &settings, std::vector<Reference> *references, int depth = 0)
{
    if (depth > 16) fail("Settings conditional nesting exceeds 16");
    std::string out;
    for (std::size_t i = 0; i < source.size();) {
        if (source.compare(i, 4, "<!--") == 0) {
            auto end = source.find("-->", i + 4);
            if (end == std::string::npos) fail("Unterminated XML comment");
            out.append(source, i, end + 3 - i); i = end + 3; continue;
        }
        if (source[i] != '$') { out += source[i++]; continue; }
        if (i + 1 < source.size() && source[i + 1] == '$') { out += '$'; i += 2; continue; }
        ++i;
        bool negate = i < source.size() && source[i] == '!';
        if (negate) ++i;
        auto start = i;
        std::string owner = mod, id;
        if (i < source.size() && source[i] == '[') {
            const auto end = source.find(']', ++i);
            const auto separator = source.find(':', i);
            if (end == std::string::npos || separator == std::string::npos || separator >= end) fail("Invalid qualified setting in " + mod);
            owner = source.substr(i, separator - i);
            id = source.substr(separator + 1, end - separator - 1);
            i = end + 1;
        } else {
            while (i < source.size() && (std::isalnum(static_cast<unsigned char>(source[i])) || source[i] == '_')) ++i;
            id = source.substr(start, i - start);
        }
        if (!valid_name(owner) || !identifier(id)) fail("Invalid settings reference in " + mod);
        auto found = std::find_if(settings.begin(), settings.end(), [&](const Setting &s) { return s.mod == owner && s.id == id; });
        if (found == settings.end()) fail("Unknown setting " + owner + ":" + id);
        const auto begin = out.size();
        if (i < source.size() && source[i] == '{') {
            if (!found->boolean) fail("Conditional requires a bool setting: " + found->key());
            start = ++i;
            int nesting = 1;
            while (i < source.size() && nesting) { if (source[i] == '{') ++nesting; if (source[i] == '}') --nesting; if (nesting) ++i; }
            if (nesting) fail("Unterminated settings conditional: " + found->key());
            // Validate even an inactive branch so typos cannot hide behind its current value.
            std::vector<Reference> nested;
            auto fragment = expand_source(source.substr(start, i - start), mod, settings, &nested, depth + 1);
            if ((found->value != 0) != negate) out += fragment;
            if (references) for (const auto &ref : nested) references->push_back({begin, out.size(), ref.key});
            ++i;
        } else {
            if (negate) fail("Negated setting requires a conditional fragment: " + found->key());
            out += found->boolean ? (found->value ? "true" : "false") : std::to_string(found->value);
        }
        if (references) references->push_back({begin, out.size(), found->key()});
    }
    return out;
}

void stamp(Node &node, const std::string &origin, const std::vector<Reference> &refs)
{
    node.origin = origin;
    for (const auto &ref : refs) if (ref.begin >= node.begin && ref.begin <= node.end) node.settings.insert(ref.key);
    for (const auto &attribute : node.attribute_ranges) for (const auto &ref : refs) {
        if (ref.begin >= attribute.second.first && ref.begin <= attribute.second.second) node.attribute_settings[attribute.first].insert(ref.key);
    }
    for (auto &child : node.children) stamp(child, origin, refs);
}

std::string definition_key(const Node &node, const std::filesystem::path &relative)
{
    std::string identity = node.attribute("type", node.attribute("id", node.attribute("name")));
    if (identity.empty()) identity = path_text(relative.stem());
    const auto category = path_text(*relative.begin());
    // Buildings can live in both BuildingType and Tiles.
    return (node.name == "building" ? "BuildingType" : category) + ":" + node.name + ":" + identity;
}
}

std::string Node::attribute(const std::string &key, const std::string &fallback) const
{
    auto it = attributes.find(key); return it == attributes.end() ? fallback : it->second;
}
const Node *Node::child(const std::string &key) const
{
    auto it = std::find_if(children.begin(), children.end(), [&](const Node &n) { return n.name == key; });
    return it == children.end() ? nullptr : &*it;
}

Node parse(const std::string &xml)
{
    if (xml.size() > 32 * 1024 * 1024) fail("XML document exceeds 32 MiB");
    std::vector<sxmltok_t> tokens(256);
    sxml_t parser;
    sxml_init(&parser);
    sxmlerr_t result;
    while ((result = sxml_parse(&parser, xml.data(), static_cast<unsigned>(xml.size()), tokens.data(), static_cast<unsigned>(tokens.size()))) == SXML_ERROR_TOKENSFULL) {
        tokens.resize(tokens.size() * 2);
    }
    if (result != SXML_SUCCESS || parser.taglevel != 0) fail("Invalid or incomplete XML");
    Node root;
    std::vector<Node *> stack;
    bool seen = false;
    for (unsigned i = 0; i < parser.ntokens; ++i) {
        const auto &t = tokens[i];
        const auto value = xml.substr(t.startpos, t.endpos - t.startpos);
        if (t.type == SXML_STARTTAG) {
            Node n; n.name = value; n.begin = t.startpos - 1;
            auto last = i + t.size;
            while (i < last) {
                const auto &a = tokens[++i];
                if (a.type != SXML_CDATA) fail("Invalid XML attribute");
                auto key = xml.substr(a.startpos, a.endpos - a.startpos);
                std::string val;
                const std::size_t begin = a.startpos;
                while (i < last && tokens[i + 1].type == SXML_CHARACTER) { const auto &v = tokens[++i]; val += xml.substr(v.startpos, v.endpos - v.startpos); }
                if (!n.attributes.emplace(key, decoded(val)).second) fail("Duplicate XML attribute " + key);
                n.attribute_ranges[key] = {begin, tokens[i].endpos};
            }
            if (stack.empty()) { if (seen) fail("Multiple XML roots"); root = std::move(n); stack.push_back(&root); seen = true; }
            else { stack.back()->children.push_back(std::move(n)); stack.push_back(&stack.back()->children.back()); }
            if (stack.size() > 128) fail("XML nesting exceeds 128");
        } else if (t.type == SXML_ENDTAG) {
            if (stack.empty() || (!value.empty() && stack.back()->name != value)) fail("Mismatched XML closing tag " + value);
            // sxml's synthetic end token for <field/> points back at its name.
            // Find the actual tag end, respecting '>' inside attribute values.
            std::size_t close = t.endpos;
            char quote = 0;
            for (; close < xml.size(); ++close) {
                const char c = xml[close];
                if (quote) { if (c == quote) quote = 0; }
                else if (c == '\'' || c == '"') quote = c;
                else if (c == '>') break;
            }
            stack.back()->end = close;
            stack.pop_back();
        } else if (t.type == SXML_DOCTYPE) fail("DOCTYPE is not supported in mod definitions");
        else if (t.type == SXML_INSTRUCTION || t.type == SXML_COMMENT) i += t.size;
        else if (t.type == SXML_CHARACTER || t.type == SXML_CDATA) {
            if (!stack.empty()) stack.back()->text += t.type == SXML_CDATA ? value : decoded(value);
            else if (value.find_first_not_of(" \t\r\n") != std::string::npos) fail("Text outside XML root");
        }
    }
    if (!seen || !stack.empty()) fail("Incomplete XML document");
    if (xml.find_first_not_of(" \t\r\n", parser.bufferpos) != std::string::npos) fail("Content after XML root");
    return root;
}

std::string serialize(const Node &node)
{
    std::string out = "<" + node.name;
    for (const auto &a : node.attributes) out += " " + a.first + "=\"" + escaped(a.second) + "\"";
    if (node.children.empty() && node.text.find_first_not_of(" \t\r\n") == std::string::npos) return out + "/>";
    out += ">";
    if (node.text.find_first_not_of(" \t\r\n") != std::string::npos) out += escaped(node.text);
    for (const auto &child : node.children) out += serialize(child);
    return out + "</" + node.name + ">";
}

std::string read(const std::filesystem::path &path)
{
    std::ifstream file(path, std::ios::binary);
    if (!file) fail("Cannot read " + path_text(path));
    file.seekg(0, std::ios::end);
    auto size = file.tellg();
    if (size < 0 || size > 32 * 1024 * 1024) fail("Invalid file size: " + path_text(path));
    file.seekg(0);
    std::string text(static_cast<std::size_t>(size), '\0');
    if (!file.read(text.data(), size)) fail("Incomplete read: " + path_text(path));
    return text;
}

void write_atomic(const std::filesystem::path &path, const std::string &text)
{
    if (!path.parent_path().empty()) std::filesystem::create_directories(path.parent_path());
    static std::atomic<unsigned int> sequence{0};
    auto temp = path; temp += ".tmp." + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) + "." + std::to_string(sequence++);
    struct Cleanup { std::filesystem::path path; ~Cleanup() { std::error_code ignored; std::filesystem::remove(path, ignored); } } cleanup{temp};
    { std::ofstream file(temp, std::ios::binary | std::ios::trunc); file.write(text.data(), static_cast<std::streamsize>(text.size())); file.flush(); if (!file) fail("Cannot save " + path_text(path)); }
#ifdef _WIN32
    if (!MoveFileExW(temp.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) fail("Cannot replace " + path_text(path));
#else
    std::filesystem::rename(temp, path);
#endif
}

std::string lower(std::string value) { for (char &c : value) if (c >= 'A' && c <= 'Z') c += 'a' - 'A'; return value; }
std::string path_text(const std::filesystem::path &path) { const auto bytes = path.u8string(); return {bytes.begin(), bytes.end()}; }
std::filesystem::path utf8_path(const std::string &path)
{
#ifdef __cpp_char8_t
    return std::filesystem::path(std::u8string(path.begin(), path.end()));
#else
    return std::filesystem::u8path(path);
#endif
}
std::string path_key(const std::filesystem::path &path) { const auto bytes = std::filesystem::absolute(path).lexically_normal().generic_u8string(); return lower({bytes.begin(), bytes.end()}); }
bool valid_name(const std::string &name)
{
    return !name.empty() && name != "." && name != ".." && name.back() != '.' && name.back() != ' ' && name.find_first_of("/\\:<>\"|?*") == std::string::npos && std::none_of(name.begin(), name.end(), [](unsigned char c) { return c < 32; });
}

Node overlay(Node base, const Node &upper)
{
    if (base.name != upper.name) fail("Cannot overlay different definition roots");
    if (upper.attribute("disabled") == "true" || base.attribute("disabled") == "true") return upper;
    for (const auto &a : upper.attributes) {
        base.attributes[a.first] = a.second;
        auto refs = upper.attribute_settings.find(a.first);
        if (refs == upper.attribute_settings.end()) base.attribute_settings.erase(a.first);
        else base.attribute_settings[a.first] = refs->second;
    }
    // Replacing a field replaces ALL its descendants. Empty is an authored value.
    std::set<std::string> replaced;
    for (const auto &n : upper.children) replaced.insert(field_key(n));
    base.children.erase(std::remove_if(base.children.begin(), base.children.end(), [&](const Node &n) { return replaced.count(field_key(n)) != 0; }), base.children.end());
    base.children.insert(base.children.end(), upper.children.begin(), upper.children.end());
    base.origin = upper.origin;
    base.settings = upper.settings;
    base.text = upper.text;
    return base;
}

Manifest manifest(const std::filesystem::path &path)
{
    Node root = parse(read(path));
    if (root.name != "mod") fail("Expected mod root: " + path_text(path));
    Manifest out;
    std::set<std::string> seen;
    for (const auto &n : root.children) {
        if (!seen.insert(n.name).second) fail("Duplicate mod field " + n.name);
        if (n.name == "name") out.name = n.attribute("value");
        else if (n.name == "description") out.description = n.attribute("value");
        else if (n.name == "version") out.version = n.attribute("value");
        else if (n.name == "dependencies") {
            std::set<std::string> deps;
            for (const auto &d : n.children) {
                auto name = d.attribute("name");
                if (d.name != "mod" || !valid_name(name) || !deps.insert(lower(name)).second) fail("Invalid or duplicate mod dependency");
                out.dependencies.push_back(name);
            }
        } else if (n.name == "settings") {
            std::set<std::string> ids;
            for (const auto &s : n.children) {
                Setting setting;
                setting.id = s.attribute("id"); setting.name = s.attribute("name"); setting.category = s.attribute("category", "General"); setting.description = s.attribute("description");
                if (s.name != "setting" || !identifier(setting.id) || setting.name.empty() || !ids.insert(setting.id).second) fail("Invalid or duplicate mod setting");
                auto type = s.attribute("type");
                if (type == "bool") {
                    const auto def = s.attribute("default");
                    if (def != "true" && def != "false") fail("Bool default must be true or false");
                    setting.default_value = def == "true";
                } else if (type.size() > 6 && type.substr(0, 4) == "int(" && type.back() == ')') {
                    const auto comma = type.find(',');
                    if (comma == std::string::npos) fail("Integer setting requires int(min,max)");
                    setting.boolean = false;
                    setting.minimum = integer(type.substr(4, comma - 4));
                    setting.maximum = integer(type.substr(comma + 1, type.size() - comma - 2));
                    setting.default_value = integer(s.attribute("default"));
                    if (setting.minimum > setting.maximum || setting.default_value < setting.minimum || setting.default_value > setting.maximum) fail("Integer setting default is outside its bounds");
                } else fail("Unsupported setting type: " + type);
                setting.value = setting.default_value; out.settings.push_back(std::move(setting));
            }
        } else fail("Unsupported mod field: " + n.name);
    }
    if (!valid_name(out.name) || out.description.empty() || out.version.empty() || !seen.count("dependencies")) fail("Incomplete mod metadata: " + path_text(path));
    for (const auto &dep : out.dependencies) if (lower(dep) == lower(out.name)) fail("A mod cannot depend on itself");
    for (auto &s : out.settings) s.mod = out.name;
    return out;
}

std::vector<std::string> read_list(const std::filesystem::path &path)
{
    Node root = parse(read(path));
    if (root.name != "mod_list") fail("Expected mod_list root");
    std::vector<std::string> mods;
    for (const auto &n : root.children) { auto name = n.attribute("name"); if (n.name != "mod" || !valid_name(name)) fail("Invalid mod-list entry"); mods.push_back(name); }
    return mods;
}
void write_list(const std::filesystem::path &path, const std::vector<std::string> &mods)
{
    Node root; root.name = "mod_list";
    for (const auto &name : mods) { if (!valid_name(name)) fail("Invalid mod name"); Node n; n.name = "mod"; n.attributes["name"] = name; root.children.push_back(n); }
    write_atomic(path, serialize(root) + "\r\n");
}

void Session::load(const std::vector<Layer> &layers, const std::filesystem::path &values_file, bool require_manifests)
{
    layers_ = layers; values_file_ = values_file; settings_.clear(); files_.clear(); saved_values_.clear();
    if (!values_file.empty() && std::filesystem::exists(values_file)) {
        const auto values = parse(read(values_file));
        if (values.name != "mod_settings") fail("Expected mod_settings root");
        for (const auto &m : values.children) for (const auto &s : m.children) {
            if (m.name != "mod" || s.name != "setting" || !valid_name(m.attribute("name")) || !identifier(s.attribute("id"))) fail("Invalid saved setting");
            if (!saved_values_.emplace(m.attribute("name") + ":" + s.attribute("id"), integer(s.attribute("value"))).second) fail("Duplicate saved setting");
        }
    }
    for (const auto &layer : layers_) {
        auto path = layer.root / "mod.xml";
        if (!require_manifests && !std::filesystem::exists(path)) continue;
        auto m = manifest(path);
        if (m.name != layer.name) fail("Manifest name does not match folder: " + layer.name);
        for (auto s : m.settings) {
            auto saved = saved_values_.find(s.key());
            if (saved != saved_values_.end()) {
                if (saved->second < s.minimum || saved->second > s.maximum) fail("Saved setting out of range: " + s.key());
                s.value = saved->second;
            }
            settings_.push_back(std::move(s));
        }
    }
    compile();
}

void Session::compile()
{
    std::map<std::string, Node> merged;
    std::map<std::string, std::string> files;
    std::set<std::string> all_refs;
    for (const auto &layer : layers_) {
        if (!std::filesystem::exists(layer.root)) continue;
        std::vector<std::filesystem::path> paths;
        for (auto it = std::filesystem::recursive_directory_iterator(layer.root); it != std::filesystem::recursive_directory_iterator(); ++it) {
            const auto &entry = *it;
            const auto relative = entry.path().lexically_relative(layer.root);
            const auto category = path_text(*relative.begin());
            // Graphics use their existing specialized compositor; text/UI documents keep their own grammar.
            if (category == "Graphics" || category == "UI" || category == "Localization") {
                if (entry.is_directory()) it.disable_recursion_pending();
                continue;
            }
            if (entry.is_regular_file() && lower(path_text(entry.path().extension())) == ".xml" && relative != "mod.xml") paths.push_back(entry.path());
        }
        std::sort(paths.begin(), paths.end());
        std::set<std::string> layer_ids;
        for (const auto &path : paths) {
            try {
                std::vector<Reference> refs;
                auto source = expand_source(read(path), layer.name, settings_, &refs);
                Node node = parse(source);
                stamp(node, layer.name, refs);
                for (const auto &r : refs) all_refs.insert(r.key);
                const auto key = definition_key(node, path.lexically_relative(layer.root));
                if (!layer_ids.insert(key).second) fail("Duplicate definition identity " + key);
                auto previous = merged.find(key);
                if (previous != merged.end()) node = overlay(previous->second, node);
                merged[key] = node;
                std::string document = serialize(node);
                // The runtime's streaming parser requires an explicit closing root token.
                if (node.children.empty() && node.text.find_first_not_of(" \t\r\n") == std::string::npos) document = document.substr(0, document.size() - 2) + "></" + node.name + ">";
                files[path_key(path)] = document + "\n";
            } catch (const std::exception &e) { fail(path_text(path) + ": " + e.what()); }
        }
    }
    std::set<std::string> live;
    for (const auto &entry : merged) {
        const auto &root = entry.second;
        if (root.attribute("disabled") == "true") continue;
        // Root references belonging to replaced children must not keep settings active.
        for (const auto &n : root.children) live.insert(n.settings.begin(), n.settings.end());
        for (const auto &a : root.attribute_settings) live.insert(a.second.begin(), a.second.end());
    }
    for (auto &s : settings_) {
        s.effective = live.count(s.key()) != 0;
        s.disabled_reason = s.effective ? "" : all_refs.count(s.key()) ? "Overridden by later mod fields in this stack" : "No active definition uses this setting";
    }
    files_ = std::move(files);
}

void Session::set(const std::string &key, int value)
{
    auto it = std::find_if(settings_.begin(), settings_.end(), [&](const Setting &s) { return s.key() == key; });
    if (it == settings_.end()) fail("Unknown setting: " + key);
    if (!it->effective) fail("Setting is overridden: " + key);
    if (value < it->minimum || value > it->maximum) fail("Setting is out of range: " + key);
    const int previous = it->value; it->value = value;
    try { compile(); } catch (...) { it->value = previous; throw; }
}

void Session::save() const
{
    auto values = saved_values_;
    for (const auto &s : settings_) values[s.key()] = s.value;
    Node root; root.name = "mod_settings";
    std::map<std::string, Node> mods;
    for (const auto &entry : values) {
        const auto split = entry.first.find(':');
        auto &mod = mods[entry.first.substr(0, split)]; mod.name = "mod"; mod.attributes["name"] = entry.first.substr(0, split);
        Node setting; setting.name = "setting"; setting.attributes["id"] = entry.first.substr(split + 1); setting.attributes["value"] = std::to_string(entry.second); mod.children.push_back(setting);
    }
    for (const auto &entry : mods) root.children.push_back(entry.second);
    if (!values_file_.empty()) write_atomic(values_file_, serialize(root) + "\r\n");
}
const std::string *Session::file(const std::filesystem::path &path) const { auto found = files_.find(path_key(path)); return found == files_.end() ? nullptr : &found->second; }
std::string Session::expand(const std::string &source, const std::string &mod) const { return expand_source(source, mod, settings_, nullptr); }
Session &runtime() { static Session session; return session; }
const std::string *resolved_file(const char *path) { return path && *path ? runtime().file(utf8_path(path)) : nullptr; }
}
