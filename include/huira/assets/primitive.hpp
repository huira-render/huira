#pragma once

#include <memory>
#include <string>

#include "huira/concepts/spectral_concepts.hpp"
#include "huira/geometry/geometry.hpp"
#include "huira/materials/material.hpp"
#include "huira/scene/scene_object.hpp"
#include "huira/volumes/medium.hpp"

namespace huira {
template <IsSpectral TSpectral>
class Primitive : public SceneObject<Primitive<TSpectral>> {
  public:
    Primitive() = default;
    ~Primitive() override = default;

    Primitive(const Primitive&) = delete;
    Primitive& operator=(const Primitive&) = delete;

    Primitive(Primitive&&) = default;
    Primitive& operator=(Primitive&&) = default;

    std::shared_ptr<Geometry<TSpectral>> geometry;
    std::shared_ptr<Material<TSpectral>> material;
    std::shared_ptr<Medium<TSpectral>> medium;

    std::string type() const override { return "Primitive"; }
};
} // namespace huira
