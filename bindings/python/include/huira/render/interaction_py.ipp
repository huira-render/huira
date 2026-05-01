#pragma once

#include "huira/render/renderer.hpp"
#include "pybind11/pybind11.h"

namespace py = pybind11;

namespace huira {

template <IsSpectral TSpectral>
void bind_interaction(py::module_& m)
{
    using InteractionT = Interaction<TSpectral>;

    py::class_<InteractionT>(
        m, "Interaction", "Surface interaction record at a ray–geometry intersection")
        .def(py::init<>())

        .def_readwrite("position", &InteractionT::position, "Intersection point in world space")
        .def_readwrite("normal_g", &InteractionT::normal_g, "Geometric normal")
        .def_readwrite("normal_s", &InteractionT::normal_s, "Shading normal")
        .def_readwrite("tangent", &InteractionT::tangent, "Tangent vector")
        .def_readwrite("bitangent", &InteractionT::bitangent, "Bitangent vector")
        .def_readwrite("uv", &InteractionT::uv, "Texture coordinates (u, v)")
        .def_readwrite("wo", &InteractionT::wo, "Outgoing direction (towards camera)")
        .def_readwrite("vertex_albedo", &InteractionT::vertex_albedo, "Interpolated vertex color")

        .def("__repr__", [](const InteractionT& it) {
            std::ostringstream ss;
            ss << "Interaction(position=" << vec_to_string<3, float>(it.position)
               << ", normal_g=" << vec_to_string<3, float>(it.normal_g)
               << ", normal_s=" << vec_to_string<3, float>(it.normal_s)
               << ", uv=" << vec_to_string<2, float>(it.uv) << ")";
            return ss.str();
        });
}
} // namespace huira
