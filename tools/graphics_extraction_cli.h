#ifndef GRAPHICS_EXTRACTION_CLI_H
#define GRAPHICS_EXTRACTION_CLI_H

#include <cstdio>
#include <cstdlib>

#if defined(_WIN32)
#include <windows.h>

inline LONG WINAPI report_graphics_extraction_exception(EXCEPTION_POINTERS *exception)
{
    std::fprintf(stderr, "Graphics extraction crashed: exception 0x%08lx\n", exception->ExceptionRecord->ExceptionCode);
    std::fflush(stderr);
    return EXCEPTION_EXECUTE_HANDLER;
}
#endif

inline void configure_graphics_extraction_cli()
{
#if defined(_WIN32)
    _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX | SEM_NOOPENFILEERRORBOX);
    SetUnhandledExceptionFilter(report_graphics_extraction_exception);
#endif
}

#endif
