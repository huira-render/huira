#pragma once

#include <iomanip>
#include <map>
#include <optional>
#include <sstream>
#include <string>

#include "huira/handles/camera_handle.hpp"
#include "huira/units/units_py.ipp"
#include "pybind11/numpy.h"
#include "pybind11/pybind11.h"
#include "pybind11/stl.h"

namespace py = pybind11;

namespace huira {
namespace detail {
/// CameraModelHandle methods removed from the Python API, with when and what to use instead.
inline const std::map<std::string, std::string>& removed_camera_methods()
{
    static const std::map<std::string, std::string> removed{
        {"set_diopters", "v0.9.10. Use set_focus_diopters() instead."},
        {"get_diopters", "v0.9.10. Use focus_diopters() instead."},
        {"get_focus_distance", "v0.9.10. Use focus_distance() instead."},
        {"set_sensor_resolution",
         "v0.9.4. Use configure_sensor_from_pitch() or configure_sensor_from_size() instead."},
        {"set_sensor_pixel_pitch", "v0.9.4. Use configure_sensor_from_pitch() instead."},
        {"set_sensor_size", "v0.9.4. Use configure_sensor_from_size() instead."},
        {"set_sensor_rotation", "v0.9.10. Use set_sensor_roll() instead."},
        {"set_sensor_simulate_noise", "v0.9.10. Use enable_sensor_noise() instead."},
        {"use_aperture_psf",
         "v0.9.10. The aperture's diffraction pattern is on by default: use "
         "enable_diffraction() to turn it on or off, and clear_psf() to go back to it from a "
         "PSF that was set. Its extent is automatic."},
        {"delete_psf",
         "v0.9.10. Use clear_psf() to remove a PSF that was set, and enable_diffraction(False) "
         "to turn the aperture's diffraction pattern off."},
        {"set_psf_convolution_radius",
         "v0.9.10. Resolved bodies are convolved with the whole PSF across the frame; use "
         "psf_image() to see it."},
        {"set_harvey_shack_scatter",
         "v0.9.10. Use set_scatter(), which takes the shoulder and outer radius as angles."},
        {"disable_harvey_shack_scatter", "v0.9.10. Use disable_scatter() instead."},
        {"get_psf_radius",
         "v0.9.10. Sources are drawn as far as their light reaches (see "
         "Renderer.set_unresolved_taper()); psf_image() takes the radius to show."},
        {"get_psf_convolution_kernel",
         "v0.9.10. Use psf_image(), the whole PSF resolved bodies are convolved with."},
    };
    return removed;
}

/// A 3-vector from Python: a huira.Vec3, or any sequence or array of three numbers.
inline Vec3<float> vec3f_from_py(const py::object& obj)
{
    if (py::isinstance<Vec3<double>>(obj)) {
        return Vec3<float>(obj.cast<Vec3<double>>());
    }
    const auto array = py::array_t<double, py::array::c_style | py::array::forcecast>::ensure(obj);
    if (!array || array.size() != 3) {
        throw py::value_error("Expected a huira.Vec3, or a sequence of three numbers");
    }
    const double* v = array.data();
    return {static_cast<float>(v[0]), static_cast<float>(v[1]), static_cast<float>(v[2])};
}
} // namespace detail

template <typename TSpectral>
inline void bind_camera_model_handle(py::module_& m)
{
    using HandleType = CameraModelHandle<TSpectral>;

    py::class_<HandleType>(m, "CameraModelHandle")
        // Focal length
        .def(
            "set_focal_length",
            [](const HandleType& self, const py::object& fl) {
                self.set_focal_length(detail::unit_from_py<units::Millimeter>(fl));
            },
            py::arg("focal_length"),
            "Set the focal length (accepts any distance unit). Until the aperture is set, the "
            "camera stays f/2.8. After set_fstop() the aperture's diameter is kept, so the "
            "f-number changes (with a warning); set the f-stop after the focal length.")
        .def("focal_length", &HandleType::focal_length)

        // Aperture
        .def("set_fstop",
             &HandleType::set_fstop,
             py::arg("fstop"),
             "Set the f-number, e.g. 8 for f/8. This sets the aperture's diameter from the "
             "current focal length, and the diameter is what is kept if the focal length "
             "changes later (with a warning), so set the f-stop after the focal length. Until "
             "the aperture is set, the camera is f/2.8 at any focal length.")
        .def("fstop", &HandleType::fstop, "The f-number: focal length / aperture diameter.")
        .def(
            "set_aperture_diameter",
            [](const HandleType& self, const py::object& diameter) {
                self.set_aperture_diameter(detail::unit_from_py<units::Millimeter>(diameter));
            },
            py::arg("diameter"),
            "Set the aperture's diameter (any distance unit). It is kept if the focal length "
            "changes, so the f-number follows the focal length.")
        .def("aperture_diameter", &HandleType::aperture_diameter)

        .def(
            "configure_sensor_from_pitch",
            [](HandleType& self,
               std::pair<int, int> res,
               const py::object& px,
               const py::object& py_,
               std::optional<float> cx,
               std::optional<float> cy) {
                units::Micrometer pitch_x = detail::unit_from_py<units::Micrometer>(px);
                std::optional<units::Micrometer> pitch_y = std::nullopt;

                if (!py_.is_none()) {
                    pitch_y = detail::unit_from_py<units::Micrometer>(py_);
                }

                self.configure_sensor_from_pitch(
                    Resolution{res.first, res.second}, pitch_x, pitch_y, cx, cy);
            },
            py::arg("resolution"),
            py::arg("pitch_x"),
            py::arg("pitch_y") = py::none(),
            py::arg("cx") = py::none(),
            py::arg("cy") = py::none(),
            "Configure sensor using a resolution tuple (width, height) and pixel pitch. pitch_y "
            "defaults to pitch_x (square pixels). cx/cy are in the camera's pixel convention "
            "(see set_pixel_convention) and default to the center of the sensor.")

        .def(
            "configure_sensor_from_size",
            [](HandleType& self,
               std::pair<int, int> res,
               const py::object& w,
               const py::object& h,
               std::optional<float> cx,
               std::optional<float> cy) {
                units::Millimeter width = detail::unit_from_py<units::Millimeter>(w);
                std::optional<units::Millimeter> height = std::nullopt;

                if (!h.is_none()) {
                    height = detail::unit_from_py<units::Millimeter>(h);
                }

                self.configure_sensor_from_size(
                    Resolution{res.first, res.second}, width, height, cx, cy);
            },
            py::arg("resolution"),
            py::arg("width"),
            py::arg("height") = py::none(),
            py::arg("cx") = py::none(),
            py::arg("cy") = py::none(),
            "Configure sensor using a resolution tuple (width, height) and physical size. height "
            "defaults to maintaining square pixels. cx/cy are in the camera's pixel convention "
            "(see set_pixel_convention) and default to the center of the sensor.")

        .def(
            "set_intrinsic_matrix",
            [](HandleType& self,
               const Mat3<double>& matrix,
               std::pair<int, int> res,
               const py::object& anchor_fl) {
                self.set_intrinsic_matrix(Mat3<float>(matrix),
                                          Resolution{res.first, res.second},
                                          detail::unit_from_py<units::Millimeter>(anchor_fl));
            },
            py::arg("intrinsic_matrix"),
            py::arg("resolution"),
            py::arg("anchor_focal_length"),
            "Set the intrinsics from a 3x3 camera matrix, a huira.Mat3 or a numpy array or nested "
            "list written row by row: [[fx, s, cx], [0, fy, cy], [0, 0, 1]] (up to scale), where "
            "s is the skew; with a resolution tuple and physical focal length anchor. The "
            "principal point is in the camera's pixel convention (see set_pixel_convention).")

        .def(
            "set_intrinsics",
            [](HandleType& self,
               float fx,
               float fy,
               float cx,
               float cy,
               std::pair<int, int> res,
               const py::object& anchor_fl,
               float skew) {
                self.set_intrinsics(fx,
                                    fy,
                                    cx,
                                    cy,
                                    Resolution{res.first, res.second},
                                    detail::unit_from_py<units::Millimeter>(anchor_fl),
                                    skew);
            },
            py::arg("fx"),
            py::arg("fy"),
            py::arg("cx"),
            py::arg("cy"),
            py::arg("resolution"),
            py::arg("anchor_focal_length"),
            py::arg("skew") = 0.f,
            "Explicitly set mathematical intrinsics with a resolution tuple and physical focal "
            "length anchor: (x, y) projects to (fx * x + skew * y + cx, fy * y + cy). cx/cy "
            "are in the camera's pixel convention (see set_pixel_convention); skew is 0 for "
            "perpendicular pixel axes.")

        .def(
            "intrinsic_matrix",
            [](const HandleType& self) { return Mat3<double>(self.intrinsic_matrix()); },
            "The camera matrix [[fx, s, cx], [0, fy, cy], [0, 0, 1]] as a huira.Mat3, as "
            "set_intrinsic_matrix() takes it: in pixels, with the principal point in the "
            "camera's pixel convention (see set_pixel_convention). It works with numpy as the "
            "3x3 array, row by row: np.asarray(K), K[0, 2].")
        .def(
            "resolution",
            [](const HandleType& self) {
                const Resolution r = self.resolution();
                return py::make_tuple(r.width, r.height);
            },
            "The sensor's resolution, as a tuple (width, height).")
        .def(
            "pixel_pitch",
            [](const HandleType& self) {
                const auto [x, y] = self.pixel_pitch();
                return py::make_tuple(x, y);
            },
            "The pixel pitch, as a tuple (along x, along y) of Micrometer.")

        // Pixel coordinate convention
        .def("set_pixel_convention",
             &HandleType::set_pixel_convention,
             py::arg("convention"),
             "Set the pixel coordinate convention for the principal point (cx, cy) and projected "
             "positions, e.g. huira.PixelConvention.fits(). The default is "
             "huira.PixelConvention.opencv(): the top-left pixel's center is (0, 0), y down. "
             "May be set before or after the principal point; changes nothing rendered unless a "
             "principal point was given.")
        .def("pixel_convention",
             &HandleType::pixel_convention,
             "The pixel coordinate convention for the principal point and projected positions.")

        // Distortion
        .def("set_brown_conrady_distortion",
             &HandleType::set_brown_conrady_distortion,
             py::arg("coeffs"))
        .def("set_opencv_distortion", &HandleType::set_opencv_distortion, py::arg("coeffs"))
        .def("set_owen_distortion", &HandleType::set_owen_distortion, py::arg("coeffs"))
        .def("delete_distortion", &HandleType::delete_distortion)

        // Filter wheel
        .def("add_filter",
             py::overload_cast<const std::string&, double, std::optional<Image<float>>>(
                 &HandleType::add_filter, py::const_),
             py::arg("name"),
             py::arg("transmission"),
             py::arg("flat") = std::nullopt,
             "Add a filter to the camera's filter wheel, and select it if it is the first; "
             "returns its index. transmission is the fraction of the light it passes, from 0 to "
             "1: one number for every bin, or a SpectralBins (or a list or array, one value per "
             "bin). flat is the filter's flat, an Image_f32 of each pixel's response through it "
             "relative to the principal point's (flat_from_flat_field() makes one from a flat "
             "field), or None for 1 everywhere. The selected filter's transmission multiplies "
             "the sensor's quantum efficiency, and its flat each pixel's signal electrons; the "
             "flat is one of the sensor's imperfections (enable_sensor_imperfections()). A "
             "camera whose wheel is empty has a clear filter and no flat. The name must be "
             "unused by the wheel's other filters.")
        .def("add_filter",
             py::overload_cast<const std::string&, const TSpectral&, std::optional<Image<float>>>(
                 &HandleType::add_filter, py::const_),
             py::arg("name"),
             py::arg("transmission"),
             py::arg("flat") = std::nullopt,
             "As above, with the transmission per bin.")
        .def("select_filter",
             py::overload_cast<std::size_t>(&HandleType::select_filter, py::const_),
             py::arg("index"),
             "Select a filter of the filter wheel by index, in the order added. A frame buffer "
             "is read out through the filter selected when it is rendered.")
        .def("select_filter",
             py::overload_cast<const std::string&>(&HandleType::select_filter, py::const_),
             py::arg("name"),
             "Select a filter of the filter wheel by name.")
        .def("set_filter_flat",
             py::overload_cast<std::size_t, std::optional<Image<float>>>(
                 &HandleType::set_filter_flat, py::const_),
             py::arg("index"),
             py::arg("flat"),
             "Replace the flat of the filter at an index, as for add_filter(), or remove it "
             "(None).")
        .def("set_filter_flat",
             py::overload_cast<const std::string&, std::optional<Image<float>>>(
                 &HandleType::set_filter_flat, py::const_),
             py::arg("name"),
             py::arg("flat"),
             "Replace the flat of the filter with a name, as for add_filter(), or remove it "
             "(None).")
        .def("clear_filters",
             &HandleType::clear_filters,
             "Empty the filter wheel: the camera then has a clear filter, with no flat.")
        .def("filter_count",
             &HandleType::filter_count,
             "The number of filters in the filter wheel; 0 without one.")
        .def("selected_filter_index",
             &HandleType::selected_filter_index,
             "The index of the selected filter, or None if the wheel is empty.")
        .def("selected_filter_name",
             &HandleType::selected_filter_name,
             "The name of the selected filter, or None if the wheel is empty.")
        .def("filter_name",
             &HandleType::filter_name,
             py::arg("index"),
             "The name of the filter at an index.")
        .def("filter_index",
             &HandleType::filter_index,
             py::arg("name"),
             "The index of the filter with a name.")
        .def("filter_transmission",
             py::overload_cast<std::size_t>(&HandleType::filter_transmission, py::const_),
             py::arg("index"),
             "The transmission, per bin, of the filter at an index.")
        .def("filter_transmission",
             py::overload_cast<const std::string&>(&HandleType::filter_transmission, py::const_),
             py::arg("name"),
             "The transmission, per bin, of the filter with a name.")
        .def("filter_flat",
             py::overload_cast<std::size_t>(&HandleType::filter_flat, py::const_),
             py::arg("index"),
             "The flat of the filter at an index, as an Image_f32, or None if it has none.")
        .def("filter_flat",
             py::overload_cast<const std::string&>(&HandleType::filter_flat, py::const_),
             py::arg("name"),
             "The flat of the filter with a name, as an Image_f32, or None if it has none.")
        .def("flat_from_flat_field",
             &HandleType::flat_from_flat_field,
             py::arg("flat_field"),
             py::arg("central_fraction") = 0.05f,
             "A filter's flat from a flat field taken through it (an Image_f32 at the sensor's "
             "resolution, bias and dark subtracted, in any scale): each pixel's response "
             "relative to the principal point's, with the falloff the renderer gives each pixel "
             "(its solid angle and cosine) divided out, so that it keeps what the renderer "
             "doesn't model: further vignetting, the filter's unevenness, dust and PRNU. It is "
             "normalized to the median of a square around the principal point, central_fraction "
             "of the shorter side on each edge (5% by default, at least 8 pixels), so a speck of "
             "dust there doesn't set the scale; the quantum efficiency and transmission are then "
             "the response at the principal point, and pixels that respond more are above 1. "
             "The camera's focal length, distortion and principal point must be as when the flat "
             "field was taken. For add_filter() or set_filter_flat().")

        // Sensor properties
        .def("set_sensor_quantum_efficiency",
             py::overload_cast<double>(&HandleType::set_sensor_quantum_efficiency, py::const_),
             py::arg("qe"),
             "Set quantum efficiency (scalar, e.g. 0.7)")
        .def("set_sensor_quantum_efficiency",
             py::overload_cast<TSpectral>(&HandleType::set_sensor_quantum_efficiency, py::const_),
             py::arg("qe"),
             "Set quantum efficiency (spectral)")
        .def("set_sensor_full_well_capacity",
             &HandleType::set_sensor_full_well_capacity,
             py::arg("fwc"),
             "Set the sensor's full well capacity, in e-: the most electrons a pixel holds.")
        .def("enable_sensor_noise",
             &HandleType::enable_sensor_noise,
             py::arg("noise") = true,
             "Turn the sensor's noise on (the default) or off: shot noise, read noise, and row "
             "and column noise. Off, each pixel collects the expected number of electrons. "
             "Independent of enable_sensor_imperfections().")
        .def("enable_sensor_imperfections",
             &HandleType::enable_sensor_imperfections,
             py::arg("imperfections") = true,
             "Turn the sensor's imperfections on (the default) or off: what is the same in every "
             "frame and a calibration would remove, the dark current (single value, map and hot "
             "pixels), PRNU, the selected filter's flat, and the bias map. Off, each pixel's "
             "gain is 1 and the bias is the bias level; the settings are kept. Independent of "
             "enable_sensor_noise(): with both off the sensor is ideal, and with only "
             "imperfections off its frames are as from a calibrated camera.")
        .def("set_sensor_read_noise",
             &HandleType::set_sensor_read_noise,
             py::arg("read_noise"),
             "Set the sensor's read noise, in e- RMS: the noise of each pixel's readout.")
        .def("set_sensor_row_noise",
             &HandleType::set_sensor_row_noise,
             py::arg("row_noise"),
             "Set the sensor's row noise, in e- RMS (0 by default): in each readout, every row's "
             "pixels shift together by one draw of this standard deviation, as banding along "
             "the rows (the image's x axis) that changes from frame to frame. For a sensor read "
             "out along y, set the column noise instead. Off with the other noise.")
        .def("set_sensor_column_noise",
             &HandleType::set_sensor_column_noise,
             py::arg("column_noise"),
             "Set the sensor's column noise, in e- RMS (0 by default): in each readout, every "
             "column's pixels shift together by one draw of this standard deviation, as banding "
             "along the columns that changes from frame to frame. Off with the other noise.")
        .def("set_sensor_prnu",
             &HandleType::set_sensor_prnu,
             py::arg("prnu"),
             "Set the sensor's photo response non-uniformity: the standard deviation of each "
             "pixel's gain (0.01 is 1%), by which its signal electrons are multiplied. A fixed "
             "pattern made from the noise seed, the same in every frame; one of the sensor's "
             "imperfections. A flat from a real flat field already holds the camera's PRNU: "
             "leave this at 0 with one. 0 by default.")
        .def("set_sensor_hot_pixels",
             &HandleType::set_sensor_hot_pixels,
             py::arg("fraction"),
             py::arg("dark_current"),
             py::arg("spread") = 0.f,
             "Give the sensor hot pixels: each pixel is hot with probability fraction, and a hot "
             "pixel gathers a dark current of its own, in e-/s, on top of the sensor's: "
             "dark_current times exp(spread * N(0, 1)), a log-normal distribution whose median "
             "is dark_current (spread 0 for the same in every hot pixel). A fixed pattern made "
             "from the noise seed, the same pixels in every frame; a larger fraction keeps a "
             "smaller one's hot pixels and adds more. One of the sensor's imperfections. A dark "
             "current map from a real master dark already holds the camera's hot pixels: leave "
             "these off with one. Off (fraction 0) by default; set_sensor_hot_pixels(0, 0) turns "
             "them off.")
        .def("set_sensor_dark_current",
             &HandleType::set_sensor_dark_current,
             py::arg("dark_current"),
             "Set the sensor's dark current, in e-/s, the same for every pixel. A dark current "
             "map, while one is set, takes its place. One of the sensor's imperfections.")
        .def("set_sensor_dark_current_map",
             &HandleType::set_sensor_dark_current_map,
             py::arg("electrons_per_second"),
             "Set the dark current of each pixel, in e-/s, in place of the one dark current for "
             "all: an Image_f32 at the sensor's resolution, in the sensor response's layout "
             "(Image_f32(array, origin=...) makes one from a numpy array). Each pixel's dark "
             "current, times the exposure time, is the mean number of electrons it gathers in "
             "the dark, added to its signal before shot noise; for RGB, each channel gathers "
             "that many. huira.dark_current_from_master_dark() makes one from a master dark and "
             "a master bias; an image with a sensor_bit_depth (normalized DN, not e-/s) is "
             "refused. One of the sensor's imperfections. The camera keeps it until it is "
             "cleared or replaced.")
        .def("clear_sensor_dark_current_map",
             &HandleType::clear_sensor_dark_current_map,
             "Clear the sensor's dark current map, so that every pixel has the dark current "
             "again.")
        .def("set_sensor_bias_level",
             &HandleType::set_sensor_bias_level,
             py::arg("bias_level"),
             "Set the sensor's bias level, in DN, the same for every pixel. An absolute bias map, "
             "while one is set, takes its place; a relative one is added to it.")
        .def("set_sensor_bias_map",
             &HandleType::set_sensor_bias_map,
             py::arg("bias_dn"),
             py::arg("kind") = BiasMapKind::Absolute,
             "Set the bias of each pixel, in DN: an Image_f32 at the sensor's resolution, in the "
             "sensor response's layout (Image_f32(array, origin=...) makes one from a numpy "
             "array). With kind BiasMapKind.Absolute (the default), it is each pixel's bias, as "
             "a master bias frame gives it, and the bias level is not used while it is set; with "
             "BiasMapKind.Relative, each pixel's offset from the bias level, averaging about 0, "
             "and the bias level still applies. Each pixel's bias must be in [0, 2^bit_depth - "
             "1). An image with a sensor_bit_depth (a sensor response, or a frame read from a "
             "file that records its counts' bit depth) holds DN / (2^bits - 1) and is "
             "converted; one without, whose values are all at most 1 DN, is refused as "
             "normalized (multiply a 32-bit float master from Siril or PixInsight by 65535). One "
             "of the sensor's imperfections. The camera keeps it until it is cleared or "
             "replaced.")
        .def("clear_sensor_bias_map",
             &HandleType::clear_sensor_bias_map,
             "Clear the sensor's bias map, so that every pixel has the bias level again.")
        .def("set_sensor_bit_depth",
             &HandleType::set_sensor_bit_depth,
             py::arg("bit_depth"),
             "Set the sensor's bit depth: its ADC's, so DN run from 0 to 2^bit_depth - 1.")
        .def("set_sensor_conversion_gain",
             &HandleType::set_sensor_conversion_gain,
             py::arg("gain"),
             "Set the sensor's conversion gain, in e-/DN.")
        .def("set_sensor_gain_db", &HandleType::set_sensor_gain_db, py::arg("gain_db"))
        .def("set_sensor_unity_db", &HandleType::set_sensor_unity_db, py::arg("unity_db"))
        .def("set_sensor_noise_seed",
             &HandleType::set_sensor_noise_seed,
             py::arg("seed"),
             "Seed the sensor's noise. Setting the same seed again repeats the same sequence of "
             "frames; the seed also makes the sensor's PRNU and hot pixels, so another seed "
             "gives another sensor's. Sensors get distinct seeds by default.")
        .def("sensor_noise_seed",
             &HandleType::sensor_noise_seed,
             "The seed of the sensor's noise. See set_sensor_noise_seed().")
        .def("sensor_quantum_efficiency",
             &HandleType::sensor_quantum_efficiency,
             "The sensor's quantum efficiency, per spectral bin.")
        .def("sensor_full_well_capacity",
             &HandleType::sensor_full_well_capacity,
             "The sensor's full well capacity, in electrons.")
        .def("sensor_noise_enabled",
             &HandleType::sensor_noise_enabled,
             "Whether the sensor's noise is on (the default). See enable_sensor_noise().")
        .def("sensor_imperfections_enabled",
             &HandleType::sensor_imperfections_enabled,
             "Whether the sensor's imperfections are on (the default). See "
             "enable_sensor_imperfections().")
        .def("sensor_read_noise",
             &HandleType::sensor_read_noise,
             "The sensor's read noise, in electrons.")
        .def("sensor_row_noise",
             &HandleType::sensor_row_noise,
             "The sensor's row noise, in e- RMS. See set_sensor_row_noise().")
        .def("sensor_column_noise",
             &HandleType::sensor_column_noise,
             "The sensor's column noise, in e- RMS. See set_sensor_column_noise().")
        .def("sensor_prnu",
             &HandleType::sensor_prnu,
             "The standard deviation of each pixel's gain. See set_sensor_prnu().")
        .def("sensor_prnu_map",
             &HandleType::sensor_prnu_map,
             "Each pixel's gain from the PRNU, as an Image_f32: 1 everywhere without it.")
        .def("sensor_hot_pixel_fraction",
             &HandleType::sensor_hot_pixel_fraction,
             "The fraction of the sensor's pixels that are hot. See set_sensor_hot_pixels().")
        .def("sensor_hot_pixel_dark_current",
             &HandleType::sensor_hot_pixel_dark_current,
             "The hot pixels' median dark current, in e-/s.")
        .def("sensor_hot_pixel_spread",
             &HandleType::sensor_hot_pixel_spread,
             "The standard deviation of the natural log of the hot pixels' dark currents.")
        .def("sensor_hot_pixel_map",
             &HandleType::sensor_hot_pixel_map,
             "Each pixel's dark current as a hot pixel, in e-/s, as an Image_f32: 0 for the "
             "pixels that aren't hot.")
        .def("sensor_dark_current",
             &HandleType::sensor_dark_current,
             "The sensor's dark current, in electrons per second.")
        .def("has_sensor_dark_current_map",
             &HandleType::has_sensor_dark_current_map,
             "Whether the sensor has a dark current map. See set_sensor_dark_current_map().")
        .def("sensor_dark_current_map",
             &HandleType::sensor_dark_current_map,
             "The dark current of each pixel, in e-/s, as an Image_f32: the dark current map, or "
             "the dark current everywhere if none is set.")
        .def("sensor_bias_level",
             &HandleType::sensor_bias_level,
             "The sensor's bias level, in digital numbers.")
        .def("has_sensor_bias_map",
             &HandleType::has_sensor_bias_map,
             "Whether the sensor has a bias map. See set_sensor_bias_map().")
        .def("sensor_bias_map_kind",
             &HandleType::sensor_bias_map_kind,
             "What the values of the sensor's bias map are (BiasMapKind.Absolute if none is "
             "set).")
        .def("sensor_bias_map",
             &HandleType::sensor_bias_map,
             "The bias of each pixel, in DN, as an Image_f32: an absolute bias map, a relative "
             "one added to the bias level, or the bias level everywhere if none is set.")
        .def("sensor_bit_depth", &HandleType::sensor_bit_depth, "The sensor's bit depth.")
        .def("sensor_conversion_gain",
             &HandleType::sensor_conversion_gain,
             "The sensor's conversion gain, in electrons per digital number.")
        .def("sensor_gain_db", &HandleType::sensor_gain_db, "The sensor's gain, in decibels.")
        .def("sensor_unity_db",
             &HandleType::sensor_unity_db,
             "The gain, in decibels, at which the sensor's conversion gain is unity.")

        // Sensor orientation
        .def(
            "set_sensor_roll",
            [](const HandleType& self, const py::object& angle) {
                self.set_sensor_roll(detail::unit_from_py<units::Radian>(angle));
            },
            py::arg("angle"),
            "Set the sensor's roll about the optical axis (any angle unit, e.g. Degree). The "
            "sensor's x axis is turned by the angle toward its y axis, so with OpenCV's axes "
            "(the default) a positive roll turns the scene counterclockwise in the image. The "
            "camera's pose is unchanged; projections and rays are in the sensor's axes.")
        .def("sensor_roll",
             &HandleType::sensor_roll,
             "The sensor's roll about the optical axis, in Radian. See set_sensor_roll().")
        .def("sensor_orientation",
             &HandleType::sensor_orientation,
             "The sensor's orientation relative to the camera, as a huira.Rotation from the "
             "sensor's axes (in which projections and rays are given) to the camera's: the roll "
             "about the optical axis.")

        // PSF: the aperture's diffraction pattern, or a PSF set in its place, with scattered
        // light, blurred by each source's defocus
        .def("enable_diffraction",
             &HandleType::enable_diffraction,
             py::arg("diffraction") = true,
             "Turn the aperture's diffraction pattern on (the default) or off. On, unresolved "
             "sources are drawn with it, averaged over each spectral bin, and resolved bodies "
             "are convolved with it; off, a source in focus puts that light in the pixel it "
             "falls in. Scattered light and defocus are added either way. A PSF set with "
             "set_measured_psf() takes its place while set; clear_psf() goes back to it.")
        .def("diffraction_enabled",
             &HandleType::diffraction_enabled,
             "Whether the aperture's diffraction pattern is on (the default).")
        .def("set_measured_psf",
             // Named by its type, past the stub for the old signature:
             static_cast<void (HandleType::*)(const Image<TSpectral>&, float, PSFSampling, PSFFocus)
                             const>(&HandleType::set_measured_psf),
             py::arg("data"),
             py::arg("samples_per_pixel"),
             py::arg("sampling"),
             py::arg("focus") = PSFFocus::InFocus,
             "Use a measured (user-supplied) PSF in place of the aperture's diffraction "
             "pattern, until clear_psf(). 'data' is an Image of centered PSF samples, taken "
             "whole: as holding all of a source's light out to its farthest sample (crop it to "
             "use less); 'samples_per_pixel' is the measurement sampling density per sensor "
             "pixel per axis; 'sampling' says what the samples are (see PSFSampling): "
             "PSFSampling.PixelIntegrated if the data came from the sensor being simulated, "
             "such as a star's image, and otherwise almost always PSFSampling.PointSampled, such "
             "as optical design software's PSF. Sources are drawn from the spline through the "
             "samples. 'focus' says whether it is a point's in focus, which the camera's focus "
             "blurs (the default), or already holds the camera's defocus (see PSFFocus). "
             "Scattered light, if set, is added to it.")
        .def("clear_psf",
             &HandleType::clear_psf,
             "Remove a PSF set with set_measured_psf(): sources are drawn with the aperture's "
             "diffraction pattern again, or without one if it is off.")
        .def("has_set_psf",
             &HandleType::has_set_psf,
             "Whether a PSF was set in place of the aperture's diffraction pattern.")
        .def(
            "set_scatter",
            [](const HandleType& self,
               float fraction,
               float slope,
               const py::object& shoulder_angle,
               const py::object& outer_angle) {
                std::optional<units::Radian> outer;
                if (!outer_angle.is_none()) {
                    outer = detail::unit_from_py<units::Radian>(outer_angle);
                }
                self.set_scatter(
                    fraction, slope, detail::unit_from_py<units::Radian>(shoulder_angle), outer);
            },
            py::arg("fraction"),
            py::arg("slope"),
            py::arg("shoulder_angle"),
            py::arg("outer_angle") = py::none(),
            "Set scattered light in angles (any angle unit, e.g. Arcsecond): 'fraction' of each "
            "source's light spreads as (1 + (theta / shoulder_angle)^2)^(-slope / 2), falling "
            "as theta^-(slope + 2) beyond 'outer_angle'. A slope of 2 or less needs an outer "
            "angle. It is added to diffraction or a PSF that was set, and included in "
            "psf_image(); scatter_image() gives it alone.")
        .def("disable_scatter",
             &HandleType::disable_scatter,
             "Turn scattered light off (the default).")
        .def("scatter_enabled", &HandleType::scatter_enabled, "Whether scattered light is on.")

        // Stray light
        .def("set_veiling_glare",
             &HandleType::set_veiling_glare,
             py::arg("alpha"),
             "Redistribute the given fraction of total collected energy uniformly across the "
             "image (veiling glare).")
        .def("disable_veiling_glare", &HandleType::disable_veiling_glare)

        // Resolved bodies
        .def("enable_psf_convolution",
             &HandleType::enable_psf_convolution,
             py::arg("convolve_psf") = true,
             "Choose whether the PSF and scattered light blur resolved bodies (the path-traced "
             "image). On by default. Off, bodies are sharp, and rendering is faster without the "
             "whole-image convolution; unresolved sources, such as stars, get the PSF and "
             "scattered light either way. Bodies get the whole PSF, across the frame, in focus: "
             "path tracing gives them their defocus.")
        .def("psf_convolution_enabled",
             &HandleType::psf_convolution_enabled,
             "Whether the PSF and scattered light blur resolved bodies (the default). See "
             "enable_psf_convolution().")

        // Inspection
        .def(
            "psf_image",
            [](const HandleType& self,
               int radius,
               float x_offset,
               float y_offset,
               const py::object& range) {
                std::optional<units::Meter> distance;
                if (!range.is_none()) {
                    distance = detail::unit_from_py<units::Meter>(range);
                }
                py::gil_scoped_release release;
                return self.psf_image(radius, x_offset, y_offset, distance);
            },
            py::arg("radius"),
            py::arg("x_offset") = 0.f,
            py::arg("y_offset") = 0.f,
            py::arg("range") = py::none(),
            "The light an unresolved source puts in each pixel around it, per channel, as the "
            "renderer draws it: the aperture's diffraction pattern (or a PSF set in its place), "
            "with scattered light, blurred by defocus for a source at 'range' along the "
            "camera's axis (any length unit; None for a star), and integrated over each pixel, "
            "for the source (x_offset, y_offset) pixels from the center pixel's center. The "
            "image is 2 radius + 1 pixels square. Resolved bodies are convolved with it in "
            "focus.")
        .def(
            "diffraction_image",
            [](const HandleType& self, int radius, float x_offset, float y_offset) {
                py::gil_scoped_release release;
                return self.diffraction_image(radius, x_offset, y_offset);
            },
            py::arg("radius"),
            py::arg("x_offset") = 0.f,
            py::arg("y_offset") = 0.f,
            "The aperture's diffraction pattern alone, in focus, in each pixel as psf_image() "
            "gives it, whether or not it is in use.")
        .def(
            "scatter_image",
            [](const HandleType& self, int radius, float x_offset, float y_offset) {
                py::gil_scoped_release release;
                return self.scatter_image(radius, x_offset, y_offset);
            },
            py::arg("radius"),
            py::arg("x_offset") = 0.f,
            py::arg("y_offset") = 0.f,
            "The scattered light alone, in focus, in each pixel as psf_image() gives it, "
            "holding all of the light rather than its fraction. Raises without scattered "
            "light.")
        .def(
            "defocus_image",
            [](const HandleType& self,
               int radius,
               float x_offset,
               float y_offset,
               const py::object& range) {
                std::optional<units::Meter> distance;
                if (!range.is_none()) {
                    distance = detail::unit_from_py<units::Meter>(range);
                }
                return self.defocus_image(radius, x_offset, y_offset, distance);
            },
            py::arg("radius"),
            py::arg("x_offset") = 0.f,
            py::arg("y_offset") = 0.f,
            py::arg("range") = py::none(),
            "The defocus blur alone of a point at 'range' (any length unit; None for a star), "
            "in each pixel as psf_image() gives it: a uniform disc of radius "
            "defocus_blur_radius(range), or in focus the pixel the point falls in.")

        // Optics kernels
        .def("precompute",
             &HandleType::precompute,
             py::call_guard<py::gil_scoped_release>(),
             "Build everything the camera needs for the next render now (the tables of its "
             "geometry, the PSF's tables or grid, and the convolution of resolved bodies), so "
             "that the time is not spent in the render. Only what is out of date is rebuilt.")
        .def("is_precomputed",
             &HandleType::is_precomputed,
             "Whether everything the next render needs from the camera is built and up to "
             "date.")
        .def("enable_auto_precompute",
             &HandleType::enable_auto_precompute,
             py::arg("auto_precompute") = true,
             "Choose what a render does when the camera is out of date: build what it needs "
             "and log the time taken (True, the default), or raise (False), which guarantees "
             "that no render includes that time.")
        .def("auto_precompute_enabled",
             &HandleType::auto_precompute_enabled,
             "Whether a render builds an out-of-date camera itself. See "
             "enable_auto_precompute().")

        // Depth of field
        .def("enable_depth_of_field",
             &HandleType::enable_depth_of_field,
             py::arg("depth_of_field") = true,
             "Turn depth of field on (the default) or off. On, the camera has the focus set "
             "(at infinity unless set): resolved bodies are traced with rays from across the "
             "aperture, which blurs whatever is out of focus, and unresolved sources are "
             "blurred by the defocus at their depth. Off, everything is in focus, as through a "
             "pinhole. "
             "Out-of-focus bodies need more samples per pixel to look clean.")
        .def("depth_of_field_enabled",
             &HandleType::depth_of_field_enabled,
             "Whether depth of field is on (the default).")

        // Motion blur
        .def("enable_motion_blur",
             &HandleType::enable_motion_blur,
             py::arg("motion_blur") = true,
             "Turn motion blur on (the default) or off. On, a SceneView through the camera shows "
             "what moves during the exposure blurred along its path, with as many temporal "
             "samples as its motion needs, or the number it is given. Off, it renders a still "
             "frame, at the middle of the exposure, whatever number it is given.")
        .def("motion_blur_enabled",
             &HandleType::motion_blur_enabled,
             "Whether motion blur is on (the default).")

        // Focus
        .def(
            "set_focus_distance",
            [](const HandleType& self, const py::object& fd) {
                self.set_focus_distance(detail::unit_from_py<units::Meter>(fd));
            },
            py::arg("focus_distance"),
            "Focus at a distance (any distance unit). Negative values focus past infinity; "
            "infinity focuses at infinity.")
        .def(
            "set_focus_diopters",
            [](const HandleType& self, const py::object& dpts) {
                self.set_focus_diopters(detail::unit_from_py<units::Diopter>(dpts));
            },
            py::arg("diopters"),
            "Focus in diopters, the reciprocal of the focus distance (Diopter). 0 focuses at "
            "infinity; negative values focus past infinity.")
        .def(
            "set_focus_sensor_offset",
            [](const HandleType& self, const py::object& offset) {
                self.set_focus_sensor_offset(detail::unit_from_py<units::Micrometer>(offset));
            },
            py::arg("offset"),
            "Focus by moving the sensor from the infinity-focus position (any distance unit). "
            "0 focuses at infinity, positive values (sensor farther from the lens) focus in "
            "front of the camera, negative values focus past infinity. The offset, not the "
            "focus distance, is kept fixed if the focal length changes.")
        .def("focus_distance",
             &HandleType::focus_distance,
             "Distance the camera is focused at: negative past infinity, inf at infinity.")
        .def("focus_diopters",
             &HandleType::focus_diopters,
             "Focus in diopters: the reciprocal of focus_distance(), 0 at infinity.")
        .def("focus_sensor_offset",
             &HandleType::focus_sensor_offset,
             "Focus as the sensor's offset from the infinity-focus position.")
        .def(
            "defocus_blur_radius",
            [](const HandleType& self, const py::object& range) {
                std::optional<units::Meter> distance;
                if (!range.is_none()) {
                    distance = detail::unit_from_py<units::Meter>(range);
                }
                return self.defocus_blur_radius(distance);
            },
            py::arg("range") = py::none(),
            "Radius in pixel widths of the defocus blur of a point at 'range' along the "
            "camera's axis (any length unit), or of a star (None), as unresolved sources are "
            "drawn with it: 0 when it would change no pixel by more than a part in 10^5, and "
            "with depth of field off.")

        // Projection and rays
        .def(
            "project_point",
            [](const HandleType& self, const py::object& point) {
                const Pixel p = self.project_point(detail::vec3f_from_py(point));
                return py::make_tuple(p.x, p.y);
            },
            py::arg("point"),
            "Project a point in camera coordinates (meters; a huira.Vec3 or three numbers) "
            "onto the image, as a tuple (x, y) in the camera's pixel convention.")
        .def(
            "try_project_point",
            [](const HandleType& self, const py::object& point) {
                const Pixel p = self.try_project_point(detail::vec3f_from_py(point));
                return py::make_tuple(p.x, p.y);
            },
            py::arg("point"),
            "As project_point(), but (nan, nan) for a point outside the field of view.")
        .def(
            "in_fov",
            [](const HandleType& self, const py::object& point) {
                return self.in_fov(detail::vec3f_from_py(point));
            },
            py::arg("point"),
            "Whether a point in camera coordinates (a huira.Vec3 or three numbers) is in the "
            "field of view.")
        .def(
            "cast_ray",
            [](const HandleType& self, float x, float y) { return self.cast_ray(Pixel{x, y}); },
            py::arg("x"),
            py::arg("y"),
            "Cast a pinhole camera ray, in camera coordinates, through position (x, y) on the "
            "image, in the camera's pixel convention. The position may be outside the image. "
            "Raises where the lens distortion has no inverse; see try_cast_ray().")
        .def(
            "try_cast_ray",
            [](const HandleType& self, float x, float y) -> py::object {
                const std::optional<Ray<TSpectral>> ray = self.try_cast_ray(Pixel{x, y});
                return ray ? py::cast(*ray) : py::none();
            },
            py::arg("x"),
            py::arg("y"),
            "As cast_ray(), but None where the lens distortion has no inverse, rather than "
            "raising.")

        // Make the FrameBuffer
        .def("make_frame_buffer", &HandleType::make_frame_buffer)
        .def("pixel_radiance_to_power",
             &HandleType::pixel_radiance_to_power,
             py::arg("x"),
             py::arg("y"),
             "The power pixel (x, y) (column, row from the top) receives per unit of radiance, "
             "in m^2 sr: its solid angle times the aperture area projected along the direction "
             "to its center. This is the falloff the renderer gives each pixel, which "
             "flat_from_flat_field() divides out of a flat field.")

        // Blender convention
        .def("use_blender_convention",
             &HandleType::use_blender_convention,
             py::arg("value") = true,
             "Use Blender's camera convention (-z forward, y up) rather than OpenCV's (z "
             "forward, y down, the default).")
        .def("uses_blender_convention",
             &HandleType::uses_blender_convention,
             "Whether the camera uses Blender's convention (-z forward, y up).")

        .def("describe",
             &HandleType::describe,
             "A summary of the camera's settings, one per line, for reading (print(camera) "
             "shows it too). The format may change.")
        .def("__str__",
             [](const HandleType& self) {
                 return self.valid() ? self.describe()
                                     : std::string("<CameraModelHandle: invalid>");
             })

        .def("valid", &HandleType::valid)
        .def("__bool__", &HandleType::valid)
        .def("__repr__",
             [](const HandleType& self) {
                 if (!self.valid()) {
                     return std::string("<CameraModelHandle: invalid>");
                 }
                 const Resolution r = self.resolution();
                 std::ostringstream out;
                 out << std::setprecision(4)
                     << "<CameraModelHandle: " << self.focal_length().to_si() * 1e3 << " mm f/"
                     << std::setprecision(3) << self.fstop() << ", " << r.width << "x" << r.height
                     << " px>";
                 return out.str();
             })

        // Methods removed in earlier versions do not exist, so hasattr() is False for them, but
        // using one names what replaced it. (Python calls __getattr__ only for names it cannot
        // find otherwise.)
        .def("__getattr__", [](const py::object& self, const std::string& name) -> py::object {
            const auto& removed = detail::removed_camera_methods();
            const auto it = removed.find(name);
            if (it != removed.end()) {
                throw py::attribute_error("API BREAKING CHANGE: " + name + " was removed in " +
                                          it->second);
            }
            throw py::attribute_error(
                "'" + py::str(py::type::of(self).attr("__name__")).cast<std::string>() +
                "' object has no attribute '" + name + "'");
        });
}
} // namespace huira
