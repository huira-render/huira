#pragma once

#include <string>
#include <memory>
#include <random>

#include "huira/core/types.hpp"
#include "huira/detail/sampler.hpp"
#include "huira/detail/concepts/numeric_concepts.hpp"
#include "huira/detail/concepts/spectral_concepts.hpp"

#include "huira/cameras/distortion/distortion.hpp"
#include "huira/scene_graph/node.hpp"

namespace huira {
    template <IsSpectral TSpectral>
    class Camera : public Node<TSpectral> {
    public:
        explicit Camera(Scene<TSpectral>* scene) : Node<TSpectral>(scene) {}

        Camera(const Camera&) = delete;
        Camera& operator=(const Camera&) = delete;

        void set_focal_length(double focal_length);

        template <IsDistortion TDistortion, typename... Args>
        void set_distortion(Args&&... args);

        std::string get_info() const override { return "Camera[" + std::to_string(this->id()) + "]" + (this->name_.empty() ? "" : " " + this->name_); }

    protected:
        double focal_length_ = 50.;
        std::unique_ptr<Distortion<TSpectral>> distortion_ = nullptr;
    };
}

#include "huira_impl/cameras/camera.ipp"
