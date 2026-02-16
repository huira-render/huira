
#pragma once

#include <cstdint>
#include <memory>
#include <string>

#include "huira/core/concepts/numeric_concepts.hpp"
#include "huira/core/concepts/spectral_concepts.hpp"
#include "huira/core/types.hpp"
#include "huira/render/sampler.hpp"

#include "huira/cameras/apertures/aperture.hpp"
#include "huira/cameras/distortion/brown_distortion.hpp"
#include "huira/cameras/distortion/distortion.hpp"
#include "huira/cameras/distortion/opencv_distortion.hpp"
#include "huira/cameras/distortion/owen_distortion.hpp"
#include "huira/cameras/psfs/psf.hpp"
#include "huira/cameras/sensors/sensor_model.hpp"
#include "huira/render/frame_buffer.hpp"
#include "huira/scene/node.hpp"
#include "huira/scene/scene_object.hpp"


namespace huira {
    template <IsSpectral TSpectral>
    class CameraModelHandle;

    /**
     * @brief CameraModel represents a pinhole or thin-lens camera with configurable sensor, aperture, and distortion models.
     *
     * This class provides a flexible camera abstraction for rendering and simulation, supporting various sensor types,
     * aperture shapes, and lens distortion models. It allows configuration of focal length, f-stop, sensor resolution,
     * pixel pitch, and more. The camera can project 3D points to the image plane, compute projected aperture area, and
     * supports both analytic and PSF-based point spread functions. All units are SI unless otherwise noted.
     *
     * @tparam TSpectral The spectral type (e.g., float, Vec3f, etc.)
     */
    template <IsSpectral TSpectral>
    class CameraModel : public SceneObject<CameraModel<TSpectral>, TSpectral> {
    public:
        CameraModel();

        CameraModel(const CameraModel&) = delete;
        CameraModel& operator=(const CameraModel&) = delete;

        void set_focal_length(units::Millimeter focal_length);
        float focal_length() const { return focal_length_; }

        void set_fstop(float fstop);
        float fstop() const;

        template <IsDistortion TDistortion, typename... Args>
        void set_distortion(Args&&... args);

        void set_brown_conrady_distortion(BrownCoefficients coeffs);
        void set_opencv_distortion(OpenCVCoefficients coeffs);
        void set_owen_distortion(OwenCoefficients coeffs);

        void delete_distortion() { distortion_ = nullptr; }

        template <IsSensor TSensor, typename... Args>
        void set_sensor(Args&&... args);

        void set_sensor_resolution(Resolution resolution);
        void set_sensor_resolution(int width, int height);

        void set_sensor_pixel_pitch(units::Micrometer pitch_x, units::Micrometer pitch_y);
        void set_sensor_pixel_pitch(units::Micrometer pitch);

        void set_sensor_size(units::Millimeter width, units::Millimeter height);
        void set_sensor_size(units::Millimeter width);

        Rotation<double> sensor_rotation() const;

        template <IsAperture TAperture, typename... Args>
        void set_aperture(Args&&... args);

        template <IsPSF TPSF, typename... Args>
        void set_psf(Args&&... args);

        void use_aperture_psf(int radius = 64, int banks = 16);
        void delete_psf();

        bool has_psf() const { return psf_ != nullptr; }
        const Image<TSpectral>& get_psf_kernel(float u, float v) const { return psf_->get_kernel(u, v); }
        int get_psf_radius() const { return psf_->get_radius(); }

        Pixel project_point(const Vec3<float>& point_camera_coords) const;

        void readout(FrameBuffer<TSpectral>& fb, float exposure_time) const { sensor_->readout(fb, exposure_time); }

        float get_projected_aperture_area(const Vec3<float>& direction) const;

        Resolution resolution() const { return sensor_->resolution(); }

        std::uint64_t id() const override { return id_; }
        std::string type() const override { return "CameraModel"; }

        FrameBuffer<TSpectral> make_frame_buffer() const { return FrameBuffer<TSpectral>(resolution()); }

        void use_blender_convention(bool value = true) { blender_convention_ = value; }
        bool is_blender_convention() const { return blender_convention_; }

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

        bool blender_convention_ = false;

        friend class CameraModelHandle<TSpectral>;
    };
}

#include "huira_impl/cameras/camera_model.ipp"
