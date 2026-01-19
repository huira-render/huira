#pragma once

#include <string>
#include <memory>
#include <random>

#include "huira/camera/distortion/distortion.hpp"
#include "huira/core/types.hpp"
#include "huira/detail/sampler.hpp"
#include "huira/detail/concepts/numeric_concepts.hpp"
#include "huira/detail/concepts/spectral_concepts.hpp"
#include "huira/scene/node.hpp"

namespace huira {
    // Forward declare:
    template <IsSpectral TSpectral, IsFloatingPoint TFloat>
    class Scene;



    template <IsSpectral TSpectral, IsFloatingPoint TFloat>
    class Camera : public Node<TSpectral, TFloat> {
    public:
        explicit Camera(Scene<TSpectral, TFloat>* scene)
            : Node<TSpectral, TFloat>(scene)
        {

        }

        void set_focal_length(TFloat focal_length) {
            focal_length_ = focal_length;
        }

        template <IsDistortion TDistortion, typename... Args>
        void set_distortion(Args&&... args) {
            distortion_ = std::make_unique<TDistortion>(std::forward<Args>(args)...);
        }

    protected:
        TFloat focal_length_ = static_cast<TFloat>(50.0);
        std::unique_ptr<Distortion<TSpectral, TFloat>> distortion_ = nullptr;

        std::string get_type_name_() const override { return "Camera"; }
    };
}

#include "huira_impl/camera/camera.ipp"
