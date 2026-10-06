// Checks of the independent reference that other tests compare the renderer's point spread
// function with. Each check rests on a closed form, an identity or a brute-force computation that
// the reference does not use itself.

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>

#include "catch2/catch_test_macros.hpp"
#include "reference/airy.hpp"

namespace {

using reference::PI;

/// Cutoff frequencies p / (lambda N) of a band, in cycles per pixel.
struct Band {
    double blue;
    double red;
};

/// A narrow band, 400 to 437.5 nm with 6.8 cycles per pixel at 400 nm (the bluest of eight bins
/// across the visible, behind optics that resolve the pattern finely), and a band an octave wide.
constexpr std::array<Band, 2> BANDS{Band{6.8, 6.8 * 400.0 / 437.5}, Band{4.0, 2.0}};

struct BesselValues {
    double x;
    std::array<double, 3> j; // J0, J1, J2
};

/// From mpmath at 40 digits.
constexpr std::array<BesselValues, 11> TABULATED{{
    {0.5, {0.9384698072408129, 0.24226845767487389, 0.030604023458682641}},
    {1.0, {0.76519768655796655, 0.44005058574493352, 0.11490348493190048}},
    {2.5, {-0.048383776468197996, 0.49709410246427404, 0.44605905843961723}},
    {5.0, {-0.1775967713143383, -0.32757913759146522, 0.046565116277752216}},
    {10.0, {-0.24593576445134834, 0.043472746168861437, 0.25463031368512062}},
    {24.9, {0.083245968353015682, -0.13485569953140874, -0.09407775144790795}},
    {25.1, {0.10827567149994929, -0.11463478413442273, -0.11740991724771206}},
    {50.0, {0.055812327669251815, -0.097511828125175138, -0.059712800794258821}},
    {100.0, {0.019985850304223122, -0.077145352014112158, -0.021528757344505366}},
    {1000.0, {0.024786686152420175, 0.0047283119070895239, -0.024777229528605996}},
    {4000.0, {-0.012608844878571356, 0.00041311978215597679, 0.012609051438462434}},
}};

/// The band-averaged pattern by brute force: the single-wavelength pattern pi c^2 (J1(x) / x)^2,
/// x = pi r c, averaged evenly over wavelength, which is 1/c, on panels short enough to follow
/// its oscillation.
double average_over_wavelength(const Band& band, double r)
{
    const double u_blue = 1.0 / band.blue;
    const double u_red = 1.0 / band.red;
    const int panels =
        8 + static_cast<int>(std::ceil(8.0 * r * band.blue * (band.blue - band.red) / band.red));
    const auto single = [r](double u) {
        const double c = 1.0 / u;
        return PI * c * c * reference::airy_shape(PI * r * c);
    };
    return reference::gauss_legendre(single, u_blue, u_red, panels) / (u_red - u_blue);
}

/// erf(b) - erf(a) for a < b, without cancellation in the tails.
double erf_difference(double a, double b)
{
    if (a >= 0.0) {
        return std::erfc(a) - std::erfc(b);
    }
    if (b <= 0.0) {
        return std::erfc(-b) - std::erfc(-a);
    }
    return std::erf(b) - std::erf(a);
}

/// Light of a radial density in a rectangle by Gauss-Legendre over x and y in turn, the panels
/// short enough to follow the rings.
template <class Density>
double light_by_area(const Density& density, double x0, double x1, double y0, double y1)
{
    constexpr int PANELS = 32;
    const auto row = [&](double y) {
        const auto point = [&](double x) { return density(std::hypot(x, y)); };
        return reference::gauss_legendre(point, x0, x1, PANELS);
    };
    return reference::gauss_legendre(row, y0, y1, PANELS);
}

/// Sources at the centre of a pixel, on its corner, on its edge, at general positions and just
/// inside its edges, as offsets from the centre in pixels.
constexpr std::array<std::array<double, 2>, 7> SOURCES{{{0.0, 0.0},
                                                        {0.5, 0.5},
                                                        {0.5, 0.0},
                                                        {0.37, -0.21},
                                                        {-0.13, 0.49},
                                                        {0.4999999, 0.49},
                                                        {0.0001, -0.4993}}};

} // namespace

TEST_CASE("Reference Bessel functions match tabulated values and zeros", "[reference]")
{
    for (const auto& values : TABULATED) {
        for (std::size_t n = 0; n < values.j.size(); ++n) {
            INFO("J" << n << "(" << values.x << ")");
            const double j = reference::bessel_j(static_cast<int>(n), values.x);
            CHECK(std::abs(j - values.j[n]) < 1e-15);
        }
    }

    // Zeros from mpmath; each is within half a unit in the last place, where J changes by
    // under 3e-16.
    for (double zero : {2.4048255576957728, 5.5200781102863106, 8.6537279129110122}) {
        CHECK(std::abs(reference::bessel_j(0, zero)) < 1e-15);
    }
    for (double zero : {3.8317059702075123, 7.0155866698156188, 10.173468135062722}) {
        CHECK(std::abs(reference::bessel_j(1, zero)) < 1e-15);
    }
    for (double zero : {5.1356223018406826, 8.4172441403998649}) {
        CHECK(std::abs(reference::bessel_j(2, zero)) < 1e-15);
    }
}

