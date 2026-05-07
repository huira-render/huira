#pragma once

#include "huira/handles/assets/primitive_handle.hpp"
#include "huira/units/units_py.ipp"
#include "pybind11/pybind11.h"
#include "pybind11/stl.h"

#include "huira/handles/handle_py.ipp"

namespace py = pybind11;

namespace huira {
/**
 * @brief Registers PrimitiveHandle<TSpectral> as a Python class.
 */
template <typename TSpectral>
inline void bind_primitive_handle(py::module_& m)
{
    using HandleType = PrimitiveHandle<TSpectral>;

    auto cls = py::class_<HandleType>(m, "PrimitiveHandle")
        // --- Handle basics ---
        .def("__bool__", &HandleType::valid)
        .def("__repr__", [](const HandleType&) { return "<PrimitiveHandle>"; });

    bind_handle_methods<Primitive<TSpectral>>(cls);
}
} // namespace huira
