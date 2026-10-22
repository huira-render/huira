// The second source file of test_sgp4: see test_sgp4.cpp.

#include "sgp4_other.hpp"

#include "huira_impl/third_party/sgp4.ipp"

namespace huira::test {

std::array<double, 3> iss_position_from_another_source_file()
{
    using namespace huira::detail::vallado;

    char line1[130] = "1 25544U 98067A   08264.51782528 -.00002182  00000-0 -11606-4 0  2927";
    char line2[130] = "2 25544  51.6416 247.4627 0006703 130.5360 325.0288 15.72125391563537";
    double startmfe = 0.0;
    double stopmfe = 0.0;
    double deltamin = 0.0;
    elsetrec satrec{};
    SGP4Funcs::twoline2rv(line1, line2, 'c', 'e', 'a', wgs72, startmfe, stopmfe, deltamin, satrec);

    double r[3] = {};
    double v[3] = {};
    SGP4Funcs::sgp4(satrec, 0.0, r, v);
    return {r[0], r[1], r[2]};
}

} // namespace huira::test
