#include <memory>
#include <limits>
#include "huira/cameras/sensors/simple_sensor.hpp"
#include "huira/cameras/aperture/circular_aperture.hpp"

namespace huira {
    template <IsSpectral TSpectral>
    CameraModel<TSpectral>::CameraModel() : id_(next_id_++)
    {
        this->sensor_ = std::make_unique<SimpleSensor<TSpectral>>(1920, 1080, .036f, .02f);
        this->aperture_ = std::make_unique<CircularAperture<TSpectral>>(.025f);
    }

    template <IsSpectral TSpectral>
    void CameraModel<TSpectral>::set_focal_length(float focal_length)
    {
        focal_length_ = focal_length;
        if (use_aperture_psf_) {
            psf_ = aperture_->make_psf(focal_length_, sensor_->pixel_pitch());
        }
    }

    template <IsSpectral TSpectral>
    template <IsDistortion TDistortion, typename... Args>
    void CameraModel<TSpectral>::set_distortion(Args&&... args)
    {
        distortion_ = std::make_unique<TDistortion>(std::forward<Args>(args)...);
    }


    /**
     * @brief Projects a 3D point in camera coordinates onto the image plane.
     * 
     * Uses the pinhole camera model: 
     *   x' = (focal_length_ * x) / z
     *   y' = (focal_length_ * y) / z
     *   z' = 1 (homogeneous coordinate, ignored here)
     * The result is in sensor plane coordinates (meters).
     * 
     * @param point_camera_coords 3D point in camera coordinates (meters)
     * @return Vec3<float> 2D point on the image plane (meters), z=0
     */
    template <IsSpectral TSpectral>
    Pixel CameraModel<TSpectral>::project_point(const Vec3<float>& point_camera_coords) const
    {
        constexpr float kEpsilon = 1e-6f;
        float x = point_camera_coords.x;
        float y = point_camera_coords.y;
        float z = point_camera_coords.z;
        auto NaN = std::numeric_limits<float>::quiet_NaN();

        // Check if point is behind camera:
        if (std::abs(z) < kEpsilon) {
            return Pixel{ NaN, NaN };
        }

        // Compute normalized image coordinates:
        Pixel normalized{ x / z, y / z };
        if (distortion_) {
            normalized = distortion_->distort(normalized);
        }

        float px = fx_ * normalized[0] + cx_;
        float py = fy_ * normalized[1] + cy_;

        // Check if point is outside the FOV:
        if (px < 0 || px >= rx_ || py < 0 || py >= ry_) {
            return Pixel{ NaN, NaN };
        }

        return Pixel{ px, py };
    }

    template <IsSpectral TSpectral>
    void CameraModel<TSpectral>::compute_intrinsics_()
    {
        fx_ = focal_length_ / sensor_->pitch_x();
        fy_ = focal_length_ / sensor_->pitch_y();

        cx_ = static_cast<float>(sensor_->res_x()) * 0.5f;
        cy_ = static_cast<float>(sensor_->res_y()) * 0.5f;

        rx_ = static_cast<float>(sensor_->res_x());
        ry_ = static_cast<float>(sensor_->res_y());
    }
}

