#pragma once

// Vallado's reference SGP4, vendored unmodified in huira_impl/third_party/sgp4/ (see the NOTICE
// there), made includable from Huira's headers. Its functions are in
// huira::detail::vallado::SGP4Funcs, with elsetrec and gravconsttype beside them in
// huira::detail::vallado.
//
// SGP4.cpp can't be included from a header as it is: it defines non-static globals (`help`,
// `dbgfile`) and non-inline functions, which every source file that included it would define
// again, and it defines a macro `pi`, which would break std::numbers::pi and any other `pi` after
// it. So it is included in an unnamed namespace, which gives every definition in it internal
// linkage, and `pi` is undefined afterwards. Its warnings are silenced, here only.
//
// test_sgp4 checks it against Vallado's own verification cases and output, and includes it from
// two source files.

// Everything SGP4.h and SGP4.cpp include, first: their own #includes, in the namespace below, then
// find these already included instead of opening a standard header inside a namespace.
#include <cmath>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <string>

#include <math.h>
#include <stdio.h>
#include <string.h>

#if defined(_MSC_VER) && !defined(__clang__)
#pragma warning(push, 0)
#elif defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Weverything"
#elif defined(__GNUC__)
// GCC can't turn off a group of warnings (-Wall, -Wextra) by pragma, so each one is listed
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
#pragma GCC diagnostic ignored "-Wconversion"
#pragma GCC diagnostic ignored "-Wsign-conversion"
#pragma GCC diagnostic ignored "-Wsign-compare"
#pragma GCC diagnostic ignored "-Wshadow"
#pragma GCC diagnostic ignored "-Wold-style-cast"
#pragma GCC diagnostic ignored "-Wuseless-cast"
#pragma GCC diagnostic ignored "-Wcast-qual"
#pragma GCC diagnostic ignored "-Wcast-align"
#pragma GCC diagnostic ignored "-Wunused"
#pragma GCC diagnostic ignored "-Wunused-function"
#pragma GCC diagnostic ignored "-Wunused-variable"
#pragma GCC diagnostic ignored "-Wunused-parameter"
#pragma GCC diagnostic ignored "-Wunused-but-set-variable"
#pragma GCC diagnostic ignored "-Wunused-but-set-parameter"
#pragma GCC diagnostic ignored "-Wunused-value"
#pragma GCC diagnostic ignored "-Wunused-result"
#pragma GCC diagnostic ignored "-Wmissing-declarations"
#pragma GCC diagnostic ignored "-Wredundant-decls"
#pragma GCC diagnostic ignored "-Wdouble-promotion"
#pragma GCC diagnostic ignored "-Wfloat-equal"
#pragma GCC diagnostic ignored "-Wformat"
#pragma GCC diagnostic ignored "-Wformat="
#pragma GCC diagnostic ignored "-Wformat-nonliteral"
#pragma GCC diagnostic ignored "-Wformat-overflow"
#pragma GCC diagnostic ignored "-Wswitch-default"
#pragma GCC diagnostic ignored "-Wdangling-else"
#pragma GCC diagnostic ignored "-Wparentheses"
#pragma GCC diagnostic ignored "-Wmisleading-indentation"
#pragma GCC diagnostic ignored "-Wimplicit-fallthrough"
#pragma GCC diagnostic ignored "-Wunreachable-code"
#pragma GCC diagnostic ignored "-Wlogical-op"
#pragma GCC diagnostic ignored "-Wduplicated-cond"
#pragma GCC diagnostic ignored "-Wduplicated-branches"
#pragma GCC diagnostic ignored "-Wnull-dereference"
#pragma GCC diagnostic ignored "-Wstrict-overflow"
#pragma GCC diagnostic ignored "-Wundef"
#pragma GCC diagnostic ignored "-Wuninitialized"
#pragma GCC diagnostic ignored "-Wmaybe-uninitialized"
#pragma GCC diagnostic ignored "-Wstringop-overflow"
#pragma GCC diagnostic ignored "-Wrestrict" // sgp4init() strcpy()s satrec.satnum onto itself
#endif

namespace huira::detail::vallado {
namespace {
#include "huira_impl/third_party/sgp4/SGP4.cpp" // which includes SGP4.h
} // namespace
} // namespace huira::detail::vallado

#undef pi

#if defined(_MSC_VER) && !defined(__clang__)
#pragma warning(pop)
#elif defined(__clang__)
#pragma clang diagnostic pop
#elif defined(__GNUC__)
#pragma GCC diagnostic pop
#endif
