// Huira is header-only, so a program may include it from as many of its source files as it
// likes. For that, everything its headers define must be a template, a member function defined
// in its class, constexpr, or inline: anything else is defined again in each source file that
// includes it, and the program doesn't link. This test is two source files that include all of
// Huira, linked into one program: this one and translation_units_other.cpp.

#include "catch2/catch_test_macros.hpp"
#include "huira/huira.hpp"
#include "huira/stars/io/load_tycho2.hpp"
#include "translation_units_other.hpp"

TEST_CASE("Huira can be included from more than one source file", "[build]")
{
    // Linking is the test. Calling into the other file keeps it in the program:
    CHECK(huira::test::rgb_bins_from_another_source_file() == huira::RGB::size());
}
