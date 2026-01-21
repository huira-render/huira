#include "huira/detail/concepts/numeric_concepts.hpp"
#include "huira/detail/concepts/spectral_concepts.hpp"
#include "huira/core/types.hpp"

namespace huira {

    /**
     * @brief Sets the position using a Vec3.
     *
     * @tparam TSpectral The spectral type satisfying the IsSpectral concept
     * @tparam TFloat The floating-point type satisfying the IsFloatingPoint concept
     * @param position The 3D position vector to set
     */
    template <IsSpectral TSpectral, IsFloatingPoint TFloat, typename TNode>
    void NodeHandle<TSpectral, TFloat, TNode>::set_position(const Vec3<TFloat>& position) const {
        this->get()->set_position(position);
    }

    /**
     * @brief Sets the position using individual coordinates.
     *
     * @tparam TSpectral The spectral type satisfying the IsSpectral concept
     * @tparam TFloat The floating-point type satisfying the IsFloatingPoint concept
     * @param x The x-coordinate
     * @param y The y-coordinate
     * @param z The z-coordinate
     */
    template <IsSpectral TSpectral, IsFloatingPoint TFloat, typename TNode>
    void NodeHandle<TSpectral, TFloat, TNode>::set_position(double x, double y, double z) const {
        this->get()->set_position(Vec3<TFloat>{x, y, z});
    }

    /**
     * @brief Gets the global position.
     *
     * @tparam TSpectral The spectral type satisfying the IsSpectral concept
     * @tparam TFloat The floating-point type satisfying the IsFloatingPoint concept
     * @return Vec3<TFloat> The global 3D position vector
     */
    template <IsSpectral TSpectral, IsFloatingPoint TFloat, typename TNode>
    Vec3<TFloat> NodeHandle<TSpectral, TFloat, TNode>::get_global_position() const {
        return this->get()->get_global_position();
    }

    /**
     * @brief Gets the local position.
     *
     * @tparam TSpectral The spectral type satisfying the IsSpectral concept
     * @tparam TFloat The floating-point type satisfying the IsFloatingPoint concept
     * @return Vec3<TFloat> The local 3D position vector
     */
    template <IsSpectral TSpectral, IsFloatingPoint TFloat, typename TNode>
    Vec3<TFloat> NodeHandle<TSpectral, TFloat, TNode>::get_local_position() const {
        return this->get()->get_local_position();
    }



    /**
     * @brief Sets the velocity using a Vec3.
     *
     * @tparam TSpectral The spectral type satisfying the IsSpectral concept
     * @tparam TFloat The floating-point type satisfying the IsFloatingPoint concept
     * @param velocity The 3D velocity vector to set
     */
    template <IsSpectral TSpectral, IsFloatingPoint TFloat, typename TNode>
    void NodeHandle<TSpectral, TFloat, TNode>::set_velocity(const Vec3<TFloat>& velocity) const {
        this->get()->set_velocity(velocity);
    }

    /**
     * @brief Sets the velocity using individual components.
     *
     * @tparam TSpectral The spectral type satisfying the IsSpectral concept
     * @tparam TFloat The floating-point type satisfying the IsFloatingPoint concept
     * @param vx The x-component of velocity
     * @param vy The y-component of velocity
     * @param vz The z-component of velocity
     */
    template <IsSpectral TSpectral, IsFloatingPoint TFloat, typename TNode>
    void NodeHandle<TSpectral, TFloat, TNode>::set_velocity(double vx, double vy, double vz) const {
        this->get()->set_velocity(Vec3<TFloat>{vx, vy, vz});
    }

    /**
     * @brief Gets the global velocity.
     *
     * @tparam TSpectral The spectral type satisfying the IsSpectral concept
     * @tparam TFloat The floating-point type satisfying the IsFloatingPoint concept
     * @return Vec3<TFloat> The global 3D velocity vector
     */
    template <IsSpectral TSpectral, IsFloatingPoint TFloat, typename TNode>
    Vec3<TFloat> NodeHandle<TSpectral, TFloat, TNode>::get_global_velocity() const {
        return this->get()->get_global_velocity();
    }

    /**
     * @brief Gets the local velocity.
     *
     * @tparam TSpectral The spectral type satisfying the IsSpectral concept
     * @tparam TFloat The floating-point type satisfying the IsFloatingPoint concept
     * @return Vec3<TFloat> The local 3D velocity vector
     */
    template <IsSpectral TSpectral, IsFloatingPoint TFloat, typename TNode>
    Vec3<TFloat> NodeHandle<TSpectral, TFloat, TNode>::get_local_velocity() const {
        return this->get()->get_local_velocity();
    }


