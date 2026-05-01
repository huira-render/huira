#pragma once

#include "huira/handles/assets/light_handle.hpp"
#include "pybind11/pybind11.h"

namespace py = pybind11;

namespace huira {
template <typename TSpectral>
inline void bind_light_handle(py::module_& m)
{
    using HandleType = LightHandle<TSpectral>;

    py::class_<HandleType>(m, "LightHandle")
        .def("valid", &HandleType::valid)
        .def("__bool__", &HandleType::valid)
        .def("__repr__", [](const HandleType&) { return "<LightHandle>"; });
}
} // namespace huira
