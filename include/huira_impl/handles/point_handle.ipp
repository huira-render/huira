#include "huira/detail/concepts/numeric_concepts.hpp"
#include "huira/detail/concepts/spectral_concepts.hpp"
#include "huira/core/types.hpp"

namespace huira {

    /**
     * @brief Sets the position of the unresolved light source using a Vec3.
     *
     * @param position The 3D position vector to set
     */
    template <IsSpectral TSpectral, typename TNode>
    void PointHandle<TSpectral, TNode>::set_position(const Vec3<double>& position) const {
        this->get()->set_position(position);
    }

    /**
     * @brief Sets the position of the unresolved light source using individual coordinates.
     *
     * @param x The x-coordinate
     * @param y The y-coordinate
     * @param z The z-coordinate
     */
    template <IsSpectral TSpectral, typename TNode>
    void PointHandle<TSpectral, TNode>::set_position(double x, double y, double z) const {
        this->get()->set_position(Vec3<double>{x, y, z});
    }



    /**
     * @brief Sets the velocity of the unresolved light source using a Vec3.
     *
     * @param velocity The 3D velocity vector to set
     */
    template <IsSpectral TSpectral, typename TNode>
    void PointHandle<TSpectral, TNode>::set_velocity(const Vec3<double>& velocity) const {
        this->get()->set_velocity(velocity);
    }

    /**
     * @brief Sets the velocity of the unresolved light source using individual components.
     *
     * @param vx The x-component of velocity
     * @param vy The y-component of velocity
     * @param vz The z-component of velocity
     */
    template <IsSpectral TSpectral, typename TNode>
    void PointHandle<TSpectral, TNode>::set_velocity(double vx, double vy, double vz) const {
        this->get()->set_velocity(Vec3<double>{vx, vy, vz});
    }



    /**
     * @brief Sets the SPICE origin identifier for the unresolved light source.
     *
     * @param spice_origin The SPICE origin string identifier
     */
    template <IsSpectral TSpectral, typename TNode>
    void PointHandle<TSpectral, TNode>::set_spice_origin(const std::string& spice_origin) const
    {
        this->get()->set_spice_origin(spice_origin);
    }

    /**
     * @brief Gets the SPICE origin identifier for the unresolved light source.
     *
     * @return std::string The SPICE origin string identifier
     */
    template <IsSpectral TSpectral, typename TNode>
    std::string PointHandle<TSpectral, TNode>::get_spice_origin() const {
        return this->get()->get_spice_origin();
    }

    /**
     * @brief Gets the global position of the unresolved light source.
     *
     * @return Vec3<double> The global 3D position vector
     */
    template <IsSpectral TSpectral, typename TNode>
    Vec3<double> PointHandle<TSpectral, TNode>::get_global_position() const {
        return this->get()->get_global_position();
    }

    /**
     * @brief Gets the local position of the unresolved light source.
     *
     * @return Vec3<double> The local 3D position vector
     */
    template <IsSpectral TSpectral, typename TNode>
    Vec3<double> PointHandle<TSpectral, TNode>::get_local_position() const {
        return this->get()->get_local_position();
    }

    /**
     * @brief Gets the global velocity of the unresolved light source.
     *
     * @return Vec3<double> The global 3D velocity vector
     */
    template <IsSpectral TSpectral, typename TNode>
    Vec3<double> PointHandle<TSpectral, TNode>::get_global_velocity() const {
        return this->get()->get_global_velocity();
    }

    /**
     * @brief Gets the local velocity of the unresolved light source.
     *
     * @return Vec3<double> The local 3D velocity vector
     */
    template <IsSpectral TSpectral, typename TNode>
    Vec3<double> PointHandle<TSpectral, TNode>::get_local_velocity() const {
        return this->get()->get_local_velocity();
    }


    /**
     * @brief Gets the position in a specified SPICE frame.
     *
     * @param target_origin The target SPICE origin identifier
     * @param target_frame The target SPICE frame identifier
     * @return Vec3<double> The position in the specified SPICE frame
     */
    template <IsSpectral TSpectral, typename TNode>
    Vec3<double> PointHandle<TSpectral, TNode>::get_position_in_frame(const std::string& target_origin, const std::string& target_frame) const
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
    Vec3<double> PointHandle<TSpectral, TNode>::get_velocity_in_frame(const std::string& target_origin, const std::string& target_frame) const
    {
        return this->get()->get_velocity_in_frame(target_origin, target_frame);
    }

    /**
     * @brief Gets the state (position and velocity) in a specified SPICE frame.
     *
     * @param target_origin The target SPICE origin identifier
     * @param target_frame The target SPICE frame identifier
     * @return std::pair<Vec3<double>, Vec3<double>> The position and velocity in the specified SPICE frame
     */
    template <IsSpectral TSpectral, typename TNode>
    std::pair<Vec3<double>, Vec3<double>> PointHandle<TSpectral, TNode>::get_state_in_frame(const std::string& target_origin, const std::string& target_frame) const
    {
        return this->get()->get_state_in_frame(target_origin, target_frame);
    }
}
