#include "bind_spectral.hpp"
#include "bindings.hpp"

namespace huira::python {

void bind_rgb(pybind11::module_& m)
{
    bind_spectral<huira::RGB>(m);
}

} // namespace huira::python
