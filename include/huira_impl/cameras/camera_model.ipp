#include <memory>
#include <limits>
#include "huira/cameras/sensors/simple_sensor.hpp"
#include "huira/cameras/aperture/circular_aperture.hpp"

namespace huira {
    template <IsSpectral TSpectral>
    CameraModel<TSpectral>::CameraModel() : id_(next_id_++)
    {
        units::Meter diameter(this->focal_length_ / 2.8f);

        this->sensor_ = std::make_unique<SimpleSensor<TSpectral>>();
        this->aperture_ = std::make_unique<CircularAperture<TSpectral>>(diameter);

        compute_intrinsics_();
    }

    template <IsSpectral TSpectral>
    void CameraModel<TSpectral>::set_focal_length(float focal_length)
    {
        focal_length_ = focal_length;
        compute_intrinsics_();

        if (use_aperture_psf_) {
            units::Meter f(focal_length_);
            units::Meter px(sensor_->pixel_pitch().x);
            units::Meter py(sensor_->pixel_pitch().y);
            psf_ = aperture_->make_psf(f, px, py, psf_->get_radius(), psf_->get_banks());
        }
    }

    template <IsSpectral TSpectral>
    template <IsDistortion TDistortion, typename... Args>
    void CameraModel<TSpectral>::set_distortion(Args&&... args)
    {
        distortion_ = std::make_unique<TDistortion>(std::forward<Args>(args)...);
    }

    template <IsSpectral TSpectral>
    void CameraModel<TSpectral>::set_brown_conrady_distortion(BrownCoefficients coeffs)
    {
        this->set_distortion<BrownDistortion<TSpectral>>(coeffs);
    }

    template <IsSpectral TSpectral>
    void CameraModel<TSpectral>::set_opencv_distortion(OpenCVCoefficients coeffs)
    {
        this->set_distortion<OpenCVDistortion<TSpectral>>(coeffs);
    }

    template <IsSpectral TSpectral>
    void CameraModel<TSpectral>::set_owen_distortion(OwenCoefficients coeffs)
    {
        this->set_distortion<OwenDistortion<TSpectral>>(coeffs);
    }

    template <IsSpectral TSpectral>
    template <IsSensor TSensor, typename... Args>
    void CameraModel<TSpectral>::set_sensor(Args&&... args) {
        sensor_ = std::make_unique<TSensor>(std::forward<Args>(args)...);
        compute_intrinsics_();
    }

    template <IsSpectral TSpectral>
    void CameraModel<TSpectral>::set_sensor_resolution(Resolution resolution)
    {
        sensor_->config_.resolution = resolution;
        compute_intrinsics_();
    }

    template <IsSpectral TSpectral>
    void CameraModel<TSpectral>::set_sensor_resolution(int width, int height)
    {
        this->set_sensor_resolution(Resolution{ width, height });
    }

    template <IsSpectral TSpectral>
    void CameraModel<TSpectral>::set_sensor_pixel_pitch(Vec2<float> pixel_pitch)
    {
        sensor_->config_.pixel_pitch = pixel_pitch;
        compute_intrinsics_();
    }

    template <IsSpectral TSpectral>
    void CameraModel<TSpectral>::set_sensor_pixel_pitch(float pixel_pitch_x, float pixel_pitch_y)
    {
        this->set_sensor_pixel_pitch(Vec2<float>{ pixel_pitch_x, pixel_pitch_y });
    }

    template <IsSpectral TSpectral>
    void CameraModel<TSpectral>::set_sensor_pixel_pitch(float pixel_pitch)
    {
        this->set_sensor_pixel_pitch(Vec2<float>{ pixel_pitch, pixel_pitch });
    }

    template <IsSpectral TSpectral>
    void CameraModel<TSpectral>::set_sensor_size(Vec2<float> size)
    {
        Vec2<float> pixel_pitch = size / sensor_->resolution();
        this->set_sensor_pixel_pitch(pixel_pitch);
    }

    template <IsSpectral TSpectral>
    void CameraModel<TSpectral>::set_sensor_size(float width, float height)
    {
        Vec2<float> pixel_pitch = Vec2<float>{ width, height } / sensor_->resolution();
        this->set_sensor_pixel_pitch(pixel_pitch);
    }

