#include "bind_spectral.hpp"
#include "bindings.hpp"

namespace huira::python {

void bind_visible8(pybind11::module_& m)
{
    bind_spectral<huira::Visible8>(m);
}

} // namespace huira::python
