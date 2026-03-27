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
            .def("set_samples_per_pixel", &Renderer::set_samples_per_pixel,
                py::arg("spp"))
            .def("set_max_bounces", &Renderer::set_max_bounces,
                py::arg("max_bounces"))

            .def("set_dynamic_sampling", &Renderer::set_dynamic_sampling,
                py::arg("dynamic_sampling") = true)
            .def("set_min_samples", &Renderer::set_min_samples,
                py::arg("min_samples"))
            .def("set_variance_threshold", &Renderer::set_variance_threshold,
                py::arg("threshold"))

            .def("set_indirect_clamp", &Renderer::set_indirect_clamp,
                py::arg("indirect_clamp"))
            .def("__repr__", [](const Renderer&) {
            return "Renderer()";
                });
    }

}
