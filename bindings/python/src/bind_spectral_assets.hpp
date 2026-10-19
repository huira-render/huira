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
#include "huira/core/spectral_bins_py.ipp"
#include "huira/geometry/vertex_py.ipp"
#include "huira/handles/assets/light_handle_py.ipp"
#include "huira/handles/assets/model_handle_py.ipp"
#include "huira/handles/assets/primitive_handle_py.ipp"
#include "huira/handles/assets/unresolved_handle_py.ipp"
#include "huira/handles/camera_handle_py.ipp"
#include "huira/handles/geometry/ellipsoid_handle_py.ipp"
#include "huira/handles/geometry/geometry_handle_py.ipp"
#include "huira/handles/geometry/mesh_handle_py.ipp"
#include "huira/units/units_py.ipp"

namespace huira::python {

/// The first part of a spectral submodule (see bindings.hpp): its units and spectral
/// bins, the vertex, and the geometry and asset handles.
template <IsSpectral TSpectral>
void bind_spectral_assets(pybind11::module_& m)
{
    huira::bind_spectral_units_for_type<TSpectral>(m);
    huira::bind_spectral_bins<TSpectral>(m);

    // --- Geometry ---
    // The vertices Scene.add_mesh() takes. After the spectral bins, which a vertex's albedo is.
    huira::bind_vertex<TSpectral>(m);

    // --- Geometry handles ---
    huira::bind_geometry_handle<TSpectral>(m);
    huira::bind_mesh_handle<TSpectral>(m);
    huira::bind_ellipsoid_handle<TSpectral>(m);

    // --- Asset handles ---
    huira::bind_primitive_handle<TSpectral>(m);
    huira::bind_camera_model_handle<TSpectral>(m);
    huira::bind_light_handle<TSpectral>(m);
    huira::bind_unresolved_object_handle<TSpectral>(m);
    huira::bind_model_handle<TSpectral>(m);
}

} // namespace huira::python
