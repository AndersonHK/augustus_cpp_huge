#include "platform.h"

#include "game/system.h"
#include "platform/emscripten/emscripten.h"

#include "SDL.h"

#include <cstring>
#include <cstdlib>
#include <string>

static SDL_version version;

namespace {
char *platform_alloc_path(char *raw_path)
{
    if (!raw_path) {
        return nullptr;
    }
    const std::string path = raw_path;
    SDL_free(raw_path);

    char *buffer = static_cast<char *>(SDL_malloc(path.size() + 1));
    if (!buffer) {
        return nullptr;
    }
    std::memcpy(buffer, path.c_str(), path.size() + 1);
    return buffer;
}

std::string copy_sdl_path(char *path)
{
    if (!path) {
        return {};
    }
    std::string result(path);
    SDL_free(path);
    return result;
}
}

const char *system_architecture(void)
{
#if defined(__x86_64__) || defined(_M_X64)
    return "x64";
#elif defined(i386) || defined(__i386__) || defined(__i386) || defined(_M_IX86)
    return "x86";
#elif defined(__ARM_ARCH_2__)
    return "ARM2";
#elif defined(__ARM_ARCH_3__) || defined(__ARM_ARCH_3M__)
    return "ARM3";
#elif defined(__ARM_ARCH_4T__) || defined(__TARGET_ARM_4T)
    return "ARM4T";
#elif defined(__ARM_ARCH_5_) || defined(__ARM_ARCH_5E_)
    return "ARM5";
#elif defined(__ARM_ARCH_6T2_)
    return "ARM6T2";
#elif defined(__ARM_ARCH_6__) || defined(__ARM_ARCH_6J__) || defined(__ARM_ARCH_6K__) || defined(__ARM_ARCH_6Z__) || defined(__ARM_ARCH_6ZK__)
    return "ARM6";
#elif defined(__ARM_ARCH_7__) || defined(__ARM_ARCH_7A__) || defined(__ARM_ARCH_7R__) || defined(__ARM_ARCH_7M__) || defined(__ARM_ARCH_7S__)
    return "ARM7";
#elif defined(__aarch64__) || defined(_M_ARM64)
    return "ARM64";
#elif defined(mips) || defined(__mips__) || defined(__mips)
    return "MIPS";
#elif defined(__sh__)
    return "SUPERH";
#elif defined(__powerpc) || defined(__powerpc__) || defined(__powerpc64__) || defined(__POWERPC__) || defined(__ppc__) || defined(__PPC__) || defined(_ARCH_PPC)
    return "POWERPC";
#elif defined(__PPC64__) || defined(__ppc64__) || defined(_ARCH_PPC64)
    return "POWERPC64";
#elif defined(__sparc__) || defined(__sparc)
    return "SPARC";
#elif defined(__m68k__)
    return "M68K";
#else
    return "(unknown architecture)";
#endif
}

#if defined(_WIN32) || defined(_WIN64)
#include <windows.h>
#elif defined (__GNUC__) && !defined (__SWITCH__)
#include <sys/utsname.h>
#endif


const char *system_OS(void)
{
#if defined(_WIN32) || defined(_WIN64)
    static const std::string full_version = []() {
        HMODULE nt_dll = LoadLibrary(TEXT("Ntdll.dll"));
        if (nt_dll) {
            using RtlGetVersionFn = LONG (CALLBACK *)(PRTL_OSVERSIONINFOW lpVersionInformation);
            auto get_current_windows_version = reinterpret_cast<RtlGetVersionFn>(GetProcAddress(nt_dll, "RtlGetVersion"));
            if (get_current_windows_version) {
                RTL_OSVERSIONINFOW ovi = {};
                ovi.dwOSVersionInfoSize = sizeof(ovi);
                if (get_current_windows_version(&ovi) == 0) {
                    unsigned long major = ovi.dwMajorVersion;
                    if (ovi.dwBuildNumber >= 22000) {
                        major = 11;
                    }
                    return std::string("Windows ") + std::to_string(major) + "." + std::to_string(ovi.dwMinorVersion) +
                        " build " + std::to_string(ovi.dwBuildNumber);
                }
            }
        }
        return std::string("Windows (unknown version)");
    }();
    return full_version.c_str();
#elif defined(__APPLE__) || defined(__MACH__)
    #include <TargetConditionals.h>
    #if TARGET_OS_MAC
        return "Mac OS X";
    #endif
#elif defined(__GNUC__) && !defined(__SWITCH__)
    static const std::string full_version = []() {
        struct utsname uts;
        if (uname(&uts) == 0) {
            return std::string(uts.sysname) + " " + uts.release;
        }
#ifdef __linux__
        return std::string("Linux");
#endif
        return std::string("Unix");
    }();
    return full_version.c_str();
#elif defined(__HAIKU__)
    return "Haiku";
#elif defined(__FreeBSD__)
    return "FreeBSD";
#elif defined(__NetBSD__)
    return "NetBSD";
#elif defined(__OpenBSD__)
    return "OpenBSD";
#elif defined(__vita__)
    return "PlayStation Vita";
#elif defined(__SWITCH__)
    return "Nintendo Switch";
#elif defined(__ANDROID__)
    return "Android";
#elif defined(__EMSCRIPTEN__)
    return "Emscripten";
#else
    return "(unknown operating system)";
#endif
}

int platform_sdl_version_at_least(int major, int minor, int patch)
{
    if (version.major == 0) {
        SDL_GetVersion(&version);
    }
    return SDL_VERSIONNUM(version.major, version.minor, version.patch) >= SDL_VERSIONNUM(major, minor, patch);
}

char *platform_get_logging_path(void)
{
#if defined(__ANDROID__)
    return NULL;
#else
    return platform_get_pref_path();
#endif
}

char *platform_get_pref_path(void)
{
#if SDL_VERSION_ATLEAST(2, 0, 1)
    if (platform_sdl_version_at_least(2, 0, 1)) {
        return platform_alloc_path(SDL_GetPrefPath("augustus", "augustus"));
    }
#endif
    return 0;
}

namespace platform {

std::string logging_path()
{
#if defined(__ANDROID__)
    return {};
#else
    return pref_path();
#endif
}

std::string pref_path()
{
#if SDL_VERSION_ATLEAST(2, 0, 1)
    if (platform_sdl_version_at_least(2, 0, 1)) {
        return copy_sdl_path(SDL_GetPrefPath("augustus", "augustus"));
    }
#endif
    return {};
}

std::filesystem::path logging_path_filesystem()
{
    return logging_path();
}

std::filesystem::path pref_path_filesystem()
{
    return pref_path();
}

}

void exit_with_status(int status)
{
#ifdef __EMSCRIPTEN__
    EM_ASM(Module.quitGame($0), status);
#endif
    exit(status);
}
