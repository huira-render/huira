// The Lambertian and Oren-Nayar BSDFs, against the independent reference in
// reference/photometry.hpp. (test_bsdf_contract.cpp checks their sampling.)

#include <cmath>
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

/// Light and viewer from overhead to 1.45 rad (83 degrees), on the same side, across and opposite.
std::vector<Angles> angle_sets()
{
    std::vector<Angles> all;
    for (double i : {0.0, 0.3, 0.7, 1.1, 1.45}) {
        for (double e : {0.0, 0.4, 0.9, 1.2, 1.45}) {
            for (double azimuth : {0.0, 0.5, 1.5, 2.5, std::numbers::pi}) {
                all.push_back({i, e, azimuth});
            }
        }
    }
    return all;
}

ShadingParams<RGB> shading(RGB albedo, float roughness)
{
    ShadingParams<RGB> params;
    params.albedo = albedo;
    params.roughness = roughness;
    return params;
}

bool close(double a, double b, double relative)
{
    return std::abs(a - b) <= relative * std::abs(b);
}

const RGB ALBEDO{0.9f, 0.5f, 0.1f};

} // namespace

TEST_CASE("A Lambertian surface reflects albedo / pi in every direction", "[materials][bsdf]")
{
    const LambertianBSDF<RGB> bsdf;
    const Interaction<RGB> isect = surface();
    const ShadingParams<RGB> params = shading(ALBEDO, 0.5f);
    for (const Angles& a : angle_sets()) {
        const RGB f = bsdf.eval(a.wo(isect), a.wi(isect), isect, params);
        INFO("i " << a.i << ", e " << a.e << ", azimuth " << a.azimuth);
        for (std::size_t c = 0; c < RGB::size(); ++c) {
            CHECK(close(f[c], ref::lambert(ALBEDO[c]), 1e-6));
        }
        // and its pdf is the cosine-weighted hemisphere's:
        CHECK(close(bsdf.pdf(a.wo(isect), a.wi(isect), isect, params),
                    std::cos(a.i) / std::numbers::pi,
                    1e-5));
    }
}

TEST_CASE("An Oren-Nayar surface reflects as Oren and Nayar's qualitative model",
          "[materials][bsdf]")
{
    // The roughness from 0 to 1 is the standard deviation of the facets' slopes from 0 to
    // pi / 2 radians.
    const OrenNayarBSDF<RGB> bsdf;
    const Interaction<RGB> isect = surface();
    for (float roughness : {0.f, 0.25f, 0.5f, 1.f}) {
        const ShadingParams<RGB> params = shading(ALBEDO, roughness);
        const double sigma = roughness * std::numbers::pi / 2.0;
        for (const Angles& a : angle_sets()) {
            const RGB f = bsdf.eval(a.wo(isect), a.wi(isect), isect, params);
            INFO("roughness " << roughness << ", i " << a.i << ", e " << a.e << ", azimuth "
                              << a.azimuth);
            for (std::size_t c = 0; c < RGB::size(); ++c) {
                CHECK(close(f[c], ref::oren_nayar(ALBEDO[c], sigma, a.i, a.e, a.azimuth), 1e-4));
            }
        }
    }
}

TEST_CASE("A smooth Oren-Nayar surface is Lambertian, and a rough one is brighter toward the "
          "light",
          "[materials][bsdf]")
{
    const OrenNayarBSDF<RGB> oren_nayar;
    const LambertianBSDF<RGB> lambertian;
    const Interaction<RGB> isect = surface();

    const ShadingParams<RGB> smooth = shading(ALBEDO, 0.f);
    for (const Angles& a : angle_sets()) {
        INFO("i " << a.i << ", e " << a.e << ", azimuth " << a.azimuth);
        CHECK(oren_nayar.eval(a.wo(isect), a.wi(isect), isect, smooth) ==
              lambertian.eval(a.wo(isect), a.wi(isect), isect, smooth));
    }

    // Rough surfaces scatter light back toward its source (the facets that face the light are
    // the ones the viewer sees), so a viewer beside the light sees more than one opposite it, who
    // sees only the A term.
    const ShadingParams<RGB> rough = shading(ALBEDO, 0.7f);
    const double sigma = 0.7 * std::numbers::pi / 2.0;
    for (double angle : {0.3, 0.8, 1.2}) {
        const Angles back{angle, angle, 0.0};
        const Angles forward{angle, angle, std::numbers::pi};
        const float f_back = oren_nayar.eval(back.wo(isect), back.wi(isect), isect, rough)[1];
        const float f_forward =
            oren_nayar.eval(forward.wo(isect), forward.wi(isect), isect, rough)[1];
        INFO("light and viewer at " << angle << " rad: " << f_back << " toward the light, "
                                    << f_forward << " away from it");
        CHECK(f_back > f_forward);
        const double a = 1.0 - 0.5 * sigma * sigma / (sigma * sigma + 0.33);
        CHECK(close(f_forward, ALBEDO[1] / std::numbers::pi * a, 1e-5));
    }
}

TEST_CASE("An Oren-Nayar surface reflects nothing toward a viewer below it", "[materials][bsdf]")
{
    const OrenNayarBSDF<RGB> bsdf;
    const Interaction<RGB> isect = surface();
    const ShadingParams<RGB> params = shading(ALBEDO, 0.5f);
    const Vec3<float> wi = direction(isect, 0.5, 0.0);
    for (double theta : {1.6, 2.5, 3.1}) {
        const Vec3<float> wo = direction(isect, theta, 1.0);
        CHECK(bsdf.eval(wo, wi, isect, params) == RGB{0.f});
        CHECK(bsdf.pdf(wo, wi, isect, params) == 0.f);
        CHECK(!bsdf.sample(wo, isect, params, 0.4f, 0.4f).is_valid());
    }
}
