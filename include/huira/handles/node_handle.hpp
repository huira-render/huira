#pragma once

#include "huira/detail/concepts/numeric_concepts.hpp"
#include "huira/detail/concepts/spectral_concepts.hpp"
#include "huira/handles/handle.hpp"

namespace huira {
    template <IsSpectral TSpectral, IsFloatingPoint TFloat, typename TNode>
    class NodeHandle : public Handle<TNode> {
    public:
        NodeHandle() = delete;
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



        // Rotation
        void set_rotation(const Rotation<TFloat>& rotation) const;

        Rotation<TFloat> get_global_rotation() const;
        Rotation<TFloat> get_local_rotation() const;



        // Angular velocity
        void set_angular_velocity(const Vec3<TFloat>& angular_velocity) const;
        void set_angular_velocity(double wx, double wy, double wz) const;

        Vec3<TFloat> get_global_angular_velocity() const;
        Vec3<TFloat> get_local_angular_velocity() const;



        // Scale
        void set_scale(const Vec3<TFloat>& scale) const;
        void set_scale(double sx, double sy, double sz) const;
        void set_scale(double s) const;

        Vec3<TFloat> get_global_scale() const;
        Vec3<TFloat> get_local_scale() const;



        // SPICE
        void set_spice_origin(const std::string& spice_origin) const;
        void set_spice_frame(const std::string& spice_frame) const;
        void set_spice(const std::string& spice_origin, const std::string& spice_frame) const;

        std::string get_spice_origin() const;
        std::string get_spice_frame() const;      
    };
}

#include "huira_impl/handles/node_handle.ipp"
