#include "game/load_save/LoadSaveModuleAbi.h"

#include <Windows.h>

#include <algorithm>
#include <climits>
#include <cstddef>
#include <cstring>
#include <string>

namespace {

void write_diagnostic(char *diagnostic, uint32_t capacity, const char *message)
{
    if (!diagnostic || !capacity) return;
    const size_t length = message ? std::strlen(message) : 0;
    const size_t copy_length = std::min(length, static_cast<size_t>(capacity - 1));
    if (copy_length) std::memcpy(diagnostic, message, copy_length);
    diagnostic[copy_length] = '\0';
}

template<typename T, size_t Size>
bool fields_are_zero(const T (&fields)[Size])
{
    for (T field : fields) if (field) return false;
    return true;
}

uint32_t reject(VespasianLoadSaveApi *api, char *diagnostic, uint32_t capacity, uint32_t result, const char *message)
{
    if (api) *api = {};
    write_diagnostic(diagnostic, capacity, message);
    return result;
}

uint32_t reject(VespasianLoadSaveReadArchiveResult *result, char *diagnostic, uint32_t capacity,
    uint32_t code, const char *message)
{
    if (result) *result = {};
    write_diagnostic(diagnostic, capacity, message);
    return code;
}

std::wstring utf8_path(const VespasianLoadSaveReadArchiveRequest &request)
{
    if (!request.path_utf8 || !request.path_size || request.path_size > INT_MAX ||
        std::memchr(request.path_utf8, '\0', request.path_size)) return {};
    const int length = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, request.path_utf8,
        static_cast<int>(request.path_size), nullptr, 0);
    if (length <= 0) return {};
    std::wstring path(static_cast<size_t>(length), L'\0');
    return MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, request.path_utf8,
        static_cast<int>(request.path_size), path.data(), length) == length ? path : std::wstring{};
}

uint64_t file_time_value(const FILETIME &value)
{
    ULARGE_INTEGER time = {};
    time.LowPart = value.dwLowDateTime;
    time.HighPart = value.dwHighDateTime;
    return time.QuadPart;
}

uint32_t read_archive_impl(const VespasianLoadSaveReadArchiveRequest *request,
    VespasianLoadSaveReadArchiveResult *result, char *diagnostic, uint32_t diagnostic_capacity)
{
    if ((!diagnostic && diagnostic_capacity) || (diagnostic && !diagnostic_capacity)) {
        return VESPASIAN_LOAD_SAVE_INVALID_ARGUMENT;
    }
    write_diagnostic(diagnostic, diagnostic_capacity, "");
    if (!request || !result) return reject(result, diagnostic, diagnostic_capacity,
        VESPASIAN_LOAD_SAVE_INVALID_ARGUMENT, "Archive read requires request and result structures.");
    if (request->struct_size != sizeof(*request)) return reject(result, diagnostic, diagnostic_capacity,
        VESPASIAN_LOAD_SAVE_STRUCT_SIZE_MISMATCH, "Archive read request size does not match version 1.");
    if (!fields_are_zero(request->reserved)) return reject(result, diagnostic, diagnostic_capacity,
        VESPASIAN_LOAD_SAVE_RESERVED_FIELD_NONZERO, "Archive read reserved fields must be zero.");
    if ((!request->destination && request->destination_capacity) ||
        (request->destination && !request->destination_capacity)) return reject(result, diagnostic, diagnostic_capacity,
        VESPASIAN_LOAD_SAVE_INVALID_ARGUMENT, "Archive destination and capacity must either both be set or both be empty.");

    const std::wstring path = utf8_path(*request);
    if (path.empty()) return reject(result, diagnostic, diagnostic_capacity,
        VESPASIAN_LOAD_SAVE_INVALID_ARGUMENT, "Archive path must be non-empty valid UTF-8 without embedded nulls.");

    HANDLE file = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN, nullptr);
    if (file == INVALID_HANDLE_VALUE) {
        const DWORD error = GetLastError();
        return reject(result, diagnostic, diagnostic_capacity,
            error == ERROR_FILE_NOT_FOUND || error == ERROR_PATH_NOT_FOUND ?
                VESPASIAN_LOAD_SAVE_NOT_FOUND : VESPASIAN_LOAD_SAVE_IO_ERROR,
            error == ERROR_FILE_NOT_FOUND || error == ERROR_PATH_NOT_FOUND ?
                "Save archive does not exist." : "Unable to open save archive.");
    }

    LARGE_INTEGER size = {};
    FILETIME write_time = {};
    if (!GetFileSizeEx(file, &size) || size.QuadPart < 0 || !GetFileTime(file, nullptr, nullptr, &write_time)) {
        CloseHandle(file);
        return reject(result, diagnostic, diagnostic_capacity,
            VESPASIAN_LOAD_SAVE_IO_ERROR, "Unable to inspect save archive.");
    }
    const uint64_t file_size = static_cast<uint64_t>(size.QuadPart);
    const uint64_t write_time_value = file_time_value(write_time);
    if (request->file_offset > file_size) {
        CloseHandle(file);
        return reject(result, diagnostic, diagnostic_capacity,
            VESPASIAN_LOAD_SAVE_INVALID_ARGUMENT, "Archive offset exceeds file size.");
    }

    *result = {};
    result->struct_size = sizeof(*result);
    result->required_size = file_size - request->file_offset;
    result->file_size = file_size;
    result->write_time = write_time_value;
    if (!request->destination) {
        CloseHandle(file);
        return VESPASIAN_LOAD_SAVE_OK;
    }
    if ((request->expected_file_size && request->expected_file_size != file_size) ||
        (request->expected_write_time && request->expected_write_time != write_time_value)) {
        CloseHandle(file);
        return reject(result, diagnostic, diagnostic_capacity,
            VESPASIAN_LOAD_SAVE_IO_ERROR, "Save archive changed between size query and read.");
    }
    if (request->destination_capacity < result->required_size) {
        CloseHandle(file);
        write_diagnostic(diagnostic, diagnostic_capacity, "Archive destination is too small.");
        return VESPASIAN_LOAD_SAVE_BUFFER_TOO_SMALL;
    }

    LARGE_INTEGER offset = {};
    offset.QuadPart = static_cast<LONGLONG>(request->file_offset);
    if (!SetFilePointerEx(file, offset, nullptr, FILE_BEGIN)) {
        CloseHandle(file);
        return reject(result, diagnostic, diagnostic_capacity,
            VESPASIAN_LOAD_SAVE_IO_ERROR, "Unable to seek within save archive.");
    }
    uint64_t remaining = result->required_size;
    uint8_t *destination = request->destination;
    while (remaining) {
        const DWORD requested = static_cast<DWORD>(std::min<uint64_t>(remaining, 1u << 30));
        DWORD read = 0;
        if (!ReadFile(file, destination, requested, &read, nullptr) || read != requested) {
            CloseHandle(file);
            return reject(result, diagnostic, diagnostic_capacity,
                VESPASIAN_LOAD_SAVE_IO_ERROR, "Unable to read complete save archive.");
        }
        destination += read;
        remaining -= read;
        result->written_size += read;
    }
    if (!CloseHandle(file)) return reject(result, diagnostic, diagnostic_capacity,
        VESPASIAN_LOAD_SAVE_IO_ERROR, "Unable to close save archive.");
    return VESPASIAN_LOAD_SAVE_OK;
}