TEST_CASE("Bessel's integral and Hankel's expansion agree where both apply", "[reference]")
{
    double worst = 0.0;
    for (int n = 0; n < 3; ++n) {
        for (double x = 25.0; x < 400.0; x += 0.73) {
            const double integral = reference::detail::bessel_integral(n, x);
            const double hankel = reference::detail::bessel_hankel(n, x);
            worst = std::max(worst, std::abs(integral - hankel));
        }
    }
    CHECK(worst < 1e-14);
}

TEST_CASE("Reference Bessel functions give Rayleigh's encircled energy", "[reference]")
{
    // 2 int_0^x J1(t)^2 / t dt = 1 - J0(x)^2 - J1(x)^2 ties J0 and J1 together at every x.
    const auto integrand = [](double t) {
        const double j1 = reference::bessel_j(1, t);
        return 2.0 * j1 * j1 / t;
    };
    for (double x : {1.0, 3.8317059702075123, 10.0, 24.5, 100.0, 1000.0}) {
        const int panels = 4 + static_cast<int>(std::ceil(4.0 * x));
        const double inside = reference::gauss_legendre(integrand, 0.0, x, panels);
        const double j0 = reference::bessel_j(0, x);
        const double j1 = reference::bessel_j(1, x);
        INFO("x = " << x);
        CHECK(std::abs(inside - (1.0 - j0 * j0 - j1 * j1)) < 1e-14);
    }
}

TEST_CASE("The reference's Airy tail integral", "[reference]")
{
    const reference::AiryTail& tail = reference::airy_tail();

    SECTION("All of it is 4 / (3 pi)")
    {
        CHECK(std::abs(tail(0.0) - 4.0 / (3.0 * PI)) < 1e-15);
    }

    SECTION("Between nodes it matches direct integration")
    {
        // Intervals that start and end between the table's nodes, across the whole table.
        for (double x = 0.011; x < reference::AiryTail::END; x *= 1.37) {
            const double width = 0.29;
            const double direct = reference::gauss_legendre(reference::airy_shape, x, x + width, 4);
            const double tabulated = tail(x) - tail(x + width);
            INFO("x = " << x);
            CHECK(std::abs(tabulated - direct) < 3e-13 * tail(x));
        }
    }

    SECTION("Far out it meets its asymptotic form")
    {
        // The asymptotic form's error oscillates within 0.36 / x^5, which the table must follow.
        // The table starts from the asymptotic form at its end, so the check stops at half its
        // length, where the error of that start adds at most 1/32 of the error checked.
        for (double x = 100.0; x < 0.5 * reference::AiryTail::END; x *= 1.19) {
            const double error = tail(x) - reference::airy_tail_asymptotic(x);
            INFO("x = " << x);
            CHECK(std::abs(error) * std::pow(x, 5) < 0.4);
        }
    }
}

TEST_CASE("The reference's band average is the average over wavelength", "[reference]")
{
    for (const Band& band : BANDS) {
        const reference::AiryBand airy(band.blue, band.red);
        INFO("band " << band.blue << " to " << band.red << " cycles per pixel");
        // Within 3 pixels the pattern has dark rings, where its value is no measure of the error,
        // so there the error is compared with the peak.
        for (double r = 0.0; r < 3.0; r += 0.0137) {
            INFO("r = " << r);
            CHECK(std::abs(airy(r) - average_over_wavelength(band, r)) < 1e-12 * airy.peak());
        }
        // Farther out, across the table's end at x = 4096 and beyond it.
        for (double r = 3.0; r < 2000.0; r *= 1.07) {
            const double brute_force = average_over_wavelength(band, r);
            INFO("r = " << r);
            CHECK(std::abs(airy(r) - brute_force) < 1e-9 * brute_force);
        }
    }
}

TEST_CASE("The reference's band-averaged pattern holds all the light", "[reference]")
{
    for (const Band& band : BANDS) {
        const reference::AiryBand airy(band.blue, band.red);
        for (double radius : {0.5, 3.0, 40.0}) {
            const auto ring = [&](double r) { return 2.0 * PI * r * airy(r); };
            const int panels = static_cast<int>(std::ceil(radius / 0.01));
            const double inside = reference::gauss_legendre(ring, 0.0, radius, panels);
            INFO("band " << band.blue << " to " << band.red << ", radius " << radius);
            CHECK(std::abs(inside + airy.light_beyond(radius) - 1.0) < 5e-14);
        }
    }
}

