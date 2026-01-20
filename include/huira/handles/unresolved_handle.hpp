#pragma once

#include <string>

#include "huira/detail/concepts/numeric_concepts.hpp"
#include "huira/detail/concepts/spectral_concepts.hpp"
#include "huira/core/types.hpp"

#include "huira/handles/handle.hpp"
#include "huira/scene/unresolved_object.hpp"

namespace huira {
    // Forward Declare:
    template <IsSpectral TSpectral, IsFloatingPoint TFloat>
    class FrameHandle;

    template <IsSpectral TSpectral, IsFloatingPoint TFloat>
    class UnresolvedHandle : public Handle<UnresolvedObject<TSpectral, TFloat>> {
    public:
        UnresolvedHandle() = delete;
        using Handle<UnresolvedObject<TSpectral, TFloat>>::Handle;


        void set_irradiance(const TSpectral& irradiance) const;
        TSpectral get_irradiance() const;


        // Position
        void set_position(const Vec3<TFloat>& position) const;
        void set_position(double x, double y, double z) const;

        // Velocity
        void set_velocity(const Vec3<TFloat>& velocity) const;
        void set_velocity(double vx, double vy, double vz) const;

        // SPICE origin only (no frame for point objects)
        void set_spice_origin(const std::string& spice_origin) const;
        std::string get_spice_origin() const;
    };
}

#include "huira_impl/handles/unresolved_handle.ipp"
