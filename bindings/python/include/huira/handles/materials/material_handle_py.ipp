#pragma once

#include "huira/handles/handle_py.ipp"
#include "huira/handles/materials/bsdf_handle.hpp"
#include "huira/handles/materials/material_handle.hpp"
#include "pybind11/numpy.h"
#include "pybind11/pybind11.h"
#include "pybind11/stl.h"

namespace py = pybind11;

namespace huira {
template <IsSpectral TSpectral>
inline void bind_material_handle(py::module_& m)
{
    using HandleType = MaterialHandle<TSpectral>;

    auto cls =
        py::class_<HandleType>(m, "MaterialHandle")

            // --- BSDF ---
            // Python has BSDF handles (Scene.new_bsdf_lambertian() and the others), not the
            // C++ BSDF objects set_bsdf() takes:
            .def(
                "set_bsdf",
                [](HandleType& self, const BSDFHandle<TSpectral>& bsdf) {
                    self.set_bsdf(bsdf.get());
                },
                py::arg("bsdf"),
                "Assign a new BSDF to this material: a BSDF handle, as Scene.new_bsdf_lambertian() "
                "and the others return")

            // --- Albedo ---
            .def("set_albedo",
                 &HandleType::set_albedo,
                 py::arg("albedo"),
                 "Set a constant albedo, from 0 to 1 in each bin")
            .def("set_albedo_image",
                 &HandleType::set_albedo_image,
                 py::arg("albedo_texture"),
                 "Set the albedo texture")
            .def("set_albedo_factor",
                 &HandleType::set_albedo_factor,
                 py::arg("albedo_factor"),
                 "Multiply the albedo image by a factor from 0 to 1 in each bin")
            .def("reset_albedo", &HandleType::reset_albedo, "Reset the albedo to default")

            // --- Alpha ---
            .def("set_alpha",
                 &HandleType::set_alpha,
                 py::arg("alpha"),
                 "Set a constant opacity, from 0 (clear) to 1 (opaque)")
            .def("set_alpha_image",
                 &HandleType::set_alpha_image,
                 py::arg("alpha_texture"),
                 "Set the alpha texture")
            .def("set_alpha_factor",
                 &HandleType::set_alpha_factor,
                 py::arg("alpha_factor"),
                 "Multiply the alpha image by a factor from 0 to 1")
            .def("reset_alpha", &HandleType::reset_alpha, "Reset alpha to default")

            // --- Metallic ---
            .def("set_metallic",
                 &HandleType::set_metallic,
                 py::arg("metallic"),
                 "Set a constant metalness, from 0 (dielectric) to 1 (metal)")
            .def("set_metallic_image",
                 &HandleType::set_metallic_image,
                 py::arg("metallic_texture"),
                 "Set the metallic texture")
            .def("set_metallic_factor",
                 &HandleType::set_metallic_factor,
                 py::arg("metallic_factor"),
                 "Multiply the metallic image by a factor from 0 to 1")
            .def("reset_metallic", &HandleType::reset_metallic, "Reset metallic to default")

            // --- Roughness ---
            .def("set_roughness",
                 &HandleType::set_roughness,
                 py::arg("roughness"),
                 "Set a constant roughness, from 0 (mirror) to 1 (rough)")
            .def("set_roughness_image",
                 &HandleType::set_roughness_image,
                 py::arg("roughness_texture"),
                 "Set the roughness texture")
            .def("set_roughness_factor",
                 &HandleType::set_roughness_factor,
                 py::arg("roughness_factor"),
                 "Multiply the roughness image by a factor from 0 to 1")
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

            // --- Transmission ---
            .def("set_transmission",
                 &HandleType::set_transmission,
                 py::arg("transmission"),
                 "Set a constant transmission: the fraction of light that passes through, "
                 "from 0 to 1 in each bin")
            .def("set_transmission_image",
                 &HandleType::set_transmission_image,
                 py::arg("transmission_texture"),
                 "Set the transmission texture")
            .def("set_transmission_factor",
                 &HandleType::set_transmission_factor,
                 py::arg("transmission_factor"),
                 "Multiply the transmission image by a factor from 0 to 1 in each bin")
            .def("reset_transmission",
                 &HandleType::reset_transmission,
                 "Reset transmission to default")

            // --- Emissive ---
            .def("set_emissive",
                 &HandleType::set_emissive,
                 py::arg("emissive"),
                 "Set a constant emission: the radiance the surface emits, from either side, "
                 "in W/(m^2 sr) in each spectral bin. Finite and non-negative.")
            .def("set_emissive_image",
                 &HandleType::set_emissive_image,
                 py::arg("emissive_texture"),
                 "Set the emissive texture, in W/(m^2 sr) per bin; shown as it is until a "
                 "factor is set")
            .def("set_emissive_factor",
                 &HandleType::set_emissive_factor,
                 py::arg("emissive_factor"),
                 "Multiply the emissive image by a finite, non-negative factor")
            .def("reset_emissive",
                 &HandleType::reset_emissive,
                 "Remove the emissive texture and factor: the surface emits nothing")

            // --- Handle basics ---
            .def("__bool__", &HandleType::valid)
            .def("__repr__", [](const HandleType&) { return "<MaterialHandle>"; });

    bind_handle_methods<Material<TSpectral>>(cls);
}
} // namespace huira
