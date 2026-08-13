#include "resource_layering_test.h"

#include "game/resource.h"

#include <ostream>

namespace {

constexpr const char *NONE_XML =
    "<resource id=\"none\" slot=\"0\" name_key=\"main_strings.23.0\"><model flags=\"special\"/></resource>";
constexpr const char *WHEAT_LOWER_XML =
    "<resource id=\"wheat\" slot=\"1\" name_key=\"main_strings.23.1\"><trade buy=\"1\" sell=\"2\"/></resource>";
constexpr const char *WHEAT_UPPER_XML =
    "<resource id=\"wheat\" slot=\"1\" name_key=\"main_strings.23.1\"><trade buy=\"7\" sell=\"9\"/></resource>";
constexpr const char *WHEAT_DISABLED_XML =
    "<resource id=\"wheat\" slot=\"1\" disabled=\"true\"></resource>";

resource_layer_test_input input(const char *xml, int layer, const char *mod, const char *path)
{
    return {xml, layer, mod, path};
}

bool valid(
    const resource_layer_test_input *inputs,
    int count,
    const char *query,
    resource_layer_test_result *result = nullptr)
{
    return resource_layered_definition_buffers_are_valid_for_test(inputs, count, query, result) != 0;
}

} // namespace

bool validate_resource_layering_contract(std::ostream &errors)
{
    const resource_layer_test_input replacement[] = {
        input(NONE_XML, 0, "Julius", "Julius/Resources/none.xml"),
        input(WHEAT_LOWER_XML, 0, "Julius", "Julius/Resources/wheat.xml"),
        input(WHEAT_UPPER_XML, 1, "Pharaoh", "Pharaoh/Resources/wheat.xml")
    };
    resource_layer_test_result result;
    if (!valid(replacement, 3, "wheat", &result) ||
        result.active_count != 2 || result.suppressed_count != 0 ||
        result.queried_slot != 1 || result.queried_disabled ||
        result.queried_buy != 7 || result.queried_sell != 9 || result.queried_source_layer != 1) {
        errors << "Resource upper-layer replacement did not preserve slot/id identity or winner data.\n";
        return false;
    }

    const resource_layer_test_input suppression[] = {
        input(NONE_XML, 0, "Julius", "Julius/Resources/none.xml"),
        input(WHEAT_LOWER_XML, 0, "Julius", "Julius/Resources/wheat.xml"),
        input(WHEAT_DISABLED_XML, 1, "Pharaoh", "Pharaoh/Resources/wheat.xml")
    };
    if (!valid(suppression, 3, "wheat", &result) ||
        result.active_count != 1 || result.suppressed_count != 1 ||
        result.queried_slot != 1 || !result.queried_disabled || result.queried_source_layer != 1) {
        errors << "Resource disabled tombstone did not suppress and retain provenance for the inherited definition.\n";
        return false;
    }

    const resource_layer_test_input same_layer_duplicate[] = {
        input(WHEAT_LOWER_XML, 0, "Julius", "Julius/Resources/wheat.xml"),
        input(WHEAT_UPPER_XML, 0, "Julius", "Julius/Resources/wheat-copy.xml")
    };
    if (valid(same_layer_duplicate, 2, "wheat")) {
        errors << "Resource same-layer duplicate was accepted.\n";
        return false;
    }

    constexpr const char *BARLEY_SAME_SLOT_XML =
        "<resource id=\"barley\" slot=\"1\" name_key=\"main_strings.23.1\"></resource>";
    const resource_layer_test_input slot_conflict[] = {
        input(WHEAT_LOWER_XML, 0, "Julius", "Julius/Resources/wheat.xml"),
        input(BARLEY_SAME_SLOT_XML, 1, "Pharaoh", "Pharaoh/Resources/barley.xml")
    };
    if (valid(slot_conflict, 2, "wheat")) {
        errors << "Resource upper layer moved a stable slot to a different id.\n";
        return false;
    }

    constexpr const char *WHEAT_DIFFERENT_SLOT_XML =
        "<resource id=\"wheat\" slot=\"2\" name_key=\"main_strings.23.1\"></resource>";
    const resource_layer_test_input id_conflict[] = {
        input(WHEAT_LOWER_XML, 0, "Julius", "Julius/Resources/wheat.xml"),
        input(WHEAT_DIFFERENT_SLOT_XML, 1, "Pharaoh", "Pharaoh/Resources/wheat.xml")
    };
    if (valid(id_conflict, 2, "wheat")) {
        errors << "Resource upper layer moved a stable id to a different slot.\n";
        return false;
    }

    constexpr const char *INVALID_DISABLED_CONTENT_XML =
        "<resource id=\"wheat\" slot=\"1\" disabled=\"true\"><model/></resource>";
    const resource_layer_test_input invalid_tombstone[] = {
        input(INVALID_DISABLED_CONTENT_XML, 0, "Pharaoh", "Pharaoh/Resources/wheat.xml")
    };
    if (valid(invalid_tombstone, 1, "wheat")) {
        errors << "Resource disabled tombstone accepted authored definition content.\n";
        return false;
    }

    constexpr const char *INVALID_LOWER_NAME_KEY_XML =
        "<resource id=\"wheat\" slot=\"1\" name_key=\"missing.translation.key\"></resource>";
    const resource_layer_test_input deferred_reference[] = {
        input(NONE_XML, 0, "Julius", "Julius/Resources/none.xml"),
        input(INVALID_LOWER_NAME_KEY_XML, 0, "Julius", "Julius/Resources/wheat.xml"),
        input(WHEAT_UPPER_XML, 1, "Pharaoh", "Pharaoh/Resources/wheat.xml")
    };
    if (!valid(deferred_reference, 3, "wheat", &result) || result.queried_buy != 7) {
        errors << "Resource references were resolved before final overlay winners were collected.\n";
        return false;
    }

    return true;
}
