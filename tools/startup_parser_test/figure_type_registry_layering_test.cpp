#include "figure_type_registry_layering_test.h"

#include "assets/image_group_entry.h"
#include "figure/FigureGraphics.h"
#include "figure/figure_type_registry.h"
#include "core/image_group.h"

#include <ostream>
#include <string>

namespace {

constexpr const char *LOWER_XML =
    "<figure type=\"homeless\"><profiles default=\"legacy\"><profile id=\"legacy\">"
    "<native class=\"legacy_action\"/><owner slot=\"none\" building=\"any\" state=\"any\"/>"
    "<movement roam_ticks=\"1\" max_roam_length=\"800\" return_mode=\"none\"/>"
    "<pathing mode=\"vanilla_roaming\" terrain=\"roads_highway\"/></profile></profiles>"
    "<graphics><default max_image_offset=\"12\"><path value=\"Walkers\\Group_118\"/></default></graphics></figure>";
constexpr const char *UPPER_XML =
    "<figure type=\"homeless\"><profiles default=\"legacy\"><profile id=\"legacy\">"
    "<native class=\"legacy_action\"/><owner slot=\"none\" building=\"any\" state=\"any\"/>"
    "<movement roam_ticks=\"1\" max_roam_length=\"999\" return_mode=\"none\"/>"
    "<pathing mode=\"vanilla_roaming\" terrain=\"roads_highway\"/></profile></profiles>"
    "<graphics><default max_image_offset=\"12\"><path value=\"Walkers\\Group_118\"/></default></graphics></figure>";
constexpr const char *DISABLED_XML =
    "<figure type=\"homeless\" disabled=\"true\"></figure>";
constexpr const char *OVERLAY_XML_PREFIX =
    "<figure type=\"homeless\"><profiles default=\"legacy\"><profile id=\"legacy\">"
    "<native class=\"legacy_action\"/><owner slot=\"none\" building=\"any\" state=\"any\"/>"
    "<movement roam_ticks=\"1\" max_roam_length=\"800\" return_mode=\"none\"/>"
    "<pathing mode=\"vanilla_roaming\" terrain=\"roads_highway\"/></profile></profiles>"
    "<graphics><default max_image_offset=\"12\"><path value=\"Walkers\\Group_118\"/></default>";
constexpr const char *OVERLAY_XML_SUFFIX = "</graphics></figure>";
constexpr const char *STANDARD_XML_PREFIX =
    "<figure type=\"fort_standard\"><profiles default=\"legacy\"><profile id=\"legacy\">"
    "<native class=\"legacy_action\"/><owner slot=\"none\" building=\"any\" state=\"any\"/>"
    "<movement roam_ticks=\"1\" max_roam_length=\"800\" return_mode=\"none\"/>"
    "<pathing mode=\"vanilla_roaming\" terrain=\"roads_highway\"/></profile></profiles>"
    "<graphics><default max_image_offset=\"21\"><path value=\"Warriors\\Group_241\"/></default>";
constexpr const char *STANDARD_XML_SUFFIX = "</graphics></figure>";
constexpr const char *STATE_XML_PREFIX =
    "<figure type=\"prefect\"><profiles default=\"service\"><profile id=\"service\">"
    "<native class=\"prefect_service\"/><owner slot=\"primary\" building=\"prefecture\" state=\"in_use\"/>"
    "<movement roam_ticks=\"1\" max_roam_length=\"640\" return_mode=\"return_to_owner_road\"/>"
    "<pathing mode=\"vanilla_roaming\" terrain=\"roads\"/></profile></profiles>"
    "<graphics><default max_image_offset=\"12\"><path value=\"Walkers\\Group_117\"/></default>";
constexpr const char *STATE_XML_SUFFIX = "</graphics></figure>";
constexpr const char *MAP_FLAG_XML_PREFIX =
    "<figure type=\"map_flag\"><profiles default=\"legacy\"><profile id=\"legacy\">"
    "<native class=\"legacy_action\"/><owner slot=\"none\" building=\"any\" state=\"any\"/>"
    "<movement roam_ticks=\"1\" max_roam_length=\"800\" return_mode=\"none\"/>"
    "<pathing mode=\"stand_still\" terrain=\"any\"/></profile></profiles>"
    "<graphics><default max_image_offset=\"16\"><path value=\"Walkers\\Group_117\"/></default>";
constexpr const char *MAP_FLAG_XML_SUFFIX = "</graphics></figure>";
constexpr const char *HIPPODROME_XML_PREFIX =
    "<figure type=\"hippodrome_horses\"><profiles default=\"legacy\"><profile id=\"legacy\">"
    "<native class=\"legacy_action\"/><owner slot=\"none\" building=\"any\" state=\"any\"/>"
    "<movement roam_ticks=\"1\" max_roam_length=\"800\" return_mode=\"none\"/>"
    "<pathing mode=\"cross_country\" terrain=\"any\"/></profile></profiles>"
    "<graphics><default max_image_offset=\"8\"><path value=\"Walkers\\Group_217\"/></default>";
constexpr const char *HIPPODROME_XML_SUFFIX = "</graphics></figure>";
constexpr const char *MISSILE_LAUNCHER_XML_PREFIX =
    "<figure type=\"fort_javelin\" graphics_only=\"true\"><graphics>";
constexpr const char *MISSILE_LAUNCHER_XML_SUFFIX = "</graphics></figure>";
constexpr const char *RESOURCE_CART_XML_PREFIX =
    "<figure type=\"lighthouse_supplier\" graphics_only=\"true\"><graphics>";
constexpr const char *RESOURCE_CART_XML_SUFFIX = "</graphics></figure>";
constexpr const char *DIRECTIONAL_XML_PREFIX =
    "<figure type=\"fishing_boat\" graphics_only=\"true\"><graphics>";
constexpr const char *DIRECTIONAL_XML_SUFFIX = "</graphics></figure>";

figure_type_layer_test_input input(const char *xml, int layer, const char *mod, const char *path)
{
    return {xml, layer, mod, path};
}

bool valid(
    const figure_type_layer_test_input *inputs,
    int count,
    const char *query,
    figure_type_layer_test_result *result = nullptr)
{
    return figure_type_layered_definition_buffers_are_valid_for_test(inputs, count, query, result) != 0;
}

std::string replace_graphics(const char *figure_xml, const char *graphics_xml)
{
    std::string result = figure_xml ? figure_xml : "";
    const std::size_t begin = result.find("<graphics");
    const std::size_t close = result.find("</graphics>", begin);
    if (begin == std::string::npos || close == std::string::npos) return {};
    result.replace(begin, close + std::string("</graphics>").size() - begin, graphics_xml ? graphics_xml : "");
    return result;
}

bool overlay_definition_is_valid(const char *overlay, figure_type_layer_test_result *result = nullptr)
{
    const std::string xml = std::string(OVERLAY_XML_PREFIX) + overlay + OVERLAY_XML_SUFFIX;
    const figure_type_layer_test_input inputs[] = {
        input(xml.c_str(), 0, "Julius", "Julius/FigureType/homeless.xml")
    };
    return valid(inputs, 1, "homeless", result);
}

bool standard_definition_is_valid(const char *standard, figure_type_layer_test_result *result = nullptr)
{
    const std::string xml = std::string(STANDARD_XML_PREFIX) + standard + STANDARD_XML_SUFFIX;
    const figure_type_layer_test_input inputs[] = {
        input(xml.c_str(), 0, "Julius", "Julius/FigureType/fort_standard.xml")
    };
    return valid(inputs, 1, "fort_standard", result);
}

bool state_definition_is_valid(const char *states, figure_type_layer_test_result *result = nullptr)
{
    const std::string xml = std::string(STATE_XML_PREFIX) + states + STATE_XML_SUFFIX;
    const figure_type_layer_test_input inputs[] = {
        input(xml.c_str(), 0, "Julius", "Julius/FigureType/prefect.xml")
    };
    return valid(inputs, 1, "prefect", result);
}

bool map_flag_definition_is_valid(const char *policy, figure_type_layer_test_result *result = nullptr)
{
    const std::string xml = std::string(MAP_FLAG_XML_PREFIX) + policy + MAP_FLAG_XML_SUFFIX;
    const figure_type_layer_test_input inputs[] = {
        input(xml.c_str(), 0, "Julius", "Julius/FigureType/map_flag.xml")
    };
    return valid(inputs, 1, "map_flag", result);
}

bool hippodrome_definition_is_valid(const char *policy, figure_type_layer_test_result *result = nullptr)
{
    const std::string xml = std::string(HIPPODROME_XML_PREFIX) + policy + HIPPODROME_XML_SUFFIX;
    const figure_type_layer_test_input inputs[] = {
        input(xml.c_str(), 0, "Julius", "Julius/FigureType/hippodrome_horses.xml")
    };
    return valid(inputs, 1, "hippodrome_horses", result);
}

bool missile_launcher_definition_is_valid(
    const char *policy,
    figure_type_layer_test_result *result = nullptr)
{
    const std::string xml =
        std::string(MISSILE_LAUNCHER_XML_PREFIX) + policy + MISSILE_LAUNCHER_XML_SUFFIX;
    const figure_type_layer_test_input inputs[] = {
        input(xml.c_str(), 0, "Julius", "Julius/FigureType/fort_javelin.xml")
    };
    return valid(inputs, 1, "fort_javelin", result);
}

bool resource_cart_definition_is_valid(
    const char *policy,
    figure_type_layer_test_result *result = nullptr)
{
    const std::string xml = std::string(RESOURCE_CART_XML_PREFIX) + policy + RESOURCE_CART_XML_SUFFIX;
    const figure_type_layer_test_input inputs[] = {
        input(xml.c_str(), 0, "Julius", "Julius/FigureType/lighthouse_supplier.xml")
    };
    return valid(inputs, 1, "lighthouse_supplier", result);
}

bool directional_definition_is_valid(
    const char *policy,
    figure_type_layer_test_result *result = nullptr)
{
    const std::string xml = std::string(DIRECTIONAL_XML_PREFIX) + policy + DIRECTIONAL_XML_SUFFIX;
    const figure_type_layer_test_input inputs[] = {
        input(xml.c_str(), 0, "Julius", "Julius/FigureType/fishing_boat.xml")
    };
    return valid(inputs, 1, "fishing_boat", result);
}

} // namespace

