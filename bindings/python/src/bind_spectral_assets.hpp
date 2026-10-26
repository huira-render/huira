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
#include "huira/geometry/vertex_py.ipp"
#include "huira/handles/assets/light_handle_py.ipp"
#include "huira/handles/assets/model_handle_py.ipp"
#include "huira/handles/assets/primitive_handle_py.ipp"
#include "huira/handles/assets/unresolved_handle_py.ipp"
#include "huira/handles/camera_handle_py.ipp"
#include "huira/handles/geometry/ellipsoid_handle_py.ipp"
#include "huira/handles/geometry/geometry_handle_py.ipp"
#include "huira/handles/geometry/mesh_handle_py.ipp"
#include "huira/handles/materials/bsdf_handle_py.ipp"
#include "huira/handles/materials/material_handle_py.ipp"
#include "huira/handles/volumes/density_field_handle_py.ipp"
#include "huira/handles/volumes/medium_handle_py.ipp"
#include "huira/handles/volumes/phase_function_handle_py.ipp"

namespace huira::python {

/// The second part of a spectral submodule (see bindings.hpp): the vertex, and the geometry,
/// material, volume and asset handles.
template <IsSpectral TSpectral>
void bind_spectral_assets(pybind11::module_& m)
{
    // --- Geometry ---
    // The vertices Scene.add_mesh() takes.
    huira::bind_vertex<TSpectral>(m);

    // --- Geometry handles ---
    huira::bind_geometry_handle<TSpectral>(m);
    huira::bind_mesh_handle<TSpectral>(m);
    huira::bind_ellipsoid_handle<TSpectral>(m);

    // --- Material handles ---
    // Before the model handle, which takes and returns them.
    huira::bind_bsdf_handle<TSpectral>(m);
    huira::bind_material_handle<TSpectral>(m);

    // --- Volume handles ---
    huira::bind_density_field_handle<TSpectral>(m);
    huira::bind_phase_function_handle<TSpectral>(m);
    huira::bind_medium_handle<TSpectral>(m);

    // --- Asset handles ---
    huira::bind_primitive_handle<TSpectral>(m);
    huira::bind_camera_model_handle<TSpectral>(m);
    huira::bind_light_handle<TSpectral>(m);
    huira::bind_unresolved_object_handle<TSpectral>(m);
    huira::bind_model_handle<TSpectral>(m);
}

} // namespace huira::python
