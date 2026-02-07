#include "huira/core/concepts/numeric_concepts.hpp"
#include "huira/core/concepts/spectral_concepts.hpp"
#include "huira/core/types.hpp"

namespace huira {

    /**
     * @brief Sets the position using a Vec3.
     *
     * @param position The 3D position vector to set
     */
    template <IsSpectral TSpectral, typename TNode>
    void NodeHandle<TSpectral, TNode>::set_position(const Vec3<double>& position) const {
        this->get()->set_position(position);
    }

    /**
     * @brief Sets the position using individual coordinates.
     *
     * @param x The x-coordinate
     * @param y The y-coordinate
     * @param z The z-coordinate
     */
    template <IsSpectral TSpectral, typename TNode>
    void NodeHandle<TSpectral, TNode>::set_position(double x, double y, double z) const {
        this->get()->set_position(Vec3<double>{x, y, z});
    }
    

    /**
     * @brief Gets the local position.
     *
     * @return Vec3<double> The local 3D position vector
     */
    template <IsSpectral TSpectral, typename TNode>
    Vec3<double> NodeHandle<TSpectral, TNode>::get_static_position() const {
        return this->get()->get_static_position();
    }



    /**
     * @brief Sets the velocity using a Vec3.
     *
     * @param velocity The 3D velocity vector to set
     */
    template <IsSpectral TSpectral, typename TNode>
    void NodeHandle<TSpectral, TNode>::set_velocity(const Vec3<double>& velocity) const {
        this->get()->set_velocity(velocity);
    }

    /**
     * @brief Sets the velocity using individual components.
     *
     * @param vx The x-component of velocity
     * @param vy The y-component of velocity
     * @param vz The z-component of velocity
     */
    template <IsSpectral TSpectral, typename TNode>
    void NodeHandle<TSpectral, TNode>::set_velocity(double vx, double vy, double vz) const {
        this->get()->set_velocity(Vec3<double>{vx, vy, vz});
    }
    

    /**
     * @brief Gets the local velocity.
     *
     * @return Vec3<double> The local 3D velocity vector
     */
    template <IsSpectral TSpectral, typename TNode>
    Vec3<double> NodeHandle<TSpectral, TNode>::get_static_velocity() const {
        return this->get()->get_static_velocity();
    }


    /**
     * @brief Sets the rotation.
     *
     * @param rotation The rotation to set
     */
    template <IsSpectral TSpectral, typename TNode>
    void NodeHandle<TSpectral, TNode>::set_rotation(const Rotation<double>& rotation) const {
        this->get()->set_rotation(rotation);
    }

    template <IsSpectral TSpectral, typename TNode>
    void NodeHandle<TSpectral, TNode>::set_rotation_local_to_parent(const Mat3<double>& matrix) const
    {
        this->get()->set_rotation(Rotation<double>::from_local_to_parent(matrix));
    }

    template <IsSpectral TSpectral, typename TNode>
    void NodeHandle<TSpectral, TNode>::set_rotation_local_to_parent(const Quaternion<double>& quaternion) const
    {
        this->get()->set_rotation(Rotation<double>::from_local_to_parent(quaternion));
    }

    template <IsSpectral TSpectral, typename TNode>
    void NodeHandle<TSpectral, TNode>::set_rotation_local_to_parent(const Vec3<double>& axis, units::Degree angle) const
    {
        this->get()->set_rotation(Rotation<double>::from_local_to_parent(axis, angle));
    }


    template <IsSpectral TSpectral, typename TNode>
    void NodeHandle<TSpectral, TNode>::set_rotation_parent_to_local(const Mat3<double>& matrix) const
    {
        this->get()->set_rotation(Rotation<double>::from_parent_to_local(matrix));
    }

    template <IsSpectral TSpectral, typename TNode>
    void NodeHandle<TSpectral, TNode>::set_rotation_parent_to_local(const Quaternion<double>& quaternion) const
    {
        this->get()->set_rotation(Rotation<double>::from_parent_to_local(quaternion));
    }

    template <IsSpectral TSpectral, typename TNode>
    void NodeHandle<TSpectral, TNode>::set_rotation_parent_to_local(const Vec3<double>& axis, units::Degree angle) const
    {
        this->get()->set_rotation(Rotation<double>::from_parent_to_local(axis, angle));
    }



    template <IsSpectral TSpectral, typename TNode>
    void NodeHandle<TSpectral, TNode>::set_euler_angles(units::Radian x, units::Radian y, units::Radian z, std::string sequence) const
    {
        this->get()->set_rotation(Rotation<double>::extrinsic_euler_angles(x, y, z, sequence));
    }


