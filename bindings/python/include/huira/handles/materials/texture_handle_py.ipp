#pragma once

#include "huira/handles/materials/texture_handle.hpp"
#include "pybind11/pybind11.h"
#include "pybind11/stl.h"

namespace py = pybind11;

namespace huira {
/**
 * @brief Registers TextureHandle<TPixel> as a Python class.
 */
template <typename TPixel>
inline void bind_texture_handle(py::module_& m, const char* name = "TextureHandle")
{
    using HandleType = TextureHandle<TPixel>;

    auto cls = py::class_<HandleType>(m, name)
                   .def("resolution",
                        &HandleType::resolution,
                        "Return the resolution of the underlying texture")
                   .def("__bool__", &HandleType::valid)
                   .def("__repr__", [](const HandleType&) { return "<TextureHandle>"; });

    bind_handle_methods<Texture<TPixel>>(cls);
}

} // namespace huira
