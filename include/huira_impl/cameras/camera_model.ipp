#include <memory>
#include <limits>
#include "huira/cameras/sensors/simple_sensor.hpp"
#include "huira/cameras/aperture/circular_aperture.hpp"

namespace huira {
    template <IsSpectral TSpectral>
    CameraModel<TSpectral>::CameraModel() : id_(next_id_++)
    {
        this->sensor_ = std::make_unique<SimpleSensor<TSpectral>>(1920, 1080, static_cast<float>(.036), static_cast<float>(.02));
        this->aperture_ = std::make_unique<CircularAperture>(static_cast<float>(.025));
    }

    template <IsSpectral TSpectral>
    void CameraModel<TSpectral>::set_focal_length(double focal_length)
    {
        focal_length_ = focal_length;
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

        // Sensor Resolution:
        float rx = static_cast<float>(this->res_x());
        float ry = static_cast<float>(this->res_y());

        // Sensor Size:
        float sx = this->sensor_->width();
        float sy = this->sensor_->height();

        float f = static_cast<float>(focal_length_);

        // Project to normalized image plane (meters)
        float x_proj = (f * x) / z;
        float y_proj = (f * y) / z;

        // Convert to pixel coordinates
        // Sensor center is at (sx/2, sy/2) in meters, (rx/2, ry/2) in pixels
        float px = (x_proj + sx * 0.5f) * (rx / sx);
        float py = (y_proj + sy * 0.5f) * (ry / sy);

        // Check if point is outside the FOV:
        if (px < 0 || px >= rx || py < 0 || py >= ry) {
            return Pixel{ NaN, NaN };
        }

        return Pixel{ px, py };
    }
}

