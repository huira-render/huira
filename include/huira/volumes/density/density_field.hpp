#pragma once

#include <cstdint>
#include <string>

#include "huira/core/concepts/spectral_concepts.hpp"
#include "huira/core/types.hpp"
#include "huira/scene/scene_object.hpp"
#include "huira/volumes/medium_properties.hpp"

namespace huira {
    template <IsSpectral TSpectral>
    class DensityField : public SceneObject<DensityField<TSpectral>> {
    public:
        DensityField() = default;
        virtual ~DensityField() override = default;

        [[nodiscard]] virtual MediumProperties<TSpectral> evaluate(const Vec3<float>& p) const = 0;

        std::uint64_t id() const override { return id_; }
        virtual std::string type() const override = 0;

    private:
        std::uint64_t id_ = 0;
        static inline std::uint64_t next_id_ = 0;
    };
}
