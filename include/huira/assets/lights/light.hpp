#pragma once

#include <string>
#include <cstdint>
#include <optional>

#include "huira/core/types.hpp"
#include "huira/core/transform.hpp"
#include "huira/core/interaction.hpp"
#include "huira/detail/sampler.hpp"
#include "huira/detail/concepts/spectral_concepts.hpp"

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
    class Light {
    public:
        Light() : id_(next_id_++) {}

        Light(const Light&) = delete;
        Light& operator=(const Light&) = delete;

        virtual ~Light() = default;

        std::uint64_t id() const noexcept { return id_; }

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
        virtual std::string get_type_name() const = 0;

    private:
        std::uint64_t id_ = 0;
        static inline std::uint64_t next_id_ = 0;
    };
}
