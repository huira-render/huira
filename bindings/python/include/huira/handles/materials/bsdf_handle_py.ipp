#pragma once

#include "huira/handles/handle_py.ipp"
#include "huira/handles/materials/bsdf_handle.hpp"
#include "pybind11/pybind11.h"
#include "pybind11/stl.h"

namespace py = pybind11;

namespace huira {
/**
 * @brief Registers BSDFHandle<TSpectral> as a Python class.
 */
template <typename TSpectral>
inline void bind_bsdf_handle(py::module_& m)
{
    using HandleType = BSDFHandle<TSpectral>;

    auto cls = py::class_<HandleType>(m, "BSDFHandle")
                   // --- Handle basics ---
                   .def("__bool__", &HandleType::valid)
                   .def("__repr__", [](const HandleType&) { return "<BSDFHandle>"; });

    bind_handle_methods<BSDF<TSpectral>>(cls);
}
} // namespace huira
