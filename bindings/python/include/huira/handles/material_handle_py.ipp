#pragma once

#include "huira/handles/material_handle.hpp"
#include "pybind11/numpy.h"
#include "pybind11/pybind11.h"
#include "pybind11/stl.h"

namespace py = pybind11;

namespace huira {
template <IsSpectral TSpectral>
inline void bind_material_handle(py::module_& m)
{
    using HandleType = MaterialHandle<TSpectral>;

    py::class_<HandleType>(m, "MaterialHandle")

        // --- BSDF ---
        .def("set_bsdf",
             &HandleType::set_bsdf,
             py::arg("bsdf"),
             "Assign a new BSDF to this material")

        // --- Albedo ---
        .def("set_albedo",
             &HandleType::set_albedo,
             py::arg("albedo_texture"),
             "Set the albedo texture")
        .def("set_albedo_factor",
             &HandleType::set_albedo_factor,
             py::arg("albedo_factor"),
             "Set the albedo factor")
        .def("reset_albedo", &HandleType::reset_albedo, "Reset the albedo to default")

        // --- Metallic ---
        .def("set_metallic_image",
             &HandleType::set_metallic_image,
             py::arg("metallic_texture"),
             "Set the metallic texture")
        .def("set_metallic_factor",
             &HandleType::set_metallic_factor,
             py::arg("metallic_factor"),
             "Set the metallic factor")
        .def("reset_metallic", &HandleType::reset_metallic, "Reset metallic to default")

        // --- Roughness ---
        .def("set_roughness_image",
             &HandleType::set_roughness_image,
             py::arg("roughness_texture"),
             "Set the roughness texture")
        .def("set_roughness_factor",
             &HandleType::set_roughness_factor,
             py::arg("roughness_factor"),
             "Set the roughness factor")
        .def("reset_roughness", &HandleType::reset_roughness, "Reset roughness to default")

        // --- Normal ---
        .def("set_normal_image",
             &HandleType::set_normal_image,
             py::arg("normal_texture"),
             "Set the normal map texture")
        .def("set_normal_factor",
             &HandleType::set_normal_factor,
             py::arg("normal_factor"),
             "Set the normal map strength factor")
        .def("reset_normal", &HandleType::reset_normal, "Reset normal map to default")

        // --- Emissive ---
        .def("set_emissive_image",
             &HandleType::set_emissive_image,
             py::arg("emissive_texture"),
             "Set the emissive texture")
        .def("set_emissive_factor",
             &HandleType::set_emissive_factor,
             py::arg("emissive_factor"),
             "Set the emissive factor")
        .def("reset_emissive", &HandleType::reset_emissive, "Reset emissive to default")

        // --- Handle basics ---
        .def("valid", &HandleType::valid)
        .def("__bool__", &HandleType::valid)
        .def("__repr__", [](const HandleType&) { return "<MaterialHandle>"; });
}
} // namespace huira
