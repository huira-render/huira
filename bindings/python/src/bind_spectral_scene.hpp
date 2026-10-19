#pragma once

// What every part of a spectral submodule needs before its bindings, kept first, out of the
// sorted includes: the type casters, as pybind11 converts a type only where its caster has been
// seen, and the parts must all convert alike; and the whole library, whose classes refer to one
// another (a node to its scene, say) and are complete only together.
// clang-format off
#include "huira/huira.hpp"
#include "pybind11/numpy.h"
#include "pybind11/operators.h"
#include "pybind11/pybind11.h"
#include "pybind11/stl.h"
#include "pybind11/stl/filesystem.h"
// clang-format on

#include "huira/concepts/spectral_concepts.hpp"
#include "huira/scene/scene_py.ipp"

namespace huira::python {

/// The fourth part of a spectral submodule (see bindings.hpp): the scene.
template <IsSpectral TSpectral>
void bind_spectral_scene(pybind11::module_& m)
{
    huira::bind_scene<TSpectral>(m);
}

} // namespace huira::python
