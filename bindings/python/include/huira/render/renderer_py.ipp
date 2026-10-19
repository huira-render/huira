#pragma once

#include <map>
#include <string>

#include "huira/render/renderer.hpp"
#include "pybind11/pybind11.h"

namespace py = pybind11;

namespace huira {
namespace detail {
/// Renderer methods removed from the Python API, with when and what to use instead.
inline const std::map<std::string, std::string>& removed_renderer_methods()
{
    static const std::map<std::string, std::string> removed{
        {"set_variance_threshold",
         "v0.9.11. Dynamic sampling now judges a pixel by the noise in its mean relative to it, "
         "with its neighbours: use set_noise_threshold(), where 0.01 is about 1% noise."},
    };
    return removed;
}
} // namespace detail

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
             py::arg("dynamic_sampling") = true,
             "Turn dynamic sampling on or off (the default). On, each pixel is traced with as "
             "many of its samples as bring its noise under the noise threshold, and at least the "
             "minimum, judged with its neighbours'. See set_noise_threshold().")
        .def("set_min_samples", &Renderer::set_min_samples, py::arg("min_samples"))
        .def("set_noise_threshold",
             &Renderer::set_noise_threshold,
             py::arg("threshold"),
             "Set how noisy dynamic sampling leaves a pixel: the standard error of its mean "
             "radiance, relative to that mean (0.01, the default, is about 1%). Pixels are traced "
             "in rounds, the minimum number of samples and then twice as many each round, up to "
             "the samples per pixel, and a pixel samples on while it or any of its eight "
             "neighbours is noisier than this. 0 never stops early.")
        .def("noise_threshold",
             &Renderer::noise_threshold,
             "How noisy dynamic sampling leaves a pixel. See set_noise_threshold().")
        .def("set_indirect_clamp", &Renderer::set_indirect_clamp, py::arg("indirect_clamp"))
        .def("set_unresolved_occlusion",
             &Renderer::set_unresolved_occlusion,
             py::arg("occlusion") = true)
        .def("set_unresolved_taper",
             &Renderer::set_unresolved_taper,
             py::arg("fraction_of_read_noise"),
             "Set how far an unresolved source, still or moving, is drawn with its PSF and "
             "scattered light: until the light it puts in a pixel, as read out, falls to this "
             "fraction of the sensor's read noise (of an electron without read noise). It then "
             "fades out smoothly by twice that distance, and the light left on the sensor is "
             "spread evenly over the frame. The default is 0.001; 0 draws every source over the "
             "whole frame, which takes as long as a frame per source.")
        .def("unresolved_taper", &Renderer::unresolved_taper)
        .def("set_fft_planning",
             &Renderer::set_fft_planning,
             py::arg("effort"),
             "Set how hard FFTW plans the transforms that convolve bodies with the PSF. "
             "FftPlanEffort.Estimate, the default, plans at once. FftPlanEffort.Measure finds "
             "faster transforms, which convolve a 1920 x 1080 Visible8 frame in about 15% less "
             "time, but takes 2 to 5 seconds the first time a size and precision are used in a "
             "process: worth it for long sequences of frames. Bodies are convolved in single "
             "precision, unless the rounding error it would spread over the frame reaches a tenth "
             "of the read noise in a pixel; then in double.")
        .def("fft_planning", &Renderer::fft_planning)
        .def("set_fft_rounding_tolerance",
             &Renderer::set_fft_rounding_tolerance,
             py::arg("fraction_of_read_noise"),
             "Set how much of the read noise (of an electron without read noise) the rounding "
             "of convolving bodies with the PSF in single precision may put in a pixel's "
             "readout before they are convolved in double precision instead. Single precision "
             "spreads a faint pattern of rounding over the frame in proportion to its brightest "
             "pixel: at the default, 0.1, lost in any one frame's read noise, but a square-root "
             "stretch of a noiseless frame to 30 electrons can show it. 0 always convolves in "
             "double precision.")
        .def("fft_rounding_tolerance", &Renderer::fft_rounding_tolerance)
        .def("set_region_culling", &Renderer::set_region_culling, py::arg("enable") = true)
        .def("set_region_cull_margin_scale",
             &Renderer::set_region_cull_margin_scale,
             py::arg("scale"))
        .def("set_region_cull_validation",
             &Renderer::set_region_cull_validation,
             py::arg("enable") = true)
        .def("__repr__", [](const Renderer&) { return "Renderer()"; })

        // Methods removed in earlier versions do not exist, so hasattr() is False for them, but
        // using one names what replaced it. (Python calls __getattr__ only for names it cannot
        // find otherwise.)
        .def("__getattr__", [](const py::object& self, const std::string& name) -> py::object {
            const auto& removed = detail::removed_renderer_methods();
            const auto it = removed.find(name);
            if (it != removed.end()) {
                throw py::attribute_error("API BREAKING CHANGE: " + name + " was removed in " +
                                          it->second);
            }
            throw py::attribute_error(
                "'" + py::str(py::type::of(self).attr("__name__")).cast<std::string>() +
                "' object has no attribute '" + name + "'");
        });
}

} // namespace huira
