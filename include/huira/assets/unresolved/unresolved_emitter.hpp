#pragma once

#include <string>
#include <vector>

#include "huira/assets/lights/light.hpp"
#include "huira/assets/unresolved/unresolved_object.hpp"
#include "huira/core/concepts/spectral_concepts.hpp"
#include "huira/core/units/units.hpp"
#include "huira/scene/scene_view_types.hpp"

namespace huira {
    /**
     * @brief Represents an unresolved isotropic point source with spectral power.
     * 
     * UnresolvedEmitter models a self-luminous object that emits light uniformly
     * in all directions. The irradiance is computed using inverse square law based
     * on the distance from the observer (assumed to be at the origin) and the
     * total spectral power.
     * 
     * @tparam TSpectral The spectral representation type.
     */
    template <IsSpectral TSpectral>
    class UnresolvedEmitter : public UnresolvedObject<TSpectral> {
    public:
        UnresolvedEmitter(const units::SpectralWatts<TSpectral>& spectral_power);
        UnresolvedEmitter(const units::Watt& power);

        void resolve_irradiance(
            const Transform<float>& self_transform,
            const std::vector<LightInstance<TSpectral>>& lights
        ) override;

        void set_spectral_power(const units::SpectralWatts<TSpectral>& spectral_power);
        void set_spectral_power(const units::Watt& power);

        std::string type() const override { return "UnresolvedEmitter"; }

    private:
        TSpectral spectral_power_;
    };
}

#include "huira_impl/assets/unresolved/unresolved_emitter.ipp"
