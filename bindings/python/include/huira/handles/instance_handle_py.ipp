#pragma once

#include "pybind11/pybind11.h"
#include "pybind11/stl.h"

#include "huira/handles/instance_handle.hpp"
#include "huira/handles/node_handle_py.ipp"

namespace py = pybind11;

namespace huira {
    template <typename TSpectral>
    inline void bind_instance_handle(py::module_& m) {
        using HandleType = InstanceHandle<TSpectral>;

        auto cls = py::class_<HandleType>(m, "InstanceHandle")
            .def("valid", &HandleType::valid)
            .def("__bool__", &HandleType::valid)
            .def("__repr__", [](const HandleType&) {
                return "<InstanceHandle>";
            });

        bind_node_handle_methods<TSpectral, Instance<TSpectral>>(cls);
    }
}
