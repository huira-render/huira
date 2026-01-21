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
    template <IsSpectral TSpectral, IsFloatingPoint TFloat>
    class Camera : public Node<TSpectral, TFloat> {
    public:
        explicit Camera(Scene<TSpectral, TFloat>* scene) : Node<TSpectral, TFloat>(scene) {}

        // Delete copying:
        Camera(const Camera&) = delete;
        Camera& operator=(const Camera&) = delete;

        void set_focal_length(TFloat focal_length);

        template <IsDistortion TDistortion, typename... Args>
        void set_distortion(Args&&... args);

        void look_at(const Vec3<TFloat>& target_position, Vec3<TFloat> up = Vec3<TFloat>{ 0,1,0 });

        std::string get_type_name() const override { return "Camera"; }

    protected:
        TFloat focal_length_ = static_cast<TFloat>(50.0);
        std::unique_ptr<Distortion<TSpectral, TFloat>> distortion_ = nullptr;
    };
}

#include "huira_impl/camera/camera.ipp"
