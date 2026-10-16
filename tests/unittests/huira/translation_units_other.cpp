// The second source file of test_translation_units: see test_translation_units.cpp.

#include "translation_units_other.hpp"

#include "huira/huira.hpp"
#include "huira/stars/io/load_tycho2.hpp"

namespace huira::test {

std::size_t rgb_bins_from_another_source_file()
{
    return huira::RGB::size();
}

} // namespace huira::test
