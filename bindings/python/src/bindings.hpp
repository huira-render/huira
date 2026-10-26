#pragma once

#include "pybind11/pybind11.h"

namespace huira::python {

// The module's bindings, split over several translation units: compiled as one, or one for each
// spectral type, they took more memory than many machines have. huira_module.cpp binds them in
// this order.
//
// A class is bound before any function that takes or returns it: pybind11 writes a function's
// signature, for help() and the API docs, when the function is bound, and names a class that is
// not bound yet by its C++ type (huira::Image<huira::SpectralBins<...>>). Run
// tools/check_python_signatures.py to find any.

/// Units, time, rotations, SPICE, logging, the images and enums shared by every spectral type.
/// Defined in core.cpp.
void bind_core(pybind11::module_& m);

// The huira.rgb, huira.visible8 and huira.swir8 submodules, which need bind_core() first, are
// each bound in parts, in order: bind_spectral_values(), bind_spectral_assets(),
// bind_spectral_nodes(), bind_spectral_scene() and bind_spectral_render(), in the headers of
// the same names. CMake compiles each part for each spectral type in a file of its own, and
// declares the parts and the submodules (bind_rgb() and the others) in spectral_submodules.hpp.
// The bindings' CMakeLists.txt lists the submodules and the parts.

/// The functions shared by every spectral type that take or return a spectral submodule's
/// classes (reading and writing images, which are RGB): bound last. Defined in core.cpp.
void bind_core_functions(pybind11::module_& m);

} // namespace huira::python
