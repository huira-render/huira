#pragma once

#include <string>
#include <cstdint>

#include "huira/core/concepts/spectral_concepts.hpp"
#include "huira/scene/node.hpp"
#include "huira/scene/scene_object.hpp"

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
        
        std::uint64_t id() const override { return id_; }
        std::string type() const override { return "UnresolvedObject"; }

    protected:
        std::uint64_t id_ = 0;
        static inline std::uint64_t next_id_ = 0;

        TSpectral irradiance_{ 0 };
    };
}

#include "huira_impl/assets/unresolved_object.ipp"
