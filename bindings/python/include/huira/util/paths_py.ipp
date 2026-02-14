#pragma once

#include "pybind11/pybind11.h"
#include "pybind11/stl/filesystem.h"

#include "huira/util/paths.hpp"

namespace py = pybind11;

namespace huira {

    inline void bind_paths(py::module_& m) {
        m.def("data_dir", []() {
            return Paths::instance().data_dir();
            }, "Get the current data directory");

        m.def("set_data_dir", [](const std::filesystem::path& path) {
            Paths::instance().set_data_dir(path);
            }, py::arg("path"),
                "Override the data directory path");

        m.def("reset_data_dir", []() {
            Paths::instance().reset_data_dir();
            }, "Reset the data directory to the default");
    }

}
