#pragma once

#include <stdint.h>

#if defined(_WIN32) && defined(STARTUP_PARSER_ABI_EXPORTS)
#define STARTUP_PARSER_ABI __declspec(dllexport)
#elif defined(_WIN32) && defined(STARTUP_PARSER_ABI_IMPORTS)
#define STARTUP_PARSER_ABI __declspec(dllimport)
#else
#define STARTUP_PARSER_ABI
#endif

#ifdef __cplusplus
extern "C" {
#endif

enum {
    STARTUP_PARSER_ABI_VERSION = 1,
    STARTUP_PARSER_FAILURE_STEP_CAPACITY = 64,
    STARTUP_PARSER_FAILURE_MESSAGE_CAPACITY = 1024
};

typedef enum startup_parser_status_v1 {
    STARTUP_PARSER_STATUS_INVALID_ABI = -1,
    STARTUP_PARSER_STATUS_FAILED = 0,
    STARTUP_PARSER_STATUS_SUCCEEDED = 1
} startup_parser_status_v1;

typedef enum startup_parser_request_flag_v1 {
    STARTUP_PARSER_LOAD_CONFIG = 1u << 0,
    STARTUP_PARSER_LOAD_LOCALIZATION = 1u << 1,
    STARTUP_PARSER_VALIDATE_MOD_LAYOUT = 1u << 2,
    STARTUP_PARSER_PREPARE_GRAPHICS_VALIDATION = 1u << 3
} startup_parser_request_flag_v1;

typedef struct startup_parser_step_v1 {
    uint32_t struct_size;
    int32_t succeeded;
    const char *label;
    const char *detail;
} startup_parser_step_v1;

typedef void (*startup_parser_step_callback_v1)(
    void *context,
    const startup_parser_step_v1 *step);

typedef struct startup_parser_request_v1 {
    uint32_t struct_size;
    uint32_t abi_version;
    uint32_t flags;
    uint32_t reserved;
    startup_parser_step_callback_v1 on_step;
    void *callback_context;
} startup_parser_request_v1;

typedef struct startup_parser_result_v1 {
    uint32_t struct_size;
    uint32_t abi_version;
    int32_t succeeded;
    int32_t resource_definitions;
    int32_t graphics_validation_prepared;
    int32_t reserved;
    uint32_t failure_step_length;
    uint32_t failure_message_length;
    char failure_step[STARTUP_PARSER_FAILURE_STEP_CAPACITY];
    char failure_message[STARTUP_PARSER_FAILURE_MESSAGE_CAPACITY];
} startup_parser_result_v1;

typedef void (*startup_parser_mod_callback_v1)(
    void *context,
    uint32_t index,
    const char *mod_name);

typedef struct startup_parser_environment_request_v1 {
    uint32_t struct_size;
    uint32_t abi_version;
    uint32_t reserved;
    uint32_t game_root_capacity;
    char *game_root;
    uint32_t mod_path_capacity;
    uint32_t reserved_alignment;
    char *mod_path;
    startup_parser_mod_callback_v1 on_mod;
    void *callback_context;
} startup_parser_environment_request_v1;

typedef struct startup_parser_environment_result_v1 {
    uint32_t struct_size;
    uint32_t abi_version;
    uint32_t mod_count;
    uint32_t game_root_length;
    uint32_t mod_path_length;
    uint32_t reserved;
} startup_parser_environment_result_v1;

// Step string pointers are valid only for the duration of the callback. The
// result owns its diagnostics and remains valid after the parser module unloads.
STARTUP_PARSER_ABI startup_parser_status_v1 startup_parser_run_v1(
    const startup_parser_request_v1 *request,
    startup_parser_result_v1 *result);

// Environment strings are copied into caller-owned buffers. Lengths report the
// complete source length excluding the terminator so truncation is detectable.
// Mod-name pointers are valid only for the duration of the callback.
STARTUP_PARSER_ABI startup_parser_status_v1 startup_parser_inspect_environment_v1(
    const startup_parser_environment_request_v1 *request,
    startup_parser_environment_result_v1 *result);

#ifdef __cplusplus
}
#endif
