#pragma once

#include <optional>

#include "huira/concepts/spectral_concepts.hpp"
#include "huira/handles/handle.hpp"
#include "huira/scene/instance.hpp"

namespace huira {
template <IsSpectral TSpectral>
class FrameHandle;

template <IsSpectral TSpectral>
class SceneView;

/**
 * @brief Handle for referencing an Instance node in the scene graph.
 *
 * InstanceHandle provides safe, type-checked access to Instance nodes, allowing
 * manipulation and querying of asset instances within the scene. Used by FrameHandle
 * and SceneView for instance management.
 *
 * @tparam TSpectral The spectral type (e.g., @ref RGB, @ref Visible8)
 */
template <IsSpectral TSpectral>
class InstanceHandle : public NodeHandle<TSpectral, Instance<TSpectral>> {
  public:
    InstanceHandle() = delete;
    using NodeHandle<TSpectral, Instance<TSpectral>>::NodeHandle;

    /// Look at another instance, following it through the exposure (see Instance::look_at).
    /// It is aimed at in the given observation mode, or by default the scene view's.
    void look_at(const InstanceHandle<TSpectral>& target,
                 const Vec3<double>& up = {0.0, 0.0, 1.0},
                 std::optional<ObservationMode> observation_mode = std::nullopt)
    {
        this->get()->look_at(*target.get(), up, observation_mode);
    }

    /// Look at a fixed point, in the parent's frame or the root's, as seen in an observation
    /// mode (see Instance::look_at).
    void look_at(const Vec3<double>& target_position,
                 const Vec3<double>& up = {0.0, 0.0, 1.0},
                 TargetFrame frame = TargetFrame::PARENT,
                 std::optional<ObservationMode> observation_mode = ObservationMode::TRUE_STATE)
    {
        this->get()->look_at(target_position, up, frame, observation_mode);
    }

    /// Designates this instance as an indirect illumination source (see
    /// Instance::set_indirect_source).
    void set_indirect_source(bool enabled = true) { this->get()->set_indirect_source(enabled); }

    /// Whether this instance is designated as an indirect illumination source.
    bool is_indirect_source() const { return this->get()->is_indirect_source(); }

    friend class FrameHandle<TSpectral>;
    friend class SceneView<TSpectral>;
};
} // namespace huira
