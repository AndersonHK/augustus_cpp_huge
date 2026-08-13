#include "housing_transition_planner_test.h"

#include "building/HousingProfileDef.h"
#include "building/HousingStateBridge.h"
#include "building/HousingTransitionPlanner.h"
#include "building/LegacyBuildingSaveDto.h"
#include "building/building_record.h"
#include "building/building_type_registry_internal.h"

#include <cstdint>
#include <initializer_list>
#include <iostream>
#include <string>

namespace {

using namespace building_type_registry_impl;

#define DECLARE_REMOVED_RECORD_FIELD_CONTRACT(field) \
    template <typename T> \
    concept has_record_field_##field = requires(T value) { value.field; }; \
    static_assert(!has_record_field_##field<building>, "Removed housing field returned to the runtime building record")

DECLARE_REMOVED_RECORD_FIELD_CONTRACT(size);
DECLARE_REMOVED_RECORD_FIELD_CONTRACT(house_is_merged);
DECLARE_REMOVED_RECORD_FIELD_CONTRACT(house_size);
DECLARE_REMOVED_RECORD_FIELD_CONTRACT(house_population);
DECLARE_REMOVED_RECORD_FIELD_CONTRACT(house_population_room);
DECLARE_REMOVED_RECORD_FIELD_CONTRACT(house_highest_population);
DECLARE_REMOVED_RECORD_FIELD_CONTRACT(house_unreachable_ticks);
DECLARE_REMOVED_RECORD_FIELD_CONTRACT(immigrant_figure_id);
DECLARE_REMOVED_RECORD_FIELD_CONTRACT(house_sentiment_message);
DECLARE_REMOVED_RECORD_FIELD_CONTRACT(house_criminal_active);
DECLARE_REMOVED_RECORD_FIELD_CONTRACT(house_figure_generation_delay);
DECLARE_REMOVED_RECORD_FIELD_CONTRACT(house_tax_coverage);
DECLARE_REMOVED_RECORD_FIELD_CONTRACT(house_pantheon_access);
DECLARE_REMOVED_RECORD_FIELD_CONTRACT(house_days_without_food);
DECLARE_REMOVED_RECORD_FIELD_CONTRACT(house_tavern_wine_access);
DECLARE_REMOVED_RECORD_FIELD_CONTRACT(house_tavern_food_access);
DECLARE_REMOVED_RECORD_FIELD_CONTRACT(house_arena_gladiator);
DECLARE_REMOVED_RECORD_FIELD_CONTRACT(house_arena_lion);
DECLARE_REMOVED_RECORD_FIELD_CONTRACT(monthly_levy);
DECLARE_REMOVED_RECORD_FIELD_CONTRACT(tax_income_or_storage);

#undef DECLARE_REMOVED_RECORD_FIELD_CONTRACT

template <typename T>
concept has_record_house_level = requires(T value) { value.subtype.house_level; };

template <typename T>
concept has_record_house_service_union = requires(T value) { value.data.house; };

template <typename T>
concept has_record_house_happiness = requires(T value) { value.sentiment.house_happiness; };

template <typename T>
concept has_record_nested_native_anger = requires(T value) { value.sentiment.native_anger; };

template <typename T>
concept has_generic_record_aliases = requires(T value) {
    value.levy_amount;
    value.collected_tax_income;
    value.native_anger;
};

static_assert(!has_record_house_level<building>);
static_assert(!has_record_house_service_union<building>);
static_assert(!has_record_house_happiness<building>);
static_assert(!has_record_nested_native_anger<building>);
static_assert(has_generic_record_aliases<building>);

const BuildingType *type_token(std::uintptr_t value)
{
    return reinterpret_cast<const BuildingType *>(value);
}

HousingMergeParticipant participant(
    uint32_t id,
    const BuildingType *target,
    std::initializer_list<HousingFootprintCell> cells,
    bool qualifies = true)
{
    return { id, target, cells, qualifies };
}

bool housing_states_match(const HousingState &a, const HousingState &b)
{
    return a.population == b.population &&
        a.population_room == b.population_room &&
        a.highest_population == b.highest_population &&
        a.unreachable_ticks == b.unreachable_ticks &&
        a.immigrant_figure_id == b.immigrant_figure_id &&
        a.services.theater == b.services.theater &&
        a.services.amphitheater_actor == b.services.amphitheater_actor &&
        a.services.amphitheater_gladiator == b.services.amphitheater_gladiator &&
        a.services.colosseum_gladiator == b.services.colosseum_gladiator &&
        a.services.colosseum_lion == b.services.colosseum_lion &&
        a.services.hippodrome == b.services.hippodrome &&
        a.services.school == b.services.school &&
        a.services.library == b.services.library &&
        a.services.academy == b.services.academy &&
        a.services.barber == b.services.barber &&
        a.services.clinic == b.services.clinic &&
        a.services.bathhouse == b.services.bathhouse &&
        a.services.hospital == b.services.hospital &&
        a.services.temple_ceres == b.services.temple_ceres &&
        a.services.temple_neptune == b.services.temple_neptune &&
        a.services.temple_mercury == b.services.temple_mercury &&
        a.services.temple_mars == b.services.temple_mars &&
        a.services.temple_venus == b.services.temple_venus &&
        a.services.num_foods == b.services.num_foods &&
        a.services.entertainment == b.services.entertainment &&
        a.services.education == b.services.education &&
        a.services.health == b.services.health &&
        a.services.num_gods == b.services.num_gods &&
        a.no_space_to_expand == b.no_space_to_expand &&
        a.devolve_delay == b.devolve_delay &&
        a.evolve_text_id == b.evolve_text_id &&
        a.sentiment_message == b.sentiment_message &&
        a.criminal_active == b.criminal_active &&
        a.figure_generation_delay == b.figure_generation_delay &&
        a.tax_coverage == b.tax_coverage &&
        a.monthly_levy == b.monthly_levy &&
        a.tax_income == b.tax_income &&
        a.pantheon_access == b.pantheon_access &&
        a.days_without_food == b.days_without_food &&
        a.happiness == b.happiness &&
        a.tavern_wine_access == b.tavern_wine_access &&
        a.tavern_food_access == b.tavern_food_access &&
        a.arena_gladiator == b.arena_gladiator &&
        a.arena_lion == b.arena_lion;
}

bool legacy_housing_payload_matches_state(
    const building_save_bridge::LegacyBuildingSaveDto &legacy,
    const HousingState &state)
{
    return legacy.housing_population == state.population &&
        legacy.housing_population_room == state.population_room &&
        legacy.housing_highest_population == state.highest_population &&
        legacy.housing_unreachable_ticks == state.unreachable_ticks &&
        legacy.housing_immigrant_figure_id == state.immigrant_figure_id &&
        legacy.housing.theater == state.services.theater &&
        legacy.housing.amphitheater_actor == state.services.amphitheater_actor &&
        legacy.housing.amphitheater_gladiator == state.services.amphitheater_gladiator &&
        legacy.housing.colosseum_gladiator == state.services.colosseum_gladiator &&
        legacy.housing.colosseum_lion == state.services.colosseum_lion &&
        legacy.housing.hippodrome == state.services.hippodrome &&
        legacy.housing.school == state.services.school &&
        legacy.housing.library == state.services.library &&
        legacy.housing.academy == state.services.academy &&
        legacy.housing.barber == state.services.barber &&
        legacy.housing.clinic == state.services.clinic &&
        legacy.housing.bathhouse == state.services.bathhouse &&
        legacy.housing.hospital == state.services.hospital &&
        legacy.housing.temple_ceres == state.services.temple_ceres &&
        legacy.housing.temple_neptune == state.services.temple_neptune &&
        legacy.housing.temple_mercury == state.services.temple_mercury &&
        legacy.housing.temple_mars == state.services.temple_mars &&
        legacy.housing.temple_venus == state.services.temple_venus &&
        legacy.housing.num_foods == state.services.num_foods &&
        legacy.housing.entertainment == state.services.entertainment &&
        legacy.housing.education == state.services.education &&
        legacy.housing.health == state.services.health &&
        legacy.housing.num_gods == state.services.num_gods &&
        legacy.housing.no_space_to_expand == state.no_space_to_expand &&
        legacy.housing.devolve_delay == state.devolve_delay &&
        legacy.housing.evolve_text_id == state.evolve_text_id &&
        legacy.housing_sentiment_message == state.sentiment_message &&
        legacy.housing_criminal_active == state.criminal_active &&
        legacy.housing_generation_delay_or_rubble_seed == state.figure_generation_delay &&
        legacy.housing_tax_coverage == state.tax_coverage &&
        legacy.housing_monthly_levy_or_building_levy == state.monthly_levy &&
        legacy.housing_tax_income_or_collected_tax == state.tax_income &&
        legacy.housing_pantheon_access == state.pantheon_access &&
        legacy.housing_days_without_food == state.days_without_food &&
        legacy.housing_happiness_or_native_anger == state.happiness &&
        legacy.housing_tavern_wine_access == state.tavern_wine_access &&
        legacy.housing_tavern_food_access == state.tavern_food_access &&
        legacy.housing_arena_gladiator == state.arena_gladiator &&
        legacy.housing_arena_lion == state.arena_lion;
}

bool dedicated_housing_payload_is_zero(const building_save_bridge::LegacyBuildingSaveDto &legacy)
{
    HousingState empty;
    building_save_bridge::LegacyBuildingSaveDto expected;
    housing_state_to_legacy_save(empty, expected);

    building_save_bridge::LegacyBuildingSaveDto actual;
    actual.housing_population = legacy.housing_population;
    actual.housing_population_room = legacy.housing_population_room;
    actual.housing_highest_population = legacy.housing_highest_population;
    actual.housing_unreachable_ticks = legacy.housing_unreachable_ticks;
    actual.housing_immigrant_figure_id = legacy.housing_immigrant_figure_id;
    actual.housing = legacy.housing;
    actual.housing_sentiment_message = legacy.housing_sentiment_message;
    actual.housing_criminal_active = legacy.housing_criminal_active;
    actual.housing_tax_coverage = legacy.housing_tax_coverage;
    actual.housing_pantheon_access = legacy.housing_pantheon_access;
    actual.housing_days_without_food = legacy.housing_days_without_food;
    actual.housing_tavern_wine_access = legacy.housing_tavern_wine_access;
    actual.housing_tavern_food_access = legacy.housing_tavern_food_access;
    actual.housing_arena_gladiator = legacy.housing_arena_gladiator;
    actual.housing_arena_lion = legacy.housing_arena_lion;

    return housing_states_match(
        housing_state_from_legacy_save(actual),
        housing_state_from_legacy_save(expected));
}

} // namespace

bool validate_housing_transition_planner_contract()
{
    using namespace building_type_registry_impl;

    const BuildingType *small_tent = definition_for_type(type_from_attr("house_small_tent"));
    const BuildingType *merged_tent = definition_for_type(type_from_attr("house_small_tent_2x2"));
    const BuildingType *luxury_palace = definition_for_type(type_from_attr("house_luxury_palace"));
    if (!small_tent || !merged_tent || !luxury_palace ||
        small_tent->housing_def().profile == nullptr ||
        small_tent->housing_def().capacity != 5 ||
        small_tent->housing_def().goods_consumption_events_per_month != 1 ||
        small_tent->housing_def().mars_offering_amount != 1 ||
        small_tent->housing_def().transition(HousingTransitionKind::MergeTo).type != merged_tent ||
        merged_tent->housing_def().capacity != 20 ||
        merged_tent->housing_def().goods_consumption_events_per_month != 2 ||
        merged_tent->housing_def().mars_offering_amount != 2 ||
        luxury_palace->housing_def().goods_consumption_events_per_month != 2 ||
        luxury_palace->housing_def().mars_offering_amount != 4) {
        std::cerr << "HousingDef registry contract failed profile, capacity, balance, or transition resolution.\n";
        return false;
    }

    const BuildingType *target = type_token(1);
    const BuildingType *other_target = type_token(2);

    HousingProfileDef profile("test/profile");
    profile.compatibility_level = 7;
    profile.resident_class_value = HousingResidentClass::Plebeian;
    profile.evolution = { -4, 12 };
    profile.requirements.entertainment = 8;
    profile.requirements.water = HousingWaterRequirement::Fountain;
    profile.requirements.pottery = 1;
    profile.prosperity = 35;
    profile.tax_multiplier = 3;
    if (profile.path_id != "test/profile" || profile.compatibility_level != 7 ||
        profile.resident_class_value != HousingResidentClass::Plebeian ||
        profile.evolution.devolve_desirability != -4 || profile.evolution.evolve_desirability != 12 ||
        profile.requirements.entertainment != 8 ||
        profile.requirements.water != HousingWaterRequirement::Fountain ||
        profile.requirements.pottery != 1 || profile.prosperity != 35 || profile.tax_multiplier != 3) {
        std::cerr << "HousingProfileDef failed typed native data ownership.\n";
        return false;
    }

    const HousingProfileDef *parsed_tent = small_tent->housing_def().profile;
    const HousingProfileDef *parsed_palace = luxury_palace->housing_def().profile;
    if (!parsed_tent || !parsed_palace || parsed_tent->path_id != "house_small_tent" ||
        parsed_tent->compatibility_level != HOUSE_SMALL_TENT ||
        parsed_tent->resident_class_value != HousingResidentClass::Plebeian ||
        parsed_tent->evolution.devolve_desirability != -101 ||
        parsed_tent->evolution.evolve_desirability != -10 ||
        parsed_tent->requirements.water != HousingWaterRequirement::None ||
        parsed_tent->prosperity != 5 || parsed_tent->tax_multiplier != 1 ||
        parsed_palace->compatibility_level != HOUSE_LUXURY_PALACE ||
        parsed_palace->resident_class_value != HousingResidentClass::Patrician ||
        parsed_palace->requirements.entertainment != 80 ||
        parsed_palace->requirements.water != HousingWaterRequirement::Fountain ||
        parsed_palace->requirements.religion != 4 || parsed_palace->requirements.education != 3 ||
        parsed_palace->requirements.barber != 1 || parsed_palace->requirements.bathhouse != 1 ||
        parsed_palace->requirements.health != 2 || parsed_palace->requirements.food_types != 3 ||
        parsed_palace->requirements.pottery != 1 || parsed_palace->requirements.oil != 1 ||
        parsed_palace->requirements.furniture != 1 || parsed_palace->requirements.wine != 2 ||
        parsed_palace->prosperity != 1750 || parsed_palace->tax_multiplier != 16) {
        std::cerr << "Parsed HousingProfile requirements did not remain authoritative.\n";
        return false;
    }

    HousingDef definition;
    definition.profile = &profile;
    definition.capacity = 20;
    definition.goods_consumption_events_per_month = 2;
    definition.mars_offering_amount = 2;
    definition.transition(HousingTransitionKind::MergeTo).type = target;
    if (definition.transition(HousingTransitionKind::MergeTo).type != target ||
        !definition.consumes_goods_on_day(0) || !definition.consumes_goods_on_day(7) ||
        definition.consumes_goods_on_day(1)) {
        std::cerr << "HousingDef failed transition ownership or explicit goods cadence.\n";
        return false;
    }

    HousingState native_state;
    native_state.population = 1234;
    native_state.population_room = -25;
    native_state.highest_population = 2345;
    native_state.unreachable_ticks = 9;
    native_state.immigrant_figure_id = 0x12345678;
    native_state.services.theater = 1;
    native_state.services.amphitheater_actor = 2;
    native_state.services.amphitheater_gladiator = 3;
    native_state.services.colosseum_gladiator = 4;
    native_state.services.colosseum_lion = 5;
    native_state.services.hippodrome = 6;
    native_state.services.school = 7;
    native_state.services.library = 8;
    native_state.services.academy = 9;
    native_state.services.barber = 10;
    native_state.services.clinic = 11;
    native_state.services.bathhouse = 12;
    native_state.services.hospital = 13;
    native_state.services.temple_ceres = 14;
    native_state.services.temple_neptune = 15;
    native_state.services.temple_mercury = 16;
    native_state.services.temple_mars = 17;
    native_state.services.temple_venus = 18;
    native_state.services.num_foods = 19;
    native_state.services.entertainment = 20;
    native_state.services.education = 21;
    native_state.services.health = 22;
    native_state.services.num_gods = 23;
    native_state.no_space_to_expand = 24;
    native_state.devolve_delay = 25;
    native_state.evolve_text_id = 26;
    native_state.sentiment_message = 27;
    native_state.criminal_active = 28;
    native_state.figure_generation_delay = 29;
    native_state.tax_coverage = 30;
    native_state.monthly_levy = -31;
    native_state.tax_income = 0x12345678;
    native_state.pantheon_access = 32;
    native_state.days_without_food = 33;
    native_state.happiness = -34;
    native_state.tavern_wine_access = 35;
    native_state.tavern_food_access = 36;
    native_state.arena_gladiator = 37;
    native_state.arena_lion = 38;

    building_save_bridge::LegacyBuildingSaveDto legacy_record;
    legacy_record.size = 7;
    legacy_record.house_is_merged = 1;
    legacy_record.house_size = 4;
    legacy_record.housing_level_or_subtype = -123;
    housing_state_to_legacy_save(native_state, legacy_record);
    const HousingState round_trip = housing_state_from_legacy_save(legacy_record);
    if (!legacy_housing_payload_matches_state(legacy_record, native_state) ||
        !housing_states_match(round_trip, native_state) ||
        legacy_record.size != 7 || legacy_record.house_is_merged != 1 ||
        legacy_record.house_size != 4 || legacy_record.housing_level_or_subtype != -123) {
        std::cerr << "HousingState bridge failed the complete save DTO mapping or exact round trip.\n";
        return false;
    }

    building_save_bridge::LegacyBuildingDefinitionView housing_save_view;
    housing_save_view.runtime_size = 9;
    housing_save_view.foundation_width = 3;
    housing_save_view.foundation_height = 2;
    housing_save_view.foundation_cell_count = 5;
    housing_save_view.housing_compatibility_level = 7;
    housing_save_view.non_housing_subtype_value = 99;
    const auto housing_save = building_save_bridge::synthesize_legacy_building_save_dto(housing_save_view);
    if (housing_save.size != 3 || housing_save.house_size != 3 || !housing_save.house_is_merged ||
        housing_save.housing_level_or_subtype != 7) {
        std::cerr << "Legacy housing save DTO was not synthesized from FoundationDef/HousingDef data.\n";
        return false;
    }

    building_save_bridge::LegacyBuildingDefinitionView non_housing_save_view;
    non_housing_save_view.runtime_size = 4;
    non_housing_save_view.non_housing_subtype_value = -123;
    auto non_housing_save = building_save_bridge::synthesize_legacy_building_save_dto(non_housing_save_view);
    if (non_housing_save.size != 4 || non_housing_save.house_size || non_housing_save.house_is_merged ||
        non_housing_save.housing_level_or_subtype != -123 ||
        !dedicated_housing_payload_is_zero(non_housing_save)) {
        std::cerr << "Legacy save DTO did not isolate non-housing definition data from housing state.\n";
        return false;
    }

    non_housing_save.housing_generation_delay_or_rubble_seed = 0xa5;
    non_housing_save.housing_monthly_levy_or_building_levy = -17;
    non_housing_save.housing_tax_income_or_collected_tax = 7654321;
    non_housing_save.housing_happiness_or_native_anger = -88;
    if (non_housing_save.housing_generation_delay_or_rubble_seed != 0xa5 ||
        non_housing_save.housing_monthly_levy_or_building_levy != -17 ||
        non_housing_save.housing_tax_income_or_collected_tax != 7654321 ||
        non_housing_save.housing_happiness_or_native_anger != -88 ||
        non_housing_save.size != 4 || non_housing_save.housing_level_or_subtype != -123 ||
        !dedicated_housing_payload_is_zero(non_housing_save)) {
        std::cerr << "Legacy shared slots leaked rubble, levy, collected-tax, or native state into housing payload.\n";
        return false;
    }
    if (!building_save_bridge::legacy_housing_identity_needs_disambiguation(0xb4, 0xb4) ||
        building_save_bridge::legacy_housing_identity_needs_disambiguation(0xb5, 0xb4)) {
        std::cerr << "Legacy housing identity disambiguation crossed the exact BuildingType table boundary.\n";
        return false;
    }
    std::cout << "Validated complete HousingState save DTO mapping and non-housing shared-slot isolation.\n";

    const std::vector<HousingFootprintCell> cells = { { 4, 8 }, { 5, 8 }, { 4, 9 }, { 5, 9 } };
    const std::vector<HousingMergeParticipant> mixed_sources = {
        participant(14, target, { { 4, 8 } }),
        participant(11, target, { { 5, 8 } }),
        participant(13, target, { { 4, 9 } }),
        participant(12, target, { { 5, 9 } }),
    };

    const HousingMergePlan valid = plan_housing_merge(target, cells, mixed_sources);
    if (!valid || valid.participant_ids != std::vector<uint32_t>({ 11, 12, 13, 14 })) {
        std::cerr << "Housing merge planner failed exact mixed-type coverage or deterministic ordering.\n";
        return false;
    }

    auto incomplete = mixed_sources;
    incomplete.pop_back();
    if (plan_housing_merge(target, cells, incomplete).failure != HousingMergeFailure::IncompleteCoverage) {
        std::cerr << "Housing merge planner accepted partial target coverage.\n";
        return false;
    }

    auto incompatible = mixed_sources;
    incompatible[2].merge_target = other_target;
    if (plan_housing_merge(target, cells, incompatible).failure != HousingMergeFailure::IncompatibleParticipant) {
        std::cerr << "Housing merge planner accepted inconsistent merge targets.\n";
        return false;
    }

    auto overlapping = mixed_sources;
    overlapping[3].cells = { { 4, 9 }, { 5, 9 } };
    if (plan_housing_merge(target, cells, overlapping).failure != HousingMergeFailure::OverlappingParticipants) {
        std::cerr << "Housing merge planner accepted overlapping source footprints.\n";
        return false;
    }

    auto outside = mixed_sources;
    outside[3].cells = { { 6, 9 } };
    if (plan_housing_merge(target, cells, outside).failure != HousingMergeFailure::ParticipantOutsideTarget) {
        std::cerr << "Housing merge planner accepted a partially outside source footprint.\n";
        return false;
    }

    std::cout << "Validated generic housing merge coverage and rejection contracts.\n";
    return true;
}
