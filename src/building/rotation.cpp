#include "rotation.h"
#include "translation/translation.h"

#include "building/connectable.h"
#include "building/construction.h"
#include "building/building_type_registry_internal.h"
#include "building/menu.h"
#include "building/properties.h"
#include "building/variant.h"
#include "city/view.h"
#include "city/warning.h"
#include "core/config.h"
#include "core/direction.h"
#include "core/string.h"
#include "core/time.h"
#include "map/grid.h"

#define MAX_ROTATION 3

static struct {
    int rotation;
    int extra_rotation;
    int road_orientation;
    building_type retained_type;
    int retained_rotation;
    int retained_preferred_rotation;
    uint8_t rotation_text[100];
} data = { 0, 0, 1, BUILDING_NONE, 0, 0 };

static void clear_retained_placement_rotation()
{
    data.retained_type = BUILDING_NONE;
}

static int get_num_rotations(building_type type)
{
    const building_type_registry_impl::BuildingType *definition =
        building_type_registry_impl::definition_for_type(type);
    if (definition && definition->has_rotated_placement_geometry()) {
        return 4;
    }
    int variants = building_construction_type_num_cycles(type);
    if (variants > 1) {
        return variants;
    }
    variants = building_variant_get_number_of_variants(type);
    if (variants > 0) {
        return variants;
    }
    variants = building_connectable_num_variants(type);
    if (variants > 0) {
        return variants;
    }
    if (building_properties_for_type(type)->rotation_offset) {
        return 2;
    }
    if (building_type_registry_impl::type_attr_is(type, "shrines")) {
        return 10;
    }
    static const char *temple_menus[] = { "large_temples", "small_temples" };
    if (building_type_registry_impl::type_attr_is_any(type, temple_menus, 2)) {
        return 5;
    }
    return 0;
}

static void update_rotation_message(building_type type)
{
    uint8_t *cursor = data.rotation_text;
    const int rotations = get_num_rotations(type);
    if (!building_variant_has_variants(type) && !rotations) {
        return; // only show for buildings with variants
    }
    cursor += string_from_int(cursor, data.extra_rotation + 1, 0);
    cursor = string_copy(string_from_ascii("/"), cursor, 100 - (int) (cursor - data.rotation_text));
    cursor += string_from_int(cursor, rotations, 0);
    cursor = string_copy(string_from_ascii(" "), cursor, 100 - (int) (cursor - data.rotation_text));
    string_copy(lang_get_building_type_string(type), cursor, 100 - (int) (cursor - data.rotation_text));

    city_warning_show(WARNING_BUILDING_ROTATION, data.rotation_text);
}

static void rotate_forward(void)
{
    clear_retained_placement_rotation();
    building_type type = building_construction_type();
    if (building_construction_type_can_cycle(type)) {
        building_construction_cycle_forward();
        type = building_construction_type();
    }
    data.rotation += 1;
    data.extra_rotation += 1;
    if (data.rotation > MAX_ROTATION) {
        data.rotation = 0;
    }
    const int rotations = get_num_rotations(type);
    if (rotations <= 0 || data.extra_rotation >= rotations) {
        data.extra_rotation = 0;
    }
    update_rotation_message(type);
}

static void rotate_backward(void)
{
    clear_retained_placement_rotation();
    building_type type = building_construction_type();
    if (building_construction_type_can_cycle(type)) {
        building_construction_cycle_back();
        type = building_construction_type();
    }
    data.rotation -= 1;
    data.extra_rotation -= 1;
    if (data.rotation < 0) {
        data.rotation = MAX_ROTATION;
    }
    if (data.extra_rotation < 0) {
        const int rotations = get_num_rotations(type);
        data.extra_rotation = rotations > 0 ? rotations - 1 : 0;
    }
    update_rotation_message(type);
}

int building_rotation_get_road_orientation(void)
{
    return data.road_orientation;
}

