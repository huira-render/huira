#pragma once

#include "pybind11/pybind11.h"
#include "pybind11/stl.h"
#include "pybind11/stl/filesystem.h"

#include "huira/scene/scene.hpp"

namespace py = pybind11;

namespace huira {

    template <typename TSpectral>
    inline void bind_scene(py::module_& m) {
        using SceneType = Scene<TSpectral>;

        py::class_<SceneType>(m, "Scene")
            .def(py::init<>())

            // Star loading
            .def("load_stars", &SceneType::load_stars,
                py::arg("star_catalog_path"),
                py::arg("time"),
                py::arg("min_magnitude") = 100.f,
                "Load stars from a catalog file for the given observation time")

            // Debug printing
            .def("print_contents", &SceneType::print_contents)
            .def("print_graph", &SceneType::print_graph)

            .def("__repr__", [](const SceneType&) {
            return "<Scene>";
                });
    }

}
