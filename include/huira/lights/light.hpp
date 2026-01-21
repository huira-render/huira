#pragma once

#include <string>
#include <memory>
#include <random>

#include "huira/core/types.hpp"
#include "huira/detail/sampler.hpp"
#include "huira/detail/concepts/numeric_concepts.hpp"
#include "huira/detail/concepts/spectral_concepts.hpp"
#include "huira/scene/node.hpp"

namespace huira {
    // Forward declare:
    template <IsSpectral TSpectral, IsFloatingPoint TFloat>
    class Scene;



    enum class LightType {
        Point,
        Sphere
    };

    template <IsSpectral TSpectral, IsFloatingPoint TFloat>
    struct LightSample {
        Vec3<TFloat> wi;
        TSpectral Li;
        TFloat distance;
        float pdf;
    };

    template <IsSpectral TSpectral, IsFloatingPoint TFloat>
    class Light : public Node<TSpectral, TFloat> {
    public:
        explicit Light(Scene<TSpectral, TFloat>* scene)
            : Node<TSpectral, TFloat>(scene)
        {

        }

        // Delete copying:
        Light(const Light&) = delete;
        Light& operator=(const Light&) = delete;

        ~Light() override = default;

        virtual LightSample<TSpectral, TFloat> sample_Li(const Vec3<TFloat>& point, Sampler<TFloat>& sampler) const = 0;

        virtual float pdf_Li(const Vec3<TFloat>& point, const Vec3<TFloat>& wi) const = 0;

        virtual LightType get_type() const = 0;

        std::string get_type_name() const override = 0;
    };
}
