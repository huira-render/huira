#pragma once

#include <sstream>
#include <vector>

#include "pybind11/pybind11.h"
#include "pybind11/stl.h"
#include "pybind11/operators.h"

#include "huira/core/spectral_bins.hpp"

namespace py = pybind11;

namespace huira {

    template <typename TSpectral>
    inline void bind_spectral_bins(py::module_& m) {
        constexpr std::size_t N = TSpectral::size();

        py::class_<TSpectral>(m, "SpectralBins")
            // Default constructor (all zeros)
            .def(py::init<>())

            // Fill constructor
            .def(py::init<float>(), py::arg("value"),
                "Create with all bins set to the given value")

            // Construct from a Python list
            .def(py::init([](std::vector<float> values) {
            if (values.size() != N) {
                throw py::value_error(
                    "Expected " + std::to_string(N) + " values, got " +
                    std::to_string(values.size()));
            }
            TSpectral result;
            for (std::size_t i = 0; i < N; ++i) {
                result[i] = values[i];
            }
            return result;
                }), py::arg("values"),
                    "Create from a list of float values (must match bin count)")

            // Element access
            .def("__getitem__", [](const TSpectral& self, std::size_t i) {
            if (i >= N) throw py::index_error("Index out of range");
            return self[i];
                })
            .def("__setitem__", [](TSpectral& self, std::size_t i, float val) {
            if (i >= N) throw py::index_error("Index out of range");
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
            .def(py::self -= py::self)
            .def(py::self *= py::self)
            .def(py::self /= py::self)
            .def("__add__", [](const TSpectral& a, const TSpectral& b) {
            TSpectral result(a); result += b; return result;
                })
            .def("__sub__", [](const TSpectral& a, const TSpectral& b) {
            TSpectral result(a); result -= b; return result;
                })
            .def("__mul__", [](const TSpectral& a, const TSpectral& b) {
            TSpectral result(a); result *= b; return result;
                })
            .def("__truediv__", [](const TSpectral& a, const TSpectral& b) {
            TSpectral result(a); result /= b; return result;
                })

            // Arithmetic: SpectralBins <op> scalar
            .def("__mul__", [](const TSpectral& self, float s) {
            TSpectral result(self); result *= s; return result;
                })
            .def("__rmul__", [](const TSpectral& self, float s) {
            TSpectral result(self); result *= s; return result;
                })
            .def("__truediv__", [](const TSpectral& self, float s) {
            TSpectral result(self); result /= s; return result;
                })
            .def("__add__", [](const TSpectral& self, float s) {
            TSpectral result(self); result += s; return result;
                })
            .def("__sub__", [](const TSpectral& self, float s) {
            TSpectral result(self); result -= s; return result;
                })

            // Unary
            .def("__neg__", [](const TSpectral& self) { return -self; })
            .def("__pos__", [](const TSpectral& self) { return +self; })

            // Comparisons
            .def(py::self == py::self)
            .def(py::self != py::self)

            // Bin information
            .def_static("size", []() { return N; },
                "Number of spectral bins")
            .def_static("get_bin", [](std::size_t index) {
            if (index >= N) throw py::index_error("Bin index out of range");
            return TSpectral::get_bin(index);
                }, py::arg("index"), "Get the Bin definition at the given index")

            // Conversion
            .def("to_list", [](const TSpectral& self) {
            std::vector<float> out(N);
            for (std::size_t i = 0; i < N; ++i) out[i] = self[i];
            return out;
                }, "Convert to a Python list of floats")

            .def("__repr__", [](const TSpectral& self) {
            return "SpectralBins(" + self.to_string() + ")";
                })
            .def("__str__", &TSpectral::to_string);
    }

}
