#pragma once

/**
 * @file win32.hpp
 * @brief The few Win32 functions huira calls, declared without including <Windows.h>.
 *
 * huira is header-only, so anything its headers include lands in every translation unit of
 * every program that uses it. <Windows.h> declares thousands of global names and macros (SIZE,
 * TBYTE, min, max, ERROR, ...) that collide with ordinary user code and with other libraries,
 * such as CFITSIO's TBYTE. So huira declares what it needs here instead, the way Boost.WinAPI
 * does.
 *
 * The declarations are exactly compatible with the Windows SDK's (same extern "C" names, same
 * parameter types, same dllimport), so a translation unit may include <Windows.h> before or
 * after huira. The rules for anything added here:
 *   - Only names that are not macros in the SDK (for example QueryFullProcessImageNameA, not
 *     QueryFullProcessImageName, and K32GetProcessMemoryInfo, not GetProcessMemoryInfo).
 *   - Only builtin types in signatures (HANDLE is void*, DWORD is unsigned long, BOOL is int),
 *     or pointers to the SDK's own struct tags, forward-declared at global scope. Typed
 *     handles such as HMODULE are SDK struct pointers under STRICT, so avoid them.
 *   - The same dllimport as the SDK: kernel32 functions have it, <Psapi.h> ones do not.
 *   - Check any change by compiling a file that includes <Windows.h> and <Psapi.h> both before
 *     and after huira.
 *   - Constants get lower-case names, since the SDK's upper-case ones are macros.
 */

#ifdef _WIN32

#include <cstddef>

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wreserved-identifier"
#endif

// The SDK's own struct tags, so pointers to them are the same types <Windows.h> uses.
struct _EXCEPTION_POINTERS;
struct _PROCESS_MEMORY_COUNTERS;

#if defined(__clang__)
#pragma clang diagnostic pop
#endif

namespace huira::win32 {

using DWORD = unsigned long;
using BOOL = int;
using LONG = long;
using HANDLE = void*;

using TopLevelExceptionFilter = LONG(__stdcall*)(::_EXCEPTION_POINTERS*);

inline constexpr DWORD std_output_handle = static_cast<DWORD>(-11);
inline constexpr DWORD std_error_handle = static_cast<DWORD>(-12);
inline constexpr DWORD enable_virtual_terminal_processing = 0x0004;
inline constexpr LONG exception_continue_search = 0;
inline constexpr DWORD max_path = 260;

/// Same layout as the SDK's PROCESS_MEMORY_COUNTERS.
struct ProcessMemoryCounters {
    DWORD cb;
    DWORD PageFaultCount;
    std::size_t PeakWorkingSetSize;
    std::size_t WorkingSetSize;
    std::size_t QuotaPeakPagedPoolUsage;
    std::size_t QuotaPagedPoolUsage;
    std::size_t QuotaPeakNonPagedPoolUsage;
    std::size_t QuotaNonPagedPoolUsage;
    std::size_t PagefileUsage;
    std::size_t PeakPagefileUsage;
};

extern "C" {
__declspec(dllimport) HANDLE __stdcall GetStdHandle(DWORD nStdHandle);
__declspec(dllimport) BOOL __stdcall GetConsoleMode(HANDLE hConsoleHandle, DWORD* lpMode);
__declspec(dllimport) BOOL __stdcall SetConsoleMode(HANDLE hConsoleHandle, DWORD dwMode);
__declspec(dllimport) HANDLE __stdcall GetCurrentProcess();
// Not GetModuleFileNameA: its HMODULE parameter is a pointer to an SDK struct under STRICT.
__declspec(dllimport) BOOL __stdcall
QueryFullProcessImageNameA(HANDLE hProcess, DWORD dwFlags, char* lpExeName, DWORD* lpdwSize);
// <Psapi.h> declares this one without dllimport; it is exported by kernel32 since Windows 7.
BOOL __stdcall
K32GetProcessMemoryInfo(HANDLE Process, ::_PROCESS_MEMORY_COUNTERS* ppsmemCounters, DWORD cb);
__declspec(dllimport) TopLevelExceptionFilter __stdcall
SetUnhandledExceptionFilter(TopLevelExceptionFilter lpTopLevelExceptionFilter);
}

/// The exception code of an unhandled structured exception: the first member of the
/// EXCEPTION_RECORD that the first member of EXCEPTION_POINTERS points to.
inline DWORD unhandled_exception_code(const ::_EXCEPTION_POINTERS* pointers)
{
    const void* const* record = static_cast<const void* const*>(static_cast<const void*>(pointers));
    return *static_cast<const DWORD*>(*record);
}

} // namespace huira::win32

#endif // _WIN32
