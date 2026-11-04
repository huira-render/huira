#pragma once

#include "huira/cameras/sensors/sensor_calibration.hpp"
#include "huira/images/image.hpp"
#include "huira/units/units.hpp"
#include "pybind11/pybind11.h"

namespace py = pybind11;

namespace huira {

/// Binds what turns a real sensor's calibration frames into maps for a simulated one. After the
/// images (huira.Image_f32) and units, which these take.
inline void bind_sensor_calibration(py::module_& m)
{
    py::enum_<BiasMapKind>(m,
                           "BiasMapKind",
                           "What the values of a bias map are "
                           "(CameraModelHandle.set_sensor_bias_map()).")
        .value("Absolute",
               BiasMapKind::Absolute,
               "Each pixel's bias, in DN: a master bias frame. It replaces the bias level, which "
               "is not used while the map is set.")
        .value("Relative",
               BiasMapKind::Relative,
               "Each pixel's offset from the bias level, in DN: the bias's structure alone, "
               "averaging about 0. The bias level still applies, and changing it moves the whole "
               "map.");

    const char* doc =
        "The dark current of each pixel, in electrons per second, from a master dark and a "
        "master bias: (dark - bias) * conversion_gain / exposure_time, for "
        "CameraModelHandle.set_sensor_dark_current_map().\n\n"
        "Both frames are in DN, as the camera that took them read them out, and conversion_gain "
        "is that camera's, in e-/DN. master_bias is an image the size of the master dark, or one "
        "value in DN for every pixel. exposure_time is the master dark's (any time unit, e.g. "
        "Second).\n\n"
        "An image with a sensor_bit_depth (a sensor response, or a frame read from a file that "
        "records its counts' bit depth) holds DN / (2^bits - 1) and is converted; "
        "any other image's values are taken as DN, and one without a bit depth whose values are "
        "all at most 1 DN is refused as normalized (multiply a 32-bit float master from Siril or "
        "PixInsight by 65535). A pixel below the master bias, which noise does near zero dark "
        "current, gets 0; a warning counts them.";

    m.def("dark_current_from_master_dark",
          py::overload_cast<const Image<float>&, const Image<float>&, float, units::Second>(
              &dark_current_from_master_dark),
          py::arg("master_dark"),
          py::arg("master_bias"),
          py::arg("conversion_gain"),
          py::arg("exposure_time"),
          doc);
    m.def("dark_current_from_master_dark",
          py::overload_cast<const Image<float>&, float, float, units::Second>(
              &dark_current_from_master_dark),
          py::arg("master_dark"),
          py::arg("master_bias"),
          py::arg("conversion_gain"),
          py::arg("exposure_time"),
          doc);
}

} // namespace huira
