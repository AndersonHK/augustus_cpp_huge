#include "translation/localization_internal.h"

extern "C" {
#include "building/type.h"
}

namespace localization::detail {

namespace {

struct legacy_project_key_mapping {
    int is_editor;
    int group;
    int index;
    translation_key key;
};

constexpr legacy_project_key_mapping k_legacy_project_key_mappings[] = {
    {0, 28, BUILDING_HOUSE_SMALL_TENT, TR_BUILDING_HOUSE_SMALL_TENT},
    {0, 28, BUILDING_HOUSE_LARGE_TENT, TR_BUILDING_HOUSE_LARGE_TENT},
    {0, 28, BUILDING_HOUSE_SMALL_SHACK, TR_BUILDING_HOUSE_SMALL_SHACK},
    {0, 28, BUILDING_HOUSE_LARGE_SHACK, TR_BUILDING_HOUSE_LARGE_SHACK},
    {0, 28, BUILDING_HOUSE_SMALL_HOVEL, TR_BUILDING_HOUSE_SMALL_HOVEL},
    {0, 28, BUILDING_HOUSE_LARGE_HOVEL, TR_BUILDING_HOUSE_LARGE_HOVEL},
    {0, 28, BUILDING_HOUSE_SMALL_CASA, TR_BUILDING_HOUSE_SMALL_CASA},
    {0, 28, BUILDING_HOUSE_LARGE_CASA, TR_BUILDING_HOUSE_LARGE_CASA},
    {0, 28, BUILDING_HOUSE_SMALL_INSULA, TR_BUILDING_HOUSE_SMALL_INSULA},
    {0, 28, BUILDING_HOUSE_MEDIUM_INSULA, TR_BUILDING_HOUSE_MEDIUM_INSULA},
    {0, 28, BUILDING_HOUSE_LARGE_INSULA, TR_BUILDING_HOUSE_LARGE_INSULA},
    {0, 28, BUILDING_HOUSE_GRAND_INSULA, TR_BUILDING_HOUSE_GRAND_INSULA},
    {0, 28, BUILDING_HOUSE_SMALL_VILLA, TR_BUILDING_HOUSE_SMALL_VILLA},
    {0, 28, BUILDING_HOUSE_MEDIUM_VILLA, TR_BUILDING_HOUSE_MEDIUM_VILLA},
    {0, 28, BUILDING_HOUSE_LARGE_VILLA, TR_BUILDING_HOUSE_LARGE_VILLA},
    {0, 28, BUILDING_HOUSE_GRAND_VILLA, TR_BUILDING_HOUSE_GRAND_VILLA},
    {0, 28, BUILDING_HOUSE_SMALL_PALACE, TR_BUILDING_HOUSE_SMALL_PALACE},
    {0, 28, BUILDING_HOUSE_MEDIUM_PALACE, TR_BUILDING_HOUSE_MEDIUM_PALACE},
    {0, 28, BUILDING_HOUSE_LARGE_PALACE, TR_BUILDING_HOUSE_LARGE_PALACE},
    {0, 28, BUILDING_HOUSE_LUXURY_PALACE, TR_BUILDING_HOUSE_LUXURY_PALACE},
    {0, 41, BUILDING_HOUSE_SMALL_TENT, TR_BUILDING_HOUSE_SMALL_TENT},
    {0, 41, BUILDING_HOUSE_LARGE_TENT, TR_BUILDING_HOUSE_LARGE_TENT},
    {0, 41, BUILDING_HOUSE_SMALL_SHACK, TR_BUILDING_HOUSE_SMALL_SHACK},
    {0, 41, BUILDING_HOUSE_LARGE_SHACK, TR_BUILDING_HOUSE_LARGE_SHACK},
    {0, 41, BUILDING_HOUSE_SMALL_HOVEL, TR_BUILDING_HOUSE_SMALL_HOVEL},
    {0, 41, BUILDING_HOUSE_LARGE_HOVEL, TR_BUILDING_HOUSE_LARGE_HOVEL},
    {0, 41, BUILDING_HOUSE_SMALL_CASA, TR_BUILDING_HOUSE_SMALL_CASA},
    {0, 41, BUILDING_HOUSE_LARGE_CASA, TR_BUILDING_HOUSE_LARGE_CASA},
    {0, 41, BUILDING_HOUSE_SMALL_INSULA, TR_BUILDING_HOUSE_SMALL_INSULA},
    {0, 41, BUILDING_HOUSE_MEDIUM_INSULA, TR_BUILDING_HOUSE_MEDIUM_INSULA},
    {0, 41, BUILDING_HOUSE_LARGE_INSULA, TR_BUILDING_HOUSE_LARGE_INSULA},
    {0, 41, BUILDING_HOUSE_GRAND_INSULA, TR_BUILDING_HOUSE_GRAND_INSULA},
    {0, 41, BUILDING_HOUSE_SMALL_VILLA, TR_BUILDING_HOUSE_SMALL_VILLA},
    {0, 41, BUILDING_HOUSE_MEDIUM_VILLA, TR_BUILDING_HOUSE_MEDIUM_VILLA},
    {0, 41, BUILDING_HOUSE_LARGE_VILLA, TR_BUILDING_HOUSE_LARGE_VILLA},
    {0, 41, BUILDING_HOUSE_GRAND_VILLA, TR_BUILDING_HOUSE_GRAND_VILLA},
    {0, 41, BUILDING_HOUSE_SMALL_PALACE, TR_BUILDING_HOUSE_SMALL_PALACE},
    {0, 41, BUILDING_HOUSE_MEDIUM_PALACE, TR_BUILDING_HOUSE_MEDIUM_PALACE},
    {0, 41, BUILDING_HOUSE_LARGE_PALACE, TR_BUILDING_HOUSE_LARGE_PALACE},
    {0, 41, BUILDING_HOUSE_LUXURY_PALACE, TR_BUILDING_HOUSE_LUXURY_PALACE},
    {0, 28, BUILDING_ROADBLOCK, TR_BUILDING_ROADBLOCK},
    {0, 41, BUILDING_ROADBLOCK, TR_BUILDING_ROADBLOCK},
    {0, 28, BUILDING_MENU_GRAND_TEMPLES, TR_BUILDING_GRAND_TEMPLE_MENU},
    {0, 41, BUILDING_MENU_GRAND_TEMPLES, TR_BUILDING_GRAND_TEMPLE_MENU},
    {0, 28, BUILDING_MESS_HALL, TR_BUILDING_MESS_HALL},
    {0, 41, BUILDING_MESS_HALL, TR_BUILDING_MESS_HALL},
    {0, 28, BUILDING_MENU_TREES, TR_BUILDING_MENU_TREES},
    {0, 41, BUILDING_MENU_TREES, TR_BUILDING_MENU_TREES},
    {0, 28, BUILDING_MENU_PATHS, TR_BUILDING_MENU_PATHS},
    {0, 41, BUILDING_MENU_PATHS, TR_BUILDING_MENU_PATHS},
    {0, 28, BUILDING_MENU_PARKS, TR_BUILDING_MENU_PARKS},
    {0, 41, BUILDING_MENU_PARKS, TR_BUILDING_MENU_PARKS},
    {0, 28, BUILDING_PINE_TREE, TR_BUILDING_PINE_TREE},
    {0, 41, BUILDING_PINE_TREE, TR_BUILDING_PINE_TREE},
    {0, 28, BUILDING_FIR_TREE, TR_BUILDING_FIR_TREE},
    {0, 41, BUILDING_FIR_TREE, TR_BUILDING_FIR_TREE},
    {0, 28, BUILDING_OAK_TREE, TR_BUILDING_OAK_TREE},
    {0, 41, BUILDING_OAK_TREE, TR_BUILDING_OAK_TREE},
    {0, 28, BUILDING_ELM_TREE, TR_BUILDING_ELM_TREE},
    {0, 41, BUILDING_ELM_TREE, TR_BUILDING_ELM_TREE},
    {0, 28, BUILDING_FIG_TREE, TR_BUILDING_FIG_TREE},
    {0, 41, BUILDING_FIG_TREE, TR_BUILDING_FIG_TREE},
    {0, 28, BUILDING_PLUM_TREE, TR_BUILDING_PLUM_TREE},
    {0, 41, BUILDING_PLUM_TREE, TR_BUILDING_PLUM_TREE},
    {0, 28, BUILDING_PALM_TREE, TR_BUILDING_PALM_TREE},
    {0, 41, BUILDING_PALM_TREE, TR_BUILDING_PALM_TREE},
    {0, 28, BUILDING_DATE_TREE, TR_BUILDING_DATE_TREE},
    {0, 41, BUILDING_DATE_TREE, TR_BUILDING_DATE_TREE},
    {0, 28, BUILDING_PINE_PATH, TR_BUILDING_PINE_PATH},
    {0, 41, BUILDING_PINE_PATH, TR_BUILDING_PINE_PATH},
    {0, 28, BUILDING_FIR_PATH, TR_BUILDING_FIR_PATH},
    {0, 41, BUILDING_FIR_PATH, TR_BUILDING_FIR_PATH},
    {0, 28, BUILDING_OAK_PATH, TR_BUILDING_OAK_PATH},
    {0, 41, BUILDING_OAK_PATH, TR_BUILDING_OAK_PATH},
    {0, 28, BUILDING_ELM_PATH, TR_BUILDING_ELM_PATH},
    {0, 41, BUILDING_ELM_PATH, TR_BUILDING_ELM_PATH},
    {0, 28, BUILDING_FIG_PATH, TR_BUILDING_FIG_PATH},
    {0, 41, BUILDING_FIG_PATH, TR_BUILDING_FIG_PATH},
    {0, 28, BUILDING_PLUM_PATH, TR_BUILDING_PLUM_PATH},
    {0, 41, BUILDING_PLUM_PATH, TR_BUILDING_PLUM_PATH},
    {0, 28, BUILDING_PALM_PATH, TR_BUILDING_PALM_PATH},
    {0, 41, BUILDING_PALM_PATH, TR_BUILDING_PALM_PATH},
    {0, 28, BUILDING_DATE_PATH, TR_BUILDING_DATE_PATH},
    {0, 41, BUILDING_DATE_PATH, TR_BUILDING_DATE_PATH},
    {0, 28, BUILDING_PAVILION_BLUE, TR_BUILDING_BLUE_PAVILION},
    {0, 41, BUILDING_PAVILION_BLUE, TR_BUILDING_BLUE_PAVILION},
    {0, 28, BUILDING_PAVILION_RED, TR_BUILDING_RED_PAVILION},
    {0, 41, BUILDING_PAVILION_RED, TR_BUILDING_RED_PAVILION},
    {0, 28, BUILDING_PAVILION_ORANGE, TR_BUILDING_ORANGE_PAVILION},
    {0, 41, BUILDING_PAVILION_ORANGE, TR_BUILDING_ORANGE_PAVILION},
    {0, 28, BUILDING_PAVILION_YELLOW, TR_BUILDING_YELLOW_PAVILION},
    {0, 41, BUILDING_PAVILION_YELLOW, TR_BUILDING_YELLOW_PAVILION},
    {0, 28, BUILDING_PAVILION_GREEN, TR_BUILDING_GREEN_PAVILION},
    {0, 41, BUILDING_PAVILION_GREEN, TR_BUILDING_GREEN_PAVILION},
    {0, 28, BUILDING_GODDESS_STATUE, TR_BUILDING_SMALL_STATUE_ALT},
    {0, 41, BUILDING_GODDESS_STATUE, TR_BUILDING_SMALL_STATUE_ALT},
    {0, 28, BUILDING_SENATOR_STATUE, TR_BUILDING_SMALL_STATUE_ALT_B},
    {0, 41, BUILDING_SENATOR_STATUE, TR_BUILDING_SMALL_STATUE_ALT_B},
    {0, 28, BUILDING_OBELISK, TR_BUILDING_OBELISK},
    {0, 41, BUILDING_OBELISK, TR_BUILDING_OBELISK},
    {0, 28, BUILDING_MENU_GOV_RES, TR_BUILDING_MENU_GOV_RES},
    {0, 41, BUILDING_MENU_GOV_RES, TR_BUILDING_MENU_GOV_RES},
    {0, 28, BUILDING_MENU_STATUES, TR_BUILDING_MENU_STATUES},
    {0, 41, BUILDING_MENU_STATUES, TR_BUILDING_MENU_STATUES},
    {0, 28, BUILDING_GRAND_GARDEN, TR_BUILDING_GRAND_GARDEN},
    {0, 41, BUILDING_GRAND_GARDEN, TR_BUILDING_GRAND_GARDEN},
    {0, 28, BUILDING_HORSE_STATUE, TR_BUILDING_HORSE_STATUE},
    {0, 41, BUILDING_HORSE_STATUE, TR_BUILDING_HORSE_STATUE},
    {0, 28, BUILDING_DOLPHIN_FOUNTAIN, TR_BUILDING_DOLPHIN_FOUNTAIN},
    {0, 41, BUILDING_DOLPHIN_FOUNTAIN, TR_BUILDING_DOLPHIN_FOUNTAIN},
    {0, 28, BUILDING_HEDGE_DARK, TR_BUILDING_HEDGE_DARK},
    {0, 41, BUILDING_HEDGE_DARK, TR_BUILDING_HEDGE_DARK},
    {0, 28, BUILDING_HEDGE_LIGHT, TR_BUILDING_HEDGE_LIGHT},
    {0, 41, BUILDING_HEDGE_LIGHT, TR_BUILDING_HEDGE_LIGHT},
    {0, 28, BUILDING_LOOPED_GARDEN_WALL, TR_BUILDING_GARDEN_WALL},
    {0, 41, BUILDING_LOOPED_GARDEN_WALL, TR_BUILDING_GARDEN_WALL},
    {0, 28, BUILDING_LEGION_STATUE, TR_BUILDING_LEGION_STATUE},
    {0, 41, BUILDING_LEGION_STATUE, TR_BUILDING_LEGION_STATUE},
    {0, 28, BUILDING_DECORATIVE_COLUMN, TR_BUILDING_DECORATIVE_COLUMN},
    {0, 41, BUILDING_DECORATIVE_COLUMN, TR_BUILDING_DECORATIVE_COLUMN},
    {0, 28, BUILDING_COLONNADE, TR_BUILDING_COLONNADE},
    {0, 41, BUILDING_COLONNADE, TR_BUILDING_COLONNADE},
    {0, 28, BUILDING_GARDEN_PATH, TR_BUILDING_GARDEN_PATH},
    {0, 41, BUILDING_GARDEN_PATH, TR_BUILDING_GARDEN_PATH},
    {0, 28, BUILDING_LARARIUM, TR_BUILDING_LARARIUM},
    {0, 41, BUILDING_LARARIUM, TR_BUILDING_LARARIUM},
    {0, 28, BUILDING_NYMPHAEUM, TR_BUILDING_NYMPHAEUM},
    {0, 41, BUILDING_NYMPHAEUM, TR_BUILDING_NYMPHAEUM},
    {0, 28, BUILDING_SMALL_MAUSOLEUM, TR_BUILDING_SMALL_MAUSOLEUM},
    {0, 41, BUILDING_SMALL_MAUSOLEUM, TR_BUILDING_SMALL_MAUSOLEUM},
    {0, 28, BUILDING_LARGE_MAUSOLEUM, TR_BUILDING_LARGE_MAUSOLEUM},
    {0, 41, BUILDING_LARGE_MAUSOLEUM, TR_BUILDING_LARGE_MAUSOLEUM},
    {0, 28, BUILDING_CARAVANSERAI, TR_BUILDING_CARAVANSERAI},
    {0, 41, BUILDING_CARAVANSERAI, TR_BUILDING_CARAVANSERAI},
    {0, 28, BUILDING_ROOFED_GARDEN_WALL, TR_BUILDING_ROOFED_GARDEN_WALL},
    {0, 41, BUILDING_ROOFED_GARDEN_WALL, TR_BUILDING_ROOFED_GARDEN_WALL},
    {0, 28, BUILDING_ROOFED_GARDEN_WALL_GATE, TR_BUILDING_GARDEN_WALL_GATE},
    {0, 41, BUILDING_ROOFED_GARDEN_WALL_GATE, TR_BUILDING_GARDEN_WALL_GATE},
    {0, 28, BUILDING_PALISADE, TR_BUILDING_PALISADE},
    {0, 41, BUILDING_PALISADE, TR_BUILDING_PALISADE},
    {0, 28, BUILDING_GLADIATOR_STATUE, TR_BUILDING_GLADIATOR_STATUE},
    {0, 41, BUILDING_GLADIATOR_STATUE, TR_BUILDING_GLADIATOR_STATUE},
    {0, 28, BUILDING_HIGHWAY, TR_BUILDING_HIGHWAY},
    {0, 41, BUILDING_HIGHWAY, TR_BUILDING_HIGHWAY},
    {0, 28, BUILDING_CITY_MINT, TR_BUILDING_CITY_MINT},
    {0, 41, BUILDING_CITY_MINT, TR_BUILDING_CITY_MINT},
    {0, 28, BUILDING_DEPOT, TR_BUILDING_DEPOT},
    {0, 41, BUILDING_DEPOT, TR_BUILDING_DEPOT},
    {0, 28, BUILDING_LOOPED_GARDEN_GATE, TR_BUILDING_LOOPED_GARDEN_WALL_GATE},
    {0, 41, BUILDING_LOOPED_GARDEN_GATE, TR_BUILDING_LOOPED_GARDEN_WALL_GATE},
    {0, 28, BUILDING_PANELLED_GARDEN_WALL, TR_BUILDING_PANELLED_GARDEN_WALL},
    {0, 41, BUILDING_PANELLED_GARDEN_WALL, TR_BUILDING_PANELLED_GARDEN_WALL},
    {0, 28, BUILDING_PANELLED_GARDEN_GATE, TR_BUILDING_PANELLED_GARDEN_WALL_GATE},
    {0, 41, BUILDING_PANELLED_GARDEN_GATE, TR_BUILDING_PANELLED_GARDEN_WALL_GATE},
    {0, 28, BUILDING_SHRINE_CERES, TR_BUILDING_SHRINE_CERES},
    {0, 41, BUILDING_SHRINE_CERES, TR_BUILDING_SHRINE_CERES},
    {0, 28, BUILDING_SHRINE_MARS, TR_BUILDING_SHRINE_MARS},
    {0, 41, BUILDING_SHRINE_MARS, TR_BUILDING_SHRINE_MARS},
    {0, 28, BUILDING_SHRINE_MERCURY, TR_BUILDING_SHRINE_MERCURY},
    {0, 41, BUILDING_SHRINE_MERCURY, TR_BUILDING_SHRINE_MERCURY},
    {0, 28, BUILDING_SHRINE_NEPTUNE, TR_BUILDING_SHRINE_NEPTUNE},
    {0, 41, BUILDING_SHRINE_NEPTUNE, TR_BUILDING_SHRINE_NEPTUNE},
    {0, 28, BUILDING_SHRINE_VENUS, TR_BUILDING_SHRINE_VENUS},
    {0, 41, BUILDING_SHRINE_VENUS, TR_BUILDING_SHRINE_VENUS},
    {0, 28, BUILDING_MENU_SHRINES, TR_BUILDING_MENU_SHRINES},
    {0, 41, BUILDING_MENU_SHRINES, TR_BUILDING_MENU_SHRINES},
    {0, 28, BUILDING_MENU_GARDENS, TR_BUILDING_FORMAL_GARDENS},
    {0, 28, BUILDING_GARDENS, TR_BUILDING_FORMAL_GARDENS},
    {0, 28, BUILDING_OVERGROWN_GARDENS, TR_BUILDING_OVERGROWN_GARDENS},
    {0, 41, BUILDING_OVERGROWN_GARDENS, TR_BUILDING_OVERGROWN_GARDENS},
    {0, 28, BUILDING_FORT_AUXILIA_INFANTRY, TR_BUILDING_FORT_AUXILIA_INFANTRY},
    {0, 41, BUILDING_FORT_AUXILIA_INFANTRY, TR_BUILDING_FORT_AUXILIA_INFANTRY},
    {0, 28, BUILDING_MENU_FORT, TR_BUILDING_FORT_MENU},
    {0, 41, BUILDING_MENU_FORT, TR_BUILDING_FORT_MENU},
    {0, 28, BUILDING_FORT_ARCHERS, TR_BUILDING_FORT_ARCHERS},
    {0, 41, BUILDING_FORT_ARCHERS, TR_BUILDING_FORT_ARCHERS},
    {0, 28, BUILDING_FORT_LEGIONARIES, TR_BUILDING_FORT_LEGIONARIES},
    {0, 41, BUILDING_FORT_LEGIONARIES, TR_BUILDING_FORT_LEGIONARIES},
    {0, 28, BUILDING_FORT_MOUNTED, TR_BUILDING_FORT_MOUNTED},
    {0, 41, BUILDING_FORT_MOUNTED, TR_BUILDING_FORT_MOUNTED},
    {0, 28, BUILDING_FORT_JAVELIN, TR_BUILDING_FORT_JAVELIN},
    {0, 41, BUILDING_FORT_JAVELIN, TR_BUILDING_FORT_JAVELIN},
    {0, 28, BUILDING_HEDGE_GATE_DARK, TR_BUILDING_HEDGE_DARK},
    {0, 41, BUILDING_HEDGE_GATE_DARK, TR_BUILDING_HEDGE_DARK},
    {0, 28, BUILDING_HEDGE_GATE_LIGHT, TR_BUILDING_HEDGE_LIGHT},
    {0, 41, BUILDING_HEDGE_GATE_LIGHT, TR_BUILDING_HEDGE_LIGHT},
    {0, 28, BUILDING_PALISADE_GATE, TR_BUILDING_PALISADE_GATE},
    {0, 41, BUILDING_PALISADE_GATE, TR_BUILDING_PALISADE_GATE},
    {0, 28, BUILDING_LATRINES, TR_BUILDING_LATRINES},
    {0, 41, BUILDING_LATRINES, TR_BUILDING_LATRINES},
    {0, 28, BUILDING_NATIVE_HUT_ALT, TR_BUILDING_NATIVE_HUT_ALT},
    {0, 41, BUILDING_NATIVE_HUT_ALT, TR_BUILDING_NATIVE_HUT_ALT},
    {0, 28, BUILDING_NATIVE_DECORATION, TR_BUILDING_NATIVE_DECORATION},
    {0, 41, BUILDING_NATIVE_DECORATION, TR_BUILDING_NATIVE_DECORATION},
    {0, 28, BUILDING_NATIVE_MONUMENT, TR_BUILDING_NATIVE_MONUMENT},
    {0, 41, BUILDING_NATIVE_MONUMENT, TR_BUILDING_NATIVE_MONUMENT},
    {0, 28, BUILDING_NATIVE_WATCHTOWER, TR_BUILDING_NATIVE_WATCHTOWER},
    {0, 41, BUILDING_NATIVE_WATCHTOWER, TR_BUILDING_NATIVE_WATCHTOWER},
    {0, 28, BUILDING_REPAIR_LAND, TR_BUILDING_LAND_REPAIR},
    {0, 41, BUILDING_REPAIR_LAND, TR_BUILDING_LAND_REPAIR},
    {0, 28, BUILDING_CLEAR_TREES, TR_BUILDING_MENU_TREES},
    {0, 41, BUILDING_CLEAR_TREES, TR_BUILDING_MENU_TREES},
    {0, 28, BUILDING_CLEAR_LAND, TR_BUILDING_LAND_CLEAR},
    {0, 41, BUILDING_CLEAR_LAND, TR_BUILDING_LAND_CLEAR},
    {0, 92, 0, TR_BUILDING_SMALL_TEMPLE_CERES_NAME},
    {0, 93, 0, TR_BUILDING_SMALL_TEMPLE_NEPTUNE_NAME},
    {0, 94, 0, TR_BUILDING_SMALL_TEMPLE_MERCURY_NAME},
    {0, 95, 0, TR_BUILDING_SMALL_TEMPLE_MARS_NAME},
    {0, 96, 0, TR_BUILDING_SMALL_TEMPLE_VENUS_NAME},
    {0, 130, 641, TR_PHRASE_FIGURE_MISSIONARY_EXACT_4},
    {0, 67, 48, TR_EDITOR_ALLOWED_BUILDINGS_MONUMENTS},
    {1, 48, TR_EDITOR_SCENARIO_BUILDING_NATIVE_HUT_ALT, TR_EDITOR_SCENARIO_BUILDING_NATIVE_HUT_ALT},
    {1, 48, TR_EDITOR_SCENARIO_BUILDING_NATIVE_DECORATION, TR_EDITOR_SCENARIO_BUILDING_NATIVE_DECORATION},
    {1, 48, TR_EDITOR_SCENARIO_BUILDING_NATIVE_MONUMENT, TR_EDITOR_SCENARIO_BUILDING_NATIVE_MONUMENT},
    {1, 48, TR_EDITOR_SCENARIO_BUILDING_NATIVE_WATCHTOWER, TR_EDITOR_SCENARIO_BUILDING_NATIVE_WATCHTOWER},
    {1, 48, TR_EDITOR_TOOL_EARTHQUAKE_POINT, TR_EDITOR_TOOL_EARTHQUAKE_POINT},
    {1, 48, TR_EDITOR_TOOL_EARTHQUAKE_CUSTOM, TR_EDITOR_TOOL_EARTHQUAKE_CUSTOM},
    {1, 48, TR_EDITOR_TOOL_EARTHQUAKE_REMOVE, TR_EDITOR_TOOL_EARTHQUAKE_REMOVE},
    {1, 48, TR_EDITOR_RUBBLE, TR_EDITOR_RUBBLE},
};

void set_legacy_string_slot(locale_catalog &catalog, int is_editor, int group, int index, const std::string &value)
{
    std::map<int, std::vector<localized_text>> &groups = is_editor ? catalog.editor_strings : catalog.main_strings;
    std::vector<localized_text> &strings = groups[group];
    if (strings.size() <= static_cast<size_t>(index)) {
        strings.resize(index + 1);
    }
    strings[index].utf8 = value;
    if (is_editor) {
        catalog.has_editor_strings = true;
    } else {
        catalog.has_main_strings = true;
    }
}

bool parse_legacy_project_key_name(const std::string &key, int &is_editor, int &group, int &index)
{
    constexpr std::string_view main_prefix = "main_strings.";
    constexpr std::string_view editor_prefix = "editor_strings.";
    std::string_view cursor(key);
    if (cursor.substr(0, main_prefix.size()) == main_prefix) {
        is_editor = 0;
        cursor.remove_prefix(main_prefix.size());
    } else if (cursor.substr(0, editor_prefix.size()) == editor_prefix) {
        is_editor = 1;
        cursor.remove_prefix(editor_prefix.size());
    } else {
        return false;
    }

    const size_t separator = cursor.find('.');
    if (separator == std::string_view::npos || separator == 0 || separator + 1 >= cursor.size()) {
        return false;
    }
    const std::string group_text(cursor.substr(0, separator));
    const std::string index_text(cursor.substr(separator + 1));
    char *end = nullptr;
    const long parsed_group = strtol(group_text.c_str(), &end, 10);
    if (!end || *end || parsed_group < 0 || parsed_group >= kMaxTextEntries) {
        return false;
    }
    const long parsed_index = strtol(index_text.c_str(), &end, 10);
    if (!end || *end || parsed_index < 0 || parsed_index > 100000) {
        return false;
    }
    group = static_cast<int>(parsed_group);
    index = static_cast<int>(parsed_index);
    return true;
}

void merge_project_key_legacy_string_slots(translation_key key, const std::string &value, locale_catalog &catalog)
{
    for (const legacy_project_key_mapping &mapping : k_legacy_project_key_mappings) {
        if (mapping.key == key) {
            set_legacy_string_slot(catalog, mapping.is_editor, mapping.group, mapping.index, value);
        }
    }
}

void detect_project_key_legacy_string_slots(translation_key key, bool &has_main_strings, bool &has_editor_strings)
{
    for (const legacy_project_key_mapping &mapping : k_legacy_project_key_mappings) {
        if (mapping.key == key) {
            if (mapping.is_editor) {
                has_editor_strings = true;
            } else {
                has_main_strings = true;
            }
        }
    }
}

} // namespace

const char *legacy_project_key_name_for_slot(int is_editor, int group, int index)
{
    for (const legacy_project_key_mapping &mapping : k_legacy_project_key_mappings) {
        if (mapping.is_editor == is_editor && mapping.group == group && mapping.index == index) {
            return translation_key_to_name(mapping.key);
        }
    }
    return nullptr;
}

json_value json_value::make_object()
{
    json_value value;
    value.type = json_type::object;
    return value;
}

json_value json_value::make_array()
{
    json_value value;
    value.type = json_type::array;
    return value;
}

json_parser::json_parser(std::string_view text) : text_(text) {}

bool json_parser::parse(json_value &value, std::string &error)
{
    skip_whitespace();
    if (!parse_value(value, error)) {
        return false;
    }
    skip_whitespace();
    if (position_ != text_.size()) {
        error = "Unexpected trailing content in localization JSON.";
        return false;
    }
    return true;
}

bool json_parser::parse_value(json_value &value, std::string &error)
{
    if (position_ >= text_.size()) {
        error = "Unexpected end of localization JSON.";
        return false;
    }

    switch (text_[position_]) {
        case '{': return parse_object(value, error);
        case '[': return parse_array(value, error);
        case '"': return parse_string_value(value, error);
        case 't': return parse_literal("true", json_type::boolean, value, error, 1);
        case 'f': return parse_literal("false", json_type::boolean, value, error, 0);
        case 'n': return parse_literal("null", json_type::null_value, value, error, 0);
        default: return parse_number(value, error);
    }
}

bool json_parser::parse_object(json_value &value, std::string &error)
{
    ++position_;
    value = json_value::make_object();
    skip_whitespace();
    if (consume('}')) {
        return true;
    }

    while (position_ < text_.size()) {
        std::string key;
        if (!parse_string(key, error)) {
            return false;
        }
        skip_whitespace();
        if (!consume(':')) {
            error = "Expected ':' in localization JSON object.";
            return false;
        }
        skip_whitespace();
        json_value child;
        if (!parse_value(child, error)) {
            return false;
        }
        value.object_value.emplace(std::move(key), std::move(child));
        skip_whitespace();
        if (consume('}')) {
            return true;
        }
        if (!consume(',')) {
            error = "Expected ',' or '}' in localization JSON object.";
            return false;
        }
        skip_whitespace();
    }

    error = "Unterminated localization JSON object.";
    return false;
}

bool json_parser::parse_array(json_value &value, std::string &error)
{
    ++position_;
    value = json_value::make_array();
    skip_whitespace();
    if (consume(']')) {
        return true;
    }

    while (position_ < text_.size()) {
        json_value child;
        if (!parse_value(child, error)) {
            return false;
        }
        value.array_value.push_back(std::move(child));
        skip_whitespace();
        if (consume(']')) {
            return true;
        }
        if (!consume(',')) {
            error = "Expected ',' or ']' in localization JSON array.";
            return false;
        }
        skip_whitespace();
    }

    error = "Unterminated localization JSON array.";
    return false;
}

bool json_parser::parse_string_value(json_value &value, std::string &error)
{
    value.type = json_type::string;
    return parse_string(value.string_value, error);
}

bool json_parser::parse_string(std::string &value, std::string &error)
{
    if (!consume('"')) {
        error = "Expected string in localization JSON.";
        return false;
    }

    value.clear();
    while (position_ < text_.size()) {
        const char ch = text_[position_++];
        if (ch == '"') {
            return true;
        }
        if (ch == '\\') {
            if (position_ >= text_.size()) {
                error = "Unterminated escape sequence in localization JSON string.";
                return false;
            }
            const char escaped = text_[position_++];
            switch (escaped) {
                case '"': value.push_back('"'); break;
                case '\\': value.push_back('\\'); break;
                case '/': value.push_back('/'); break;
                case 'b': value.push_back('\b'); break;
                case 'f': value.push_back('\f'); break;
                case 'n': value.push_back('\n'); break;
                case 'r': value.push_back('\r'); break;
                case 't': value.push_back('\t'); break;
                case 'u': {
                    if (position_ + 4 > text_.size()) {
                        error = "Invalid unicode escape in localization JSON string.";
                        return false;
                    }
                    uint32_t codepoint = 0;
                    for (int i = 0; i < 4; ++i) {
                        codepoint <<= 4;
                        const char hex = text_[position_++];
                        if (hex >= '0' && hex <= '9') {
                            codepoint |= static_cast<uint32_t>(hex - '0');
                        } else if (hex >= 'a' && hex <= 'f') {
                            codepoint |= static_cast<uint32_t>(hex - 'a' + 10);
                        } else if (hex >= 'A' && hex <= 'F') {
                            codepoint |= static_cast<uint32_t>(hex - 'A' + 10);
                        } else {
                            error = "Invalid unicode escape in localization JSON string.";
                            return false;
                        }
                    }
                    append_utf8(codepoint, value);
                    break;
                }
                default:
                    error = "Unsupported escape sequence in localization JSON string.";
                    return false;
            }
            continue;
        }
        value.push_back(ch);
    }

    error = "Unterminated localization JSON string.";
    return false;
}

bool json_parser::parse_literal(const char *literal, json_type type, json_value &value, std::string &error, int boolean_value)
{
    const size_t length = strlen(literal);
    if (text_.substr(position_, length) != literal) {
        error = "Invalid literal in localization JSON.";
        return false;
    }
    position_ += length;
    value.type = type;
    value.boolean_value = boolean_value != 0;
    return true;
}

bool json_parser::parse_number(json_value &value, std::string &error)
{
    const size_t start = position_;
    if (text_[position_] == '-') {
        ++position_;
    }
    if (position_ >= text_.size() || !std::isdigit(static_cast<unsigned char>(text_[position_]))) {
        error = "Invalid number in localization JSON.";
        return false;
    }
    while (position_ < text_.size() && std::isdigit(static_cast<unsigned char>(text_[position_]))) {
        ++position_;
    }
    if (position_ < text_.size() && (text_[position_] == '.' || text_[position_] == 'e' || text_[position_] == 'E')) {
        error = "Floating-point numbers are not supported in localization JSON.";
        return false;
    }

    value.type = json_type::number;
    value.number_value = strtoll(std::string(text_.substr(start, position_ - start)).c_str(), nullptr, 10);
    return true;
}

bool json_parser::consume(char expected)
{
    if (position_ < text_.size() && text_[position_] == expected) {
        ++position_;
        return true;
    }
    return false;
}

void json_parser::skip_whitespace()
{
    while (position_ < text_.size() && std::isspace(static_cast<unsigned char>(text_[position_]))) {
        ++position_;
    }
}

void json_parser::append_utf8(uint32_t codepoint, std::string &value)
{
    if (codepoint <= 0x7f) {
        value.push_back(static_cast<char>(codepoint));
    } else if (codepoint <= 0x7ff) {
        value.push_back(static_cast<char>(0xc0 | (codepoint >> 6)));
        value.push_back(static_cast<char>(0x80 | (codepoint & 0x3f)));
    } else if (codepoint <= 0xffff) {
        value.push_back(static_cast<char>(0xe0 | (codepoint >> 12)));
        value.push_back(static_cast<char>(0x80 | ((codepoint >> 6) & 0x3f)));
        value.push_back(static_cast<char>(0x80 | (codepoint & 0x3f)));
    } else {
        value.push_back(static_cast<char>(0xf0 | (codepoint >> 18)));
        value.push_back(static_cast<char>(0x80 | ((codepoint >> 12) & 0x3f)));
        value.push_back(static_cast<char>(0x80 | ((codepoint >> 6) & 0x3f)));
        value.push_back(static_cast<char>(0x80 | (codepoint & 0x3f)));
    }
}

static const json_value *json_find(const json_value &object, const char *key)
{
    if (object.type != json_type::object) {
        return nullptr;
    }
    const auto it = object.object_value.find(key);
    return it == object.object_value.end() ? nullptr : &it->second;
}

static bool json_object_has_entries(const json_value *value)
{
    return value && value->type == json_type::object && !value->object_value.empty();
}

static std::string json_string_or_empty(const json_value *value)
{
    return value && value->type == json_type::string ? value->string_value : std::string();
}

static int json_number_or_default(const json_value *value, int default_value)
{
    return value && value->type == json_type::number ? static_cast<int>(value->number_value) : default_value;
}

static bool parse_locale_json_file(const std::string &path, json_value &root, std::string &error)
{
    std::string contents;
    if (!read_text_file(path, contents)) {
        error = "Unable to read localization JSON.";
        return false;
    }

    json_parser parser(contents);
    if (!parser.parse(root, error)) {
        return false;
    }
    if (root.type != json_type::object) {
        error = "Localization JSON root must be an object.";
        return false;
    }
    return true;
}

static void merge_string_groups(const json_value *section, std::map<int, std::vector<localized_text>> &target)
{
    if (!section || section->type != json_type::object) {
        return;
    }
    for (const auto &group_pair : section->object_value) {
        if (group_pair.second.type != json_type::object) {
            continue;
        }
        const int group = atoi(group_pair.first.c_str());
        std::vector<localized_text> &strings = target[group];
        for (const auto &index_pair : group_pair.second.object_value) {
            if (index_pair.second.type != json_type::string) {
                continue;
            }
            const int index = atoi(index_pair.first.c_str());
            if (index < 0) {
                continue;
            }
            if (strings.size() <= static_cast<size_t>(index)) {
                strings.resize(index + 1);
            }
            strings[index].utf8 = index_pair.second.string_value;
        }
    }
}

static void merge_message_string_field(const json_value *field_value, message_string_definition &field)
{
    if (!field_value || field_value->type != json_type::object) {
        return;
    }
    if (const json_value *text = json_find(*field_value, "text")) {
        if (text->type == json_type::string) {
            field.text.utf8 = text->string_value;
            field.has_text = true;
        }
    }
    if (const json_value *x = json_find(*field_value, "x")) {
        field.x = json_number_or_default(x, field.x);
        field.has_x = true;
    }
    if (const json_value *y = json_find(*field_value, "y")) {
        field.y = json_number_or_default(y, field.y);
        field.has_y = true;
    }
    if (const json_value *legacy_offset = json_find(*field_value, "legacy_offset")) {
        field.legacy_offset = json_number_or_default(legacy_offset, field.legacy_offset);
        field.has_legacy_offset = true;
    }
}

static void merge_messages_object(const json_value *messages_value, std::vector<message_definition> &target)
{
    if (!messages_value || messages_value->type != json_type::object) {
        return;
    }
    for (const auto &message_pair : messages_value->object_value) {
        if (message_pair.second.type != json_type::object) {
            continue;
        }
        const int id = atoi(message_pair.first.c_str());
        if (id < 0) {
            continue;
        }
        if (target.size() <= static_cast<size_t>(id)) {
            target.resize(id + 1);
        }
        message_definition &message = target[id];
        message.present = true;

        if (const json_value *type = json_find(message_pair.second, "type")) {
            message.type = static_cast<lang_type>(json_number_or_default(type, message.type));
            message.has_type = true;
        }
        if (const json_value *message_type = json_find(message_pair.second, "message_type")) {
            message.message_type = static_cast<lang_message_type>(json_number_or_default(message_type, message.message_type));
            message.has_message_type = true;
        }
        if (const json_value *x = json_find(message_pair.second, "x")) {
            message.x = json_number_or_default(x, message.x);
            message.has_x = true;
        }
        if (const json_value *y = json_find(message_pair.second, "y")) {
            message.y = json_number_or_default(y, message.y);
            message.has_y = true;
        }
        if (const json_value *width = json_find(message_pair.second, "width_blocks")) {
            message.width_blocks = json_number_or_default(width, message.width_blocks);
            message.has_width_blocks = true;
        }
        if (const json_value *height = json_find(message_pair.second, "height_blocks")) {
            message.height_blocks = json_number_or_default(height, message.height_blocks);
            message.has_height_blocks = true;
        }
        if (const json_value *urgent = json_find(message_pair.second, "urgent")) {
            message.urgent = json_number_or_default(urgent, message.urgent);
            message.has_urgent = true;
        }
        if (const json_value *image = json_find(message_pair.second, "image")) {
            if (const json_value *id_value = json_find(*image, "id")) {
                message.image.id = json_number_or_default(id_value, message.image.id);
                message.image.has_id = true;
            }
            if (const json_value *x_value = json_find(*image, "x")) {
                message.image.x = json_number_or_default(x_value, message.image.x);
                message.image.has_x = true;
            }
            if (const json_value *y_value = json_find(*image, "y")) {
                message.image.y = json_number_or_default(y_value, message.image.y);
                message.image.has_y = true;
            }
        }

        merge_message_string_field(json_find(message_pair.second, "title"), message.title);
        merge_message_string_field(json_find(message_pair.second, "subtitle"), message.subtitle);
        merge_message_string_field(json_find(message_pair.second, "video"), message.video);
        merge_message_string_field(json_find(message_pair.second, "content"), message.content);
    }
}

bool merge_locale_json(const std::string &path, locale_catalog &catalog, std::string &error)
{
    json_value root;
    if (!parse_locale_json_file(path, root, error)) {
        return false;
    }

    const json_value *metadata = json_find(root, "metadata");
    if (!metadata || metadata->type != json_type::object) {
        error = "Localization JSON is missing metadata.";
        return false;
    }

    if (catalog.info.code.empty()) {
        catalog.info.code = json_string_or_empty(json_find(*metadata, "code"));
        catalog.info.display_name = json_string_or_empty(json_find(*metadata, "display_name"));
        catalog.info.english_name = json_string_or_empty(json_find(*metadata, "english_name"));
        catalog.language = language_from_code(catalog.info.code);
    } else {
        const std::string display_name = json_string_or_empty(json_find(*metadata, "display_name"));
        const std::string english_name = json_string_or_empty(json_find(*metadata, "english_name"));
        if (!display_name.empty()) catalog.info.display_name = display_name;
        if (!english_name.empty()) catalog.info.english_name = english_name;
    }

    merge_string_groups(json_find(root, "main_strings"), catalog.main_strings);
    merge_string_groups(json_find(root, "editor_strings"), catalog.editor_strings);
    catalog.has_main_strings = catalog.has_main_strings || json_object_has_entries(json_find(root, "main_strings"));
    catalog.has_editor_strings = catalog.has_editor_strings || json_object_has_entries(json_find(root, "editor_strings"));

    if (const json_value *messages = json_find(root, "messages")) {
        merge_messages_object(json_find(*messages, "main"), catalog.main_messages);
        merge_messages_object(json_find(*messages, "editor"), catalog.editor_messages);
        catalog.has_main_messages = catalog.has_main_messages || json_object_has_entries(json_find(*messages, "main"));
        catalog.has_editor_messages = catalog.has_editor_messages || json_object_has_entries(json_find(*messages, "editor"));
    }

    if (const json_value *project_keys = json_find(root, "project_keys")) {
        if (project_keys->type == json_type::object) {
            for (const auto &entry : project_keys->object_value) {
                if (entry.second.type != json_type::string) continue;
                translation_key key;
                if (translation_key_from_name(entry.first.c_str(), &key) && key >= 0 && key < TRANSLATION_MAX_KEY) {
                    catalog.project_keys[key].utf8 = entry.second.string_value;
                    merge_project_key_legacy_string_slots(key, entry.second.string_value, catalog);
                } else {
                    int is_editor = 0;
                    int group = 0;
                    int index = 0;
                    if (parse_legacy_project_key_name(entry.first, is_editor, group, index)) {
                        set_legacy_string_slot(catalog, is_editor, group, index, entry.second.string_value);
                    } else {
                        catalog.named_project_keys[entry.first].utf8 = entry.second.string_value;
                    }
                }
            }
        }
    }
    return true;
}

static void detect_project_key_legacy_sections(const json_value *project_keys, bool &has_main_strings, bool &has_editor_strings)
{
    if (!project_keys || project_keys->type != json_type::object) {
        return;
    }
    for (const auto &entry : project_keys->object_value) {
        if (entry.second.type != json_type::string) {
            continue;
        }
        translation_key key;
        if (translation_key_from_name(entry.first.c_str(), &key) && key >= 0 && key < TRANSLATION_MAX_KEY) {
            detect_project_key_legacy_string_slots(key, has_main_strings, has_editor_strings);
            continue;
        }
        int is_editor = 0;
        int group = 0;
        int index = 0;
        if (parse_legacy_project_key_name(entry.first, is_editor, group, index)) {
            if (is_editor) {
                has_editor_strings = true;
            } else {
                has_main_strings = true;
            }
        }
    }
}

int enumerate_available_locales_internal(std::vector<locale_info> &out_locales)
{
    std::map<std::string, locale_info> merged;
    for (int i = 0; i < mod_manager_get_mod_count(); ++i) {
        const std::string root = append_path_component(mod_manager_get_mod_path_at(i), "Localization");
        std::vector<std::string> files;
        list_directory_contents(root, TYPE_FILE, "json", files);
        for (const std::string &file_name : files) {
            json_value root_value;
            std::string error;
            if (!parse_locale_json_file(append_path_component(root, file_name), root_value, error)) {
                continue;
            }
            const json_value *metadata = json_find(root_value, "metadata");
            if (!metadata || metadata->type != json_type::object) {
                continue;
            }
            locale_info info;
            info.code = json_string_or_empty(json_find(*metadata, "code"));
            info.display_name = json_string_or_empty(json_find(*metadata, "display_name"));
            info.english_name = json_string_or_empty(json_find(*metadata, "english_name"));
            info.has_main_strings = json_object_has_entries(json_find(root_value, "main_strings"));
            info.has_editor_strings = json_object_has_entries(json_find(root_value, "editor_strings"));
            detect_project_key_legacy_sections(json_find(root_value, "project_keys"),
                info.has_main_strings, info.has_editor_strings);
            if (const json_value *messages = json_find(root_value, "messages")) {
                info.has_main_messages = json_object_has_entries(json_find(*messages, "main"));
                info.has_editor_messages = json_object_has_entries(json_find(*messages, "editor"));
            }
            if (info.code.empty()) {
                continue;
            }
            locale_info &merged_info = merged[info.code];
            if (!info.display_name.empty()) merged_info.display_name = info.display_name;
            if (!info.english_name.empty()) merged_info.english_name = info.english_name;
            merged_info.code = info.code;
            merged_info.has_main_strings = merged_info.has_main_strings || info.has_main_strings;
            merged_info.has_editor_strings = merged_info.has_editor_strings || info.has_editor_strings;
            merged_info.has_main_messages = merged_info.has_main_messages || info.has_main_messages;
            merged_info.has_editor_messages = merged_info.has_editor_messages || info.has_editor_messages;
        }
    }

    out_locales.clear();
    for (const auto &pair : merged) {
        if (pair.second.has_main_strings && pair.second.has_main_messages) {
            out_locales.push_back(pair.second);
        }
    }
    return static_cast<int>(out_locales.size());
}

std::string select_default_locale_code()
{
    std::vector<locale_info> locales;
    enumerate_available_locales_internal(locales);
    for (const locale_info &locale : locales) {
        if (locale.code == "en") {
            return locale.code;
        }
    }
    return locales.empty() ? std::string("en") : locales.front().code;
}

} // namespace localization::detail
