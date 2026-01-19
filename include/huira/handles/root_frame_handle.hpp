#pragma once

#include "huira/core/types.hpp"
#include "huira/core/rotation.hpp"

#include "huira/detail/concepts/numeric_concepts.hpp"
#include "huira/detail/concepts/spectral_concepts.hpp"
#include "huira/handles/frame_handle.hpp"

namespace huira {
    template <IsSpectral TSpectral, IsFloatingPoint TFloat>
    class RootFrameHandle : public FrameHandle<TSpectral, TFloat> {
    public:
        RootFrameHandle() = delete;
        using FrameHandle<TSpectral, TFloat>::FrameHandle;

        // Disable transform modification for root
        void set_position(const Vec3<TFloat>&) = delete;
        void set_rotation(const Rotation<TFloat>&) = delete;
        void set_scale(const Vec3<TFloat>&) = delete;
        void set_velocity(const Vec3<TFloat>&) = delete;
        void set_angular_velocity(const Vec3<TFloat>&) = delete;
    };
}
