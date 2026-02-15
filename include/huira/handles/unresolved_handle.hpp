#pragma once

#include "huira/assets/unresolved/unresolved_object.hpp"
#include "huira/core/concepts/spectral_concepts.hpp"
#include "huira/core/time.hpp"
#include "huira/core/units/units.hpp"
#include "huira/handles/handle.hpp"

namespace huira {

    /**
     * @brief Handle for unresolved scene objects.
     *
     * Provides access and control for unresolved scene objects, allowing irradiance to be set or queried.
     * This handle is used to interact with unresolved objects in a type-safe manner.
     *
     * @tparam TSpectral The spectral representation type.
     */
    template <IsSpectral TSpectral>
    class UnresolvedObjectHandle : public Handle<UnresolvedObject<TSpectral>> {
    public:
        UnresolvedObjectHandle() = delete;
        using Handle<UnresolvedObject<TSpectral>>::Handle;

        void set_irradiance(const units::SpectralWattsPerMeterSquared<TSpectral>& irradiance) const;
        void set_irradiance(const units::WattsPerMeterSquared& irradiance) const;
        
        TSpectral get_irradiance(Time time) const;

        friend class Scene<TSpectral>;
        friend class FrameHandle<TSpectral>;
    };
}

#include "huira_impl/handles/unresolved_handle.ipp"
