#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "huira/core/concepts/spectral_concepts.hpp"
#include "huira/core/time.hpp"
#include "huira/core/transform.hpp"
#include "huira/scene/node.hpp"
#include "huira/scene/scene_object.hpp"
#include "huira/scene/scene_view_types.hpp"

namespace huira {
    /**
     * @brief Represents an unresolved object to be rendered.
     * 
     * UnresolvedObject serves as a base class for objects whose irradiance can be
     * computed or updated based on light sources in the scene. Subclasses can override
     * resolve_irradiance() to implement custom irradiance computation logic.  This base
     * class assumes that the object's spectral irradiance is constant and does not
     * depend on any observer or light positions.
     * 
     * @tparam TSpectral The spectral representation type.
     */
    template <IsSpectral TSpectral>
    class UnresolvedObject : public SceneObject<UnresolvedObject<TSpectral>, TSpectral> {
    public:
        UnresolvedObject() = default;
        UnresolvedObject(const units::SpectralWattsPerMeterSquared<TSpectral>& spectral_irradiance);
        UnresolvedObject(const units::WattsPerMeterSquared& irradiance);

        UnresolvedObject(const UnresolvedObject&) = delete;
        UnresolvedObject& operator=(const UnresolvedObject&) = delete;

        void set_irradiance(const units::SpectralWattsPerMeterSquared<TSpectral>& spectral_irradiance);
        void set_irradiance(const units::WattsPerMeterSquared& irradiance);

        virtual TSpectral get_irradiance(Time time) const;

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
