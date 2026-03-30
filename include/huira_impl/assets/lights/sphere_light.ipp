#include <algorithm>
#include <cmath>

#include "glm/glm.hpp"

#include "huira/core/constants.hpp"
#include "huira/core/physics.hpp"
#include "huira/util/logger.hpp"

namespace huira {
    /**
     * @brief Constructs a SphereLight from spectral power.
     * @param radius The radius of the sphere in meters.
     * @param spectral_power Total spectral power emitted over the entire surface.
     */
    template <IsSpectral TSpectral>
    SphereLight<TSpectral>::SphereLight(const units::Meter& radius, const units::SpectralWatts<TSpectral>& spectral_power)
        : radius_(std::max(radius.to_si_f(), 1e-5f))
    {
        this->set_spectral_power(spectral_power);
    }

    /**
     * @brief Constructs a SphereLight from spectral radiance.
     * @param radius The radius of the sphere in meters.
     * @param spectral_radiance Spectral radiance emitted by the surface.
     */
    template <IsSpectral TSpectral>
    SphereLight<TSpectral>::SphereLight(const units::Meter& radius, const units::SpectralWattsPerMeterSquaredSteradian<TSpectral>& spectral_radiance)
        : radius_(std::max(radius.to_si_f(), 1e-5f)), radiance_{spectral_radiance.to_si()}
    {
        
    }

    /**
     * @brief Constructs a SphereLight from total scalar power.
     * @param radius The radius of the sphere in meters.
     * @param power Total power emitted in Watts.
     */
    template <IsSpectral TSpectral>
    SphereLight<TSpectral>::SphereLight(const units::Meter& radius, const units::Watt& power)
        : radius_(std::max(radius.to_si_f(), 1e-5f))
    {
        this->set_spectral_power(power);
    }

    /**
     * @brief Constructs a blackbody SphereLight from a temperature.
     * @param radius The radius of the sphere in meters.
     * @param power Temperature in Kelvin.
     */
    template <IsSpectral TSpectral>
    SphereLight<TSpectral>::SphereLight(const units::Meter& radius, const units::Kelvin& temperature)
        : radius_(std::max(radius.to_si_f(), 1e-5f))
    {
        this->radiance_ = black_body<TSpectral>(temperature.to_si(), 1000);
    }

    template <IsSpectral TSpectral>
    void SphereLight<TSpectral>::set_spectral_power(const units::Watt& power)
    {
        float power_si = static_cast<float>(power.to_si());
        if (power_si < 0.f || std::isnan(power_si) || std::isinf(power_si)) {
            HUIRA_THROW_ERROR("SphereLight::set_spectral_power - Invalid power: " +
                std::to_string(power_si));
        }

        // L = Phi / (4 * pi^2 * r^2)
        float surface_area = 4.0f * PI<float>() * radius_ * radius_;
        this->radiance_ = TSpectral{ power_si / (PI<float>() * surface_area) };
    }

    template <IsSpectral TSpectral>
    void SphereLight<TSpectral>::set_spectral_power(const units::SpectralWatts<TSpectral>& spectral_power)
    {
        // Assuming your units system allows extracting the raw TSpectral
        TSpectral power_si = spectral_power.to_si();
        float surface_area = 4.0f * PI<float>() * radius_ * radius_;
        this->radiance_ = power_si / (PI<float>() * surface_area);
    }

