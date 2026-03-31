#pragma once

#include "pybind11/pybind11.h"
#include "pybind11/stl.h"

#include "huira/handles/model_handle.hpp"

namespace py = pybind11;

namespace huira {
    template <IsSpectral TSpectral>
    inline void bind_model_handle(py::module_& m) {
        using HandleType = ModelHandle<TSpectral>;

        py::class_<HandleType>(m, "ModelHandle")

            .def("print_graph", &HandleType::print_graph,
                "Print the model's node graph")

            .def("get_material_by_id", &HandleType::get_material_by_id,
                py::arg("material_id"),
                "Get a specific material handle by its unique ID (as shown in print_graph)")

            .def("set_all_bsdfs", &HandleType::set_all_bsdfs, py::arg("bsdf"),
                "Assign a new BSDF to all materials")

            // --- Handle basics ---
            .def("valid", &HandleType::valid)
            .def("__bool__", &HandleType::valid)
            .def("__repr__", [](const HandleType&) {
            return "<ModelHandle>";
                });
    }
}
