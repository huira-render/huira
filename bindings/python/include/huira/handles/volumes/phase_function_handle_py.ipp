#pragma once

#include "huira/handles/handle_py.ipp"
#include "huira/handles/volumes/phase_function_handle.hpp"
#include "pybind11/pybind11.h"
#include "pybind11/stl.h"

namespace py = pybind11;

namespace huira {
/**
 * @brief Registers PhaseFunctionHandle<TSpectral> as a Python class.
 */
template <IsSpectral TSpectral>
inline void bind_phase_function_handle(py::module_& m)
{
    using HandleType = PhaseFunctionHandle<TSpectral>;

    auto cls = py::class_<HandleType>(m, "PhaseFunctionHandle")
                   .def("__bool__", &HandleType::valid)
                   .def("__repr__", [](const HandleType&) { return "<PhaseFunctionHandle>"; });

    bind_handle_methods<PhaseFunction<TSpectral>>(cls);
}
} // namespace huira
