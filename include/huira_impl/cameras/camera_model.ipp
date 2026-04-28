
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
        HUIRA_TRACE_SCOPE("CameraModel::CameraModel()");
        units::Meter diameter(this->focal_length_ / 2.8f);
        this->sensor_ = std::make_unique<SimpleSensor<TSpectral>>();
        this->aperture_ = std::make_unique<CircularAperture<TSpectral>>(diameter);
        cx_ = static_cast<float>(sensor_->resolution().x) * 0.5f;
        cy_ = static_cast<float>(sensor_->resolution().y) * 0.5f;
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
        is_explicit_matrix_ = false;
        focal_length_ = focal_length.to_si_f();

        if (focal_length_ <= 0 || std::isinf(focal_length_) || std::isnan(focal_length_)) {
            HUIRA_THROW_ERROR("CameraModel::set_focal_length - Focal length must be a positive finite value: " + std::to_string(focal_length_));
        }

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
    template <IsDistortion<TSpectral> TDistortion, typename... Args>
    void CameraModel<TSpectral>::set_distortion(Args&&... args)
    {
        distortion_ = std::make_unique<TDistortion>(std::forward<Args>(args)...);
        compute_distortion_field_();
        compute_frustum_();
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
    template <IsSensor<TSpectral> TSensor, typename... Args>
    void CameraModel<TSpectral>::set_sensor(Args&&... args) {
        sensor_ = std::make_unique<TSensor>(std::forward<Args>(args)...);
        compute_intrinsics_();
    }

    /**
     * @brief Configure the sensor using pixel pitch and resolution.
     *
     * This method sets the sensor resolution, pixel pitch, and principal point. It also computes the intrinsics based on the new configuration.
     * @param resolution Sensor resolution
     * @param pitch_x Pixel pitch in x direction (micrometers)
     * @param pitch_y Pixel pitch in y direction (micrometers)
     * @param cx Principal point x coordinate (must be within resolution bounds)
     * @param cy Principal point y coordinate (must be within resolution bounds)
     */
    template <IsSpectral TSpectral>
    void CameraModel<TSpectral>::configure_sensor_from_pitch(
        const Resolution& resolution,
        units::Micrometer pitch_x, std::optional<units::Micrometer> pitch_y,
        std::optional<float> cx, std::optional<float> cy)
    {
        is_explicit_matrix_ = false;

        sensor_->set_resolution(resolution);

        if (!pitch_y.has_value()) {
            pitch_y = pitch_x;
        }
        sensor_->set_pixel_pitch(pitch_x, pitch_y.value());

        float final_cx = cx.value_or(static_cast<float>(resolution.x) * 0.5f);
        float final_cy = cy.value_or(static_cast<float>(resolution.y) * 0.5f);

        if (std::isnan(final_cx) || std::isnan(final_cy) || std::isinf(final_cx) || std::isinf(final_cy)) {
            HUIRA_THROW_ERROR("CameraModel - Principal point (cx, cy) must be finite numeric values.");
        }

        if (final_cx < -static_cast<float>(resolution.x) || final_cx > static_cast<float>(resolution.x) * 2.0f ||
            final_cy < -static_cast<float>(resolution.y) || final_cy > static_cast<float>(resolution.y) * 2.0f) {
            HUIRA_LOG_WARNING("Principal point is significantly outside the sensor resolution. Ensure this intended for an off-axis projection.");
        }

        cx_ = final_cx;
        cy_ = final_cy;

        compute_intrinsics_();
    }

    /**
     * @brief Configure the sensor using physical size and resolution.
     *
     * This method sets the sensor resolution, physical size, and principal point. It also computes the intrinsics based on the new configuration.
     * @param resolution Sensor resolution
     * @param width Sensor width in millimeters
     * @param height Sensor height in millimeters
     * @param cx Principal point x coordinate (must be within resolution bounds)
     * @param cy Principal point y coordinate (must be within resolution bounds)
     */
    template <IsSpectral TSpectral>
    void CameraModel<TSpectral>::configure_sensor_from_size(
        const Resolution& resolution,
        units::Millimeter width, std::optional<units::Millimeter> height,
        std::optional<float> cx, std::optional<float> cy)
    {
        is_explicit_matrix_ = false;

        sensor_->set_resolution(resolution);

        if (height.has_value()) {
            sensor_->set_sensor_size(width, height.value());
        }
        else {
            // If height is not provided, assume square pixels and compute height from width and resolution
            float pixel_size_x = width.to_si_f() / static_cast<float>(resolution.x);
            float pixel_size_y = pixel_size_x; // Square pixels
            sensor_->set_pixel_pitch(units::Meter(pixel_size_x), units::Meter(pixel_size_y));
        }

        float final_cx = cx.value_or(static_cast<float>(resolution.x) * 0.5f);
        float final_cy = cy.value_or(static_cast<float>(resolution.y) * 0.5f);

        if (std::isnan(final_cx) || std::isnan(final_cy) || std::isinf(final_cx) || std::isinf(final_cy)) {
            HUIRA_THROW_ERROR("CameraModel - Principal point (cx, cy) must be finite numeric values.");
        }

        if (final_cx < -static_cast<float>(resolution.x) || final_cx > static_cast<float>(resolution.x) * 2.0f ||
            final_cy < -static_cast<float>(resolution.y) || final_cy > static_cast<float>(resolution.y) * 2.0f) {
            HUIRA_LOG_WARNING("Principal point is significantly outside the sensor resolution. Ensure this intended for an off-axis projection.");
        }

        cx_ = final_cx;
        cy_ = final_cy;

        compute_intrinsics_();
    }

    /**
     * @brief Set the intrinsic matrix for the camera.
     * @param intrinsic_matrix 3x3 intrinsic matrix
     * @param resolution Sensor resolution
     * @param anchor_focal_length Anchor focal length in millimeters
     */
    template <IsSpectral TSpectral>
    void CameraModel<TSpectral>::set_intrinsic_matrix(
        const Mat3<float>& intrinsic_matrix,
        const Resolution& resolution,
        units::Millimeter anchor_focal_length)
    {
        this->set_intrinsics(intrinsic_matrix[0][0], intrinsic_matrix[1][1],
            intrinsic_matrix[0][2], intrinsic_matrix[1][2],
            resolution, anchor_focal_length);
    }

    /**
     * @brief Set the intrinsic parameters for the camera.
     * @param fx Focal length in x direction
     * @param fy Focal length in y direction
     * @param cx Principal point x coordinate
     * @param cy Principal point y coordinate
     * @param resolution Sensor resolution
     * @param anchor_focal_length Anchor focal length in millimeters
     */
    template <IsSpectral TSpectral>
    void CameraModel<TSpectral>::set_intrinsics(
        float fx, float fy, float cx, float cy,
        const Resolution& resolution,
        units::Millimeter anchor_focal_length)
    {
        is_explicit_matrix_ = true;
        
        fx_ = fx;
        fy_ = fy;
        cx_ = cx;
        cy_ = cy;
        sensor_->set_resolution(resolution);   
        focal_length_ = anchor_focal_length.to_si_f();

        // Use the anchor to compute the pixel_pitch/size
        units::Meter px(focal_length_ / fx_);
        units::Meter py(focal_length_ / fy_);
        sensor_->set_pixel_pitch(px, py);
        sensor_->set_sensor_size(px * resolution.x, py * resolution.y);

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
     * @brief Set the veiling glare alpha value.
     * @param alpha Veiling glare alpha (0 to 1)
     */
    template <IsSpectral TSpectral>
    void CameraModel<TSpectral>::set_veiling_glare(float alpha) {
        if (alpha < 0.f || alpha > 1.f || std::isnan(alpha)) {
            HUIRA_THROW_ERROR("CameraModel::set_veiling_glare - Alpha must be in the range [0, 1]: " + std::to_string(alpha));
        }
        veiling_alpha_ = alpha;
        veiling_glare_enabled_ = (alpha > 0.f);
    }
    
    /**
     * @brief Disable veiling glare effects.
     */
    template <IsSpectral TSpectral>
    void CameraModel<TSpectral>::disable_veiling_glare() {
        veiling_alpha_ = 0.f;
        veiling_glare_enabled_ = false;
    }

    /**
     * @brief Set Harvey-Shack scatter parameters.
     * @param scatter_fraction Fraction of light scattered (0 to 1)
     * @param falloff_exponent Exponent for scatter falloff (typically > 1)
     * @param r0 Radius at which scatter fraction is measured (default 0.5)
     * @param radius Maximum scatter radius in pixels (default 0, meaning infinite)
     */
    template <IsSpectral TSpectral>
    void CameraModel<TSpectral>::set_harvey_shack_scatter(float scatter_fraction, float falloff_exponent, float r0, float radius) {
        scatter_fraction_ = scatter_fraction;
        scatter_falloff_exponent_ = falloff_exponent;
        r0_ = r0;
        scatter_radius_ = radius;
        scatter_enabled_ = true;
    }

    /**
     * @brief Disable Harvey-Shack scatter effects.
     */
    template <IsSpectral TSpectral>
    void CameraModel<TSpectral>::disable_harvey_shack_scatter() {
        scatter_fraction_ = 0.f;
        scatter_falloff_exponent_ = 2.f;
        r0_ = 0.5f;
        scatter_radius_ = 0.f;
        scatter_enabled_ = false;
    }

    /**
     * @brief Set the focus distance for depth of field calculations.
     * @param focus_distance Focus distance in meters
     */
    template <IsSpectral TSpectral>
    void CameraModel<TSpectral>::set_focus_distance(units::Meter focus_distance)
    {
        float focus_distance_ = focus_distance.to_si_f();
        if (std::isnan(focus_distance_)) {
            HUIRA_THROW_ERROR("CameraModel::set_focus_distance - Focus distance cannot be NaN");
        }
        if (std::abs(focus_distance_) < 1e-12f) {
            HUIRA_THROW_ERROR("CameraModel::set_focus_distance - Focus distance is too small");
        }
        d_ = focus_distance_;

        units::Meter pitch_x(sensor_->pixel_pitch().x);
        units::Meter pitch_y(sensor_->pixel_pitch().y);
        this->aperture_->build_defocus_kernel(this->get_diopters(), this->focal_length(), pitch_x, pitch_y, 16);
    }

    /**
     * @brief Set the focus distance using diopters.
      *
      * Converts the given diopter value to a focus distance in meters and forwards
      * it to set_focus_distance().
      *
      * @param diopters Focus distance expressed in diopters
     */
    template <IsSpectral TSpectral>
    void CameraModel<TSpectral>::set_diopters(units::Diopter diopters)
    {
        units::Meter focus_distance;
        if (std::abs(diopters.to_si_f()) < 1e-12f) {
            focus_distance = units::Meter(std::numeric_limits<float>::infinity());
        }
        else {
            focus_distance = 1.f / diopters;
        }
        set_focus_distance(focus_distance);
    }

    /**
     * @brief Get the current focus distance in diopters.
     * @return units::Diopter Focus distance expressed in diopters
     */
    template <IsSpectral TSpectral>
    units::Diopter CameraModel<TSpectral>::get_diopters() const
    {
        if (std::isinf(d_)) {
            return units::Diopter(0.f);
        }
        return units::Diopter(1.f / d_);
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
    Pixel CameraModel<TSpectral>::try_project_point(const Vec3<float>& point_camera_coords) const
    {
        auto NaN = std::numeric_limits<float>::quiet_NaN();
        if (!in_fov(point_camera_coords)) {
            return Pixel{ NaN, NaN };
        }

        return project_point(point_camera_coords);
    }

    template <IsSpectral TSpectral>
    Ray<TSpectral> CameraModel<TSpectral>::cast_ray(const Pixel& pixel, Sampler<float>& sampler) const
    {
        assert(pixel[0] >= 0 && pixel[0] < rx_ && pixel[1] >= 0 && pixel[1] < ry_);

        Vec3<float> origin{ 0,0,0 };
        Vec3<float> direction{ 0,0,1 };
        if (distortion_) {
            float u = pixel[0] / static_cast<float>(rx_ - 1);
            float v = pixel[1] / static_cast<float>(ry_ - 1);
            direction = distortion_field_.sample_bilinear<WrapMode::Clamp>(u, v);
        }
        else {
            direction = pixel_to_direction_<float>(pixel);
        }

        if (depth_of_field_) {
            Vec2<float> aperture_sample = aperture_->sample(sampler);
            Vec3<float> aperture_point{ aperture_sample.x, aperture_sample.y, 0.f };

            if (!std::isinf(d_)) {
                Vec3<float> focal_point = direction * d_;
                direction = focal_point - aperture_point;
            }
            origin = aperture_point;
        }

        return Ray<TSpectral>{ origin, glm::normalize(direction) };
    }

    template <IsSpectral TSpectral>
    Ray<TSpectral> CameraModel<TSpectral>::cast_ray(const Pixel& pixel) const
    {
        assert(pixel[0] >= 0 && pixel[0] < rx_ && pixel[1] >= 0 && pixel[1] < ry_);

        Vec3<float> origin{ 0,0,0 };
        Vec3<float> direction{ 0,0,1 };
        if (distortion_) {
            float u = pixel[0] / static_cast<float>(rx_ - 1);
            float v = pixel[1] / static_cast<float>(ry_ - 1);
            direction = distortion_field_.sample_bilinear<WrapMode::Clamp>(u, v);
        }
        else {
            direction = pixel_to_direction_<float>(pixel);
        }

        return Ray<TSpectral>{ origin, glm::normalize(direction) };
    }

    template <IsSpectral TSpectral>
    Ray<TSpectral> CameraModel<TSpectral>::cast_ray(int x, int y) const
    {
        Pixel pixel{ static_cast<float>(x), static_cast<float>(y) };
        return cast_ray(pixel);
    }

    template <IsSpectral TSpectral>
    float CameraModel<TSpectral>::pixel_radiance_to_power(int x, int y) const
    {
        Ray<TSpectral> ray = cast_ray(x, y);
        return pixel_solid_angles_(x, y) * this->get_projected_aperture_area(ray.direction());
    }

    template <IsSpectral TSpectral>
    bool CameraModel<TSpectral>::in_fov(const Vec3<float>& point) const
    {
        float len2 = glm::dot(point, point);
        if (len2 < 1e-12f) {
            return false;
        }
        return view_frustum_.contains(point);
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
        return this->aperture_->get_area().to_si_f() * std::abs(cosTheta);
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
        float area = this->aperture_->get_area().to_si_f();
        float aperture_diameter = 2.f * std::sqrt(area / PI<float>());
        return focal_length_ / aperture_diameter;
    }



    /**
     * @brief Compute the camera intrinsic parameters (focal lengths, principal point, resolution).
     */
    template <IsSpectral TSpectral>
    void CameraModel<TSpectral>::compute_intrinsics_()
    {
        rx_ = static_cast<float>(sensor_->resolution().x);
        ry_ = static_cast<float>(sensor_->resolution().y);

        if (!is_explicit_matrix_) {
            fx_ = focal_length_ / sensor_->pixel_pitch().x;
            fy_ = focal_length_ / sensor_->pixel_pitch().y;
        }

        compute_frustum_();
        compute_pixel_solid_angles_();
    }

    template <IsSpectral TSpectral>
    template <IsFloatingPoint TFloat>
    Vec3<TFloat> CameraModel<TSpectral>::pixel_to_direction_(const Pixel& pixel) const
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
        TFloat dir_x = static_cast<TFloat>(normalized[0]);
        TFloat dir_y = static_cast<TFloat>(normalized[1]);
        
        Vec3<TFloat> direction;
        if (blender_convention_) {
            direction = Vec3<TFloat>{ dir_x, -dir_y, static_cast<TFloat>(-1)};
        }
        else {
            direction = Vec3<TFloat>{ dir_x, dir_y, static_cast<TFloat>(1) };
        }

        return direction;
    }

    template <IsSpectral TSpectral>
    void CameraModel<TSpectral>::compute_distortion_field_()
    {
        Resolution res = sensor_->resolution();

        distortion_field_ = Image<Vec3<float>>(res, Vec3<float>{0, 0, 0});

        for (int x = 0; x < res.x; ++x) {
            for (int y = 0; y < res.y; ++y) {
                Pixel pixel{ static_cast<float>(x), static_cast<float>(y) };
                distortion_field_(x, y) = glm::normalize(pixel_to_direction_<float>(pixel));
            }
        }
    }

    template <IsSpectral TSpectral>
    void CameraModel<TSpectral>::compute_pixel_solid_angles_()
    {
        Resolution res = sensor_->resolution();

        pixel_solid_angles_ = Image<float>(res, 0.f);
        for (int x = 0; x < res.x; ++x) {
            for (int y = 0; y < res.y; ++y) {
                // Calculate normalized directions to pixel corners
                Vec3<double> c0 = glm::normalize(pixel_to_direction_<double>(Pixel{static_cast<float>(x),     static_cast<float>(y)}));
                Vec3<double> c1 = glm::normalize(pixel_to_direction_<double>(Pixel{static_cast<float>(x + 1), static_cast<float>(y)}));
                Vec3<double> c2 = glm::normalize(pixel_to_direction_<double>(Pixel{static_cast<float>(x + 1), static_cast<float>(y + 1)}));
                Vec3<double> c3 = glm::normalize(pixel_to_direction_<double>(Pixel{static_cast<float>(x),     static_cast<float>(y + 1)}));

                // Compute solid angle as sum of two triangular areas
                double omega1 = triangle_solid_angle_(c0, c1, c2);
                double omega2 = triangle_solid_angle_(c0, c2, c3);

                pixel_solid_angles_(x, y) = static_cast<float>(omega1 + omega2);
            }
        }
    }

    template <IsSpectral TSpectral>
    Vec3<double> CameraModel<TSpectral>::tangent_(const Vec3<double>& p0, const Vec3<double>& p1) const
    {
        Vec3<double> p = p1 - p0;
        Vec3<double> r = glm::cross(p0, p);
        Vec3<double> t = glm::cross(r, p0);
        return glm::normalize(t);
    }

    template <IsSpectral TSpectral>
    double CameraModel<TSpectral>::triangle_solid_angle_(const Vec3<double>& c0, const Vec3<double>& c1, const Vec3<double>& c2) const
    {
        // Compute interior angles using tangent vectors
        Vec3<double> t01 = tangent_(c0, c1);
        Vec3<double> t02 = tangent_(c0, c2);
        double angle0 = std::acos(glm::dot(t01, t02));

        Vec3<double> t10 = tangent_(c1, c0);
        Vec3<double> t12 = tangent_(c1, c2);
        double angle1 = std::acos(glm::dot(t10, t12));

        Vec3<double> t20 = tangent_(c2, c0);
        Vec3<double> t21 = tangent_(c2, c1);
        double angle2 = std::acos(glm::dot(t20, t21));

        // Apply Girard's theorem
        return angle0 + angle1 + angle2 - PI<double>();
    }

    template <IsSpectral TSpectral>
    void CameraModel<TSpectral>::compute_frustum_()
    {
        Resolution res = sensor_->resolution();

        Vec3<float> xdir{ 1,0,0 };
        Vec3<float> ydir{ 0,1,0 };
        Vec3<float> zdir{ 0, 0, 1 };
        if (blender_convention_) {
            zdir = Vec3<float>{ 0, 0, -1 };
            ydir = Vec3<float>{ 0, -1, 0 };
        }

        // Initialize the frustum side planes:
        Vec3<float> left_extrema{ 0,0,0 };
        float left_min_dot = 100.f;

        Vec3<float> right_extrema{ 0,0,0 };
        float right_min_dot = 100.f;

        Vec3<float> top_extrema{ 0,0,0 };
        float top_min_dot = 100.f;

        Vec3<float> bottom_extrema{ 0,0,0 };
        float bottom_min_dot = 100.f;

        auto update = [&](int i, int j, Vec3<float>& extrema, float& min_dot) {
            Ray<TSpectral> ray = cast_ray(i, j);
            Vec3<float> direction = ray.direction();
            float d = glm::dot(direction, zdir);
            if (d < min_dot) {
                extrema = direction;
                min_dot = d;
            }
            };

        // Loop over all edge-pixels
        for (int i = 0; i < res.x; ++i) {
            update(i, 0, top_extrema, top_min_dot);
            update(i, res.y - 1, bottom_extrema, bottom_min_dot);
        }
        for (int j = 1; j < res.y - 1; ++j) {
            update(0, j, left_extrema, left_min_dot);
            update(res.x - 1, j, right_extrema, right_min_dot);
        }

        // Form the frustum planes:
        Vec3<float> left_normal = glm::normalize(glm::cross(ydir, left_extrema));
        Vec3<float> right_normal = glm::normalize(glm::cross(right_extrema, ydir));
        Vec3<float> top_normal = glm::normalize(glm::cross(top_extrema, xdir));
        Vec3<float> bottom_normal = glm::normalize(glm::cross(xdir, bottom_extrema));

        view_frustum_ = Frustum<TSpectral>({ zdir, left_normal, right_normal, top_normal, bottom_normal });
    }
}

