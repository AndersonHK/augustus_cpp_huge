#include "storage_type_registry_layering_test.h"

#include "building/storage_type_registry.h"

#include <ostream>

namespace {

constexpr const char *LOWER_XML =
    "<storage_type role=\"output\"><accepts resource=\"wheat\"/><capacity amount=\"100\"/></storage_type>";
constexpr const char *UPPER_XML =
    "<storage_type role=\"output\"><accepts resource=\"wheat\"/><capacity amount=\"800\"/></storage_type>";
constexpr const char *DISABLED_XML = "<storage_type disabled=\"true\"></storage_type>";

storage_type_layer_test_input input(
    const char *xml,
    int layer,
    const char *mod,
    const char *definition_path,
    const char *source_path)
{
    return {xml, layer, mod, definition_path, source_path};
}

bool valid(
    const storage_type_layer_test_input *inputs,
    int count,
    const char *query,
    storage_type_layer_test_result *result = nullptr)
{
    return storage_type_layered_definition_buffers_are_valid_for_test(inputs, count, query, result) != 0;
}

} // namespace

bool validate_storage_type_registry_layering_contract(std::ostream &errors)
{
    const storage_type_layer_test_input replacement[] = {
        input(LOWER_XML, 0, "Julius", "wheat_output", "Julius/StorageType/wheat_output.xml"),
        input(UPPER_XML, 1, "Pharaoh", "wheat_output", "Pharaoh/StorageType/wheat_output.xml")
    };
    storage_type_layer_test_result result;
    if (!valid(replacement, 2, "wheat_output", &result) ||
        result.active_count != 1 || result.suppressed_count != 0 || result.queried_disabled ||
        result.queried_capacity != 800 || result.queried_resource_count != 1 ||
        result.queried_source_layer != 1) {
        errors << "StorageType upper-layer replacement lost winner data or provenance.\n";
        return false;
    }

    const storage_type_layer_test_input suppression[] = {
        input(LOWER_XML, 0, "Julius", "wheat_output", "Julius/StorageType/wheat_output.xml"),
        input(DISABLED_XML, 1, "Pharaoh", "wheat_output", "Pharaoh/StorageType/wheat_output.xml")
    };
    if (!valid(suppression, 2, "wheat_output", &result) ||
        result.active_count != 0 || result.suppressed_count != 1 || !result.queried_disabled ||
        result.queried_source_layer != 1) {
        errors << "StorageType disabled tombstone did not suppress its inherited path.\n";
        return false;
    }

    const storage_type_layer_test_input duplicate[] = {
        input(LOWER_XML, 0, "Julius", "wheat_output", "Julius/StorageType/wheat_output.xml"),
        input(UPPER_XML, 0, "Julius", "wheat_output", "Julius/StorageType/wheat_output-copy.xml")
    };
    if (valid(duplicate, 2, "wheat_output")) {
        errors << "StorageType same-layer duplicate path was accepted.\n";
        return false;
    }

    constexpr const char *INVALID_DISABLED_CONTENT =
        "<storage_type disabled=\"true\"><accepts resource=\"wheat\"/></storage_type>";
    const storage_type_layer_test_input invalid_tombstone[] = {
        input(INVALID_DISABLED_CONTENT, 0, "Pharaoh", "wheat_output", "Pharaoh/StorageType/wheat_output.xml")
    };
    if (valid(invalid_tombstone, 1, "wheat_output")) {
        errors << "StorageType disabled tombstone accepted authored content.\n";
        return false;
    }

    constexpr const char *INVALID_LOWER_REFERENCE =
        "<storage_type role=\"output\"><accepts resource=\"missing_resource\"/></storage_type>";
    const storage_type_layer_test_input deferred_reference[] = {
        input(INVALID_LOWER_REFERENCE, 0, "Julius", "wheat_output", "Julius/StorageType/wheat_output.xml"),
        input(UPPER_XML, 1, "Pharaoh", "wheat_output", "Pharaoh/StorageType/wheat_output.xml")
    };
    if (!valid(deferred_reference, 2, "wheat_output", &result) || result.queried_capacity != 800) {
        errors << "StorageType resource references were resolved before final winners were collected.\n";
        return false;
    }

    const storage_type_layer_test_input unresolved_winner[] = {
        input(INVALID_LOWER_REFERENCE, 0, "Pharaoh", "invalid_output", "Pharaoh/StorageType/invalid_output.xml")
    };
    if (valid(unresolved_winner, 1, "invalid_output")) {
        errors << "StorageType final winner accepted an unresolved resource reference.\n";
        return false;
    }

    return true;
}
