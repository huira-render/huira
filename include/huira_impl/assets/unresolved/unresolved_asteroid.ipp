#include <cmath>
#include <memory>
#include <vector>

#include "glm/glm.hpp"

#include "huira/assets/lights/light.hpp"
#include "huira/core/concepts/spectral_concepts.hpp"
#include "huira/core/constants.hpp"
#include "huira/core/physics.hpp"
#include "huira/core/types.hpp"
#include "huira/scene/scene_view_types.hpp"
#include "huira/util/logger.hpp"

namespace huira {
    /**
     * @brief Computes the first basis function for the H-G magnitude system.
     *
     * Calculates \f$\phi_1(\alpha)\f$, one of the two basis functions used in the IAU H-G
     * photometric system for asteroids. This function models the scattering
     * behavior at various phase angles.
     *
     * @param alpha The phase angle in radians.
     * @return The value of \f$\phi_1(\alpha)\f$.
     */
    static inline double asteroid_phi1(double alpha)
    {
        double A = 3.33;
        double B = 0.64;
        return std::exp(-A * std::pow(std::tan(alpha / 2.), B));
    };

    /**
     * @brief Computes the second basis function for the H-G magnitude system.
     *
     * Calculates \f$\phi_2(\alpha)\f$, the second basis function used in the IAU H-G
     * photometric system for asteroids. This function complements \f$\phi_1\f$ to
     * model the complete phase function.
     *
     * @param alpha The phase angle in radians.
     * @return The value of \f$\phi_2(\alpha)\f$.
     */
    static inline double asteroid_phi2(double alpha)
    {
        double A = 1.87;
        double B = 1.22;
        return std::exp(-A * std::pow(std::tan(alpha / 2.), B));
    };

    /**
     * @brief Constructs an UnresolvedAsteroid with H-G magnitude parameters.
     *
     * Initializes an asteroid object using the H-G photometric system. The constructor
     * validates that the provided light instance actually contains a Light object.
     *
     * @param H Absolute magnitude (H) - the asteroid's brightness at 1 AU from both
     *          the Sun and observer with zero phase angle.
     * @param G Slope parameter (G) - controls the shape of the phase function,
     *          typically ranging from 0 to 1.
     * @param light_instance Handle to the Instance containing the illuminating light source.
     * @param albedo Spectral albedo of the asteroid (default: 1.0 for all wavelengths).
     * @throws std::runtime_error if the light_instance does not contain a Light.
     */
    template <IsSpectral TSpectral>
    UnresolvedAsteroid<TSpectral>::UnresolvedAsteroid(double H, double G, InstanceHandle<TSpectral> light_instance, TSpectral albedo) :
        H_{ H }, G_{ G },
        light_instance_{ light_instance.get() },
        albedo_{ albedo }
    {
        const Instantiable<TSpectral>& asset = light_instance_->asset();
        auto* light_ptr = std::get_if<Light<TSpectral>*>(&asset);
        if (!light_ptr) {
            HUIRA_THROW_ERROR("UnresolvedAsteroid::UnresolvedAsteroid - Requires an Instance containing a Light");
        }
        light_ = *light_ptr;

        if (std::isnan(H) || std::isinf(H)) {
            HUIRA_THROW_ERROR("UnresolvedAsteroid::UnresolvedAsteroid - H must be real and finite");
        }
        if (std::isnan(G) || std::isinf(G)) {
            HUIRA_THROW_ERROR("UnresolvedAsteroid::UnresolvedAsteroid - G must be real and finite");
        }

        if (!albedo.valid_ratio()) {
            HUIRA_THROW_ERROR("UnresolvedAsteroid::UnresolvedAsteroid - Invalid spectral albedo: " +
                albedo.to_string());
        }
    }

