#include "building_graphics_contract_test.h"

#include "assets/image_group_payload.h"
#include "building/BuildingGraphicsDef.h"
#include "building/FoundationDef.h"
#include "building/building_type_registry_internal.h"

#include <cstring>
#include <ostream>
#include <string>

namespace {

bool target_entry_has_footprint(
    const building_type_registry_impl::GraphicsTarget &target,
    std::ostream &errors,
    const char *type)
{
    if (!target.has_path() || !target.has_image() || !image_group_payload_load(target.path())) {
        errors << "Building graphics contract has an incomplete native target for " << type << ".\n";
        return false;
    }
    const ImageGroupPayload *payload = image_group_payload_get(target.path());
    const ImageGroupEntry *entry = payload ? payload->entry_for(target.image()) : nullptr;
    if (!entry || !entry->footprint()) {
        errors << "Building graphics contract has no native footprint for " << type << ".\n";
        return false;
    }
    return true;
}

bool target_entry_has_fixed_1_to_1_size(const building_type_registry_impl::GraphicsTarget &target, std::ostream &errors, const char *type)
{
    if (!target.has_path() || !target.has_image() || !image_group_payload_load(target.path())) return false;
    const ImageGroupPayload *payload = image_group_payload_get(target.path());
    const ImageGroupEntry *entry = payload ? payload->entry_for(target.image()) : nullptr;
    const render_logical_size logical = entry ? entry->fixed_logical_size() : render_logical_size{};
    if (!entry || logical.width != entry->source_pixel_width() * RENDER_LOGICAL_UNITS_PER_PIXEL ||
        logical.height != entry->source_pixel_height() * RENDER_LOGICAL_UNITS_PER_PIXEL) {
        errors << "Bridge logical-size metadata is not expressed in 1/120-pixel units: " << type << ".\n";
        return false;
    }
    return true;
}

} // namespace

bool validate_native_gatehouse_bridge_graphics_contract(std::ostream &errors)
{
    using namespace building_type_registry_impl;

    const BuildingType *gatehouse = definition_for_type(type_from_attr("gatehouse"));
    if (!gatehouse) {
        errors << "Building graphics contract is missing gatehouse.\n";
        return false;
    }
    const GraphicsTarget &gate_target = gatehouse->graphics().default_target();
    if (!gate_target.has_path() || std::strcmp(gate_target.path(), "Military\\Tower") != 0 ||
        gate_target.option_selection() != GraphicsOptionSelection::GatehouseOrientation ||
        gate_target.option_count() != 2 || !gate_target.has_layer_role("gatehouse_overlay")) {
        errors << "Gatehouse does not own its complete orientation graphics strategy.\n";
        return false;
    }
    for (int option = 0; option < gate_target.option_count(); ++option) {
        if (!target_entry_has_footprint(gate_target.resolved_option(option), errors, "gatehouse")) {
            return false;
        }
    }

    struct ExpectedBridgeTarget {
        const char *type;
        const char *path;
        int option_count;
        int no_draw_option;
    };
    static const ExpectedBridgeTarget bridges[] = {
        { "low_bridge", "Terrain\\low_bridge_images", 6, -1 },
        { "ship_bridge", "Terrain\\ship_bridge_images", 9, 6 },
    };
    for (const ExpectedBridgeTarget &expected : bridges) {
        const BuildingType *bridge = definition_for_type(type_from_attr(expected.type));
        if (!bridge) {
            errors << "Building graphics contract is missing " << expected.type << ".\n";
            return false;
        }
        const GraphicsTarget &target = bridge->graphics().default_target();
        if (!target.has_path() || std::strcmp(target.path(), expected.path) != 0 ||
            target.option_selection() != GraphicsOptionSelection::StableVariant ||
            target.option_count() != expected.option_count) {
            errors << "Bridge does not own its complete native strategy: " << expected.type << ".\n";
            return false;
        }
        for (int option = 0; option < target.option_count(); ++option) {
            const GraphicsTarget resolved = target.resolved_option(option);
            if (option == expected.no_draw_option) {
                if (!resolved.no_draw() || !resolved.uses_terrain_foundation()) {
                    errors << "Ship bridge no-draw option lost its terrain foundation.\n";
                    return false;
                }
            } else if (!target_entry_has_footprint(resolved, errors, expected.type) ||
                !target_entry_has_fixed_1_to_1_size(resolved, errors, expected.type)) {
                return false;
            }
        }
    }
    return true;
}

