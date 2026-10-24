#include <algorithm>
#include <array>
#include <cmath>
#include <concepts>
#include <cstddef>
#include <numbers>

#include "huira/core/constants.hpp"
#include "huira/materials/sampling_utils.hpp"

namespace huira {
namespace detail {

/**
 * @brief McEwen's lunar-Lambert function per unit albedo, as a BSDF: (1 - beta) / pi +
 * 2 beta / (pi (cos_i + cos_o)), with beta = exp(-phase / 60 degrees). Its Lommel-Seeliger part
 * is normalized, as McEwen's is, to equal Lambert's at i = e = 0.
 */
template <std::floating_point T>
[[nodiscard]] T mcewen_reflectance(T phase, T cos_i, T cos_o) noexcept
{
    // 60 degrees = pi / 3 radians, so division by (pi / 3) is multiplication by (3 / pi)
    const T beta = std::exp(-phase * (T{3} * std::numbers::inv_pi_v<T>));
    const T lambert = std::numbers::inv_pi_v<T>;
    const T lommel_seeliger = T{2} * std::numbers::inv_pi_v<T> / (cos_i + cos_o);
    return (T{1} - beta) * lambert + beta * lommel_seeliger;
}

/**
 * @brief The light McEwen's lunar-Lambert function reflects per unit albedo, of the light from a
 * direction at cosine mu to the normal (or toward it: the function is reciprocal): rho(mu), the
 * integral of mcewen_reflectance() cos_i over the hemisphere. It is 1.09 overhead and 1.84 at the
 * horizon.
 *
 * Built once, from mcewen_reflectance() itself, in about 3 ms: at nodes evenly spaced in sqrt(mu),
 * by Gauss-Legendre quadrature in polar coordinates about the direction (in which beta, a
 * function of the angle from it, is smooth), over only the azimuths above the surface. The nodes
 * are raised so that interpolating them linearly is above rho at the middle of every cell, then
 * by 1e-3 more, which covers the quadrature's error: the table is never below rho, and at most
 * 2e-3 above it.
 */
class McEwenDirectionalAlbedo {
  public:
    [[nodiscard]] static const McEwenDirectionalAlbedo& instance() noexcept
    {
        static const McEwenDirectionalAlbedo table;
        return table;
    }

    /// rho at the given cosine to the normal, never below the function's.
    [[nodiscard]] float operator()(float mu) const noexcept
    {
        const float x = std::sqrt(std::clamp(mu, 0.0f, 1.0f)) * static_cast<float>(CELLS);
        const std::size_t i = std::min(static_cast<std::size_t>(x), CELLS - 1);
        const float t = x - static_cast<float>(i);
        return values_[i] + t * (values_[i + 1] - values_[i]);
    }

    /// The largest value of the table.
    [[nodiscard]] float max() const noexcept { return max_; }

  private:
    static constexpr std::size_t CELLS = 64;
    static constexpr std::size_t POINTS = 64; // Gauss-Legendre points per dimension

    McEwenDirectionalAlbedo() noexcept
    {
        // Gauss-Legendre nodes and weights on [-1, 1]:
        std::array<double, POINTS> x{};
        std::array<double, POINTS> w{};
        constexpr double pi = std::numbers::pi;
        constexpr auto n = static_cast<double>(POINTS);
        for (std::size_t i = 0; i < POINTS; ++i) {
            double z = std::cos(pi * (static_cast<double>(i) + 0.75) / (n + 0.5));
            double derivative = 1.0;
            for (int iteration = 0; iteration < 100; ++iteration) {
                double p0 = 1.0;
                double p1 = z;
                for (std::size_t k = 2; k <= POINTS; ++k) {
                    const auto kd = static_cast<double>(k);
                    const double p2 = ((2.0 * kd - 1.0) * z * p1 - (kd - 1.0) * p0) / kd;
                    p0 = p1;
                    p1 = p2;
                }
                derivative = n * (z * p1 - p0) / (z * z - 1.0);
                const double step = p1 / derivative;
                z -= step;
                if (std::abs(step) < 1e-15) {
                    break;
                }
            }
            x[i] = z;
            w[i] = 2.0 / ((1.0 - z * z) * derivative * derivative);
        }

        // rho at cosine mu, over directions at angle a from the direction (the phase angle) and
        // azimuth p about it, from the plane of the normal: cos_i = mu cos a + s sin a cos p, with
        // s = sqrt(1 - mu^2), above the surface for |p| < p_max(a).
        const auto rho = [&](double mu) {
            const double s = std::sqrt(std::max(0.0, 1.0 - mu * mu));
            double total = 0.0;
            for (std::size_t k = 0; k < POINTS; ++k) {
                const double a = 0.5 * pi * (x[k] + 1.0);
                const double cos_a = std::cos(a);
                const double sin_a = std::sin(a);
                double p_max = 0.0;
                if (s * sin_a > 0.0) {
                    p_max = std::acos(std::clamp(-mu * cos_a / (s * sin_a), -1.0, 1.0));
                } else {
                    p_max = mu * cos_a > 0.0 ? pi : 0.0;
                }
                double inner = 0.0;
                for (std::size_t m = 0; m < POINTS; ++m) {
                    const double cos_i = mu * cos_a + s * sin_a * std::cos(p_max * x[m]);
                    if (cos_i > 0.0) {
                        inner += w[m] * mcewen_reflectance(a, cos_i, mu) * cos_i;
                    }
                }
                total += 0.5 * pi * w[k] * sin_a * p_max * inner;
            }
            return total;
        };

        std::array<double, CELLS + 1> node{};
        for (std::size_t i = 0; i <= CELLS; ++i) {
            const double xi = static_cast<double>(i) / static_cast<double>(CELLS);
            node[i] = rho(xi * xi);
        }
        std::array<double, CELLS + 1> raise{};
        for (std::size_t i = 0; i < CELLS; ++i) {
            const double middle = (static_cast<double>(i) + 0.5) / static_cast<double>(CELLS);
            const double shortfall =
                std::max(0.0, rho(middle * middle) - 0.5 * (node[i] + node[i + 1]));
            raise[i] = std::max(raise[i], shortfall);
            raise[i + 1] = std::max(raise[i + 1], shortfall);
        }
        for (std::size_t i = 0; i <= CELLS; ++i) {
            values_[i] = static_cast<float>(node[i] + raise[i] + 1e-3);
            max_ = std::max(max_, values_[i]);
        }
    }