    /**
     * @brief Sets the rotation.
     *
     * @tparam TSpectral The spectral type satisfying the IsSpectral concept
     * @tparam TFloat The floating-point type satisfying the IsFloatingPoint concept
     * @param rotation The rotation to set
     */
    template <IsSpectral TSpectral, IsFloatingPoint TFloat, typename TNode>
    void NodeHandle<TSpectral, TFloat, TNode>::set_rotation(const Rotation<TFloat>& rotation) const {
        this->get()->set_rotation(rotation);
    }

    /**
     * @brief Gets the global rotation.
     *
     * @tparam TSpectral The spectral type satisfying the IsSpectral concept
     * @tparam TFloat The floating-point type satisfying the IsFloatingPoint concept
     * @return Rotation<TFloat> The global rotation
     */
    template <IsSpectral TSpectral, IsFloatingPoint TFloat, typename TNode>
    Rotation<TFloat> NodeHandle<TSpectral, TFloat, TNode>::get_global_rotation() const {
        return this->get()->get_global_rotation();
    }

    /**
     * @brief Gets the local rotation.
     *
     * @tparam TSpectral The spectral type satisfying the IsSpectral concept
     * @tparam TFloat The floating-point type satisfying the IsFloatingPoint concept
     * @return Rotation<TFloat> The local rotation
     */
    template <IsSpectral TSpectral, IsFloatingPoint TFloat, typename TNode>
    Rotation<TFloat> NodeHandle<TSpectral, TFloat, TNode>::get_local_rotation() const {
        return this->get()->get_local_rotation();
    }


    /**
     * @brief Sets the angular velocity using a Vec3.
     *
     * @tparam TSpectral The spectral type satisfying the IsSpectral concept
     * @tparam TFloat The floating-point type satisfying the IsFloatingPoint concept
     * @param angular_velocity The 3D angular velocity vector to set
     */
    template <IsSpectral TSpectral, IsFloatingPoint TFloat, typename TNode>
    void NodeHandle<TSpectral, TFloat, TNode>::set_angular_velocity(const Vec3<TFloat>& angular_velocity) const {
        this->get()->set_angular_velocity(angular_velocity);
    }

    /**
     * @brief Sets the angular velocity using individual components.
     *
     * @tparam TSpectral The spectral type satisfying the IsSpectral concept
     * @tparam TFloat The floating-point type satisfying the IsFloatingPoint concept
     * @param wx The x-component of angular velocity
     * @param wy The y-component of angular velocity
     * @param wz The z-component of angular velocity
     */
    template <IsSpectral TSpectral, IsFloatingPoint TFloat, typename TNode>
    void NodeHandle<TSpectral, TFloat, TNode>::set_angular_velocity(double wx, double wy, double wz) const {
        this->get()->set_angular_velocity(Vec3<TFloat>{wx, wy, wz});
    }

    /**
     * @brief Gets the global angular velocity.
     *
     * @tparam TSpectral The spectral type satisfying the IsSpectral concept
     * @tparam TFloat The floating-point type satisfying the IsFloatingPoint concept
     * @return Vec3<TFloat> The global 3D angular velocity vector
     */
    template <IsSpectral TSpectral, IsFloatingPoint TFloat, typename TNode>
    Vec3<TFloat> NodeHandle<TSpectral, TFloat, TNode>::get_global_angular_velocity() const {
        return this->get()->get_global_angular_velocity();
    }

    /**
     * @brief Gets the local angular velocity.
     *
     * @tparam TSpectral The spectral type satisfying the IsSpectral concept
     * @tparam TFloat The floating-point type satisfying the IsFloatingPoint concept
     * @return Vec3<TFloat> The local 3D angular velocity vector
     */
    template <IsSpectral TSpectral, IsFloatingPoint TFloat, typename TNode>
    Vec3<TFloat> NodeHandle<TSpectral, TFloat, TNode>::get_local_angular_velocity() const {
        return this->get()->get_local_angular_velocity();
    }


    /**
     * @brief Sets the scale using a Vec3.
     *
     * @tparam TSpectral The spectral type satisfying the IsSpectral concept
     * @tparam TFloat The floating-point type satisfying the IsFloatingPoint concept
     * @param s The 3D scale vector to set
     */
    template <IsSpectral TSpectral, IsFloatingPoint TFloat, typename TNode>
    void NodeHandle<TSpectral, TFloat, TNode>::set_scale(const Vec3<TFloat>& scale) const {
        this->get()->set_scale(scale);
    }

