#pragma once

#include "huira/core/concepts/numeric_concepts.hpp"
#include "huira/core/concepts/spectral_concepts.hpp"
#include "huira/handles/handle.hpp"
#include "huira/scene/node.hpp"

namespace huira {
    template <IsSpectral TSpectral, typename TNode>
    class NodeHandle : public Handle<TNode> {
    public:
        NodeHandle() = delete;
        using Handle<TNode>::Handle;

        // Position
        void set_position(const Vec3<double>& position) const;
        void set_position(double x, double y, double z) const;
        Vec3<double> get_static_position() const;


        // Velocity
        void set_velocity(const Vec3<double>& velocity) const;
        void set_velocity(double vx, double vy, double vz) const;
        Vec3<double> get_static_velocity() const;


        // Rotation
        void set_rotation(const Rotation<double>& rotation) const;

        void set_rotation_local_to_parent(const Mat3<double>& matrix) const;
        void set_rotation_local_to_parent(const Quaternion<double>& quaternion) const;
        void set_rotation_local_to_parent(const Vec3<double>& axis, units::Degree angle) const;

        void set_rotation_parent_to_local(const Mat3<double>& matrix) const;
        void set_rotation_parent_to_local(const Quaternion<double>& quaternion) const;
        void set_rotation_parent_to_local(const Vec3<double>& axis, units::Degree angle) const;

        void set_euler_angles(units::Radian x, units::Radian y, units::Radian z, std::string sequence = "XYZ") const;
        Rotation<double> get_static_rotation() const;


        // Angular velocity
        void set_angular_velocity(const Vec3<double>& angular_velocity) const;
        void set_angular_velocity(double wx, double wy, double wz) const;
        Vec3<double> get_static_angular_velocity() const;



        // Scale
        void set_scale(const Vec3<double>& scale) const;
        void set_scale(double sx, double sy, double sz) const;
        void set_scale(double s) const;
        Vec3<double> get_static_scale() const;



        // SPICE
        void set_spice_origin(const std::string& spice_origin) const;
        void set_spice_frame(const std::string& spice_frame) const;
        void set_spice(const std::string& spice_origin, const std::string& spice_frame) const;
        std::string get_spice_origin() const;
        std::string get_spice_frame() const;


        // Parent access
        NodeHandle<TSpectral, Node<TSpectral>> get_parent() const;
        
        template <typename TParentNode>
        NodeHandle<TSpectral, TParentNode> get_parent_as() const;
    };
}

#include "huira_impl/handles/node_handle.ipp"
