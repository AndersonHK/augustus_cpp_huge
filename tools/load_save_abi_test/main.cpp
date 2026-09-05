#include "game/load_save/LoadSaveModuleAbi.h"
#include "game/load_save/LoadSaveModuleClient.h"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>

#include <algorithm>
#include <array>
#include <cstring>
#include <iostream>
#include <string>
#include <vector>

namespace {

std::wstring executable_directory()
{
    std::array<wchar_t, 32768> path = {};
    const DWORD length = GetModuleFileNameW(nullptr, path.data(), static_cast<DWORD>(path.size()));
    if (!length || length >= path.size()) return {};
    const std::wstring executable(path.data(), length);
    const size_t separator = executable.find_last_of(L"\\/");
    return separator == std::wstring::npos ? std::wstring() : executable.substr(0, separator);
}

std::string utf8(const std::wstring &text)
{
    const int length = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, text.data(),
        static_cast<int>(text.size()), nullptr, 0, nullptr, nullptr);
    if (length <= 0) return {};
    std::string result(static_cast<size_t>(length), '\0');
    return WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, text.data(),
        static_cast<int>(text.size()), result.data(), length, nullptr, nullptr) == length ? result : std::string{};
}

class FixtureFile {
public:
    bool create()
    {
        std::array<wchar_t, MAX_PATH> directory = {};
        std::array<wchar_t, MAX_PATH> path = {};
        if (!GetTempPathW(static_cast<DWORD>(directory.size()), directory.data()) ||
            !GetTempFileNameW(directory.data(), L"vls", 0, path.data())) return false;
        path_ = path.data();
        HANDLE file = CreateFileW(path_.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_TEMPORARY, nullptr);
        if (file == INVALID_HANDLE_VALUE) return false;
        bytes_.resize(257);
        for (size_t i = 0; i < bytes_.size(); ++i) bytes_[i] = static_cast<uint8_t>(i);
        DWORD written = 0;
        const bool wrote = WriteFile(file, bytes_.data(), static_cast<DWORD>(bytes_.size()), &written, nullptr) &&
            written == bytes_.size();
        return CloseHandle(file) && wrote;
    }

    ~FixtureFile() { if (!path_.empty()) DeleteFileW(path_.c_str()); }

    const std::wstring &path() const { return path_; }
    const std::vector<uint8_t> &bytes() const { return bytes_; }

private:
    std::wstring path_;
    std::vector<uint8_t> bytes_;
};

bool expect(uint32_t actual, uint32_t expected, const char *contract, const char *diagnostic)
{
    if (actual == expected && (expected == VESPASIAN_LOAD_SAVE_OK || (diagnostic && *diagnostic))) return true;
    std::cerr << contract << " failed: result " << actual << ", expected " << expected
        << ", diagnostic '" << (diagnostic ? diagnostic : "") << "'.\n";
    return false;
}

