#pragma once

#include "huira/handles/assets/light_handle.hpp"
#include "huira/handles/handle_py.ipp"
#include "pybind11/pybind11.h"

namespace py = pybind11;

namespace huira {
template <typename TSpectral>
inline void bind_light_handle(py::module_& m)
{
    using HandleType = LightHandle<TSpectral>;

    auto cls = py::class_<HandleType>(m, "LightHandle")
                   .def("__bool__", &HandleType::valid)
                   .def("__repr__", [](const HandleType&) { return "<LightHandle>"; });

    bind_handle_methods<Light<TSpectral>>(cls);
}
} // namespace huira
