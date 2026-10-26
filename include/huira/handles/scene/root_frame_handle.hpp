#pragma once

#include "huira/concepts/spectral_concepts.hpp"
#include "huira/core/rotation.hpp"
#include "huira/core/types.hpp"
#include "huira/handles/scene/frame_handle.hpp"

namespace huira {
/**
 * @brief Handle for referencing the root frame node in the scene graph.
 *
 * RootFrameHandle is a specialized FrameHandle that represents the root of the scene graph.
 * It disables all transform modification methods to ensure the root frame remains fixed. The root
 * node refuses them too (see RootFrameNode), so another handle to it, such as a child's
 * get_parent(), can't change it either. Copy and assignment are also disabled to enforce unique
 * ownership semantics.
 *
 * @tparam TSpectral The spectral type (e.g., @ref RGB, @ref Visible8)
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
    void set_body_angular_velocity(const Vec3<double>&) = delete;
    template <typename... Args>
    void set_position_function(Args&&...) = delete;
    template <typename... Args>
    void set_position_table(Args&&...) = delete;
    template <typename... Args>
    void set_keplerian_orbit(Args&&...) = delete;
    template <typename... Args>
    void set_tle_position(Args&&...) = delete;
    template <typename... Args>
    void set_custom_position_callback(Args&&...) = delete;
    template <typename... Args>
    void set_rotation_function(Args&&...) = delete;
    template <typename... Args>
    void set_rotation_table(Args&&...) = delete;
    template <typename... Args>
    void set_custom_rotation_callback(Args&&...) = delete;
};
} // namespace huira
