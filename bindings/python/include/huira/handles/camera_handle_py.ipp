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

            // Sensor properties
            .def("set_sensor_quantum_efficiency", &HandleType::set_sensor_quantum_efficiency,
                py::arg("qe"), "Set quantum efficiency (as SpectralBins)")
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
            .def("use_aperture_psf", &HandleType::use_aperture_psf,
                py::arg("radius"), py::arg("banks"))
            .def("delete_psf", &HandleType::delete_psf)

            // Distortion
            .def("set_brown_conrady_distortion", &HandleType::set_brown_conrady_distortion,
                py::arg("coeffs"))
            .def("set_opencv_distortion", &HandleType::set_opencv_distortion,
                py::arg("coeffs"))
            .def("set_owen_distortion", &HandleType::set_owen_distortion,
                py::arg("coeffs"))
            .def("delete_distortion", &HandleType::delete_distortion)

            // Blender convention
            .def("use_blender_convention", &HandleType::use_blender_convention,
                py::arg("value") = true)

            .def("__repr__", [](const HandleType&) {
            return "<CameraModelHandle>";
                });
    }
}
