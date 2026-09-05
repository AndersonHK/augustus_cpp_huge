#include "assets/graphics_extraction_client.h"

#ifdef _WIN32

#include <Windows.h>

#include <array>
#include <cstring>
#include <filesystem>

namespace {

class LoadedGraphicsExtractor {
public:
    LoadedGraphicsExtractor()
        : module_(load())
    {
    }

    ~LoadedGraphicsExtractor()
    {
        if (module_) {
            FreeLibrary(module_);
        }
    }

    LoadedGraphicsExtractor(const LoadedGraphicsExtractor &) = delete;
    LoadedGraphicsExtractor &operator=(const LoadedGraphicsExtractor &) = delete;

    template<typename Entry>
    Entry entryPoint(const char *name) const
    {
        const FARPROC address = module_ ? GetProcAddress(module_, name) : nullptr;
        static_assert(sizeof(Entry) == sizeof(address));
        Entry entry = nullptr;
        std::memcpy(&entry, &address, sizeof(entry));
        return entry;
    }

private:
    static HMODULE load()
    {
        try {
            std::array<wchar_t, 32768> executable_path = {};
            const DWORD length = GetModuleFileNameW(nullptr, executable_path.data(), static_cast<DWORD>(executable_path.size()));
            if (!length || length >= executable_path.size()) {
                return nullptr;
            }
            std::filesystem::path module_path(executable_path.data());
            module_path.replace_filename(GRAPHICS_EXTRACTION_MODULE_FILENAME_W);
            return LoadLibraryExW(
                module_path.c_str(),
                nullptr,
                LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_SYSTEM32);
        } catch (...) {
            return nullptr;
        }
    }

    HMODULE module_ = nullptr;
};

using RunAugustusEntry = graphics_extraction_status_v1 (*)(
    const graphics_extraction_augustus_request_v1 *, graphics_extraction_result_v1 *);
using BootstrapClimateEntry = graphics_extraction_status_v1 (*)(
    const graphics_extraction_climate_request_v1 *, graphics_extraction_result_v1 *);

} // namespace

graphics_extraction_status_v1 GraphicsExtractionClient::runAugustus(
    const graphics_extraction_augustus_request_v1 &request,
    graphics_extraction_result_v1 &result) const
{
    LoadedGraphicsExtractor module;
    const RunAugustusEntry entry = module.entryPoint<RunAugustusEntry>(
        GRAPHICS_EXTRACTION_RUN_AUGUSTUS_V1_NAME);
    return entry ? entry(&request, &result) : GRAPHICS_EXTRACTION_STATUS_UNAVAILABLE;
}

graphics_extraction_status_v1 GraphicsExtractionClient::bootstrapClimate(
    const graphics_extraction_climate_request_v1 &request,
    graphics_extraction_result_v1 &result) const
{
    LoadedGraphicsExtractor module;
    const BootstrapClimateEntry entry = module.entryPoint<BootstrapClimateEntry>(
        GRAPHICS_EXTRACTION_BOOTSTRAP_CLIMATE_V1_NAME);
    return entry ? entry(&request, &result) : GRAPHICS_EXTRACTION_STATUS_UNAVAILABLE;
}

#ifdef STARTUP_PARSER_TEST
bool GraphicsExtractionClient::moduleIsLoadedForTest()
{
    return GetModuleHandleW(GRAPHICS_EXTRACTION_MODULE_FILENAME_W) != nullptr;
}
#endif

#else

graphics_extraction_status_v1 GraphicsExtractionClient::runAugustus(
    const graphics_extraction_augustus_request_v1 &request,
    graphics_extraction_result_v1 &result) const
{
    (void) request;
    (void) result;
    return GRAPHICS_EXTRACTION_STATUS_UNAVAILABLE;
}

graphics_extraction_status_v1 GraphicsExtractionClient::bootstrapClimate(
    const graphics_extraction_climate_request_v1 &request,
    graphics_extraction_result_v1 &result) const
{
    (void) request;
    (void) result;
    return GRAPHICS_EXTRACTION_STATUS_UNAVAILABLE;
}

#ifdef STARTUP_PARSER_TEST
bool GraphicsExtractionClient::moduleIsLoadedForTest()
{
    return false;
}
#endif

#endif
