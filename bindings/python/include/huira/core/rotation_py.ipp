#pragma once

#include <string>

#include "huira/core/rotation.hpp"
#include "huira/core/types_py.ipp"
#include "huira/units/units_py.ipp"
#include "pybind11/numpy.h"
#include "pybind11/operators.h"
#include "pybind11/pybind11.h"
#include "pybind11/stl.h"

namespace py = pybind11;

namespace huira {

/**
 * @brief Bind Rotation<double> to Python.
 *
 * Its factory methods (matrix, quaternion, axis-angle, Euler, basis vectors) take a Mat3 or Vec3,
 * or, as everywhere (see bind_types()), any list, tuple or numpy array of that shape.
 */
inline void bind_rotation(py::module_& m)
{
    using Rot = Rotation<double>;

    auto cls = py::class_<Rot>(m,
                               "Rotation",
                               "3D rotation (double precision). "
                               "Internally stored as a 3x3 orthonormal matrix.");
    cls

        // ---- Default constructor (identity) ----
        .def(py::init<>(), "Construct an identity rotation")

        // ==============================================================
        // from_local_to_parent factories
        // ==============================================================
        .def_static("from_local_to_parent",
                    py::overload_cast<Mat3<double>>(&Rot::from_local_to_parent),
                    py::arg("matrix"),
                    "Create from a local-to-parent rotation matrix: a Mat3, or a 3x3 array or "
                    "nested list, row by row")
        .def_static("from_local_to_parent",
                    py::overload_cast<Quaternion<double>>(&Rot::from_local_to_parent),
                    py::arg("quaternion"),
                    "Create from a local-to-parent Hamilton quaternion")
        .def_static("from_local_to_parent",
                    py::overload_cast<ShusterQuaternion<double>>(&Rot::from_local_to_parent),
                    py::arg("shuster_quaternion"),
                    "Create from a local-to-parent Shuster quaternion")
        .def_static(
            "from_local_to_parent",
            [](const Vec3<double>& axis, const py::object& angle) {
                return Rot::from_local_to_parent(axis, detail::unit_from_py<units::Radian>(angle));
            },
            py::arg("axis"),
            py::arg("angle"),
            "Create from an axis (a Vec3, or any sequence or array of three numbers) and an angle "
            "(local-to-parent)")

        // ==============================================================
        // from_parent_to_local factories
        // ==============================================================
        .def_static("from_parent_to_local",
                    py::overload_cast<Mat3<double>>(&Rot::from_parent_to_local),
                    py::arg("matrix"),
                    "Create from a parent-to-local rotation matrix: a Mat3, or a 3x3 array or "
                    "nested list, row by row")
        .def_static("from_parent_to_local",
                    py::overload_cast<Quaternion<double>>(&Rot::from_parent_to_local),
                    py::arg("quaternion"),
                    "Create from a parent-to-local Hamilton quaternion")
        .def_static("from_parent_to_local",
                    py::overload_cast<ShusterQuaternion<double>>(&Rot::from_parent_to_local),
                    py::arg("shuster_quaternion"),
                    "Create from a parent-to-local Shuster quaternion")
        .def_static(
            "from_parent_to_local",
            [](const Vec3<double>& axis, const py::object& angle) {
                return Rot::from_parent_to_local(axis, detail::unit_from_py<units::Radian>(angle));
            },
            py::arg("axis"),
            py::arg("angle"),
            "Create from an axis (a Vec3, or any sequence or array of three numbers) and an angle "
            "(parent-to-local)")

        // ==============================================================
        // Euler angle factories
        // ==============================================================
        .def_static(
            "extrinsic_euler_angles",
            [](const py::object& a1,
               const py::object& a2,
               const py::object& a3,
               const std::string& seq) {
                return Rot::extrinsic_euler_angles(detail::unit_from_py<units::Radian>(a1),
                                                   detail::unit_from_py<units::Radian>(a2),
                                                   detail::unit_from_py<units::Radian>(a3),
                                                   seq);
            },
            py::arg("angle1"),
            py::arg("angle2"),
            py::arg("angle3"),
            py::arg("sequence") = "XYZ",
            "Create from extrinsic Euler angles (accepts any angle unit)")
        .def_static(
            "intrinsic_euler_angles",
            [](const py::object& a1,
               const py::object& a2,
               const py::object& a3,
               const std::string& seq) {
                return Rot::intrinsic_euler_angles(detail::unit_from_py<units::Radian>(a1),
                                                   detail::unit_from_py<units::Radian>(a2),
                                                   detail::unit_from_py<units::Radian>(a3),
                                                   seq);
            },
            py::arg("angle1"),
            py::arg("angle2"),
            py::arg("angle3"),
            py::arg("sequence") = "XYZ",
            "Create from intrinsic Euler angles (accepts any angle unit)")

        // ==============================================================
        // Basis-vector factory
        // ==============================================================
        .def_static("from_basis_vectors",
                    &Rot::from_basis_vectors,
                    py::arg("x_axis"),
                    py::arg("y_axis"),
                    py::arg("z_axis"),
                    "Create from three orthonormal basis vectors, each a Vec3 or any sequence or "
                    "array of three numbers")

        // ==============================================================
        // Conversion / query methods
        // ==============================================================
        .def("inverse", &Rot::inverse)
        .def("local_to_parent_quaternion", &Rot::local_to_parent_quaternion)
        .def("local_to_parent_shuster_quaternion", &Rot::local_to_parent_shuster_quaternion)
        .def("parent_to_local_quaternion", &Rot::parent_to_local_quaternion)
        .def("parent_to_local_shuster_quaternion", &Rot::parent_to_local_shuster_quaternion)
        .def("local_to_parent_matrix", &Rot::local_to_parent_matrix)
        .def("parent_to_local_matrix", &Rot::parent_to_local_matrix)

        // NumPy convenience for matrices:
        .def(
            "local_to_parent_numpy",
            [](const Rot& r) { return detail::mat3_to_numpy(r.local_to_parent_matrix()); },
            "Return the local-to-parent matrix as a 3x3 numpy array")
        .def(
            "parent_to_local_numpy",
            [](const Rot& r) { return detail::mat3_to_numpy(r.parent_to_local_matrix()); },
            "Return the parent-to-local matrix as a 3x3 numpy array")

        // Axis accessors
        .def("x_axis", &Rot::x_axis)
        .def("y_axis", &Rot::y_axis)
        .def("z_axis", &Rot::z_axis)

        // ==============================================================
        // Operators
        // ==============================================================
        .def(
            "__mul__", [](const Rot& a, const Rot& b) { return a * b; }, py::is_operator())
        .def(
            "__imul__",
            [](Rot& a, const Rot& b) {
                a *= b;
                return a;
            },
            py::is_operator())
        .def(
            "__mul__", [](const Rot& r, const Vec3<double>& v) { return r * v; }, py::is_operator())

        // Apply the rotation to a vector, as rotation * vector does:
        .def(
            "apply",
            [](const Rot& r, const Vec3<double>& v) { return r * v; },
            py::arg("vector"),
            "Apply this rotation to a vector (a Vec3, or any sequence or array of three "
            "numbers), giving a Vec3")

        .def("__repr__", &Rot::to_string);

    // Removed in v0.9.11: what these did, from_local_to_parent() and from_parent_to_local() now
    // do, taking any form of matrix or axis. Calling one says what to use instead.
    const auto removed = [&cls](const char* name, const char* instead) {
        const std::string message = std::string("API BREAKING CHANGE: Rotation.") + name +
                                    " was removed in v0.9.11. Use " + instead + " instead.";
        cls.def_static(
            name,
            [message](const py::args&, const py::kwargs&) -> Rot {
                throw py::attribute_error(message);
            },
            ("Removed in v0.9.11: use " + std::string(instead) + ".").c_str());
    };
    removed("from_local_to_parent_matrix", "Rotation.from_local_to_parent(matrix)");
    removed("from_local_to_parent_axis_angle", "Rotation.from_local_to_parent(axis, angle)");
    removed("from_parent_to_local_matrix", "Rotation.from_parent_to_local(matrix)");
    removed("from_parent_to_local_axis_angle", "Rotation.from_parent_to_local(axis, angle)");
}

} // namespace huira
