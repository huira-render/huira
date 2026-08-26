#pragma once

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

    void look_at(const InstanceHandle<TSpectral>& target, const Vec3<double>& up = {0.0, 0.0, 1.0})
    {
        this->get()->look_at(*target.get(), up);
    }

    void look_at(const Vec3<double>& target_position, const Vec3<double>& up = {0.0, 0.0, 1.0})
    {
        this->get()->look_at(target_position, up);
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
