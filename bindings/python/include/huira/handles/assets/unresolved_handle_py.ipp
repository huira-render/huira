#pragma once

#include "huira/assets/unresolved/unresolved_object.hpp"
#include "huira/assets/unresolved/unresolved_sphere.hpp"
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

    auto cls =
        py::class_<HandleType>(m, "UnresolvedObjectHandle")
            .def("set_irradiance",
                 py::overload_cast<const units::SpectralWattsPerMeterSquared<TSpectral>&>(
                     &HandleType::set_irradiance, py::const_),
                 py::arg("irradiance"),
                 "Set spectral irradiance")
            .def("set_irradiance",
                 py::overload_cast<const units::WattsPerMeterSquared&>(&HandleType::set_irradiance,
                                                                       py::const_),
                 py::arg("irradiance"),
                 "Set scalar irradiance")
            .def("get_irradiance", &HandleType::get_irradiance, py::arg("time"))

            // Shading sample counts. These are properties of an
            // UnresolvedLambertianSphere; get<>() raises if the underlying
            // object is a different unresolved type.
            .def(
                "set_light_samples",
                [](const HandleType& h, std::size_t samples) {
                    h.template get<UnresolvedLambertianSphere<TSpectral>>()->set_light_samples(
                        samples);
                },
                py::arg("samples"),
                "Set the number of shadow samples taken per emitting light")
            .def(
                "set_indirect_source_samples",
                [](const HandleType& h, std::size_t samples) {
                    h.template get<UnresolvedLambertianSphere<TSpectral>>()
                        ->set_indirect_source_samples(samples);
                },
                py::arg("samples"),
                "Set the number of samples taken per designated indirect source")

            .def("__bool__", &HandleType::valid)
            .def("__repr__", [](const HandleType&) { return "<UnresolvedObjectHandle>"; });

    bind_handle_methods<UnresolvedObject<TSpectral>>(cls);
}
} // namespace huira
