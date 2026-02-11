#include <memory>
#include <vector>
#include <cmath>

#include "glm/glm.hpp"

#include "huira/core/concepts/spectral_concepts.hpp"
#include "huira/assets/lights/light.hpp"
#include "huira/util/logger.hpp"
#include "huira/scene/scene_view_types.hpp"
#include "huira/core/constants.hpp"
#include "huira/core/physics.hpp"
#include "huira/core/types.hpp"

namespace huira {
    static inline double asteroid_phi1(double alpha)
    {
        double A = 3.33;
        double B = 0.64;
        return std::exp(-A * std::pow(std::tan(alpha / 2.), B));
    };

    static inline double asteroid_phi2(double alpha)
    {
        double A = 1.87;
        double B = 1.22;
        return std::exp(-A * std::pow(std::tan(alpha / 2.), B));
    };

    template <IsSpectral TSpectral>
    UnresolvedAsteroid<TSpectral>::UnresolvedAsteroid(double H, double G, InstanceHandle<TSpectral> light_instance, TSpectral albedo) :
            H_ { H }, G_{ G }, 
            light_instance_{ light_instance.get() }, 
            albedo_{ albedo }
        {
            const Instantiable<TSpectral>& asset = light_instance_->asset();
            auto* light_ptr = std::get_if<Light<TSpectral>*>(&asset);
            if (!light_ptr) {
                HUIRA_THROW_ERROR("UnresolvedAsteroid requires an Instance containing a Light");
            }
            light_ = *light_ptr;
        }

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
                        HUIRA_THROW_ERROR("Invalid geometry for UnresolvedAsteroid: distances must be greater than zero.");
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

                    this->set_irradiance(visual_magnitude_to_irradiance<TSpectral>(apparent_visual_mag, albedo_));
                    return;
                }
            }
            HUIRA_THROW_ERROR("UnresolvedAsteroid could not find its light source in SceneView");
        }
}
