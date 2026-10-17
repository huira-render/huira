#pragma once

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
#include "huira/handles/handle_py.ipp"
#include "huira/handles/materials/bsdf_handle_py.ipp"
#include "huira/handles/materials/material_handle_py.ipp"
#include "huira/handles/materials/texture_handle_py.ipp"
#include "huira/handles/scene/frame_handle_py.ipp"
#include "huira/handles/scene/instance_handle_py.ipp"
#include "huira/handles/scene/node_handle_py.ipp"
#include "huira/handles/scene/root_frame_handle_py.ipp"
#include "huira/handles/volumes/density_field_handle_py.ipp"
#include "huira/handles/volumes/medium_handle_py.ipp"
#include "huira/handles/volumes/phase_function_handle_py.ipp"
#include "huira/images/image_py.ipp"
#include "huira/render/frame_buffer_py.ipp"
#include "huira/render/interaction_py.ipp"
#include "huira/render/ray_py.ipp"
#include "huira/render/renderer_py.ipp"
#include "huira/scene/scene_py.ipp"
#include "huira/scene/scene_view_py.ipp"
#include "huira/units/units_py.ipp"
#include "pybind11/pybind11.h"
#include "pybind11/stl.h"

namespace huira::python {

/// The bindings of one spectral type, into its submodule. Each spectral type's source file
/// (rgb.cpp, visible8.cpp, swir8.cpp) instantiates it once, so that no file compiles more than
/// one.
template <IsSpectral TSpectral>
void bind_spectral(pybind11::module_& m)
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

    // --- Scene node handles ---
    huira::bind_instance_handle<TSpectral>(m);
    huira::bind_frame_handle<TSpectral>(m);
    huira::bind_root_frame_handle<TSpectral>(m);

    // --- Material handles ---
    huira::bind_bsdf_handle<TSpectral>(m);
    huira::bind_material_handle<TSpectral>(m);

    // --- Volume handles ---
    huira::bind_density_field_handle<TSpectral>(m);
    huira::bind_phase_function_handle<TSpectral>(m);
    huira::bind_medium_handle<TSpectral>(m);

    // --- Images ---
    // Each spectral submodule has its own: huira.rgb.Image, huira.visible8.Image and so on.
    huira::bind_image<TSpectral>(m, "Image");
    huira::bind_image_bundle<TSpectral>(m, "ImageBundle");
    huira::bind_rgb_to_spectral<TSpectral>(m);

    // --- Spectral texture handle ---
    huira::bind_texture_handle<TSpectral>(m, "TextureHandle");

    // --- Rendering ---
    huira::bind_frame_buffer<TSpectral>(m);
    huira::bind_scene<TSpectral>(m);
    huira::bind_ray<TSpectral>(m);
    huira::bind_interaction<TSpectral>(m);
    huira::bind_scene_view<TSpectral>(m);
    huira::bind_renderer<TSpectral>(m);
}

} // namespace huira::python
