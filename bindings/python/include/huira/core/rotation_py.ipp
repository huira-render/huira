#pragma once

#include "pybind11/pybind11.h"
#include "pybind11/numpy.h"
#include "pybind11/stl.h"
#include "pybind11/operators.h"

#include "huira/core/rotation.hpp"
#include "huira/core/units/units_py.ipp"

namespace py = pybind11;

namespace huira {

    /**
     * @brief Bind Rotation<double> to Python.
     *
     * Provides all factory methods (matrix, quaternion, axis-angle, Euler, basis vectors)
     * with numpy array overloads so users can pass raw arrays directly.
     */
    inline void bind_rotation(py::module_& m) {
        using Rot = Rotation<double>;

        py::class_<Rot>(m, "Rotation",
            "3D rotation (double precision). "
            "Internally stored as a 3x3 orthonormal matrix.")

            // ---- Default constructor (identity) ----
            .def(py::init<>(), "Construct an identity rotation")

            // ==============================================================
            // from_local_to_parent factories
            // ==============================================================
            .def_static("from_local_to_parent",
                py::overload_cast<Mat3<double>>(&Rot::from_local_to_parent),
                py::arg("matrix"),
                "Create from a local-to-parent rotation matrix (Mat3)")
            .def_static("from_local_to_parent",
                py::overload_cast<Quaternion<double>>(&Rot::from_local_to_parent),
                py::arg("quaternion"),
                "Create from a local-to-parent Hamilton quaternion")
            .def_static("from_local_to_parent",
                py::overload_cast<ShusterQuaternion<double>>(&Rot::from_local_to_parent),
                py::arg("shuster_quaternion"),
                "Create from a local-to-parent Shuster quaternion")
            .def_static("from_local_to_parent",
                [](const Vec3<double>& axis, const py::object& angle) {
                    return Rot::from_local_to_parent(axis, detail::unit_from_py<units::Radian>(angle));
                },
                py::arg("axis"), py::arg("angle"),
                "Create from axis + angle (local-to-parent)")

            // NumPy overloads for from_local_to_parent:
            .def_static("from_local_to_parent_matrix",
                [](py::array_t<double, py::array::c_style | py::array::forcecast> arr) {
                    auto buf = arr.request();
                    if (buf.ndim != 2 || buf.shape[0] != 3 || buf.shape[1] != 3)
                        throw std::runtime_error("Expected a 3x3 numpy array");
                    auto* ptr = static_cast<double*>(buf.ptr);
                    Mat3<double> mat;
                    for (int row = 0; row < 3; ++row)
                        for (int col = 0; col < 3; ++col)
                            mat[col][row] = ptr[row * 3 + col];
                    return Rot::from_local_to_parent(mat);
                },
                py::arg("matrix"),
                "Create from a 3x3 numpy array (row-major, local-to-parent)")
            .def_static("from_local_to_parent_axis_angle",
                [](py::array_t<double> axis_arr, const py::object& angle) {
                    auto buf = axis_arr.request();
                    if (buf.ndim != 1 || buf.shape[0] != 3)
                        throw std::runtime_error("Axis must be a 1-D array of length 3");
                    auto* ptr = static_cast<double*>(buf.ptr);
                    Vec3<double> axis(ptr[0], ptr[1], ptr[2]);
                    return Rot::from_local_to_parent(axis, detail::unit_from_py<units::Radian>(angle));
                },
                py::arg("axis"), py::arg("angle"),
                "Create from a numpy axis [3] + angle (local-to-parent)")

            // ==============================================================
            // from_parent_to_local factories
            // ==============================================================
            .def_static("from_parent_to_local",
                py::overload_cast<Mat3<double>>(&Rot::from_parent_to_local),
                py::arg("matrix"),
                "Create from a parent-to-local rotation matrix (Mat3)")
            .def_static("from_parent_to_local",
                py::overload_cast<Quaternion<double>>(&Rot::from_parent_to_local),
                py::arg("quaternion"),
                "Create from a parent-to-local Hamilton quaternion")
            .def_static("from_parent_to_local",
                py::overload_cast<ShusterQuaternion<double>>(&Rot::from_parent_to_local),
                py::arg("shuster_quaternion"),
                "Create from a parent-to-local Shuster quaternion")
            .def_static("from_parent_to_local",
                [](const Vec3<double>& axis, const py::object& angle) {
                    return Rot::from_parent_to_local(axis, detail::unit_from_py<units::Radian>(angle));
                },
                py::arg("axis"), py::arg("angle"),
                "Create from axis + angle (parent-to-local)")

            // NumPy overloads for from_parent_to_local:
            .def_static("from_parent_to_local_matrix",
                [](py::array_t<double, py::array::c_style | py::array::forcecast> arr) {
                    auto buf = arr.request();
                    if (buf.ndim != 2 || buf.shape[0] != 3 || buf.shape[1] != 3)
                        throw std::runtime_error("Expected a 3x3 numpy array");
                    auto* ptr = static_cast<double*>(buf.ptr);
                    Mat3<double> mat;
                    for (int row = 0; row < 3; ++row)
                        for (int col = 0; col < 3; ++col)
                            mat[col][row] = ptr[row * 3 + col];
                    return Rot::from_parent_to_local(mat);
                },
                py::arg("matrix"),
                "Create from a 3x3 numpy array (row-major, parent-to-local)")
            .def_static("from_parent_to_local_axis_angle",
                [](py::array_t<double> axis_arr, const py::object& angle) {
                    auto buf = axis_arr.request();
                    if (buf.ndim != 1 || buf.shape[0] != 3)
                        throw std::runtime_error("Axis must be a 1-D array of length 3");
                    auto* ptr = static_cast<double*>(buf.ptr);
                    Vec3<double> axis(ptr[0], ptr[1], ptr[2]);
                    return Rot::from_parent_to_local(axis, detail::unit_from_py<units::Radian>(angle));
                },
                py::arg("axis"), py::arg("angle"),
                "Create from a numpy axis [3] + angle (parent-to-local)")

            // ==============================================================
            // Euler angle factories
            // ==============================================================
            .def_static("extrinsic_euler_angles",
                [](const py::object& a1, const py::object& a2, const py::object& a3,
                   const std::string& seq) {
                    return Rot::extrinsic_euler_angles(
                        detail::unit_from_py<units::Radian>(a1),
                        detail::unit_from_py<units::Radian>(a2),
                        detail::unit_from_py<units::Radian>(a3),
                        seq);
                },
                py::arg("angle1"), py::arg("angle2"), py::arg("angle3"),
                py::arg("sequence") = "XYZ",
                "Create from extrinsic Euler angles (accepts any angle unit)")
            .def_static("intrinsic_euler_angles",
                [](const py::object& a1, const py::object& a2, const py::object& a3,
                   const std::string& seq) {
                    return Rot::intrinsic_euler_angles(
                        detail::unit_from_py<units::Radian>(a1),
                        detail::unit_from_py<units::Radian>(a2),
                        detail::unit_from_py<units::Radian>(a3),
                        seq);
                },
                py::arg("angle1"), py::arg("angle2"), py::arg("angle3"),
                py::arg("sequence") = "XYZ",
                "Create from intrinsic Euler angles (accepts any angle unit)")

            // ==============================================================
            // Basis-vector factory
            // ==============================================================
            .def_static("from_basis_vectors",
                &Rot::from_basis_vectors,
                py::arg("x_axis"), py::arg("y_axis"), py::arg("z_axis"),
                "Create from three orthonormal basis vectors (Vec3)")
            .def_static("from_basis_vectors",
                [](py::array_t<double> x_arr,
                   py::array_t<double> y_arr,
                   py::array_t<double> z_arr) {
                    auto parse = [](py::array_t<double>& a, const char* name) {
                        auto buf = a.request();
                        if (buf.ndim != 1 || buf.shape[0] != 3)
                            throw std::runtime_error(
                                std::string(name) + " must be a 1-D array of length 3");
                        auto* p = static_cast<double*>(buf.ptr);
                        return Vec3<double>(p[0], p[1], p[2]);
                    };
                    return Rot::from_basis_vectors(
                        parse(x_arr, "x_axis"),
                        parse(y_arr, "y_axis"),
                        parse(z_arr, "z_axis"));
                },
                py::arg("x_axis"), py::arg("y_axis"), py::arg("z_axis"),
                "Create from three numpy basis-vector arrays of length 3")

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
            .def("local_to_parent_numpy", [](const Rot& r) {
                Mat3<double> mat = r.local_to_parent_matrix();
                py::array_t<double> arr({3, 3});
                auto buf = arr.mutable_unchecked<2>();
                for (int row = 0; row < 3; ++row)
                    for (int col = 0; col < 3; ++col)
                        buf(row, col) = mat[col][row];
                return arr;
            }, "Return the local-to-parent matrix as a 3x3 numpy array")
            .def("parent_to_local_numpy", [](const Rot& r) {
                Mat3<double> mat = r.parent_to_local_matrix();
                py::array_t<double> arr({3, 3});
                auto buf = arr.mutable_unchecked<2>();
                for (int row = 0; row < 3; ++row)
                    for (int col = 0; col < 3; ++col)
                        buf(row, col) = mat[col][row];
                return arr;
            }, "Return the parent-to-local matrix as a 3x3 numpy array")

            // Axis accessors
            .def("x_axis", &Rot::x_axis)
            .def("y_axis", &Rot::y_axis)
            .def("z_axis", &Rot::z_axis)

            // ==============================================================
            // Operators
            // ==============================================================
            .def("__mul__", [](const Rot& a, const Rot& b) { return a * b; },
                py::is_operator())
            .def("__imul__", [](Rot& a, const Rot& b) { a *= b; return a; },
                py::is_operator())
            .def("__mul__", [](const Rot& r, const Vec3<double>& v) { return r * v; },
                py::is_operator())

            // Apply rotation to a numpy vector:
            .def("apply", [](const Rot& r, py::array_t<double> arr) {
                auto buf = arr.request();
                if (buf.ndim != 1 || buf.shape[0] != 3)
                    throw std::runtime_error("Expected a 1-D array of length 3");
                auto* ptr = static_cast<double*>(buf.ptr);
                Vec3<double> v(ptr[0], ptr[1], ptr[2]);
                Vec3<double> result = r * v;
                py::array_t<double> out(3);
                auto obuf = out.mutable_unchecked<1>();
                obuf(0) = result.x; obuf(1) = result.y; obuf(2) = result.z;
                return out;
            }, py::arg("vector"),
                "Apply this rotation to a numpy vector [3], returns numpy array")

            .def("__repr__", &Rot::to_string)
            ;
    }

} // namespace huira