bool validate_figure_type_registry_layering_contract(std::ostream &errors)
{
    RuntimeDrawSlice footprint = {};
    footprint.handle = 1;
    footprint.width = 10;
    footprint.height = 12;
    footprint.draw_offset_x = 3;
    footprint.draw_offset_y = 4;
    RuntimeDrawSlice animation_frame = footprint;
    animation_frame.draw_offset_x = 7;
    animation_frame.draw_offset_y = 9;
    Animation animation;
    animation.set_sprite_offset(20, 28);
    animation.add_frame(animation_frame);
    ImageGroupEntry entry("walker");
    entry.set_base_slice(footprint, 0, 0);
    entry.set_sprite_offset(20, 28);
    entry.set_animation(animation);
    figure_type_registry_impl::GraphicsTargetBinding animated_binding;
    animated_binding.entry = &entry;
    animated_binding.animation = &entry.animation();
    animated_binding.frame = 1;
    const RuntimeDrawSlice resolved_animation_frame = animated_binding.resolved_slice();
    if (resolved_animation_frame.draw_offset_x != 7 || resolved_animation_frame.draw_offset_y != 9) {
        errors << "Figure graphics applied the placement anchor twice to an animation frame.\n";
        return false;
    }
    animated_binding.frame_selects_entry = 1;
    const RuntimeDrawSlice resolved_discrete_frame = animated_binding.resolved_slice();
    if (animated_binding.uses_animation() || resolved_discrete_frame.draw_offset_x != 3 || resolved_discrete_frame.draw_offset_y != 4) {
        errors << "Figure graphics treated a discrete per-frame image entry as a nested animation.\n";
        return false;
    }
    figure_type_registry_impl::FigureGraphics pattern_graphics;
    pattern_graphics.asset_target(GraphicsTargetRole::Default).set_path("Walkers\\Group_107");
    if (!pattern_graphics.target_binding(GraphicsTargetRole::Default, 0, 1).frame_selects_entry) {
        errors << "Figure graphics did not recognize a legacy per-frame entry pattern.\n";
        return false;
    }
    pattern_graphics.asset_target(GraphicsTargetRole::Default).set_path("Walkers\\walker_{dir}");
    if (pattern_graphics.target_binding(GraphicsTargetRole::Default, 0, 1).frame_selects_entry) {
        errors << "Figure graphics disabled a consolidated directional animation.\n";
        return false;
    }

    struct CorpseFrameCase {
        int wait_ticks;
        int expected_frame;
    };
    constexpr CorpseFrameCase CORPSE_FRAME_CASES[] = {
        {-1, 0}, {0, 0}, {1, 0}, {2, 1}, {3, 1}, {4, 2}, {5, 2}, {6, 3}, {7, 3},
        {8, 4}, {31, 4}, {32, 5}, {71, 5}, {72, 6}, {151, 6}, {152, 7}, {255, 7}, {256, 7}
    };
    for (const CorpseFrameCase &test_case : CORPSE_FRAME_CASES) {
        if (figure_type_registry_impl::FigureGraphics::corpse_frame_for_wait_ticks(test_case.wait_ticks) !=
            test_case.expected_frame) {
            errors << "FigureGraphics corpse timing lost an exact legacy frame boundary.\n";
            return false;
        }
    }

    const figure_type_layer_test_input replacement[] = {
        input(LOWER_XML, 0, "Julius", "Julius/FigureType/homeless.xml"),
        input(UPPER_XML, 3, "SparsePatch", "SparsePatch/FigureType/homeless.xml")
    };
    figure_type_layer_test_result result;
    if (!valid(replacement, 2, "homeless", &result) ||
        result.active_count != 1 || result.suppressed_count != 0 || result.queried_disabled ||
        result.queried_max_roam_length != 999 || result.queried_source_layer != 3) {
        errors << "FigureType upper-layer replacement lost winner data or provenance.\n";
        return false;
    }

    const figure_type_layer_test_input inherited_only[] = {
        input(LOWER_XML, 0, "Julius", "Julius/FigureType/homeless.xml")
    };
    if (!valid(inherited_only, 1, "homeless", &result) ||
        result.queried_max_roam_length != 800 || result.queried_source_layer != 0) {
        errors << "FigureType lower definition was not inherited when the upper layer was absent.\n";
        return false;
    }

    constexpr const char *PROPORTIONAL_LOGICAL_SIZE_XML =
        "<figure type=\"homeless\"><profiles default=\"legacy\"><profile id=\"legacy\"><native class=\"legacy_action\"/><owner slot=\"none\" building=\"any\" state=\"any\"/><movement roam_ticks=\"1\" max_roam_length=\"800\" return_mode=\"none\"/><pathing mode=\"vanilla_roaming\" terrain=\"roads_highway\"/></profile></profiles><graphics><default max_image_offset=\"12\" logical_units_per_source_pixel=\"60\"><path value=\"Walkers\\Group_118\"/></default></graphics></figure>";
    const figure_type_layer_test_input proportional_logical_size[] = { input(PROPORTIONAL_LOGICAL_SIZE_XML, 0, "Vespasian", "Vespasian/FigureType/homeless.xml") };
    if (valid(proportional_logical_size, 1, "homeless")) {
        errors << "FigureType accepted asset-owned proportional logical sizing.\n";
        return false;
    }
    constexpr const char *FIXED_LOGICAL_SIZE_XML =
        "<figure type=\"homeless\"><profiles default=\"legacy\"><profile id=\"legacy\"><native class=\"legacy_action\"/><owner slot=\"none\" building=\"any\" state=\"any\"/><movement roam_ticks=\"1\" max_roam_length=\"800\" return_mode=\"none\"/><pathing mode=\"vanilla_roaming\" terrain=\"roads_highway\"/></profile></profiles><graphics><default max_image_offset=\"12\" logical_width=\"120\" logical_height=\"120\"><path value=\"Walkers\\Group_118\"/></default></graphics></figure>";
    const figure_type_layer_test_input fixed_logical_size[] = { input(FIXED_LOGICAL_SIZE_XML, 0, "Vespasian", "Vespasian/FigureType/homeless.xml") };
    if (valid(fixed_logical_size, 1, "homeless")) {
        errors << "FigureType accepted asset-owned fixed logical dimensions.\n";
        return false;
    }
    constexpr const char *SPRITE_OFFSET_XML =
        "<figure type=\"homeless\"><profiles default=\"legacy\"><profile id=\"legacy\"><native class=\"legacy_action\"/><owner slot=\"none\" building=\"any\" state=\"any\"/><movement roam_ticks=\"1\" max_roam_length=\"800\" return_mode=\"none\"/><pathing mode=\"vanilla_roaming\" terrain=\"roads_highway\"/></profile></profiles><graphics><default max_image_offset=\"12\" sprite_offset_x=\"26\" sprite_offset_y=\"29\"><path value=\"Walkers\\Group_118\"/></default></graphics></figure>";
    const figure_type_layer_test_input sprite_offset[] = { input(SPRITE_OFFSET_XML, 0, "Vespasian", "Vespasian/FigureType/homeless.xml") };
    if (valid(sprite_offset, 1, "homeless")) {
        errors << "FigureType accepted asset-owned sprite offsets.\n";
        return false;
    }

    const std::string invalid_graphics_sources[] = {
        replace_graphics(LOWER_XML, "<graphics path_pattern=\"Walkers\\Group_118\" image_pattern=\"{legacy_entry}\"></graphics>"),
        replace_graphics(LOWER_XML, "<graphics runtime_selected_path=\"Walkers\\Group_117\" runtime_selected_source=\"Environment\\Group_234\"></graphics>"),
        replace_graphics(LOWER_XML, "<graphics><default><path value=\"Walkers\\Group_118.xml\"/></default></graphics>"),
        replace_graphics(LOWER_XML, "<graphics><default><path value=\"Walkers\\Group_118\"/><image value=\"Image_0000\"/></default></graphics>")
    };
    for (const std::string &xml : invalid_graphics_sources) {
        const figure_type_layer_test_input inputs[] = {
            input(xml.c_str(), 0, "Vespasian", "Vespasian/FigureType/homeless.xml")
        };
        if (valid(inputs, 1, "homeless")) {
            errors << "FigureType accepted an obsolete, file-qualified, or image-selecting graphics source.\n";
            return false;
        }
    }

    const figure_type_layer_test_input suppression[] = {
        input(LOWER_XML, 0, "Julius", "Julius/FigureType/homeless.xml"),
        input(DISABLED_XML, 1, "Pharaoh", "Pharaoh/FigureType/homeless.xml")
    };
    if (!valid(suppression, 2, "homeless", &result) ||
        result.active_count != 0 || result.suppressed_count != 1 || !result.queried_disabled ||
        result.queried_source_layer != 1) {
        errors << "FigureType disabled tombstone did not suppress its inherited identity.\n";
        return false;
    }

    const figure_type_layer_test_input duplicate[] = {
        input(LOWER_XML, 0, "Julius", "Julius/FigureType/homeless.xml"),
        input(UPPER_XML, 0, "Julius", "Julius/FigureType/homeless-copy.xml")
    };
    if (valid(duplicate, 2, "homeless")) {
        errors << "FigureType same-layer duplicate identity was accepted.\n";
        return false;
    }

    constexpr const char *INVALID_DISABLED_CONTENT =
        "<figure type=\"homeless\" disabled=\"true\"><profiles></profiles></figure>";
    const figure_type_layer_test_input invalid_tombstone[] = {
        input(INVALID_DISABLED_CONTENT, 0, "Pharaoh", "Pharaoh/FigureType/homeless.xml")
    };
    if (valid(invalid_tombstone, 1, "homeless")) {
        errors << "FigureType disabled tombstone accepted authored content.\n";
        return false;
    }

    constexpr const char *INCOMPLETE_LOWER = "<figure type=\"homeless\"></figure>";
    const figure_type_layer_test_input deferred_validation[] = {
        input(INCOMPLETE_LOWER, 0, "Julius", "Julius/FigureType/homeless.xml"),
        input(UPPER_XML, 1, "Pharaoh", "Pharaoh/FigureType/homeless.xml")
    };
    if (!valid(deferred_validation, 2, "homeless", &result) ||
        result.queried_max_roam_length != 999) {
        errors << "FigureType semantic validation ran before final winners were collected.\n";
        return false;
    }

    const figure_type_layer_test_input invalid_winner[] = {
        input(INCOMPLETE_LOWER, 0, "Pharaoh", "Pharaoh/FigureType/homeless.xml")
    };
    if (valid(invalid_winner, 1, "homeless")) {
        errors << "FigureType incomplete final winner was accepted.\n";
        return false;
    }

    const char *invalid_graphics_only_definitions[] = {
        "<figure type=\"fort_javelin\" graphics_only=\"maybe\"><graphics><missile_launcher cursor=\"attack_image_offset\" frame_divisor=\"2\" frames=\"0\" after_frame=\"0\"/></graphics></figure>",
        "<figure type=\"fort_javelin\" graphics_only=\"true\"></figure>",
        "<figure type=\"fort_javelin\" graphics_only=\"true\"><profiles default=\"legacy\"><profile id=\"legacy\"><native class=\"legacy_action\"/><owner slot=\"none\" building=\"any\" state=\"any\"/><movement roam_ticks=\"1\" max_roam_length=\"800\" return_mode=\"none\"/><pathing mode=\"cross_country\" terrain=\"any\"/></profile></profiles><graphics><missile_launcher cursor=\"attack_image_offset\" frame_divisor=\"2\" frames=\"0\" after_frame=\"0\"/></graphics></figure>",
        "<figure type=\"fort_javelin\" disabled=\"true\" graphics_only=\"true\"></figure>"
    };
    for (const char *xml : invalid_graphics_only_definitions) {
        const figure_type_layer_test_input inputs[] = {
            input(xml, 0, "Julius", "Julius/FigureType/fort_javelin.xml")
        };
        if (valid(inputs, 1, "fort_javelin")) {
            errors << "FigureType accepted a malformed graphics-only definition.\n";
            return false;
        }
    }

    constexpr const char *ANIMAL_OVERLAY =
        "<overlay role=\"animal\" path=\"Walkers\\Group_117\" direction=\"figure\" frame=\"figure\" "
        "direction_stride=\"8\" resource_base_offset=\"0\" resource_stride=\"0\" actions=\"any\" "
        "offsets_x=\"13,18,12,0,-13,-18,-13,0\" offsets_y=\"-7,-1,7,11,6,-1,-7,-12\" "
        "hide_on_corpse=\"true\"/>";
    if (!overlay_definition_is_valid(ANIMAL_OVERLAY, &result) ||
        result.queried_overlay_count != 1 ||
        !result.queried_overlay_has_path ||
        !result.queried_overlay_follows_direction ||
        !result.queried_overlay_follows_frame ||
        result.queried_overlay_action_count != 0 ||
        result.queried_overlay_resource_stride != 0 ||
        result.queried_overlay_direction_stride != 8 ||
        !result.queried_overlay_hide_on_corpse ||
        result.queried_overlay_sample_image_offset != 35 ||
        result.queried_overlay_sample_x_offset != 0 ||
        result.queried_overlay_sample_y_offset != 11) {
        errors << "FigureType authored animal overlay lost its direction, frame, offset, or corpse policy.\n";
        return false;
    }

    constexpr const char *RESOURCE_OVERLAY =
        "<overlay role=\"goods\" path=\"Walkers\\Group_117\" direction=\"figure\" frame=\"static\" "
        "direction_stride=\"8\" resource_base_offset=\"8\" resource_stride=\"8\" actions=\"immigrant_arriving\" "
        "offsets_x=\"13,18,12,0,-13,-18,-13,0\" offsets_y=\"-7,-1,7,11,6,-1,-7,-12\" "
        "hide_on_corpse=\"true\"/>";
    if (!overlay_definition_is_valid(RESOURCE_OVERLAY, &result) ||
        result.queried_overlay_count != 1 ||
        !result.queried_overlay_has_path ||
        !result.queried_overlay_follows_direction ||
        result.queried_overlay_follows_frame ||
        result.queried_overlay_action_count != 1 ||
        result.queried_overlay_resource_stride != 8 ||
        result.queried_overlay_direction_stride != 8 ||
        !result.queried_overlay_hide_on_corpse ||
        result.queried_overlay_sample_image_offset != 27 ||
        result.queried_overlay_sample_x_offset != 0 ||
        result.queried_overlay_sample_y_offset != 11) {
        errors << "FigureType authored resource overlay lost its action, resource stride, direction, or offset policy.\n";
        return false;
    }

    const char *invalid_overlays[] = {
        "<overlay/>",
        "<overlay role=\"animal\" path=\"\" direction=\"figure\" frame=\"figure\" direction_stride=\"8\" resource_base_offset=\"0\" resource_stride=\"0\" actions=\"any\" offsets_x=\"13,18,12,0,-13,-18,-13,0\" offsets_y=\"-7,-1,7,11,6,-1,-7,-12\" hide_on_corpse=\"true\"/>",
        "<overlay role=\"animal\" path=\"Walkers\\Group_117\" direction=\"diagonal\" frame=\"figure\" direction_stride=\"8\" resource_base_offset=\"0\" resource_stride=\"0\" actions=\"any\" offsets_x=\"13,18,12,0,-13,-18,-13,0\" offsets_y=\"-7,-1,7,11,6,-1,-7,-12\" hide_on_corpse=\"true\"/>",
        "<overlay role=\"animal\" path=\"Walkers\\Group_117\" direction=\"figure\" frame=\"loop\" direction_stride=\"8\" resource_base_offset=\"0\" resource_stride=\"0\" actions=\"any\" offsets_x=\"13,18,12,0,-13,-18,-13,0\" offsets_y=\"-7,-1,7,11,6,-1,-7,-12\" hide_on_corpse=\"true\"/>",
        "<overlay role=\"animal\" path=\"Walkers\\Group_117\" direction=\"figure\" frame=\"figure\" direction_stride=\"0\" resource_base_offset=\"0\" resource_stride=\"0\" actions=\"any\" offsets_x=\"13,18,12,0,-13,-18,-13,0\" offsets_y=\"-7,-1,7,11,6,-1,-7,-12\" hide_on_corpse=\"true\"/>",
        "<overlay role=\"animal\" path=\"Walkers\\Group_117\" direction=\"figure\" frame=\"figure\" direction_stride=\"8\" resource_base_offset=\"-1\" resource_stride=\"0\" actions=\"any\" offsets_x=\"13,18,12,0,-13,-18,-13,0\" offsets_y=\"-7,-1,7,11,6,-1,-7,-12\" hide_on_corpse=\"true\"/>",
        "<overlay role=\"animal\" path=\"Walkers\\Group_117\" direction=\"figure\" frame=\"figure\" direction_stride=\"8\" resource_base_offset=\"0\" resource_stride=\"-1\" actions=\"any\" offsets_x=\"13,18,12,0,-13,-18,-13,0\" offsets_y=\"-7,-1,7,11,6,-1,-7,-12\" hide_on_corpse=\"true\"/>",
        "<overlay role=\"animal\" path=\"Walkers\\Group_117\" direction=\"figure\" frame=\"figure\" direction_stride=\"8\" resource_base_offset=\"0\" resource_stride=\"0\" actions=\"missing\" offsets_x=\"13,18,12,0,-13,-18,-13,0\" offsets_y=\"-7,-1,7,11,6,-1,-7,-12\" hide_on_corpse=\"true\"/>",
        "<overlay role=\"animal\" path=\"Walkers\\Group_117\" direction=\"figure\" frame=\"figure\" direction_stride=\"8\" resource_base_offset=\"0\" resource_stride=\"0\" actions=\"2,2\" offsets_x=\"13,18,12,0,-13,-18,-13,0\" offsets_y=\"-7,-1,7,11,6,-1,-7,-12\" hide_on_corpse=\"true\"/>",
        "<overlay role=\"animal\" path=\"Walkers\\Group_117\" direction=\"figure\" frame=\"figure\" direction_stride=\"8\" resource_base_offset=\"0\" resource_stride=\"0\" actions=\"any\" offsets_x=\"13,18,12,0,-13,-18,-13\" offsets_y=\"-7,-1,7,11,6,-1,-7,-12\" hide_on_corpse=\"true\"/>",
        "<overlay role=\"animal\" path=\"Walkers\\Group_117\" direction=\"figure\" frame=\"figure\" direction_stride=\"8\" resource_base_offset=\"0\" resource_stride=\"0\" actions=\"any\" offsets_x=\"128,18,12,0,-13,-18,-13,0\" offsets_y=\"-7,-1,7,11,6,-1,-7,-12\" hide_on_corpse=\"true\"/>",
        "<overlay role=\"animal\" path=\"Walkers\\Group_117\" direction=\"figure\" frame=\"figure\" direction_stride=\"8\" resource_base_offset=\"0\" resource_stride=\"0\" actions=\"any\" offsets_x=\"13,18,12,0,-13,-18,-13,0\" offsets_y=\"-7,-1,7,11,6,-1,-7,-12\" hide_on_corpse=\"sometimes\"/>",
        "<overlay role=\"animal\" path=\"Walkers\\Group_117\" direction=\"figure\" frame=\"figure\" direction_stride=\"8\" resource_base_offset=\"0\" resource_stride=\"0\" actions=\"any\" offsets_x=\"13,18,12,0,-13,-18,-13,0\" offsets_y=\"-7,-1,7,11,6,-1,-7,-12\" hide_on_corpse=\"true\"/>"
        "<overlay role=\"animal\" path=\"Walkers\\Group_117\" direction=\"figure\" frame=\"figure\" direction_stride=\"8\" resource_base_offset=\"0\" resource_stride=\"0\" actions=\"any\" offsets_x=\"13,18,12,0,-13,-18,-13,0\" offsets_y=\"-7,-1,7,11,6,-1,-7,-12\" hide_on_corpse=\"true\"/>"
    };
    for (const char *invalid_overlay : invalid_overlays) {
        if (overlay_definition_is_valid(invalid_overlay)) {
            errors << "FigureType graphics accepted an invalid or duplicate overlay contract.\n";
            return false;
        }
    }

    constexpr const char *STANDARD_GRAPHICS =
        "<standard moving_frame_divisor=\"2\">"
        "<flag unit=\"fort_legionary\" path=\"Walkers\\Group_117\" moving_base=\"0\" halted_frame=\"8\"/>"
        "<flag unit=\"fort_javelin\" path=\"Walkers\\Group_117\" moving_base=\"9\" halted_frame=\"17\"/>"
        "<flag unit=\"fort_mounted\" path=\"Walkers\\Group_117\" moving_base=\"18\" halted_frame=\"26\"/>"
        "</standard>";
    if (!standard_definition_is_valid(STANDARD_GRAPHICS, &result) ||
        !result.queried_standard_enabled ||
        result.queried_standard_flag_count != 3 ||
        result.queried_standard_moving_frame_divisor != 2 ||
        result.queried_standard_sample_moving_offset != 5 ||
        result.queried_standard_sample_halted_offset != 8) {
        errors << "FigureType fort-standard policy lost its frame divisor or ordered flag offsets.\n";
        return false;
    }

    const char *invalid_standards[] = {
        "<standard></standard>",
        "<standard moving_frame_divisor=\"0\"><flag unit=\"fort_legionary\" path=\"Walkers\\Group_117\" moving_base=\"0\" halted_frame=\"8\"/></standard>",
        "<standard moving_frame_divisor=\"2\"></standard>",
        "<standard moving_frame_divisor=\"2\"><flag unit=\"missing\" path=\"Walkers\\Group_117\" moving_base=\"0\" halted_frame=\"8\"/></standard>",
        "<standard moving_frame_divisor=\"2\"><flag unit=\"fort_legionary\" path=\"\" moving_base=\"0\" halted_frame=\"8\"/></standard>",
        "<standard moving_frame_divisor=\"2\"><flag unit=\"fort_legionary\" path=\"Walkers\\Group_117\" moving_base=\"-1\" halted_frame=\"8\"/></standard>",
        "<standard moving_frame_divisor=\"2\"><flag unit=\"fort_legionary\" path=\"Walkers\\Group_117\" moving_base=\"0\" halted_frame=\"8\"/>"
        "<flag unit=\"fort_legionary\" path=\"Walkers\\Group_117\" moving_base=\"0\" halted_frame=\"8\"/></standard>"
    };
    for (const char *invalid_standard : invalid_standards) {
        if (standard_definition_is_valid(invalid_standard)) {
            errors << "FigureType graphics accepted an invalid or duplicate fort-standard flag contract.\n";
            return false;
        }
    }
    if (overlay_definition_is_valid(STANDARD_GRAPHICS)) {
        errors << "FigureType graphics accepted fort-standard policy on a non-standard figure.\n";
        return false;
    }

    constexpr const char *MISSILE_LAUNCHER_GRAPHICS =
        "<missile_launcher cursor=\"attack_image_offset\" frame_divisor=\"2\" "
        "frames=\"0,1,2,3,4,4,4,4,4,4,5,5,5,5,5,5\" after_frame=\"0\"/>";
    if (!missile_launcher_definition_is_valid(MISSILE_LAUNCHER_GRAPHICS, &result) ||
        !result.queried_missile_launcher_enabled ||
        !result.queried_missile_launcher_uses_attack_cursor ||
        result.queried_missile_launcher_uses_wait_cursor ||
        result.queried_missile_launcher_frame_divisor != 2 ||
        result.queried_missile_launcher_frame_count != 16 ||
        result.queried_missile_launcher_after_frame != 0 ||
        result.queried_missile_launcher_sample_start != 0 ||
        result.queried_missile_launcher_sample_middle != 5 ||
        result.queried_missile_launcher_sample_last != 5 ||
        result.queried_missile_launcher_sample_after != 0) {
        errors << "FigureType missile-launcher policy lost its exact attack-cursor schedule.\n";
        return false;
    }

    constexpr const char *LEGION_MISSILE_LAUNCHER_GRAPHICS =
        "<missile_launcher cursor=\"attack_image_offset\" frame_divisor=\"2\" "
        "frames=\"-1,0,0,1,1,2,2,3,3,4,4,4,4,4,4,-1\" after_frame=\"-1\"/>";
    if (!missile_launcher_definition_is_valid(LEGION_MISSILE_LAUNCHER_GRAPHICS, &result) ||
        result.queried_missile_launcher_sample_start != -1 ||
        result.queried_missile_launcher_sample_middle != 4 ||
        result.queried_missile_launcher_sample_last != -1 ||
        result.queried_missile_launcher_sample_after != -1) {
        errors << "FigureType missile-launcher policy lost the legion ready-pose sentinel schedule.\n";
        return false;
    }

    constexpr const char *WAIT_TICK_MISSILE_LAUNCHER_GRAPHICS =
        "<missile_launcher cursor=\"missile_wait_ticks\" frame_divisor=\"4\" "
        "frames=\"0,1,2,3,4,5,6,0\" after_frame=\"0\"/>";
    if (!missile_launcher_definition_is_valid(WAIT_TICK_MISSILE_LAUNCHER_GRAPHICS, &result) ||
        result.queried_missile_launcher_uses_attack_cursor ||
        !result.queried_missile_launcher_uses_wait_cursor ||
        result.queried_missile_launcher_frame_divisor != 4 ||
        result.queried_missile_launcher_sample_middle != 5 ||
        result.queried_missile_launcher_sample_after != 0) {
        errors << "FigureType missile-launcher policy lost its wait-tick cursor selection.\n";
        return false;
    }

    const char *invalid_missile_launchers[] = {
        "<missile_launcher/>",
        "<missile_launcher cursor=\"unknown\" frame_divisor=\"2\" frames=\"0,1\" after_frame=\"0\"/>",
        "<missile_launcher cursor=\"attack_image_offset\" frame_divisor=\"0\" frames=\"0,1\" after_frame=\"0\"/>",
        "<missile_launcher cursor=\"attack_image_offset\" frame_divisor=\"2x\" frames=\"0,1\" after_frame=\"0\"/>",
        "<missile_launcher cursor=\"attack_image_offset\" frame_divisor=\"2\" frames=\"\" after_frame=\"0\"/>",
        "<missile_launcher cursor=\"attack_image_offset\" frame_divisor=\"2\" frames=\"0,x\" after_frame=\"0\"/>",
        "<missile_launcher cursor=\"attack_image_offset\" frame_divisor=\"2\" frames=\"0,256\" after_frame=\"0\"/>",
        "<missile_launcher cursor=\"attack_image_offset\" frame_divisor=\"2\" frames=\"0,1\" after_frame=\"-2\"/>",
        "<missile_launcher cursor=\"attack_image_offset\" frame_divisor=\"2\" frames=\"0,1\" after_frame=\"x\"/>",
        "<missile_launcher cursor=\"attack_image_offset\" frame_divisor=\"2\" frames=\"0,1\" after_frame=\"0\"/>"
        "<missile_launcher cursor=\"attack_image_offset\" frame_divisor=\"2\" frames=\"0,1\" after_frame=\"0\"/>"
    };
    for (const char *invalid_launcher : invalid_missile_launchers) {
        if (missile_launcher_definition_is_valid(invalid_launcher)) {
            errors << "FigureType graphics accepted an invalid or duplicate missile-launcher contract.\n";
            return false;
        }
    }

    constexpr const char *RESOURCE_CART_GRAPHICS =
        "<resource_cart empty_path=\"Walkers\\Group_117\" "
        "resource_source=\"collecting_item_on_action\" resource_action=\"supplier_returning\" "
        "load_mode=\"fixed_one\" state_source=\"action\" suppress_body_when_hidden=\"false\" "
        "lift_food_at_loads=\"0\" lift_food_y_adjust=\"0\" "
        "offsets_x=\"13,18,12,0,-13,-18,-13,0\" "
        "offsets_y=\"-7,-1,7,11,6,-1,-7,-12\" hide_on_corpse=\"true\"/>";
    if (!resource_cart_definition_is_valid(RESOURCE_CART_GRAPHICS, &result) ||
        !result.queried_resource_cart_enabled ||
        !result.queried_resource_cart_empty_has_path ||
        !result.queried_resource_cart_collecting_item_source ||
        result.queried_resource_cart_resource_action != 146 ||
        !result.queried_resource_cart_fixed_one_load ||
        result.queried_resource_cart_carried_or_one_load ||
        result.queried_resource_cart_runtime_state ||
        result.queried_resource_cart_suppress_body_when_hidden ||
        result.queried_resource_cart_lift_food_at_loads != 0 ||
        result.queried_resource_cart_lift_food_y_adjust != 0 ||
        !result.queried_resource_cart_hide_on_corpse ||
        result.queried_resource_cart_sample_x != 0 ||
        result.queried_resource_cart_sample_y != 11) {
        errors << "FigureType resource-cart policy lost its source, load, attachment, or corpse contract.\n";
        return false;
    }

    constexpr const char *RUNTIME_RESOURCE_CART_GRAPHICS =
        "<resource_cart empty_path=\"Walkers\\Group_117\" resource_source=\"resource_id\" "
        "resource_action=\"0\" load_mode=\"carried_or_one\" state_source=\"runtime_state\" "
        "suppress_body_when_hidden=\"true\" lift_food_at_loads=\"8\" lift_food_y_adjust=\"-40\" "
        "offsets_x=\"13,18,12,0,-13,-18,-13,0\" offsets_y=\"-7,-1,7,11,6,-1,-7,-12\" "
        "hide_on_corpse=\"true\"/>";
    if (!resource_cart_definition_is_valid(RUNTIME_RESOURCE_CART_GRAPHICS, &result) ||
        result.queried_resource_cart_collecting_item_source ||
        result.queried_resource_cart_fixed_one_load ||
        !result.queried_resource_cart_carried_or_one_load ||
        !result.queried_resource_cart_runtime_state ||
        !result.queried_resource_cart_suppress_body_when_hidden ||
        result.queried_resource_cart_lift_food_at_loads != 8 ||
        result.queried_resource_cart_lift_food_y_adjust != -40) {
        errors << "FigureType runtime resource-cart policy lost its state, carried-load, or hidden-body contract.\n";
        return false;
    }

    figure_type_registry_impl::FigureResourceCartGraphics pure_policy;
    constexpr resource_type PRESENT_RESOURCE = RESOURCE_NONE + 1;
    if (pure_policy.normalize_presentation(
            figure_type_registry_impl::FigureResourceCartPresentation::Resource, RESOURCE_NONE) !=
            figure_type_registry_impl::FigureResourceCartPresentation::Empty ||
        pure_policy.normalize_presentation(
            figure_type_registry_impl::FigureResourceCartPresentation::Resource, PRESENT_RESOURCE) !=
            figure_type_registry_impl::FigureResourceCartPresentation::Resource ||
        pure_policy.normalize_presentation(
            figure_type_registry_impl::FigureResourceCartPresentation::Hidden, PRESENT_RESOURCE) !=
            figure_type_registry_impl::FigureResourceCartPresentation::Hidden) {
        errors << "FigureGraphics resource-cart state accepted an invalid resource presentation.\n";
        return false;
    }

    const char *invalid_resource_carts[] = {
        "<resource_cart/>",
        "<resource_cart empty_path=\"\" resource_source=\"resource_id\" resource_action=\"0\" load_mode=\"carried_or_one\" state_source=\"runtime_state\" suppress_body_when_hidden=\"false\" lift_food_at_loads=\"8\" lift_food_y_adjust=\"-40\" offsets_x=\"13,18,12,0,-13,-18,-13,0\" offsets_y=\"-7,-1,7,11,6,-1,-7,-12\" hide_on_corpse=\"true\"/>",
        "<resource_cart empty_path=\"Walkers\\Group_117\" resource_source=\"unknown\" resource_action=\"0\" load_mode=\"carried_or_one\" state_source=\"runtime_state\" suppress_body_when_hidden=\"false\" lift_food_at_loads=\"8\" lift_food_y_adjust=\"-40\" offsets_x=\"13,18,12,0,-13,-18,-13,0\" offsets_y=\"-7,-1,7,11,6,-1,-7,-12\" hide_on_corpse=\"true\"/>",
        "<resource_cart empty_path=\"Walkers\\Group_117\" resource_source=\"resource_id\" resource_action=\"x\" load_mode=\"carried_or_one\" state_source=\"runtime_state\" suppress_body_when_hidden=\"false\" lift_food_at_loads=\"8\" lift_food_y_adjust=\"-40\" offsets_x=\"13,18,12,0,-13,-18,-13,0\" offsets_y=\"-7,-1,7,11,6,-1,-7,-12\" hide_on_corpse=\"true\"/>",
        "<resource_cart empty_path=\"Walkers\\Group_117\" resource_source=\"resource_id\" resource_action=\"0\" load_mode=\"unknown\" state_source=\"runtime_state\" suppress_body_when_hidden=\"false\" lift_food_at_loads=\"8\" lift_food_y_adjust=\"-40\" offsets_x=\"13,18,12,0,-13,-18,-13,0\" offsets_y=\"-7,-1,7,11,6,-1,-7,-12\" hide_on_corpse=\"true\"/>",
        "<resource_cart empty_path=\"Walkers\\Group_117\" resource_source=\"resource_id\" resource_action=\"0\" load_mode=\"carried_or_one\" state_source=\"unknown\" suppress_body_when_hidden=\"false\" lift_food_at_loads=\"8\" lift_food_y_adjust=\"-40\" offsets_x=\"13,18,12,0,-13,-18,-13,0\" offsets_y=\"-7,-1,7,11,6,-1,-7,-12\" hide_on_corpse=\"true\"/>",
        "<resource_cart empty_path=\"Walkers\\Group_117\" resource_source=\"resource_id\" resource_action=\"0\" load_mode=\"carried_or_one\" state_source=\"runtime_state\" suppress_body_when_hidden=\"sometimes\" lift_food_at_loads=\"8\" lift_food_y_adjust=\"-40\" offsets_x=\"13,18,12,0,-13,-18,-13,0\" offsets_y=\"-7,-1,7,11,6,-1,-7,-12\" hide_on_corpse=\"true\"/>",
        "<resource_cart empty_path=\"Walkers\\Group_117\" resource_source=\"collecting_item_on_action\" resource_action=\"146\" load_mode=\"fixed_one\" state_source=\"runtime_state\" suppress_body_when_hidden=\"false\" lift_food_at_loads=\"0\" lift_food_y_adjust=\"0\" offsets_x=\"13,18,12,0,-13,-18,-13,0\" offsets_y=\"-7,-1,7,11,6,-1,-7,-12\" hide_on_corpse=\"true\"/>",
        "<resource_cart empty_path=\"Walkers\\Group_117\" resource_source=\"collecting_item_on_action\" resource_action=\"146\" load_mode=\"fixed_one\" state_source=\"action\" suppress_body_when_hidden=\"true\" lift_food_at_loads=\"0\" lift_food_y_adjust=\"0\" offsets_x=\"13,18,12,0,-13,-18,-13,0\" offsets_y=\"-7,-1,7,11,6,-1,-7,-12\" hide_on_corpse=\"true\"/>",
        "<resource_cart empty_path=\"Walkers\\Group_117\" resource_source=\"resource_id\" resource_action=\"0\" load_mode=\"carried_or_one\" state_source=\"runtime_state\" suppress_body_when_hidden=\"false\" lift_food_at_loads=\"-1\" lift_food_y_adjust=\"0\" offsets_x=\"13,18,12,0,-13,-18,-13,0\" offsets_y=\"-7,-1,7,11,6,-1,-7,-12\" hide_on_corpse=\"true\"/>",
        "<resource_cart empty_path=\"Walkers\\Group_117\" resource_source=\"resource_id\" resource_action=\"0\" load_mode=\"carried_or_one\" state_source=\"runtime_state\" suppress_body_when_hidden=\"false\" lift_food_at_loads=\"0\" lift_food_y_adjust=\"-40\" offsets_x=\"13,18,12,0,-13,-18,-13,0\" offsets_y=\"-7,-1,7,11,6,-1,-7,-12\" hide_on_corpse=\"true\"/>",
        "<resource_cart empty_path=\"Walkers\\Group_117\" resource_source=\"resource_id\" resource_action=\"0\" load_mode=\"carried_or_one\" state_source=\"runtime_state\" suppress_body_when_hidden=\"false\" lift_food_at_loads=\"8\" lift_food_y_adjust=\"0\" offsets_x=\"13,18,12,0,-13,-18,-13,0\" offsets_y=\"-7,-1,7,11,6,-1,-7,-12\" hide_on_corpse=\"true\"/>",
        "<resource_cart empty_path=\"Walkers\\Group_117\" resource_source=\"resource_id\" resource_action=\"0\" load_mode=\"carried_or_one\" state_source=\"runtime_state\" suppress_body_when_hidden=\"false\" lift_food_at_loads=\"8\" lift_food_y_adjust=\"-40\" offsets_x=\"13,18,12,0,-13,-18,-13\" offsets_y=\"-7,-1,7,11,6,-1,-7,-12\" hide_on_corpse=\"true\"/>",
        "<resource_cart empty_path=\"Walkers\\Group_117\" resource_source=\"resource_id\" resource_action=\"0\" load_mode=\"carried_or_one\" state_source=\"runtime_state\" suppress_body_when_hidden=\"false\" lift_food_at_loads=\"8\" lift_food_y_adjust=\"-40\" offsets_x=\"128,18,12,0,-13,-18,-13,0\" offsets_y=\"-7,-1,7,11,6,-1,-7,-12\" hide_on_corpse=\"true\"/>",
        "<resource_cart empty_path=\"Walkers\\Group_117\" resource_source=\"resource_id\" resource_action=\"0\" load_mode=\"carried_or_one\" state_source=\"runtime_state\" suppress_body_when_hidden=\"false\" lift_food_at_loads=\"8\" lift_food_y_adjust=\"-40\" offsets_x=\"13,18,12,0,-13,-18,-13,0\" offsets_y=\"-7,-1,7,11,6,-1,-7,-12\" hide_on_corpse=\"sometimes\"/>",
        "<resource_cart empty_path=\"Walkers\\Group_117\" resource_source=\"resource_id\" resource_action=\"0\" load_mode=\"carried_or_one\" state_source=\"runtime_state\" suppress_body_when_hidden=\"false\" lift_food_at_loads=\"8\" lift_food_y_adjust=\"-40\" offsets_x=\"13,18,12,0,-13,-18,-13,0\" offsets_y=\"-7,-1,7,11,6,-1,-7,-12\" hide_on_corpse=\"true\"/>"
        "<resource_cart empty_path=\"Walkers\\Group_117\" resource_source=\"resource_id\" resource_action=\"0\" load_mode=\"carried_or_one\" state_source=\"runtime_state\" suppress_body_when_hidden=\"false\" lift_food_at_loads=\"8\" lift_food_y_adjust=\"-40\" offsets_x=\"13,18,12,0,-13,-18,-13,0\" offsets_y=\"-7,-1,7,11,6,-1,-7,-12\" hide_on_corpse=\"true\"/>"
    };
    for (const char *invalid_cart : invalid_resource_carts) {
        if (resource_cart_definition_is_valid(invalid_cart)) {
            errors << "FigureType graphics accepted an invalid or duplicate resource-cart contract.\n";
            return false;
        }
    }

    constexpr const char *DIRECTIONAL_GRAPHICS =
        "<directional path=\"Walkers\\Group_117\" default_base_offset=\"8\" view_adjustments=\"1\" "
        "frame_divisor=\"1\" frame_stride=\"0\">"
        "<pose role=\"fishing\" action=\"fishing_boat_fishing\" base_offset=\"16\"/>"
        "</directional>";
    if (!directional_definition_is_valid(DIRECTIONAL_GRAPHICS, &result) ||
        !result.queried_directional_enabled ||
        !result.queried_directional_has_path ||
        result.queried_directional_pose_count != 1 ||
        result.queried_directional_default_sample_offset != 12 ||
        result.queried_directional_pose_sample_offset != 20) {
        errors << "FigureType directional policy lost its default or fishing action pose.\n";
        return false;
    }

    figure_type_registry_impl::FigureDirectionalGraphics pure_directional;
    pure_directional.enabled = 1;
    pure_directional.default_base_image_offset = 8;
    pure_directional.view_adjustments = 1;
    pure_directional.frame_divisor = 2;
    pure_directional.frame_stride = 8;
    pure_directional.poses.push_back({ "fishing", 192, 16 });
    if (pure_directional.image_offset_for(0, 6, 2, 5) != 28 ||
        pure_directional.image_offset_for(192, 6, 2, 5) != 36) {
        errors << "FigureGraphics directional pose arithmetic changed its view, frame, or action ordering.\n";
        return false;
    }

    const char *invalid_directional_graphics[] = {
        "<directional/>",
        "<directional path=\"\" default_base_offset=\"8\" view_adjustments=\"1\" frame_divisor=\"1\" frame_stride=\"0\"/>",
        "<directional path=\"Walkers\\Group_117\" default_base_offset=\"-1\" view_adjustments=\"1\" frame_divisor=\"1\" frame_stride=\"0\"/>",
        "<directional path=\"Walkers\\Group_117\" default_base_offset=\"8x\" view_adjustments=\"1\" frame_divisor=\"1\" frame_stride=\"0\"/>",
        "<directional path=\"Walkers\\Group_117\" default_base_offset=\"8\" view_adjustments=\"5\" frame_divisor=\"1\" frame_stride=\"0\"/>",
        "<directional path=\"Walkers\\Group_117\" default_base_offset=\"8\" view_adjustments=\"1\" frame_divisor=\"0\" frame_stride=\"0\"/>",
        "<directional path=\"Walkers\\Group_117\" default_base_offset=\"8\" view_adjustments=\"1\" frame_divisor=\"1\" frame_stride=\"-1\"/>",
        "<directional path=\"Walkers\\Group_117\" default_base_offset=\"8\" view_adjustments=\"1\" frame_divisor=\"1\" frame_stride=\"0\"><pose/></directional>",
        "<directional path=\"Walkers\\Group_117\" default_base_offset=\"8\" view_adjustments=\"1\" frame_divisor=\"1\" frame_stride=\"0\"><pose role=\"fishing\" action=\"missing\" base_offset=\"16\"/></directional>",
        "<directional path=\"Walkers\\Group_117\" default_base_offset=\"8\" view_adjustments=\"1\" frame_divisor=\"1\" frame_stride=\"0\"><pose role=\"\" action=\"fishing_boat_fishing\" base_offset=\"16\"/></directional>",
        "<directional path=\"Walkers\\Group_117\" default_base_offset=\"8\" view_adjustments=\"1\" frame_divisor=\"1\" frame_stride=\"0\"><pose role=\"fishing\" action=\"fishing_boat_fishing\" base_offset=\"-1\"/></directional>",
        "<directional path=\"Walkers\\Group_117\" default_base_offset=\"8\" view_adjustments=\"1\" frame_divisor=\"1\" frame_stride=\"0\"><pose role=\"fishing\" action=\"fishing_boat_fishing\" base_offset=\"16\"/><pose role=\"fishing\" action=\"prefect_at_fire\" base_offset=\"24\"/></directional>",
        "<directional path=\"Walkers\\Group_117\" default_base_offset=\"8\" view_adjustments=\"1\" frame_divisor=\"1\" frame_stride=\"0\"><pose role=\"fishing\" action=\"fishing_boat_fishing\" base_offset=\"16\"/><pose role=\"also_fishing\" action=\"fishing_boat_fishing\" base_offset=\"24\"/></directional>",
        "<directional path=\"Walkers\\Group_117\" default_base_offset=\"8\" view_adjustments=\"1\" frame_divisor=\"1\" frame_stride=\"0\"/><directional path=\"Walkers\\Group_117\" default_base_offset=\"8\" view_adjustments=\"1\" frame_divisor=\"1\" frame_stride=\"0\"/>"
    };
    for (const char *invalid_directional : invalid_directional_graphics) {
        if (directional_definition_is_valid(invalid_directional)) {
            errors << "FigureType graphics accepted an invalid or duplicate directional-pose contract.\n";
            return false;
        }
    }

    constexpr const char *PREFECT_BUCKET_STATES =
        "<state role=\"water_bucket\" action=\"prefect_going_to_fire\" path=\"Walkers\\Group_117\" "
        "base_offset=\"0\" direction=\"movement\" view_adjustments=\"2\" frame=\"image_offset\" "
        "frame_divisor=\"1\" direction_stride=\"8\"/>"
        "<state role=\"throwing_water\" action=\"prefect_at_fire\" path=\"Walkers\\Group_117\" "
        "base_offset=\"96\" direction=\"attack\" view_adjustments=\"2\" frame=\"image_offset\" "
        "frame_divisor=\"2\" direction_stride=\"8\"/>";
    if (!state_definition_is_valid(PREFECT_BUCKET_STATES, &result) ||
        result.queried_state_layer_count != 2 ||
        !result.queried_bucket_going_has_path ||
        result.queried_bucket_going_sample_offset != 25 ||
        !result.queried_bucket_at_fire_has_path ||
        result.queried_bucket_at_fire_sample_offset != 123) {
        errors << "FigureType prefect bucket states lost their exact direction, frame, or atlas-row policy.\n";
        return false;
    }

    constexpr const char *BALLISTA_GRAPHICS =
        "<figure type=\"ballista\" graphics_only=\"true\"><graphics>"
        "<missile_launcher cursor=\"missile_wait_ticks\" frame_divisor=\"4\" frames=\"0,1,2,3,4,5,6\" after_frame=\"0\"/>"
        "<state role=\"idle\" action=\"ballista_created\" path=\"Walkers\\Group_117\" base_offset=\"0\" direction=\"movement\" view_adjustments=\"1\" frame=\"static\" frame_divisor=\"1\" direction_stride=\"8\"/>"
        "<state role=\"firing\" action=\"ballista_firing\" path=\"Walkers\\Group_117\" base_offset=\"0\" direction=\"movement\" view_adjustments=\"1\" frame=\"missile_launcher\" frame_divisor=\"1\" direction_stride=\"8\"/>"
        "<state role=\"dead\" action=\"corpse\" path=\"Walkers\\Group_117\" base_offset=\"0\" direction=\"movement\" view_adjustments=\"1\" frame=\"static\" frame_divisor=\"1\" direction_stride=\"8\"/>"
        "</graphics></figure>";
    const figure_type_layer_test_input ballista_inputs[] = {
        input(BALLISTA_GRAPHICS, 0, "Julius", "Julius/FigureType/ballista.xml")
    };
    if (!valid(ballista_inputs, 1, "ballista", &result) ||
        result.queried_state_layer_count != 3 ||
        !result.queried_ballista_idle_has_path ||
        result.queried_ballista_idle_sample_offset != 3 ||
        !result.queried_ballista_firing_has_path ||
        !result.queried_ballista_firing_uses_missile_launcher ||
        result.queried_ballista_firing_sample_offset != 51 ||
        !result.queried_missile_launcher_enabled ||
        !result.queried_missile_launcher_uses_wait_cursor) {
        errors << "FigureType ballista states lost exact direction or missile-launcher frame policy.\n";
        return false;
    }

    const char *invalid_states[] = {
        "<state role=\"water_bucket\"/>",
        "<state role=\"water_bucket\" action=\"missing\" path=\"Walkers\\Group_117\" base_offset=\"0\" direction=\"movement\" view_adjustments=\"2\" frame=\"image_offset\" frame_divisor=\"1\" direction_stride=\"8\"/>",
        "<state role=\"water_bucket\" action=\"prefect_going_to_fire\" path=\"\" base_offset=\"0\" direction=\"movement\" view_adjustments=\"2\" frame=\"image_offset\" frame_divisor=\"1\" direction_stride=\"8\"/>",
        "<state role=\"water_bucket\" action=\"prefect_going_to_fire\" path=\"Walkers\\Group_117\" base_offset=\"0\" direction=\"sideways\" view_adjustments=\"2\" frame=\"image_offset\" frame_divisor=\"1\" direction_stride=\"8\"/>",
        "<state role=\"water_bucket\" action=\"prefect_going_to_fire\" path=\"Walkers\\Group_117\" base_offset=\"0\" direction=\"movement\" view_adjustments=\"2\" frame=\"loop\" frame_divisor=\"1\" direction_stride=\"8\"/>",
        "<state role=\"water_bucket\" action=\"prefect_going_to_fire\" path=\"Walkers\\Group_117\" base_offset=\"0\" direction=\"movement\" view_adjustments=\"5\" frame=\"image_offset\" frame_divisor=\"1\" direction_stride=\"8\"/>",
        "<state role=\"water_bucket\" action=\"prefect_going_to_fire\" path=\"Walkers\\Group_117\" base_offset=\"0\" direction=\"movement\" view_adjustments=\"2\" frame=\"image_offset\" frame_divisor=\"0\" direction_stride=\"8\"/>",
        "<state role=\"water_bucket\" action=\"prefect_going_to_fire\" path=\"Walkers\\Group_117\" base_offset=\"0\" direction=\"movement\" view_adjustments=\"2\" frame=\"image_offset\" frame_divisor=\"1\" direction_stride=\"0\"/>",
        "<state role=\"water_bucket\" action=\"prefect_going_to_fire\" path=\"Walkers\\Group_117\" base_offset=\"0\" direction=\"movement\" view_adjustments=\"2\" frame=\"missile_launcher\" frame_divisor=\"1\" direction_stride=\"8\"/>",
        "<state role=\"water_bucket\" action=\"prefect_going_to_fire\" path=\"Walkers\\Group_117\" base_offset=\"0\" direction=\"movement\" view_adjustments=\"2\" frame=\"image_offset\" frame_divisor=\"1\" direction_stride=\"8\"/>"
        "<state role=\"water_bucket\" action=\"prefect_at_fire\" path=\"Walkers\\Group_117\" base_offset=\"96\" direction=\"attack\" view_adjustments=\"2\" frame=\"image_offset\" frame_divisor=\"2\" direction_stride=\"8\"/>"
    };
    for (const char *invalid_state : invalid_states) {
        if (state_definition_is_valid(invalid_state)) {
            errors << "FigureType graphics accepted an invalid or duplicate state-layer contract.\n";
            return false;
        }
    }

    constexpr const char *MAP_FLAG_GRAPHICS =
        "<map_flag resource_min=\"1\" resource_max_exclusive=\"26\" base_direction=\"0\" view_adjustments=\"1\" frame_divisor=\"2\" frame_stride=\"1\" number_x=\"6\" number_y=\"7\">"
        "<marker resource_min=\"1\" resource_max_exclusive=\"2\" path=\"Walkers\\Group_117\" image_offset=\"0\" number_base=\"0\"/>"
        "<marker resource_min=\"2\" resource_max_exclusive=\"3\" path=\"Walkers\\Group_117\" image_offset=\"2\" number_base=\"0\"/>"
        "<marker resource_min=\"3\" resource_max_exclusive=\"4\" path=\"Walkers\\Group_117\" image_offset=\"3\" number_base=\"0\"/>"
        "<marker resource_min=\"4\" resource_max_exclusive=\"12\" path=\"Walkers\\Group_117\" image_offset=\"1\" number_base=\"1\"/>"
        "<marker resource_min=\"12\" resource_max_exclusive=\"13\" path=\"Walkers\\Group_117\" image_offset=\"4\" number_base=\"0\"/>"
        "<marker resource_min=\"13\" resource_max_exclusive=\"14\" path=\"Walkers\\Group_117\" image_offset=\"5\" number_base=\"0\"/>"
        "<marker resource_min=\"14\" resource_max_exclusive=\"22\" path=\"Walkers\\Group_117\" image_offset=\"3\" number_base=\"1\"/>"
        "<marker resource_min=\"22\" resource_max_exclusive=\"26\" path=\"Walkers\\Group_117\" image_offset=\"4\" number_base=\"1\"/>"
        "</map_flag>";
    if (!map_flag_definition_is_valid(MAP_FLAG_GRAPHICS, &result) ||
        !result.queried_map_flag_enabled ||
        result.queried_map_flag_marker_count != 8 ||
        result.queried_map_flag_sample_base_offset != 9 ||
        !result.queried_map_flag_invasion_has_path ||
        result.queried_map_flag_invasion_image_offset != 1 ||
        result.queried_map_flag_invasion_number != 8 ||
        !result.queried_map_flag_fishing_has_path ||
        result.queried_map_flag_fishing_number != 8 ||
        result.queried_map_flag_herd_image_offset != 4 ||
        result.queried_map_flag_number_x != 6 ||
        result.queried_map_flag_number_y != 7) {
        errors << "FigureType map-flag policy lost its base animation, category icon, or number-overlay behavior.\n";
        return false;
    }

    const char *invalid_map_flags[] = {
        "<map_flag></map_flag>",
        "<map_flag resource_min=\"1\" resource_max_exclusive=\"26\" base_direction=\"8\" view_adjustments=\"1\" frame_divisor=\"2\" frame_stride=\"1\" number_x=\"6\" number_y=\"7\"><marker resource_min=\"1\" resource_max_exclusive=\"26\" path=\"Walkers\\Group_117\" image_offset=\"0\" number_base=\"0\"/></map_flag>",
        "<map_flag resource_min=\"1\" resource_max_exclusive=\"26\" base_direction=\"0\" view_adjustments=\"5\" frame_divisor=\"2\" frame_stride=\"1\" number_x=\"6\" number_y=\"7\"><marker resource_min=\"1\" resource_max_exclusive=\"26\" path=\"Walkers\\Group_117\" image_offset=\"0\" number_base=\"0\"/></map_flag>",
        "<map_flag resource_min=\"1\" resource_max_exclusive=\"26\" base_direction=\"0\" view_adjustments=\"1\" frame_divisor=\"0\" frame_stride=\"1\" number_x=\"6\" number_y=\"7\"><marker resource_min=\"1\" resource_max_exclusive=\"26\" path=\"Walkers\\Group_117\" image_offset=\"0\" number_base=\"0\"/></map_flag>",
        "<map_flag resource_min=\"1\" resource_max_exclusive=\"26\" base_direction=\"0\" view_adjustments=\"1\" frame_divisor=\"2\" frame_stride=\"1\" number_x=\"6\" number_y=\"7\"></map_flag>",
        "<map_flag resource_min=\"1\" resource_max_exclusive=\"26\" base_direction=\"0\" view_adjustments=\"1\" frame_divisor=\"2\" frame_stride=\"1\" number_x=\"6\" number_y=\"7\"><marker resource_min=\"1\" resource_max_exclusive=\"25\" path=\"Walkers\\Group_117\" image_offset=\"0\" number_base=\"0\"/></map_flag>",
        "<map_flag resource_min=\"1\" resource_max_exclusive=\"26\" base_direction=\"0\" view_adjustments=\"1\" frame_divisor=\"2\" frame_stride=\"1\" number_x=\"6\" number_y=\"7\"><marker resource_min=\"2\" resource_max_exclusive=\"2\" path=\"Walkers\\Group_117\" image_offset=\"0\" number_base=\"0\"/></map_flag>",
        "<map_flag resource_min=\"1\" resource_max_exclusive=\"26\" base_direction=\"0\" view_adjustments=\"1\" frame_divisor=\"2\" frame_stride=\"1\" number_x=\"6\" number_y=\"7\"><marker resource_min=\"1\" resource_max_exclusive=\"20\" path=\"Walkers\\Group_117\" image_offset=\"0\" number_base=\"0\"/><marker resource_min=\"10\" resource_max_exclusive=\"26\" path=\"Walkers\\Group_117\" image_offset=\"1\" number_base=\"0\"/></map_flag>"
    };
    for (const char *invalid_map_flag : invalid_map_flags) {
        if (map_flag_definition_is_valid(invalid_map_flag)) {
            errors << "FigureType graphics accepted an invalid or overlapping map-flag policy.\n";
            return false;
        }
    }
    if (overlay_definition_is_valid(MAP_FLAG_GRAPHICS)) {
        errors << "FigureType graphics accepted map-flag policy on a non-map-flag figure.\n";
        return false;
    }

    if (hippodrome_definition_is_valid("<hippodrome_race/>")) {
        errors << "FigureType graphics accepted the removed hippodrome-specific schema.\n";
        return false;
    }

    return true;
}
