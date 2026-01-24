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
     * @brief Gets the local position of the unresolved light source.
     *
     * @return Vec3<double> The local 3D position vector
     */
    template <IsSpectral TSpectral, typename TNode>
    Vec3<double> PointHandle<TSpectral, TNode>::get_static_position() const {
        return this->get()->get_static_position();
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
     * @brief Gets the local velocity of the unresolved light source.
     *
     * @return Vec3<double> The local 3D velocity vector
     */
    template <IsSpectral TSpectral, typename TNode>
    Vec3<double> PointHandle<TSpectral, TNode>::get_static_velocity() const {
        return this->get()->get_static_velocity();
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
}
