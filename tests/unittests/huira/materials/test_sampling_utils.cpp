#include <array>
#include <cmath>
#include <functional>
#include <numbers>
#include <random>

#include "catch2/catch_test_macros.hpp"
#include "huira/huira.hpp"
#include "huira/materials/sampling_utils.hpp"

using namespace huira;

namespace {

using std::numbers::pi;

/// A direction and its pdf, as a sampler draws them from two uniform numbers.
struct Drawn {
    Vec3<float> direction;
    float pdf;
};

using Sampler = std::function<Drawn(float, float)>;

Vec3<double> to_double(const Vec3<float>& v)
{
    return Vec3<double>{v.x, v.y, v.z};
}

/// The density, per steradian, of the directions a sampler draws near the one it draws from
/// (u1, u2): the reciprocal of the solid angle that a unit area of (u1, u2) maps onto, the cross
/// product of the direction's derivatives, here by central differences.
double true_density(const Sampler& sampler, double u1, double u2)
{
    constexpr double DU = 1e-3;
    const auto at = [&](double a, double b) {
        return to_double(sampler(static_cast<float>(a), static_cast<float>(b)).direction);
    };
    const Vec3<double> d1 = (at(u1 + DU, u2) - at(u1 - DU, u2)) / (2.0 * DU);
    const Vec3<double> d2 = (at(u1, u2 + DU) - at(u1, u2 - DU)) / (2.0 * DU);
    return 1.0 / glm::length(glm::cross(d1, d2));
}

/// Checks a sampler over a grid of (u1, u2) away from the edges of the unit square: each
/// direction is a unit vector in the upper hemisphere, and its pdf is the density of the
/// directions the sampler draws there, so that f / pdf averages to the integral of f.
void check_sampler(const Sampler& sampler)
{
    for (double u1 : {0.1, 0.3, 0.5, 0.7, 0.9}) {
        for (double u2 : {0.05, 0.3, 0.55, 0.8, 0.95}) {
            const Drawn drawn = sampler(static_cast<float>(u1), static_cast<float>(u2));
            const double density = true_density(sampler, u1, u2);
            INFO("u1 " << u1 << ", u2 " << u2 << ": direction (" << drawn.direction.x << ", "
                       << drawn.direction.y << ", " << drawn.direction.z << "), pdf " << drawn.pdf
                       << ", density of the samples " << density);
            CHECK(std::abs(glm::length(drawn.direction) - 1.f) < 1e-5f);
            CHECK(drawn.direction.z >= 0.f);
            CHECK(std::abs(drawn.pdf / density - 1.0) < 2e-3);
        }
    }
}

/// The direction at the given angle from +z, at the given azimuth around it.
Vec3<float> spherical(double theta, double phi)
{
    return Vec3<float>{static_cast<float>(std::sin(theta) * std::cos(phi)),
                       static_cast<float>(std::sin(theta) * std::sin(phi)),
                       static_cast<float>(std::cos(theta))};
}

/// GGX's D for the microfacet normal h (local, +z up), computed in double from the closed form
/// alpha^2 / (pi cos^4 (alpha^2 + tan^2)^2), with tan^2 from h's components: an angle from
/// acos(h.z) has too little precision in the narrow lobe of a smooth surface.
double ggx_d(const Vec3<double>& h, double alpha2)
{
    const double cos2 = h.z * h.z;
    const double tan2 = (h.x * h.x + h.y * h.y) / cos2;
    return alpha2 / (pi * cos2 * cos2 * (alpha2 + tan2) * (alpha2 + tan2));
}

/// Smith's masking G1 of GGX for a direction at angle theta from the normal.
double ggx_g1(double theta, double alpha2)
{
    const double tan2 = std::tan(theta) * std::tan(theta);
    return 2.0 / (1.0 + std::sqrt(1.0 + alpha2 * tan2));
}

} // namespace

TEST_CASE("The hemisphere samplers return the pdfs they document", "[materials][sampling]")
{
    for (int i = 0; i <= 16; ++i) {
        for (int j = 0; j < 4; ++j) {
            // u1 runs from 0 to just under 1, the range a sampler is given:
            const float u1 = std::min(static_cast<float>(i) / 16.f, 0.99999f);
            const float u2 = (static_cast<float>(j) + 0.5f) / 4.f;
            INFO("u1 " << u1 << ", u2 " << u2);

            const auto uniform = sampling::uniform_hemisphere(u1, u2);
            CHECK(std::abs(uniform.pdf - 1.0 / (2.0 * pi)) < 1e-7);

            const auto cosine = sampling::cosine_hemisphere(u1, u2);
            CHECK(std::abs(cosine.pdf - cosine.direction.z / pi) < 1e-7);

            for (float n : {0.f, 1.f, 4.f, 20.f}) {
                const auto power = sampling::power_cosine_hemisphere(u1, u2, n);
                const double expected = (n + 1.0) / (2.0 * pi) * std::pow(power.direction.z, n);
                CHECK(std::abs(power.pdf - expected) <= 1e-5 * expected + 1e-7);
            }

            for (const auto& direction : {uniform.direction, cosine.direction}) {
                CHECK(std::abs(glm::length(direction) - 1.f) < 1e-5f);
                CHECK(direction.z >= 0.f);
            }
        }
    }
}

