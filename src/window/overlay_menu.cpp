#include "translation/translation.h"
#include "overlay_menu.h"

#include "building/building_type.h"
#include "building/building_type_registry_internal.h"
#include "city/view.h"
#include "core/image.h"
#include "core/image_group.h"
#include "core/time.h"
#include "game/state.h"
#include "graphics/generic_button.h"
#include "graphics/ui_runtime_api.h"
#include "graphics/screen.h"
#include "graphics/text.h"
#include "graphics/window.h"
#include "input/input.h"
#include "window/city.h"
#include "graphics/image.h"

#include <algorithm>
#include <array>
#include <memory>
#include "graphics/declarative_window.h"

#define TOP_MARGIN 74
#define SIDEBAR_MARGIN_X 10
#define OVERLAY_MENU_END { -1, {}, JULIUS, NULL, NULL }



typedef enum
{
    JULIUS = 0,
    AUGUSTUS = 1,
    XML_BUILDING_NAME = 2,
} translation_type;

struct overlay_menu_entry {
    int overlay;
    translation_key translation;
    translation_type translation_kind;
    const struct overlay_menu_entry *submenu;
    const char *building_text_id;

    overlay_menu_entry(
        int overlay_id,
        translation_key key,
        translation_type type,
        const overlay_menu_entry *child_menu,
        const char *building_id = nullptr)
        : overlay(overlay_id),
          translation(key),
          translation_kind(type),
          submenu(child_menu),
          building_text_id(building_id)
    {
    }

    overlay_menu_entry(
        int overlay_id,
        int,
        translation_type type,
        const overlay_menu_entry *child_menu,
        const char *building_id = nullptr)
        : overlay(overlay_id),
          translation(),
          translation_kind(type),
          submenu(child_menu),
          building_text_id(building_id)
    {
    }
};

static const overlay_menu_entry submenu_risks[] = {
    { OVERLAY_FIRE, 0, JULIUS, NULL },
    { OVERLAY_DAMAGE, 0, JULIUS, NULL },
    { OVERLAY_CRIME, 0, JULIUS, NULL },
    { OVERLAY_NATIVE, 0, JULIUS, NULL },
    { OVERLAY_PROBLEMS, 0, JULIUS, NULL },
    { OVERLAY_ENEMY, "TR_OVERLAY_ENEMY", AUGUSTUS, NULL },
    { OVERLAY_SICKNESS, "TR_OVERLAY_SICKNESS", AUGUSTUS, NULL },
    OVERLAY_MENU_END
};

static const overlay_menu_entry submenu_entertainment[] = {
    { OVERLAY_ENTERTAINMENT, OVERLAY_ENTERTAINMENT, JULIUS, NULL },
    { OVERLAY_TAVERN, "TR_OVERLAY_TAVERN", AUGUSTUS, NULL },
    { OVERLAY_THEATER, 0, JULIUS, NULL },
    { OVERLAY_AMPHITHEATER, 0, JULIUS, NULL },
    { OVERLAY_ARENA, "TR_OVERLAY_ARENA_COL", AUGUSTUS, NULL },
    { OVERLAY_COLOSSEUM, 0, JULIUS, NULL },
    { OVERLAY_HIPPODROME, 0, JULIUS, NULL },
    OVERLAY_MENU_END
};

static const overlay_menu_entry submenu_education[] = {
    {OVERLAY_EDUCATION, OVERLAY_EDUCATION, JULIUS, NULL},
    {OVERLAY_SCHOOL, 0, JULIUS, NULL},
    {OVERLAY_LIBRARY, 0, JULIUS, NULL},
    {OVERLAY_ACADEMY, 0, JULIUS, NULL},
    OVERLAY_MENU_END
};

static const overlay_menu_entry submenu_health[] = {
    {OVERLAY_HEALTH, "TR_OVERLAY_HEALTH", AUGUSTUS, NULL},
    {OVERLAY_BARBER, 0, JULIUS, NULL},
    {OVERLAY_BATHHOUSE, 0, JULIUS, NULL},
    {OVERLAY_CLINIC, 0, JULIUS, NULL},
    {OVERLAY_HOSPITAL, 0, JULIUS, NULL},
    OVERLAY_MENU_END
};

