#pragma once

#include "scenario/event/parameter_data.h"


void window_editor_select_special_attribute_mapping_show(parameter_type type, void (*callback)(int), int current_value);


class Terrain;
#include "graphics/generic_button.h"
void window_editor_select_terrain_show(const generic_button *button, void (*callback)(const Terrain &));

void window_editor_reset_terrain_selection();