int building_rotation_get_rotation(void)
{
    return data.rotation;
}

int building_rotation_get_retained_placement_rotation(building_type type, int *rotation)
{
    if (!rotation || data.retained_type != type ||
        data.retained_preferred_rotation != data.rotation) {
        return 0;
    }
    *rotation = data.retained_rotation;
    return 1;
}

void building_rotation_retain_placement_rotation(building_type type, int rotation)
{
    data.retained_type = type;
    data.retained_rotation = (rotation % 4 + 4) % 4;
    data.retained_preferred_rotation = data.rotation;
}

int building_rotation_get_rotation_with_limit(int limit)
{
    return data.extra_rotation % limit;
}

void building_rotation_rotate_forward(void)
{
    if (building_rotation_type_has_rotations(building_construction_type())) {
        rotate_forward();
        data.road_orientation = data.road_orientation == 1 ? 2 : 1;
    }
}

void building_rotation_rotate_backward(void)
{
    if (building_rotation_type_has_rotations(building_construction_type())) {
        rotate_backward();
        data.road_orientation = data.road_orientation == 1 ? 2 : 1;
    }
}

void building_rotation_reset_rotation(void)
{
    clear_retained_placement_rotation();
    data.rotation = 0;
    data.extra_rotation = 0;
}

void building_rotation_setup_rotation(int variant)
{
    building_rotation_reset_rotation();

    for (int i = 0; i < variant; i++) {
        building_rotation_rotate_forward();
    }
    update_rotation_message(building_construction_type());
}

void building_rotation_remove_rotation(void)
{
    city_warning_clear(WARNING_BUILDING_ROTATION);
}

int building_rotation_get_building_orientation(int building_rotation)
{
    return (2 * building_rotation + city_view_orientation()) % 8;
}

int building_rotation_get_delta_with_rotation(int default_delta)
{
    switch (data.rotation) {
        case 0:
            return map_grid_delta(default_delta, 0);
        case 1:
            return map_grid_delta(0, -default_delta);
        case 2:
            return map_grid_delta(-default_delta, 0);
        default:
            return map_grid_delta(0, default_delta);
    }
}

void building_rotation_get_offset_with_rotation(int offset, int rotation, int *x, int *y)
{
    switch (rotation) {
        case 0:
            *x = offset;
            *y = 0;
            return;
        case 1:
            *x = 0;
            *y = -offset;
            return;
        case 2:
            *x = -offset;
            *y = 0;
            return;
        default:
            *x = 0;
            *y = offset;
            return;
    }
}

int building_rotation_get_corner(int rotation)
{
    switch (rotation) {
        case DIR_2_RIGHT:
            return 4; // left corner
        case DIR_4_BOTTOM:
            return 8; // bottom corner
        case DIR_6_LEFT:
            return 5; // right corner
        default:
            return 0; // top corner
    }
}

static int menu_has_buildings_with_cycles(build_menu_group menu, building_type type)
{
    int num_items = building_menu_count_items(menu);
    int item_index = -1;
    for (int i = 0; i < num_items; i++) {
        item_index = building_menu_next_index(menu, item_index);
        building_type item_type = building_menu_type(menu, item_index);
        if (item_type != type && building_construction_type_can_cycle(item_type)) {
            return 1;
        }
    }
    return 0;
}

int building_rotation_type_has_rotations(building_type type)
{
    const building_type_registry_impl::BuildingType *definition =
        building_type_registry_impl::definition_for_type(type);
    if (definition && definition->has_rotated_placement_geometry()) {
        return 1;
    }
    if (building_variant_has_variants(type) || building_properties_for_type(type)->rotation_offset ||
        building_construction_type_can_cycle(type)) {
        return 1;
    }
    build_menu_group menu = static_cast<build_menu_group>(building_menu_get_submenu_for_type(type));
    if (menu) {
        return menu_has_buildings_with_cycles(menu, type);
    }
    return 0;
}
