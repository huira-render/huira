#pragma once

#include "pybind11/pybind11.h"

#include "huira/materials/bsdfs/bsdf.hpp"

namespace py = pybind11;

namespace huira {

    template <IsSpectral TSpectral>
    inline void bind_bsdfs(py::module_& m) {
        
        py::class_<BSDF<TSpectral>>(m, "BSDF",
            "Abstract base class for all Bidirectional Scattering Distribution Functions");
        
        py::class_<CookTorranceBSDF<TSpectral>, BSDF<TSpectral>>(m, "CookTorranceBSDF")
            .def(py::init<>(), "Create a Cook-Torrance microfacet BSDF");

        py::class_<LambertBSDF<TSpectral>, BSDF<TSpectral>>(m, "LambertBSDF")
            .def(py::init<>(), "Create a Lambertian (perfectly diffuse) BSDF");

        py::class_<LommelSeeligerBSDF<TSpectral>, BSDF<TSpectral>>(m, "LommelSeeligerBSDF")
            .def(py::init<>(), "Create a Lommel-Seeliger BSDF for dark, porous particulate surfaces");

        py::class_<McEwenBSDF<TSpectral>, BSDF<TSpectral>>(m, "McEwenBSDF")
            .def(py::init<>(), "Create a McEwen BSDF for photometric planetary surface rendering");

        py::class_<OrenNayarBSDF<TSpectral>, BSDF<TSpectral>>(m, "OrenNayarBSDF")
            .def(py::init<>(), "Create an Oren-Nayar BSDF for rough diffuse surfaces");
        
        py::class_<HapkeBSDF<TSpectral>, BSDF<TSpectral>>(m, "HapkeBSDF")
            .def(py::init<float, float, float, float>(),
                py::arg("h") = 0.05f,
                py::arg("B0") = 1.0f,
                py::arg("b") = 0.4f,
                py::arg("c") = 0.5f,
                "Create a 5-Parameter Hapke BSDF for planetary regolith.\n"
                "Parameters:\n"
                "  h: Opposition effect width\n"
                "  B0: Opposition effect amplitude\n"
                "  b: DHG phase function asymmetry\n"
                "  c: DHG phase function forward/backward fraction");
    }

}
