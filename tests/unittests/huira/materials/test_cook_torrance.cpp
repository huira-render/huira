#include <cmath>
#include <numbers>

#include "catch2/catch_test_macros.hpp"
#include "huira/huira.hpp"

using namespace huira;

namespace {

/// A surface facing +z, with the default tangent frame.
Interaction<RGB> flat_surface()
{
    Interaction<RGB> isect;
    isect.normal_g = Vec3<float>{0.f, 0.f, 1.f};
    isect.normal_s = isect.normal_g;
    build_default_tangent_frame(isect.normal_s, isect.tangent, isect.bitangent);
    return isect;
}

ShadingParams<RGB> metal(float roughness)
{
    ShadingParams<RGB> params;
    params.albedo = RGB{0.8f};
    params.roughness = roughness;
    params.metallic = 1.f;
    return params;
}

/// The direction at the given angle from the normal, in the x-z plane.
Vec3<float> at_angle(double radians)
{
    return Vec3<float>{
        static_cast<float>(std::sin(radians)), 0.f, static_cast<float>(std::cos(radians))};
}

bool finite(const RGB& value)
{
    return std::isfinite(value[0]) && std::isfinite(value[1]) && std::isfinite(value[2]);
}

/// The light reflected from a unit irradiance from wo: the integral of f(wo, wi) cos(theta_i)
/// over the hemisphere, by quadrature in rings around the mirror direction, finely spaced near
/// it, where a smooth surface's lobe is.
double reflected(const CookTorranceBSDF<RGB>& bsdf,
                 const Interaction<RGB>& isect,
                 const ShadingParams<RGB>& params,
                 double incidence)
{
    const double sin_i = std::sin(incidence);
    const double cos_i = std::cos(incidence);
    const Vec3<float> wo = at_angle(incidence);
    // The mirror direction, and two directions perpendicular to it:
    const double r[3] = {-sin_i, 0.0, cos_i};
    const double e1[3] = {cos_i, 0.0, sin_i};
    const double e2[3] = {0.0, 1.0, 0.0};

    constexpr int RINGS = 3000;
    constexpr int SPOKES = 256;
    const double log_min = std::log(1e-8);
    const double log_max = std::log(std::numbers::pi / 2.0);
    const double step = (log_max - log_min) / RINGS;
    double total = 0.0;
    for (int i = 0; i < RINGS; ++i) {
        const double t = std::exp(log_min + (i + 0.5) * step); // angle from the mirror direction
        const double ring = std::sin(t) * t * step * (2.0 * std::numbers::pi / SPOKES);
        for (int j = 0; j < SPOKES; ++j) {
            const double phi = (j + 0.5) * 2.0 * std::numbers::pi / SPOKES;
            const double a = std::cos(t);
            const double b = std::sin(t) * std::cos(phi);
            const double c = std::sin(t) * std::sin(phi);
            const Vec3<float> wi{static_cast<float>(a * r[0] + b * e1[0] + c * e2[0]),
                                 static_cast<float>(a * r[1] + b * e1[1] + c * e2[1]),
                                 static_cast<float>(a * r[2] + b * e1[2] + c * e2[2])};
            if (wi.z <= 0.f) {
                continue;
            }
            const RGB f = bsdf.eval(wo, wi, isect, params);
            total += static_cast<double>(f[1]) * static_cast<double>(wi.z) * ring;
        }
    }
    return total;
}

} // namespace

TEST_CASE("A smooth metal's BSDF samples are finite", "[materials][bsdf]")
{
    // Roughness under 0.013 (0 is raised to 0.01) used to give an infinite D wherever the half
    // vector rounded to the normal: alpha^2 - 1 rounded to -1, leaving 0 in D's denominator.
    // Such samples' values were NaN, and dropped, and pixels that saw only these were NaN.
    const CookTorranceBSDF<RGB> bsdf;
    const Interaction<RGB> isect = flat_surface();
    for (float roughness : {0.f, 0.01f, 0.02f}) {
        const ShadingParams<RGB> params = metal(roughness);
        for (double incidence : {0.0, 0.5, 1.2, 1.5}) {
            const Vec3<float> wo = at_angle(incidence);
            int valid = 0;
            int bad = 0;
            for (int i = 0; i < 64; ++i) {
                for (int j = 0; j < 64; ++j) {
                    const float u1 = (static_cast<float>(i) + 0.5f) / 64.f;
                    const float u2 = (static_cast<float>(j) + 0.5f) / 64.f;
                    const BSDFSample<RGB> sample = bsdf.sample(wo, isect, params, u1, u2);
                    if (!sample.is_valid()) {
                        continue;
                    }
                    ++valid;
                    if (!finite(sample.value) || !std::isfinite(sample.pdf) ||
                        !finite(bsdf.eval(wo, sample.wi, isect, params)) ||
                        !std::isfinite(bsdf.pdf(wo, sample.wi, isect, params))) {
                        ++bad;
                    }
                }
            }
            INFO("roughness " << roughness << ", incidence " << incidence << " rad: " << bad
                              << " of " << valid << " samples not finite");
            CHECK(valid > 0);
            CHECK(bad == 0);
        }
    }
}

TEST_CASE("A smooth metal reflects its albedo", "[materials][bsdf]")
{
    // A metal reflects F of the light, which is its albedo at normal incidence and a little
    // more at 45 degrees. D used to be infinite at roughness 0.01, and 48% too large at normal
    // incidence at roughness 0.02. Computing its sine as 1 - cos^2 instead flattens the lobe of
    // a surface this smooth (cos rounds to 1 within 2.4e-4 rad of the normal), which reflects
    // 18 times the light at roughness 0.01.
    const CookTorranceBSDF<RGB> bsdf;
    const Interaction<RGB> isect = flat_surface();
    for (float roughness : {0.f, 0.02f, 0.05f}) {
        const ShadingParams<RGB> params = metal(roughness);
        for (double incidence : {0.0, std::numbers::pi / 4.0}) {
            const double fresnel = 0.8 + 0.2 * std::pow(1.0 - std::cos(incidence), 5.0);
            const double light = reflected(bsdf, isect, params, incidence);
            INFO("roughness " << roughness << ", incidence " << incidence << " rad: reflects "
                              << light << ", against " << fresnel);
            CHECK(std::abs(light - fresnel) <= 0.01 * fresnel);
        }
    }
}
