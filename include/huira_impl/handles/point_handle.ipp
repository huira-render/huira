#include "huira/detail/concepts/numeric_concepts.hpp"
#include "huira/detail/concepts/spectral_concepts.hpp"
#include "huira/core/types.hpp"

namespace huira {

    /**
     * @brief Sets the position of the unresolved light source using a Vec3.
     *
     * @tparam TSpectral The spectral type satisfying the IsSpectral concept
     * @tparam TFloat The floating-point type satisfying the IsFloatingPoint concept
     * @param position The 3D position vector to set
     */
    template <IsSpectral TSpectral, IsFloatingPoint TFloat, typename TNode>
    void PointHandle<TSpectral, TFloat, TNode>::set_position(const Vec3<TFloat>& position) const {
        this->get()->set_position(position);
    }

    /**
     * @brief Sets the position of the unresolved light source using individual coordinates.
     *
     * @tparam TSpectral The spectral type satisfying the IsSpectral concept
     * @tparam TFloat The floating-point type satisfying the IsFloatingPoint concept
     * @param x The x-coordinate
     * @param y The y-coordinate
     * @param z The z-coordinate
     */
    template <IsSpectral TSpectral, IsFloatingPoint TFloat, typename TNode>
    void PointHandle<TSpectral, TFloat, TNode>::set_position(double x, double y, double z) const {
        this->get()->set_position(Vec3<TFloat>{x, y, z});
    }



    /**
     * @brief Sets the velocity of the unresolved light source using a Vec3.
     *
     * @tparam TSpectral The spectral type satisfying the IsSpectral concept
     * @tparam TFloat The floating-point type satisfying the IsFloatingPoint concept
     * @param velocity The 3D velocity vector to set
     */
    template <IsSpectral TSpectral, IsFloatingPoint TFloat, typename TNode>
    void PointHandle<TSpectral, TFloat, TNode>::set_velocity(const Vec3<TFloat>& velocity) const {
        this->get()->set_velocity(velocity);
    }

    /**
     * @brief Sets the velocity of the unresolved light source using individual components.
     *
     * @tparam TSpectral The spectral type satisfying the IsSpectral concept
     * @tparam TFloat The floating-point type satisfying the IsFloatingPoint concept
     * @param vx The x-component of velocity
     * @param vy The y-component of velocity
     * @param vz The z-component of velocity
     */
    template <IsSpectral TSpectral, IsFloatingPoint TFloat, typename TNode>
    void PointHandle<TSpectral, TFloat, TNode>::set_velocity(double vx, double vy, double vz) const {
        this->get()->set_velocity(Vec3<TFloat>{vx, vy, vz});
    }



    /**
     * @brief Sets the SPICE origin identifier for the unresolved light source.
     *
     * @tparam TSpectral The spectral type satisfying the IsSpectral concept
     * @tparam TFloat The floating-point type satisfying the IsFloatingPoint concept
     * @param spice_origin The SPICE origin string identifier
     */
    template <IsSpectral TSpectral, IsFloatingPoint TFloat, typename TNode>
    void PointHandle<TSpectral, TFloat, TNode>::set_spice_origin(const std::string& spice_origin) const
    {
        this->get()->set_spice_origin(spice_origin);
    }

    /**
     * @brief Gets the SPICE origin identifier for the unresolved light source.
     *
     * @tparam TSpectral The spectral type satisfying the IsSpectral concept
     * @tparam TFloat The floating-point type satisfying the IsFloatingPoint concept
     * @return std::string The SPICE origin string identifier
     */
    template <IsSpectral TSpectral, IsFloatingPoint TFloat, typename TNode>
    std::string PointHandle<TSpectral, TFloat, TNode>::get_spice_origin() const {
        return this->get()->get_spice_origin();
    }

    /**
     * @brief Gets the global position of the unresolved light source.
     *
     * @tparam TSpectral The spectral type satisfying the IsSpectral concept
     * @tparam TFloat The floating-point type satisfying the IsFloatingPoint concept
     * @return Vec3<TFloat> The global 3D position vector
     */
    template <IsSpectral TSpectral, IsFloatingPoint TFloat, typename TNode>
    Vec3<TFloat> PointHandle<TSpectral, TFloat, TNode>::get_global_position() const {
        return this->get()->get_global_position();
    }

    /**
     * @brief Gets the local position of the unresolved light source.
     *
     * @tparam TSpectral The spectral type satisfying the IsSpectral concept
     * @tparam TFloat The floating-point type satisfying the IsFloatingPoint concept
     * @return Vec3<TFloat> The local 3D position vector
     */
    template <IsSpectral TSpectral, IsFloatingPoint TFloat, typename TNode>
    Vec3<TFloat> PointHandle<TSpectral, TFloat, TNode>::get_local_position() const {
        return this->get()->get_local_position();
    }

    /**
     * @brief Gets the global velocity of the unresolved light source.
     *
     * @tparam TSpectral The spectral type satisfying the IsSpectral concept
     * @tparam TFloat The floating-point type satisfying the IsFloatingPoint concept
     * @return Vec3<TFloat> The global 3D velocity vector
     */
    template <IsSpectral TSpectral, IsFloatingPoint TFloat, typename TNode>
    Vec3<TFloat> PointHandle<TSpectral, TFloat, TNode>::get_global_velocity() const {
        return this->get()->get_global_velocity();
    }

    /**
     * @brief Gets the local velocity of the unresolved light source.
     *
     * @tparam TSpectral The spectral type satisfying the IsSpectral concept
     * @tparam TFloat The floating-point type satisfying the IsFloatingPoint concept
     * @return Vec3<TFloat> The local 3D velocity vector
     */
    template <IsSpectral TSpectral, IsFloatingPoint TFloat, typename TNode>
    Vec3<TFloat> PointHandle<TSpectral, TFloat, TNode>::get_local_velocity() const {
        return this->get()->get_local_velocity();
    }
}
