#pragma once

#include "huira/handles/camera_handle.hpp"
#include "huira/units/units_py.ipp"
#include "pybind11/pybind11.h"
#include "pybind11/stl.h"

namespace py = pybind11;

namespace huira {

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
            "Set the focal length (accepts any distance unit)")
        .def("focal_length", &HandleType::focal_length)

        // F-stop
        .def("set_fstop", &HandleType::set_fstop, py::arg("fstop"))
        .def("fstop", &HandleType::fstop)

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
               const Mat3<float>& matrix,
               std::pair<int, int> res,
               const py::object& anchor_fl) {
                self.set_intrinsic_matrix(matrix,
                                          Resolution{res.first, res.second},
                                          detail::unit_from_py<units::Millimeter>(anchor_fl));
            },
            py::arg("intrinsic_matrix"),
            py::arg("resolution"),
            py::arg("anchor_focal_length"),
            "Explicitly set the 3x3 intrinsic matrix with a resolution tuple and physical focal "
            "length anchor. The principal point is in the camera's pixel convention (see "
            "set_pixel_convention).")

        .def(
            "set_intrinsics",
            [](HandleType& self,
               float fx,
               float fy,
               float cx,
               float cy,
               std::pair<int, int> res,
               const py::object& anchor_fl) {
                self.set_intrinsics(fx,
                                    fy,
                                    cx,
                                    cy,
                                    Resolution{res.first, res.second},
                                    detail::unit_from_py<units::Millimeter>(anchor_fl));
            },
            py::arg("fx"),
            py::arg("fy"),
            py::arg("cx"),
            py::arg("cy"),
            py::arg("resolution"),
            py::arg("anchor_focal_length"),
            "Explicitly set mathematical intrinsics with a resolution tuple and physical focal "
            "length anchor. cx/cy are in the camera's pixel convention (see "
            "set_pixel_convention).")

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
        .def("set_sensor_simulate_noise",
             &HandleType::set_sensor_simulate_noise,
             py::arg("simulate_noise"))
        .def("set_sensor_read_noise", &HandleType::set_sensor_read_noise, py::arg("read_noise"))
        .def("set_sensor_dark_current",
             &HandleType::set_sensor_dark_current,
             py::arg("dark_current"))
        .def("set_sensor_bias_level", &HandleType::set_sensor_bias_level, py::arg("bias_level"))
        .def("set_sensor_bit_depth", &HandleType::set_sensor_bit_depth, py::arg("bit_depth"))
        .def("set_sensor_conversion_gain", &HandleType::set_sensor_conversion_gain, py::arg("gain"))
        .def("set_sensor_gain_db", &HandleType::set_sensor_gain_db, py::arg("gain_db"))
        .def("set_sensor_unity_db", &HandleType::set_sensor_unity_db, py::arg("unity_db"))

        // Sensor rotation
        .def(
            "set_sensor_rotation",
            [](const HandleType& self, const py::object& angle) {
                self.set_sensor_rotation(detail::unit_from_py<units::Radian>(angle));
            },
            py::arg("angle"),
            "Set sensor rotation (accepts any angle unit, e.g. Radian, Degree)")

        // PSF
        .def("use_aperture_psf",
             py::overload_cast<bool>(&HandleType::use_aperture_psf, py::const_),
             py::arg("value"))
        .def("use_aperture_psf",
             py::overload_cast<int, int>(&HandleType::use_aperture_psf, py::const_),
             py::arg("radius") = 64,
             py::arg("banks") = 16)
        .def("enable_psf_convolution",
             &HandleType::enable_psf_convolution,
             py::arg("convolve_psf") = true)
        .def("delete_psf", &HandleType::delete_psf)
        .def("set_psf_convolution_radius",
             &HandleType::set_psf_convolution_radius,
             py::arg("radius"),
             "Set the radius (pixels) of the whole-image PSF convolution kernel, independent "
             "of the polyphase stamping radius. Large frame-wide radii are supported.")
        .def("set_measured_psf",
             &HandleType::set_measured_psf,
             py::arg("data"),
             py::arg("samples_per_pixel"),
             py::arg("radius") = 0,
             py::arg("banks") = 16,
             "Use a measured (user-supplied) PSF as the camera's core PSF. 'data' is an Image "
             "of centered PSF samples; 'samples_per_pixel' is the measurement sampling density "
             "per sensor pixel per axis. radius=0 auto-selects the largest stamping radius "
             "covered by the measurement (capped at 64).")

        // Stray light
        .def("set_veiling_glare",
             &HandleType::set_veiling_glare,
             py::arg("alpha"),
             "Redistribute the given fraction of total collected energy uniformly across the "
             "image (veiling glare).")
        .def("disable_veiling_glare", &HandleType::disable_veiling_glare)
        .def("set_harvey_shack_scatter",
             &HandleType::set_harvey_shack_scatter,
             py::arg("scatter_fraction"),
             py::arg("falloff_exponent"),
             py::arg("r0") = 0.5f,
             py::arg("radius") = 0.f,
             "Add Harvey-Shack scattered-light wings to the total system PSF: "
             "'scatter_fraction' of the energy follows a power-law halo with the given "
             "falloff exponent (typically 2-3), shoulder radius r0 (pixels), and optional "
             "hard cutoff radius (0 = none).")
        .def("disable_harvey_shack_scatter", &HandleType::disable_harvey_shack_scatter)

        // Optics kernels
        .def("precompute",
             &HandleType::precompute,
             py::call_guard<py::gil_scoped_release>(),
             "Build everything the camera's optics need for the next render (PSF and defocus "
             "stamps, convolution kernels and their spectra) now, so that the time is not spent "
             "in the render. Only what is out of date is rebuilt.")
        .def("is_precomputed",
             &HandleType::is_precomputed,
             "Whether everything the next render needs from the camera's optics is built and "
             "up to date.")
        .def("set_auto_precompute",
             &HandleType::set_auto_precompute,
             py::arg("auto_precompute") = true,
             "Choose what a render does when the camera's optics are out of date: build them "
             "and log the time taken (True, the default), or raise (False), which guarantees "
             "that no render includes that time.")
        .def("auto_precompute",
             &HandleType::auto_precompute,
             "Whether a render builds out-of-date optics itself. See set_auto_precompute().")

        // Depth of field
        .def("enable_depth_of_field",
             &HandleType::enable_depth_of_field,
             py::arg("depth_of_field") = true)

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
            "Focus by vergence, the reciprocal of the focus distance (Diopter). 0 focuses at "
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
             "Focus as a vergence: the reciprocal of focus_distance(), 0 at infinity.")
        .def("focus_sensor_offset",
             &HandleType::focus_sensor_offset,
             "Focus as the sensor's offset from the infinity-focus position.")
        .def("defocus_blur_radius",
             &HandleType::defocus_blur_radius,
             "Radius in pixels of the defocus blur applied to unresolved sources (0 if in "
             "focus).")

        // Make the FrameBuffer
        .def("make_frame_buffer", &HandleType::make_frame_buffer)

        // Blender convention
        .def("use_blender_convention", &HandleType::use_blender_convention, py::arg("value") = true)

        .def("valid", &HandleType::valid)
        .def("__bool__", &HandleType::valid)
        .def("__repr__", [](const HandleType&) { return "<CameraModelHandle>"; })

        // ========================== //
        // === DEPRECATED METHODS === //
        // ========================== //
        .def(
            "set_diopters",
            [](const HandleType& self, const py::object& dpts) {
                throw std::runtime_error("API BREAKING CHANGE: set_diopters was removed in "
                                         "v0.9.10. Use set_focus_diopters() instead.");
            },
            py::arg("diopters"))
        .def("get_diopters",
             [](const HandleType& self) {
                 throw std::runtime_error("API BREAKING CHANGE: get_diopters was removed in "
                                          "v0.9.10. Use focus_diopters() instead.");
             })
        .def("get_focus_distance",
             [](const HandleType& self) {
                 throw std::runtime_error("API BREAKING CHANGE: get_focus_distance was removed in "
                                          "v0.9.10. Use focus_distance() instead.");
             })
        .def("set_sensor_resolution",
             [](HandleType&, py::args, py::kwargs) {
                 throw std::runtime_error(
                     "API BREAKING CHANGE: set_sensor_resolution was removed in v0.9.4. "
                     "Use configure_sensor_from_pitch() or configure_sensor_from_size() instead.");
             })
        .def("set_sensor_pixel_pitch",
             [](HandleType&, py::args, py::kwargs) {
                 throw std::runtime_error(
                     "API BREAKING CHANGE: set_sensor_pixel_pitch was removed in v0.9.4. "
                     "Use configure_sensor_from_pitch() instead.");
             })
        .def("set_sensor_size", [](HandleType&, py::args, py::kwargs) {
            throw std::runtime_error("API BREAKING CHANGE: set_sensor_size was removed in v0.9.4. "
                                     "Use configure_sensor_from_size() instead.");
        });
}
} // namespace huira
