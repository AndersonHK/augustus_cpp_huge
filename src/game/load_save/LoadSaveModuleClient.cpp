#include "game/load_save/LoadSaveModuleClient.h"

#ifdef _WIN32

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>

#include <array>
#include <climits>
#include <cstring>
#include <filesystem>

namespace {

class LoadedLoadSaveModule {
public:
    LoadedLoadSaveModule() : module_(load()) {}
    ~LoadedLoadSaveModule() { if (module_) FreeLibrary(module_); }

    LoadedLoadSaveModule(const LoadedLoadSaveModule &) = delete;
    LoadedLoadSaveModule &operator=(const LoadedLoadSaveModule &) = delete;

    bool api(VespasianLoadSaveApi &api, std::string &diagnostic) const
    {
        if (!module_) {
            diagnostic = "Unable to load VespasianLoadSave.dll beside the executable.";
            return false;
        }
        const FARPROC address = GetProcAddress(module_, VESPASIAN_LOAD_SAVE_GET_API_SYMBOL);
        VespasianLoadSaveGetApiFn get_api = nullptr;
        static_assert(sizeof(get_api) == sizeof(address));
        std::memcpy(&get_api, &address, sizeof(get_api));
        if (!get_api) {
            diagnostic = "VespasianLoadSave.dll does not export the versioned ABI handshake.";
            return false;
        }
        VespasianLoadSaveApiRequest request = {};
        request.struct_size = sizeof(request);
        request.abi_version = VESPASIAN_LOAD_SAVE_ABI_VERSION;
        std::array<char, 512> module_diagnostic = {};
        const uint32_t status = get_api(&request, &api, module_diagnostic.data(),
            static_cast<uint32_t>(module_diagnostic.size()));
        if (status != VESPASIAN_LOAD_SAVE_OK) {
            diagnostic = module_diagnostic[0] ? module_diagnostic.data() : "Load/Save module rejected the ABI handshake.";
            return false;
        }
        if (api.struct_size != sizeof(api) || api.abi_version != VESPASIAN_LOAD_SAVE_ABI_VERSION ||
            !(api.capabilities & VESPASIAN_LOAD_SAVE_CAPABILITY_READ_ARCHIVE) || !api.read_archive || api.reserved0) {
            diagnostic = "Load/Save module returned an incompatible API table.";
            return false;
        }
        for (uint64_t value : api.reserved) {
            if (value) {
                diagnostic = "Load/Save module returned nonzero reserved API fields.";
                return false;
            }
        }
        return true;
    }

    bool close(std::string &diagnostic)
    {
        if (!module_) return true;
        if (!FreeLibrary(module_)) {
            diagnostic = "Unable to unload VespasianLoadSave.dll after the archive operation.";
            return false;
        }
        module_ = nullptr;
        return true;
    }

private:
    static HMODULE load()
    {
        try {
            std::array<wchar_t, 32768> executable_path = {};
            const DWORD length = GetModuleFileNameW(nullptr, executable_path.data(),
                static_cast<DWORD>(executable_path.size()));
            if (!length || length >= executable_path.size()) return nullptr;
            std::filesystem::path module_path(executable_path.data());
            module_path.replace_filename(VESPASIAN_LOAD_SAVE_MODULE_FILENAME_W);
            return LoadLibraryExW(module_path.c_str(), nullptr,
                LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_SYSTEM32);
        } catch (...) {
            return nullptr;
        }
    }

    HMODULE module_ = nullptr;
};

uint32_t run_archive_read(VespasianLoadSaveReadArchiveFn read_archive,
    VespasianLoadSaveReadArchiveRequest &request, VespasianLoadSaveReadArchiveResult &result,
    std::string &diagnostic)
{
    std::array<char, 512> module_diagnostic = {};
    const uint32_t status = read_archive(&request, &result, module_diagnostic.data(),
        static_cast<uint32_t>(module_diagnostic.size()));
    diagnostic = module_diagnostic.data();
    return status;
}

} // namespace

