#pragma once

#include "huira/core/types.hpp"
#include "huira/core/rotation.hpp"

#include "huira/detail/concepts/spectral_concepts.hpp"
#include "huira/handles/frame_handle.hpp"

namespace huira {
    template <IsSpectral TSpectral>
    class RootFrameHandle : public FrameHandle<TSpectral> {
    public:
        RootFrameHandle() = delete;
        using FrameHandle<TSpectral>::FrameHandle;

        // Disable transform modification for root
        void set_position(const Vec3<double>&) = delete;
        void set_rotation(const Rotation<double>&) = delete;
        void set_scale(const Vec3<double>&) = delete;
        void set_velocity(const Vec3<double>&) = delete;
        void set_angular_velocity(const Vec3<double>&) = delete;
    };
}
