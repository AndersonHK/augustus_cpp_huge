#pragma once

#include <stdint.h>

#define VESPASIAN_LOAD_SAVE_ABI_VERSION 1u
#define VESPASIAN_LOAD_SAVE_GET_API_SYMBOL "vespasian_load_save_get_api"
#define VESPASIAN_LOAD_SAVE_MODULE_FILENAME_W L"VespasianLoadSave.dll"

#if defined(_WIN32)
#define VESPASIAN_LOAD_SAVE_API_CALL __cdecl
#if defined(VESPASIAN_LOAD_SAVE_MODULE_EXPORTS)
#define VESPASIAN_LOAD_SAVE_API_EXPORT __declspec(dllexport)
#else
#define VESPASIAN_LOAD_SAVE_API_EXPORT
#endif
#else
#define VESPASIAN_LOAD_SAVE_API_CALL
#define VESPASIAN_LOAD_SAVE_API_EXPORT
#endif

#ifdef __cplusplus
extern "C" {
#endif

enum VespasianLoadSaveResult {
    VESPASIAN_LOAD_SAVE_OK = 0,
    VESPASIAN_LOAD_SAVE_INVALID_ARGUMENT = 1,
    VESPASIAN_LOAD_SAVE_STRUCT_SIZE_MISMATCH = 2,
    VESPASIAN_LOAD_SAVE_VERSION_MISMATCH = 3,
    VESPASIAN_LOAD_SAVE_RESERVED_FIELD_NONZERO = 4,
    VESPASIAN_LOAD_SAVE_IO_ERROR = 5,
    VESPASIAN_LOAD_SAVE_BUFFER_TOO_SMALL = 6,
    VESPASIAN_LOAD_SAVE_UNAVAILABLE = 7,
    VESPASIAN_LOAD_SAVE_NOT_FOUND = 8
};

enum VespasianLoadSaveCapability {
    VESPASIAN_LOAD_SAVE_CAPABILITY_READ_ARCHIVE = 1u << 0
};

typedef struct VespasianLoadSaveApiRequest {
    uint32_t struct_size;
    uint32_t abi_version;
    uint32_t reserved[6];
} VespasianLoadSaveApiRequest;

typedef struct VespasianLoadSaveReadArchiveRequest {
    uint32_t struct_size;
    uint32_t path_size;
    const char *path_utf8;
    uint64_t file_offset;
    uint64_t expected_file_size;
    uint64_t expected_write_time;
    uint8_t *destination;
    uint64_t destination_capacity;
    uint32_t reserved[4];
} VespasianLoadSaveReadArchiveRequest;

typedef struct VespasianLoadSaveReadArchiveResult {
    uint32_t struct_size;
    uint32_t reserved0;
    uint64_t required_size;
    uint64_t written_size;
    uint64_t file_size;
    uint64_t write_time;
    uint64_t reserved[2];
} VespasianLoadSaveReadArchiveResult;

typedef uint32_t (VESPASIAN_LOAD_SAVE_API_CALL *VespasianLoadSaveReadArchiveFn)(
    const VespasianLoadSaveReadArchiveRequest *request,
    VespasianLoadSaveReadArchiveResult *result,
    char *diagnostic,
    uint32_t diagnostic_capacity);

typedef struct VespasianLoadSaveApi {
    uint32_t struct_size;
    uint32_t abi_version;
    uint32_t capabilities;
    uint32_t reserved0;
    VespasianLoadSaveReadArchiveFn read_archive;
    uint64_t reserved[3];
} VespasianLoadSaveApi;

typedef uint32_t (VESPASIAN_LOAD_SAVE_API_CALL *VespasianLoadSaveGetApiFn)(
    const VespasianLoadSaveApiRequest *request,
    VespasianLoadSaveApi *out_api,
    char *diagnostic,
    uint32_t diagnostic_capacity);

VESPASIAN_LOAD_SAVE_API_EXPORT uint32_t VESPASIAN_LOAD_SAVE_API_CALL vespasian_load_save_get_api(
    const VespasianLoadSaveApiRequest *request,
    VespasianLoadSaveApi *out_api,
    char *diagnostic,
    uint32_t diagnostic_capacity);

#ifdef __cplusplus
}

static_assert(sizeof(void *) == 8, "The Load/Save ABI currently supports x64 builds only.");
static_assert(sizeof(VespasianLoadSaveApiRequest) == 32, "Load/Save ABI request layout changed.");
static_assert(sizeof(VespasianLoadSaveReadArchiveRequest) == 72, "Load/Save read request layout changed.");
static_assert(sizeof(VespasianLoadSaveReadArchiveResult) == 56, "Load/Save read result layout changed.");
static_assert(sizeof(VespasianLoadSaveApi) == 48, "Load/Save ABI response layout changed.");
#endif