    /**
     * @brief Constructs an UnresolvedAsteroid with H-G magnitude parameters.
     *
     * Initializes an asteroid object using the H-G photometric system. The constructor
     * validates that the provided light instance actually contains a Light object.
     *
     * @param H Absolute magnitude (H) - the asteroid's brightness at 1 AU from both
     *          the Sun and observer with zero phase angle.
     * @param G Slope parameter (G) - controls the shape of the phase function,
     *          typically ranging from 0 to 1.
     * @param light_instance Handle to the Instance containing the illuminating light source.
     * @param albedo Constant spectral albedo of the asteroid.
     * @throws std::runtime_error if the light_instance does not contain a Light.
     */
    template <IsSpectral TSpectral>
    UnresolvedAsteroid<TSpectral>::UnresolvedAsteroid(double H, double G, InstanceHandle<TSpectral> light_instance, float albedo) :
        H_{ H }, G_{ G },
        light_instance_{ light_instance.get() },
        albedo_{ TSpectral{ albedo } }
    {
        const Instantiable<TSpectral>& asset = light_instance_->asset();
        auto* light_ptr = std::get_if<Light<TSpectral>*>(&asset);
        if (!light_ptr) {
            HUIRA_THROW_ERROR("UnresolvedAsteroid::UnresolvedAsteroid - Requires an Instance containing a Light");
        }
        light_ = *light_ptr;

        if (std::isnan(H) || std::isinf(H)) {
            HUIRA_THROW_ERROR("UnresolvedAsteroid::UnresolvedAsteroid - H must be real and finite");
        }
        if (std::isnan(G) || std::isinf(G)) {
            HUIRA_THROW_ERROR("UnresolvedAsteroid::UnresolvedAsteroid - G must be real and finite");
        }

        if (!albedo_.valid_ratio()) {
            HUIRA_THROW_ERROR("UnresolvedAsteroid::UnresolvedAsteroid - Invalid albedo: " +
                std::to_string(albedo));
        }
    }

    /**
     * @brief Resolves the spectral irradiance based on asteroid photometry.
     *
     * Computes the asteroid's apparent brightness using the H-G magnitude system,
     * accounting for the distances to the Sun and observer, as well as the phase
     * angle. The irradiance is then set based on the computed apparent visual
     * magnitude and the asteroid's albedo.
     *
     * @param self_transform The world-space transform of the asteroid.
     * @param lights A vector of all light instances in the scene.
     * @throws std::runtime_error if the asteroid's light source is not found in the scene,
     *         or if geometry is invalid (zero distances).
     */
    template <IsSpectral TSpectral>
    void UnresolvedAsteroid<TSpectral>::resolve_irradiance(
        const Transform<float>& self_transform,
        const std::vector<LightInstance<TSpectral>>& lights
    ) {
        for (const auto& light_inst : lights) {
            if (light_inst.light.get() == light_) {
                Vec3<double> to_obs = -self_transform.position;
                Vec3<double> to_light = light_inst.transform.position - self_transform.position;

                // Normalize vectors for angles
                double delta_m = glm::length(to_obs);
                double r_m = glm::length(to_light);
                if (r_m <= 0 || delta_m <= 0) {
                    HUIRA_THROW_ERROR("UnresolvedAsteroid::resolve_irradiance - Distances must be greater than zero.");
                }

                Vec3<double> to_obs_n = to_obs / delta_m;
                Vec3<double> to_light_n = to_light / r_m;

                // Phase Angle (alpha)
                // Cos(alpha) = dot product of (Vector to Sun) and (Vector to Observer)
                double cos_alpha = glm::dot(to_light_n, to_obs_n);
                cos_alpha = std::max(-1.0, std::min(1.0, cos_alpha));
                double alpha_rad = std::acos(cos_alpha);

                // Compute H-G Magnitude:
                double phi1 = asteroid_phi1(alpha_rad);
                double phi2 = asteroid_phi2(alpha_rad);
                double reduced_mag = H_ - 2.5 * std::log10((1.0 - G_) * phi1 + G_ * phi2);

                double r_au = r_m * AU<double>();
                double delta_au = delta_m * AU<double>();
                double apparent_visual_mag = reduced_mag + 5.0 * std::log10(r_au * delta_au);

                TSpectral computed_irradiance = visual_magnitude_to_irradiance<TSpectral>(apparent_visual_mag, albedo_);

                if (!computed_irradiance.valid()) {
                    HUIRA_THROW_ERROR("UnresolvedAsteroid::resolve_irradiance - Computed invalid irradiance: " +
                        computed_irradiance.to_string());
                }

                this->irradiance_ = computed_irradiance;
                return;
            }
        }
        HUIRA_THROW_ERROR("UnresolvedAsteroid::resolve_irradiance - Could not find its light source in SceneView");
    }
}
