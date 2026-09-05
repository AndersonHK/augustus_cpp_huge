#pragma once

#include <algorithm>
#include <cstdint>
#include <limits>

namespace building_save_bridge {

// Save-only compatibility view for legacy fields peeled out of the runtime
// building record. state.cpp controls their historical byte positions.
struct LegacyBuildingDefinitionView {
    int runtime_size = 1;
    int foundation_width = 0;
    int foundation_height = 0;
    int foundation_cell_count = 0;
    int housing_compatibility_level = -1;
    int16_t non_housing_subtype_value = 0;
};

struct LegacyBuildingSaveDto {
    uint8_t size = 0;
    uint8_t house_is_merged = 0;
    uint8_t house_size = 0;
    int16_t housing_level_or_subtype = 0;

    int16_t housing_population = 0;
    int16_t housing_population_room = 0;
    int16_t housing_highest_population = 0;
    int16_t housing_unreachable_ticks = 0;
    uint32_t housing_immigrant_figure_id = 0;

    struct {
        uint8_t theater = 0;
        uint8_t amphitheater_actor = 0;
        uint8_t amphitheater_gladiator = 0;
        uint8_t colosseum_gladiator = 0;
        uint8_t colosseum_lion = 0;
        uint8_t hippodrome = 0;
        uint8_t school = 0;
        uint8_t library = 0;
        uint8_t academy = 0;
        uint8_t barber = 0;
        uint8_t clinic = 0;
        uint8_t bathhouse = 0;
        uint8_t hospital = 0;
        uint8_t temple_ceres = 0;
        uint8_t temple_neptune = 0;
        uint8_t temple_mercury = 0;
        uint8_t temple_mars = 0;
        uint8_t temple_venus = 0;
        uint8_t no_space_to_expand = 0;
        uint8_t num_foods = 0;
        uint8_t entertainment = 0;
        uint8_t education = 0;
        uint8_t health = 0;
        uint8_t num_gods = 0;
        uint8_t devolve_delay = 0;
        uint8_t evolve_text_id = 0;
    } housing;

    uint8_t housing_sentiment_message = 0;
    uint8_t housing_criminal_active = 0;
    uint8_t housing_tax_coverage = 0;
    uint8_t housing_pantheon_access = 0;
    uint8_t housing_days_without_food = 0;
    uint8_t housing_tavern_wine_access = 0;
    uint8_t housing_tavern_food_access = 0;
    uint8_t housing_arena_gladiator = 0;
    uint8_t housing_arena_lion = 0;

    // Compatibility slots shared with non-housing runtime state in old saves.
    uint8_t housing_generation_delay_or_rubble_seed = 0;
    int8_t housing_monthly_levy_or_building_levy = 0;
    int32_t housing_tax_income_or_collected_tax = 0;
    int8_t housing_happiness_or_native_anger = 0;
};

inline uint8_t legacy_u8(int value)
{
    return static_cast<uint8_t>(std::clamp(value, 0, static_cast<int>(std::numeric_limits<uint8_t>::max())));
}

inline LegacyBuildingSaveDto synthesize_legacy_building_save_dto(
    const LegacyBuildingDefinitionView &definition)
{
    LegacyBuildingSaveDto result;
    const int foundation_size = std::max(definition.foundation_width, definition.foundation_height);
    result.size = legacy_u8(foundation_size > 0 ? foundation_size : definition.runtime_size);
    result.housing_level_or_subtype = definition.non_housing_subtype_value;
    if (definition.housing_compatibility_level >= 0) {
        result.house_size = result.size;
        result.house_is_merged = definition.foundation_cell_count > 1 ? 1 : 0;
        result.housing_level_or_subtype = static_cast<int16_t>(std::clamp(
            definition.housing_compatibility_level,
            static_cast<int>(std::numeric_limits<int16_t>::min()),
            static_cast<int>(std::numeric_limits<int16_t>::max())));
    }
    return result;
}

inline bool legacy_housing_identity_needs_disambiguation(int save_version, int last_version_without_type_table)
{
    return save_version <= last_version_without_type_table;
}

} // namespace building_save_bridge
