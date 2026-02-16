#pragma once

#include "pybind11/pybind11.h"

#include "huira/render/raster_renderer.hpp"

namespace py = pybind11;

namespace huira {

    template <IsSpectral TSpectral>
    void bind_raster_renderer(py::module_& m) {
        using RR = RasterRenderer<TSpectral>;

        py::class_<RR>(m, "RasterRenderer")
            .def(py::init<>())
            .def("render", &RR::render,
                py::arg("scene_view"),
                py::arg("frame_buffer"),
                py::arg("exposure_time"),
                py::call_guard<py::gil_scoped_release>())
            .def("__repr__", [](const RR&) {
            return "RasterRenderer()";
                })
            ;
    }

}
