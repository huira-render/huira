#pragma once

#include <cstdint>
#include <memory>
#include <string>

#include "huira/core/concepts/spectral_concepts.hpp"
#include "huira/scene/scene_object.hpp"

#include "huira/geometry/geometry.hpp"
#include "huira/materials/material.hpp"

namespace huira {
    template <IsSpectral TSpectral>
    class Primitive : public SceneObject<Primitive<TSpectral>> {
    public:
        Primitive() : id_(next_id_++) {}
        ~Primitive() override = default;

        Primitive(const Primitive&) = delete;
        Primitive& operator=(const Primitive&) = delete;

        Primitive(Primitive&&) = default;
        Primitive& operator=(Primitive&&) = default;

        std::shared_ptr<Geometry<TSpectral>> geometry;
        std::shared_ptr<Material<TSpectral>> material;

        std::uint64_t id() const override { return id_; }
        std::string type() const override { return "Primitive"; }

    private:
        std::uint64_t id_ = 0;
        static inline std::uint64_t next_id_ = 0;
    };
}
