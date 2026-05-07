#pragma once

#include "huira/handles/geometry/mesh_handle.hpp"
#include "huira/handles/handle_py.ipp"
#include "pybind11/pybind11.h"
#include "pybind11/stl.h"

namespace py = pybind11;

namespace huira {
/**
 * @brief Registers MeshHandle<TSpectral> as a Python class.
 */
template <typename TSpectral>
inline void bind_mesh_handle(py::module_& m)
{
    using HandleType = MeshHandle<TSpectral>;

    auto cls = py::class_<HandleType, GeometryHandle<TSpectral>>(m, "MeshHandle")
                   .def("get_vertex_count",
                        &HandleType::get_vertex_count,
                        "Return the number of vertices in the mesh")

                   // --- Handle basics ---
                   .def("__bool__", &HandleType::valid)
                   .def("__repr__", [](const HandleType&) { return "<MeshHandle>"; });

    bind_handle_methods<Mesh<TSpectral>>(cls);
}
} // namespace huira
