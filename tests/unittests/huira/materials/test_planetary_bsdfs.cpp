// The Lommel-Seeliger, McEwen and Hapke BSDFs, against the independent reference in
// reference/photometry.hpp. (test_bsdf_contract.cpp checks their sampling.)

#include <array>
#include <cmath>
#include <limits>
#include <numbers>
#include <vector>

#include "bsdf_testing.hpp"
#include "catch2/catch_test_macros.hpp"
#include "huira/huira.hpp"
#include "reference/photometry.hpp"

using namespace huira;
using namespace huira::test;
namespace ref = reference::photometry;

namespace {

/// Light and viewer from overhead to 1.45 rad (83 degrees), on the same side, across and
/// opposite. The phase angle of the float directions at zero phase is acos(0.99999994), 3e-4 rad,
/// rather than 0, which moves a narrow opposition surge, so zero phase is left out here and
/// checked overhead, where the directions are exact.
std::vector<Angles> angle_sets()
{
    std::vector<Angles> all;
    for (double i : {0.0, 0.3, 0.7, 1.1, 1.45}) {
        for (double e : {0.0, 0.4, 0.9, 1.2, 1.45}) {
            for (double azimuth : {0.0, 0.5, 1.5, 2.5, std::numbers::pi}) {
                if (ref::phase_angle(i, e, azimuth) > 0.05) {
                    all.push_back({i, e, azimuth});
                }
            }
        }
    }
    return all;
}

ShadingParams<RGB> shading(RGB albedo)
{
    ShadingParams<RGB> params;
    params.albedo = albedo;
    return params;
}

bool close(double a, double b, double relative)
{
    return std::abs(a - b) <= relative * std::abs(b);
}

const RGB ALBEDO{0.9f, 0.5f, 0.1f};

/// The radiance factor I/F = pi f cos i of a surface's green channel, seen at zero phase with
/// the light and the viewer at angle i from the normal.
double radiance_factor_at_zero_phase(const BSDF<RGB>& bsdf, double i)
{
    const Interaction<RGB> isect = surface();
    const Vec3<float> w = direction(isect, i, 0.3);
    return std::numbers::pi * bsdf.eval(w, w, isect, shading(ALBEDO))[1] * std::cos(i);
}

} // namespace

TEST_CASE("A Lommel-Seeliger surface reflects albedo / (4 pi (cos i + cos e))", "[materials][bsdf]")
{
    const LommelSeeligerBSDF<RGB> bsdf;
    const Interaction<RGB> isect = surface();
    for (const Angles& a : angle_sets()) {
        const RGB f = bsdf.eval(a.wo(isect), a.wi(isect), isect, shading(ALBEDO));
        INFO("i " << a.i << ", e " << a.e << ", azimuth " << a.azimuth);
        for (std::size_t c = 0; c < RGB::size(); ++c) {
            CHECK(close(f[c], ref::lommel_seeliger(ALBEDO[c], a.i, a.e), 1e-5));
        }
    }
}

TEST_CASE("A Lommel-Seeliger disk at full phase is evenly bright to its limb", "[materials][bsdf]")
{
    // At zero phase (i = e), I/F = w cos i / (4 (2 cos i)) = w / 8 wherever the light falls: the
    // flat disk of dark, dusty bodies, where a Lambertian disk darkens as cos i.
    const LommelSeeligerBSDF<RGB> lommel_seeliger;
    const LambertianBSDF<RGB> lambertian;
    for (double i : {0.0, 0.5, 1.0, 1.4}) {
        INFO("i = e = " << i);
        CHECK(close(radiance_factor_at_zero_phase(lommel_seeliger, i), ALBEDO[1] / 8.0, 1e-4));
        CHECK(close(radiance_factor_at_zero_phase(lambertian, i), ALBEDO[1] * std::cos(i), 1e-4));
    }
}

