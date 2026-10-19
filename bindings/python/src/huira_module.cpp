#include "bindings.hpp"
#include "pybind11/pybind11.h"
#include "spectral_submodules.hpp"

namespace py = pybind11;

// The bindings are split over several translation units (see bindings.hpp). The core ones come
// first: the spectral submodules use their types, in signatures and default arguments.
PYBIND11_MODULE(_huira, m)
{
    m.doc() = "Python bindings for the Huira C++ library";
    m.attr("__version__") = HUIRA_VERSION;

    huira::python::bind_core(m);

    // Bind RGB spectral specializations:
    auto rgb = m.def_submodule("rgb", "RGB (3-bin) spectral specialization");
    huira::python::bind_rgb(rgb);

    // Bind 8-bin Visible spectral specialization:
    auto visible8 = m.def_submodule("visible8", "Visible (8-bin) spectral specialization");
    huira::python::bind_visible8(visible8);

    auto swir8 = m.def_submodule("swir8", "Short-wave infrared (8-bin) spectral specialization");
    huira::python::bind_swir8(swir8);
}
