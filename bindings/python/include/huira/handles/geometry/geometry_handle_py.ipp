#pragma once

#include "huira/handles/geometry/geometry_handle.hpp"
#include "pybind11/pybind11.h"
#include "pybind11/stl.h"

#include "huira/handles/handle_py.ipp"

namespace py = pybind11;

namespace huira {
/**
 * @brief Registers GeometryHandle<TSpectral> as a Python class.
 */
template <typename TSpectral>
inline void bind_geometry_handle(py::module_& m)
{
    using HandleType = GeometryHandle<TSpectral>;

    auto cls = py::class_<HandleType>(m, "GeometryHandle")
        // --- Handle basics ---
        .def("__bool__", &HandleType::valid)
        .def("__repr__", [](const HandleType&) { return "<GeometryHandle>"; });

    bind_handle_methods<Geometry<TSpectral>>(cls);
}
} // namespace huira
