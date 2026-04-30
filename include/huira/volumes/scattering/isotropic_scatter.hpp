#pragma once

#include "huira/core/concepts/spectral_concepts.hpp"
#include "huira/core/constants.hpp"
#include "huira/core/types.hpp"
#include "huira/render/sampler.hpp"
#include "huira/volumes/scattering/phase_function.hpp"

namespace huira {

    template <IsSpectral TSpectral>
    class IsotropicPhaseFunction : public PhaseFunction<TSpectral> {
    public:
        IsotropicPhaseFunction() = default;
        ~IsotropicPhaseFunction() override = default;
        
        [[nodiscard]] float evaluate(const Vec3<float>& wo, const Vec3<float>& wi) const override {
            (void)wo;
            (void)wi;
            return 1.0f / (4.0f * PI<float>());
        }

        [[nodiscard]] PhaseSample sample(const Vec3<float>& wo, RandomSampler<float>& sampler) const override {
            (void)wo;
            
            Vec2<float> u = sampler.get_2d();
            
            float z = 1.0f - 2.0f * u.x;
            float r = std::sqrt(std::max(0.0f, 1.0f - z * z));
            float phi = 2.0f * PI<float>() * u.y;

            Vec3<float> wi{ r * std::cos(phi), r * std::sin(phi), z };
            
            return { wi, evaluate(Vec3<float>{}, wi) }; 
        }
    };

}
