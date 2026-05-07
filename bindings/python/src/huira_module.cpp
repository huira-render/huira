#include <filesystem>
#include <string>

#include "huira/concepts/spectral_concepts.hpp"

#include "huira/cameras/distortion_coeffs_py.ipp"
#include "huira/core/interval_py.ipp"
#include "huira/core/rotation_py.ipp"
#include "huira/core/spectral_bin_py.ipp"
#include "huira/core/spectral_bins_py.ipp"
#include "huira/core/spice_py.ipp"
#include "huira/core/time_py.ipp"
#include "huira/core/types_py.ipp"
#include "huira/handles/assets/light_handle_py.ipp"
#include "huira/handles/assets/model_handle_py.ipp"
#include "huira/handles/assets/primitive_handle_py.ipp"
#include "huira/handles/assets/unresolved_handle_py.ipp"
#include "huira/handles/geometry/ellipsoid_handle_py.ipp"
#include "huira/handles/geometry/geometry_handle_py.ipp"
#include "huira/handles/geometry/mesh_handle_py.ipp"
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
#include "huira/handles/camera_handle_py.ipp"
#include "huira/handles/handle_py.ipp"
#include "huira/images/fits_metadata_py.ipp"
#include "huira/images/image_py.ipp"
#include "huira/render/frame_buffer_py.ipp"
#include "huira/render/interaction_py.ipp"
#include "huira/render/ray_py.ipp"
#include "huira/render/renderer_py.ipp"
#include "huira/scene/scene_py.ipp"
#include "huira/scene/scene_view_py.ipp"
#include "huira/units/units_py.ipp"
#include "huira/util/logger_py.ipp"
#include "huira/util/paths_py.ipp"
#include "pybind11/pybind11.h"
#include "pybind11/stl.h"

namespace py = pybind11;

template <huira::IsSpectral TSpectral>
inline void bind_spectral(py::module_& m)
{
    huira::bind_spectral_units_for_type<TSpectral>(m);
    huira::bind_spectral_bins<TSpectral>(m);

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
    std::string img_name = "Image_" + m.attr("__name__").cast<std::string>();
    huira::bind_image<TSpectral>(m, img_name.c_str());
    huira::bind_image_bundle<TSpectral>(m, "ImageBundle_" + m.attr("__name__").cast<std::string>());

    // --- Spectral texture handle ---
    std::string tex_name = "TextureHandle_" + m.attr("__name__").cast<std::string>();
    huira::bind_texture_handle<TSpectral>(m, tex_name.c_str());

    // --- Rendering ---
    huira::bind_frame_buffer<TSpectral>(m);
    huira::bind_scene<TSpectral>(m);
    huira::bind_ray<TSpectral>(m);
    huira::bind_interaction<TSpectral>(m);
    huira::bind_scene_view<TSpectral>(m);
    huira::bind_renderer<TSpectral>(m);
}

PYBIND11_MODULE(_huira, m)
{
    m.doc() = "Python bindings for the Huira C++ library";
    m.attr("__version__") = HUIRA_VERSION;

    // Bind core types and utilities:
    huira::bind_units(m);
    huira::bind_time(m);
    huira::bind_interval(m);
    huira::bind_bin(m);
    huira::bind_hit_record(m);

    huira::bind_types(m);
    huira::bind_rotation(m);

    huira::bind_paths(m);
    huira::bind_logger(m);

    huira::spice::bind_spice(m);

    huira::bind_distortion_coefficients(m);

    huira::bind_fits_metadata(m);
    huira::bind_common_images(m);

    huira::bind_texture_handle<float>(m, "TextureHandle_float");
    huira::bind_texture_handle<huira::Vec3<float>>(m, "TextureHandle_vec3");

    // Observation Mode Enum:
    py::enum_<huira::ObservationMode>(m, "ObservationMode", py::arithmetic())
        .value("TRUE_STATE", huira::ObservationMode::TRUE_STATE)
        .value("GEOMETRIC_STATE", huira::ObservationMode::GEOMETRIC_STATE)
        .value("ABERRATED_STATE", huira::ObservationMode::ABERRATED_STATE);

    // Bind RGB spectral specializations:
    auto rgb = m.def_submodule("rgb", "RGB (3-bin) spectral specialization");
    bind_spectral<huira::RGB>(rgb);

    // Bind 8-bin Visible spectral specialization:
    auto visible8 = m.def_submodule("visible8", "Visible (8-bin) spectral specialization");
    bind_spectral<huira::Visible8>(visible8);

    auto swir8 = m.def_submodule("swir8", "Short-wave infrared (8-bin) spectral specialization");
    bind_spectral<huira::SWIR8>(swir8);
}
