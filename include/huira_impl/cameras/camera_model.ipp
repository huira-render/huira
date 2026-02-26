
#include <limits>
#include <memory>

#include "huira/cameras/apertures/circular_aperture.hpp"
#include "huira/cameras/sensors/simple_sensor.hpp"

namespace huira {
    /**
     * @brief Construct a new CameraModel with default sensor and aperture.
     *
     * Initializes the camera with a default focal length, a SimpleSensor, and a CircularAperture.
     * The aperture diameter is set based on the focal length and a default f-stop of 2.8.
     */
    template <IsSpectral TSpectral>
    CameraModel<TSpectral>::CameraModel() : id_(next_id_++)
    {
        units::Meter diameter(this->focal_length_ / 2.8f);
        this->sensor_ = std::make_unique<SimpleSensor<TSpectral>>();
        this->aperture_ = std::make_unique<CircularAperture<TSpectral>>(diameter);
        compute_intrinsics_();
    }


    /**
     * @brief Set the focal length of the camera (in millimeters).
     *
     * Updates the camera intrinsics and, if using aperture PSF, updates the PSF as well.
     * @param focal_length Focal length in millimeters
     */
    template <IsSpectral TSpectral>
    void CameraModel<TSpectral>::set_focal_length(units::Millimeter focal_length)
    {
        focal_length_ = static_cast<float>(focal_length.to_si());
        compute_intrinsics_();
        if (use_aperture_psf_) {
            units::Meter f(focal_length_);
            units::Meter px(sensor_->pixel_pitch().x);
            units::Meter py(sensor_->pixel_pitch().y);
            psf_ = aperture_->make_psf(f, px, py, psf_->get_radius(), psf_->get_banks());
        }
    }


    /**
     * @brief Set the distortion model for the camera.
     *
     * @tparam TDistortion Distortion model type
     * @tparam Args Constructor arguments for the distortion model
     * @param args Arguments to construct the distortion model
     */
    template <IsSpectral TSpectral>
    template <IsDistortion TDistortion, typename... Args>
    void CameraModel<TSpectral>::set_distortion(Args&&... args)
    {
        distortion_ = std::make_unique<TDistortion>(std::forward<Args>(args)...);
        compute_distortion_field_();
        compute_visibility_cone_();
    }


    /**
     * @brief Set Brown-Conrady distortion coefficients.
     * @param coeffs Brown distortion coefficients
     */
    template <IsSpectral TSpectral>
    void CameraModel<TSpectral>::set_brown_conrady_distortion(BrownCoefficients coeffs)
    {
        this->set_distortion<BrownDistortion<TSpectral>>(coeffs);
    }


    /**
     * @brief Set OpenCV distortion coefficients.
     * @param coeffs OpenCV distortion coefficients
     */
    template <IsSpectral TSpectral>
    void CameraModel<TSpectral>::set_opencv_distortion(OpenCVCoefficients coeffs)
    {
        this->set_distortion<OpenCVDistortion<TSpectral>>(coeffs);
    }


    /**
     * @brief Set Owen distortion coefficients.
     * @param coeffs Owen distortion coefficients
     */
    template <IsSpectral TSpectral>
    void CameraModel<TSpectral>::set_owen_distortion(OwenCoefficients coeffs)
    {
        this->set_distortion<OwenDistortion<TSpectral>>(coeffs);
    }


    /**
     * @brief Set the sensor model for the camera.
     *
     * @tparam TSensor Sensor model type
     * @tparam Args Constructor arguments for the sensor
     * @param args Arguments to construct the sensor
     */
    template <IsSpectral TSpectral>
    template <IsSensor TSensor, typename... Args>
    void CameraModel<TSpectral>::set_sensor(Args&&... args) {
        sensor_ = std::make_unique<TSensor>(std::forward<Args>(args)...);
        compute_intrinsics_();
    }


    /**
     * @brief Set the sensor resolution.
     * @param resolution Sensor resolution (width, height)
     */
    template <IsSpectral TSpectral>
    void CameraModel<TSpectral>::set_sensor_resolution(Resolution resolution)
    {
        sensor_->config_.resolution = resolution;
        compute_intrinsics_();
    }


    /**
     * @brief Set the sensor resolution by width and height.
     * @param width Sensor width in pixels
     * @param height Sensor height in pixels
     */
    template <IsSpectral TSpectral>
    void CameraModel<TSpectral>::set_sensor_resolution(int width, int height)
    {
        this->set_sensor_resolution(Resolution{ width, height });
    }


    /**
     * @brief Set the sensor pixel pitch in x and y directions.
     * @param pitch_x Pixel pitch in x (micrometers)
     * @param pitch_y Pixel pitch in y (micrometers)
     */
    template <IsSpectral TSpectral>
    void CameraModel<TSpectral>::set_sensor_pixel_pitch(units::Micrometer pitch_x, units::Micrometer pitch_y)
    {
        sensor_->set_pixel_pitch(pitch_x, pitch_y);
        compute_intrinsics_();
    }


