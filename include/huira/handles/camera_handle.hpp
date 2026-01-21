#pragma once

#include "huira/camera/camera.hpp"
#include "huira/handles/node_handle.hpp"
#include "huira/handles/unresolved_handle.hpp"

#include "huira/detail/concepts/numeric_concepts.hpp"
#include "huira/detail/concepts/spectral_concepts.hpp"
#include "huira/core/types.hpp"

namespace huira {
    template <IsSpectral TSpectral, IsFloatingPoint TFloat>
    class CameraHandle : public NodeHandle<TSpectral, TFloat, Camera<TSpectral, TFloat>> {
    public:
        CameraHandle() = delete;
        using NodeHandle<TSpectral, TFloat, Camera<TSpectral, TFloat>>::NodeHandle;

        void set_focal_length(TFloat focal_length) const;

        void look_at(const UnresolvedObjectHandle<TSpectral, TFloat>& target, Vec3<TFloat> up = Vec3<TFloat>{ 0,1,0 }) const;
        void look_at(const Vec3<TFloat>& target_position, Vec3<TFloat> up = Vec3<TFloat>{ 0,1,0 }) const;

    };
}

#include "huira_impl/handles/camera_handle.ipp"
