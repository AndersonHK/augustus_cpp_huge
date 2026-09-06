#pragma once

// Included by config.cpp after its screen state; owns dynamic rows without changing legacy config storage.
static std::vector<config_widget> effective_config_rows;
static std::vector<mod_content::Setting> screen_mod_settings;
static std::deque<std::string> config_row_text;

static const uint8_t *config_literal(const std::string &text)
{
    std::string encoded(text.size() * 4 + 1, '\0');
    encoding_from_utf8(text.c_str(), reinterpret_cast<uint8_t *>(encoded.data()), static_cast<int>(encoded.size()));
    encoded.resize(std::strlen(encoded.c_str()));
    config_row_text.push_back(std::move(encoded));
    return reinterpret_cast<const uint8_t *>(config_row_text.back().c_str());
}

static bool mod_setting_on_page(const mod_content::Setting &setting, unsigned int page)
{
    const auto category = mod_content::lower(setting.category);
    if (category == "difficulty" || category.find("difficulty / ") == 0) return page == CONFIG_PAGE_GAMEPLAY_CHANGES;
    if (category == "city life" || category == "housing") return page == CONFIG_PAGE_CITY_MANAGEMENT_CHANGES && selected_categories.city_mgmt_category == CATEGORY_CITY_MANAGEMENT_HOUSING;
    if (category.find("city management") == 0) {
        int selected = CATEGORY_CITY_MANAGEMENT_STORAGE;
        if (category.find("roadblock") != std::string::npos) selected = CATEGORY_CITY_MANAGEMENT_ROADBLOCK_SETTINGS;
        else if (category.find("roads") != std::string::npos) selected = CATEGORY_CITY_MANAGEMENT_ROADS;
        else if (category.find("housing") != std::string::npos) selected = CATEGORY_CITY_MANAGEMENT_HOUSING;
        else if (category.find("destruction") != std::string::npos) selected = CATEGORY_CITY_MANAGEMENT_DESTRUCTION;
        return page == CONFIG_PAGE_CITY_MANAGEMENT_CHANGES && selected_categories.city_mgmt_category == selected;
    }
    if (category.find("user interface") == 0 || category == "ui") {
        int selected = CATEGORY_UI_GENERAL;
        if (category.find("scrolling") != std::string::npos) selected = CATEGORY_UI_SCROLLING;
        else if (category.find("building") != std::string::npos) selected = CATEGORY_UI_BUILDING;
        else if (category.find("city view") != std::string::npos) selected = CATEGORY_UI_CITY_VIEW;
        else if (category.find("weather") != std::string::npos) selected = CATEGORY_UI_WEATHER;
        else if (category.find("empire") != std::string::npos) selected = CATEGORY_UI_EMPIRE;
        return page == CONFIG_PAGE_UI_CHANGES && selected_categories.ui_category == selected;
    }
    return page == CONFIG_PAGE_GENERAL;
}

static void build_config_rows(unsigned int page, const config_widget *hardcoded)
{
    effective_config_rows.clear();
    config_row_text.clear();
    screen_mod_settings = mod_content::runtime().settings();
    auto header = [&](const std::string &text) {
        config_widget row = {TYPE_HEADER, 0, nullptr, nullptr, 0, 1, ITEM_BASE_H, 10};
        row.literal = config_literal(text);
        effective_config_rows.push_back(row);
    };
    if (hardcoded && hardcoded->type != TYPE_NONE) {
        header("Hardcoded settings");
        for (int i = 0; i < MAX_WIDGETS && hardcoded[i].type != TYPE_NONE; ++i) if (hardcoded[i].enabled) effective_config_rows.push_back(hardcoded[i]);
    }
    std::string mod;
    for (size_t i = 0; i < screen_mod_settings.size(); ++i) {
        const auto &setting = screen_mod_settings[i];
        if (!mod_setting_on_page(setting, page)) continue;
        if (setting.mod != mod) { mod = setting.mod; header(mod); }
        config_widget row = {TYPE_MOD_SETTING, static_cast<int>(i), nullptr, nullptr, 0, 1, ITEM_BASE_H, CHECKBOX_MARGIN};
        row.literal = config_literal(setting.name);
        effective_config_rows.push_back(row);
    }
}

