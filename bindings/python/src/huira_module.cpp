#include <filesystem>
#include <string>

#include "pybind11/pybind11.h"
#include "pybind11/stl.h"

#include "huira/core/concepts/spectral_concepts.hpp"

#include "huira/cameras/distortion_coeffs_py.ipp"

#include "huira/core/units/units_py.ipp"
#include "huira/core/spectral_bin_py.ipp"
#include "huira/core/spectral_bins_py.ipp"
#include "huira/core/time_py.ipp"

#include "huira/ephemeris/spice_py.ipp"

#include "huira/handles/camera_handle_py.ipp"
#include "huira/handles/frame_handle_py.ipp"
#include "huira/handles/instance_handle_py.ipp"
#include "huira/handles/light_handle_py.ipp"
#include "huira/handles/node_handle_py.ipp"
#include "huira/handles/root_frame_handle_py.ipp"
#include "huira/handles/unresolved_handle_py.ipp"

#include "huira/images/fits_metadata_py.ipp"
#include "huira/images/image_py.ipp"

#include "huira/render/frame_buffer_py.ipp"
#include "huira/render/raster_renderer_py.ipp"

#include "huira/scene/scene_py.ipp"
#include "huira/scene/scene_view_py.ipp"

#include "huira/util/paths_py.ipp"

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

    huira::bind_frame_buffer<TSpectral>(m);

    huira::bind_scene<TSpectral>(m);

    huira::bind_scene_view<TSpectral>(m);
    huira::bind_raster_renderer<TSpectral>(m);
}

PYBIND11_MODULE(_huira, m) {
    m.doc() = "Python bindings for the Huira C++ library";
    m.attr("__version__") = HUIRA_VERSION;

    // Bind core types and utilities:
    huira::bind_units(m);
    huira::bind_time(m);
    huira::bind_bin(m);

    huira::bind_paths(m);

    huira::spice::bind_spice(m);

    huira::bind_distortion_coefficients(m);

    huira::bind_fits_metadata(m);
    huira::bind_all_images(m);

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
}
