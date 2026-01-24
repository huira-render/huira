#pragma once

#include "huira/core/types.hpp"
#include "huira/detail/concepts/spectral_concepts.hpp"
#include "huira/handles/node_handle.hpp"
#include "huira/handles/unresolved_handle.hpp"
#include "huira/objects/cameras/camera.hpp"

namespace huira {
    template <IsSpectral TSpectral>
    class CameraHandle : public NodeHandle<TSpectral, Camera<TSpectral>> {
    public:
        CameraHandle() = delete;
        using NodeHandle<TSpectral, Camera<TSpectral>>::NodeHandle;

        void set_focal_length(double focal_length) const;

        void look_at(const UnresolvedObjectHandle<TSpectral>& target, Vec3<double> up = Vec3<double>{ 0,1,0 }) const;
        void look_at(const Vec3<double>& target_position, Vec3<double> up = Vec3<double>{ 0,1,0 }) const;

    };
}

#include "huira_impl/handles/camera_handle.ipp"
