#pragma once

#include <string>
#include <cstdint>
#include <optional>
#include <memory>

#include "huira/core/types.hpp"
#include "huira/core/transform.hpp"
#include "huira/render/interaction.hpp"
#include "huira/render/sampler.hpp"
#include "huira/core/concepts/spectral_concepts.hpp"
#include "huira/scene/scene_object.hpp"

namespace huira {
    enum class LightType {
        Point,
        Sphere
    };

    template <IsSpectral TSpectral>
    struct LightSample {
        Vec3<float> wi;
        TSpectral Li;
        float distance;
        float pdf;
    };

    template <IsSpectral TSpectral>
    class Light : public SceneObject<Light<TSpectral>, TSpectral> {
    public:
        Light() : id_(next_id_++) {}

        Light(const Light&) = delete;
        Light& operator=(const Light&) = delete;

        virtual ~Light() override = default;

        virtual std::optional<LightSample<TSpectral>> sample_li(
            const Interaction<TSpectral>& ref,
            const Transform<float>& light_to_world,
            const Sampler<float>& sampler
        ) const = 0;

        virtual float pdf_li(
            const Interaction<TSpectral>& ref,
            const Transform<float>& light_to_world,
            const Vec3<float>& wi
        ) const = 0;


        virtual LightType get_type() const = 0;

        std::uint64_t id() const override { return id_; }

    protected:
        std::uint64_t id_ = 0;
        static inline std::uint64_t next_id_ = 0;
    };
}
