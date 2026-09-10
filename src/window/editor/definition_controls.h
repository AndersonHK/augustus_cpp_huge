#pragma once
#include "scenario/definition_overrides.h"
#include "window/editor/select_special_attribute_mapping.h"
#include "window/select_list.h"
#include <array>
#include <string>
#include <vector>

// Direct editing and event actions share the same scenario overlay owner.
static building_type definition_target = BUILDING_NONE;
static int definition_field = 0, definition_phase = 1;
static resource_type definition_resource = RESOURCE_NONE;
static unsigned int definition_control_focus = 0;
static std::vector<std::string> definition_labels;
static std::vector<const uint8_t *> definition_label_pointers;
static std::vector<resource_type> definition_resources;
static generic_button definition_list_anchor = {0, 0, 420, 24};

static void show_definition_choices(void (*callback)(int))
{
    definition_label_pointers.clear();
    for (const auto &label : definition_labels) definition_label_pointers.push_back(reinterpret_cast<const uint8_t *>(label.c_str()));
    window_select_list_show_text(screen_dialog_offset_x() + 100, screen_dialog_offset_y() + 30, &definition_list_anchor, definition_label_pointers.data(), static_cast<int>(definition_label_pointers.size()), callback);
}

static void edit_house_field(int field)
{
    definition_field = field;
    window_numeric_input_bound_show(100, 120, nullptr, 9, field < 2 ? -1000 : 0, scenario_house_model_maximum(field), [](int value) {
        scenario_house_model_change(definition_target, definition_field, value, true);
        window_invalidate();
    });
}

static void select_house_definition(int type)
{
    definition_target = static_cast<building_type>(type);
    definition_labels.clear();
    for (int field = 0; field < 17; ++field) {
        const auto *mapping = scenario_events_parameter_data_get_attribute_mapping_by_value(PARAMETER_TYPE_HOUSE_DATA_TYPE, field);
        definition_labels.push_back(std::string(reinterpret_cast<const char *>(translation_for(mapping->key))) + ": " + std::to_string(scenario_house_model_value(definition_target, field)));
    }
    show_definition_choices(edit_house_field);
}

static void select_construction_resource(int index)
{
    definition_resource = definition_resources.at(index);
    window_numeric_input_bound_show(100, 120, nullptr, 9, 0, 1000000000, [](int value) {
        scenario_construction_requirement_change(definition_target, definition_phase - 1, definition_resource, value);
        window_invalidate();
    });
}

static void select_construction_phase(int index)
{
    definition_phase = index + 1;
    const auto &construction = building_type_registry_impl::definition_for_type(definition_target)->construction();
    definition_labels.clear(); definition_resources.clear();
    for (int i = 0; i < resource_production_count(); ++i) {
        const auto resource = resource_get_production(i);
        definition_resources.push_back(resource);
        const int amount = construction.is_phased() ? construction.requirement_amount(resource, index) : construction.instant_requirement_amount(resource);
        definition_labels.push_back(std::string(reinterpret_cast<const char *>(resource_get_data(resource)->text)) + ": " + std::to_string(amount));
    }
    show_definition_choices(select_construction_resource);
}

static void select_construction_definition(int type)
{
    definition_target = static_cast<building_type>(type);
    const auto &construction = building_type_registry_impl::definition_for_type(definition_target)->construction();
    definition_labels.clear();
    for (int phase = 1; phase <= std::max(1, construction.phase_count()); ++phase) definition_labels.push_back(std::string(reinterpret_cast<const char *>(translation_for("TR_PARAMETER_MONUMENT_STAGE"))) + " " + std::to_string(phase));
    show_definition_choices(select_construction_phase);
}

static generic_button definition_controls[] = {
    {340, 5, 152, 24, [](const generic_button *) { window_editor_select_special_attribute_mapping_show(PARAMETER_TYPE_HOUSING_BUILDING, select_house_definition, definition_target); }},
    {500, 5, 180, 24, [](const generic_button *) { window_editor_select_special_attribute_mapping_show(PARAMETER_TYPE_CONSTRUCTION_BUILDING, select_construction_definition, definition_target); }}
};

static void draw_definition_controls()
{
    const char *labels[] = {"TR_EDITOR_HOUSE_MODELS", "TR_EDITOR_CONSTRUCTION_COSTS"};
    for (int i = 0; i < 2; ++i) {
        const auto &button = definition_controls[i];
        button_border_draw(button.x, button.y, button.width, button.height, definition_control_focus == static_cast<unsigned int>(i + 1));
        lang_text_draw_centered(labels[i], button.x, button.y + 5, button.width, FONT_NORMAL_BLACK, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_BLACK)->line_height));
    }
}