    /**
     * @brief Gets the local rotation.
     *
     * @return Rotation<double> The local rotation
     */
    template <IsSpectral TSpectral, typename TNode>
    Rotation<double> NodeHandle<TSpectral, TNode>::get_static_rotation() const {
        return this->get()->get_static_rotation();
    }


    /**
     * @brief Sets the angular velocity using a Vec3.
     *
     * @param angular_velocity The 3D angular velocity vector to set
     */
    template <IsSpectral TSpectral, typename TNode>
    void NodeHandle<TSpectral, TNode>::set_angular_velocity(const Vec3<double>& angular_velocity) const {
        this->get()->set_angular_velocity(angular_velocity);
    }

    /**
     * @brief Sets the angular velocity using individual components.
     *
     * @param wx The x-component of angular velocity
     * @param wy The y-component of angular velocity
     * @param wz The z-component of angular velocity
     */
    template <IsSpectral TSpectral, typename TNode>
    void NodeHandle<TSpectral, TNode>::set_angular_velocity(double wx, double wy, double wz) const {
        this->get()->set_angular_velocity(Vec3<double>{wx, wy, wz});
    }

    /**
     * @brief Gets the local angular velocity.
     *
     * @return Vec3<double> The local 3D angular velocity vector
     */
    template <IsSpectral TSpectral, typename TNode>
    Vec3<double> NodeHandle<TSpectral, TNode>::get_static_angular_velocity() const {
        return this->get()->get_static_angular_velocity();
    }


    /**
     * @brief Sets the scale using a Vec3.
     *
     * @param s The 3D scale vector to set
     */
    template <IsSpectral TSpectral, typename TNode>
    void NodeHandle<TSpectral, TNode>::set_scale(const Vec3<double>& scale) const {
        this->get()->set_scale(scale);
    }

    /**
     * @brief Sets the scale using individual components.
     *
     * @param sx The x-component of the scale
     * @param sy The y-component of the scale
     * @param sz The z-component of the scale
     */
    template <IsSpectral TSpectral, typename TNode>
    void NodeHandle<TSpectral, TNode>::set_scale(double sx, double sy, double sz) const {
        this->get()->set_scale(Vec3<double>{sx, sy, sz});
    }

    /**
     * @brief Sets the scale using a single scale factor.
     *
     * @param s The 3D scale factor to set
     */
    template <IsSpectral TSpectral, typename TNode>
    void NodeHandle<TSpectral, TNode>::set_scale(double s) const {
        this->get()->set_scale(Vec3<double>{s, s, s});
    }


    /**
     * @brief Gets the local scale.
     *
     * @return Vec3<double> The local 3D scale
     */
    template <IsSpectral TSpectral, typename TNode>
    Vec3<double> NodeHandle<TSpectral, TNode>::get_static_scale() const {
        return this->get()->get_static_scale();
    }


    /**
     * @brief Sets the SPICE origin identifier.
     *
     * @param spice_origin The SPICE origin string identifier
     */
    template <IsSpectral TSpectral, typename TNode>
    void NodeHandle<TSpectral, TNode>::set_spice_origin(const std::string& spice_origin) const
    {
        this->get()->set_spice_origin(spice_origin);
    }

    /**
     * @brief Sets the SPICE frame identifier.
     *
     * @param spice_origin The SPICE frame string identifier
     */
    template <IsSpectral TSpectral, typename TNode>
    void NodeHandle<TSpectral, TNode>::set_spice_frame(const std::string& spice_frame) const
    {
        this->get()->set_spice_frame(spice_frame);
    }


    /**
     * @brief Sets the SPICE frame identifier.
     *
     * @param spice_origin The SPICE frame string identifier
     */
    template <IsSpectral TSpectral, typename TNode>
    void NodeHandle<TSpectral, TNode>::set_spice(const std::string& spice_origin, const std::string& spice_frame) const {
        this->get()->set_spice(spice_origin, spice_frame);
    }

    /**
     * @brief Gets the SPICE origin identifier.
     *
     * @return std::string The SPICE origin string identifier
     */
    template <IsSpectral TSpectral, typename TNode>
    std::string NodeHandle<TSpectral, TNode>::get_spice_origin() const {
        return this->get()->get_spice_origin();
    }

    /**
     * @brief Gets the SPICE frame identifier.
     *
     * @return std::string The SPICE frame string identifier
     */
    template <IsSpectral TSpectral, typename TNode>
    std::string NodeHandle<TSpectral, TNode>::get_spice_frame() const {
        return this->get()->get_spice_frame();
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
    NodeHandle<TSpectral, Node<TSpectral>> NodeHandle<TSpectral, TNode>::get_parent() const {
        return this->get()->get_parent();
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
    NodeHandle<TSpectral, TParentNode> NodeHandle<TSpectral, TNode>::get_parent_as() const {
        return this->get()->template get_parent_as<TParentNode>();
    }
}
