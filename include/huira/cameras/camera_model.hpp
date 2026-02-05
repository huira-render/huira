#pragma once

#include <string>
#include <memory>
#include <cstdint>

#include "huira/core/types.hpp"
#include "huira/render/sampler.hpp"
#include "huira/core/concepts/numeric_concepts.hpp"
#include "huira/core/concepts/spectral_concepts.hpp"

#include "huira/cameras/distortion/distortion.hpp"
#include "huira/cameras/aperture/aperture.hpp"
#include "huira/scene/node.hpp"
#include "huira/render/frame_buffer.hpp"
#include "huira/cameras/sensors/sensor_model.hpp"
#include "huira/scene/scene_object.hpp"
#include "huira/cameras/psf/psf.hpp"

namespace huira {
    template <IsSpectral TSpectral>
    class CameraModel : public SceneObject<CameraModel<TSpectral>, TSpectral> {
    public:
        CameraModel();
        
        CameraModel(const CameraModel&) = delete;
        CameraModel& operator=(const CameraModel&) = delete;

        void set_focal_length(float focal_length);

        template <IsDistortion TDistortion, typename... Args>
        void set_distortion(Args&&... args);


        Pixel project_point(const Vec3<float>& point_camera_coords) const;

        void readout(FrameBuffer<TSpectral>& fb, float exposure_time) const { sensor_->readout(fb, exposure_time); }

        float get_projected_aperture_area(const Vec3<float>& direction) const
        {
            float cosTheta = glm::dot(glm::normalize(direction), Vec3<float>{0, 0, 1});
            return this->aperture_->get_area() * std::abs(cosTheta);
        }

        void set_fstop(float fstop)
        {
            float aperture_diameter = static_cast<float>(focal_length_) / fstop;
            float aperture_area = PI<float>() * (aperture_diameter * aperture_diameter) / 4.f;
            this->aperture_->set_area(aperture_area);

            if (use_aperture_psf_) {
                psf_ = aperture_->make_psf(focal_length_, sensor_->pixel_pitch());
            }
        }

        int res_x() const { return sensor_->res_x(); }
        int res_y() const { return sensor_->res_y(); }

        std::uint64_t id() const override { return id_; }
        std::string type() const override { return "CameraModel"; }

        void disable_psf() {
            psf_ = nullptr;
            use_aperture_psf_ = false;
        }
        void set_custom_psf(std::unique_ptr<PSF<TSpectral>> psf) {
            psf_ = std::move(psf);
            use_aperture_psf_ = false;
        }

        void use_aperture_psf(bool value = true) {
            use_aperture_psf_ = value;
            psf_ = aperture_->make_psf(focal_length_, sensor_->pixel_pitch());
        }
        bool has_psf() const { return psf_ != nullptr; }
        const Image<TSpectral>& get_psf_kernel(float u, float v) const {
            return psf_->get_kernel(u, v);
        }
        int get_psf_radius() const {
            return psf_->get_radius();
        }

        FrameBuffer<TSpectral> make_frame_buffer() const { return FrameBuffer<TSpectral>(res_x(), res_y()); }

    protected:
        float focal_length_ = .05f;

        std::unique_ptr<SensorModel<TSpectral>> sensor_;
        std::unique_ptr<Aperture<TSpectral>> aperture_;
        std::unique_ptr<Distortion<TSpectral>> distortion_ = nullptr;
        std::unique_ptr<PSF<TSpectral>> psf_ = nullptr;

        bool use_aperture_psf_ = false;

        std::uint64_t id_ = 0;
        static inline std::uint64_t next_id_ = 0;

        float fx_;
        float fy_;
        float cx_;
        float cy_;
        float rx_;
        float ry_;
        void compute_intrinsics_();
    };
}

#include "huira_impl/cameras/camera_model.ipp"
