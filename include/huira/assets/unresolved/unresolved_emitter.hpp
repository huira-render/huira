#pragma once

#include <vector>
#include <string>

#include "huira/assets/unresolved/unresolved_object.hpp"
#include "huira/core/concepts/spectral_concepts.hpp"
#include "huira/assets/lights/light.hpp"
#include "huira/scene/scene_view_types.hpp"

namespace huira {
    template <IsSpectral TSpectral>
    class UnresolvedEmitter : public UnresolvedObject<TSpectral> {
    public:
        UnresolvedEmitter(TSpectral spectral_power) : spectral_power_{ spectral_power } {}

        void resolve_irradiance(
            const Transform<float>& self_transform,
            const std::vector<LightInstance<TSpectral>>& lights
        ) override;

        std::string type() const override { return "UnresolvedEmitter"; }

    private:
        TSpectral spectral_power_;
    };
}

#include "huira_impl/assets/unresolved/unresolved_emitter.ipp"
