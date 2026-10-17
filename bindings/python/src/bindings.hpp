#pragma once

#include "pybind11/pybind11.h"

namespace huira::python {

// The module's bindings, split over several translation units: compiled as one, they took more
// memory than many machines have. Each function is defined in its own source file, and
// huira_module.cpp calls them in this order.

/// Units, time, rotations, SPICE, logging, the images and enums shared by every spectral type.
/// Defined in core.cpp.
void bind_core(pybind11::module_& m);

/// The huira.rgb, huira.visible8 and huira.swir8 submodules, which need bind_core() first.
/// Defined in rgb.cpp, visible8.cpp and swir8.cpp.
void bind_rgb(pybind11::module_& m);
void bind_visible8(pybind11::module_& m);
void bind_swir8(pybind11::module_& m);

} // namespace huira::python
