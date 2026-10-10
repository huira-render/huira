#pragma once

#include "huira/render/renderer.hpp"
#include "pybind11/pybind11.h"

namespace py = pybind11;

namespace huira {

template <IsSpectral TSpectral>
void bind_renderer(py::module_& m)
{
    using Renderer = Renderer<TSpectral>;

    py::class_<Renderer>(m, "Renderer")
        .def(py::init<>())
        .def("render",
             &Renderer::render,
             py::arg("scene_view"),
             py::arg("frame_buffer"),
             py::call_guard<py::gil_scoped_release>())
        .def("set_samples_per_pixel", &Renderer::set_samples_per_pixel, py::arg("spp"))
        .def("set_max_bounces", &Renderer::set_max_bounces, py::arg("max_bounces"))
        .def("set_dynamic_sampling",
             &Renderer::set_dynamic_sampling,
             py::arg("dynamic_sampling") = true)
        .def("set_min_samples", &Renderer::set_min_samples, py::arg("min_samples"))
        .def("set_variance_threshold", &Renderer::set_variance_threshold, py::arg("threshold"))
        .def("set_indirect_clamp", &Renderer::set_indirect_clamp, py::arg("indirect_clamp"))
        .def("set_unresolved_occlusion",
             &Renderer::set_unresolved_occlusion,
             py::arg("occlusion") = true)
        .def("set_stamp_cropping",
             &Renderer::set_stamp_cropping,
             py::arg("enable") = true,
             "Crop each unresolved source's PSF stamp to the pixels its light would show in (the "
             "default): those receiving at least a tenth of the sensor's read noise over the "
             "exposure. Cropped stamps keep all of the source's light, and never move its "
             "centroid by more than 0.01 px. False always stamps the whole PSF.")
        .def("set_unresolved_taper",
             &Renderer::set_unresolved_taper,
             py::arg("fraction_of_read_noise"),
             "Set how far an unresolved source, still or moving, is drawn with the aperture's "
             "PSF: until the light it puts in a pixel, as read out, falls to this fraction of the "
             "sensor's read noise (of an electron without read noise). It then fades out "
             "smoothly by twice that distance, and the light left on the sensor is spread evenly "
             "over the frame. The default is 0.001; 0 draws every source over the whole frame.")
        .def("unresolved_taper", &Renderer::unresolved_taper)
        .def("set_region_culling", &Renderer::set_region_culling, py::arg("enable") = true)
        .def("set_region_cull_margin_scale",
             &Renderer::set_region_cull_margin_scale,
             py::arg("scale"))
        .def("set_region_cull_validation",
             &Renderer::set_region_cull_validation,
             py::arg("enable") = true)
        .def("__repr__", [](const Renderer&) { return "Renderer()"; });
}

} // namespace huira
