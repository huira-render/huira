#pragma once

#include <cstdint>
#include <string>

#include "huira/concepts/spectral_concepts.hpp"
#include "huira/core/types.hpp"
#include "huira/render/sampler.hpp"
#include "huira/scene/scene_object.hpp"

namespace huira {

struct PhaseSample {
    Vec3<float> wi;
    float p;
};

template <IsSpectral TSpectral>
class PhaseFunction : public SceneObject<PhaseFunction<TSpectral>> {
  public:
    PhaseFunction() = default;
    virtual ~PhaseFunction() override = default;

    [[nodiscard]] virtual float evaluate(const Vec3<float>& wo, const Vec3<float>& wi) const = 0;

    [[nodiscard]] virtual PhaseSample sample(const Vec3<float>& wo,
                                             RandomSampler<float>& sampler) const = 0;

    virtual std::string type() const override = 0;
};

} // namespace huira
