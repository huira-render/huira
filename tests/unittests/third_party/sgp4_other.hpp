#pragma once

#include <array>

namespace huira::test {

/// The ISS's position in TEME (km), at the epoch of an element set of 2008, from the vendored SGP4
/// in sgp4_other.cpp: the second source file of test_sgp4.
std::array<double, 3> iss_position_from_another_source_file();

} // namespace huira::test
