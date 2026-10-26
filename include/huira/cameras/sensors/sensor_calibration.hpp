#pragma once

#include "huira/images/image.hpp"
#include "huira/units/units.hpp"

namespace huira {

/**
 * @brief What the values of a bias map are: see SensorModel::set_bias_map().
 */
enum class BiasMapKind {
    /// Each pixel's bias, in DN: a master bias frame. It replaces the bias level, which is not
    /// used while the map is set.
    Absolute,

    /// Each pixel's offset from the bias level, in DN: the bias's structure alone, averaging
    /// about 0. The bias level still applies, and changing it moves the whole map.
    Relative
};

Image<float> dark_current_from_master_dark(const Image<float>& master_dark,
                                           const Image<float>& master_bias,
                                           float conversion_gain,
                                           units::Second exposure_time);

Image<float> dark_current_from_master_dark(const Image<float>& master_dark,
                                           float master_bias_dn,
                                           float conversion_gain,
                                           units::Second exposure_time);

namespace detail {
/// What an image's values are multiplied by to give DN: 2^bits - 1 for an image with a sensor
/// bit depth, which holds DN / (2^bits - 1) (a sensor response, or a frame read from a file that
/// records its bit depth), and 1 for any other image, whose values are DN.
double dn_per_value(const Image<float>& image);

/// Throws when an image that should hold DN, without a sensor bit depth, looks normalized: its
/// largest value is above 0 but at most 1 DN, which a real master bias or dark never is, since
/// the bias alone is many DN. Its DN can't be recovered from it, and taking it as it is would
/// give a bias and dark current of about 0.
void check_not_normalized(const Image<float>& image, const char* what, const char* caller);
} // namespace detail

} // namespace huira

#include "huira_impl/cameras/sensors/sensor_calibration.ipp"
