#include "startup/startup_parser_abi.h"
#include "startup/startup_definition_loader.h"

#include <algorithm>
#include <cstring>

namespace startup_parser {
void install_graphics_validation_renderer();
#ifndef STARTUP_PARSER_TEST
void install_graphics_validation_renderer()
{
}
#endif
} // namespace startup_parser

namespace {

uint32_t copy_abi_text(char *destination, size_t capacity, const std::string &source)
{
    if (!destination || !capacity) {
        return static_cast<uint32_t>(source.size());
    }
    const size_t copied = std::min(capacity - 1, source.size());
    std::memcpy(destination, source.data(), copied);
    destination[copied] = '\0';
    return static_cast<uint32_t>(source.size());
}

void initialize_abi_result(startup_parser_result_v1 &result)
{
    std::memset(&result, 0, sizeof(result));
    result.struct_size = sizeof(result);
    result.abi_version = STARTUP_PARSER_ABI_VERSION;
}

} // namespace

extern "C" startup_parser_status_v1 startup_parser_run_v1(
    const startup_parser_request_v1 *request,
    startup_parser_result_v1 *result)
{
    if (!result || result->struct_size < sizeof(startup_parser_result_v1)) {
        return STARTUP_PARSER_STATUS_INVALID_ABI;
    }

    initialize_abi_result(*result);
    constexpr uint32_t known_flags = STARTUP_PARSER_LOAD_CONFIG |
        STARTUP_PARSER_LOAD_LOCALIZATION |
        STARTUP_PARSER_VALIDATE_MOD_LAYOUT |
        STARTUP_PARSER_PREPARE_GRAPHICS_VALIDATION;
    if (!request || request->struct_size < sizeof(startup_parser_request_v1) ||
        request->abi_version != STARTUP_PARSER_ABI_VERSION || request->reserved != 0 ||
        (request->flags & ~known_flags) != 0) {
        result->failure_step_length = copy_abi_text(
            result->failure_step, sizeof(result->failure_step), "startup parser ABI");
        result->failure_message_length = copy_abi_text(
            result->failure_message,
            sizeof(result->failure_message),
            "Startup parser request has an unsupported version or structure size.");
        return STARTUP_PARSER_STATUS_INVALID_ABI;
    }

    startup_definition_loader::Request internal_request;
    internal_request.load_config = (request->flags & STARTUP_PARSER_LOAD_CONFIG) != 0;
    internal_request.load_localization = (request->flags & STARTUP_PARSER_LOAD_LOCALIZATION) != 0;
    internal_request.validate_mod_layout = (request->flags & STARTUP_PARSER_VALIDATE_MOD_LAYOUT) != 0;
    internal_request.prepare_graphics_validation =
        (request->flags & STARTUP_PARSER_PREPARE_GRAPHICS_VALIDATION) != 0;

    const startup_definition_loader::Result internal_result =
        startup_definition_loader::load(internal_request);
    result->succeeded = internal_result.succeeded;
    result->resource_definitions = internal_result.resource_definitions;
    result->graphics_validation_prepared = internal_result.graphics_validation_prepared;
    result->failure_step_length = copy_abi_text(
        result->failure_step, sizeof(result->failure_step), internal_result.failure_step);
    result->failure_message_length = copy_abi_text(
        result->failure_message, sizeof(result->failure_message), internal_result.failure_message);

    if (request->on_step) {
        for (const startup_definition_loader::Step &internal_step : internal_result.steps) {
            startup_parser_step_v1 step = {};
            step.struct_size = sizeof(step);
            step.succeeded = internal_step.succeeded;
            step.label = internal_step.label.c_str();
            step.detail = internal_step.detail.c_str();
            request->on_step(request->callback_context, &step);
        }
    }
    return internal_result.succeeded ?
        STARTUP_PARSER_STATUS_SUCCEEDED :
        STARTUP_PARSER_STATUS_FAILED;
}

extern "C" startup_parser_status_v1 startup_parser_inspect_environment_v1(
    const startup_parser_environment_request_v1 *request,
    startup_parser_environment_result_v1 *result)
{
    if (!result || result->struct_size < sizeof(startup_parser_environment_result_v1)) {
        return STARTUP_PARSER_STATUS_INVALID_ABI;
    }

    std::memset(result, 0, sizeof(*result));
    result->struct_size = sizeof(*result);
    result->abi_version = STARTUP_PARSER_ABI_VERSION;
    if (!request || request->struct_size < sizeof(startup_parser_environment_request_v1) ||
        request->abi_version != STARTUP_PARSER_ABI_VERSION || request->reserved != 0 ||
        request->reserved_alignment != 0 ||
        (request->game_root_capacity && !request->game_root) ||
        (request->mod_path_capacity && !request->mod_path)) {
        return STARTUP_PARSER_STATUS_INVALID_ABI;
    }

    const startup_definition_loader::Environment environment =
        startup_definition_loader::inspect_environment();
    result->game_root_length = copy_abi_text(
        request->game_root, request->game_root_capacity, environment.game_root);
    result->mod_path_length = copy_abi_text(
        request->mod_path, request->mod_path_capacity, environment.mod_path);
    result->mod_count = static_cast<uint32_t>(environment.mod_stack.size());
    if (request->on_mod) {
        for (uint32_t index = 0; index < result->mod_count; ++index) {
            request->on_mod(
                request->callback_context,
                index,
                environment.mod_stack[index].c_str());
        }
    }
    return STARTUP_PARSER_STATUS_SUCCEEDED;
}
