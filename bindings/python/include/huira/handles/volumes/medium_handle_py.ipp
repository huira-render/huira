#pragma once

#include "huira/handles/handle_py.ipp"
#include "huira/handles/volumes/medium_handle.hpp"
#include "pybind11/pybind11.h"
#include "pybind11/stl.h"

namespace py = pybind11;

namespace huira {
/**
 * @brief Registers MediumHandle<TSpectral> as a Python class.
 */
template <IsSpectral TSpectral>
inline void bind_medium_handle(py::module_& m)
{
    using HandleType = MediumHandle<TSpectral>;

    auto cls = py::class_<HandleType>(m, "MediumHandle")
                   .def("__bool__", &HandleType::valid)
                   .def("__repr__", [](const HandleType&) { return "<MediumHandle>"; });

    bind_handle_methods<Medium<TSpectral>>(cls);
}
} // namespace huira
