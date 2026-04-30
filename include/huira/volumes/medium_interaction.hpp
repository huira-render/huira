#pragma once

#include "huira/concepts/spectral_concepts.hpp"
#include "huira/core/types.hpp"
#include "huira/volumes/medium_properties.hpp"

namespace huira {
    // Forward declare
    template <IsSpectral TSpectral> class PhaseFunction;

    template <IsSpectral TSpectral>
    struct MediumInteraction {
        Vec3<float> p;
        float t;
        Vec3<float> wo;

        MediumProperties<TSpectral> properties;

        const PhaseFunction<TSpectral>* phase_function{ nullptr };

        MediumInteraction() = default;

        MediumInteraction(const Vec3<float>& p_set, float t_set, const Vec3<float>& wo_set,
            const MediumProperties<TSpectral>& properties_set,
            const PhaseFunction<TSpectral>* phase_function_set)
            : p(p_set), t(t_set), wo(wo_set), properties(properties_set), phase_function(phase_function_set) {}
        
        [[nodiscard]] bool is_valid() const {
            return phase_function != nullptr;
        }
    };

}
