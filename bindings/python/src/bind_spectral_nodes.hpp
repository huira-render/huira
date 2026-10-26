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
#include "huira/handles/scene/frame_handle_py.ipp"
#include "huira/handles/scene/instance_handle_py.ipp"
#include "huira/handles/scene/root_frame_handle_py.ipp"

namespace huira::python {

/// The third part of a spectral submodule (see bindings.hpp): the scene node handles.
template <IsSpectral TSpectral>
void bind_spectral_nodes(pybind11::module_& m)
{
    huira::bind_instance_handle<TSpectral>(m);
    huira::bind_frame_handle<TSpectral>(m);
    huira::bind_root_frame_handle<TSpectral>(m);
}

} // namespace huira::python
