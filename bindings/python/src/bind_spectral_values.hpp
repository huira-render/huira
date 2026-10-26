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
#include "huira/handles/materials/texture_handle_py.ipp"
#include "huira/images/image_py.ipp"
#include "huira/render/frame_buffer_py.ipp"
#include "huira/render/ray_py.ipp"
#include "huira/units/units_py.ipp"

namespace huira::python {

/// The first part of a spectral submodule (see bindings.hpp): the values the later parts take and
/// return, so that their signatures name them: the spectral bins and spectral units, images, the
/// texture handle, the frame buffer and rays.
template <IsSpectral TSpectral>
void bind_spectral_values(pybind11::module_& m)
{
    // The spectral bins first: the spectral units, and everything after, take them.
    huira::bind_spectral_bins<TSpectral>(m);
    huira::bind_spectral_units_for_type<TSpectral>(m);

    // --- Images ---
    // Each spectral submodule has its own: huira.rgb.Image, huira.visible8.Image and so on.
    huira::bind_image<TSpectral>(m, "Image");
    huira::bind_image_bundle<TSpectral>(m, "ImageBundle");
    huira::bind_rgb_to_spectral<TSpectral>(m);

    // --- Spectral texture handle ---
    huira::bind_texture_handle<TSpectral>(m, "TextureHandle");

    // --- Rendering ---
    huira::bind_frame_buffer<TSpectral>(m);
    huira::bind_ray<TSpectral>(m);
}

} // namespace huira::python