bool validate_authoritative_building_graphics_contract(std::ostream &errors)
{
    using namespace building_type_registry_impl;

    struct DirectTarget {
        const char *type;
        const char *path;
        const char *image;
    };
    static const DirectTarget direct_targets[] = {
        { "tower", "Military\\Tower", "Image_0000" },
        { "vacant_lot", "Aesthetics\\House_Vacant_Lot", "Image_0000" },
    };
    for (const DirectTarget &expected : direct_targets) {
        const BuildingType *definition = definition_for_type(type_from_attr(expected.type));
        const GraphicsTarget &target = definition ? definition->graphics().default_target() : GraphicsTarget{};
        if (!definition || target.has_options() || !target.has_path() || !target.has_image() ||
            std::strcmp(target.path(), expected.path) != 0 ||
            std::strcmp(target.image(), expected.image) != 0 ||
            !target_entry_has_footprint(target, errors, expected.type)) {
            errors << "Building does not own its direct native graphics: " << expected.type << ".\n";
            return false;
        }
    }

    const BuildingType *arch = definition_for_type(type_from_attr("triumphal_arch"));
    const GraphicsTarget &arch_target = arch ? arch->graphics().default_target() : GraphicsTarget{};
    static const char *arch_images[] = { "Image_0002", "Image_0000", "Image_0002", "Image_0000" };
    if (!arch || arch_target.option_selection() != GraphicsOptionSelection::Orientation ||
        arch_target.option_count() != 4) {
        errors << "Triumphal arch does not own its four-way orientation graphics.\n";
        return false;
    }
    for (int option = 0; option < 4; ++option) {
        const GraphicsTarget resolved = arch_target.resolved_option(option);
        if (!resolved.has_path() || !resolved.has_image() ||
            std::strcmp(resolved.path(), "Monuments\\Triumphal_Arch") != 0 ||
            std::strcmp(resolved.image(), arch_images[option]) != 0 ||
            !target_entry_has_footprint(resolved, errors, "triumphal_arch")) {
            errors << "Triumphal arch orientation target is incomplete.\n";
            return false;
        }
    }

    struct RubbleTarget {
        const char *type;
        int options;
    };
    static const RubbleTarget rubble_targets[] = {
        { "rubble", 8 },
        { "burning_ruin", 5 },
    };
    for (const RubbleTarget &expected : rubble_targets) {
        const BuildingType *definition = definition_for_type(type_from_attr(expected.type));
        const GraphicsTarget &target = definition ? definition->graphics().default_target() : GraphicsTarget{};
        if (!definition || target.option_selection() != GraphicsOptionSelection::Rubble ||
            target.option_count() != expected.options) {
            errors << "Rubble building does not own its native variants: " << expected.type << ".\n";
            return false;
        }
        for (int option = 0; option < target.option_count(); ++option) {
            if (!target_entry_has_footprint(target.resolved_option(option), errors, expected.type)) {
                return false;
            }
        }
    }

    static const char *fort_types[] = {
        "fort_archers", "fort_javelin", "fort_legionaries", "fort_mounted", "fort_swords"
    };
    for (const char *type : fort_types) {
        const BuildingType *definition = definition_for_type(type_from_attr(type));
        const GraphicsTarget &target = definition ? definition->graphics().default_target() : GraphicsTarget{};
        if (!definition || !definition->has_graphic()) {
            errors << "Fort does not own native graphics: " << type << ".\n";
            return false;
        }
        if (target.has_options()) {
            for (int option = 0; option < target.option_count(); ++option) {
                if (!target_entry_has_footprint(target.resolved_option(option), errors, type)) {
                    return false;
                }
            }
        } else if (!target_entry_has_footprint(target, errors, type)) {
            return false;
        }
    }
    bool all_world_graphics_valid = true;
    for (int runtime_type = BUILDING_NONE + 1; runtime_type < BUILDING_TYPE_MAX; ++runtime_type) {
        const BuildingType *definition = definition_for_type(static_cast<building_type>(runtime_type));
        if (!definition || !definition->foundation_def() || definition->has_graphic()) {
            continue;
        }
        const ConstructionToolDefinition &tool = definition->tool();
        const bool terrain_owned = definition->has_tile() || tool.is_road() ||
            tool.is_highway() || tool.is_aqueduct();
        if (!terrain_owned) {
            errors << "World BuildingType has no authoritative graphics definition: "
                << definition->attr() << ".\n";
            all_world_graphics_valid = false;
        }
    }
    return all_world_graphics_valid;
}

