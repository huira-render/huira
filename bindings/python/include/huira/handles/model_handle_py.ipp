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

            // --- Handle basics ---
            .def("valid", &HandleType::valid)
            .def("__bool__", &HandleType::valid)
            .def("__repr__", [](const HandleType&) {
            return "<ModelHandle>";
                });
    }
}
