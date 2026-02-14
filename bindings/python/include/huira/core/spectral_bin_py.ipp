#pragma once

#include <sstream>

#include "pybind11/pybind11.h"
#include "huira/core/spectral_bins.hpp"

namespace py = pybind11;

namespace huira {

    inline void bind_bin(py::module_& m) {
        py::class_<Bin>(m, "Bin")
            .def_readonly("min_wavelength", &Bin::min_wavelength)
            .def_readonly("max_wavelength", &Bin::max_wavelength)
            .def_readonly("center_wavelength", &Bin::center_wavelength)
            .def("__repr__", [](const Bin& b) {
            std::ostringstream os;
            os << "Bin(" << b.min_wavelength << ", " << b.max_wavelength << ")";
            return os.str();
                });
    }

}
