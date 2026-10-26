#pragma once

#include <cstdint>
#include <optional>

#include "huira/core/interval.hpp"
#include "huira/sampling/sampler.hpp"
#include "huira/scene/scene_view.hpp"
#include "huira/scene/scene_view_types.hpp"
#include "huira/volumes/medium_stack.hpp"
#include "pybind11/pybind11.h"
#include "pybind11/stl.h"

namespace py = pybind11;

namespace huira {

template <IsSpectral TSpectral>
void bind_scene_view(py::module_& m)
{
    using SV = SceneView<TSpectral>;

    py::class_<SV>(m, "SceneView")
        .def(py::init<const Scene<TSpectral>&,
                      const Interval&,
                      const InstanceHandle<TSpectral>&,
                      ObservationMode,
                      std::optional<std::size_t>,
                      double>(),
             py::arg("scene"),
             py::arg("exposure_interval"),
             py::arg("camera_instance"),
             py::arg("observation_mode") = ObservationMode::ABERRATED_STATE,
             py::arg("num_temporal_samples") = py::none(),
             py::arg("motion_tolerance") = SV::DEFAULT_MOTION_TOLERANCE,
             "A view of the scene over an exposure, from a camera. The observation mode says "
             "where each thing is seen: ABERRATED_STATE (the default), with light time and the "
             "aberration from the camera's velocity, as a camera really sees it; GEOMETRIC_STATE, "
             "with light time alone; or TRUE_STATE, where it is at the instant. With "
             "num_temporal_samples None "
             "(the default), each thing in view is given as many temporal samples as its motion "
             "needs, to be drawn within motion_tolerance pixels of its path: one, a still frame, "
             "if it does not move in view. A number gives everything that many; 1 renders a "
             "still frame, as does turning the camera's motion blur off "
             "(enable_motion_blur(False)), whatever the number.")
        .def("has_motion_blur",
             &SV::has_motion_blur,
             "Whether anything rays can hit moves in view during the exposure, as sampled")
        .def("max_temporal_samples",
             &SV::max_temporal_samples,
             "The most temporal samples anything in the view was given")

        // Ray tracing — single ray
        .def("intersect",
             static_cast<HitRecord (SV::*)(const Ray<TSpectral>&, float, unsigned int) const>(
                 &SV::intersect),
             py::arg("ray"),
             py::arg("time") = 0.5f,
             py::arg("mask") = 0xFFFFFFFF,
             "Trace a ray against the scene and return a HitRecord")
        // The medium stack and sampler of the C++ function have no Python classes: the ray starts
        // outside any medium, and the sampler is made here, from a seed if one is given.
        .def(
            "evaluate_transmittance",
            [](const SV& self,
               const Ray<TSpectral>& shadow_ray,
               float t_far,
               float time,
               AlphaMode alpha_mode,
               std::optional<std::uint32_t> seed) {
                RandomSampler<float> sampler =
                    seed ? RandomSampler<float>(*seed) : RandomSampler<float>();
                return self.evaluate_transmittance(
                    shadow_ray, t_far, MediumStack<TSpectral>{}, sampler, time, alpha_mode);
            },
            py::arg("shadow_ray"),
            py::arg("t_far"),
            py::arg("time") = 0.5f,
            py::arg("alpha_mode") = AlphaMode::Stochastic,
            py::arg("seed") = py::none(),
            "The transmittance along a shadow ray, out to t_far, through participating media and "
            "partly transparent surfaces. The ray starts outside any medium. With "
            "AlphaMode.STOCHASTIC, partly transparent surfaces are sampled at random: give a seed "
            "to repeat the result.")
        .def("resolve_hit",
             &SV::resolve_hit,
             py::arg("ray"),
             py::arg("hit"),
             "Resolve a HitRecord into a full Interaction")
        .def("object_ids",
             &SV::object_ids,
             py::arg("hit"),
             "The IDs of the geometry, model, instance and part a HitRecord landed on, as an "
             "ObjectIds: each NO_OBJECT_ID for a miss or a light")
        .def("unresolved_irradiance",
             &SV::unresolved_irradiance,
             py::arg("instance"),
             py::arg("time"),
             "The irradiance an instance of an unresolved object gives the camera at a time, as "
             "this view resolved it")

        // Ray tracing — batch
        .def("intersect_batch",
             static_cast<std::vector<HitRecord> (SV::*)(const std::vector<Ray<TSpectral>>&, float)
                             const>(&SV::intersect),
             py::arg("rays"),
             py::arg("time") = 0.5f,
             "Trace a batch of rays against the scene and return HitRecords")
        .def("resolve_hits_batch",
             &SV::resolve_hits,
             py::arg("rays"),
             py::arg("hits"),
             "Resolve batches of HitRecords into Interactions")

        // Geometry occupancy
        .def("tlas_is_empty",
             &SV::tlas_is_empty,
             "True when the TLAS contains nothing that a ray could ever hit")
        .def("occupancy_bounds_complete",
             &SV::occupancy_bounds_complete,
             "False when the occupancy bounds are not usable for region culling")

        // Exposure / timing
        .def("get_exposure_interval", &SV::get_exposure_interval)
        .def("duration", &SV::duration)
        .def("get_time", &SV::get_time)
        .def("get_start_time", &SV::get_start_time)
        .def("get_end_time", &SV::get_end_time)

        .def("__repr__", [](const SV& sv) {
            std::ostringstream os;
            os << "SceneView(time=" << sv.get_time().to_utc_string() << ")";
            return os.str();
        });
}

} // namespace huira
