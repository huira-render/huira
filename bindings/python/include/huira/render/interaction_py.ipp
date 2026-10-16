#pragma once

#include <sstream>

#include "huira/geometry/vertex_py.ipp"
#include "huira/render/renderer.hpp"
#include "pybind11/pybind11.h"

namespace py = pybind11;

namespace huira {

namespace detail {
/// A read-write property for one of an interaction's single-precision vectors, which Python has
/// no type for: read as a huira.Vec3, and set from a huira.Vec3 or any three numbers.
template <typename ClassT>
void def_vec3f_property(ClassT& cls,
                        const char* name,
                        Vec3<float> ClassT::type::* member,
                        const char* doc)
{
    using T = typename ClassT::type;
    cls.def_property(
        name,
        [member](const T& self) { return Vec3<double>(self.*member); },
        [member](T& self, const py::object& value) { self.*member = vec3f_from_py(value); },
        doc);
}
} // namespace detail

/**
 * @brief Bind Interaction<TSpectral>, the surface at a hit, as SceneView.resolve_hit() fills it.
 *
 * Its vectors are single precision, which Python has no type for: they are read as a huira.Vec3
 * (and its texture coordinates as a tuple), and set from a huira.Vec3 or any sequence of numbers.
 */
template <IsSpectral TSpectral>
void bind_interaction(py::module_& m)
{
    using InteractionT = Interaction<TSpectral>;

    auto cls = py::class_<InteractionT>(
        m, "Interaction", "Surface interaction record at a ray–geometry intersection");
    cls.def(py::init<>());

    detail::def_vec3f_property(
        cls, "position", &InteractionT::position, "Intersection point in world space");
    detail::def_vec3f_property(cls, "normal_g", &InteractionT::normal_g, "Geometric normal");
    detail::def_vec3f_property(cls, "normal_s", &InteractionT::normal_s, "Shading normal");
    detail::def_vec3f_property(cls, "tangent", &InteractionT::tangent, "Tangent vector");
    detail::def_vec3f_property(cls, "bitangent", &InteractionT::bitangent, "Bitangent vector");
    detail::def_vec3f_property(cls, "wo", &InteractionT::wo, "Outgoing direction (towards camera)");

    cls.def_property(
           "uv",
           [](const InteractionT& self) { return py::make_tuple(self.uv.x, self.uv.y); },
           [](InteractionT& self, const py::object& uv) { self.uv = detail::vec2f_from_py(uv); },
           "Texture coordinates (u, v)")
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