static void apply_screen_mod_setting(const std::string &key, int value)
{
    const auto previous = data;
    std::string error;
    try { mod_settings_apply(key, value); }
    catch (const std::exception &failure) { error = failure.what(); }
    const auto category = previous.page == CONFIG_PAGE_UI_CHANGES ? static_cast<unsigned>(selected_categories.ui_category) : static_cast<unsigned>(selected_categories.city_mgmt_category);
    window_config_show(static_cast<window_config_page>(previous.page), category, previous.show_background_image);
    data = previous; // Preserve unrelated hardcoded edits, including their live callbacks.
    data.layout.visible_from = data.layout.visible_to = 0;
    window_invalidate();
    if (!error.empty()) {
        static std::vector<uint8_t> message;
        message.resize(error.size() * 4 + 1);
        encoding_from_utf8(error.c_str(), message.data(), static_cast<int>(message.size()));
        window_plain_message_dialog_show_with_extra("TR_MOD_SETTINGS_TITLE", "TR_MOD_SETTINGS_CHANGE_FAILED", message.data(), nullptr);
    }
}

static void op_measure_mod_setting(const config_widget *w, int width, int *height)
{
    const auto &setting = screen_mod_settings.at(w->subtype);
    int largest = 0;
    const int lines = text_measure_multiline(w->literal, width - 30, FONT_NORMAL_BLACK, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_BLACK)->line_height), &largest);
    *height = std::max(ITEM_BASE_H, lines * one_line_ml_height(FONT_NORMAL_BLACK) + (setting.boolean ? 10 : 38));
}

static void op_draw_mod_setting(const config_widget *w, int x, int y, int width)
{
    const auto &setting = screen_mod_settings.at(w->subtype);
    const auto color = setting.effective ? COLOR_MASK_NONE : COLOR_FONT_LIGHT_GRAY;
    const int label_height = text_draw_multiline(w->literal, x + (setting.boolean ? 30 : 0), y + 3, width - 30, 0, FONT_NORMAL_BLACK, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_BLACK)->line_height), color);
    if (setting.boolean) {
        ui_runtime_draw_one_row_button_border(x, y + 2, CHECKBOX_CHECK_SIZE, CHECKBOX_CHECK_SIZE, 0, color);
        if (setting.value) text_draw(string_from_ascii("x"), x + 6, y + 5, FONT_NORMAL_BLACK, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_BLACK)->line_height), color);
        return;
    }
    const int bar_y = y + label_height + 14, bar_width = std::max(1, width - 90);
    const int64_t range = static_cast<int64_t>(setting.maximum) - setting.minimum;
    const int knob = range ? static_cast<int>((static_cast<int64_t>(setting.value) - setting.minimum) * bar_width / range) : 0;
    graphics_draw_inset_rect(x, bar_y + 7, bar_width + 16, 3, COLOR_INSET_DARK, COLOR_INSET_LIGHT);
    ui_runtime_draw_one_row_button_border(x + knob, bar_y, 16, 18, 0, color);
    text_draw_number(setting.value, 0, "", x + bar_width + 25, bar_y + 3, FONT_NORMAL_BLACK, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_BLACK)->line_height), color);
}

static int op_input_mod_setting(const config_widget *w, int x, int y, int width, const mouse *m, unsigned *focused)
{
    int height = 0;
    op_measure_mod_setting(w, width, &height);
    if (m->x < x || m->x >= x + width || m->y < y || m->y >= y + height) return 0;
    *focused = 1;
    const auto setting = screen_mod_settings.at(w->subtype);
    if (!setting.effective || !m->left.went_up) return 0;
    int value = !setting.value;
    if (!setting.boolean) {
        if (m->y < y + height - 28) return 0;
        const int bar_width = std::max(1, width - 90);
        const int position = std::clamp(m->x - x - 8, 0, bar_width);
        value = static_cast<int>(setting.minimum + (static_cast<int64_t>(setting.maximum) - setting.minimum) * position / bar_width);
    }
    if (value != setting.value) apply_screen_mod_setting(setting.key(), value);
    return 1;
}