uint32_t LoadSaveModuleClient::readArchive(
    const char *path, uint64_t offset, std::vector<uint8_t> &archive, std::string &diagnostic) const
{
    constexpr uint64_t MAX_ARCHIVE_SIZE = 1024ull * 1024ull * 1024ull;
    archive.clear();
    diagnostic.clear();
    if (!path || !*path || std::strlen(path) > static_cast<size_t>(INT_MAX)) {
        diagnostic = "Save archive path is empty or too long.";
        return VESPASIAN_LOAD_SAVE_INVALID_ARGUMENT;
    }

    LoadedLoadSaveModule module;
    VespasianLoadSaveApi api = {};
    if (!module.api(api, diagnostic)) {
        const uint32_t status = module.close(diagnostic) ? VESPASIAN_LOAD_SAVE_UNAVAILABLE : VESPASIAN_LOAD_SAVE_IO_ERROR;
        return status;
    }

    VespasianLoadSaveReadArchiveRequest request = {};
    request.struct_size = sizeof(request);
    request.path_size = static_cast<uint32_t>(std::strlen(path));
    request.path_utf8 = path;
    request.file_offset = offset;
    VespasianLoadSaveReadArchiveResult result = {};
    uint32_t status = run_archive_read(api.read_archive, request, result, diagnostic);
    if (status == VESPASIAN_LOAD_SAVE_OK) {
        bool reserved_are_zero = !result.reserved0;
        for (uint64_t value : result.reserved) reserved_are_zero = reserved_are_zero && !value;
        if (result.struct_size != sizeof(result) || !reserved_are_zero || result.written_size ||
            result.file_size < offset || result.required_size != result.file_size - offset) {
            diagnostic = "Load/Save module returned an incompatible archive query result.";
            status = VESPASIAN_LOAD_SAVE_STRUCT_SIZE_MISMATCH;
        } else if (result.required_size > MAX_ARCHIVE_SIZE || result.required_size > archive.max_size()) {
            diagnostic = "Save archive is too large for caller-owned memory.";
            status = VESPASIAN_LOAD_SAVE_BUFFER_TOO_SMALL;
        } else if (result.required_size) {
            try {
                archive.resize(static_cast<size_t>(result.required_size));
            } catch (...) {
                diagnostic = "Unable to allocate caller-owned memory for save archive.";
                status = VESPASIAN_LOAD_SAVE_BUFFER_TOO_SMALL;
            }
        }
        if (status == VESPASIAN_LOAD_SAVE_OK && result.required_size) {
            request.expected_file_size = result.file_size;
            request.expected_write_time = result.write_time;
            request.destination = archive.data();
            request.destination_capacity = archive.size();
            VespasianLoadSaveReadArchiveResult filled = {};
            status = run_archive_read(api.read_archive, request, filled, diagnostic);
            reserved_are_zero = !filled.reserved0;
            for (uint64_t value : filled.reserved) reserved_are_zero = reserved_are_zero && !value;
            if (status != VESPASIAN_LOAD_SAVE_OK || filled.struct_size != sizeof(filled) ||
                !reserved_are_zero || filled.written_size != archive.size() || filled.required_size != archive.size() ||
                filled.file_size != result.file_size || filled.write_time != result.write_time) {
                if (status == VESPASIAN_LOAD_SAVE_OK) {
                    diagnostic = "Load/Save module returned an inconsistent archive fill result.";
                    status = VESPASIAN_LOAD_SAVE_IO_ERROR;
                }
                archive.clear();
            }
        }
    }

    if (!module.close(diagnostic)) {
        archive.clear();
        return VESPASIAN_LOAD_SAVE_IO_ERROR;
    }
    return status;
}

#ifdef STARTUP_PARSER_TEST
bool LoadSaveModuleClient::moduleIsLoadedForTest()
{
    return GetModuleHandleW(VESPASIAN_LOAD_SAVE_MODULE_FILENAME_W) != nullptr;
}
#endif

#else

uint32_t LoadSaveModuleClient::readArchive(
    const char *, uint64_t, std::vector<uint8_t> &archive, std::string &diagnostic) const
{
    archive.clear();
    diagnostic = "The Load/Save DLL boundary is currently available only on Windows.";
    return VESPASIAN_LOAD_SAVE_UNAVAILABLE;
}

#ifdef STARTUP_PARSER_TEST
bool LoadSaveModuleClient::moduleIsLoadedForTest() { return false; }
#endif

#endif
