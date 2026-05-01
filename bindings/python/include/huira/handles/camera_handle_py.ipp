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
               std::pair<int, int> res, // <-- Removed 'const' here
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
            "defaults to pitch_x (square pixels). cx/cy default to center.")

        .def(
            "configure_sensor_from_size",
            [](HandleType& self,
               std::pair<int, int> res, // <-- Removed 'const' here
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
            "defaults to maintaining square pixels. cx/cy default to center.")

        .def(
            "set_intrinsic_matrix",
            [](HandleType& self,
               const Mat3<float>& matrix, // <-- Removed 'const' here
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
            "length anchor.")

        .def(
            "set_intrinsics",
            [](HandleType& self,
               float fx,
               float fy,
               float cx,
               float cy, // <-- Removed 'const' here
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
            "length anchor.")

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
        .def("set_sensor_gain", &HandleType::set_sensor_gain, py::arg("gain"))
        .def("set_sensor_gain_db", &HandleType::set_sensor_gain_db, py::arg("gain_db"))
        .def("set_sensor_uinty_db", &HandleType::set_sensor_uinty_db, py::arg("unity_db"))

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

        .def("enable_depth_of_field",
             &HandleType::enable_depth_of_field,
             py::arg("depth_of_field") = true)
        .def(
            "set_focus_distance",
            [](const HandleType& self, const py::object& fd) {
                self.set_focus_distance(detail::unit_from_py<units::Meter>(fd));
            },
            py::arg("focus_distance"),
            "Set the focus distance (accepts any distance unit)")
        .def("get_focus_distance", &HandleType::get_focus_distance)
        .def(
            "set_diopters",
            [](const HandleType& self, const py::object& dpts) {
                self.set_diopters(detail::unit_from_py<units::Diopter>(dpts));
            },
            py::arg("diopters"),
            "Set the camera diopter (accepts units of diopters)")
        .def("get_diopters", &HandleType::get_diopters)

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
        .def("set_sensor_resolution",
             [](HandleType& self, py::args args, py::kwargs kwargs) {
                 throw std::runtime_error(
                     "API BREAKING CHANGE: set_sensor_resolution was removed in v0.9.4. "
                     "Use configure_sensor_from_pitch() or configure_sensor_from_size() instead.");
             })
        .def("set_sensor_pixel_pitch",
             [](HandleType& self, py::args args, py::kwargs kwargs) {
                 throw std::runtime_error(
                     "API BREAKING CHANGE: set_sensor_pixel_pitch was removed in v0.9.4. "
                     "Use configure_sensor_from_pitch() instead.");
             })
        .def("set_sensor_size", [](HandleType& self, py::args args, py::kwargs kwargs) {
            throw std::runtime_error("API BREAKING CHANGE: set_sensor_size was removed in v0.9.4. "
                                     "Use configure_sensor_from_size() instead.");
        });
}
} // namespace huira
