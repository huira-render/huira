
#include "huira/core/concepts/spectral_concepts.hpp"

namespace huira {
    template <IsSpectral TSpectral>

    /**
     * @brief Set the focal length of the camera (in millimeters).
     * @param focal_length Focal length in millimeters
     */
    template <IsSpectral TSpectral>
    void CameraModelHandle<TSpectral>::set_focal_length(units::Millimeter focal_length) const {
        this->get_()->set_focal_length(focal_length);
    }


    /**
     * @brief Get the focal length of the camera (in millimeters).
     * @return float Focal length
     */
    template <IsSpectral TSpectral>
    float CameraModelHandle<TSpectral>::focal_length() const
    {
        return this->get_()->focal_length();
    }


    /**
     * @brief Set the f-stop (aperture ratio) of the camera.
     * @param fstop F-stop value
     */
    template <IsSpectral TSpectral>
    void CameraModelHandle<TSpectral>::set_fstop(float fstop) const
    {
        this->get_()->set_fstop(fstop);
    }


    /**
     * @brief Get the f-stop (aperture ratio) of the camera.
     * @return float F-stop value
     */
    template <IsSpectral TSpectral>
    float CameraModelHandle<TSpectral>::fstop() const
    {
        return this->get_()->fstop();
    }



    /**
     * @brief Set the distortion model for the camera.
     * @tparam TDistortion Distortion model type
     * @tparam Args Constructor arguments for the distortion model
     * @param args Arguments to construct the distortion model
     */
    template <IsSpectral TSpectral>
    template <IsDistortion TDistortion, typename... Args>
    void CameraModelHandle<TSpectral>::set_distortion(Args&&... args) const
    {
        this->get_()->template set_distortion<TDistortion>(std::forward<Args>(args)...);
    }


    /**
     * @brief Set Brown-Conrady distortion coefficients.
     * @param coeffs Brown distortion coefficients
     */
    template <IsSpectral TSpectral>
    void CameraModelHandle<TSpectral>::set_brown_conrady_distortion(BrownCoefficients coeffs) const
    {
        this->get_()->set_brown_conrady_distortion(coeffs);
    }


    /**
     * @brief Set OpenCV distortion coefficients.
     * @param coeffs OpenCV distortion coefficients
     */
    template <IsSpectral TSpectral>
    void CameraModelHandle<TSpectral>::set_opencv_distortion(OpenCVCoefficients coeffs) const
    {
        this->get_()->set_opencv_distortion(coeffs);
    }


    /**
     * @brief Set Owen distortion coefficients.
     * @param coeffs Owen distortion coefficients
     */
    template <IsSpectral TSpectral>
    void CameraModelHandle<TSpectral>::set_owen_distortion(OwenCoefficients coeffs) const
    {
        this->get_()->set_owen_distortion(coeffs);
    }


    /**
     * @brief Delete the distortion model from the camera.
     */
    template <IsSpectral TSpectral>
    void CameraModelHandle<TSpectral>::delete_distortion() const
    {
        this->get_()->delete_distortion();
    }


    /**
     * @brief Set the sensor model for the camera.
     * @tparam TSensor Sensor model type
     * @tparam Args Constructor arguments for the sensor
     * @param args Arguments to construct the sensor
     */
    template <IsSpectral TSpectral>
    template <IsSensor TSensor, typename... Args>
    void CameraModelHandle<TSpectral>::set_sensor(Args&&... args) const
    {
        this->get_()->template set_sensor<TSensor>(std::forward<Args>(args)...);
    }


    /**
     * @brief Set the sensor resolution.
     * @param resolution Sensor resolution (width, height)
     */
    template <IsSpectral TSpectral>
    void CameraModelHandle<TSpectral>::set_sensor_resolution(Resolution resolution) const
    {
        this->get_()->set_sensor_resolution(resolution);
    }


    /**
     * @brief Set the sensor resolution by width and height.
     * @param width Sensor width in pixels
     * @param height Sensor height in pixels
     */
    template <IsSpectral TSpectral>
    void CameraModelHandle<TSpectral>::set_sensor_resolution(int width, int height) const
    {
        this->get_()->set_sensor_resolution(Resolution{ width, height });
    }


    /**
     * @brief Set the sensor pixel pitch in x and y directions.
     * @param pitch_x Pixel pitch in x (millimeters)
     * @param pitch_y Pixel pitch in y (millimeters)
     */
    template <IsSpectral TSpectral>
    void CameraModelHandle<TSpectral>::set_sensor_pixel_pitch(units::Millimeter pitch_x, units::Millimeter pitch_y) const
    {
        this->get_()->set_sensor_pixel_pitch(pitch_x, pitch_y);
    }


    /**
     * @brief Set the sensor pixel pitch (square pixels).
     * @param pitch Pixel pitch in both x and y (millimeters)
     */
    template <IsSpectral TSpectral>
    void CameraModelHandle<TSpectral>::set_sensor_pixel_pitch(units::Millimeter pitch) const
    {
        this->get_()->set_sensor_pixel_pitch(pitch, pitch);
    }


    /**
     * @brief Set the physical sensor size.
     * @param width Sensor width in millimeters
     * @param height Sensor height in millimeters
     */
    template <IsSpectral TSpectral>
    void CameraModelHandle<TSpectral>::set_sensor_size(units::Millimeter width, units::Millimeter height) const
    {
        this->get_()->set_sensor_size(width, height);
    }


    /**
     * @brief Set the sensor width and compute height from aspect ratio.
     * @param width Sensor width in millimeters
     */
    template <IsSpectral TSpectral>
    void CameraModelHandle<TSpectral>::set_sensor_size(units::Millimeter width) const
    {
        this->get_()->set_sensor_size(width);
    }



