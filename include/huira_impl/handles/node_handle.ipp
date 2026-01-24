#include "huira/detail/concepts/numeric_concepts.hpp"
#include "huira/detail/concepts/spectral_concepts.hpp"
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
     * @brief Gets the global position.
     *
     * @return Vec3<double> The global 3D position vector
     */
    template <IsSpectral TSpectral, typename TNode>
    Vec3<double> NodeHandle<TSpectral, TNode>::get_global_position() const {
        return this->get()->get_global_position();
    }

    /**
     * @brief Gets the local position.
     *
     * @return Vec3<double> The local 3D position vector
     */
    template <IsSpectral TSpectral, typename TNode>
    Vec3<double> NodeHandle<TSpectral, TNode>::get_local_position() const {
        return this->get()->get_local_position();
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
     * @brief Gets the global velocity.
     *
     * @return Vec3<double> The global 3D velocity vector
     */
    template <IsSpectral TSpectral, typename TNode>
    Vec3<double> NodeHandle<TSpectral, TNode>::get_global_velocity() const {
        return this->get()->get_global_velocity();
    }

    /**
     * @brief Gets the local velocity.
     *
     * @return Vec3<double> The local 3D velocity vector
     */
    template <IsSpectral TSpectral, typename TNode>
    Vec3<double> NodeHandle<TSpectral, TNode>::get_local_velocity() const {
        return this->get()->get_local_velocity();
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

    /**
     * @brief Gets the global rotation.
     *
     * @return Rotation<double> The global rotation
     */
    template <IsSpectral TSpectral, typename TNode>
    Rotation<double> NodeHandle<TSpectral, TNode>::get_global_rotation() const {
        return this->get()->get_global_rotation();
    }

    /**
     * @brief Gets the local rotation.
     *
     * @return Rotation<double> The local rotation
     */
    template <IsSpectral TSpectral, typename TNode>
    Rotation<double> NodeHandle<TSpectral, TNode>::get_local_rotation() const {
        return this->get()->get_local_rotation();
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
     * @brief Gets the global angular velocity.
     *
     * @return Vec3<double> The global 3D angular velocity vector
     */
    template <IsSpectral TSpectral, typename TNode>
    Vec3<double> NodeHandle<TSpectral, TNode>::get_global_angular_velocity() const {
        return this->get()->get_global_angular_velocity();
    }

    /**
     * @brief Gets the local angular velocity.
     *
     * @return Vec3<double> The local 3D angular velocity vector
     */
    template <IsSpectral TSpectral, typename TNode>
    Vec3<double> NodeHandle<TSpectral, TNode>::get_local_angular_velocity() const {
        return this->get()->get_local_angular_velocity();
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
     * @brief Gets the global scale.
     *
     * @return Vec3<double> The local 3D scale
     */
    template <IsSpectral TSpectral, typename TNode>
    Vec3<double> NodeHandle<TSpectral, TNode>::get_global_scale() const {
        return this->get()->get_global_scale();
    }


    /**
     * @brief Gets the local scale.
     *
     * @return Vec3<double> The local 3D scale
     */
    template <IsSpectral TSpectral, typename TNode>
    Vec3<double> NodeHandle<TSpectral, TNode>::get_local_scale() const {
        return this->get()->get_local_scale();
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
     * @brief Gets the position in a specified SPICE frame.
     *
     * @param target_origin The target SPICE origin identifier
     * @param target_frame The target SPICE frame identifier
     * @return Vec3<double> The position in the specified SPICE frame
     */
    template <IsSpectral TSpectral, typename TNode>
    Vec3<double> NodeHandle<TSpectral, TNode>::get_position_in_frame(const std::string& target_origin, const std::string& target_frame) const
    {
        return this->get()->get_position_in_frame(target_origin, target_frame);
    }

    /**
     * @brief Gets the velocity in a specified SPICE frame.
     *
     * @param target_origin The target SPICE origin identifier
     * @param target_frame The target SPICE frame identifier
     * @return Vec3<double> The velocity in the specified SPICE frame
     */
    template <IsSpectral TSpectral, typename TNode>
    Vec3<double> NodeHandle<TSpectral, TNode>::get_velocity_in_frame(const std::string& target_origin, const std::string& target_frame) const
    {
        return this->get()->get_velocity_in_frame(target_origin, target_frame);
    }

    /**
     * @brief Gets the rotation in a specified SPICE frame.
     *
     * @param target_frame The target SPICE frame identifier
     * @return Rotation<double> The rotation in the specified SPICE frame
     */
    template <IsSpectral TSpectral, typename TNode>
    Rotation<double> NodeHandle<TSpectral, TNode>::get_rotation_in_frame(const std::string& target_frame) const
    {
        return this->get()->get_rotation_in_frame(target_frame);
    }

    /**
     * @brief Gets the angular velocity in a specified SPICE frame.
     *
     * @param target_frame The target SPICE frame identifier
     * @return Vec3<double> The angular velocity in the specified SPICE frame
     */
    template <IsSpectral TSpectral, typename TNode>
    Vec3<double> NodeHandle<TSpectral, TNode>::get_angular_velocity_in_frame(const std::string& target_frame) const
    {
        return this->get()->get_angular_velocity_in_frame(target_frame);
    }

    /**
     * @brief Gets the state (position and velocity) in a specified SPICE frame.
     *
     * @param target_origin The target SPICE origin identifier
     * @param target_frame The target SPICE frame identifier
     * @return std::pair<Vec3<double>, Vec3<double>> The position and velocity in the specified SPICE frame
     */
    template <IsSpectral TSpectral, typename TNode>
    std::pair<Vec3<double>, Vec3<double>> NodeHandle<TSpectral, TNode>::get_state_in_frame(const std::string& target_origin, const std::string& target_frame) const
    {
        return this->get()->get_state_in_frame(target_origin, target_frame);
    }

    /**
     * @brief Gets the attitude (rotation and angular velocity) in a specified SPICE frame.
     *
     * @param target_frame The target SPICE frame identifier
     * @return std::pair<Rotation<double>, Vec3<double>> The rotation and angular velocity in the specified SPICE frame
     */
    template <IsSpectral TSpectral, typename TNode>
    std::pair<Rotation<double>, Vec3<double>> NodeHandle<TSpectral, TNode>::get_attitude_in_frame(const std::string& target_frame) const
    {
        return this->get()->get_attitude_in_frame(target_frame);
    }
}
