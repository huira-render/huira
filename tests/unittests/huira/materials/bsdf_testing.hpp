#pragma once

// Surfaces, directions and integrals over the sphere of directions, shared by the BSDF tests.

#include <cmath>
#include <cstddef>
#include <numbers>

#include "huira/huira.hpp"

namespace huira::test {

/// A surface with the given (unit) normal and its default tangent frame, at texture coordinates
/// (0.5, 0.5).
inline Interaction<RGB> surface(const Vec3<float>& normal = Vec3<float>{0.f, 0.f, 1.f})
{
    Interaction<RGB> isect;
    isect.normal_g = normal;
    isect.normal_s = normal;
    build_default_tangent_frame(isect.normal_s, isect.tangent, isect.bitangent);
    isect.uv = Vec2<float>{0.5f, 0.5f};
    return isect;
}

/// The direction at angle theta from a surface's normal, at azimuth phi from its tangent.
inline Vec3<float> direction(const Interaction<RGB>& isect, double theta, double phi)
{
    const auto sin_theta = static_cast<float>(std::sin(theta));
    return glm::normalize(isect.tangent * (sin_theta * static_cast<float>(std::cos(phi))) +
                          isect.bitangent * (sin_theta * static_cast<float>(std::sin(phi))) +
                          isect.normal_s * static_cast<float>(std::cos(theta)));
}

/// The mirror image of w in the surface's normal.
inline Vec3<float> mirror(const Interaction<RGB>& isect, const Vec3<float>& w)
{
    return 2.f * glm::dot(w, isect.normal_s) * isect.normal_s - w;
}

/// The geometry photometry describes reflection with, in radians: the incidence angle i of the
/// light, the emission angle e of the viewer, and the azimuth between them around the normal.
struct Angles {
    double i;
    double e;
    double azimuth;

    /// The direction to the light, wi, at azimuth 0.
    [[nodiscard]] Vec3<float> wi(const Interaction<RGB>& isect) const
    {
        return direction(isect, i, 0.0);
    }
    /// The direction to the viewer, wo.
    [[nodiscard]] Vec3<float> wo(const Interaction<RGB>& isect) const
    {
        return direction(isect, e, azimuth);
    }
};

inline bool finite(const RGB& value)
{
    return std::isfinite(value[0]) && std::isfinite(value[1]) && std::isfinite(value[2]);
}

/// The integral of fn(w) over the sphere of directions, by quadrature in rings around `axis`
/// (a unit vector): finely spaced near it, where a smooth surface's lobe is, and evenly beyond.
template <class F>
double integrate_sphere(const F& fn, const Vec3<float>& axis)
{
    constexpr int NEAR_RINGS = 100; // log-spaced, from 1e-6 to 0.05 rad from the axis
    constexpr int FAR_RINGS = 300;  // evenly spaced, from 0.05 to pi
    constexpr int SPOKES = 128;
    constexpr double PI = std::numbers::pi;
    constexpr double SPLIT = 0.05;

    // Two directions perpendicular to the axis:
    Vec3<float> e1;
    Vec3<float> e2;
    build_default_tangent_frame(axis, e1, e2);

    const auto ring = [&](double t, double dt) {
        double sum = 0.0;
        for (int j = 0; j < SPOKES; ++j) {
            const double phi = (j + 0.5) * 2.0 * PI / SPOKES;
            const Vec3<float> w =
                glm::normalize(axis * static_cast<float>(std::cos(t)) +
                               e1 * static_cast<float>(std::sin(t) * std::cos(phi)) +
                               e2 * static_cast<float>(std::sin(t) * std::sin(phi)));
            sum += fn(w);
        }
        return sum * std::sin(t) * dt * (2.0 * PI / SPOKES);
    };

    double total = 0.0;
    const double log_min = std::log(1e-6);
    const double log_step = (std::log(SPLIT) - log_min) / NEAR_RINGS;
    for (int i = 0; i < NEAR_RINGS; ++i) {
        const double t = std::exp(log_min + (i + 0.5) * log_step);
        total += ring(t, t * log_step);
    }
    const double step = (PI - SPLIT) / FAR_RINGS;
    for (int i = 0; i < FAR_RINGS; ++i) {
        total += ring(SPLIT + (i + 0.5) * step, step);
    }
    return total;
}

/// The light a BSDF reflects toward wo, in the given channel, from a unit irradiance: the
/// integral of f(wo, wi) cos(theta_i) over the directions wi above the surface.
inline double reflected(const BSDF<RGB>& bsdf,
                        const Interaction<RGB>& isect,
                        const ShadingParams<RGB>& params,
                        const Vec3<float>& wo,
                        std::size_t channel = 1)
{
    return integrate_sphere(
        [&](const Vec3<float>& wi) {
            const float cos_i = glm::dot(wi, isect.normal_s);
            if (cos_i <= 0.f) {
                return 0.0;
            }
            return static_cast<double>(bsdf.eval(wo, wi, isect, params)[channel]) * cos_i;
        },
        mirror(isect, wo));
}

/// The integral of a BSDF's pdf(wo, wi) over all directions wi.
inline double total_pdf(const BSDF<RGB>& bsdf,
                        const Interaction<RGB>& isect,
                        const ShadingParams<RGB>& params,
                        const Vec3<float>& wo)
{
    return integrate_sphere(
        [&](const Vec3<float>& wi) { return static_cast<double>(bsdf.pdf(wo, wi, isect, params)); },
        mirror(isect, wo));
}

/// The mean of the values of a BSDF's samples, in the given channel, over an n x n grid of
/// (u1, u2), counting rejected samples as 0: an estimate of reflected().
inline double mean_sample_value(const BSDF<RGB>& bsdf,
                                const Interaction<RGB>& isect,
                                const ShadingParams<RGB>& params,
                                const Vec3<float>& wo,
                                int n,
                                std::size_t channel = 1)
{
    double total = 0.0;
    for (int i = 0; i < n; ++i) {
        for (int j = 0; j < n; ++j) {
            const float u1 = (static_cast<float>(i) + 0.5f) / static_cast<float>(n);
            const float u2 = (static_cast<float>(j) + 0.5f) / static_cast<float>(n);
            const BSDFSample<RGB> sample = bsdf.sample(wo, isect, params, u1, u2);
            if (sample.is_valid()) {
                total += static_cast<double>(sample.value[channel]);
            }
        }
    }
    return total / (static_cast<double>(n) * n);
}

/// The fraction of a BSDF's samples, over an n x n grid of (u1, u2), that are directions rather
/// than rejected: the probability that sample() returns one, which the pdf integrates to.
inline double valid_fraction(const BSDF<RGB>& bsdf,
                             const Interaction<RGB>& isect,
                             const ShadingParams<RGB>& params,
                             const Vec3<float>& wo,
                             int n)
{
    int valid = 0;
    for (int i = 0; i < n; ++i) {
        for (int j = 0; j < n; ++j) {
            const float u1 = (static_cast<float>(i) + 0.5f) / static_cast<float>(n);
            const float u2 = (static_cast<float>(j) + 0.5f) / static_cast<float>(n);
            if (bsdf.sample(wo, isect, params, u1, u2).is_valid()) {
                ++valid;
            }
        }
    }
    return static_cast<double>(valid) / (static_cast<double>(n) * n);
}

} // namespace huira::test
