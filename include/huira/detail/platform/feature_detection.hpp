#pragma once

// Check for C++20 library feature: utc_clock
#if defined(__cpp_lib_chrono) && __cpp_lib_chrono >= 201907L
#if defined(__GNUC__) && !defined(__clang__) && __GNUC__ < 13
  // GCC < 13 has incomplete utc_clock implementation
#define HUIRA_HAS_UTC_CLOCK 0
#else
#define HUIRA_HAS_UTC_CLOCK 1
#endif
#else
  // No __cpp_lib_chrono or version too old - use compiler/stdlib heuristics
#if defined(_MSC_VER) && _MSC_VER >= 1933
#define HUIRA_HAS_UTC_CLOCK 1
#elif defined(__GLIBCXX__) && defined(__GNUC__) && __GNUC__ >= 13
#define HUIRA_HAS_UTC_CLOCK 1
#elif defined(_LIBCPP_VERSION) && _LIBCPP_VERSION >= 15000
#define HUIRA_HAS_UTC_CLOCK 1
#else
#define HUIRA_HAS_UTC_CLOCK 0
#endif
#endif