bool validate_native_statue_orientation_graphics_contract(std::ostream &errors)
{
    using namespace building_type_registry_impl;

    for (int building_orientation = 0; building_orientation < 4; ++building_orientation) {
        for (int view_orientation = 0; view_orientation < 8; view_orientation += 2) {
            const int expected_legacy_pair = (building_orientation + view_orientation / 2) % 2;
            const int selected = graphics_orientation_option_index(
                building_orientation, view_orientation, 2);
            if (selected != expected_legacy_pair) {
                errors << "Statue orientation strategy diverged from legacy parity at building rotation "
                    << building_orientation << " and view orientation " << view_orientation << ".\n";
                return false;
            }
        }
    }

    struct ExpectedStatueTarget {
        const char *type;
        const char *base_image;
        const char *rotated_path;
        const char *rotated_image;
        int foundation_size;
    };
    static const ExpectedStatueTarget statues[] = {
        { "small_statue", "Image_0000", "Aesthetics\\V_Small_Statue_R", "V Small Statue R", 1 },
        { "medium_statue", "Image_0001", "Aesthetics\\Med_Statue_R", "Med_Statue_R", 2 },
    };
    for (const ExpectedStatueTarget &expected : statues) {
        const BuildingType *statue = definition_for_type(type_from_attr(expected.type));
        if (!statue || !statue->foundation_def() ||
            statue->foundation_def()->width() != expected.foundation_size ||
            statue->foundation_def()->height() != expected.foundation_size) {
            errors << "Statue graphics contract is missing type or foundation: " << expected.type << ".\n";
            return false;
        }
        const GraphicsTarget &target = statue->graphics().default_target();
        if (!target.has_path() || std::strcmp(target.path(), "Aesthetics\\Statue") != 0 ||
            target.option_selection() != GraphicsOptionSelection::Orientation || target.option_count() != 2) {
            errors << "Statue does not own a two-way native orientation strategy: " << expected.type << ".\n";
            return false;
        }
        const GraphicsTarget base = target.resolved_option(0);
        const GraphicsTarget rotated = target.resolved_option(1);
        if (std::strcmp(base.path(), "Aesthetics\\Statue") != 0 ||
            std::strcmp(base.image(), expected.base_image) != 0 ||
            std::strcmp(rotated.path(), expected.rotated_path) != 0 ||
            std::strcmp(rotated.image(), expected.rotated_image) != 0 ||
            !target_entry_has_footprint(base, errors, expected.type) ||
            !target_entry_has_footprint(rotated, errors, expected.type)) {
            errors << "Statue orientation alternatives are incomplete: " << expected.type << ".\n";
            return false;
        }
    }
    return true;
}

bool validate_native_building_selection_parity(std::ostream &errors)
{
    using namespace building_type_registry_impl;

    for (int percentage = 0; percentage <= 100; ++percentage) {
        const int growth = percentage / 5;
        const int common_stage = growth / 5;
        const int advanced_fields = growth % 5;
        for (int field = 0; field < 5; ++field) {
            const int expected = common_stage + (field < advanced_fields ? 1 : 0);
            const int selected = graphics_farm_field_option_index(percentage, field, 5);
            if (selected != expected) {
                errors << "Farm field XML selection diverged from legacy growth distribution at "
                    << percentage << "% for field " << field << ".\n";
                return false;
            }
        }
    }

    if (graphics_farm_field_option_index(-1, -1, 5) != 0 ||
        graphics_farm_field_option_index(101, 5, 5) != 4 ||
        graphics_farm_field_option_index(50, 2, 0) != 0) {
        errors << "Farm field XML selection does not safely clamp invalid runtime state.\n";
        return false;
    }
    return true;
}

