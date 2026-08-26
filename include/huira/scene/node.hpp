#pragma once

#include <memory>
#include <span>
#include <stdexcept>
#include <string>

#include "huira/concepts/numeric_concepts.hpp"
#include "huira/concepts/spectral_concepts.hpp"
#include "huira/core/time.hpp"
#include "huira/core/transform.hpp"
#include "huira/scene/scene_object.hpp"
#include "huira/scene/state_callbacks/state_callbacks.hpp"

namespace huira {
// Forward declare:
template <IsSpectral TSpectral>
class Scene;

template <IsSpectral TSpectral>
class SceneView;

template <IsSpectral TSpectral>
class FrameNode;

template <IsSpectral TSpectral, typename TNode>
class NodeHandle;

template <IsSpectral TSpectral>
class LookAtCallbackBase;

template <IsSpectral TSpectral>
class LookAtCallback;

enum class TransformMode {
    MANUAL_TRANSFORM,
    SPICE_TRANSFORM,
    POSITION_CALLBACK,
    ROTATION_CALLBACK,
    TRANSFORM_CALLBACK
};

enum class ObservationMode { TRUE_STATE, GEOMETRIC_STATE, ABERRATED_STATE };

/**
 * @brief Base class for all scene graph nodes.
 *
 * Node represents a transformable entity in the scene graph. It handles:
 * - Local and global transforms (position, rotation, scale)
 * - SPICE-based transforms for celestial mechanics
 * - Parent-child relationships (being a child)
 *
 * Node itself cannot have children - use FrameNode for nodes that need children.
 * Leaf nodes (lights, unresolved objects, etc.) should derive from Node directly.
 *
 * @tparam TSpectral The spectral type (e.g., @ref RGB, @ref Visible8)
 */
template <IsSpectral TSpectral>
class Node : public SceneObject<Node<TSpectral>> {
  public:
    Node(Scene<TSpectral>* scene);
    virtual ~Node() override = default;

    Node(const Node&) = delete;
    Node& operator=(const Node&) = delete;

    void set_position(const Vec3<double>& position);
    void set_position(units::Meter x, units::Meter y, units::Meter z);

    void set_rotation(const Rotation<double>& rotation);

    void set_scale(const Vec3<double>& scale);
    void set_scale(double sx, double sy, double sz);

    void set_velocity(const Vec3<double>& velocity);
    void
    set_velocity(units::MetersPerSecond vx, units::MetersPerSecond vy, units::MetersPerSecond vz);

    void set_angular_velocity(const Vec3<double>& angular_velocity);
    void set_angular_velocity(units::RadiansPerSecond wx,
                              units::RadiansPerSecond wy,
                              units::RadiansPerSecond wz);
    void set_body_angular_velocity(units::RadiansPerSecond wx,
                                   units::RadiansPerSecond wy,
                                   units::RadiansPerSecond wz);

    void set_manual_transform(const Transform<double>& transform);

    void set_spice_origin(const std::string& spice_origin);
    void set_spice_frame(const std::string& spice_frame);
    void set_spice(const std::string& spice_origin, const std::string& spice_frame);

    template <IsPositionCallback TCallback, typename... Args>
    void set_custom_position_callback(Args&&... args);
    void set_keplerian_orbit(units::Meter semi_major_axis,
                             double eccentricity,
                             units::Radian inclination,
                             units::Radian raan,
                             units::Radian arg_periapsis,
                             units::Radian mean_anomaly,
                             Time epoch,
                             double mu);

    template <IsRotationCallback TCallback, typename... Args>
    void set_custom_rotation_callback(Args&&... args);
    void set_z_up_y_forward_callback();
    void set_z_down_y_forward_callback();

    template <IsTransformCallback TCallback, typename... Args>
    void set_custom_state_callback(Args&&... args);

    virtual std::string type() const override { return "Node"; }

    TransformMode get_position_mode() const { return position_mode_; }
    TransformMode get_rotation_mode() const { return rotation_mode_; }

    Transform<double> get_apparent_transform(ObservationMode obs_mode,
                                             const Time& epoch,
                                             const Time& t_obs,
                                             const Transform<double>& observer_ssb_state) const;

    Vec3<double> get_static_position() const;
    Rotation<double> get_static_rotation() const;
    Vec3<double> get_static_scale() const;
    Vec3<double> get_static_velocity() const;
    Vec3<double> get_static_angular_velocity() const;

    std::string get_spice_origin() const;
    std::string get_spice_frame() const;

    NodeHandle<TSpectral, Node<TSpectral>> get_parent() const;

    template <typename TParentNode>
    NodeHandle<TSpectral, TParentNode> get_parent_as() const;

    virtual std::span<const std::shared_ptr<Node<TSpectral>>> get_children() const { return {}; }

  protected:
    Transform<double> local_transform_;

    /// When true, local_transform_.angular_velocity was supplied via
    /// set_body_angular_velocity() and is expressed in the node's own body axes;
    /// orientation propagates as q(t) = q_0 * delta. When false (the default),
    /// the rates are expressed in the parent frame's axes and orientation
    /// propagates as q(t) = delta * q_0. Note that get_local_rotation_at_()
    /// always re-expresses the angular velocity in parent axes before storing it
    /// in the returned Transform (the frame Transform::operator* expects).
    bool body_frame_rates_ = false;

    TransformMode position_mode_ = TransformMode::MANUAL_TRANSFORM;
    TransformMode rotation_mode_ = TransformMode::MANUAL_TRANSFORM;

    std::string spice_origin_ = "";
    std::string spice_frame_ = "";

    bool position_can_be_spice_() const;
    virtual bool position_must_be_spice_() const { return false; }

    bool rotation_can_be_spice_() const;
    virtual bool rotation_must_be_spice_() const { return false; }

    std::unique_ptr<PositionCallback> position_callback_;
    std::unique_ptr<RotationCallback> rotation_callback_;
    std::unique_ptr<StateCallback> transform_callback_;

    Scene<TSpectral>* scene_;
    Node<TSpectral>* parent_ = nullptr;

    void set_parent_(Node<TSpectral>* parent) { parent_ = parent; }

    std::pair<const Node<TSpectral>*, Transform<double>> find_spice_origin_ancestor_() const;
    std::pair<const Node<TSpectral>*, std::pair<Rotation<double>, Vec3<double>>>
    find_spice_frame_ancestor_() const;

    std::pair<Transform<double>, double>
    get_geometric_state_(const Time& epoch,
                         const Time& t_obs,
                         const Transform<double>& observer_ssb_state,
                         bool iterate,
                         double tol = 1e-12) const;

    Transform<double>
    get_ssb_transform_(const Time& epoch, const Time& t_obs, double dt = 0.0) const;
    Transform<double> get_local_position_at_(const Time& epoch, const Time& t_obs, double dt) const;
    Transform<double> get_local_rotation_at_(const Time& epoch, const Time& t_obs, double dt) const;

    friend class Scene<TSpectral>;
    friend class FrameNode<TSpectral>;
    friend class SceneView<TSpectral>;

    friend class LookAtCallbackBase<TSpectral>;
    friend class LookAtCallback<TSpectral>;
};
} // namespace huira

#include "huira_impl/scene/node.ipp"
