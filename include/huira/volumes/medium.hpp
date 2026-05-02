#pragma once

#include <memory>
#include <optional>
#include <string>

#include "huira/concepts/spectral_concepts.hpp"
#include "huira/core/types.hpp"
#include "huira/geometry/ray.hpp"
#include "huira/render/sampler.hpp"
#include "huira/scene/scene_object.hpp"
#include "huira/volumes/density/density_field.hpp"
#include "huira/volumes/medium_interaction.hpp"
#include "huira/volumes/medium_properties.hpp"
#include "huira/volumes/scattering/phase_function.hpp"

namespace huira {
// Forward Declare
template <IsSpectral TSpectral>
class Scene;

template <IsSpectral TSpectral>
class Medium : public SceneObject<Medium<TSpectral>> {
  public:
    // Explicit constructor enforcing composition
    Medium(std::shared_ptr<DensityField<TSpectral>> density_field,
           std::shared_ptr<PhaseFunction<TSpectral>> phase_function)
        : density_field_(std::move(density_field)), phase_function_(std::move(phase_function))
    {
    }

    virtual ~Medium() override = default;

    // Prevent copying due to unique_ptr ownership
    Medium(const Medium&) = delete;
    Medium& operator=(const Medium&) = delete;

    // Move semantics
    Medium(Medium&&) noexcept = default;
    Medium& operator=(Medium&&) noexcept = default;

    // Queries the density field to return optical properties at a specific point
    [[nodiscard]] MediumProperties<TSpectral> get_properties(const Vec3<float>& p) const
    {
        return density_field_->evaluate(p);
    }

    // Expose the phase function for the integrator to use upon a scattering event
    [[nodiscard]] const PhaseFunction<TSpectral>& get_phase_function() const
    {
        return *phase_function_;
    }

    // Returns transmittance along a segment (could be analytical for constant media,
    // or delegated to an integrator/marching utility for heterogeneous media)
    [[nodiscard]] TSpectral
    evaluate_transmittance(const Ray<TSpectral>& ray, float t, RandomSampler<float>& sampler) const
    {
        (void)sampler;

        if (t <= 0.0f) {
            return TSpectral{1.0f};
        }
        
        MediumProperties<TSpectral> props = get_properties(ray.origin());
        TSpectral ext = props.extinction();

        TSpectral Tr{0.0f};
        for (std::size_t c = 0; c < TSpectral::size(); ++c) {
            Tr[c] = std::exp(-ext[c] * t);
        }
        return Tr;
    }

    // Samples a distance along the ray to determine if/where a scattering event occurs.
    // Returns the distance 't' and the sampled optical properties, or std::nullopt if the ray
    // escapes.
    [[nodiscard]] std::optional<MediumInteraction<TSpectral>>
    sample_free_path(const Ray<TSpectral>& ray, RandomSampler<float>& sampler) const
    {
        // Query properties at the ray origin (perfect for our ConstantDensityField)
        MediumProperties<TSpectral> props = get_properties(ray.origin());
        TSpectral ext = props.extinction();

        // Compute scalar average extinction for sampling
        float avg_ext = 0.0f;
        for (std::size_t c = 0; c < TSpectral::size(); ++c) {
            avg_ext += ext[c];
        }
        avg_ext /= static_cast<float>(TSpectral::size());

        // If the medium is practically a vacuum, the ray escapes infinitely
        if (avg_ext <= 1e-6f) {
            return std::nullopt;
        }

        // Inverse Beer-Lambert sampling
        float t = -std::log(1.0f - sampler.get_1d()) / avg_ext;

        return MediumInteraction<TSpectral>(
            ray.at(t), t, -ray.direction(), props, phase_function_.get());
    }

    std::string type() const override { return "Medium"; }

  private:
    std::shared_ptr<DensityField<TSpectral>> density_field_;
    std::shared_ptr<PhaseFunction<TSpectral>> phase_function_;
};

} // namespace huira
