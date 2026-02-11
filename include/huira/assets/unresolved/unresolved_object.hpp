#pragma once

#include <string>
#include <cstdint>
#include <vector>

#include "huira/core/concepts/spectral_concepts.hpp"
#include "huira/scene/node.hpp"
#include "huira/core/transform.hpp"
#include "huira/scene/scene_object.hpp"
#include "huira/scene/scene_view_types.hpp"

namespace huira {
    template <IsSpectral TSpectral>
    class UnresolvedObject : public SceneObject<UnresolvedObject<TSpectral>, TSpectral> {
    public:
        UnresolvedObject(TSpectral irradiance = TSpectral{ 0 })
            : id_(next_id_++), irradiance_(irradiance) {}

        UnresolvedObject(const UnresolvedObject&) = delete;
        UnresolvedObject& operator=(const UnresolvedObject&) = delete;

        void set_irradiance(TSpectral irradiance) { irradiance_ = irradiance; }
        TSpectral get_irradiance() const { return irradiance_; }

        virtual void resolve_irradiance(
            const Transform<float>& self_transform,
            const std::vector<LightInstance<TSpectral>>& lights
        );
        
        std::uint64_t id() const override { return id_; }
        virtual std::string type() const override { return "UnresolvedObject"; }

    protected:
        std::uint64_t id_ = 0;
        static inline std::uint64_t next_id_ = 0;

        TSpectral irradiance_{ 0 };
    };
}

#include "huira_impl/assets/unresolved/unresolved_object.ipp"
