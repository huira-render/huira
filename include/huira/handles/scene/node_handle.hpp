#pragma once

#include <concepts>
#include <string>
#include <utility>
#include <vector>

#include "huira/concepts/numeric_concepts.hpp"
#include "huira/concepts/spectral_concepts.hpp"
#include "huira/handles/handle.hpp"
#include "huira/scene/node.hpp"

namespace huira {
/**
 * @brief Handle for referencing and manipulating nodes in the scene graph.
 *
 * NodeHandle provides safe, type-checked access to scene graph nodes, allowing
 * manipulation of position, velocity, rotation, scale, SPICE parameters, and parent access.
 * It is used for both base Node and derived node types (e.g., FrameNode, Instance).
 *
 * @tparam TSpectral The spectral type (e.g., @ref RGB, @ref Visible8)
 * @tparam TNode Node type (must derive from Node<TSpectral>)
 */
template <IsSpectral TSpectral, typename TNode>
class NodeHandle : public Handle<TNode> {
  public:
    NodeHandle() = delete;
    using Handle<TNode>::Handle;

    // Position
    void set_position(units::Meter x, units::Meter y, units::Meter z) const;
    Vec3<double> get_static_position() const;

    // Velocity
    void set_velocity(units::MetersPerSecond vx,
                      units::MetersPerSecond vy,
                      units::MetersPerSecond vz) const;
    Vec3<double> get_static_velocity() const;

    // Rotation
    void set_rotation(const Rotation<double>& rotation) const;

    void set_rotation_local_to_parent(const Mat3<double>& matrix) const;
    void set_rotation_local_to_parent(const Quaternion<double>& quaternion) const;
    void set_rotation_local_to_parent(const Vec3<double>& axis, units::Degree angle) const;

    void set_rotation_parent_to_local(const Mat3<double>& matrix) const;
    void set_rotation_parent_to_local(const Quaternion<double>& quaternion) const;
    void set_rotation_parent_to_local(const Vec3<double>& axis, units::Degree angle) const;

    void set_euler_angles(units::Radian x,
                          units::Radian y,
                          units::Radian z,
                          std::string sequence = "XYZ") const;
    Rotation<double> get_static_rotation() const;

    // Angular velocity
    void set_angular_velocity(units::RadiansPerSecond wx,
                              units::RadiansPerSecond wy,
                              units::RadiansPerSecond wz) const;
    void set_body_angular_velocity(units::RadiansPerSecond wx,
                                   units::RadiansPerSecond wy,
                                   units::RadiansPerSecond wz) const;
    Vec3<double> get_static_angular_velocity() const;

    // Scale
    void set_scale(double sx, double sy, double sz) const;
    void set_scale(double s) const;
    Vec3<double> get_static_scale() const;

    // SPICE
    void set_spice_origin(const std::string& spice_origin) const;
    void set_spice_frame(const std::string& spice_frame) const;
    void set_spice(const std::string& spice_origin, const std::string& spice_frame) const;
    std::string get_spice_origin() const;
    std::string get_spice_frame() const;

    // Other position sources (see Node): each replaces the node's position source.
    template <typename TFunction>
        requires std::invocable<const TFunction&, const PoseContext&>
    void set_position_function(TFunction function) const;
    void set_position_table(const std::vector<Time>& times,
                            std::vector<Vec3<double>> positions,
                            std::vector<Vec3<double>> velocities = {}) const;
    void set_keplerian_orbit(units::Meter semi_major_axis,
                             double eccentricity,
                             units::Radian inclination,
                             units::Radian raan,
                             units::Radian arg_periapsis,
                             units::Radian mean_anomaly,
                             Time epoch,
                             units::CubicKilometersPerSecondSquared mu) const;
    template <IsPositionCallback TCallback, typename... Args>
    void set_custom_position_callback(Args&&... args) const;

    // Other rotation sources (see Node): each replaces the node's rotation source.
    template <typename TFunction>
        requires std::invocable<const TFunction&, const PoseContext&>
    void set_rotation_function(TFunction function) const;
    void set_rotation_table(const std::vector<Time>& times,
                            std::vector<Rotation<double>> rotations,
                            std::vector<Vec3<double>> angular_velocities = {}) const;
    template <IsRotationCallback TCallback, typename... Args>
    void set_custom_rotation_callback(Args&&... args) const;

    // Parent access
    NodeHandle<TSpectral, Node<TSpectral>> get_parent() const;

    template <typename TParentNode>
    NodeHandle<TSpectral, TParentNode> get_parent_as() const;

    void set_visible(bool visible = true) const;
    [[nodiscard]] bool is_visible() const;
    [[nodiscard]] bool is_effectively_visible() const;
};
} // namespace huira

#include "huira_impl/handles/scene/node_handle.ipp"
