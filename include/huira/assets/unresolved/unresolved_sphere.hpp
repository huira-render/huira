#pragma once

#include <string>
#include <vector>

#include "huira/assets/lights/light.hpp"
#include "huira/assets/unresolved/unresolved_object.hpp"
#include "huira/core/concepts/spectral_concepts.hpp"
#include "huira/scene/scene_view_types.hpp"

namespace huira {
    /**
     * @brief Represents an unresolved sphere with Lambertian reflectance illuminated by a light source.
     * 
     * UnresolvedLambertianSphere models a spherical body with uniform Lambertian scattering.
     * The apparent brightness depends on the phase angle between the light source, sphere,
     * and observer, calculated using Lambert's phase function. The reflected light is
     * computed based on the sphere's radius, albedo, and the incident irradiance.
     * 
     * @tparam TSpectral The spectral representation type.
     */
    template <IsSpectral TSpectral>
    class UnresolvedLambertianSphere : public UnresolvedObject<TSpectral> {
    public:
        UnresolvedLambertianSphere(units::Meter radius, InstanceHandle<TSpectral> light_instance, TSpectral albedo = TSpectral{ 1.f });

        void resolve_irradiance(
            const Transform<float>& self_transform,
            const std::vector<LightInstance<TSpectral>>& lights
        ) override;

        std::string type() const override { return "UnresolvedLambertianSphere"; }

    private:
        float radius_;
        std::shared_ptr<Instance<TSpectral>> light_instance_;
        Light<TSpectral>* light_;
        TSpectral albedo_;
    };
}

#include "huira_impl/assets/unresolved/unresolved_sphere.ipp"
