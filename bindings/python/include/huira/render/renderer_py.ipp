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
        .def("set_region_culling", &Renderer::set_region_culling, py::arg("enable") = true)
        .def("set_region_cull_margin_scale",
             &Renderer::set_region_cull_margin_scale,
             py::arg("scale"))
        .def("set_region_cull_validation",
             &Renderer::set_region_cull_validation,
             py::arg("enable") = true)
        .def("set_psf_application",
             &Renderer::set_psf_application,
             py::arg("mode"),
             "Select which parts of the image the camera's optical model is applied to. "
             "PSFApplication.Full is the default; the other modes trade fidelity for speed.")
        .def("psf_application", &Renderer::psf_application)
        .def("set_max_psf_radius",
             &Renderer::set_max_psf_radius,
             py::arg("radius"),
             "Set the upper bound on any PSF kernel radius the camera may generate, in pixels.")
        .def("max_psf_radius", &Renderer::max_psf_radius)
        .def("set_aperture_sampling",
             &Renderer::set_aperture_sampling,
             py::arg("enable"),
             "Force aperture-sampled camera rays on or off.")
        .def("clear_aperture_sampling",
             &Renderer::clear_aperture_sampling,
             "Let the camera decide whether to sample the aperture, based on its focus setting.")
        .def("__repr__", [](const Renderer&) { return "Renderer()"; });
}

} // namespace huira
