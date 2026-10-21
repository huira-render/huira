#pragma once

#include "huira/handles/handle_py.ipp"
#include "huira/handles/scene/instance_handle.hpp"
#include "huira/handles/scene/node_handle_py.ipp"
#include "pybind11/pybind11.h"
#include "pybind11/stl.h"

namespace py = pybind11;

namespace huira {
template <typename TSpectral>
inline void bind_instance_handle(py::module_& m)
{
    using HandleType = InstanceHandle<TSpectral>;

    auto cls =
        py::class_<HandleType>(m, "InstanceHandle")
            .def("__bool__", &HandleType::valid)
            .def("__repr__", [](const HandleType&) { return "<InstanceHandle>"; })
            // Bind the overload taking another InstanceHandle target
            .def("look_at",
                 py::overload_cast<const InstanceHandle<TSpectral>&,
                                   const Vec3<double>&,
                                   std::optional<ObservationMode>>(&HandleType::look_at),
                 py::arg("target"),
                 py::arg("up") = Vec3<double>{0.0, 0.0, 1.0},
                 py::arg("observation_mode") = py::none(),
                 "Look at another instance, following it through the exposure. It is aimed at "
                 "where it is seen in the given observation mode, or by default (None) in the "
                 "scene view's, which keeps it where the scene view renders it.")
            // Bind the overload taking a static target position
            .def("look_at",
                 py::overload_cast<const Vec3<double>&,
                                   const Vec3<double>&,
                                   TargetFrame,
                                   std::optional<ObservationMode>>(&HandleType::look_at),
                 py::arg("target_position"),
                 py::arg("up") = Vec3<double>{0.0, 0.0, 1.0},
                 py::arg("frame") = TargetFrame::PARENT,
                 py::arg("observation_mode") = ObservationMode::TRUE_STATE,
                 "Look at a fixed point, in meters: in the parent's frame (TargetFrame.PARENT, "
                 "the default), so that it moves with the parent, or in the scene's root frame "
                 "(TargetFrame.ROOT). It is aimed at where it is (TRUE_STATE, the default), "
                 "shifted by the aberration of the instance's velocity (ABERRATED_STATE), or, "
                 "given None, as the scene view observes.")
            // Designate this instance (which must contain a Primitive or a Model)
            // as an indirect illumination source that is explicitly sampled during
            // rendering, rather than relying on it being found by chance.
            .def("set_indirect_source",
                 &HandleType::set_indirect_source,
                 py::arg("enabled") = true,
                 "Designate this instance as an indirect illumination source")
            .def("is_indirect_source",
                 &HandleType::is_indirect_source,
                 "Whether this instance is designated as an indirect illumination source");

    bind_handle_methods<Instance<TSpectral>>(cls);
    bind_node_handle_methods<TSpectral, Instance<TSpectral>>(cls);
}
} // namespace huira