    template <IsSpectral TSpectral>
    std::optional<LightSample<TSpectral>> SphereLight<TSpectral>::sample_li(
        const Interaction<TSpectral>& isect,
        const Transform<float>& transform,
        Sampler<float>& sampler) const
    {
        Vec3<float> p_center = transform.position;
        Vec3<float> wc = p_center - isect.position;
        float d2 = glm::dot(wc, wc);
        float d = std::sqrt(d2);

        // If the shading point is inside the light sphere, it receives no direct lighting
        if (d <= radius_) {
            return std::nullopt;
        }

        // Calculate the cone subtended by the sphere
        float sin_theta_max2 = (radius_ * radius_) / d2;
        float cos_theta_max = std::sqrt(std::max(0.0f, 1.0f - sin_theta_max2));

        // Uniformly sample the cone
        float u1 = sampler.get_1d();
        float u2 = sampler.get_1d();

        float cos_theta = (1.0f - u1) + u1 * cos_theta_max;
        float sin_theta = std::sqrt(std::max(0.0f, 1.0f - cos_theta * cos_theta));
        float phi = u2 * 2.0f * PI<float>();

        // Build a local coordinate system around the vector to the center (wc)
        Vec3<float> w = wc / d;
        Vec3<float> u, v;
        if (std::abs(w.z) < 0.999f) {
            u = glm::normalize(Vec3<float>(-w.y, w.x, 0.0f));
        }
        else {
            u = glm::normalize(Vec3<float>(0.0f, -w.z, w.y));
        }
        v = glm::cross(w, u);

        // Convert sampled local direction to world space
        Vec3<float> wi = u * (sin_theta * std::cos(phi)) +
            v * (sin_theta * std::sin(phi)) +
            w * cos_theta;

        // Calculate Solid Angle and PDF
        float solid_angle = 2.0f * PI<float>() * (1.0f - cos_theta_max);

        // Robustness fallback for tiny spheres or immense distances 
        // to prevent catastrophic cancellation in floating point math
        if (solid_angle < 1e-7f) {
            solid_angle = PI<float>() * sin_theta_max2;
        }

        // Calculate distance to the sphere surface along wi using standard ray-sphere intersection:
        float b = glm::dot(wc, wi);
        float discriminant = b * b - d2 + (radius_ * radius_);
        float distance = b - std::sqrt(std::max(0.0f, discriminant));

        LightSample<TSpectral> ls;
        ls.wi = wi;
        ls.Li = radiance_;
        ls.pdf = 1.0f / solid_angle;
        ls.distance = distance;

        return ls;
    }

    template <IsSpectral TSpectral>
    float SphereLight<TSpectral>::pdf_li(
        const Interaction<TSpectral>& isect,
        const Transform<float>& transform,
        const Vec3<float>& wi) const
    {
        Vec3<float> p_center = transform.position;
        Vec3<float> wc = p_center - isect.position;
        float d2 = glm::dot(wc, wc);

        // If inside the sphere, probability is 0
        if (d2 <= radius_ * radius_) return 0.0f;

        float sin_theta_max2 = (radius_ * radius_) / d2;
        float cos_theta_max = std::sqrt(std::max(0.0f, 1.0f - sin_theta_max2));

        // Check if the given direction `wi` actually hits the sphere cone
        float cos_theta = glm::dot(wi, glm::normalize(wc));
        if (cos_theta < cos_theta_max) {
            return 0.0f; // Ray missed the sphere
        }

        float solid_angle = 2.0f * PI<float>() * (1.0f - cos_theta_max);
        if (solid_angle < 1e-7f) {
            solid_angle = PI<float>() * sin_theta_max2;
        }

        return 1.0f / solid_angle;
    }

    template <IsSpectral TSpectral>
    TSpectral SphereLight<TSpectral>::radiance(const Vec3<float>& point_on_light, const Vec3<float>& outgoing_direction) const
    {
        (void)point_on_light;
        (void)outgoing_direction;
        return radiance_;
    }

    template <IsSpectral TSpectral>
    TSpectral SphereLight<TSpectral>::irradiance_at(
        const Vec3<float>& position,
        const Transform<float>& light_to_world) const
    {
        Vec3<float> wc = light_to_world.position - position;
        float d2 = glm::dot(wc, wc);
        float r2 = radius_ * radius_;

        if (d2 <= r2) {
            return TSpectral{ 0 };
        }

        // E = L * pi * (r^2 / d^2)
        float sin2_theta_max = r2 / d2;
        return radiance_ * PI<float>() * sin2_theta_max;
    }
}
