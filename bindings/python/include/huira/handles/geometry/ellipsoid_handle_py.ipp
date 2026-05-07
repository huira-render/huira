#pragma once

#include "huira/handles/geometry/ellipsoid_handle.hpp"
#include "pybind11/pybind11.h"
#include "pybind11/stl.h"

#include "huira/handles/handle_py.ipp"

namespace py = pybind11;

namespace huira {
/**
 * @brief Registers EllipsoidHandle<TSpectral> as a Python class.
 */
template <typename TSpectral>
inline void bind_ellipsoid_handle(py::module_& m)
{
    using HandleType = EllipsoidHandle<TSpectral>;

    auto cls = py::class_<HandleType, GeometryHandle<TSpectral>>(m, "EllipsoidHandle")
        // --- Handle basics ---
        .def("__bool__", &HandleType::valid)
        .def("__repr__", [](const HandleType&) { return "<EllipsoidHandle>"; });

    bind_handle_methods<Ellipsoid<TSpectral>>(cls);
}
} // namespace huira
