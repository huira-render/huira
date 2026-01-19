#pragma once

#include "huira/detail/concepts/numeric_concepts.hpp"
#include "huira/detail/concepts/spectral_concepts.hpp"

#include "huira/handles/handle.hpp"
#include "huira/lights/point_light.hpp"

namespace huira {
    template <IsSpectral TSpectral, IsFloatingPoint TFloat>
    class PointLightHandle : public Handle<PointLight<TSpectral, TFloat>> {
    public:
        PointLightHandle() = delete;
        using Handle<PointLight<TSpectral, TFloat>>::Handle;

        void set_position(const Vec3<TFloat>& position) const { this->get()->set_position(position); }
        void set_position(double x, double y, double z) const { this->get()->set_position(Vec3<TFloat>{x, y, z}); }

        void set_velocity(const Vec3<TFloat>& velocity) const { this->get()->set_velocity(velocity); }
        void set_velocity(double vx, double vy, double vz) const { this->get()->set_velocity(Vec3<TFloat>{vx, vy, vz}); }

        void set_spice_origin(const std::string& spice_origin) const { this->get()->set_spice_origin(spice_origin); }

        void set_intensity(const TSpectral& intensity) const { this->get()->set_intensity(intensity); }
    };
}
