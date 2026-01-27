#pragma once

#include <string>
#include <memory>
#include <cstdint>

#include "huira/core/types.hpp"
#include "huira/detail/sampler.hpp"
#include "huira/detail/concepts/numeric_concepts.hpp"
#include "huira/detail/concepts/spectral_concepts.hpp"

#include "huira/cameras/distortion/distortion.hpp"
#include "huira/scene_graph/node.hpp"
#include "huira/render/frame_buffer.hpp"

namespace huira {
    template <IsSpectral TSpectral>
    class CameraModel : public std::enable_shared_from_this<CameraModel<TSpectral>> {
    public:
        CameraModel() noexcept : id_(next_id_++) {}
        
        CameraModel(const CameraModel&) = delete;
        CameraModel& operator=(const CameraModel&) = delete;

        std::uint64_t id() const noexcept { return id_; }
        void set_name(const std::string& name) { name_ = name; }
        const std::string& name() const noexcept { return name_; }

        void set_focal_length(double focal_length);

        template <IsDistortion TDistortion, typename... Args>
        void set_distortion(Args&&... args);

        std::string get_info() const { return "CameraModel[" + std::to_string(id_) + "]" + (name_.empty() ? "" : " " + name_); }

        FrameBuffer<TSpectral> make_frame_buffer() const { return FrameBuffer<TSpectral>(0, 0); }

    protected:
        double focal_length_ = 50.;
        std::unique_ptr<Distortion<TSpectral>> distortion_ = nullptr;

        std::uint64_t id_ = 0;
        static inline std::uint64_t next_id_ = 0;
        std::string name_;
    };
}

#include "huira_impl/cameras/camera_model.ipp"
