#pragma once

#include "huira/detail/concepts/numeric_concepts.hpp"
#include "huira/detail/concepts/spectral_concepts.hpp"

#include "huira/handles/handle.hpp"
#include "huira/camera/camera.hpp"

namespace huira {
    template <IsSpectral TSpectral, IsFloatingPoint TFloat>
    class CameraHandle : public Handle<Camera<TSpectral, TFloat>> {
    public:
        CameraHandle() = delete;
        using Handle<Camera<TSpectral, TFloat>>::Handle;

        // Position
        void set_position(const Vec3<TFloat>& position) const { this->get()->set_position(position); }
        void set_position(double x, double y, double z) const { this->get()->set_position(Vec3<TFloat>{x, y, z}); }

        // Velocity
        void set_velocity(const Vec3<TFloat>& velocity) const { this->get()->set_velocity(velocity); }
        void set_velocity(double vx, double vy, double vz) const { this->get()->set_velocity(Vec3<TFloat>{vx, vy, vz}); }

        // Rotation
        void set_rotation(const Rotation<TFloat>& rotation) const { this->get()->set_rotation(rotation); }

        // Angular velocity
        void set_angular_velocity(const Vec3<TFloat>& angular_velocity) const { this->get()->set_angular_velocity(angular_velocity); }
        void set_angular_velocity(double wx, double wy, double wz) const { this->get()->set_angular_velocity(Vec3<TFloat>{wx, wy, wz}); }

        // Scale
        void set_scale(const Vec3<TFloat>& scale) const { this->get()->set_scale(scale); }
        void set_scale(double sx, double sy, double sz) const { this->get()->set_scale(Vec3<TFloat>{sx, sy, sz}); }
        void set_scale(double s) const { this->get()->set_scale(Vec3<TFloat>{s, s, s}); }

        // SPICE
        void set_spice_origin(const std::string& spice_origin) const { this->get()->set_spice_origin(spice_origin); }
        void set_spice_frame(const std::string& spice_frame) const { this->get()->set_spice_frame(spice_frame); }
        void set_spice(const std::string& spice_origin, const std::string& spice_frame) const { this->get()->set_spice(spice_origin, spice_frame); }

        std::string get_spice_origin() const { return this->get()->get_spice_origin(); }
        std::string get_spice_frame() const { return this->get()->get_spice_frame(); }
    };
}