TEST_CASE("The hemisphere samplers draw directions with the density of their pdfs",
          "[materials][sampling]")
{
    // A pdf that is not the density of the directions drawn biases every estimate made with it.
    SECTION("uniform")
    {
        check_sampler([](float u1, float u2) {
            const auto s = sampling::uniform_hemisphere(u1, u2);
            return Drawn{s.direction, s.pdf};
        });
    }
    SECTION("cosine")
    {
        check_sampler([](float u1, float u2) {
            const auto s = sampling::cosine_hemisphere(u1, u2);
            return Drawn{s.direction, s.pdf};
        });
    }
    SECTION("power cosine")
    {
        for (float n : {0.f, 1.f, 4.f, 20.f}) {
            INFO("exponent " << n);
            check_sampler([n](float u1, float u2) {
                const auto s = sampling::power_cosine_hemisphere(u1, u2, n);
                return Drawn{s.direction, s.pdf};
            });
        }
    }
}

TEST_CASE("The GGX samplers draw microfacet normals with the density of their pdfs",
          "[materials][sampling]")
{
    SECTION("from the distribution of normals, D(h) cos(theta_h)")
    {
        for (float roughness : {0.3f, 0.6f, 1.f}) {
            INFO("roughness " << roughness);
            check_sampler([roughness](float u1, float u2) {
                const auto s = sampling::ggx_sample_half_vector(u1, u2, roughness);
                return Drawn{s.half_vector, s.pdf};
            });
        }
    }

    SECTION("from the distribution of visible normals, D(h) G1(wo) max(0, wo.h) / cos(theta_o)")
    {
        // Overhead takes the sampler's other branch, which has no azimuth to align to:
        for (double theta_o : {0.0, 0.5, 1.2, 1.5}) {
            const Vec3<float> wo = spherical(theta_o, 0.7);
            for (float roughness : {0.3f, 0.6f, 1.f}) {
                INFO("wo at " << theta_o << " rad, roughness " << roughness);
                check_sampler([wo, roughness](float u1, float u2) {
                    const auto s = sampling::ggx_vndf_sample(wo, roughness, u1, u2);
                    return Drawn{s.half_vector, s.pdf};
                });
            }
        }
    }
}

TEST_CASE("The GGX samplers' pdfs are the densities of GGX they document", "[materials][sampling]")
{
    for (float roughness : {0.05f, 0.3f, 1.f}) {
        const double alpha2 = std::pow(static_cast<double>(roughness), 4.0);
        for (int i = 0; i < 8; ++i) {
            for (int j = 0; j < 3; ++j) {
                const float u1 = (static_cast<float>(i) + 0.5f) / 8.f;
                const float u2 = (static_cast<float>(j) + 0.5f) / 3.f;
                INFO("roughness " << roughness << ", u1 " << u1 << ", u2 " << u2);

                const auto ms = sampling::ggx_sample_half_vector(u1, u2, roughness);
                const Vec3<double> h = glm::normalize(to_double(ms.half_vector));
                CHECK(std::abs(ms.pdf / (ggx_d(h, alpha2) * h.z) - 1.0) < 1e-3);

                const double theta_o = 1.0;
                const Vec3<float> wo = spherical(theta_o, 2.0);
                const auto vs = sampling::ggx_vndf_sample(wo, roughness, u1, u2);
                const Vec3<double> v = glm::normalize(to_double(vs.half_vector));
                const double wo_dot_v = glm::dot(to_double(wo), v);
                // The visible normals face the viewer:
                CHECK(wo_dot_v >= 0.0);
                CHECK(v.z >= 0.0);
                const double visible =
                    ggx_d(v, alpha2) * ggx_g1(theta_o, alpha2) * wo_dot_v / std::cos(theta_o);
                CHECK(std::abs(vs.pdf / visible - 1.0) < 1e-3);
            }
        }
    }
}

TEST_CASE("GGX's distribution of normals is normalized and finite on smooth surfaces",
          "[materials][sampling]")
{
    // The projected areas of the microfacets add to the area of the surface: the integral of
    // D(h) cos(theta_h) over the hemisphere is 1. The lobe of a smooth surface is as narrow as
    // alpha, so the quadrature is over log(theta).
    for (float roughness : {0.01f, 0.05f, 0.2f, 0.5f, 1.f}) {
        const float alpha2 = roughness * roughness * roughness * roughness;
        constexpr int STEPS = 20000;
        const double log_min = std::log(1e-9);
        const double log_max = std::log(pi / 2.0);
        const double step = (log_max - log_min) / STEPS;
        double total = 0.0;
        for (int k = 0; k < STEPS; ++k) {
            const double theta = std::exp(log_min + (k + 0.5) * step);
            const double cos_theta = std::cos(theta);
            const double sin_theta = std::sin(theta);
            const double d = sampling::ggx_distribution(static_cast<float>(cos_theta * cos_theta),
                                                        static_cast<float>(sin_theta * sin_theta),
                                                        alpha2);
            total += 2.0 * pi * d * cos_theta * sin_theta * theta * step;
        }
        INFO("roughness " << roughness << ": D cos(theta) integrates to " << total);
        CHECK(std::abs(total - 1.0) < 1e-3);

        // Its peak, along the normal, is 1 / (pi alpha^2):
        const float peak = sampling::ggx_distribution(1.f, 0.f, alpha2);
        CHECK(std::isfinite(peak));
        CHECK(std::abs(peak * pi * alpha2 - 1.0) < 1e-5);
    }
}

