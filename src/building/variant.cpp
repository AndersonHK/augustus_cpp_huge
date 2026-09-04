#include "figure/figure.h"
#include "building/building.h"
#include "variant.h"

#include "building/building_type_registry_internal.h"
#include "building/rotation.h"
#include "city/view.h"
#include "map/random.h"

#include <cstddef>

#define CITY_DIRECTION_ANY -1
typedef struct {
    const unsigned char number_of_variants;
    const char *type_text_id;
    int orientation;
} building_variant;

static building_variant variants[] = {
    {6, "pavilion", CITY_DIRECTION_ANY},
    {12, "decorative_column", 0},
    {12, "decorative_column", 1},
    {12, "decorative_column", 2},
    {12, "decorative_column", 3},
    {3, "large_mausoleum", 0},
    {3, "large_mausoleum", 1},
    {3, "large_mausoleum", 2},
    {3, "large_mausoleum", 3},
    {4, "watchtower", 0},
    {4, "watchtower", 1},
    {4, "watchtower", 2},
    {4, "watchtower", 3},
    {2, "city_mint", CITY_DIRECTION_ANY},
};

#define BUILDINGS_WITH_VARIANTS (sizeof(variants) / sizeof(building_variant))

static const building_type_registry_impl::GraphicsTarget *rotation_graphics_target(building_type type)
{
    const building_type_registry_impl::BuildingType *definition =
        building_type_registry_impl::definition_for_type(type);
    if (!definition || !definition->has_graphic()) {
        return nullptr;
    }

    const building_type_registry_impl::GraphicsTarget &target = definition->graphics().default_target();
    if (!target.has_options() ||
        target.option_selection() != building_type_registry_impl::GraphicsOptionSelection::BuildRotation) {
        return nullptr;
    }
    return &target;
}

int building_variant_has_variants(building_type type)
{
    for (size_t i = 0; i < BUILDINGS_WITH_VARIANTS; i++) {
        if (building_type_registry_impl::type_attr_is(type, variants[i].type_text_id)) {
            return 1;
        }
    }
    return rotation_graphics_target(type) ? 1 : 0;
}

static building_variant *get_variant_data(building_type type)
{
    if (!building_variant_has_variants(type)) {
        return 0;
    }

    building_variant *variant = 0;
    for (size_t i = 0; i < BUILDINGS_WITH_VARIANTS; i++) {
        if ((city_view_orientation() / 2 != variants[i].orientation && variants[i].orientation != CITY_DIRECTION_ANY)) {
            continue;
        }

        if (building_type_registry_impl::type_attr_is(type, variants[i].type_text_id)) {
            variant = &variants[i];
            break;
        }
    }
    return variant;
}

int building_variant_get_number_of_variants(building_type type)
{
    building_variant *variant = get_variant_data(type);

    if (variant) {
        return variant->number_of_variants;
    }

    if (const building_type_registry_impl::GraphicsTarget *target = rotation_graphics_target(type)) {
        return target->option_count();
    }
    return 0;
}

int building_variant_get_graphics_option(const Building &building_obj, int force_reseed, unsigned char graphics_variant)
{
    const building_type_registry_impl::BuildingType *definition = building_obj.type;
    if (!definition || !definition->has_graphic()) {
        return -1;
    }

    const building_type_registry_impl::GraphicsTarget *target =
        definition ? definition->graphics().resolve_target(building_obj) : nullptr;
    if (!target || !target->has_options()) {
        return -1;
    }

    const int option_count = target->option_count();
    if (option_count <= 1) {
        return 0;
    }
    if (target->option_selection() == building_type_registry_impl::GraphicsOptionSelection::BuildRotation) {
        return graphics_variant % option_count;
    }
    if (force_reseed) {
        return map_random_get(building_obj.grid_offset()) % option_count;
    }
    return graphics_variant % option_count;
}