bool validate_native_hippodrome_graphics_contract(std::ostream &errors)
{
    using namespace building_type_registry_impl;

    struct ExpectedPart {
        const char *type;
        const char *paths[4];
        const char *images[4];
    };
    static const ExpectedPart parts[] = {
        {
            "hippodrome",
            { "Health_Culture\\Hippodrome_2", "Health_Culture\\Hippodrome_1",
                "Health_Culture\\Hippodrome_2", "Health_Culture\\Hippodrome_1" },
            { "Image_0000", "Image_0004", "Image_0004", "Image_0000" },
        },
        {
            "hippodrome_middle",
            { "Health_Culture\\Hippodrome_2", "Health_Culture\\Hippodrome_1",
                "Health_Culture\\Hippodrome_2", "Health_Culture\\Hippodrome_1" },
            { "Image_0002", "Image_0002", "Image_0002", "Image_0002" },
        },
        {
            "hippodrome_end",
            { "Health_Culture\\Hippodrome_2", "Health_Culture\\Hippodrome_1",
                "Health_Culture\\Hippodrome_2", "Health_Culture\\Hippodrome_1" },
            { "Image_0004", "Image_0000", "Image_0000", "Image_0004" },
        },
    };

    const BuildingType *definitions[3] = {};
    for (int part_index = 0; part_index < 3; ++part_index) {
        const ExpectedPart &expected = parts[part_index];
        const BuildingType *definition = definition_for_type(type_from_attr(expected.type));
        definitions[part_index] = definition;
        if (!definition || !definition->foundation_def() ||
            definition->foundation_def()->width() != 5 || definition->foundation_def()->height() != 5) {
            errors << "Hippodrome graphics contract is missing a 5x5 part: " << expected.type << ".\n";
            return false;
        }
        const GraphicsTarget &target = definition->graphics().default_target();
        if (target.option_selection() != GraphicsOptionSelection::BuildRotation || target.option_count() != 4) {
            errors << "Hippodrome part does not own four build-rotation targets: " << expected.type << ".\n";
            return false;
        }
        for (int option = 0; option < 4; ++option) {
            const GraphicsTarget resolved = target.resolved_option(option);
            if (!resolved.has_path() || !resolved.has_image() ||
                std::strcmp(resolved.path(), expected.paths[option]) != 0 ||
                std::strcmp(resolved.image(), expected.images[option]) != 0 ||
                !target_entry_has_footprint(resolved, errors, expected.type)) {
                errors << "Hippodrome build-rotation target changed for " << expected.type
                    << " option " << option << ".\n";
                return false;
            }
        }
    }

    const BuildingType *owner = definitions[0];
    const CompositionOffset middle_offset = owner->composition().children().size() == 2 ?
        owner->composition().children()[0].canonical_offset() : CompositionOffset{};
    const CompositionOffset end_offset = owner->composition().children().size() == 2 ?
        owner->composition().children()[1].canonical_offset() : CompositionOffset{};
    if (!owner->has_composition() || owner->composition().children().size() != 2 ||
        owner->composition().children()[0].role != "middle" ||
        owner->composition().children()[0].type != definitions[1] ||
        owner->composition().children()[0].orientation != CompositionChildOrientation::InheritOwner ||
        !middle_offset.defined || middle_offset.x != 5 || middle_offset.y != 0 ||
        owner->composition().children()[1].role != "end" ||
        owner->composition().children()[1].type != definitions[2] ||
        owner->composition().children()[1].orientation != CompositionChildOrientation::InheritOwner ||
        !end_offset.defined || end_offset.x != 10 || end_offset.y != 0) {
        errors << "Hippodrome composition roles or offsets no longer match its native graphics parts.\n";
        return false;
    }

    const int phased = owner->has_phased_construction();
    for (const BuildingType *definition : definitions) {
        if (definition->has_phased_construction() != phased ||
            definition->construction().phase_count() != (phased ? 4 : 0)) {
            errors << "Hippodrome stack mixed phased and instant part definitions.\n";
            return false;
        }
        if (!phased) {
            continue;
        }
        for (int phase_index = 1; phase_index <= 4; ++phase_index) {
            const ConstructionPhase *phase = definition->construction().phase(phase_index);
            const GraphicsTarget *phase_graphics = definition->graphics().construction_phase(phase_index);
            if (!phase || !phase_graphics || phase_graphics->option_selection() != GraphicsOptionSelection::BuildRotation || phase_graphics->option_count() != 4) {
                errors << "Hippodrome construction phase lacks four native rotation targets.\n";
                return false;
            }
            for (int option = 0; option < 4; ++option) {
                if (!target_entry_has_footprint(phase_graphics->resolved_option(option), errors, definition->attr())) {
                    return false;
                }
            }
        }
    }
    return true;
}

