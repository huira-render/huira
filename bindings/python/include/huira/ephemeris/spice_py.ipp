#pragma once

#include "pybind11/pybind11.h"
#include "pybind11/stl/filesystem.h"

#include "huira/ephemeris/spice.hpp"

namespace py = pybind11;

namespace huira::spice {

    inline void bind_spice(py::module_& m) {
        auto spice_mod = m.def_submodule("spice", "SPICE kernel and ephemeris utilities");

        spice_mod.def("furnsh", &huira::spice::furnsh,
            py::arg("file_path"),
            "Load a SPICE kernel file");

        spice_mod.def("furnsh_relative_to_file", &huira::spice::furnsh_relative_to_file,
            py::arg("kernel_path"),
            "Load a SPICE kernel using a path relative to the calling file");

        spice_mod.def("load_default_pck", &huira::spice::load_default_pck,
            "Load the default planetary constants kernel");
    }

}
