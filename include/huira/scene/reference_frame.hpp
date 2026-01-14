#pragma once

#include <string>

#include "huira/detail/concepts/numeric_concepts.hpp"
#include "huira/detail/concepts/spectral_concepts.hpp"

namespace huira {
    // Forward declare instead of #include "scene.hpp"
    template <IsSpectral TSpectral, IsFloatingPoint TFloat>
    class Scene;

    template <IsSpectral TSpectral, IsFloatingPoint TFloat>
    class ReferenceFrame {
    public:
        ReferenceFrame(Scene<TSpectral, TFloat>* scene)
            : scene_(scene)
        {

        }

        double get_x() const {
            return x_;
        }

        const std::string& name() const;

    private:
        double x_ = 100;
        Scene<TSpectral, TFloat>* scene_; // Parent
    };

}

#include "huira_impl/scene/reference_frame.ipp"
