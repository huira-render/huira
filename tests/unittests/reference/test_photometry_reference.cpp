// Checks of the independent photometric reference that the BSDF tests compare huira's BSDFs with.
// Each check rests on a closed form, an identity or a direct computation that the reference does
// not use itself.

#include <cmath>

#include "catch2/catch_test_macros.hpp"
#include "reference/photometry.hpp"

using namespace reference::photometry;

namespace {

/// The unit vector at angle theta from the normal (+z), at the given azimuth around it.
struct Direction {
    double x;
    double y;
    double z;
};

Direction direction(double theta, double azimuth)
{
    return {
        std::sin(theta) * std::cos(azimuth), std::sin(theta) * std::sin(azimuth), std::cos(theta)};
}

/// The integral of fn over [a, b] by Simpson's rule on n (even) intervals.
template <class F>
double simpson(const F& fn, double a, double b, int n)
{
    const double h = (b - a) / n;
    double sum = fn(a) + fn(b);
    for (int k = 1; k < n; ++k) {
        sum += fn(a + k * h) * (k % 2 == 1 ? 4.0 : 2.0);
    }
    return sum * h / 3.0;
}

/// The light a surface reflects from a unit irradiance at incidence i: the integral of
/// f cos(e) over the hemisphere of emission directions, on a grid in cos(e) and azimuth.
template <class Brdf>
double directional_albedo(const Brdf& brdf, double i)
{
    constexpr int RINGS = 400;
    constexpr int SPOKES = 200;
    double total = 0.0;
    for (int r = 0; r < RINGS; ++r) {
        const double mu = (r + 0.5) / RINGS;
        const double e = std::acos(mu);
        for (int s = 0; s < SPOKES; ++s) {
            const double azimuth = (s + 0.5) * 2.0 * PI / SPOKES;
            total += brdf(i, e, azimuth) * mu;
        }
    }
    return total * (1.0 / RINGS) * (2.0 * PI / SPOKES);
}

bool close(double a, double b, double relative)
{
    return std::abs(a - b) <= relative * std::abs(b);
}

} // namespace

TEST_CASE("The reference's phase angle is the angle between the two directions", "[reference]")
{
    for (double i : {0.0, 0.3, 1.0, 1.5}) {
        for (double e : {0.0, 0.4, 1.2}) {
            for (double azimuth : {0.0, 0.7, 2.0, PI}) {
                const Direction light = direction(i, 0.0);
                const Direction eye = direction(e, azimuth);
                const double dot = light.x * eye.x + light.y * eye.y + light.z * eye.z;
                INFO("i " << i << ", e " << e << ", azimuth " << azimuth);
                CHECK(std::abs(phase_angle(i, e, azimuth) -
                               std::acos(std::max(-1.0, std::min(1.0, dot)))) < 1e-7);
            }
        }
    }
}

TEST_CASE("The reference's double Henyey-Greenstein phase function averages to 1 over the sphere",
          "[reference]")
{
    // (1/2) int_0^pi p(g) sin(g) dg = 1, in cos(g), where a strongly peaked lobe is smooth:
    for (double b : {0.0, 0.2, 0.5, 0.8}) {
        for (double c : {0.0, 0.3, 1.0}) {
            const double mean =
                0.5 *
                simpson([&](double x) { return double_henyey_greenstein(b, c, std::acos(x)); },
                        -1.0,
                        1.0,
                        20000);
            INFO("b " << b << ", c " << c << ": mean " << mean);
            CHECK(close(mean, 1.0, 1e-6));
        }
    }
    // c weights the lobe that scatters back toward the light:
    CHECK(double_henyey_greenstein(0.5, 1.0, 0.0) > double_henyey_greenstein(0.5, 1.0, PI));
    CHECK(double_henyey_greenstein(0.5, 0.0, 0.0) < double_henyey_greenstein(0.5, 0.0, PI));
}

TEST_CASE("The reference's opposition surge and H function have their defining values",
          "[reference]")
{
    // B is B0 at opposition and half of it where tan(g/2) = h, its half width:
    CHECK(close(hapke_opposition(1.2, 0.06, 0.0), 1.2, 1e-12));
    CHECK(close(hapke_opposition(1.2, 0.06, 2.0 * std::atan(0.06)), 0.6, 1e-12));
    // H is 1 for black particles and 1 + 2x for ones that absorb nothing:
    for (double x : {0.0, 0.3, 1.0}) {
        CHECK(close(hapke_h(x, 0.0), 1.0, 1e-12));
        CHECK(close(hapke_h(x, 1.0), 1.0 + 2.0 * x, 1e-12));
    }
}