    std::array<float, CELLS + 1> values_{};
    float max_ = 0.0f;
};

} // namespace detail

template <IsSpectral TSpectral>
McEwenBSDF<TSpectral>::McEwenBSDF() noexcept
{
    // Build the table now, while the scene is set up, rather than at the first eval():
    (void)detail::McEwenDirectionalAlbedo::instance();
}

template <IsSpectral TSpectral>
BSDFRequirements McEwenBSDF<TSpectral>::requirements() const
{
    BSDFRequirements reqs{};
    reqs.needs_albedo = true;
    reqs.needs_normal = true;
    reqs.needs_metallic = false;
    reqs.needs_roughness = false;
    return reqs;
}

template <IsSpectral TSpectral>
TSpectral McEwenBSDF<TSpectral>::eval(const Vec3<float>& wo,
                                      const Vec3<float>& wi,
                                      const Interaction<TSpectral>& isect,
                                      const ShadingParams<TSpectral>& params) const
{
    const float cos_i = glm::dot(wi, isect.normal_s);
    const float cos_o = glm::dot(wo, isect.normal_s);

    if (cos_i <= 0.0f || cos_o <= 0.0f) {
        return TSpectral{0};
    }

    // Phase angle (angle between incoming and outgoing light directions), resolved near 0 (see
    // sampling::angle_between()):
    const float phase = sampling::angle_between(wi, wo);
    const float reflectance = detail::mcewen_reflectance(phase, cos_i, cos_o);

    // The lunar-Lambert function, a fit of I/F, reflects albedo * rho(mu) of the light from a
    // direction at cosine mu, which is more than all of it at a high albedo: up to 1.84 times
    // the albedo. Where it would, cap the albedo, for this pair of directions, at
    // 1 / max(rho(cos_i), rho(cos_o)), so that it reflects no more than all the light from
    // either. That keeps the BSDF reciprocal and continuous, and leaves the function as it is
    // wherever it conserves energy: always at an albedo up to 1 / 1.84.
    const auto& rho = detail::McEwenDirectionalAlbedo::instance();
    if (params.albedo.max() * rho.max() <= 1.0f) {
        return params.albedo * reflectance;
    }
    const float cap = 1.0f / std::max(rho(cos_i), rho(cos_o));
    TSpectral albedo = params.albedo;
    for (std::size_t c = 0; c < TSpectral::size(); ++c) {
        albedo[c] = std::min(albedo[c], cap);
    }
    return albedo * reflectance;
}

template <IsSpectral TSpectral>
BSDFSample<TSpectral> McEwenBSDF<TSpectral>::sample(const Vec3<float>& wo,
                                                    const Interaction<TSpectral>& isect,
                                                    const ShadingParams<TSpectral>& params,
                                                    float u1,
                                                    float u2) const
{
    auto hs = sampling::cosine_hemisphere(u1, u2);
    const Vec3<float> wi =
        sampling::local_to_world(hs.direction, isect.tangent, isect.bitangent, isect.normal_s);

    const float cos_i = glm::dot(wi, isect.normal_s);
    const float cos_o = glm::dot(wo, isect.normal_s);

    if (cos_i <= 0.0f || cos_o <= 0.0f) {
        BSDFSample<TSpectral> result{};
        result.wi = wi;
        result.value = TSpectral{0};
        result.pdf = 0.f;
        return result;
    }

    TSpectral f = this->eval(wo, wi, isect, params);

    BSDFSample<TSpectral> result{};
    result.wi = wi;
    result.value = f * PI<float>();
    result.pdf = hs.pdf;
    return result;
}

template <IsSpectral TSpectral>
float McEwenBSDF<TSpectral>::pdf(const Vec3<float>& wo,
                                 const Vec3<float>& wi,
                                 const Interaction<TSpectral>& isect,
                                 const ShadingParams<TSpectral>& params) const
{
    (void)params;

    const float cos_i = glm::dot(wi, isect.normal_s);
    const float cos_o = glm::dot(wo, isect.normal_s);

    if (cos_i <= 0.0f || cos_o <= 0.0f) {
        return 0.0f;
    }

    return cos_i * INV_PI<float>();
}
} // namespace huira