static const overlay_menu_entry submenu_commerce[] = {
    {OVERLAY_LOGISTICS, "TR_OVERLAY_LOGISTICS", AUGUSTUS, NULL},
    {OVERLAY_FOOD_STOCKS, 0, JULIUS, NULL},
    {OVERLAY_EFFICIENCY, "TR_OVERLAY_EFFICIENCY", AUGUSTUS, NULL},
    {OVERLAY_MOTHBALL, "TR_OVERLAY_MOTHBALL", AUGUSTUS, NULL},
    {OVERLAY_TAX_INCOME, 0, JULIUS, NULL},
    {OVERLAY_LEVY, "TR_OVERLAY_LEVY", AUGUSTUS, NULL},
    {OVERLAY_EMPLOYMENT, "TR_OVERLAY_EMPLOYMENT", AUGUSTUS, NULL},
    OVERLAY_MENU_END
};

static const overlay_menu_entry submenu_housing_groups[] = {
    { OVERLAY_HOUSING_GROUPS_TENTS, "TR_OVERLAY_HOUSING_TENTS", AUGUSTUS, NULL},
    { OVERLAY_HOUSING_GROUPS_SHACKS,"TR_OVERLAY_HOUSING_SHACKS", AUGUSTUS, NULL},
    { OVERLAY_HOUSING_GROUPS_HOVELS,"TR_OVERLAY_HOUSING_HOVELS", AUGUSTUS, NULL},
    { OVERLAY_HOUSING_GROUPS_CASAE,"TR_OVERLAY_HOUSING_CASAS", AUGUSTUS, NULL},
    { OVERLAY_HOUSING_GROUPS_INSULAE,"TR_OVERLAY_HOUSE_INSULAS", AUGUSTUS, NULL},
    { OVERLAY_HOUSING_GROUPS_VILLAS,"TR_OVERLAY_HOUSE_VILLAS", AUGUSTUS, NULL},
    { OVERLAY_HOUSING_GROUPS_PALACES,"TR_OVERLAY_HOUSE_PALACES", AUGUSTUS, NULL},
    OVERLAY_MENU_END
};

static const overlay_menu_entry submenu_housing[] = {
    { OVERLAY_HOUSING_GROUPS, "TR_OVERLAY_BY_GROUP", AUGUSTUS, submenu_housing_groups},
    { OVERLAY_HOUSE_SMALL_TENT, 0, XML_BUILDING_NAME, NULL, "house_small_tent"},
    { OVERLAY_HOUSE_LARGE_TENT, 0, XML_BUILDING_NAME, NULL, "house_large_tent"},
    { OVERLAY_HOUSE_SMALL_SHACK, 0, XML_BUILDING_NAME, NULL, "house_small_shack" },
    { OVERLAY_HOUSE_LARGE_SHACK, 0, XML_BUILDING_NAME, NULL, "house_large_shack" },
    { OVERLAY_HOUSE_SMALL_HOVEL, 0, XML_BUILDING_NAME, NULL, "house_small_hovel" },
    { OVERLAY_HOUSE_LARGE_HOVEL, 0, XML_BUILDING_NAME, NULL, "house_large_hovel" },
    { OVERLAY_HOUSE_SMALL_CASA, 0, XML_BUILDING_NAME, NULL, "house_small_casa" },
    { OVERLAY_HOUSE_LARGE_CASA, 0, XML_BUILDING_NAME, NULL, "house_large_casa" },
    { OVERLAY_HOUSE_SMALL_INSULA, 0, XML_BUILDING_NAME, NULL, "house_small_insula" },
    { OVERLAY_HOUSE_MEDIUM_INSULA, 0, XML_BUILDING_NAME, NULL, "house_medium_insula" },
    { OVERLAY_HOUSE_LARGE_INSULA, 0, XML_BUILDING_NAME, NULL, "house_large_insula" },
    { OVERLAY_HOUSE_GRAND_INSULA, 0, XML_BUILDING_NAME, NULL, "house_grand_insula" },
    { OVERLAY_HOUSE_SMALL_VILLA, 0, XML_BUILDING_NAME, NULL, "house_small_villa" },
    { OVERLAY_HOUSE_MEDIUM_VILLA, 0, XML_BUILDING_NAME, NULL, "house_medium_villa" },
    { OVERLAY_HOUSE_LARGE_VILLA, 0, XML_BUILDING_NAME, NULL, "house_large_villa" },
    { OVERLAY_HOUSE_GRAND_VILLA, 0, XML_BUILDING_NAME, NULL, "house_grand_villa" },
    { OVERLAY_HOUSE_SMALL_PALACE, 0, XML_BUILDING_NAME, NULL, "house_small_palace" },
    { OVERLAY_HOUSE_MEDIUM_PALACE, 0, XML_BUILDING_NAME, NULL, "house_medium_palace" },
    { OVERLAY_HOUSE_LARGE_PALACE, 0, XML_BUILDING_NAME, NULL, "house_large_palace" },
    { OVERLAY_HOUSE_LUXURY_PALACE, 0, XML_BUILDING_NAME, NULL, "house_luxury_palace" },
    OVERLAY_MENU_END
};