    template <IsSpectral TSpectral>
    void CameraModel<TSpectral>::set_sensor_size(float width)
    {
        float height = width * static_cast<float>(sensor_->resolution().y) / static_cast<float>(sensor_->resolution().x);
        Vec2<float> pixel_pitch = Vec2<float>{ width, height } / sensor_->resolution();
        this->set_sensor_pixel_pitch(pixel_pitch);
    }

    template <IsSpectral TSpectral>
    Rotation<double> CameraModel<TSpectral>::sensor_rotation() const
    {
        Mat3<double> rot_matrix = Rotation<double>::local_to_parent_z(sensor_->config_.rotation);
        return Rotation<double>::from_local_to_parent(rot_matrix);
    }


    template <IsSpectral TSpectral>
    template <IsAperture TAperture, typename... Args>
    void CameraModel<TSpectral>::set_aperture(Args&&... args) {
        aperture_ = std::make_unique<TAperture>(std::forward<Args>(args)...);
    }

    template <IsSpectral TSpectral>
    template <IsPSF TPSF, typename... Args>
    void CameraModel<TSpectral>::set_psf(Args&&... args) {
        psf_ = std::make_unique<TPSF>(std::forward<Args>(args)...);
        use_aperture_psf_ = false;
    }

    template <IsSpectral TSpectral>
    void CameraModel<TSpectral>::use_aperture_psf(int radius, int banks) {
        use_aperture_psf_ = true;

        units::Meter f(focal_length_);
        units::Meter px(sensor_->pixel_pitch().x);
        units::Meter py(sensor_->pixel_pitch().y);
        psf_ = aperture_->make_psf(f, px, py, radius, banks);
    }

    template <IsSpectral TSpectral>
    void CameraModel<TSpectral>::delete_psf() {
        psf_ = nullptr;
        use_aperture_psf_ = false;
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


        float depth = z;
        float sign_y = 1.0f;
        if (blender_convention_) {
            depth = -z;
            sign_y = -1.0f;
        }


        if (depth < kEpsilon) {
            return Pixel{ NaN, NaN };  // behind camera
        }
        
        // Compute normalized image coordinates:
        Pixel normalized{ x / depth, sign_y * y / depth };
        if (distortion_) {
            normalized = distortion_->distort(normalized);
        }

        float px = fx_ * normalized[0] + cx_;
        float py = fy_ * normalized[1] + cy_;
        return Pixel{ px, py };
    }

    template <IsSpectral TSpectral>
    float CameraModel<TSpectral>::get_projected_aperture_area(const Vec3<float>& direction) const
    {
        float cosTheta = glm::dot(glm::normalize(direction), Vec3<float>{0, 0, 1});
        return this->aperture_->get_area() * std::abs(cosTheta);
    }

    template <IsSpectral TSpectral>
    void CameraModel<TSpectral>::set_fstop(float fstop)
    {
        units::Meter aperture_diameter(focal_length_ / fstop);
        units::SquareMeter aperture_area = PI<float>() * (aperture_diameter * aperture_diameter) / 4.f;

        this->aperture_->set_area(aperture_area);

        if (use_aperture_psf_) {
            units::Meter f(focal_length_);
            units::Meter px(sensor_->pixel_pitch().x);
            units::Meter py(sensor_->pixel_pitch().y);
            psf_ = aperture_->make_psf(f, px, py, psf_->get_radius(), psf_->get_banks());
        }
    }

    template <IsSpectral TSpectral>
    float CameraModel<TSpectral>::fstop() const
    {
        this->aperture_->get_area();
        float aperture_diameter = 2.f * std::sqrt(this->aperture_->get_area() / PI<float>());

        return focal_length_ / aperture_diameter;
    }


    template <IsSpectral TSpectral>
    void CameraModel<TSpectral>::compute_intrinsics_()
    {
        fx_ = focal_length_ / sensor_->pixel_pitch().x;
        fy_ = focal_length_ / sensor_->pixel_pitch().y;

        cx_ = static_cast<float>(sensor_->resolution().x) * 0.5f;
        cy_ = static_cast<float>(sensor_->resolution().y) * 0.5f;

        rx_ = static_cast<float>(sensor_->resolution().x);
        ry_ = static_cast<float>(sensor_->resolution().y);
    }
}

