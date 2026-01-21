#pragma once

#include "huira/detail/concepts/numeric_concepts.hpp"
#include "huira/detail/concepts/spectral_concepts.hpp"
#include "huira/handles/handle.hpp"
#include "huira/scene/node.hpp"

namespace huira {
    template <IsSpectral TSpectral, IsFloatingPoint TFloat, typename TNode>
    class PointHandle : public Handle<TNode> {
    public:
        using Handle<TNode>::Handle;

        // Position
        void set_position(const Vec3<TFloat>& position) const;
        void set_position(double x, double y, double z) const;

        Vec3<TFloat> get_global_position() const;
        Vec3<TFloat> get_local_position() const;


        // Velocity
        void set_velocity(const Vec3<TFloat>& velocity) const;
        void set_velocity(double vx, double vy, double vz) const;

        Vec3<TFloat> get_global_velocity() const;
        Vec3<TFloat> get_local_velocity() const;


        // SPICE
        void set_spice_origin(const std::string& spice_origin) const;

        std::string get_spice_origin() const;
    };
}

#include "huira_impl/handles/point_handle.ipp"
