#pragma once

#include <sstream>
#include <string>
#include <utility>

#include "huira/core/types.hpp"
#include "pybind11/numpy.h"
#include "pybind11/pybind11.h"
#include "pybind11/stl.h"

namespace py = pybind11;

namespace huira {

/// A numpy array of doubles, made contiguous if it is not, which the bindings read through a
/// pointer.
using DoubleArray = py::array_t<double, py::array::c_style | py::array::forcecast>;

namespace detail {
/// A Vec3 as a numpy array [x, y, z].
inline py::array_t<double> vec3_to_numpy(const Vec3<double>& v)
{
    py::array_t<double> array(3);
    auto values = array.mutable_unchecked<1>();
    values(0) = v.x;
    values(1) = v.y;
    values(2) = v.z;
    return array;
}

/// A Mat3 as a 3x3 numpy array, row by row. (GLM stores it column by column, mat[col][row].)
inline py::array_t<double> mat3_to_numpy(const Mat3<double>& mat)
{
    py::array_t<double> array({3, 3});
    auto values = array.mutable_unchecked<2>();
    for (int row = 0; row < 3; ++row) {
        for (int col = 0; col < 3; ++col) {
            values(row, col) = mat[col][row];
        }
    }
    return array;
}

/// A Hamilton quaternion as a numpy array [w, x, y, z].
inline py::array_t<double> quaternion_to_numpy(const Quaternion<double>& q)
{
    py::array_t<double> array(4);
    auto values = array.mutable_unchecked<1>();
    values(0) = q.w;
    values(1) = q.x;
    values(2) = q.y;
    values(3) = q.z;
    return array;
}

/// A Shuster quaternion as a numpy array [x, y, z, w].
inline py::array_t<double> shuster_quaternion_to_numpy(const ShusterQuaternion<double>& q)
{
    py::array_t<double> array(4);
    auto values = array.mutable_unchecked<1>();
    values(0) = q.x;
    values(1) = q.y;
    values(2) = q.z;
    values(3) = q.w;
    return array;
}

/**
 * @brief Let numpy take a value type as the array its to_numpy() gives (__array__): np.asarray()
 * and every numpy function take it, and arithmetic with numpy arrays works.
 */
template <typename T, typename PyClass, typename ToNumpy>
void bind_array_protocol(PyClass& cls, ToNumpy to_numpy)
{
    cls.def(
        "__array__",
        [to_numpy](const T& value, const py::object& dtype, const py::object& /*copy*/) {
            py::object array = to_numpy(value);
            return dtype.is_none() ? array : array.attr("astype")(dtype);
        },
        py::arg("dtype") = py::none(),
        py::arg("copy") = py::none());
}

/**
 * @brief Let a value type index (v[0], m[0, 2], slices), have a len() and iterate, as the array
 * its to_numpy() gives does.
 */
template <typename T, typename PyClass, typename ToNumpy>
void bind_sequence_protocol(PyClass& cls, ToNumpy to_numpy)
{
    cls.def("__len__", [to_numpy](const T& value) { return py::len(to_numpy(value)); })
        .def("__getitem__",
             [to_numpy](const T& value, const py::object& index) {
                 return to_numpy(value).attr("__getitem__")(index);
             })
        .def("__iter__", [to_numpy](const T& value) { return py::iter(to_numpy(value)); });
}

/**
 * @brief Let every function that takes a T take any list, tuple or numpy array T's constructor
 * takes, such as [1, 2, 3] for a Vec3. Call after T is bound.
 */
template <typename T>
void accept_array_likes()
{
    py::implicitly_convertible<py::list, T>();
    py::implicitly_convertible<py::tuple, T>();
    py::implicitly_convertible<py::array, T>();
}

/// Whether a Python object is a Vec3 or a Mat3, the types numpy's arithmetic works on through
/// their __array_ufunc__ (see bind_numpy_arithmetic()).
inline bool is_vec3_or_mat3(const py::handle& obj)
{
    return py::isinstance<Vec3<double>>(obj) || py::isinstance<Mat3<double>>(obj);
}

/// A numpy ufunc's result, with each array of three numbers made a Vec3 and each 3x3 one a Mat3.
/// Anything else is left as numpy gave it: arrays of other shapes, booleans (as comparisons
/// give), and scalars.
inline py::object wrap_ufunc_result(const py::object& result)
{
    if (py::isinstance<py::tuple>(result)) {
        py::list wrapped;
        for (const py::handle item : result) {
            wrapped.append(wrap_ufunc_result(py::reinterpret_borrow<py::object>(item)));
        }
        return py::tuple(wrapped);
    }
    if (!py::isinstance<py::array>(result)) {
        return result;
    }
    const auto array = py::reinterpret_borrow<py::array>(result);
    const char kind = array.dtype().kind();
    if (kind != 'f' && kind != 'i' && kind != 'u') {
        return result;
    }
    if (array.ndim() == 1 && array.shape(0) == 3) {
        return py::type::of<Vec3<double>>()(array);
    }
    if (array.ndim() == 2 && array.shape(0) == 3 && array.shape(1) == 3) {
        return py::type::of<Mat3<double>>()(array);
    }
    return result;
}

/**
 * @brief Arithmetic and comparisons for a Vec3 or Mat3, as numpy has them for the array its
 * to_numpy() gives.
 *
 * Each operator calls numpy's ufunc, as an array's does, and the type's __array_ufunc__ (which
 * numpy calls whenever one is an operand, whichever side it is on) has numpy do the work on its
 * array and gives back a Vec3 or Mat3 where the result is a vector or 3x3 matrix of numbers. So
 * the rules are numpy's: +, -, *, / and the rest are element by element, broadcasting with
 * scalars and arrays (a Mat3 * Mat3 too); @ is the matrix product; comparisons give arrays of
 * booleans; and results of other shapes are numpy arrays.
 */
template <typename PyClass>
void bind_numpy_arithmetic(PyClass& cls)
{
    cls.def("__array_ufunc__",
            [](const py::object& /*self*/,
               const py::object& ufunc,
               const std::string& method,
               const py::args& inputs,
               const py::kwargs& kwargs) -> py::object {
                // A Vec3 or Mat3 can't be written into in place:
                if (kwargs.contains("out")) {
                    for (const py::handle out : kwargs["out"]) {
                        if (is_vec3_or_mat3(out)) {
                            return py::reinterpret_borrow<py::object>(Py_NotImplemented);
                        }
                    }
                }
                py::list arrays;
                for (const py::handle input : inputs) {
                    arrays.append(is_vec3_or_mat3(input)
                                      ? input.attr("__array__")()
                                      : py::reinterpret_borrow<py::object>(input));
                }
                return wrap_ufunc_result(ufunc.attr(method.c_str())(*arrays, **kwargs));
            });

    struct Operator {
        const char* name;
        const char* ufunc;
        bool reflected;
    };
    for (const Operator op : {Operator{"__add__", "add", false},
                              Operator{"__radd__", "add", true},
                              Operator{"__sub__", "subtract", false},
                              Operator{"__rsub__", "subtract", true},
                              Operator{"__mul__", "multiply", false},
                              Operator{"__rmul__", "multiply", true},
                              Operator{"__truediv__", "divide", false},
                              Operator{"__rtruediv__", "divide", true},
                              Operator{"__floordiv__", "floor_divide", false},
                              Operator{"__rfloordiv__", "floor_divide", true},
                              Operator{"__mod__", "remainder", false},
                              Operator{"__rmod__", "remainder", true},
                              Operator{"__pow__", "power", false},
                              Operator{"__rpow__", "power", true},
                              Operator{"__matmul__", "matmul", false},
                              Operator{"__rmatmul__", "matmul", true},
                              Operator{"__eq__", "equal", false},
                              Operator{"__ne__", "not_equal", false},
                              Operator{"__lt__", "less", false},
                              Operator{"__le__", "less_equal", false},
                              Operator{"__gt__", "greater", false},
                              Operator{"__ge__", "greater_equal", false}}) {
        cls.def(
            op.name,
            [ufunc = op.ufunc, reflected = op.reflected](const py::object& self,
                                                         const py::object& other) {
                const py::object function = py::module_::import("numpy").attr(ufunc);
                return reflected ? function(other, self) : function(self, other);
            },
            py::is_operator());
    }
    for (const auto& [name, ufunc] : {std::pair{"__neg__", "negative"},
                                      std::pair{"__pos__", "positive"},
                                      std::pair{"__abs__", "absolute"}}) {
        cls.def(name, [ufunc = ufunc](const py::object& self) {
            return py::module_::import("numpy").attr(ufunc)(self);
        });
    }
}
} // namespace detail

/**
 * @brief Register the core math types (Vec3, Mat3, Quaternion, ShusterQuaternion), in double
 * precision.
 *
 * Wherever a function takes a Vec3 or a Mat3, it also takes any list, tuple or numpy array of
 * that shape: [1, 2, 3], or a 3x3 array or nested list, row by row. A quaternion must be a
 * Quaternion or ShusterQuaternion, since four numbers alone don't say which order they are in.
 * Each type works with numpy as the array its to_numpy() gives: np.asarray(v), numpy functions,
 * indexing, len() and iteration.
 */
inline void bind_types(py::module_& m)
{
    // -----------------------------------------------------------------
    // Vec3<double>
    // -----------------------------------------------------------------
    auto vec3 = py::class_<Vec3<double>>(
        m,
        "Vec3",
        "A 3-component vector, in double precision. Functions that take a Vec3 also take any "
        "list, tuple or numpy array of three numbers. It works with numpy as the array "
        "[x, y, z]: np.asarray(v), numpy functions, v[0], len(v) and iteration, and its "
        "arithmetic is numpy's, element by element with scalars, vectors and arrays (v + w, "
        "v * 2, v / w), giving a Vec3 where the result is three numbers.");
    vec3.def(py::init<>())
        .def(py::init<double, double, double>(), py::arg("x"), py::arg("y"), py::arg("z"))
        .def(py::init([](DoubleArray arr) {
                 auto buf = arr.request();
                 if (buf.ndim != 1 || buf.shape[0] != 3) {
                     throw std::runtime_error("Vec3 requires a 1-D array of length 3");
                 }
                 auto* ptr = static_cast<double*>(buf.ptr);
                 return Vec3<double>(ptr[0], ptr[1], ptr[2]);
             }),
             py::arg("array"),
             "Construct from a numpy array of length 3")

        .def_property(
            "x",
            [](const Vec3<double>& v) { return v.x; },
            [](Vec3<double>& v, double val) { v.x = val; })
        .def_property(
            "y",
            [](const Vec3<double>& v) { return v.y; },
            [](Vec3<double>& v, double val) { v.y = val; })
        .def_property(
            "z",
            [](const Vec3<double>& v) { return v.z; },
            [](Vec3<double>& v, double val) { v.z = val; })

        .def("to_numpy", &detail::vec3_to_numpy, "Return a numpy array [x, y, z]")

        .def("__repr__", [](const Vec3<double>& v) {
            std::ostringstream ss;
            ss << "Vec3(" << py::repr(py::float_(v.x)).cast<std::string>() << ", "
               << py::repr(py::float_(v.y)).cast<std::string>() << ", "
               << py::repr(py::float_(v.z)).cast<std::string>() << ")";
            return ss.str();
        });
    detail::bind_array_protocol<Vec3<double>>(vec3, &detail::vec3_to_numpy);
    detail::bind_sequence_protocol<Vec3<double>>(vec3, &detail::vec3_to_numpy);

    // -----------------------------------------------------------------
    // Mat3<double>
    // -----------------------------------------------------------------
    auto mat3 = py::class_<Mat3<double>>(
        m,
        "Mat3",
        "A 3x3 matrix, in double precision. Functions that take a Mat3 also take any 3x3 numpy "
        "array or nested list, row by row. It works with numpy as that 3x3 array: "
        "np.asarray(m), numpy functions, m[0, 2], len(m) and iteration over its rows, and its "
        "arithmetic is numpy's: element by element (m + n, m * n, m / 2), and the matrix "
        "product with @ (m @ v gives a Vec3, m @ m a Mat3).");
    mat3.def(py::init<>())
        .def(py::init<double>(),
             py::arg("diagonal"),
             "Construct a diagonal matrix with the given value")
        .def(py::init([](DoubleArray arr) {
                 auto buf = arr.request();
                 if (buf.ndim != 2 || buf.shape[0] != 3 || buf.shape[1] != 3) {
                     throw std::runtime_error("Mat3 requires a 3x3 numpy array");
                 }
                 auto* ptr = static_cast<double*>(buf.ptr);
                 // NumPy is row-major; GLM Mat3 stores column-major.
                 // mat[col][row] in GLM.
                 Mat3<double> mat;
                 for (int row = 0; row < 3; ++row) {
                     for (int col = 0; col < 3; ++col) {
                         mat[col][row] = ptr[row * 3 + col];
                     }
                 }
                 return mat;
             }),
             py::arg("array"),
             "Construct from a 3x3 numpy array (row-major)")

        .def("to_numpy", &detail::mat3_to_numpy, "Return a 3x3 numpy array, row by row")

        // Row by row, as it is written and as numpy shows it. (GLM's own string is column by
        // column, which reads as the transpose.)
        .def("__repr__", [](const Mat3<double>& mat) {
            std::string text = py::repr(detail::mat3_to_numpy(mat)).cast<std::string>();
            // "array([[...],\n       [...]])" -> "Mat3([[...],\n      [...]])"
            text = "Mat3" + text.substr(5);
            for (std::size_t at = text.find("\n       "); at != std::string::npos;
                 at = text.find("\n       ", at + 1)) {
                text.erase(at + 1, 1);
            }
            return text;
        });
    detail::bind_array_protocol<Mat3<double>>(mat3, &detail::mat3_to_numpy);
    detail::bind_sequence_protocol<Mat3<double>>(mat3, &detail::mat3_to_numpy);

    // -----------------------------------------------------------------
    // Quaternion<double>  (Hamilton: w, x, y, z)
    // -----------------------------------------------------------------
    auto quaternion = py::class_<Quaternion<double>>(
        m,
        "Quaternion",
        "Hamilton quaternion (w, x, y, z) – double precision. It works with numpy as the array "
        "[w, x, y, z]. Functions that take one take only a Quaternion, not four numbers, which "
        "don't say which order they are in.");
    quaternion.def(py::init<>())
        .def(py::init<double, double, double, double>(),
             py::arg("w"),
             py::arg("x"),
             py::arg("y"),
             py::arg("z"))
        .def(py::init([](DoubleArray arr) {
                 auto buf = arr.request();
                 if (buf.ndim != 1 || buf.shape[0] != 4) {
                     throw std::runtime_error(
                         "Quaternion requires a 1-D array of length 4 [w, x, y, z]");
                 }
                 auto* ptr = static_cast<double*>(buf.ptr);
                 return Quaternion<double>(ptr[0], ptr[1], ptr[2], ptr[3]);
             }),
             py::arg("array"),
             "Construct from numpy array [w, x, y, z]")

        .def_property(
            "w",
            [](const Quaternion<double>& q) { return q.w; },
            [](Quaternion<double>& q, double val) { q.w = val; })
        .def_property(
            "x",
            [](const Quaternion<double>& q) { return q.x; },
            [](Quaternion<double>& q, double val) { q.x = val; })
        .def_property(
            "y",
            [](const Quaternion<double>& q) { return q.y; },
            [](Quaternion<double>& q, double val) { q.y = val; })
        .def_property(
            "z",
            [](const Quaternion<double>& q) { return q.z; },
            [](Quaternion<double>& q, double val) { q.z = val; })

        .def("to_numpy", &detail::quaternion_to_numpy, "Return numpy array [w, x, y, z]")

        .def("__repr__", [](const Quaternion<double>& q) {
            std::ostringstream ss;
            ss << "Quaternion(w=" << q.w << ", x=" << q.x << ", y=" << q.y << ", z=" << q.z << ")";
            return ss.str();
        });
    detail::bind_array_protocol<Quaternion<double>>(quaternion, &detail::quaternion_to_numpy);
    detail::bind_sequence_protocol<Quaternion<double>>(quaternion, &detail::quaternion_to_numpy);

    // -----------------------------------------------------------------
    // ShusterQuaternion<double>  (x, y, z, w)
    // -----------------------------------------------------------------
    // ShusterQuaternion is a Vec4<double> alias, so we bind it as a
    // distinct Python type to avoid colliding with a generic Vec4 binding.
    auto shuster = py::class_<ShusterQuaternion<double>>(
        m,
        "ShusterQuaternion",
        "Shuster (aerospace) quaternion (x, y, z, w) – double precision. It works with numpy as "
        "the array [x, y, z, w].");
    shuster.def(py::init<>())
        .def(py::init<double, double, double, double>(),
             py::arg("x"),
             py::arg("y"),
             py::arg("z"),
             py::arg("w"))
        .def(py::init([](DoubleArray arr) {
                 auto buf = arr.request();
                 if (buf.ndim != 1 || buf.shape[0] != 4) {
                     throw std::runtime_error(
                         "ShusterQuaternion requires a 1-D array of length 4 [x, y, z, w]");
                 }
                 auto* ptr = static_cast<double*>(buf.ptr);
                 return ShusterQuaternion<double>(ptr[0], ptr[1], ptr[2], ptr[3]);
             }),
             py::arg("array"),
             "Construct from numpy array [x, y, z, w]")

        .def_property(
            "x",
            [](const ShusterQuaternion<double>& q) { return q.x; },
            [](ShusterQuaternion<double>& q, double val) { q.x = val; })
        .def_property(
            "y",
            [](const ShusterQuaternion<double>& q) { return q.y; },
            [](ShusterQuaternion<double>& q, double val) { q.y = val; })
        .def_property(
            "z",
            [](const ShusterQuaternion<double>& q) { return q.z; },
            [](ShusterQuaternion<double>& q, double val) { q.z = val; })
        .def_property(
            "w",
            [](const ShusterQuaternion<double>& q) { return q.w; },
            [](ShusterQuaternion<double>& q, double val) { q.w = val; })

        .def("to_numpy", &detail::shuster_quaternion_to_numpy, "Return numpy array [x, y, z, w]")

        .def("__repr__", [](const ShusterQuaternion<double>& q) {
            std::ostringstream ss;
            ss << "ShusterQuaternion(x=" << q.x << ", y=" << q.y << ", z=" << q.z << ", w=" << q.w
               << ")";
            return ss.str();
        });
    detail::bind_array_protocol<ShusterQuaternion<double>>(shuster,
                                                           &detail::shuster_quaternion_to_numpy);
    detail::bind_sequence_protocol<ShusterQuaternion<double>>(shuster,
                                                              &detail::shuster_quaternion_to_numpy);

    // Vectors and matrices from lists, tuples and numpy arrays, wherever they are taken (not
    // quaternions: see the Quaternion docstring):
    detail::accept_array_likes<Vec3<double>>();
    detail::accept_array_likes<Mat3<double>>();

    // Arithmetic as numpy has it, once both are bound (a result can be either):
    detail::bind_numpy_arithmetic(vec3);
    detail::bind_numpy_arithmetic(mat3);

    // -----------------------------------------------------------------
    // Free-function conversions
    // -----------------------------------------------------------------
    m.def("to_shuster",
          &to_shuster<double>,
          py::arg("quaternion"),
          "Convert a Hamilton quaternion to a Shuster quaternion");
    m.def("to_hamilton",
          &to_hamilton<double>,
          py::arg("shuster_quaternion"),
          "Convert a Shuster quaternion to a Hamilton quaternion");
}

} // namespace huira
