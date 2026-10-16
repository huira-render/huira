#pragma once

#include <sstream>
#include <string>

#include "huira/geometry/vertex.hpp"
#include "huira/handles/camera_handle_py.ipp"
#include "pybind11/numpy.h"
#include "pybind11/pybind11.h"

namespace py = pybind11;

namespace huira {
namespace detail {
/// Texture coordinates from Python: any sequence or array of two numbers.
inline Vec2<float> vec2f_from_py(const py::object& obj)
{
    const auto array = py::array_t<double, py::array::c_style | py::array::forcecast>::ensure(obj);
    if (!array || array.size() != 2) {
        throw py::value_error("Expected a sequence of two numbers");
    }
    const double* v = array.data();
    return {static_cast<float>(v[0]), static_cast<float>(v[1])};
}
} // namespace detail

/**
 * @brief Bind Vertex<TSpectral>, the vertices Scene.add_mesh() builds a mesh from.
 *
 * Its position and normal are single precision, which Python has no vector type for: they are
 * given as a huira.Vec3 or any sequence of three numbers, and read back as a huira.Vec3. Its
 * texture coordinates are given as any sequence of two numbers, and read back as a tuple.
 */
template <IsSpectral TSpectral>
void bind_vertex(py::module_& m)
{
    using VertexT = Vertex<TSpectral>;

    py::class_<VertexT>(m,
                        "Vertex",
                        "A vertex of a mesh: its position and normal, in the mesh's frame, its "
                        "texture coordinates, and its albedo, which scales the material's.")
        .def(py::init([](const py::object& position,
                         const py::object& normal,
                         const py::object& uv,
                         const TSpectral& albedo) {
                 VertexT vertex;
                 vertex.position = detail::vec3f_from_py(position);
                 vertex.normal = detail::vec3f_from_py(normal);
                 vertex.uv = detail::vec2f_from_py(uv);
                 vertex.albedo = albedo;
                 return vertex;
             }),
             py::arg("position"),
             py::arg("normal"),
             py::arg("uv") = py::make_tuple(0.0, 0.0),
             py::arg("albedo") = TSpectral{1.f},
             "Create a vertex. position and normal are a huira.Vec3 or three numbers (in meters, "
             "and a unit vector), uv two numbers, and albedo the vertex's color, 1 by default.")

        .def_property(
            "position",
            [](const VertexT& v) { return Vec3<double>(v.position); },
            [](VertexT& v, const py::object& position) {
                v.position = detail::vec3f_from_py(position);
            },
            "Position, in the mesh's frame (m)")
        .def_property(
            "normal",
            [](const VertexT& v) { return Vec3<double>(v.normal); },
            [](VertexT& v, const py::object& normal) { v.normal = detail::vec3f_from_py(normal); },
            "Normal, a unit vector in the mesh's frame")
        .def_property(
            "uv",
            [](const VertexT& v) { return py::make_tuple(v.uv.x, v.uv.y); },
            [](VertexT& v, const py::object& uv) { v.uv = detail::vec2f_from_py(uv); },
            "Texture coordinates (u, v)")
        .def_readwrite("albedo", &VertexT::albedo, "Albedo, which scales the material's")

        .def("__repr__", [](const VertexT& v) {
            std::ostringstream out;
            out << "<Vertex position=(" << v.position.x << ", " << v.position.y << ", "
                << v.position.z << ") normal=(" << v.normal.x << ", " << v.normal.y << ", "
                << v.normal.z << ") uv=(" << v.uv.x << ", " << v.uv.y << ")>";
            return out.str();
        });
}

} // namespace huira