TEST_CASE("Reference pixel light of a uniform density is the pixel's area", "[reference]")
{
    const auto uniform = [](double) { return 1.0; };
    for (const auto& source : SOURCES) {
        for (int j = -3; j <= 3; ++j) {
            for (int i = -3; i <= 3; ++i) {
                INFO("pixel (" << i << ", " << j << "), source at (" << source[0] << ", "
                               << source[1] << ")");
                const double light = reference::pixel_light(uniform, i - source[0], j - source[1]);
                CHECK(std::abs(light - 1.0) < 1e-14);
            }
        }
    }
    // A rectangle across both axes, and one far from the source.
    CHECK(std::abs(reference::rectangle_light(uniform, -2.0, 3.0, -0.1, 0.4) - 2.5) < 1e-14);
    CHECK(std::abs(reference::rectangle_light(uniform, 40.0, 41.5, -7.0, -6.5) - 0.75) < 1e-14);
}

TEST_CASE("Reference pixel light of a Gaussian matches error functions", "[reference]")
{
    // A circular Gaussian separates into x and y, so its light in a rectangle is a product of
    // differences of error functions.
    for (double sigma : {0.25, 0.7, 2.0}) {
        const auto gaussian = [sigma](double r) {
            return std::exp(-0.5 * r * r / (sigma * sigma)) / (2.0 * PI * sigma * sigma);
        };
        const double scale = 1.0 / (std::sqrt(2.0) * sigma);
        for (const auto& source : SOURCES) {
            for (int j = -4; j <= 4; ++j) {
                for (int i = -4; i <= 4; ++i) {
                    const double x0 = i - source[0] - 0.5;
                    const double y0 = j - source[1] - 0.5;
                    const double exact = 0.25 * erf_difference(x0 * scale, (x0 + 1.0) * scale) *
                                         erf_difference(y0 * scale, (y0 + 1.0) * scale);
                    const double light =
                        reference::rectangle_light(gaussian, x0, x0 + 1.0, y0, y0 + 1.0);
                    INFO("sigma " << sigma << ", pixel (" << i << ", " << j << "), source at ("
                                  << source[0] << ", " << source[1] << ")");
                    CHECK(std::abs(light - exact) < 1e-15 + 1e-13 * exact);
                }
            }
        }
    }
}

TEST_CASE("Reference pixel light of the Airy pattern matches integration over area", "[reference]")
{
    // The pattern is smooth across every pixel, the one holding the source included, so
    // Gauss-Legendre over x and y converges quickly and checks the arc-length method.
    const reference::AiryBand airy(BANDS[0].blue, BANDS[0].red);
    for (const auto& source : SOURCES) {
        for (const auto& pixel : {std::array<int, 2>{0, 0}, {1, 0}, {2, -3}, {-7, 5}}) {
            const double x0 = pixel[0] - source[0] - 0.5;
            const double y0 = pixel[1] - source[1] - 0.5;
            const double by_area = light_by_area(airy, x0, x0 + 1.0, y0, y0 + 1.0);
            const double light = reference::rectangle_light(airy, x0, x0 + 1.0, y0, y0 + 1.0);
            INFO("pixel (" << pixel[0] << ", " << pixel[1] << "), source at (" << source[0] << ", "
                           << source[1] << ")");
            CHECK(std::abs(light - by_area) < 1e-13 * by_area);
        }
    }
}

TEST_CASE("Reference pixel light adds up and keeps the pattern's symmetry", "[reference]")
{
    const reference::AiryBand airy(BANDS[0].blue, BANDS[0].red);
    const double u = 0.37;
    const double v = -0.21;
    constexpr int RADIUS = 6;

    // A stamp's pixels hold what the whole stamp holds, which lies between the light within the
    // largest circle about the source inside the stamp and the smallest outside it.
    double stamp = 0.0;
    for (int j = -RADIUS; j <= RADIUS; ++j) {
        for (int i = -RADIUS; i <= RADIUS; ++i) {
            stamp += reference::pixel_light(airy, i - u, j - v);
        }
    }
    const double edge = RADIUS + 0.5;
    const double whole = reference::rectangle_light(airy, -edge - u, edge - u, -edge - v, edge - v);
    CHECK(std::abs(stamp - whole) < 1e-13);
    const double inner = edge - std::max(std::abs(u), std::abs(v));
    const double outer = std::hypot(edge + std::abs(u), edge + std::abs(v));
    CHECK(whole > 1.0 - airy.light_beyond(inner));
    CHECK(whole < 1.0 - airy.light_beyond(outer));

    // Reflections and the swap of x and y leave a pixel's light unchanged.
    for (const auto& offset : {std::array<double, 2>{0.37, 1.21}, {2.5, -0.5}, {-3.9, 4.4}}) {
        const double light = reference::pixel_light(airy, offset[0], offset[1]);
        CHECK(std::abs(reference::pixel_light(airy, -offset[0], offset[1]) - light) <
              1e-14 * light);
        CHECK(std::abs(reference::pixel_light(airy, offset[0], -offset[1]) - light) <
              1e-14 * light);
        CHECK(std::abs(reference::pixel_light(airy, offset[1], offset[0]) - light) < 1e-14 * light);
    }
}