TEST_CASE("The default tangent frame is orthonormal and right-handed, and the frame conversions "
          "invert each other",
          "[materials][sampling]")
{
    const std::array<Vec3<float>, 7> normals{
        Vec3<float>{0.f, 0.f, 1.f},
        Vec3<float>{0.f, 0.f, -1.f},
        Vec3<float>{1.f, 0.f, 0.f},
        Vec3<float>{0.f, -1.f, 0.f},
        glm::normalize(Vec3<float>{1.f, 2.f, 3.f}),
        glm::normalize(Vec3<float>{-0.3f, 0.1f, -2.f}),
        glm::normalize(Vec3<float>{1e-4f, 0.f, -1.f}), // where the construction divides by ~0
    };
    const Vec3<float> local{0.48f, -0.6f, 0.64f};
    for (const Vec3<float>& n : normals) {
        Vec3<float> t;
        Vec3<float> b;
        build_default_tangent_frame(n, t, b);
        INFO("normal (" << n.x << ", " << n.y << ", " << n.z << ")");
        CHECK(std::abs(glm::length(t) - 1.f) < 1e-5f);
        CHECK(std::abs(glm::length(b) - 1.f) < 1e-5f);
        CHECK(std::abs(glm::dot(t, n)) < 1e-5f);
        CHECK(std::abs(glm::dot(b, n)) < 1e-5f);
        CHECK(std::abs(glm::dot(t, b)) < 1e-5f);
        CHECK(glm::length(glm::cross(t, b) - n) < 1e-5f);

        // The local axes are the frame's, and a direction goes there and back unchanged:
        CHECK(glm::length(sampling::local_to_world({0.f, 0.f, 1.f}, t, b, n) - n) < 1e-6f);
        CHECK(glm::length(sampling::local_to_world({1.f, 0.f, 0.f}, t, b, n) - t) < 1e-6f);
        CHECK(glm::length(sampling::local_to_world({0.f, 1.f, 0.f}, t, b, n) - b) < 1e-6f);
        const Vec3<float> world = sampling::local_to_world(local, t, b, n);
        CHECK(std::abs(glm::dot(world, n) - local.z) < 1e-5f);
        CHECK(glm::length(sampling::world_to_local(world, t, b, n) - local) < 1e-5f);
    }
}

TEST_CASE("The angle between two directions is resolved at every angle", "[materials][sampling]")
{
    // acos(dot) of float unit vectors comes out as 0 or about 3.5e-4 rad for every angle below
    // that, and likewise near pi; angle_between() and tan_half_angle_between() keep float's
    // precision. The reference is the angle between the same float vectors, in double.
    std::mt19937 rng(7);
    std::uniform_real_distribution<double> u(-1.0, 1.0);
    for (double angle : {0.0, 1e-7, 1e-6, 1e-5, 1e-4, 3.5e-4, 1e-3, 0.1, 1.0, 2.0, 3.0, 3.14}) {
        double worst_angle = 0.0;
        double worst_tan_half = 0.0;
        for (int k = 0; k < 2000; ++k) {
            const Vec3<double> a = glm::normalize(Vec3<double>{u(rng), u(rng), u(rng)});
            const Vec3<double> p =
                glm::normalize(glm::cross(a, Vec3<double>{u(rng), u(rng), u(rng)}));
            const auto fa = Vec3<float>(a);
            const auto fb = Vec3<float>(glm::normalize(a * std::cos(angle) + p * std::sin(angle)));
            const Vec3<double> da{fa};
            const Vec3<double> db{fb};
            const double exact = std::atan2(glm::length(glm::cross(da, db)), glm::dot(da, db));
            worst_angle =
                std::max(worst_angle, std::abs(double{sampling::angle_between(fa, fb)} - exact));
            const double tan_half = std::tan(exact / 2.0);
            worst_tan_half =
                std::max(worst_tan_half,
                         std::abs(double{sampling::tan_half_angle_between(fa, fb)} - tan_half) /
                             std::max(1.0, tan_half));
        }
        INFO("angle " << angle << " rad: worst error " << worst_angle << " rad, and "
                      << worst_tan_half << " in tan(angle / 2)");
        CHECK(worst_angle < 3e-7);
        CHECK(worst_tan_half < 3e-7);
    }
}
