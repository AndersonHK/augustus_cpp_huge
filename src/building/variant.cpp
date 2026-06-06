#include "building/building_record.h"
#include "variant.h"

#include "building/building_type_api.h"
#include "building/properties.h"
#include "building/rotation.h"
#include "city/view.h"
#include "core/image.h"

#include <cstddef>

#define CITY_DIRECTION_ANY -1
#define MAX_VARIANTS_PER_BUILDING 12

typedef struct {
    const unsigned char number_of_variants;
    const char *type_text_id;
    int variants_offsets[MAX_VARIANTS_PER_BUILDING];
    int orientation;
} building_variant;

static building_variant variants[] = {
    {6, "pavilion_blue", {0,1,2,3,4,5}, CITY_DIRECTION_ANY},
    {12, "decorative_column", {0,1,2,3,4,5,6,7,8,9,10,11}, 0},
    {12, "decorative_column", {1,0,3,2,5,4,7,6,9,8,11,10}, 1},
    {12, "decorative_column", {0,1,2,3,4,5,6,7,8,9,10,11}, 2},
    {12, "decorative_column", {1,0,3,2,5,4,7,6,9,8,11,10}, 3},
    {3, "large_mausoleum", {0,1,2}, 0},
    {3, "large_mausoleum", {0,2,1}, 1},
    {3, "large_mausoleum", {0,1,2}, 2},
    {3, "large_mausoleum", {0,2,1}, 3},
    {4, "watchtower", {0,21,52,73}, 0},
    {4, "watchtower", {21,0,73,52}, 1},
    {4, "watchtower", {0,21,52,73}, 2},
    {4, "watchtower", {21,0,73,52}, 3},
    {11, "roadblock", {0,1,2,3,4,5,6,7,8,9,10}, CITY_DIRECTION_ANY},
    {2, "city_mint", {0,6}, CITY_DIRECTION_ANY},
};

#define BUILDINGS_WITH_VARIANTS (sizeof(variants) / sizeof(building_variant))

static int variant_matches_type(const building_variant &variant, building_type type)
{
    return type == building_type_registry_runtime_id_from_text(variant.type_text_id);
}

int building_variant_has_variants(building_type type)
{
    for (size_t i = 0; i < BUILDINGS_WITH_VARIANTS; i++) {
        if (variant_matches_type(variants[i], type)) {
            return 1;
        }
    }
    return 0;
}

int building_variant_get_image_id(building_type type)
{
    int variant_number = building_variant_get_number_of_variants(type);
    int rotation = building_rotation_get_rotation_with_limit(variant_number);
    return building_variant_get_image_id_with_rotation(type, rotation);
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

        if (variant_matches_type(variants[i], type)) {
            variant = &variants[i];
            break;
        }
    }
    return variant;
}

int building_variant_get_image_id_with_rotation(building_type type, int rotation)
{
    building_variant *variant = get_variant_data(type);

    if (!variant) {
        return 0;
    }
    rotation = rotation % variant->number_of_variants;
    int group_id = building_properties_for_type(type)->image_group;
    int image_offset = building_properties_for_type(type)->image_offset + variant->variants_offsets[rotation];
    int image_id = group_id + image_offset;

    return image_id;
}

int building_variant_get_offset_with_rotation(building_type type, int rotation)
{
    building_variant *variant = get_variant_data(type);

    if (!variant) {
        return 0;
    }
    return variant->variants_offsets[rotation];
}

int building_variant_get_number_of_variants(building_type type)
{
    building_variant *variant = get_variant_data(type);

    if (!variant) {
        return 0;
    }
    return variant->number_of_variants;
}