TEST_CASE("A McEwen surface mixes Lambert's law with Lommel-Seeliger's in the proportion "
          "exp(-g / 60 degrees)",
          "[materials][bsdf]")
{
    // f(i, e, g) = (1 - L(g)) a / pi + L(g) LS(i, e): at fixed i and e, f is a line in L, with
    // the value a / pi at L = 0 and a value proportional to 1 / (cos i + cos e) at L = 1.
    // Through two azimuths, at phase angles g1 and g2, the line gives f at any other.
    const McEwenBSDF<RGB> bsdf;
    const Interaction<RGB> isect = surface();
    const ShadingParams<RGB> params = shading(ALBEDO);
    std::vector<double> at_l1; // f at L = 1, times cos i + cos e
    for (double i : {0.3, 0.8, 1.3}) {
        for (double e : {0.2, 0.9, 1.4}) {
            const Angles a1{i, e, 0.3};
            const Angles a2{i, e, 2.8};
            const double l1 = ref::mcewen_l(ref::phase_angle(i, e, a1.azimuth));
            const double l2 = ref::mcewen_l(ref::phase_angle(i, e, a2.azimuth));
            const double f1 = bsdf.eval(a1.wo(isect), a1.wi(isect), isect, params)[1];
            const double f2 = bsdf.eval(a2.wo(isect), a2.wi(isect), isect, params)[1];
            const double slope = (f2 - f1) / (l2 - l1);
            INFO("i " << i << ", e " << e);

            for (double azimuth : {1.0, 1.7, std::numbers::pi}) {
                const Angles a{i, e, azimuth};
                const double l = ref::mcewen_l(ref::phase_angle(i, e, azimuth));
                const double f = bsdf.eval(a.wo(isect), a.wi(isect), isect, params)[1];
                INFO("azimuth " << azimuth);
                CHECK(close(f, f1 + (l - l1) * slope, 1e-4));
            }
            CHECK(close(f1 - l1 * slope, ref::lambert(ALBEDO[1]), 1e-4));
            at_l1.push_back((f1 + (1.0 - l1) * slope) * (std::cos(i) + std::cos(e)));
        }
    }
    for (double value : at_l1) {
        CHECK(close(value, at_l1.front(), 1e-4));
    }
}

TEST_CASE("A Lommel-Seeliger or McEwen surface reflects nothing toward a viewer below it",
          "[materials][bsdf]")
{
    const LommelSeeligerBSDF<RGB> lommel_seeliger;
    const McEwenBSDF<RGB> mcewen;
    const Interaction<RGB> isect = surface();
    const ShadingParams<RGB> params = shading(ALBEDO);
    const Vec3<float> wi = direction(isect, 0.5, 0.0);
    for (const BSDF<RGB>* bsdf : {static_cast<const BSDF<RGB>*>(&lommel_seeliger),
                                  static_cast<const BSDF<RGB>*>(&mcewen)}) {
        for (double theta : {1.6, 2.5, 3.1}) {
            const Vec3<float> wo = direction(isect, theta, 1.0);
            INFO(bsdf->type() << ", viewer at " << theta << " rad");
            CHECK(bsdf->eval(wo, wi, isect, params) == RGB{0.f});
            CHECK(bsdf->pdf(wo, wi, isect, params) == 0.f);
            CHECK(!bsdf->sample(wo, isect, params, 0.4f, 0.4f).is_valid());
        }
    }
}

TEST_CASE("A Hapke surface reflects as Hapke's model", "[materials][bsdf]")
{
    // Each channel's albedo is its own single scattering albedo, from black to one that absorbs
    // nothing, and the model is not linear in it.
    const RGB albedo{1.f, 0.6f, 0.f};
    const std::array<ref::HapkeParameters, 5> models{{
        {0.0, 0.06, 1.0, 0.3, 0.6}, // a narrow surge, scattering back
        {0.0, 0.4, 0.5, -0.2, 0.2}, // a broad surge, and b < 0: the lobes swap
        {0.0, 0.01, 2.0, 0.8, 1.0}, // a sharp surge, and one strong lobe
        {0.0, 0.0, 1.0, 0.0, 0.0},  // no surge (h = 0), isotropic
        {0.0, 0.2, 0.0, 0.95, 0.5}, // no surge (B0 = 0), sharp lobes both ways
    }};
    const Interaction<RGB> isect = surface();
    for (const ref::HapkeParameters& m : models) {
        const HapkeBSDF<RGB> bsdf(static_cast<float>(m.h),
                                  static_cast<float>(m.b0),
                                  static_cast<float>(m.b),
                                  static_cast<float>(m.c));
        for (const Angles& a : angle_sets()) {
            const RGB f = bsdf.eval(a.wo(isect), a.wi(isect), isect, shading(albedo));
            const double phase = ref::phase_angle(a.i, a.e, a.azimuth);
            INFO("h " << m.h << ", B0 " << m.b0 << ", b " << m.b << ", c " << m.c << "; i " << a.i
                      << ", e " << a.e << ", g " << phase);
            for (std::size_t c = 0; c < RGB::size(); ++c) {
                ref::HapkeParameters channel = m;
                channel.w = albedo[c];
                const double expected = ref::hapke(channel, a.i, a.e, phase);
                CHECK(std::abs(f[c] - expected) <= 1e-4 * expected + 1e-9);
            }
        }
    }
}

