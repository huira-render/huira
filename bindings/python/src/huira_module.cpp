#include <filesystem>
#include <string>

#include "pybind11/pybind11.h"
#include "pybind11/stl.h"

#include "huira/core/concepts/spectral_concepts.hpp"

#include "huira/core/units/units_py.ipp"
#include "huira/core/spectral_bin_py.ipp"
#include "huira/core/spectral_bins_py.ipp"
#include "huira/core/time_py.ipp"

#include "huira/ephemeris/spice_py.ipp"
#include "huira/util/paths_py.ipp"

#include "huira/scene/scene_py.ipp"

namespace py = pybind11;

template <huira::IsSpectral TSpectral>
inline void bind_spectral(py::module_& m) {
    huira::bind_spectral_bins<TSpectral>(m);

    huira::bind_scene<TSpectral>(m);
}

PYBIND11_MODULE(_huira, m) {
    m.doc() = "Python bindings for the Huira C++ library";

    huira::bind_units(m);
    huira::bind_time(m);
    huira::bind_bin(m);

    huira::bind_paths(m);

    huira::spice::bind_spice(m);

    auto rgb = m.def_submodule("rgb", "RGB (3-bin) spectral specialization");
    bind_spectral<huira::RGB>(rgb);
}
