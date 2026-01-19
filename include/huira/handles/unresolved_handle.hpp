#pragma once

#include "huira/detail/concepts/numeric_concepts.hpp"
#include "huira/detail/concepts/spectral_concepts.hpp"

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

        // Position
        void set_position(const Vec3<TFloat>& position) const { this->get()->set_position(position); }
        void set_position(double x, double y, double z) const { this->get()->set_position(Vec3<TFloat>{x, y, z}); }

        // Velocity
        void set_velocity(const Vec3<TFloat>& velocity) const { this->get()->set_velocity(velocity); }
        void set_velocity(double vx, double vy, double vz) const { this->get()->set_velocity(Vec3<TFloat>{vx, vy, vz}); }

        // SPICE origin only (no frame for point objects)
        void set_spice_origin(const std::string& spice_origin) const { this->get()->set_spice_origin(spice_origin); }
    };
}
