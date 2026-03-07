#pragma once

#include "pybind11/pybind11.h"

#include "huira/render/renderer.hpp"

namespace py = pybind11;

namespace huira {

    template <IsSpectral TSpectral>
    void bind_renderer(py::module_& m) {
        using Renderer = Renderer<TSpectral>;

        py::class_<Renderer>(m, "Renderer")
            .def(py::init<>())
            .def("render", &Renderer::render,
                py::arg("scene_view"),
                py::arg("frame_buffer"),
                py::call_guard<py::gil_scoped_release>())
            .def("__repr__", [](const Renderer&) {
            return "Renderer()";
                })
            ;
    }

}