static const overlay_menu_entry overlay_menu[] = {
    { OVERLAY_NONE,0, JULIUS, NULL },
    { OVERLAY_WATER,0, JULIUS, NULL },
    { 1, 0, JULIUS, submenu_risks},
    { 3, 0, JULIUS, submenu_entertainment},
    { 5,0, JULIUS, submenu_education},
    { 6,0, JULIUS, submenu_health},
    { 7,0, JULIUS, submenu_commerce},
    { OVERLAY_RELIGION,0, JULIUS, NULL },
    { OVERLAY_ROADS, "TR_OVERLAY_ROADS", AUGUSTUS, NULL },
    { OVERLAY_DESIRABILITY,0, JULIUS, NULL },
    { OVERLAY_SENTIMENT, "TR_OVERLAY_SENTIMENT", AUGUSTUS, NULL },
    { OVERLAY_HOUSING, "TR_HEADER_HOUSING", AUGUSTUS, submenu_housing },
    OVERLAY_MENU_END
};


namespace {
constexpr int kMenuLevels = 3;
constexpr time_millis kHoverTimeout = 900;
int selected_overlay_id;
int sticky_level = -1;
time_millis last_hover;
int clicked_level = -1, clicked_item = -1;
const DeclarativeWindowDefinition *menu_definition;

const uint8_t *entry_text(const overlay_menu_entry &entry)
{
    if (entry.translation_kind == AUGUSTUS) return translation_for(entry.translation);
    if (entry.translation_kind == XML_BUILDING_NAME) return lang_get_building_type_string(building_type_registry_impl::type_from_attr(entry.building_text_id));
    return lang_get_string(current_string_key(14, entry.overlay));
}

struct MenuColumn final : DeclarativeWindowController {
    const overlay_menu_entry *entries = nullptr;
    int level = 0, count = 0, first = 0, capacity = 0, selected = -1;
    int x = 0, y = 0, width = 0, height = 0;
    std::unique_ptr<DeclarativeWindowRuntime> runtime;

