#pragma once

#include "huira/handles/node_handle.hpp"
#include "huira/camera/camera.hpp"
#include "huira/handles/unresolved_handle.hpp"

namespace huira {
    template <IsSpectral TSpectral, IsFloatingPoint TFloat>
    class CameraHandle : public NodeHandle<TSpectral, TFloat, Camera<TSpectral, TFloat>> {
    public:
        CameraHandle() = delete;
        using NodeHandle<TSpectral, TFloat, Camera<TSpectral, TFloat>>::NodeHandle;

        void look_at(const UnresolvedObjectHandle<TSpectral, TFloat>& target,
            Vec3<TFloat> up = Vec3<TFloat>{ 0,1,0 }) const {
            this->look_at(target.get_global_position(), up);
        }
        void look_at(const Vec3<TFloat>& target_position,
            Vec3<TFloat> up = Vec3<TFloat>{ 0,1,0 }) const {
            this->get()->look_at(target_position, up);
        }

        void set_focal_length(TFloat focal_length) const {
            this->get()->set_focal_length(focal_length);
        }
    };
}