    /**
     * @brief Set the sensor quantum efficiency.
     * @param qe Quantum efficiency value
     */
    template <IsSpectral TSpectral>
    void CameraModelHandle<TSpectral>::set_sensor_quantum_efficiency(TSpectral qe) const
    {
        this->get_()->sensor_->set_quantum_efficiency(qe);
    }


    /**
     * @brief Set the sensor full well capacity.
     * @param fwc Full well capacity value
     */
    template <IsSpectral TSpectral>
    void CameraModelHandle<TSpectral>::set_sensor_full_well_capacity(float fwc) const
    {
        this->get_()->sensor_->set_full_well_capacity(fwc);
    }


    /**
     * @brief Set the sensor read noise.
     * @param read_noise Read noise value
     */
    template <IsSpectral TSpectral>
    void CameraModelHandle<TSpectral>::set_sensor_read_noise(float read_noise) const
    {
        this->get_()->sensor_->set_read_noise(read_noise);
    }


    /**
     * @brief Set the sensor dark current.
     * @param dark_current Dark current value
     */
    template <IsSpectral TSpectral>
    void CameraModelHandle<TSpectral>::set_sensor_dark_current(float dark_current) const
    {
        this->get_()->sensor_->set_dark_current(dark_current);
    }


    /**
     * @brief Set the sensor bias level.
     * @param bias_level Bias level value
     */
    template <IsSpectral TSpectral>
    void CameraModelHandle<TSpectral>::set_sensor_bias_level(float bias_level) const
    {
        this->get_()->sensor_->set_bias_level_dn(bias_level);
    }


    /**
     * @brief Set the sensor bit depth.
     * @param bit_depth Bit depth value
     */
    template <IsSpectral TSpectral>
    void CameraModelHandle<TSpectral>::set_sensor_bit_depth(int bit_depth) const
    {
        this->get_()->sensor_->set_bit_depth(bit_depth);
    }


    /**
     * @brief Set the sensor gain (ADU).
     * @param gain Gain value
     */
    template <IsSpectral TSpectral>
    void CameraModelHandle<TSpectral>::set_sensor_gain(float gain) const
    {
        this->get_()->sensor_->set_gain_adu(gain);
    }


    /**
     * @brief Set the sensor gain in decibels (dB).
     * @param gain_db Gain in dB
     */
    template <IsSpectral TSpectral>
    void CameraModelHandle<TSpectral>::set_sensor_gain_db(float gain_db) const
    {
        this->get_()->sensor_->set_gain_db(gain_db);
    }


    /**
     * @brief Set the sensor unity gain in decibels (dB).
     * @param unity_db Unity gain in dB
     */
    template <IsSpectral TSpectral>
    void CameraModelHandle<TSpectral>::set_sensor_uinty_db(float unity_db) const
    {
        this->get_()->sensor_->set_unity_db(unity_db);
    }


    /**
     * @brief Set the sensor rotation angle.
     * @param angle Rotation angle in radians
     */
    template <IsSpectral TSpectral>
    void CameraModelHandle<TSpectral>::set_sensor_rotation(units::Radian angle) const
    {
        this->get_()->sensor_->set_rotation(angle);
    }


    /**
     * @brief Set the aperture model for the camera.
     * @tparam TAperture Aperture model type
     * @tparam Args Constructor arguments for the aperture
     * @param args Arguments to construct the aperture
     */
    template <IsSpectral TSpectral>
    template <IsAperture TAperture, typename... Args>
    void CameraModelHandle<TSpectral>::set_aperture(Args&&... args) const
    {
        this->get_()->template set_aperture<TAperture>(std::forward<Args>(args)...);
    }


    /**
     * @brief Set the point spread function (PSF) model for the camera.
     * @tparam TPSF PSF model type
     * @tparam Args Constructor arguments for the PSF
     * @param args Arguments to construct the PSF
     */
    template <IsSpectral TSpectral>
    template <IsPSF TPSF, typename... Args>
    void CameraModelHandle<TSpectral>::set_psf(Args&&... args) const
    {
        this->get_()->template set_psf<TPSF>(std::forward<Args>(args)...);
    }


    /**
     * @brief Use the aperture to generate a PSF (point spread function).
     * @param radius PSF kernel radius
     * @param banks Number of PSF banks
     */
    template <IsSpectral TSpectral>
    void CameraModelHandle<TSpectral>::use_aperture_psf(int radius, int banks) const
    {
        this->get_()->use_aperture_psf(radius, banks);
    }


    /**
     * @brief Delete the PSF and disable aperture PSF usage.
     */
    template <IsSpectral TSpectral>
    void CameraModelHandle<TSpectral>::delete_psf() const
    {
        this->get_()->delete_psf();
    }


    /**
     * @brief Project a 3D point in camera coordinates onto the image plane.
     * @param point_camera_coords 3D point in camera coordinates (meters)
     * @return Pixel 2D point on the image plane (pixels)
     */
    template <IsSpectral TSpectral>
    Pixel CameraModelHandle<TSpectral>::project_point(const Vec3<float>& point_camera_coords) const
    {
        return this->get_()->project_point(point_camera_coords);
    }



    /**
     * @brief Create a new frame buffer with the camera's resolution.
     * @return FrameBuffer<TSpectral> Frame buffer
     */
    template <IsSpectral TSpectral>
    FrameBuffer<TSpectral> CameraModelHandle<TSpectral>::make_frame_buffer() const {
        return this->get_()->make_frame_buffer();
    }


    /**
     * @brief Set whether to use Blender's camera convention (z forward, y up).
     * @param value True to use Blender convention
     */
    template <IsSpectral TSpectral>
    void CameraModelHandle<TSpectral>::use_blender_convention(bool value) const
    {
        this->get_()->use_blender_convention(value);
    }
}
