#pragma once

#include "pybind11/pybind11.h"
#include "pybind11/numpy.h"
#include "pybind11/stl.h"

#include "huira/core/types.hpp"

namespace py = pybind11;

namespace huira {

    /**
     * @brief Register pybind11 type casters and classes for core math types
     *        (Vec3, Mat3, Quaternion, ShusterQuaternion) using double precision.
     *
     * Vec3<double> and Mat3<double> are exposed as buffer-protocol objects so they
     * round-trip transparently with numpy arrays.  Quaternion and ShusterQuaternion
     * are thin wrappers with named component access.
     */
    inline void bind_types(py::module_& m) {
        // -----------------------------------------------------------------
        // Vec3<double>
        // -----------------------------------------------------------------
        py::class_<Vec3<double>>(m, "Vec3",
            "3-component vector (double precision)")
            .def(py::init<>())
            .def(py::init<double, double, double>(),
                py::arg("x"), py::arg("y"), py::arg("z"))
            .def(py::init([](py::array_t<double> arr) {
                auto buf = arr.request();
                if (buf.ndim != 1 || buf.shape[0] != 3)
                    throw std::runtime_error("Vec3 requires a 1-D array of length 3");
                auto* ptr = static_cast<double*>(buf.ptr);
                return Vec3<double>(ptr[0], ptr[1], ptr[2]);
            }), py::arg("array"), "Construct from a numpy array of length 3")

            .def_property("x",
                [](const Vec3<double>& v) { return v.x; },
                [](Vec3<double>& v, double val) { v.x = val; })
            .def_property("y",
                [](const Vec3<double>& v) { return v.y; },
                [](Vec3<double>& v, double val) { v.y = val; })
            .def_property("z",
                [](const Vec3<double>& v) { return v.z; },
                [](Vec3<double>& v, double val) { v.z = val; })

            .def("to_numpy", [](const Vec3<double>& v) {
                py::array_t<double> arr(3);
                auto buf = arr.mutable_unchecked<1>();
                buf(0) = v.x; buf(1) = v.y; buf(2) = v.z;
                return arr;
            }, "Return a numpy array [x, y, z]")

            .def("__repr__", [](const Vec3<double>& v) {
                return vec_to_string<3, double>(v);
            })
            ;

        // -----------------------------------------------------------------
        // Mat3<double>
        // -----------------------------------------------------------------
        py::class_<Mat3<double>>(m, "Mat3",
            "3x3 matrix (double precision, column-major / GLM layout)")
            .def(py::init<>())
            .def(py::init<double>(), py::arg("diagonal"),
                "Construct a diagonal matrix with the given value")
            .def(py::init([](py::array_t<double, py::array::c_style | py::array::forcecast> arr) {
                auto buf = arr.request();
                if (buf.ndim != 2 || buf.shape[0] != 3 || buf.shape[1] != 3)
                    throw std::runtime_error("Mat3 requires a 3x3 numpy array");
                auto* ptr = static_cast<double*>(buf.ptr);
                // NumPy is row-major; GLM Mat3 stores column-major.
                // mat[col][row] in GLM.
                Mat3<double> mat;
                for (int row = 0; row < 3; ++row)
                    for (int col = 0; col < 3; ++col)
                        mat[col][row] = ptr[row * 3 + col];
                return mat;
            }), py::arg("array"), "Construct from a 3x3 numpy array (row-major)")

            .def("to_numpy", [](const Mat3<double>& mat) {
                py::array_t<double> arr({3, 3});
                auto buf = arr.mutable_unchecked<2>();
                for (int row = 0; row < 3; ++row)
                    for (int col = 0; col < 3; ++col)
                        buf(row, col) = mat[col][row];
                return arr;
            }, "Return a 3x3 numpy array (row-major)")

            .def("__repr__", [](const Mat3<double>& m) {
                return mat_to_string<3, 3, double>(m);
            })
            ;

        // -----------------------------------------------------------------
        // Quaternion<double>  (Hamilton: w, x, y, z)
        // -----------------------------------------------------------------
        py::class_<Quaternion<double>>(m, "Quaternion",
            "Hamilton quaternion (w, x, y, z) – double precision")
            .def(py::init<>())
            .def(py::init<double, double, double, double>(),
                py::arg("w"), py::arg("x"), py::arg("y"), py::arg("z"))
            .def(py::init([](py::array_t<double> arr) {
                auto buf = arr.request();
                if (buf.ndim != 1 || buf.shape[0] != 4)
                    throw std::runtime_error("Quaternion requires a 1-D array of length 4 [w, x, y, z]");
                auto* ptr = static_cast<double*>(buf.ptr);
                return Quaternion<double>(ptr[0], ptr[1], ptr[2], ptr[3]);
            }), py::arg("array"), "Construct from numpy array [w, x, y, z]")

            .def_property("w",
                [](const Quaternion<double>& q) { return q.w; },
                [](Quaternion<double>& q, double val) { q.w = val; })
            .def_property("x",
                [](const Quaternion<double>& q) { return q.x; },
                [](Quaternion<double>& q, double val) { q.x = val; })
            .def_property("y",
                [](const Quaternion<double>& q) { return q.y; },
                [](Quaternion<double>& q, double val) { q.y = val; })
            .def_property("z",
                [](const Quaternion<double>& q) { return q.z; },
                [](Quaternion<double>& q, double val) { q.z = val; })

            .def("to_numpy", [](const Quaternion<double>& q) {
                py::array_t<double> arr(4);
                auto buf = arr.mutable_unchecked<1>();
                buf(0) = q.w; buf(1) = q.x; buf(2) = q.y; buf(3) = q.z;
                return arr;
            }, "Return numpy array [w, x, y, z]")

            .def("__repr__", [](const Quaternion<double>& q) {
                std::ostringstream ss;
                ss << "Quaternion(w=" << q.w << ", x=" << q.x
                   << ", y=" << q.y << ", z=" << q.z << ")";
                return ss.str();
            })
            ;

        // -----------------------------------------------------------------
        // ShusterQuaternion<double>  (x, y, z, w)
        // -----------------------------------------------------------------
        // ShusterQuaternion is a Vec4<double> alias, so we bind it as a
        // distinct Python type to avoid colliding with a generic Vec4 binding.
        py::class_<ShusterQuaternion<double>>(m, "ShusterQuaternion",
            "Shuster (aerospace) quaternion (x, y, z, w) – double precision")
            .def(py::init<>())
            .def(py::init<double, double, double, double>(),
                py::arg("x"), py::arg("y"), py::arg("z"), py::arg("w"))
            .def(py::init([](py::array_t<double> arr) {
                auto buf = arr.request();
                if (buf.ndim != 1 || buf.shape[0] != 4)
                    throw std::runtime_error("ShusterQuaternion requires a 1-D array of length 4 [x, y, z, w]");
                auto* ptr = static_cast<double*>(buf.ptr);
                return ShusterQuaternion<double>(ptr[0], ptr[1], ptr[2], ptr[3]);
            }), py::arg("array"), "Construct from numpy array [x, y, z, w]")

            .def_property("x",
                [](const ShusterQuaternion<double>& q) { return q.x; },
                [](ShusterQuaternion<double>& q, double val) { q.x = val; })
            .def_property("y",
                [](const ShusterQuaternion<double>& q) { return q.y; },
                [](ShusterQuaternion<double>& q, double val) { q.y = val; })
            .def_property("z",
                [](const ShusterQuaternion<double>& q) { return q.z; },
                [](ShusterQuaternion<double>& q, double val) { q.z = val; })
            .def_property("w",
                [](const ShusterQuaternion<double>& q) { return q.w; },
                [](ShusterQuaternion<double>& q, double val) { q.w = val; })

            .def("to_numpy", [](const ShusterQuaternion<double>& q) {
                py::array_t<double> arr(4);
                auto buf = arr.mutable_unchecked<1>();
                buf(0) = q.x; buf(1) = q.y; buf(2) = q.z; buf(3) = q.w;
                return arr;
            }, "Return numpy array [x, y, z, w]")

            .def("__repr__", [](const ShusterQuaternion<double>& q) {
                std::ostringstream ss;
                ss << "ShusterQuaternion(x=" << q.x << ", y=" << q.y
                   << ", z=" << q.z << ", w=" << q.w << ")";
                return ss.str();
            })
            ;

        // -----------------------------------------------------------------
        // Free-function conversions
        // -----------------------------------------------------------------
        m.def("to_shuster", &to_shuster<double>,
            py::arg("quaternion"),
            "Convert a Hamilton quaternion to a Shuster quaternion");
        m.def("to_hamilton", &to_hamilton<double>,
            py::arg("shuster_quaternion"),
            "Convert a Shuster quaternion to a Hamilton quaternion");
    }

} // namespace huira
