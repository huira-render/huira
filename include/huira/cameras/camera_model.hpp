#pragma once

#include <string>
#include <memory>
#include <cstdint>

#include "huira/core/types.hpp"
#include "huira/render/sampler.hpp"
#include "huira/core/concepts/numeric_concepts.hpp"
#include "huira/core/concepts/spectral_concepts.hpp"

#include "huira/cameras/distortion/distortion.hpp"
#include "huira/scene/node.hpp"
#include "huira/render/frame_buffer.hpp"
#include "huira/cameras/sensors/sensor_model.hpp"
#include "huira/scene/scene_object.hpp"

namespace huira {
    template <IsSpectral TSpectral>
    class CameraModel : public SceneObject<CameraModel<TSpectral>, TSpectral> {
    public:
        CameraModel();
        
        CameraModel(const CameraModel&) = delete;
        CameraModel& operator=(const CameraModel&) = delete;

        std::uint64_t id() const noexcept { return id_; }
        void set_name(const std::string& name) { name_ = name; }
        const std::string& name() const noexcept { return name_; }

        void set_focal_length(double focal_length);

        template <IsDistortion TDistortion, typename... Args>
        void set_distortion(Args&&... args);


        Pixel project_point(const Vec3<float>& point_camera_coords) const;


        int res_x() const { return sensor_->res_x(); }
        int res_y() const { return sensor_->res_y(); }

        std::string get_info() const { return "CameraModel[" + std::to_string(id_) + "]" + (name_.empty() ? "" : " " + name_); }

        FrameBuffer<TSpectral> make_frame_buffer() const { return FrameBuffer<TSpectral>(res_x(), res_y()); }

    protected:
        double focal_length_ = .05;

        std::unique_ptr<SensorModel<TSpectral>> sensor_;
        std::unique_ptr<Distortion<TSpectral>> distortion_ = nullptr;

        std::uint64_t id_ = 0;
        static inline std::uint64_t next_id_ = 0;
        std::string name_;
    };
}

#include "huira_impl/cameras/camera_model.ipp"
