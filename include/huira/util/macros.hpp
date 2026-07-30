#pragma once

/**
 * @file macros.hpp
 * @brief Compiler-portability helper macros shared across huira headers.
 */

// Portable wrapper for Clang's __has_warning() so the checks below are
// harmless on GCC/MSVC, which do not define it.
#if defined(__clang__) && defined(__has_warning)
#define HUIRA_HAS_WARNING(warning) __has_warning(warning)
#else
#define HUIRA_HAS_WARNING(warning) 0
#endif

/**
 * @brief Suppress Clang's -Wunique-object-duplication around intentional
 *        per-module state.
 *
 * huira is a header-only library, so mutable objects defined in headers
 * (logger singleton, FFTW plan cache, SPICE init flags, etc.) receive one
 * instance per linked module (EXE/DLL) on Windows, where the dynamic linker
 * never deduplicates them. Clang >= 21 warns about this on Windows targets.
 *
 * This is accepted behavior: huira does not support sharing its internal
 * state across DLL boundaries. Each object below is either idempotent to
 * duplicate or scoped to a single module by design. These macros silence the
 * warning at the declaration sites so the suppression travels with the
 * headers to downstream consumers, whatever warning flags they build with.
 */
#if HUIRA_HAS_WARNING("-Wunique-object-duplication")
#define HUIRA_PER_MODULE_STATE_BEGIN                                                               \
    _Pragma("clang diagnostic push")                                                               \
        _Pragma("clang diagnostic ignored \"-Wunique-object-duplication\"")
#define HUIRA_PER_MODULE_STATE_END _Pragma("clang diagnostic pop")
#else
#define HUIRA_PER_MODULE_STATE_BEGIN
#define HUIRA_PER_MODULE_STATE_END
#endif
