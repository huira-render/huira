#include <filesystem>
#include <string>

#include "pybind11/pybind11.h"
#include "pybind11/stl.h"

#include "huira/core/concepts/spectral_concepts.hpp"

#include "huira/cameras/distortion_coeffs_py.ipp"

#include "huira/core/units/units_py.ipp"
#include "huira/core/interval_py.ipp"
#include "huira/core/rotation_py.ipp"
#include "huira/core/spectral_bin_py.ipp"
#include "huira/core/spectral_bins_py.ipp"
#include "huira/core/time_py.ipp"
#include "huira/core/types_py.ipp"

#include "huira/ephemeris/spice_py.ipp"

#include "huira/handles/camera_handle_py.ipp"
#include "huira/handles/frame_handle_py.ipp"
#include "huira/handles/instance_handle_py.ipp"
#include "huira/handles/light_handle_py.ipp"
#include "huira/handles/material_handle_py.ipp"
#include "huira/handles/model_handle_py.ipp"
#include "huira/handles/node_handle_py.ipp"
#include "huira/handles/root_frame_handle_py.ipp"
#include "huira/handles/texture_handle_py.ipp"
#include "huira/handles/unresolved_handle_py.ipp"

#include "huira/images/fits_metadata_py.ipp"
#include "huira/images/image_py.ipp"

#include "huira/materials/bsdfs_py.ipp"

#include "huira/render/frame_buffer_py.ipp"
#include "huira/render/interaction_py.ipp"
#include "huira/render/ray_py.ipp"
#include "huira/render/renderer_py.ipp"

#include "huira/scene/scene_py.ipp"
#include "huira/scene/scene_view_py.ipp"

#include "huira/util/paths_py.ipp"
#include "huira/util/logger_py.ipp"

namespace py = pybind11;

template <huira::IsSpectral TSpectral>
inline void bind_spectral(py::module_& m) {
    huira::bind_spectral_units_for_type<TSpectral>(m);
    huira::bind_spectral_bins<TSpectral>(m);
    
    huira::bind_camera_model_handle<TSpectral>(m);
    huira::bind_light_handle<TSpectral>(m);
    huira::bind_unresolved_object_handle<TSpectral>(m);

    huira::bind_instance_handle<TSpectral>(m);
    huira::bind_frame_handle<TSpectral>(m);
    huira::bind_root_frame_handle<TSpectral>(m);

    huira::bind_bsdfs<TSpectral>(m);

    std::string img_name = "Image_" + m.attr("__name__").cast<std::string>();
    huira::bind_image<TSpectral>(m, img_name.c_str());
    huira::bind_image_bundle<TSpectral>(m, "ImageBundle_" + m.attr("__name__").cast<std::string>());

    huira::bind_material_handle<TSpectral>(m);

    huira::bind_model_handle<TSpectral>(m);

    huira::bind_frame_buffer<TSpectral>(m);

    huira::bind_scene<TSpectral>(m);

    huira::bind_ray<TSpectral>(m);
    huira::bind_interaction<TSpectral>(m);

    huira::bind_scene_view<TSpectral>(m);
    huira::bind_renderer<TSpectral>(m);
}

PYBIND11_MODULE(_huira, m) {
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

    huira::bind_common_textures(m);

    // Observation Mode Enum:
    py::enum_<huira::ObservationMode>(m, "ObservationMode", py::arithmetic())
        .value("TRUE_STATE", huira::ObservationMode::TRUE_STATE)
        .value("GEOMETRIC_STATE", huira::ObservationMode::GEOMETRIC_STATE)
        .value("ABERRATED_STATE", huira::ObservationMode::ABERRATED_STATE)
        ;

    // Bind RGB spectral specializations:
    auto rgb = m.def_submodule("rgb", "RGB (3-bin) spectral specialization");
    bind_spectral<huira::RGB>(rgb);

    // Bind 8-bin Visible spectral specialization:
    auto visible8 = m.def_submodule("visible8", "Visible (8-bin) spectral specialization");
    bind_spectral<huira::Visible8>(visible8);

    auto swir8 = m.def_submodule("swir8", "Short-wave infrared (8-bin) spectral specialization");
    bind_spectral<huira::SWIR8>(swir8);
}
