#include <filesystem>
#include <string>

#include "pybind11/pybind11.h"
#include "pybind11/stl.h"

#include "pyhuira/units/units_py.ipp"

namespace py = pybind11;

PYBIND11_MODULE(pyhuira, m) {
    m.doc() = "Python bindings for the Huira C++ library";

    huira::bind_units(m);
};