    /**
     * @brief Set the sensor pixel pitch (square pixels).
     * @param pitch Pixel pitch in both x and y (micrometers)
     */
    template <IsSpectral TSpectral>
    void CameraModel<TSpectral>::set_sensor_pixel_pitch(units::Micrometer pitch)
    {
        sensor_->set_pixel_pitch(pitch, pitch);
        compute_intrinsics_();
    }


    /**
     * @brief Set the physical sensor size.
     * @param width Sensor width in millimeters
     * @param height Sensor height in millimeters
     */
    template <IsSpectral TSpectral>
    void CameraModel<TSpectral>::set_sensor_size(units::Millimeter width, units::Millimeter height)
    {
        sensor_->set_sensor_size(width, height);
        compute_intrinsics_();
    }


    /**
     * @brief Set the sensor width and compute height from aspect ratio.
     * @param width Sensor width in millimeters
     */
    template <IsSpectral TSpectral>
    void CameraModel<TSpectral>::set_sensor_size(units::Millimeter width)
    {
        float aspect_ratio = static_cast<float>(sensor_->resolution().y) / static_cast<float>(sensor_->resolution().x);
        units::Millimeter height = width * aspect_ratio;
        sensor_->set_sensor_size(width, height);
        compute_intrinsics_();
    }


    /**
     * @brief Get the sensor rotation as a Rotation object.
     * @return Rotation<double> Sensor rotation
     */
    template <IsSpectral TSpectral>
    Rotation<double> CameraModel<TSpectral>::sensor_rotation() const
    {
        Mat3<double> rot_matrix = Rotation<double>::local_to_parent_z(sensor_->config_.rotation);
        return Rotation<double>::from_local_to_parent(rot_matrix);
    }



    /**
     * @brief Set the aperture model for the camera.
     *
     * @tparam TAperture Aperture model type
     * @tparam Args Constructor arguments for the aperture
     * @param args Arguments to construct the aperture
     */
    template <IsSpectral TSpectral>
    template <IsAperture TAperture, typename... Args>
    void CameraModel<TSpectral>::set_aperture(Args&&... args) {
        aperture_ = std::make_unique<TAperture>(std::forward<Args>(args)...);
    }


    /**
     * @brief Set the point spread function (PSF) model for the camera.
     *
     * @tparam TPSF PSF model type
     * @tparam Args Constructor arguments for the PSF
     * @param args Arguments to construct the PSF
     */
    template <IsSpectral TSpectral>
    template <IsPSF TPSF, typename... Args>
    void CameraModel<TSpectral>::set_psf(Args&&... args) {
        psf_ = std::make_unique<TPSF>(std::forward<Args>(args)...);
        use_aperture_psf_ = false;
    }


    /**
     * @brief Use the aperture to generate a PSF (point spread function).
     * @param radius PSF kernel radius
     * @param banks Number of PSF banks
     */
    template <IsSpectral TSpectral>
    void CameraModel<TSpectral>::use_aperture_psf(int radius, int banks) {
        use_aperture_psf_ = true;
        units::Meter f(focal_length_);
        units::Meter px(sensor_->pixel_pitch().x);
        units::Meter py(sensor_->pixel_pitch().y);
        psf_ = aperture_->make_psf(f, px, py, radius, banks);
    }


    /**
     * @brief Delete the PSF and disable aperture PSF usage.
     */
    template <IsSpectral TSpectral>
    void CameraModel<TSpectral>::delete_psf() {
        psf_ = nullptr;
        use_aperture_psf_ = false;
    }


    /**
     * @brief Project a 3D point in camera coordinates onto the image plane.
     *
     * Uses the pinhole camera model and applies distortion if present.
     * @param point_camera_coords 3D point in camera coordinates (meters)
     * @return Pixel 2D point on the image plane (pixels)
     */
    template <IsSpectral TSpectral>
    Pixel CameraModel<TSpectral>::project_point(const Vec3<float>& point_camera_coords) const
    {
        auto NaN = std::numeric_limits<float>::quiet_NaN();

        if (!in_fov(point_camera_coords)) {
            return Pixel{ NaN, NaN };
        }

        float sign_y = 1.0f;
        float depth = point_camera_coords.z;
        if (blender_convention_) {
            depth = -depth;
            sign_y = -1.0f;
        }

        Pixel normalized{ point_camera_coords.x / depth, sign_y * point_camera_coords.y / depth };
        if (distortion_) {
            normalized = distortion_->distort(normalized);
        }
        return Pixel{ fx_ * normalized[0] + cx_, fy_ * normalized[1] + cy_ };
    }

    template <IsSpectral TSpectral>
    Ray<TSpectral> CameraModel<TSpectral>::cast_ray(const Pixel& pixel) const
    {
        if (distortion_) {
            float u = pixel[0] / static_cast<float>(rx_ - 1);
            float v = pixel[1] / static_cast<float>(ry_ - 1);
            Vec3<float> direction = distortion_field_.sample_bilinear<WrapMode::Clamp>(u, v);
            return Ray<TSpectral>{ Vec3<float>{0.f, 0.f, 0.f}, glm::normalize(direction) };
        }
        return cast_ray_(pixel);
    }

