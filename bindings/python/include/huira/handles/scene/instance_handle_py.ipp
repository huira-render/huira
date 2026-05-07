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

    auto cls = py::class_<HandleType>(m, "InstanceHandle")
                   .def("__bool__", &HandleType::valid)
                   .def("__repr__", [](const HandleType&) { return "<InstanceHandle>"; });

    bind_handle_methods<Instance<TSpectral>>(cls);
    bind_node_handle_methods<TSpectral, Instance<TSpectral>>(cls);
}
} // namespace huira
