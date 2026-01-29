#pragma once

#include "huira/assets/unresolved_object.hpp"
#include "huira/core/concepts/spectral_concepts.hpp"
#include "huira/handles/handle.hpp"

namespace huira {
    // Forward declaration
    template <IsSpectral TSpectral>
    class FrameHandle;

    template <IsSpectral TSpectral>
    class UnresolvedObjectHandle : public Handle<UnresolvedObject<TSpectral>> {
    public:
        UnresolvedObjectHandle() = delete;
        using Handle<UnresolvedObject<TSpectral>>::Handle;


        void set_irradiance(const TSpectral& irradiance) const;
        TSpectral get_irradiance() const;

        friend class Scene<TSpectral>;
        friend class FrameHandle<TSpectral>;
    };
}

#include "huira_impl/handles/unresolved_handle.ipp"
