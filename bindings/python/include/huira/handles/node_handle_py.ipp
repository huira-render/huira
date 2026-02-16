#pragma once

#include "pybind11/pybind11.h"
#include "pybind11/stl.h"

#include "huira/handles/node_handle.hpp"
#include "huira/core/units/units_py.ipp"

namespace py = pybind11;

namespace huira {
    /**
     * @brief Injects all NodeHandle methods into an existing py::class_ binding.
     *
     * Call this on any handle whose C++ type inherits from NodeHandle<TSpectral, TNode>.
     * It adds position, velocity, rotation, angular velocity, scale, SPICE, and parent
     * access methods without requiring pybind11 inheritance.
     */
    template <typename TSpectral, typename TNode, typename PyClass>
    inline void bind_node_handle_methods(PyClass& cls) {
        using HandleType = NodeHandle<TSpectral, TNode>;

        cls
            // Position
            .def("set_position", [](const HandleType& self,
                    const py::object& x, const py::object& y, const py::object& z) {
                self.set_position(
                    detail::unit_from_py<units::Meter>(x),
                    detail::unit_from_py<units::Meter>(y),
                    detail::unit_from_py<units::Meter>(z));
            }, py::arg("x"), py::arg("y"), py::arg("z"),
                "Set position (accepts any distance unit)")
            .def("get_static_position", &HandleType::get_static_position)

            // Velocity
            .def("set_velocity", [](const HandleType& self,
                    const py::object& vx, const py::object& vy, const py::object& vz) {
                self.set_velocity(
                    detail::unit_from_py<units::MetersPerSecond>(vx),
                    detail::unit_from_py<units::MetersPerSecond>(vy),
                    detail::unit_from_py<units::MetersPerSecond>(vz));
            }, py::arg("vx"), py::arg("vy"), py::arg("vz"),
                "Set velocity (accepts any velocity unit)")
            .def("get_static_velocity", &HandleType::get_static_velocity)

            // Rotation
            .def("set_rotation", &HandleType::set_rotation, py::arg("rotation"))

            .def("set_rotation_local_to_parent",
                py::overload_cast<const Mat3<double>&>(
                    &HandleType::set_rotation_local_to_parent, py::const_),
                py::arg("matrix"))
            .def("set_rotation_local_to_parent",
                py::overload_cast<const Quaternion<double>&>(
                    &HandleType::set_rotation_local_to_parent, py::const_),
                py::arg("quaternion"))
            .def("set_rotation_local_to_parent",
                [](const HandleType& self, const Vec3<double>& axis, const py::object& angle) {
                    self.set_rotation_local_to_parent(axis, detail::unit_from_py<units::Degree>(angle));
                }, py::arg("axis"), py::arg("angle"))

            .def("set_rotation_parent_to_local",
                py::overload_cast<const Mat3<double>&>(
                    &HandleType::set_rotation_parent_to_local, py::const_),
                py::arg("matrix"))
            .def("set_rotation_parent_to_local",
                py::overload_cast<const Quaternion<double>&>(
                    &HandleType::set_rotation_parent_to_local, py::const_),
                py::arg("quaternion"))
            .def("set_rotation_parent_to_local",
                [](const HandleType& self, const Vec3<double>& axis, const py::object& angle) {
                    self.set_rotation_parent_to_local(axis, detail::unit_from_py<units::Degree>(angle));
                }, py::arg("axis"), py::arg("angle"))

            .def("set_euler_angles",
                [](const HandleType& self,
                    const py::object& x, const py::object& y, const py::object& z,
                    const std::string& sequence) {
                    self.set_euler_angles(
                        detail::unit_from_py<units::Radian>(x),
                        detail::unit_from_py<units::Radian>(y),
                        detail::unit_from_py<units::Radian>(z),
                        sequence);
                }, py::arg("x"), py::arg("y"), py::arg("z"),
                py::arg("sequence") = "XYZ")
            .def("get_static_rotation", &HandleType::get_static_rotation)

            // Angular velocity
            .def("set_angular_velocity", [](const HandleType& self,
                    const py::object& wx, const py::object& wy, const py::object& wz) {
                self.set_angular_velocity(
                    detail::unit_from_py<units::RadiansPerSecond>(wx),
                    detail::unit_from_py<units::RadiansPerSecond>(wy),
                    detail::unit_from_py<units::RadiansPerSecond>(wz));
            }, py::arg("wx"), py::arg("wy"), py::arg("wz"),
                "Set angular velocity (accepts any angular velocity unit)")
            .def("get_static_angular_velocity", &HandleType::get_static_angular_velocity)

            // Scale
            .def("set_scale",
                py::overload_cast<double, double, double>(&HandleType::set_scale, py::const_),
                py::arg("sx"), py::arg("sy"), py::arg("sz"))
            .def("set_scale",
                py::overload_cast<double>(&HandleType::set_scale, py::const_),
                py::arg("s"))
            .def("get_static_scale", &HandleType::get_static_scale)

            // SPICE
            .def("set_spice_origin", &HandleType::set_spice_origin, py::arg("spice_origin"))
            .def("set_spice_frame", &HandleType::set_spice_frame, py::arg("spice_frame"))
            .def("set_spice", &HandleType::set_spice,
                py::arg("spice_origin"), py::arg("spice_frame"))
            .def("get_spice_origin", &HandleType::get_spice_origin)
            .def("get_spice_frame", &HandleType::get_spice_frame)

            // Parent access
            .def("get_parent", &HandleType::get_parent);
    }
}