TEST_CASE("The reference's diffuse laws reflect what their closed forms say", "[reference]")
{
    // Lommel-Seeliger: int f cos(e) dw = (w/2) (1 - mu0 ln((1 + mu0) / mu0)), mu0 = cos i.
    for (double i : {0.0, 0.6, 1.3}) {
        const double mu0 = std::cos(i);
        const double expected = 0.5 * 0.7 * (1.0 - mu0 * std::log((1.0 + mu0) / mu0));
        const double albedo = directional_albedo(
            [](double i_, double e, double) { return lommel_seeliger(0.7, i_, e); }, i);
        INFO("Lommel-Seeliger at incidence " << i << ": " << albedo << ", against " << expected);
        CHECK(close(albedo, expected, 1e-4));
    }

    // Oren-Nayar: at normal incidence tan(min(i, e)) = 0, so it reflects a A, and at sigma = 0
    // it is Lambert's law.
    const double sigma = 0.6;
    const double a = 1.0 - 0.5 * sigma * sigma / (sigma * sigma + 0.33);
    const double albedo = directional_albedo(
        [&](double i, double e, double azimuth) { return oren_nayar(0.8, sigma, i, e, azimuth); },
        0.0);
    CHECK(close(albedo, 0.8 * a, 1e-4));
    CHECK(close(oren_nayar(0.8, 0.0, 0.7, 1.1, 0.4), lambert(0.8), 1e-12));
    // and it is reciprocal:
    CHECK(close(
        oren_nayar(0.8, sigma, 0.7, 1.1, 0.4), oren_nayar(0.8, sigma, 1.1, 0.7, -0.4), 1e-12));
}

TEST_CASE("The reference's lunar-Lambert function mixes Lambert's and Lommel-Seeliger's laws",
          "[reference]")
{
    // L = 0 is Lambert's law, L = 1 Lommel-Seeliger's scaled to Lambert's at i = e = 0, and with
    // the light and the observer both overhead it is Lambert's law for any L:
    CHECK(close(lunar_lambert(0.3, 0.0, 0.7, 1.1), lambert(0.3), 1e-12));
    CHECK(close(
        lunar_lambert(0.3, 1.0, 0.7, 1.1), 8.0 * 0.3 * lommel_seeliger(1.0, 0.7, 1.1), 1e-12));
    for (double l : {0.0, 0.4, 1.0}) {
        CHECK(close(lunar_lambert(0.3, l, 0.0, 0.0), lambert(0.3), 1e-12));
    }
    // McEwen's L falls by e every 60 degrees of phase:
    CHECK(close(mcewen_l(0.0), 1.0, 1e-12));
    CHECK(close(mcewen_l(PI / 3.0), std::exp(-1.0), 1e-12));
}

TEST_CASE("The reference's Hapke model reduces to its parts", "[reference]")
{
    // Without an opposition surge and with isotropic particles (b = 0), only the H functions
    // remain: f = LS (H(cos i) H(cos e)), which for w = 1 is LS (1 + 2 cos i)(1 + 2 cos e).
    const HapkeParameters isotropic{1.0, 0.06, 0.0, 0.0, 0.5};
    const double i = 0.5;
    const double e = 1.0;
    const double g = phase_angle(i, e, 2.0);
    CHECK(close(hapke(isotropic, i, e, g),
                lommel_seeliger(1.0, i, e) * (1.0 + 2.0 * std::cos(i)) * (1.0 + 2.0 * std::cos(e)),
                1e-12));

    // The surge adds B0 p(0) times Lommel-Seeliger's law at opposition:
    const HapkeParameters surge{0.4, 0.06, 1.5, 0.3, 0.6};
    HapkeParameters flat = surge;
    flat.b0 = 0.0;
    CHECK(close(hapke(surge, i, i, 0.0) - hapke(flat, i, i, 0.0),
                lommel_seeliger(0.4, i, i) * 1.5 * double_henyey_greenstein(0.3, 0.6, 0.0),
                1e-12));
}
