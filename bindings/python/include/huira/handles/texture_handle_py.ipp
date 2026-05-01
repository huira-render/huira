#pragma once

#include "huira/handles/texture_handle.hpp"
#include "pybind11/numpy.h"
#include "pybind11/pybind11.h"
#include "pybind11/stl.h"

namespace py = pybind11;

namespace huira {

template <IsImagePixel PixelT>
void bind_texture_handle(py::module_& m, const char* class_name)
{
    using HandleType = TextureHandle<PixelT>;

    py::class_<HandleType>(m, class_name)
        .def(
            "image",
            [](HandleType& self) -> py::object {
                Image<PixelT>* img = self.image();
                if (!img) {
                    return py::none(); // This should never happen?
                }
                return py::cast(img, py::return_value_policy::reference_internal, py::cast(self));
            },
            "Get the texture's image (returns None if no image is set)")

        .def_property_readonly("resolution",
                               [](const HandleType& self) {
                                   auto r = self.resolution();
                                   return py::make_tuple(r.width, r.height);
                               })

        .def("valid", &HandleType::valid)
        .def("__bool__", &HandleType::valid)
        .def("__repr__", [](const HandleType&) { return "<TextureHandle>"; });
}

inline void bind_common_textures(py::module_& m)
{
    bind_texture_handle<float>(m, "FloatTextureHandle");
    bind_texture_handle<Vec3<float>>(m, "NormalTextureHandle");
}
} // namespace huira
