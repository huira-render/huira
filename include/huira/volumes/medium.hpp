#pragma once

#include <memory>
#include <optional>

#include "huira/core/concepts/spectral_concepts.hpp"
#include "huira/core/types.hpp"
#include "huira/geometry/ray.hpp"
#include "huira/render/sampler.hpp"
#include "huira/scene/scene_object.hpp"
#include "huira/volumes/medium_properties.hpp"
#include "huira/volumes/density/density_field.hpp"
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
            : density_field_(std::move(density_field)),
            phase_function_(std::move(phase_function)) {}

        virtual ~Medium() = default;

        // Prevent copying due to unique_ptr ownership
        Medium(const Medium&) = delete;
        Medium& operator=(const Medium&) = delete;

        // Move semantics
        Medium(Medium&&) noexcept = default;
        Medium& operator=(Medium&&) noexcept = default;

        // Queries the density field to return optical properties at a specific point
        [[nodiscard]] MediumProperties<TSpectral> get_properties(const Vec3<float>& p) const {
            return density_field_->evaluate(p);
        }

        // Expose the phase function for the integrator to use upon a scattering event
        [[nodiscard]] const PhaseFunction<TSpectral>& get_phase_function() const {
            return *phase_function_;
        }

        // Returns transmittance along a segment (could be analytical for constant media, 
        // or delegated to an integrator/marching utility for heterogeneous media)
        [[nodiscard]] TSpectral evaluate_transmittance(const Ray<TSpectral>& ray,
            RandomSampler<float>& sampler) const;

        // Samples a distance along the ray to determine if/where a scattering event occurs.
        // Returns the distance 't' and the sampled optical properties, or std::nullopt if the ray escapes.
        [[nodiscard]] std::optional<MediumInteraction<TSpectral>> sample_free_path(
            const Ray<TSpectral>& ray, RandomSampler<float>& sampler) const;

    private:
        std::shared_ptr<DensityField<TSpectral>> density_field_;
        std::shared_ptr<PhaseFunction<TSpectral>> phase_function_;
    };

}