TEST_CASE("A Hapke surface brightens by B0 p(0) times Lommel-Seeliger's law at opposition",
          "[materials][bsdf]")
{
    // Overhead, the directions are exact and the phase angle is 0.
    const Interaction<RGB> isect = surface();
    const Vec3<float> overhead = direction(isect, 0.0, 0.0);
    const ShadingParams<RGB> params = shading(ALBEDO);
    const LommelSeeligerBSDF<RGB> lommel_seeliger;
    const float ls = lommel_seeliger.eval(overhead, overhead, isect, params)[1];
    for (float b0 : {0.5f, 1.f, 2.f}) {
        for (float h : {0.01f, 0.06f, 0.4f}) {
            const HapkeBSDF<RGB> surge(h, b0, 0.3f, 0.6f);
            const HapkeBSDF<RGB> none(h, 0.f, 0.3f, 0.6f);
            const float added = surge.eval(overhead, overhead, isect, params)[1] -
                                none.eval(overhead, overhead, isect, params)[1];
            INFO("B0 " << b0 << ", h " << h);
            CHECK(close(added, ls * b0 * ref::double_henyey_greenstein(0.3, 0.6, 0.0), 1e-4));
        }
    }
    // A surge of no width adds nothing, whatever its amplitude:
    const HapkeBSDF<RGB> no_width(0.f, 2.f, 0.3f, 0.6f);
    const HapkeBSDF<RGB> none(0.f, 0.f, 0.3f, 0.6f);
    CHECK(no_width.eval(overhead, overhead, isect, params) ==
          none.eval(overhead, overhead, isect, params));
}

TEST_CASE("A Hapke surface with black particles reflects nothing, and one with isotropic, faint "
          "particles is Lommel-Seeliger's",
          "[materials][bsdf]")
{
    // With w = 0 there is nothing to scatter; as w goes to 0 with p = 1 and no surge, the H
    // functions go to 1 and only single scattering is left.
    const HapkeBSDF<RGB> hapke(0.06f, 0.f, 0.f, 0.5f);
    const LommelSeeligerBSDF<RGB> lommel_seeliger;
    const Interaction<RGB> isect = surface();
    for (const Angles& a : angle_sets()) {
        INFO("i " << a.i << ", e " << a.e << ", azimuth " << a.azimuth);
        CHECK(hapke.eval(a.wo(isect), a.wi(isect), isect, shading(RGB{0.f})) == RGB{0.f});
        const ShadingParams<RGB> faint = shading(RGB{1e-5f});
        CHECK(close(hapke.eval(a.wo(isect), a.wi(isect), isect, faint)[1],
                    lommel_seeliger.eval(a.wo(isect), a.wi(isect), isect, faint)[1],
                    1e-4));
    }
}

TEST_CASE("Hapke's parameters are checked when the BSDF is made", "[materials][bsdf]")
{
    constexpr float NaN = std::numeric_limits<float>::quiet_NaN();
    constexpr float INF = std::numeric_limits<float>::infinity();

    // h and B0 are finite and non-negative:
    CHECK_THROWS(HapkeBSDF<RGB>(-0.01f, 1.f, 0.3f, 0.5f));
    CHECK_THROWS(HapkeBSDF<RGB>(NaN, 1.f, 0.3f, 0.5f));
    CHECK_THROWS(HapkeBSDF<RGB>(INF, 1.f, 0.3f, 0.5f));
    CHECK_THROWS(HapkeBSDF<RGB>(0.06f, -1.f, 0.3f, 0.5f));
    CHECK_THROWS(HapkeBSDF<RGB>(0.06f, NaN, 0.3f, 0.5f));
    CHECK_THROWS(HapkeBSDF<RGB>(0.06f, INF, 0.3f, 0.5f));
    // b is within (-1, 1), where the lobes are finite:
    CHECK_THROWS(HapkeBSDF<RGB>(0.06f, 1.f, 1.f, 0.5f));
    CHECK_THROWS(HapkeBSDF<RGB>(0.06f, 1.f, -1.f, 0.5f));
    CHECK_THROWS(HapkeBSDF<RGB>(0.06f, 1.f, NaN, 0.5f));
    // c is from 0 to 1:
    CHECK_THROWS(HapkeBSDF<RGB>(0.06f, 1.f, 0.3f, -0.1f));
    CHECK_THROWS(HapkeBSDF<RGB>(0.06f, 1.f, 0.3f, 1.1f));
    CHECK_THROWS(HapkeBSDF<RGB>(0.06f, 1.f, 0.3f, NaN));

    // and the limits themselves are allowed:
    CHECK_NOTHROW(HapkeBSDF<RGB>(0.f, 0.f, 0.f, 0.f));
    CHECK_NOTHROW(HapkeBSDF<RGB>(1e3f, 1e3f, 0.999f, 1.f));
    CHECK_NOTHROW(HapkeBSDF<RGB>(0.06f, 1.f, -0.999f, 0.5f));

    // as they are through the scene:
    Scene<RGB> scene;
    CHECK_THROWS(scene.new_bsdf_hapke(0.06f, -1.f, 0.3f, 0.5f));
    CHECK(scene.new_bsdf_hapke(0.06f, 1.f, 0.3f, 0.5f).get()->type() == "HapkeBSDF");
}
