#pragma once

#include <memory>
#include <vector>

#include "huira/assets/unresolved/unresolved_object.hpp"
#include "huira/core/concepts/spectral_concepts.hpp"
#include "huira/assets/lights/light.hpp"
#include "huira/scene/scene_view_types.hpp"

namespace huira {
    template <IsSpectral TSpectral>
    class UnresolvedLambertianSphere : public UnresolvedObject<TSpectral> {
    public:
        UnresolvedLambertianSphere(units::Meter radius, InstanceHandle<TSpectral> light_instance, TSpectral albedo = TSpectral{ 1.f });

        void resolve_irradiance(
            const Transform<float>& self_transform,
            const std::vector<LightInstance<TSpectral>>& lights
        ) override;

    private:
        float radius_;
        std::shared_ptr<Instance<TSpectral>> light_instance_;
        Light<TSpectral>* light_;
        TSpectral albedo_;
    };
}

#include "huira_impl/assets/unresolved/unresolved_sphere.ipp"
