#pragma once

#include "huira/handles/handle_py.ipp"
#include "huira/handles/volumes/density_field_handle.hpp"
#include "pybind11/pybind11.h"
#include "pybind11/stl.h"

namespace py = pybind11;

namespace huira {
/**
 * @brief Registers DensityFieldHandle<TSpectral> as a Python class.
 */
template <IsSpectral TSpectral>
inline void bind_density_field_handle(py::module_& m)
{
    using HandleType = DensityFieldHandle<TSpectral>;

    auto cls = py::class_<HandleType>(m, "DensityFieldHandle")
                   .def("__bool__", &HandleType::valid)
                   .def("__repr__", [](const HandleType&) { return "<DensityFieldHandle>"; });

    bind_handle_methods<DensityField<TSpectral>>(cls);
}
} // namespace huira