uint32_t read_archive(const VespasianLoadSaveReadArchiveRequest *request,
    VespasianLoadSaveReadArchiveResult *result, char *diagnostic, uint32_t diagnostic_capacity)
{
    try {
        return read_archive_impl(request, result, diagnostic, diagnostic_capacity);
    } catch (...) {
        return reject(result, diagnostic, diagnostic_capacity,
            VESPASIAN_LOAD_SAVE_UNAVAILABLE, "Load/Save module could not allocate archive boundary state.");
    }
}

} // namespace

extern "C" uint32_t VESPASIAN_LOAD_SAVE_API_CALL vespasian_load_save_get_api(
    const VespasianLoadSaveApiRequest *request, VespasianLoadSaveApi *out_api,
    char *diagnostic, uint32_t diagnostic_capacity)
{
    if ((!diagnostic && diagnostic_capacity) || (diagnostic && !diagnostic_capacity)) {
        return VESPASIAN_LOAD_SAVE_INVALID_ARGUMENT;
    }
    write_diagnostic(diagnostic, diagnostic_capacity, "");
    if (!request || !out_api) return reject(out_api, diagnostic, diagnostic_capacity,
        VESPASIAN_LOAD_SAVE_INVALID_ARGUMENT, "Load/Save ABI requires request and output structures.");
    if (request->struct_size != sizeof(*request)) return reject(out_api, diagnostic, diagnostic_capacity,
        VESPASIAN_LOAD_SAVE_STRUCT_SIZE_MISMATCH, "Load/Save ABI request size does not match version 1.");
    if (request->abi_version != VESPASIAN_LOAD_SAVE_ABI_VERSION) return reject(out_api, diagnostic, diagnostic_capacity,
        VESPASIAN_LOAD_SAVE_VERSION_MISMATCH, "Load/Save ABI version is not supported.");
    if (!fields_are_zero(request->reserved)) return reject(out_api, diagnostic, diagnostic_capacity,
        VESPASIAN_LOAD_SAVE_RESERVED_FIELD_NONZERO, "Load/Save ABI reserved request fields must be zero.");

    *out_api = {};
    out_api->struct_size = sizeof(*out_api);
    out_api->abi_version = VESPASIAN_LOAD_SAVE_ABI_VERSION;
    out_api->capabilities = VESPASIAN_LOAD_SAVE_CAPABILITY_READ_ARCHIVE;
    out_api->read_archive = read_archive;
    return VESPASIAN_LOAD_SAVE_OK;
}