bool validate_export(const std::wstring &module_path, const FixtureFile &fixture)
{
    HMODULE module = LoadLibraryExW(module_path.c_str(), nullptr,
        LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_SYSTEM32);
    if (!module) {
        std::cerr << "Unable to load ABI test module; Windows error " << GetLastError() << ".\n";
        return false;
    }
    const auto get_api = reinterpret_cast<VespasianLoadSaveGetApiFn>(
        GetProcAddress(module, VESPASIAN_LOAD_SAVE_GET_API_SYMBOL));
    if (!get_api) {
        std::cerr << "Load/Save module does not export its ABI handshake.\n";
        FreeLibrary(module);
        return false;
    }

    std::array<char, 256> diagnostic = {};
    VespasianLoadSaveApiRequest api_request = {};
    api_request.struct_size = sizeof(api_request);
    api_request.abi_version = VESPASIAN_LOAD_SAVE_ABI_VERSION;
    VespasianLoadSaveApi api = {};
    if (!expect(get_api(&api_request, &api, diagnostic.data(), static_cast<uint32_t>(diagnostic.size())),
            VESPASIAN_LOAD_SAVE_OK, "valid handshake", diagnostic.data()) ||
        api.struct_size != sizeof(api) || api.capabilities != VESPASIAN_LOAD_SAVE_CAPABILITY_READ_ARCHIVE ||
        !api.read_archive) {
        FreeLibrary(module);
        return false;
    }

    const std::string path = utf8(fixture.path());
    VespasianLoadSaveReadArchiveRequest request = {};
    request.struct_size = sizeof(request);
    request.path_utf8 = path.data();
    request.path_size = static_cast<uint32_t>(path.size());
    request.file_offset = 17;
    VespasianLoadSaveReadArchiveResult query = {};
    diagnostic = {};
    if (!expect(api.read_archive(&request, &query, diagnostic.data(), static_cast<uint32_t>(diagnostic.size())),
            VESPASIAN_LOAD_SAVE_OK, "archive size query", diagnostic.data()) ||
        query.struct_size != sizeof(query) || query.required_size != fixture.bytes().size() - request.file_offset ||
        !query.file_size || !query.write_time || query.written_size) {
        FreeLibrary(module);
        return false;
    }

    std::array<uint8_t, 16> canary;
    canary.fill(0xA5);
    request.destination = canary.data();
    request.destination_capacity = canary.size();
    request.expected_file_size = query.file_size;
    request.expected_write_time = query.write_time;
    VespasianLoadSaveReadArchiveResult too_small = {};
    diagnostic = {};
    if (!expect(api.read_archive(&request, &too_small, diagnostic.data(), static_cast<uint32_t>(diagnostic.size())),
            VESPASIAN_LOAD_SAVE_BUFFER_TOO_SMALL, "undersized destination rejection", diagnostic.data())) {
        FreeLibrary(module);
        return false;
    }
    for (uint8_t byte : canary) if (byte != 0xA5) {
        std::cerr << "Archive reader modified an undersized destination.\n";
        FreeLibrary(module);
        return false;
    }

    std::vector<uint8_t> output(static_cast<size_t>(query.required_size));
    request.destination = output.data();
    request.destination_capacity = output.size();
    VespasianLoadSaveReadArchiveResult filled = {};
    diagnostic = {};
    if (!expect(api.read_archive(&request, &filled, diagnostic.data(), static_cast<uint32_t>(diagnostic.size())),
            VESPASIAN_LOAD_SAVE_OK, "archive fill", diagnostic.data()) ||
        filled.written_size != output.size() ||
        !std::equal(output.begin(), output.end(), fixture.bytes().begin() + 17)) {
        FreeLibrary(module);
        return false;
    }

    request.expected_file_size++;
    diagnostic = {};
    if (!expect(api.read_archive(&request, &filled, diagnostic.data(), static_cast<uint32_t>(diagnostic.size())),
            VESPASIAN_LOAD_SAVE_IO_ERROR, "file identity rejection", diagnostic.data())) {
        FreeLibrary(module);
        return false;
    }

    api_request.struct_size--;
    diagnostic = {};
    if (!expect(get_api(&api_request, &api, diagnostic.data(), static_cast<uint32_t>(diagnostic.size())),
            VESPASIAN_LOAD_SAVE_STRUCT_SIZE_MISMATCH, "ABI size rejection", diagnostic.data())) {
        FreeLibrary(module);
        return false;
    }
    if (!FreeLibrary(module) || GetModuleHandleW(VESPASIAN_LOAD_SAVE_MODULE_FILENAME_W)) {
        std::cerr << "Load/Save module remained resident after direct ABI validation.\n";
        return false;
    }
    return true;
}

bool validate_client(const FixtureFile &fixture)
{
    const std::string path = utf8(fixture.path());
    std::vector<uint8_t> archive;
    std::string diagnostic;
    if (LoadSaveModuleClient().readArchive(path.c_str(), 31, archive, diagnostic) != VESPASIAN_LOAD_SAVE_OK ||
        !std::equal(archive.begin(), archive.end(), fixture.bytes().begin() + 31) ||
        GetModuleHandleW(VESPASIAN_LOAD_SAVE_MODULE_FILENAME_W)) {
        std::cerr << "One-shot client archive read/unload failed: " << diagnostic << '\n';
        return false;
    }
    if (LoadSaveModuleClient().readArchive("missing-load-save-fixture.svv", 0, archive, diagnostic) ==
            VESPASIAN_LOAD_SAVE_OK || diagnostic.empty() || GetModuleHandleW(VESPASIAN_LOAD_SAVE_MODULE_FILENAME_W)) {
        std::cerr << "One-shot client failure path did not fail and unload cleanly.\n";
        return false;
    }
    return true;
}

} // namespace

int main()
{
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX | SEM_NOOPENFILEERRORBOX);
    FixtureFile fixture;
    const std::wstring directory = executable_directory();
    if (directory.empty() || !fixture.create()) {
        std::cerr << "Unable to create Load/Save ABI fixture.\n";
        return 1;
    }
    if (!validate_export(directory + L"\\VespasianLoadSave.dll", fixture) || !validate_client(fixture)) return 1;
    std::cout << "Load/Save archive read, caller-owned memory, ABI rejection, and unload contracts passed.\n";
    return 0;
}
