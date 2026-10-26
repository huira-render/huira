#include <algorithm>
#include <cmath>
#include <cstddef>
#include <string>

#include "huira/util/logger.hpp"
#include "huira/util/macros.hpp"

namespace huira {

namespace detail {

inline double dn_per_value(const Image<float>& image)
{
    const int bits = image.sensor_bit_depth();
    return bits > 0 ? std::ldexp(1.0, bits) - 1.0 : 1.0;
}

inline void warn_if_normalized(const Image<float>& image, const char* what, const char* caller)
{
    if (image.sensor_bit_depth() > 0 || image.empty()) {
        return;
    }
    float largest = 0.f;
    for (std::size_t i = 0; i < image.size(); ++i) {
        largest = std::max(largest, image[i]);
    }
    if (largest > 0.f && largest <= 1.f) {
        HUIRA_LOG_WARNING(std::string(caller) + " - Every value of the " + what +
                          " is at most 1 DN. If they are normalized, as DN / (2^bits - 1) (as a "
                          "sensor response holds them, or an integer FITS file read without the "
                          "ADCBITS or SATURATE keys that give its bit depth), multiply them by "
                          "2^bits - 1 first.");
    }
}

/// The dark current from a master dark, with bias_dn(x, y) the master bias of each pixel in DN.
template <typename TBias>
Image<float> dark_current_from_master_dark_(const Image<float>& master_dark,
                                            const TBias& bias_dn,
                                            float conversion_gain,
                                            units::Second exposure_time)
{
    const char* caller = "dark_current_from_master_dark";
    const double t = exposure_time.to_si();
    if (!(t > 0.0) || !std::isfinite(t)) {
        HUIRA_THROW_ERROR(std::string(caller) +
                          " - The master dark's exposure time must be "
                          "positive and finite: " +
                          std::to_string(t) + " s");
    }
    if (!(conversion_gain > 0.f) || !std::isfinite(conversion_gain)) {
        HUIRA_THROW_ERROR(std::string(caller) +
                          " - The conversion gain must be positive and "
                          "finite: " +
                          std::to_string(conversion_gain) + " e-/DN");
    }
    if (master_dark.empty()) {
        HUIRA_THROW_ERROR(std::string(caller) + " - The master dark is empty.");
    }
    warn_if_normalized(master_dark, "master dark", caller);

    const double dark_scale = dn_per_value(master_dark);
    const double electrons_per_dn_second = static_cast<double>(conversion_gain) / t;

    Image<float> rates(master_dark.resolution());
    std::size_t non_finite = 0;
    std::size_t negative = 0;
    for (int y = 0; y < master_dark.height(); ++y) {
        for (int x = 0; x < master_dark.width(); ++x) {
            const double dark = static_cast<double>(master_dark(x, y)) * dark_scale;
            const double rate = (dark - bias_dn(x, y)) * electrons_per_dn_second;
            if (!std::isfinite(rate)) {
                ++non_finite;
                continue;
            }
            if (rate < 0.0) {
                ++negative;
            }
            rates(x, y) = static_cast<float>(std::max(rate, 0.0));
        }
    }
    if (non_finite > 0) {
        HUIRA_THROW_ERROR(std::string(caller) + " - " + std::to_string(non_finite) +
                          " pixel(s) of the master dark or bias are not finite (NaN or "
                          "infinite), as a file's missing data reads: replace them first, with "
                          "their neighbors' value, say.");
    }
    if (negative > 0) {
        HUIRA_LOG_WARNING(std::string(caller) + " - " + std::to_string(negative) +
                          " pixel(s) of the master dark are below the master bias, which noise "
                          "in either does near zero dark current. Their dark current is taken "
                          "as 0.");
    }
    return rates;
}
} // namespace detail

/**
 * @brief The dark current of each pixel, in electrons per second, from a master dark and a
 * master bias: (dark - bias) * gain / exposure time.
 *
 * A master dark is the mean, or median, of many frames taken with the shutter closed, and a
 * master bias of many taken with no exposure; both in DN, as the camera that took them read
 * them out. Their difference is the dark current's charge over the exposure, in DN.
 *
 * The values of an image with a sensor bit depth (Image::sensor_bit_depth(): a sensor response,
 * or a frame read from a file that records its bit depth) are DN / (2^bits - 1), and are turned
 * into DN; any other image's values are DN. A warning points out an image without a sensor bit
 * depth whose values are all at most 1 DN, which looks normalized.
 *
 * The result is for SensorModel::set_dark_current_map() (CameraModelHandle::
 * set_sensor_dark_current_map()), and is in the master dark's layout. A pixel below the master
 * bias, which noise in either does near zero dark current, has a dark current of 0; a warning
 * gives the number of them.
 *
 * @param master_dark The master dark.
 * @param master_bias The master bias, the same size as the master dark.
 * @param conversion_gain The conversion gain, in e-/DN, of the camera that took them.
 * @param exposure_time The master dark's exposure time.
 * @return The dark current of each pixel, in e-/s.
 * @throws std::runtime_error if the images differ in size, a pixel of either is not finite, or
 *         the gain or the exposure time is not positive and finite.
 */
inline Image<float> dark_current_from_master_dark(const Image<float>& master_dark,
                                                  const Image<float>& master_bias,
                                                  float conversion_gain,
                                                  units::Second exposure_time)
{
    if (!(master_bias.resolution() == master_dark.resolution())) {
        HUIRA_THROW_ERROR(
            "dark_current_from_master_dark - The master bias must be the size of the master "
            "dark: " +
            std::to_string(master_bias.width()) + "x" + std::to_string(master_bias.height()) +
            ", not " + std::to_string(master_dark.width()) + "x" +
            std::to_string(master_dark.height()));
    }
    detail::warn_if_normalized(master_bias, "master bias", "dark_current_from_master_dark");
    const double bias_scale = detail::dn_per_value(master_bias);
    auto bias_dn = [&](int x, int y) {
        return static_cast<double>(master_bias(x, y)) * bias_scale;
    };
    return detail::dark_current_from_master_dark_(
        master_dark, bias_dn, conversion_gain, exposure_time);
}

/**
 * @brief The dark current of each pixel, in electrons per second, from a master dark and one
 * bias for every pixel: (dark - bias) * gain / exposure time.
 *
 * As the overload that takes a master bias, for a camera whose bias is the same everywhere.
 *
 * @param master_dark The master dark.
 * @param master_bias_dn The bias, in DN.
 * @param conversion_gain The conversion gain, in e-/DN, of the camera that took the master dark.
 * @param exposure_time The master dark's exposure time.
 * @return The dark current of each pixel, in e-/s.
 * @throws std::runtime_error if a pixel of the master dark or the bias is not finite, or the gain
 *         or the exposure time is not positive and finite.
 */
inline Image<float> dark_current_from_master_dark(const Image<float>& master_dark,
                                                  float master_bias_dn,
                                                  float conversion_gain,
                                                  units::Second exposure_time)
{
    const double bias = static_cast<double>(master_bias_dn);
    auto bias_dn = [bias](int, int) { return bias; };
    return detail::dark_current_from_master_dark_(
        master_dark, bias_dn, conversion_gain, exposure_time);
}

} // namespace huira