    /**
     * @brief Sets the scale using individual components.
     *
     * @tparam TSpectral The spectral type satisfying the IsSpectral concept
     * @tparam TFloat The floating-point type satisfying the IsFloatingPoint concept
     * @param sx The x-component of the scale
     * @param sy The y-component of the scale
     * @param sz The z-component of the scale
     */
    template <IsSpectral TSpectral, IsFloatingPoint TFloat, typename TNode>
    void NodeHandle<TSpectral, TFloat, TNode>::set_scale(double sx, double sy, double sz) const {
        this->get()->set_scale(Vec3<TFloat>{sx, sy, sz});
    }

    /**
     * @brief Sets the scale using a single scale factor.
     *
     * @tparam TSpectral The spectral type satisfying the IsSpectral concept
     * @tparam TFloat The floating-point type satisfying the IsFloatingPoint concept
     * @param s The 3D scale factor to set
     */
    template <IsSpectral TSpectral, IsFloatingPoint TFloat, typename TNode>
    void NodeHandle<TSpectral, TFloat, TNode>::set_scale(double s) const {
        this->get()->set_scale(Vec3<TFloat>{s, s, s});
    }


    /**
     * @brief Gets the global scale.
     *
     * @tparam TSpectral The spectral type satisfying the IsSpectral concept
     * @tparam TFloat The floating-point type satisfying the IsFloatingPoint concept
     * @return Vec3<TFloat> The local 3D scale
     */
    template <IsSpectral TSpectral, IsFloatingPoint TFloat, typename TNode>
    Vec3<TFloat> NodeHandle<TSpectral, TFloat, TNode>::get_global_scale() const {
        return this->get()->get_global_scale();
    }


    /**
     * @brief Gets the local scale.
     *
     * @tparam TSpectral The spectral type satisfying the IsSpectral concept
     * @tparam TFloat The floating-point type satisfying the IsFloatingPoint concept
     * @return Vec3<TFloat> The local 3D scale
     */
    template <IsSpectral TSpectral, IsFloatingPoint TFloat, typename TNode>
    Vec3<TFloat> NodeHandle<TSpectral, TFloat, TNode>::get_local_scale() const {
        return this->get()->get_local_scale();
    }


    /**
     * @brief Sets the SPICE origin identifier.
     *
     * @tparam TSpectral The spectral type satisfying the IsSpectral concept
     * @tparam TFloat The floating-point type satisfying the IsFloatingPoint concept
     * @param spice_origin The SPICE origin string identifier
     */
    template <IsSpectral TSpectral, IsFloatingPoint TFloat, typename TNode>
    void NodeHandle<TSpectral, TFloat, TNode>::set_spice_origin(const std::string& spice_origin) const
    {
        this->get()->set_spice_origin(spice_origin);
    }

    /**
     * @brief Sets the SPICE frame identifier.
     *
     * @tparam TSpectral The spectral type satisfying the IsSpectral concept
     * @tparam TFloat The floating-point type satisfying the IsFloatingPoint concept
     * @param spice_origin The SPICE frame string identifier
     */
    template <IsSpectral TSpectral, IsFloatingPoint TFloat, typename TNode>
    void NodeHandle<TSpectral, TFloat, TNode>::set_spice_frame(const std::string& spice_frame) const
    {
        this->get()->set_spice_frame(spice_frame);
    }


    /**
     * @brief Sets the SPICE frame identifier.
     *
     * @tparam TSpectral The spectral type satisfying the IsSpectral concept
     * @tparam TFloat The floating-point type satisfying the IsFloatingPoint concept
     * @param spice_origin The SPICE frame string identifier
     */
    template <IsSpectral TSpectral, IsFloatingPoint TFloat, typename TNode>
    void NodeHandle<TSpectral, TFloat, TNode>::set_spice(const std::string& spice_origin, const std::string& spice_frame) const {
        this->get()->set_spice(spice_origin, spice_frame);
    }

    /**
     * @brief Gets the SPICE origin identifier.
     *
     * @tparam TSpectral The spectral type satisfying the IsSpectral concept
     * @tparam TFloat The floating-point type satisfying the IsFloatingPoint concept
     * @return std::string The SPICE origin string identifier
     */
    template <IsSpectral TSpectral, IsFloatingPoint TFloat, typename TNode>
    std::string NodeHandle<TSpectral, TFloat, TNode>::get_spice_origin() const {
        return this->get()->get_spice_origin();
    }

    /**
     * @brief Gets the SPICE frame identifier.
     *
     * @tparam TSpectral The spectral type satisfying the IsSpectral concept
     * @tparam TFloat The floating-point type satisfying the IsFloatingPoint concept
     * @return std::string The SPICE frame string identifier
     */
    template <IsSpectral TSpectral, IsFloatingPoint TFloat, typename TNode>
    std::string NodeHandle<TSpectral, TFloat, TNode>::get_spice_frame() const {
        return this->get()->get_spice_frame();
    }
}
