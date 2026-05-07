#pragma once

#include "huira/handles/assets/unresolved_handle.hpp"
#include "huira/handles/handle_py.ipp"
#include "huira/units/units_py.ipp"
#include "pybind11/pybind11.h"
#include "pybind11/stl.h"

namespace py = pybind11;

namespace huira {
template <typename TSpectral>
inline void bind_unresolved_object_handle(py::module_& m)
{
    using HandleType = UnresolvedObjectHandle<TSpectral>;

    auto cls = py::class_<HandleType>(m, "UnresolvedObjectHandle")
                   .def("set_irradiance",
                        py::overload_cast<const units::SpectralWattsPerMeterSquared<TSpectral>&>(
                            &HandleType::set_irradiance, py::const_),
                        py::arg("irradiance"),
                        "Set spectral irradiance")
                   .def("set_irradiance",
                        py::overload_cast<const units::WattsPerMeterSquared&>(
                            &HandleType::set_irradiance, py::const_),
                        py::arg("irradiance"),
                        "Set scalar irradiance")
                   .def("get_irradiance", &HandleType::get_irradiance, py::arg("time"))

                   .def("__bool__", &HandleType::valid)
                   .def("__repr__", [](const HandleType&) { return "<UnresolvedObjectHandle>"; });

    bind_handle_methods<UnresolvedObject<TSpectral>>(cls);
}
} // namespace huira
