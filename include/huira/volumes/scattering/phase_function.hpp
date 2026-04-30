#pragma once

#include "huira/core/concepts/spectral_concepts.hpp"
#include "huira/core/types.hpp"
#include "huira/render/sampler.hpp"

namespace huira {
    
    struct PhaseSample {
        Vec3<float> wi;
        float p;
    };

    template <IsSpectral TSpectral>
    class PhaseFunction {
    public:
        PhaseFunction() = default;
        virtual ~PhaseFunction() = default;
        
        [[nodiscard]] virtual float evaluate(const Vec3<float>& wo, const Vec3<float>& wi) const = 0;

        [[nodiscard]] virtual PhaseSample sample(const Vec3<float>& wo, RandomSampler<float>& sampler) const = 0;
    };

}