    void reset(const overlay_menu_entry *value)
    {
        entries = value;
        first = 0;
        selected = -1;
        count = 0;
        if (entries) while (entries[count].overlay != -1) ++count;
    }
    int repeat_count(std::string_view source) const override { return source == "entries" ? std::min(capacity, count - first) : 0; }
    std::string text(std::string_view binding, int item) const override
    {
        if (binding != "entry.name" || item < 0 || first + item >= count) return {};
        const auto &entry = entries[first + item];
        return std::string(entry.submenu ? "< " : "") + reinterpret_cast<const char *>(entry_text(entry));
    }
    int condition(std::string_view binding, int item) const override
    {
        if (binding == "entry.selected") return selected == first + item;
        if (binding == "page.previous") return first > 0;
        if (binding == "page.next") return first + capacity < count;
        return 0;
    }
    void action(std::string_view action, int item) override
    {
        if (action == "entry.select") { clicked_level = level; clicked_item = first + item; }
        if (action == "page.previous") first = std::max(0, first - capacity);
        if (action == "page.next") first = std::min(std::max(0, count - capacity), first + capacity);
    }
};
std::array<MenuColumn, kMenuLevels> columns;

void clear_after(int level)
{
    for (int i = level + 1; i < kMenuLevels; ++i) columns[i].reset(nullptr);
}

void select_parent(int level, int item)
{
    auto &column = columns[level];
    if (column.selected == item) return;
    column.selected = item;
    clear_after(level);
    if (level + 1 < kMenuLevels && item >= 0 && item < column.count) columns[level + 1].reset(column.entries[item].submenu);
}

void layout_columns()
{
    int vx, vy, vw, vh;
    city_view_get_viewport(&vx, &vy, &vw, &vh);
    const auto *rows = menu_definition->widget("entries");
    const int spacing = std::max(1, rows->repeat_spacing_y);
    const int available = std::max(spacing + 48, screen_height() - TOP_MARGIN - 8);
    int right = screen_pixel_to_ui(vx + vw) - SIDEBAR_MARGIN_X;
    for (int i = 0; i < kMenuLevels; ++i) {
        auto &column = columns[i];
        if (!column.entries) break;
        column.width = menu_definition->base_width();
        column.capacity = std::max(1, (available - 48) / spacing);
        column.first = std::clamp(column.first, 0, std::max(0, column.count - column.capacity));
        column.height = std::min(column.capacity, column.count) * spacing + 48;
        column.x = std::max(0, right - column.width);
        int parent_y = i ? columns[i - 1].y + rows->y + (columns[i - 1].selected - columns[i - 1].first) * spacing : TOP_MARGIN;
        column.y = std::clamp(parent_y, TOP_MARGIN, std::max(TOP_MARGIN, screen_height() - column.height - 8));
        right = column.x - 4;
    }
}

void draw_background() { window_city_draw_panels(); }
void draw_foreground()
{
    window_city_draw();
    if (!menu_definition) return;
    layout_columns();
    for (auto &column : columns) {
        if (!column.entries) break;
        column.runtime->draw(DeclarativeDrawPhase::Background, column.width, column.height, column.x, column.y);
        column.runtime->draw(DeclarativeDrawPhase::Foreground, column.width, column.height, column.x, column.y);
    }
}

void handle_input(const mouse *m, const hotkeys *keys)
{
    if (!menu_definition) return;
    if (input_go_back_requested(m, keys)) { window_city_show(); return; }
    layout_columns();
    clicked_level = clicked_item = -1;
    bool inside = false;
    int hovered_level = -1, hovered_item = -1;
    for (int i = kMenuLevels - 1; i >= 0; --i) {
        auto &column = columns[i];
        if (!column.entries) continue;
        mouse local = *m;
        local.x -= column.x;
        local.y -= column.y;
        const bool in_column = !inside && local.x >= 0 && local.x < column.width && local.y >= 0 && local.y < column.height;
        if (in_column) {
            inside = true;
            if (m->scrolled) column.first = std::clamp(column.first + static_cast<int>(m->scrolled), 0, std::max(0, column.count - column.capacity));
        } else {
            local.x = local.y = -1;
            local.left = {};
        }
        column.runtime->handle_mouse(local, column.width, column.height);
        if (in_column && column.runtime->focused_item() >= 0) {
            hovered_level = i;
            hovered_item = column.first + column.runtime->focused_item();
        }
    }
    if (clicked_level >= 0) {
        auto &column = columns[clicked_level];
        const auto &entry = column.entries[clicked_item];
        if (!entry.submenu) {
            selected_overlay_id = entry.overlay;
            game_state_set_overlay(entry.overlay);
            window_city_show();
            return;
        }
        if (column.selected == clicked_item && sticky_level == clicked_level) {
            column.selected = -1;
            clear_after(clicked_level);
            sticky_level = -1;
        } else {
            select_parent(clicked_level, clicked_item);
            sticky_level = clicked_level;
        }
    } else if (hovered_level >= 0 && (sticky_level < 0 || hovered_level > sticky_level)) {
        select_parent(hovered_level, hovered_item);
    }
    if (inside) last_hover = time_get_millis();
    else if (m->left.went_up) { window_city_show(); return; }
    else if (sticky_level < 0 && time_get_millis() - last_hover > kHoverTimeout) {
        columns[0].selected = -1;
        clear_after(0);
    }
}

void get_tooltip(tooltip_context *context)
{
    for (auto &column : columns) if (column.entries) column.runtime->tooltip(*context);
}

const overlay_menu_entry *find_leaf(const overlay_menu_entry *entries, int overlay)
{
    for (int i = 0; entries[i].overlay != -1; ++i) {
        if (!entries[i].submenu && entries[i].overlay == overlay) return &entries[i];
        if (entries[i].submenu) if (const auto *found = find_leaf(entries[i].submenu, overlay)) return found;
    }
    return nullptr;
}
}

void window_overlay_menu_show()
{
    menu_definition = declarative_window_definition("overlay_menu");
    if (!menu_definition || !menu_definition->widget("entries")) return;
    for (int i = 0; i < kMenuLevels; ++i) {
        columns[i].level = i;
        columns[i].reset(i ? nullptr : overlay_menu);
        columns[i].runtime = std::make_unique<DeclarativeWindowRuntime>(*menu_definition, columns[i]);
    }
    sticky_level = -1;
    last_hover = time_get_millis();
    const window_type window = {WINDOW_OVERLAY_MENU, draw_background, draw_foreground, handle_input, get_tooltip};
    window_show(&window);
}

void window_overlay_menu_update() { selected_overlay_id = game_state_overlay(); }

const uint8_t *get_current_overlay_text()
{
    const auto *entry = find_leaf(overlay_menu, selected_overlay_id);
    return entry ? entry_text(*entry) : lang_get_string(current_string_key(14, selected_overlay_id));
}
