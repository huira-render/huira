#include <optional>

#include "glm/glm.hpp"

#include "huira/core/types.hpp"
#include "huira/core/transform.hpp"
#include "huira/render/interaction.hpp"
#include "huira/render/sampler.hpp"
#include "huira/core/concepts/spectral_concepts.hpp"

namespace huira {
    template <IsSpectral TSpectral>
    std::optional<LightSample<TSpectral>> PointLight<TSpectral>::sample_li(
        const Interaction<TSpectral>& ref,
        const Transform<float>& light_to_world,
        const Sampler<float>& sampler
    ) const
    {
        (void)sampler;
        LightSample<TSpectral> sample;
        
        Vec3<float> p_light = light_to_world.apply_to_point(Vec3<float>(0, 0, 0));

        // Compute shading point:
        Vec3<float> wi_unorm = p_light - ref.position;
        float dist_sq = glm::dot(wi_unorm, wi_unorm);
        sample.distance = std::sqrt(dist_sq);

        // Shading point exactly coincides with light:
        if (sample.distance == 0) {
            return std::nullopt;
        }

        sample.wi = wi_unorm / sample.distance;

        // Compute irradiance (radiance for point light):
        sample.Li = intensity_ / dist_sq;
        
        // Point lights have PDF of 1
        sample.pdf = 1.0f;

        return sample;
    }

    template <IsSpectral TSpectral>
    float PointLight<TSpectral>::pdf_li(
        const Interaction<TSpectral>& ref,
        const Transform<float>& light_to_world,
        const Vec3<float>& wi
    ) const
    {
        (void)ref;
        (void)light_to_world;
        (void)wi;
        return 1.f;
    }

    template <IsSpectral TSpectral>
    TSpectral PointLight<TSpectral>::irradiance_at(
        const Vec3<float>& position,
        const Transform<float>& light_to_world
    ) const
    {
        Vec3<float> p_light = light_to_world.apply_to_point(Vec3<float>(0, 0, 0));
        float dist_sq = glm::dot(position - p_light, position - p_light);
        return intensity_ / dist_sq;
    }
}
