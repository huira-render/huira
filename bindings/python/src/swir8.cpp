#include "bind_spectral.hpp"
#include "bindings.hpp"

namespace huira::python {

void bind_swir8(pybind11::module_& m)
{
    bind_spectral<huira::SWIR8>(m);
}

} // namespace huira::python
