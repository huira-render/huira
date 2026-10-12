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
            "Set the intrinsics from a huira.Mat3 camera matrix [[fx, s, cx], [0, fy, cy], "
            "[0, 0, 1]] (up to scale), with a resolution tuple and physical focal length "
            "anchor. The principal point is in the camera's pixel convention (see "
            "set_pixel_convention).")
        .def(
            "set_intrinsic_matrix",
            [](HandleType& self,
               py::array_t<double, py::array::c_style | py::array::forcecast> matrix,
               std::pair<int, int> res,
               const py::object& anchor_fl) {
                const auto buf = matrix.request();
                if (buf.ndim != 2 || buf.shape[0] != 3 || buf.shape[1] != 3) {
                    throw std::runtime_error("set_intrinsic_matrix - Expected a 3x3 matrix");
                }
                // NumPy is row-major, and GLM column-major: K[row][col] -> k[col][row].
                const auto* values = static_cast<const double*>(buf.ptr);
                Mat3<float> k;
                for (int row = 0; row < 3; ++row) {
                    for (int col = 0; col < 3; ++col) {
                        k[col][row] = static_cast<float>(values[row * 3 + col]);
                    }
                }
                self.set_intrinsic_matrix(k,
                                          Resolution{res.first, res.second},
                                          detail::unit_from_py<units::Millimeter>(anchor_fl));
            },
            py::arg("intrinsic_matrix"),
            py::arg("resolution"),
            py::arg("anchor_focal_length"),
            "Set the intrinsics from a 3x3 camera matrix, as a NumPy array or nested list "
            "written row by row: [[fx, s, cx], [0, fy, cy], [0, 0, 1]] (up to scale), where s "
            "is the skew. The principal point is in the camera's pixel convention (see "
            "set_pixel_convention).")

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
            [](const HandleType& self) {
                // GLM is column-major, K[col][row]; NumPy is row-major.
                const Mat3<float> k = self.intrinsic_matrix();
                py::array_t<double> matrix({3, 3});
                auto values = matrix.mutable_unchecked<2>();
                for (py::ssize_t row = 0; row < 3; ++row) {
                    for (py::ssize_t col = 0; col < 3; ++col) {
                        values(row, col) =
                            static_cast<double>(k[static_cast<int>(col)][static_cast<int>(row)]);
                    }
                }
                return matrix;
            },
            "The camera matrix [[fx, s, cx], [0, fy, cy], [0, 0, 1]] as a 3x3 NumPy array, "
            "row by row, as set_intrinsic_matrix() takes it: in pixels, with the principal "
            "point in the camera's pixel convention (see set_pixel_convention).")
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
             py::arg("fwc"))
        .def("enable_sensor_noise",
             &HandleType::enable_sensor_noise,
             py::arg("noise") = true,
             "Turn the sensor's shot noise and read noise on (the default) or off. Off, each "
             "pixel collects the expected number of electrons; the dark current's electrons and "
             "the bias are still added, since they are not noise.")
        .def("set_sensor_read_noise", &HandleType::set_sensor_read_noise, py::arg("read_noise"))
        .def("set_sensor_dark_current",
             &HandleType::set_sensor_dark_current,
             py::arg("dark_current"))
        .def("set_sensor_bias_level", &HandleType::set_sensor_bias_level, py::arg("bias_level"))
        .def("set_sensor_bit_depth", &HandleType::set_sensor_bit_depth, py::arg("bit_depth"))
        .def("set_sensor_conversion_gain", &HandleType::set_sensor_conversion_gain, py::arg("gain"))
        .def("set_sensor_gain_db", &HandleType::set_sensor_gain_db, py::arg("gain_db"))
        .def("set_sensor_unity_db", &HandleType::set_sensor_unity_db, py::arg("unity_db"))
        .def("set_sensor_noise_seed",
             &HandleType::set_sensor_noise_seed,
             py::arg("seed"),
             "Seed the sensor's noise. Setting the same seed again repeats the same sequence of "
             "frames; sensors get distinct seeds by default.")
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
             "Whether the sensor's shot noise and read noise are on (the default). See "
             "enable_sensor_noise().")
        .def("sensor_read_noise",
             &HandleType::sensor_read_noise,
             "The sensor's read noise, in electrons.")
        .def("sensor_dark_current",
             &HandleType::sensor_dark_current,
             "The sensor's dark current, in electrons per second.")
        .def("sensor_bias_level",
             &HandleType::sensor_bias_level,
             "The sensor's bias level, in digital numbers.")
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
             &HandleType::set_measured_psf,
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