bool validate_native_overlay_summary_graphics_contract(std::ostream &errors)
{
    using namespace building_type_registry_impl;

    if (!graphics_overlay_summary_draws(
            GraphicsOverlaySummaryPolicy::EveryDrawTile, 0, 0, 0) ||
        !graphics_overlay_summary_draws(
            GraphicsOverlaySummaryPolicy::EveryDrawTile, 1, 0, 0) ||
        !graphics_overlay_summary_draws(
            GraphicsOverlaySummaryPolicy::CompositionOwner, 1, 1, 1) ||
        graphics_overlay_summary_draws(
            GraphicsOverlaySummaryPolicy::CompositionOwner, 1, 1, 0) ||
        graphics_overlay_summary_draws(
            GraphicsOverlaySummaryPolicy::CompositionOwner, 1, 0, 1) ||
        !graphics_overlay_summary_draws(
            GraphicsOverlaySummaryPolicy::CompositionOwner, 0, 1, 1)) {
        errors << "Overlay-summary draw policy changed for an owner, child, or ordinary building.\n";
        return false;
    }

    if (!graphics_overlay_summary_policy_is_valid_for_test("composition_owner") ||
        graphics_overlay_summary_policy_is_valid_for_test(nullptr) ||
        graphics_overlay_summary_policy_is_valid_for_test("") ||
        graphics_overlay_summary_policy_is_valid_for_test("each_draw_tile") ||
        graphics_overlay_summary_policy_is_valid_for_test("farm") ||
        graphics_overlay_summary_policy_is_valid_for_test("owner")) {
        errors << "Graphics overlay_summary parser accepted a malformed mode or rejected a valid mode.\n";
        return false;
    }

    static const char *const farm_stems[] = {
        "fruit", "olive", "pig", "vegetable", "vines", "wheat"
    };
    for (const char *stem : farm_stems) {
        const std::string owner_attr = std::string(stem) + "_farm";
        const std::string field_attr = owner_attr + "_field";
        const BuildingType *owner = definition_for_type(type_from_attr(owner_attr.c_str()));
        const BuildingType *field = definition_for_type(type_from_attr(field_attr.c_str()));
        if (!owner || !field || !owner->has_composition() ||
            owner->composition().children().size() != 5) {
            errors << "Overlay-summary graphics contract is missing farm owner or field "
                << owner_attr << ".\n";
            return false;
        }

        const BuildingGraphicsDef &owner_graphics = owner->graphics();
        const BuildingGraphicsDef &field_graphics = field->graphics();
        if (std::string(owner_graphics.default_target().path()) != "Industry\\Farm_House" ||
            std::string(field_graphics.default_target().path()) != "Industry\\Farm_Crops" ||
            field_graphics.default_target().option_count() != 5) {
            errors << "Farm owner/field graphics targets are not distinct and complete: "
                << owner_attr << ".\n";
            return false;
        }
        for (int option = 0; option < field_graphics.default_target().option_count(); ++option) {
            const GraphicsTarget resolved = field_graphics.default_target().resolved_option(
                static_cast<unsigned char>(option));
            if (std::string(resolved.path()) != "Industry\\Farm_Crops" || !resolved.has_image()) {
                errors << "Farm field option escaped its crop asset: " << field_attr << ".\n";
                return false;
            }
        }
        int status_x = 0;
        int status_y = 0;
        if (!owner_graphics.has_overlay_summary_policy() ||
            owner_graphics.overlay_summary_policy() !=
                GraphicsOverlaySummaryPolicy::CompositionOwner ||
            !owner_graphics.status_icon_anchor(&status_x, &status_y) ||
            status_x != 50 || status_y != -50) {
            errors << "Farm owner does not own its overlay-summary and status-icon policy: "
                << owner_attr << ".\n";
            return false;
        }
        if (field_graphics.has_overlay_summary_policy() ||
            field_graphics.overlay_summary_policy() !=
                GraphicsOverlaySummaryPolicy::EveryDrawTile ||
            field_graphics.status_icon_anchor(&status_x, &status_y)) {
            errors << "Farm field unexpectedly duplicates owner graphics policy: "
                << field_attr << ".\n";
            return false;
        }

        for (const CompositionChildDef &child : owner->composition().children()) {
            if (child.type != field) {
                errors << "Farm owner composition contains a non-field child: "
                    << owner_attr << ".\n";
                return false;
            }
        }
    }

    const BuildingType *ordinary = definition_for_type(type_from_attr("prefecture"));
    if (!ordinary || ordinary->has_composition() ||
        ordinary->graphics().has_overlay_summary_policy() ||
        ordinary->graphics().overlay_summary_policy() !=
            GraphicsOverlaySummaryPolicy::EveryDrawTile) {
        errors << "Ordinary building no longer retains the default per-draw-tile overlay policy.\n";
        return false;
    }

    return true;
}
