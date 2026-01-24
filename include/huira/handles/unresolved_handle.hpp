#pragma once

#include "huira/detail/concepts/numeric_concepts.hpp"
#include "huira/detail/concepts/spectral_concepts.hpp"

#include "huira/handles/point_handle.hpp"
#include "huira/objects/unresolved_object.hpp"

namespace huira {
    template <IsSpectral TSpectral>
    class UnresolvedObjectHandle : public PointHandle<TSpectral, UnresolvedObject<TSpectral>> {
    public:
        UnresolvedObjectHandle() = delete;
        using PointHandle<TSpectral, UnresolvedObject<TSpectral>>::PointHandle;


        void set_irradiance(const TSpectral& irradiance) const;
        TSpectral get_irradiance() const;
    };
}

#include "huira_impl/handles/unresolved_handle.ipp"
