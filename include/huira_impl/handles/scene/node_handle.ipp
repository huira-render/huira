#include "huira/concepts/numeric_concepts.hpp"
#include "huira/concepts/spectral_concepts.hpp"
#include "huira/core/types.hpp"

namespace huira {
/**
 * @brief Sets the position using individual coordinates.
 *
 * @param x The x-coordinate
 * @param y The y-coordinate
 * @param z The z-coordinate
 */
template <IsSpectral TSpectral, typename TNode>
void NodeHandle<TSpectral, TNode>::set_position(units::Meter x,
                                                units::Meter y,
                                                units::Meter z) const
{
    this->get_()->set_position(x, y, z);
}

/**
 * @brief Gets the local position.
 *
 * @return Vec3<double> The local 3D position vector
 */
template <IsSpectral TSpectral, typename TNode>
Vec3<double> NodeHandle<TSpectral, TNode>::get_static_position() const
{
    return this->get_()->get_static_position();
}

/**
 * @brief Sets the velocity using individual components.
 *
 * @param vx The x-component of velocity
 * @param vy The y-component of velocity
 * @param vz The z-component of velocity
 */
template <IsSpectral TSpectral, typename TNode>
void NodeHandle<TSpectral, TNode>::set_velocity(units::MetersPerSecond vx,
                                                units::MetersPerSecond vy,
                                                units::MetersPerSecond vz) const
{
    this->get_()->set_velocity(vx, vy, vz);
}

/**
 * @brief Gets the local velocity.
 *
 * @return Vec3<double> The local 3D velocity vector
 */
template <IsSpectral TSpectral, typename TNode>
Vec3<double> NodeHandle<TSpectral, TNode>::get_static_velocity() const
{
    return this->get_()->get_static_velocity();
}

/**
 * @brief Sets the rotation.
 *
 * @param rotation The rotation to set
 */
template <IsSpectral TSpectral, typename TNode>
void NodeHandle<TSpectral, TNode>::set_rotation(const Rotation<double>& rotation) const
{
    this->get_()->set_rotation(rotation);
}

/**
 * @brief Sets the local-to-parent rotation using a rotation matrix.
 * @param matrix 3x3 rotation matrix
 */
template <IsSpectral TSpectral, typename TNode>
void NodeHandle<TSpectral, TNode>::set_rotation_local_to_parent(const Mat3<double>& matrix) const
{
    this->get_()->set_rotation(Rotation<double>::from_local_to_parent(matrix));
}

/**
 * @brief Sets the local-to-parent rotation using a quaternion.
 * @param quaternion Quaternion representing the rotation
 */
template <IsSpectral TSpectral, typename TNode>
void NodeHandle<TSpectral, TNode>::set_rotation_local_to_parent(
    const Quaternion<double>& quaternion) const
{
    this->get_()->set_rotation(Rotation<double>::from_local_to_parent(quaternion));
}

/**
 * @brief Sets the local-to-parent rotation using an axis and angle.
 * @param axis Axis of rotation
 * @param angle Angle in degrees
 */
template <IsSpectral TSpectral, typename TNode>
void NodeHandle<TSpectral, TNode>::set_rotation_local_to_parent(const Vec3<double>& axis,
                                                                units::Degree angle) const
{
    this->get_()->set_rotation(Rotation<double>::from_local_to_parent(axis, angle));
}

/**
 * @brief Sets the parent-to-local rotation using a rotation matrix.
 * @param matrix 3x3 rotation matrix
 */
template <IsSpectral TSpectral, typename TNode>
void NodeHandle<TSpectral, TNode>::set_rotation_parent_to_local(const Mat3<double>& matrix) const
{
    this->get_()->set_rotation(Rotation<double>::from_parent_to_local(matrix));
}

/**
 * @brief Sets the parent-to-local rotation using a quaternion.
 * @param quaternion Quaternion representing the rotation
 */
template <IsSpectral TSpectral, typename TNode>
void NodeHandle<TSpectral, TNode>::set_rotation_parent_to_local(
    const Quaternion<double>& quaternion) const
{
    this->get_()->set_rotation(Rotation<double>::from_parent_to_local(quaternion));
}

/**
 * @brief Sets the parent-to-local rotation using an axis and angle.
 * @param axis Axis of rotation
 * @param angle Angle in degrees
 */
template <IsSpectral TSpectral, typename TNode>
void NodeHandle<TSpectral, TNode>::set_rotation_parent_to_local(const Vec3<double>& axis,
                                                                units::Degree angle) const
{
    this->get_()->set_rotation(Rotation<double>::from_parent_to_local(axis, angle));
}

/**
 * @brief Sets the rotation using extrinsic Euler angles and a rotation sequence.
 * @param x First Euler angle (radians)
 * @param y Second Euler angle (radians)
 * @param z Third Euler angle (radians)
 * @param sequence Rotation order (e.g., "XYZ")
 */
template <IsSpectral TSpectral, typename TNode>
void NodeHandle<TSpectral, TNode>::set_euler_angles(units::Radian x,
                                                    units::Radian y,
                                                    units::Radian z,
                                                    std::string sequence) const
{
    this->get_()->set_rotation(Rotation<double>::extrinsic_euler_angles(x, y, z, sequence));
}

/**
 * @brief Gets the local rotation.
 *
 * @return Rotation<double> The local rotation
 */
template <IsSpectral TSpectral, typename TNode>
Rotation<double> NodeHandle<TSpectral, TNode>::get_static_rotation() const
{
    return this->get_()->get_static_rotation();
}

/**
 * @brief Sets the angular velocity using individual components, expressed in the
 * PARENT frame.
 *
 * The components are interpreted in the axes of this node's parent frame; the
 * orientation evolves as q(t) = delta(t) * q_0. This is only an "inertial" rate
 * if every ancestor of the node is static. To command rates about the node's
 * own axes (e.g. a roll about a camera boresight), use
 * set_body_angular_velocity() instead.
 *
 * @param wx The x-component of angular velocity (parent-frame axes)
 * @param wy The y-component of angular velocity (parent-frame axes)
 * @param wz The z-component of angular velocity (parent-frame axes)
 */
template <IsSpectral TSpectral, typename TNode>
void NodeHandle<TSpectral, TNode>::set_angular_velocity(units::RadiansPerSecond wx,
                                                        units::RadiansPerSecond wy,
                                                        units::RadiansPerSecond wz) const
{
    this->get_()->set_angular_velocity(wx, wy, wz);
}

/**
 * @brief Sets the angular velocity using individual components, expressed in the
 * node's own BODY frame.
 *
 * The components are interpreted in this node's own axes; the orientation
 * evolves as q(t) = q_0 * delta(t). For a camera (OpenCV convention,
 * +z = boresight), a pure wz produces a roll about the boresight regardless of
 * how the camera is oriented relative to its parent. To command rates in the
 * parent frame's axes, use set_angular_velocity() instead.
 *
 * @param wx The x-body frame component of angular velocity
 * @param wy The y-body frame component of angular velocity
 * @param wz The z-body frame component of angular velocity
 */
template <IsSpectral TSpectral, typename TNode>
void NodeHandle<TSpectral, TNode>::set_body_angular_velocity(units::RadiansPerSecond wx,
                                                             units::RadiansPerSecond wy,
                                                             units::RadiansPerSecond wz) const
{
    this->get_()->set_body_angular_velocity(wx, wy, wz);
}

/**
 * @brief Gets the local angular velocity.
 *
 * @return Vec3<double> The local 3D angular velocity vector
 */
template <IsSpectral TSpectral, typename TNode>
Vec3<double> NodeHandle<TSpectral, TNode>::get_static_angular_velocity() const
{
    return this->get_()->get_static_angular_velocity();
}

/**
 * @brief Sets the scale using individual components.
 *
 * The scale sizes what the node holds, and places and sizes its children. A sphere light can
 * only be scaled evenly, which grows it at the same radiance: a scene view refuses one scaled
 * unevenly, by its own scale or its frames', which would make it an ellipsoid.
 *
 * @param sx The x-component of the scale
 * @param sy The y-component of the scale
 * @param sz The z-component of the scale
 */
template <IsSpectral TSpectral, typename TNode>
void NodeHandle<TSpectral, TNode>::set_scale(double sx, double sy, double sz) const
{
    this->get_()->set_scale(sx, sy, sz);
}

/**
 * @brief Sets the scale using a single scale factor.
 *
 * The scale sizes what the node holds, and places and sizes its children. A sphere light can
 * only be scaled evenly, which grows it at the same radiance: a scene view refuses one scaled
 * unevenly, by its own scale or its frames', which would make it an ellipsoid.
 *
 * @param s The 3D scale factor to set
 */
template <IsSpectral TSpectral, typename TNode>
void NodeHandle<TSpectral, TNode>::set_scale(double s) const
{
    this->get_()->set_scale(s, s, s);
}

/**
 * @brief Gets the local scale.
 *
 * @return Vec3<double> The local 3D scale
 */
template <IsSpectral TSpectral, typename TNode>
Vec3<double> NodeHandle<TSpectral, TNode>::get_static_scale() const
{
    return this->get_()->get_static_scale();
}

/**
 * @brief Sets the SPICE origin identifier.
 *
 * @param spice_origin The SPICE origin string identifier
 */
template <IsSpectral TSpectral, typename TNode>
void NodeHandle<TSpectral, TNode>::set_spice_origin(const std::string& spice_origin) const
{
    this->get_()->set_spice_origin(spice_origin);
}

/**
 * @brief Sets the SPICE frame identifier.
 *
 * @param spice_frame The SPICE frame string identifier
 */
template <IsSpectral TSpectral, typename TNode>
void NodeHandle<TSpectral, TNode>::set_spice_frame(const std::string& spice_frame) const
{
    this->get_()->set_spice_frame(spice_frame);
}

/**
 * @brief Sets the SPICE origin and frame identifiers.
 *
 * @param spice_origin The SPICE origin string identifier
 * @param spice_frame The SPICE frame string identifier
 */
template <IsSpectral TSpectral, typename TNode>
void NodeHandle<TSpectral, TNode>::set_spice(const std::string& spice_origin,
                                             const std::string& spice_frame) const
{
    this->get_()->set_spice(spice_origin, spice_frame);
}

/**
 * @brief Gets the SPICE origin identifier.
 *
 * @return std::string The SPICE origin string identifier
 */
template <IsSpectral TSpectral, typename TNode>
std::string NodeHandle<TSpectral, TNode>::get_spice_origin() const
{
    return this->get_()->get_spice_origin();
}

/**
 * @brief Gets the SPICE frame identifier.
 *
 * @return std::string The SPICE frame string identifier
 */
template <IsSpectral TSpectral, typename TNode>
std::string NodeHandle<TSpectral, TNode>::get_spice_frame() const
{
    return this->get_()->get_spice_frame();
}

/**
 * @brief Places the node by a function of the pose context: see Node::set_position_function.
 *
 * @param function Called with a PoseContext; returns the position in the parent's frame, in
 *        meters (a Vec3<double>, whose velocity is then estimated), or a PositionState.
 */
template <IsSpectral TSpectral, typename TNode>
template <typename TFunction>
    requires std::invocable<const TFunction&, const PoseContext&>
void NodeHandle<TSpectral, TNode>::set_position_function(TFunction function) const
{
    this->get_()->set_position_function(std::move(function));
}

/**
 * @brief Places the node by a table of times and positions: see Node::set_position_table.
 *
 * @param times When each entry is: at least two, each later than the one before.
 * @param positions The position at each time in the parent's frame, in meters.
 * @param velocities The velocity at each time, in meters per second, or none to estimate them.
 */
template <IsSpectral TSpectral, typename TNode>
void NodeHandle<TSpectral, TNode>::set_position_table(const std::vector<Time>& times,
                                                      std::vector<Vec3<double>> positions,
                                                      std::vector<Vec3<double>> velocities) const
{
    this->get_()->set_position_table(times, std::move(positions), std::move(velocities));
}

/**
 * @brief Moves the node on a Keplerian orbit about its parent: see Node::set_keplerian_orbit.
 */
template <IsSpectral TSpectral, typename TNode>
void NodeHandle<TSpectral, TNode>::set_keplerian_orbit(
    units::Meter semi_major_axis,
    double eccentricity,
    units::Radian inclination,
    units::Radian raan,
    units::Radian arg_periapsis,
    units::Radian mean_anomaly,
    Time epoch,
    units::CubicKilometersPerSecondSquared mu) const
{
    this->get_()->set_keplerian_orbit(
        semi_major_axis, eccentricity, inclination, raan, arg_periapsis, mean_anomaly, epoch, mu);
}

/**
 * @brief Places the node by SGP4, from a satellite's TLE: see Node::set_tle_position.
 */
template <IsSpectral TSpectral, typename TNode>
void NodeHandle<TSpectral, TNode>::set_tle_position(const SatRec& satrec) const
{
    this->get_()->set_tle_position(satrec);
}

/**
 * @brief Places the node by a position callback of a class of one's own, made from the arguments
 * given: see Node::set_custom_position_callback.
 *
 * @tparam TCallback A class derived from PositionCallback.
 */
template <IsSpectral TSpectral, typename TNode>
template <IsPositionCallback TCallback, typename... Args>
void NodeHandle<TSpectral, TNode>::set_custom_position_callback(Args&&... args) const
{
    this->get_()->template set_custom_position_callback<TCallback>(std::forward<Args>(args)...);
}

/**
 * @brief Orients the node by a function of the pose context: see Node::set_rotation_function.
 *
 * @param function Called with a PoseContext; returns the rotation relative to the parent (a
 *        Rotation<double>, whose angular velocity is then estimated), or a RotationState.
 */
template <IsSpectral TSpectral, typename TNode>
template <typename TFunction>
    requires std::invocable<const TFunction&, const PoseContext&>
void NodeHandle<TSpectral, TNode>::set_rotation_function(TFunction function) const
{
    this->get_()->set_rotation_function(std::move(function));
}

/**
 * @brief Orients the node by a table of times and rotations: see Node::set_rotation_table.
 *
 * @param times When each entry is: at least two, each later than the one before.
 * @param rotations The rotation at each time, from the node's frame to its parent's.
 * @param angular_velocities The angular velocity at each time, in radians per second about the
 *        parent's axes, or none to take the steady rate between entries.
 */
template <IsSpectral TSpectral, typename TNode>
void NodeHandle<TSpectral, TNode>::set_rotation_table(
    const std::vector<Time>& times,
    std::vector<Rotation<double>> rotations,
    std::vector<Vec3<double>> angular_velocities) const
{
    this->get_()->set_rotation_table(times, std::move(rotations), std::move(angular_velocities));
}

/**
 * @brief Orients the node by a rotation callback of a class of one's own, made from the arguments
 * given: see Node::set_custom_rotation_callback.
 *
 * @tparam TCallback A class derived from RotationCallback.
 */
template <IsSpectral TSpectral, typename TNode>
template <IsRotationCallback TCallback, typename... Args>
void NodeHandle<TSpectral, TNode>::set_custom_rotation_callback(Args&&... args) const
{
    this->get_()->template set_custom_rotation_callback<TCallback>(std::forward<Args>(args)...);
}

/**
 * @brief Gets a handle to the parent node.
 *
 * Returns a base NodeHandle to the parent. This is safe for all node types
 * but may require casting if you need access to parent-specific functionality.
 * For type-specific access, use get_parent_as() instead.
 *
 * @return NodeHandle<TSpectral, Node<TSpectral>> Handle to the parent node
 * @throws std::runtime_error If this node has no parent
 */
template <IsSpectral TSpectral, typename TNode>
NodeHandle<TSpectral, Node<TSpectral>> NodeHandle<TSpectral, TNode>::get_parent() const
{
    return this->get_()->get_parent();
}

/**
 * @brief Gets a handle to the parent node with a specific type.
 *
 * Returns a handle to the parent cast to the specified node type. This is useful
 * when you know the parent's type and need access to type-specific functionality
 * (e.g., getting a FrameHandle to access frame-specific methods).
 *
 * Example usage:
 * @code
 * auto frame_parent = camera_handle.get_parent_as<FrameNode<RGB>>();
 * auto grandparent = frame_parent.get_parent_as<FrameNode<RGB>>();
 * @endcode
 *
 * @tparam TParentNode The expected type of the parent node (e.g., FrameNode<TSpectral>)
 * @return NodeHandle<TSpectral, TParentNode> Handle to the parent with the specified type
 * @throws std::runtime_error If this node has no parent
 * @throws std::runtime_error If the parent is not of type TParentNode
 */
template <IsSpectral TSpectral, typename TNode>
template <typename TParentNode>
NodeHandle<TSpectral, TParentNode> NodeHandle<TSpectral, TNode>::get_parent_as() const
{
    return this->get_()->template get_parent_as<TParentNode>();
}

/**
 * @brief Sets whether this node is collected into a SceneView.
 *
 * Takes effect for SceneViews constructed after the call; ones that already exist are
 * unaffected. Descendants are skipped along with this node but keep their own flags.
 *
 * @param visible True to collect this node, false to skip it and its descendants
 */
template <IsSpectral TSpectral, typename TNode>
void NodeHandle<TSpectral, TNode>::set_visible(bool visible) const
{
    this->get_()->set_visible(visible);
}

/**
 * @brief Reads this node's own visibility flag, ignoring any hidden ancestor.
 * @return bool The flag as last set on this node
 */
template <IsSpectral TSpectral, typename TNode>
bool NodeHandle<TSpectral, TNode>::is_visible() const
{
    return this->get_()->is_visible();
}

/**
 * @brief Reports whether this node would be collected, ancestors included.
 * @return bool True only if this node and every ancestor are visible
 */
template <IsSpectral TSpectral, typename TNode>
bool NodeHandle<TSpectral, TNode>::is_effectively_visible() const
{
    return this->get_()->is_effectively_visible();
}
} // namespace huira