    template <IsSpectral TSpectral>
    Ray<TSpectral> CameraModel<TSpectral>::cast_ray(int x, int y) const
    {
        Pixel pixel{ static_cast<float>(x), static_cast<float>(y) };
        return cast_ray(pixel);
    }

    template <IsSpectral TSpectral>
    bool CameraModel<TSpectral>::in_fov(const Vec3<float>& point) const
    {
        float len2 = glm::dot(point, point);
        if (len2 < 1e-12f) {
            return false;
        }

        float z = blender_convention_ ? -point.z : point.z;
        return z * glm::inversesqrt(len2) >= z_cutoff_;
    }


    /**
     * @brief Get the projected aperture area for a given direction.
     * @param direction Direction vector
     * @return float Projected aperture area
     */
    template <IsSpectral TSpectral>
    float CameraModel<TSpectral>::get_projected_aperture_area(const Vec3<float>& direction) const
    {
        float cosTheta = glm::dot(glm::normalize(direction), Vec3<float>{0, 0, 1});
        return this->aperture_->get_area() * std::abs(cosTheta);
    }


    /**
     * @brief Set the f-stop (aperture ratio) of the camera.
     * @param fstop F-stop value
     */
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


    /**
     * @brief Get the f-stop (aperture ratio) of the camera.
     * @return float F-stop value
     */
    template <IsSpectral TSpectral>
    float CameraModel<TSpectral>::fstop() const
    {
        this->aperture_->get_area();
        float aperture_diameter = 2.f * std::sqrt(this->aperture_->get_area() / PI<float>());
        return focal_length_ / aperture_diameter;
    }



    /**
     * @brief Compute the camera intrinsic parameters (focal lengths, principal point, resolution).
     */
    template <IsSpectral TSpectral>
    void CameraModel<TSpectral>::compute_intrinsics_()
    {
        fx_ = focal_length_ / sensor_->pixel_pitch().x;
        fy_ = focal_length_ / sensor_->pixel_pitch().y;
        cx_ = static_cast<float>(sensor_->resolution().x) * 0.5f;
        cy_ = static_cast<float>(sensor_->resolution().y) * 0.5f;
        rx_ = static_cast<float>(sensor_->resolution().x);
        ry_ = static_cast<float>(sensor_->resolution().y);

        compute_visibility_cone_();
    }

    template <IsSpectral TSpectral>
    Ray<TSpectral> CameraModel<TSpectral>::cast_ray_(const Pixel& pixel) const
    {
        // Invert the intrinsic matrix
        float nx = (pixel[0] - cx_) / fx_;
        float ny = (pixel[1] - cy_) / fy_;

        Pixel normalized{ nx, ny };

        // Undistort
        if (distortion_) {
            normalized = distortion_->undistort(normalized);
        }

        // Build direction in camera coordinates
        float dir_x = normalized[0];
        float dir_y = normalized[1];

        Vec3<float> origin{ 0.f, 0.f, 0.f };
        Vec3<float> direction;

        if (blender_convention_) {
            direction = Vec3<float>{ dir_x, -dir_y, -1.0f };
        }
        else {
            direction = Vec3<float>{ dir_x, dir_y, 1.0f };
        }

        return Ray<TSpectral>{ origin, glm::normalize(direction) };
    }

    template <IsSpectral TSpectral>
    void CameraModel<TSpectral>::compute_distortion_field_()
    {
        Resolution res = sensor_->resolution();

        distortion_field_ = Image<Vec3<float>>(res, Vec3<float>{0, 0, 0});

        for (int x = 0; x < res.x; ++x) {
            for (int y = 0; y < res.y; ++y) {
                Pixel pixel{ static_cast<float>(x), static_cast<float>(y) };
                Ray<TSpectral> ray = cast_ray_(pixel);
                distortion_field_(x, y) = ray.direction();
            }
        }
    }

    template <IsSpectral TSpectral>
    void CameraModel<TSpectral>::compute_visibility_cone_()
    {
        Resolution res = sensor_->resolution();

        Vec3<float> zdir{ 0, 0, 1 };
        if (blender_convention_) {
            zdir = Vec3<float>{ 0, 0, -1 };
        }

        float min_dot = 1.f;

        auto update = [&](int i, int j) {
            Ray<TSpectral> ray = cast_ray(i, j);
            Vec3<float> direction = ray.direction();
            min_dot = std::min(min_dot, glm::dot(direction, zdir));
            };

        for (int i = 0; i < res.x; ++i) {
            update(i, 0);
            update(i, res.y - 1);
        }
        for (int j = 1; j < res.y - 1; ++j) {
            update(0, j);
            update(res.x - 1, j);
        }

        visibility_cone_ = std::acos(std::clamp(min_dot, -1.f, 1.f));
        z_cutoff_ = std::cos(visibility_cone_);
    }
}

