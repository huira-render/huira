#include <string>

#include "huira/detail/concepts/numeric_concepts.hpp"
#include "huira/detail/concepts/spectral_concepts.hpp"
#include "huira/core/types.hpp"

namespace huira {
    /**
     * @brief Sets the irradiance of the unresolved light source.
     * 
     * @tparam TSpectral The spectral type satisfying the IsSpectral concept
     * @tparam TFloat The floating-point type satisfying the IsFloatingPoint concept
     * @param irradiance The spectral irradiance value to set
     */
    template <IsSpectral TSpectral, IsFloatingPoint TFloat>
    void UnresolvedHandle<TSpectral, TFloat>::set_irradiance(const TSpectral& irradiance) const
    {
        this->get()->set_irradiance(irradiance);
    }

    /**
     * @brief Gets the irradiance of the unresolved light source.
     * 
     * @tparam TSpectral The spectral type satisfying the IsSpectral concept
     * @tparam TFloat The floating-point type satisfying the IsFloatingPoint concept
     * @return TSpectral The current spectral irradiance value
     */
    template <IsSpectral TSpectral, IsFloatingPoint TFloat>
    TSpectral UnresolvedHandle<TSpectral, TFloat>::get_irradiance() const {
        return this->get()->get_irradiance();
    }
    


    /**
     * @brief Sets the position of the unresolved light source using a Vec3.
     * 
     * @tparam TSpectral The spectral type satisfying the IsSpectral concept
     * @tparam TFloat The floating-point type satisfying the IsFloatingPoint concept
     * @param position The 3D position vector to set
     */
    template <IsSpectral TSpectral, IsFloatingPoint TFloat>
    void UnresolvedHandle<TSpectral, TFloat>::set_position(const Vec3<TFloat>& position) const {
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
    template <IsSpectral TSpectral, IsFloatingPoint TFloat>
    void UnresolvedHandle<TSpectral, TFloat>::set_position(double x, double y, double z) const {
        this->get()->set_position(Vec3<TFloat>{x, y, z});
    }



    /**
     * @brief Sets the velocity of the unresolved light source using a Vec3.
     * 
     * @tparam TSpectral The spectral type satisfying the IsSpectral concept
     * @tparam TFloat The floating-point type satisfying the IsFloatingPoint concept
     * @param velocity The 3D velocity vector to set
     */
    template <IsSpectral TSpectral, IsFloatingPoint TFloat>
    void UnresolvedHandle<TSpectral, TFloat>::set_velocity(const Vec3<TFloat>& velocity) const {
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
    template <IsSpectral TSpectral, IsFloatingPoint TFloat>
    void UnresolvedHandle<TSpectral, TFloat>::set_velocity(double vx, double vy, double vz) const {
        this->get()->set_velocity(Vec3<TFloat>{vx, vy, vz});
    }



    /**
     * @brief Sets the SPICE origin identifier for the unresolved light source.
     * 
     * @tparam TSpectral The spectral type satisfying the IsSpectral concept
     * @tparam TFloat The floating-point type satisfying the IsFloatingPoint concept
     * @param spice_origin The SPICE origin string identifier
     */
    template <IsSpectral TSpectral, IsFloatingPoint TFloat>
    void UnresolvedHandle<TSpectral, TFloat>::set_spice_origin(const std::string& spice_origin) const
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
    template <IsSpectral TSpectral, IsFloatingPoint TFloat>
    std::string UnresolvedHandle<TSpectral, TFloat>::get_spice_origin() const {
        return this->get()->get_spice_origin();
    }
}
