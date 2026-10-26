#pragma once

#include <sstream>
#include <vector>

#include "huira/core/spectral_bins.hpp"
#include "huira/core/types_py.ipp"
#include "pybind11/numpy.h"
#include "pybind11/operators.h"
#include "pybind11/pybind11.h"
#include "pybind11/stl.h"

namespace py = pybind11;

namespace huira {
namespace detail {
/// SpectralBins as a numpy array, one value per bin, in single precision as they are kept.
template <typename TSpectral>
py::array_t<float> spectral_to_numpy(const TSpectral& bins)
{
    py::array_t<float> out(static_cast<py::ssize_t>(TSpectral::size()));
    auto values = out.mutable_unchecked<1>();
    for (std::size_t i = 0; i < TSpectral::size(); ++i) {
        values(static_cast<py::ssize_t>(i)) = bins[i];
    }
    return out;
}
} // namespace detail

template <typename TSpectral>
inline void bind_spectral_bins(py::module_& m)
{
    constexpr std::size_t N = TSpectral::size();

    auto cls = py::class_<TSpectral>(
        m,
        "SpectralBins",
        "A value in each spectral bin. Functions that take one also take any list, tuple or "
        "numpy array of one value per bin. It works with numpy as that array: np.asarray(s), "
        "numpy functions, s[0], len(s) and iteration.");
    cls
        // Default constructor (all zeros)
        .def(py::init<>())

        // Fill constructor
        .def(py::init<float>(), py::arg("value"), "Create with all bins set to the given value")

        // Factory method
        .def_static(
            "integrate_over_data",
            [](const std::vector<double>& wavelengths, const std::vector<float>& values) {
                return TSpectral::integrate_over_data(wavelengths, values);
            },
            py::arg("wavelengths"),
            py::arg("values"))

        // Construct from a Python list
        .def(py::init([](std::vector<float> values) {
                 if (values.size() != N) {
                     throw py::value_error("Expected " + std::to_string(N) + " values, got " +
                                           std::to_string(values.size()));
                 }
                 TSpectral result;
                 for (std::size_t i = 0; i < N; ++i) {
                     result[i] = values[i];
                 }
                 return result;
             }),
             py::arg("values"),
             "Create from a list of float values (must match bin count)")

        // Element access
        .def("__getitem__",
             [](const TSpectral& self, std::size_t i) {
                 if (i >= N) {
                     throw py::index_error("Index out of range");
                 }
                 return self[i];
             })
        .def("__setitem__",
             [](TSpectral& self, std::size_t i, float val) {
                 if (i >= N) {
                     throw py::index_error("Index out of range");
                 }
                 self[i] = val;
             })
        .def("__len__", [](const TSpectral&) { return N; })

        // Summary methods
        .def("total", &TSpectral::total)
        .def("magnitude", &TSpectral::magnitude)
        .def("max", &TSpectral::max)
        .def("min", &TSpectral::min)
        .def("integrate", &TSpectral::integrate)
        .def("fill", &TSpectral::fill, py::arg("value"))

        // Arithmetic: SpectralBins <op> SpectralBins
        .def(py::self += py::self)
        .def(py::self *= py::self)

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wself-assign-overloaded"
#endif
        .def(py::self -= py::self)
        .def(py::self /= py::self)
#if defined(__clang__)
#pragma clang diagnostic pop
#endif

        .def("__add__",
             [](const TSpectral& a, const TSpectral& b) {
                 TSpectral result(a);
                 result += b;
                 return result;
             })
        .def("__sub__",
             [](const TSpectral& a, const TSpectral& b) {
                 TSpectral result(a);
                 result -= b;
                 return result;
             })
        .def("__mul__",
             [](const TSpectral& a, const TSpectral& b) {
                 TSpectral result(a);
                 result *= b;
                 return result;
             })
        .def("__truediv__",
             [](const TSpectral& a, const TSpectral& b) {
                 TSpectral result(a);
                 result /= b;
                 return result;
             })

        // Arithmetic: SpectralBins <op> scalar
        .def("__mul__",
             [](const TSpectral& self, float s) {
                 TSpectral result(self);
                 result *= s;
                 return result;
             })
        .def("__rmul__",
             [](const TSpectral& self, float s) {
                 TSpectral result(self);
                 result *= s;
                 return result;
             })
        .def("__truediv__",
             [](const TSpectral& self, float s) {
                 TSpectral result(self);
                 result /= s;
                 return result;
             })
        .def("__add__",
             [](const TSpectral& self, float s) {
                 TSpectral result(self);
                 result += s;
                 return result;
             })
        .def("__sub__",
             [](const TSpectral& self, float s) {
                 TSpectral result(self);
                 result -= s;
                 return result;
             })

        // Unary
        .def("__neg__", [](const TSpectral& self) { return -self; })
        .def("__pos__", [](const TSpectral& self) { return +self; })

        // Comparisons
        .def(py::self == py::self)
        .def(py::self != py::self)

        // Bin information
        .def_static(
            "size", []() { return N; }, "Number of spectral bins")
        .def_static(
            "get_bin",
            [](std::size_t index) {
                if (index >= N) {
                    throw py::index_error("Bin index out of range");
                }
                return TSpectral::get_bin(index);
            },
            py::arg("index"),
            "Get the Bin definition at the given index")

        // Conversion
        .def("to_numpy",
             &detail::spectral_to_numpy<TSpectral>,
             "Return a numpy array of the values, one per bin (float32)")
        .def(
            "to_list",
            [](const TSpectral& self) {
                std::vector<float> out(TSpectral::size());
                for (std::size_t i = 0; i < TSpectral::size(); ++i) {
                    out[i] = self[i];
                }
                return out;
            },
            "Convert to a Python list of floats")

        .def("__repr__",
             [](const TSpectral& self) { return "SpectralBins(" + self.to_string() + ")"; })
        .def("__str__", &TSpectral::to_string);

    detail::bind_array_protocol<TSpectral>(cls, &detail::spectral_to_numpy<TSpectral>);
    detail::accept_array_likes<TSpectral>();
}

} // namespace huira
