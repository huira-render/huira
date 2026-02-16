#pragma once

#include "huira/core/types.hpp"
#include "huira/core/rotation.hpp"

#include "huira/core/concepts/spectral_concepts.hpp"
#include "huira/handles/frame_handle.hpp"

namespace huira {
    /**
     * @brief Handle for referencing the root frame node in the scene graph.
     *
     * RootFrameHandle is a specialized FrameHandle that represents the root of the scene graph.
     * It disables all transform modification methods to ensure the root frame remains fixed.
     * Copy and assignment are also disabled to enforce unique ownership semantics.
     *
     * @tparam TSpectral Spectral type for the scene
     */
    template <IsSpectral TSpectral>
    class RootFrameHandle : public FrameHandle<TSpectral> {
    public:
        RootFrameHandle() = delete;
        using FrameHandle<TSpectral>::FrameHandle;
        
        RootFrameHandle(const RootFrameHandle&) = delete;
        RootFrameHandle& operator=(const RootFrameHandle&) = delete;

        // Disable transform modification for root
        void set_position(const Vec3<double>&) = delete;
        void set_rotation(const Rotation<double>&) = delete;
        void set_scale(const Vec3<double>&) = delete;
        void set_velocity(const Vec3<double>&) = delete;
        void set_angular_velocity(const Vec3<double>&) = delete;
    };
}
