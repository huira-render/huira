#pragma once

#include "huira/detail/concepts/numeric_concepts.hpp"
#include "huira/detail/concepts/spectral_concepts.hpp"
#include "huira/handles/handle.hpp"

namespace huira {
    template <IsSpectral TSpectral, typename TNode>
    class PointHandle : public Handle<TNode> {
    public:
        PointHandle() = delete;
        using Handle<TNode>::Handle;

        // Position
        void set_position(const Vec3<double>& position) const;
        void set_position(double x, double y, double z) const;

        Vec3<double> get_global_position() const;
        Vec3<double> get_local_position() const;


        // Velocity
        void set_velocity(const Vec3<double>& velocity) const;
        void set_velocity(double vx, double vy, double vz) const;

        Vec3<double> get_global_velocity() const;
        Vec3<double> get_local_velocity() const;


        // SPICE
        void set_spice_origin(const std::string& spice_origin) const;

        std::string get_spice_origin() const;

        Vec3<double> get_position_in_frame(const std::string& target_origin, const std::string& target_frame) const;
        Vec3<double> get_velocity_in_frame(const std::string& target_origin, const std::string& target_frame) const;

        std::pair<Vec3<double>, Vec3<double>> get_state_in_frame(const std::string& target_origin, const std::string& target_frame) const;
    };
}

#include "huira_impl/handles/point_handle.ipp"
