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
#include "huira/handles/materials/texture_handle_py.ipp"
#include "huira/images/image_py.ipp"
#include "huira/render/frame_buffer_py.ipp"

namespace huira::python {

/// The third part of a spectral submodule (see bindings.hpp): its images, the
/// texture handle and the frame buffer.
template <IsSpectral TSpectral>
void bind_spectral_images(pybind11::module_& m)
{
    // --- Images ---
    // Each spectral submodule has its own: huira.rgb.Image, huira.visible8.Image and so on.
    huira::bind_image<TSpectral>(m, "Image");
    huira::bind_image_bundle<TSpectral>(m, "ImageBundle");
    huira::bind_rgb_to_spectral<TSpectral>(m);

    // --- Spectral texture handle ---
    huira::bind_texture_handle<TSpectral>(m, "TextureHandle");

    // --- Rendering ---
    huira::bind_frame_buffer<TSpectral>(m);
}

} // namespace huira::python
