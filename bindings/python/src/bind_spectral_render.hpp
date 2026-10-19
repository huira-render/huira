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
#include "huira/render/interaction_py.ipp"
#include "huira/render/ray_py.ipp"
#include "huira/render/renderer_py.ipp"
#include "huira/scene/scene_view_py.ipp"

namespace huira::python {

/// The last part of a spectral submodule (see bindings.hpp): rays and interactions, the scene
/// view and the renderer.
template <IsSpectral TSpectral>
void bind_spectral_render(pybind11::module_& m)
{
    huira::bind_ray<TSpectral>(m);
    huira::bind_interaction<TSpectral>(m);
    huira::bind_scene_view<TSpectral>(m);
    huira::bind_renderer<TSpectral>(m);
}

} // namespace huira::python
