#pragma once

#include "pybind11/pybind11.h"
#include "pybind11/stl.h"

#include "huira/handles/camera_handle.hpp"
#include "huira/core/units/units_py.ipp"

namespace py = pybind11;

namespace huira {
    template <typename TSpectral>
    inline void bind_camera_model_handle(py::module_& m) {
        using HandleType = CameraModelHandle<TSpectral>;

        py::class_<HandleType>(m, "CameraModelHandle")
            // Focal length
            .def("set_focal_length", [](const HandleType& self, const py::object& fl) {
            self.set_focal_length(detail::unit_from_py<units::Millimeter>(fl));
                }, py::arg("focal_length"),
                    "Set the focal length (accepts any distance unit)")
            .def("focal_length", &HandleType::focal_length)

            // F-stop
            .def("set_fstop", &HandleType::set_fstop, py::arg("fstop"))
            .def("fstop", &HandleType::fstop)

            // Sensor resolution
            .def("set_sensor_resolution",
                py::overload_cast<int, int>(&HandleType::set_sensor_resolution, py::const_),
                py::arg("width"), py::arg("height"))

            // Sensor pixel pitch
            .def("set_sensor_pixel_pitch",
                [](const HandleType& self, const py::object& px, const py::object& py_) {
                    self.set_sensor_pixel_pitch(
                        detail::unit_from_py<units::Millimeter>(px),
                        detail::unit_from_py<units::Millimeter>(py_));
                }, py::arg("pixel_pitch_x"), py::arg("pixel_pitch_y"))
            .def("set_sensor_pixel_pitch",
                [](const HandleType& self, const py::object& pp) {
                    self.set_sensor_pixel_pitch(detail::unit_from_py<units::Millimeter>(pp));
                }, py::arg("pixel_pitch"))

            // Sensor size
            .def("set_sensor_size",
                [](const HandleType& self, const py::object& w, const py::object& h) {
                    self.set_sensor_size(
                        detail::unit_from_py<units::Millimeter>(w),
                        detail::unit_from_py<units::Millimeter>(h));
                }, py::arg("width"), py::arg("height"))
            .def("set_sensor_size",
                [](const HandleType& self, const py::object& w) {
                    self.set_sensor_size(detail::unit_from_py<units::Millimeter>(w));
                }, py::arg("width"))

            // Distortion
            .def("set_brown_conrady_distortion", &HandleType::set_brown_conrady_distortion,
                py::arg("coeffs"))
            .def("set_opencv_distortion", &HandleType::set_opencv_distortion,
                py::arg("coeffs"))
            .def("set_owen_distortion", &HandleType::set_owen_distortion,
                py::arg("coeffs"))
            .def("delete_distortion", &HandleType::delete_distortion)

            // Sensor properties
            .def("set_sensor_quantum_efficiency",
                py::overload_cast<double>(&HandleType::set_sensor_quantum_efficiency, py::const_),
                py::arg("qe"), "Set quantum efficiency (scalar, e.g. 0.7)")
            .def("set_sensor_quantum_efficiency",
                py::overload_cast<TSpectral>(&HandleType::set_sensor_quantum_efficiency, py::const_),
                py::arg("qe"), "Set quantum efficiency (spectral)")
            .def("set_sensor_full_well_capacity", &HandleType::set_sensor_full_well_capacity,
                py::arg("fwc"))
            .def("set_sensor_read_noise", &HandleType::set_sensor_read_noise,
                py::arg("read_noise"))
            .def("set_sensor_dark_current", &HandleType::set_sensor_dark_current,
                py::arg("dark_current"))
            .def("set_sensor_bias_level", &HandleType::set_sensor_bias_level,
                py::arg("bias_level"))
            .def("set_sensor_bit_depth", &HandleType::set_sensor_bit_depth,
                py::arg("bit_depth"))
            .def("set_sensor_gain", &HandleType::set_sensor_gain,
                py::arg("gain"))
            .def("set_sensor_gain_db", &HandleType::set_sensor_gain_db,
                py::arg("gain_db"))
            .def("set_sensor_uinty_db", &HandleType::set_sensor_uinty_db,
                py::arg("unity_db"))

            // Sensor rotation
            .def("set_sensor_rotation", [](const HandleType& self, const py::object& angle) {
            self.set_sensor_rotation(detail::unit_from_py<units::Radian>(angle));
                }, py::arg("angle"),
                    "Set sensor rotation (accepts any angle unit, e.g. Radian, Degree)")

            // PSF
            .def("use_aperture_psf",
                py::overload_cast<bool>(&HandleType::use_aperture_psf, py::const_),
                py::arg("value"))
            .def("use_aperture_psf",
                py::overload_cast<int, int>(&HandleType::use_aperture_psf, py::const_),
                py::arg("radius") = 64, py::arg("banks") = 16)
            .def("enable_psf_convolution", &HandleType::enable_psf_convolution,
                py::arg("convolve_psf") = true)
            .def("delete_psf", &HandleType::delete_psf)

            .def("enable_depth_of_field", &HandleType::enable_depth_of_field,
                py::arg("depth_of_field") = true)
            .def("set_focus_distance", [](const HandleType& self, const py::object& fd) {
            self.set_focus_distance(detail::unit_from_py<units::Meter>(fd));
                }, py::arg("focus_distance"),
                    "Set the focus distance (accepts any distance unit)")
            .def("get_focus_distance", &HandleType::get_focus_distance)
            .def("set_diopters", [](const HandleType& self, const py::object& dpts) {
            self.set_diopters(detail::unit_from_py<units::Diopter>(dpts));
                }, py::arg("diopters"),
                    "Set the camera diopter (accepts units of diopters)")
            .def("get_diopters", &HandleType::get_diopters)

            // Make the FrameBuffer
            .def("make_frame_buffer", &HandleType::make_frame_buffer)

            // Blender convention
            .def("use_blender_convention", &HandleType::use_blender_convention,
                py::arg("value") = true)

            .def("valid", &HandleType::valid)
            .def("__bool__", &HandleType::valid)
            .def("__repr__", [](const HandleType&) {
                return "<CameraModelHandle>";
            });
    }
}
