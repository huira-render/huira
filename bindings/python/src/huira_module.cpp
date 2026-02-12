#include <filesystem>
#include <string>

#include "pybind11/pybind11.h"
#include "pybind11/stl.h"

#include "huira/core/units/units_py.ipp"

namespace py = pybind11;

PYBIND11_MODULE(_huira, m) {
    m.doc() = "Python bindings for the Huira C++ library";

    huira::bind_units(m);
};
