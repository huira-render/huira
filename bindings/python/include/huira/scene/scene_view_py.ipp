#pragma once

#include "pybind11/pybind11.h"

#include "huira/scene/scene_view.hpp"
#include "huira/scene/scene_view_types.hpp"

namespace py = pybind11;

namespace huira {

    template <IsSpectral TSpectral>
    void bind_scene_view(py::module_& m) {
        using SV = SceneView<TSpectral>;

        py::class_<SV>(m, "SceneView")
            .def(py::init<const Scene<TSpectral>&,
                const Time&,
                const InstanceHandle<TSpectral>&,
                ObservationMode>(),
                py::arg("scene"),
                py::arg("time"),
                py::arg("camera_instance"),
                py::arg("observation_mode"))
            .def("get_time", &SV::get_time)
            .def("__repr__", [](const SV& sv) {
            std::ostringstream os;
            os << "SceneView(time=" << sv.get_time().to_utc_string() << ")";
            return os.str();
                })
            ;
    }

}
