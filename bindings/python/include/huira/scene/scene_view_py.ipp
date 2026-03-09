#pragma once

#include "pybind11/pybind11.h"

#include "huira/core/interval.hpp"
#include "huira/scene/scene_view.hpp"
#include "huira/scene/scene_view_types.hpp"

namespace py = pybind11;

namespace huira {

    template <IsSpectral TSpectral>
    void bind_scene_view(py::module_& m) {
        using SV = SceneView<TSpectral>;

        py::class_<SV>(m, "SceneView")
            .def(py::init<const Scene<TSpectral>&,
                const Interval&,
                const InstanceHandle<TSpectral>&,
                ObservationMode,
                std::size_t>(),
                py::arg("scene"),
                py::arg("exposure_interval"),
                py::arg("camera_instance"),
                py::arg("observation_mode"),
                py::arg("num_temporal_samples") = 1)

            // Ray tracing
            .def("intersect", &SV::intersect,
                py::arg("ray"), py::arg("time") = 0.5f,
                "Trace a ray against the scene and return a HitRecord")
            .def("occluded", &SV::occluded,
                py::arg("ray"), py::arg("t_far"), py::arg("time") = 0.5f,
                "Test whether a ray is occluded within distance t_far")
            .def("resolve_hit", &SV::resolve_hit,
                py::arg("ray"), py::arg("hit"),
                "Resolve a HitRecord into a full Interaction")

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

}