#pragma once

#include <memory>
#include <optional>
#include <string>

#include "huira/core/concepts/spectral_concepts.hpp"
#include "huira/core/transform.hpp"
#include "huira/core/types.hpp"
#include "huira/render/interaction.hpp"
#include "huira/render/sampler.hpp"
#include "huira/scene/scene_object.hpp"

namespace huira {
    
    /**
     * @brief Specifies the type of light source.
     */
    enum class LightType {
        Sphere
    };

    /**
     * @brief Represents a sampled light contribution at a point.
     * 
     * @tparam TSpectral The spectral representation type.
     */
    template <IsSpectral TSpectral>
    struct LightSample {
        Vec3<float> wi;      ///< Incident direction from surface to light (normalized)
        TSpectral Li;        ///< Incident radiance from the light
        float distance;      ///< Distance from the surface point to the light
        float pdf;           ///< Probability density function value for this sample
    };


    /**
     * @brief Abstract base class for all light sources in the scene.
     * 
     * Provides an interface for sampling light contributions, evaluating PDFs,
     * and computing irradiance. All lights have a unique ID for identification.
     * 
     * @tparam TSpectral The spectral representation type.
     */
    template <IsSpectral TSpectral>
    class Light : public SceneObject<Light<TSpectral>> {
    public:
        Light() = default;

        Light(const Light&) = delete;
        Light& operator=(const Light&) = delete;

        virtual ~Light() override = default;

        virtual std::optional<LightSample<TSpectral>> sample_li(
            const Interaction<TSpectral>& ref,
            const Transform<float>& light_to_world,
            Sampler<float>& sampler
        ) const = 0;

        virtual float pdf_li(
            const Interaction<TSpectral>& ref,
            const Transform<float>& light_to_world,
            const Vec3<float>& wi
        ) const = 0;

        virtual TSpectral radiance(const Vec3<float>& point_on_light, const Vec3<float>& outgoing_direction) const = 0;

        virtual TSpectral irradiance_at(
            const Vec3<float>& position,
            const Transform<float>& light_to_world
        ) const = 0;

        virtual LightType get_type() const = 0;

        virtual std::string type() const override { return "Light"; }
    };
}
