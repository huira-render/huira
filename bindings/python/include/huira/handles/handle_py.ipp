#pragma once

#include "huira/handles/handle.hpp"
#include "pybind11/pybind11.h"
#include "pybind11/stl.h"

namespace py = pybind11;

namespace huira {
/**
 * @brief Injects all Handle methods into an existing py::class_ binding.
 */
template <typename T, typename PyClass>
inline void bind_handle_methods(PyClass& cls)
{
    using HandleType = Handle<T>;
    using BoundType = typename PyClass::type;

    cls.def("valid", &BoundType::valid)
        .def("name", &BoundType::name)
        .def("id", &BoundType::id)
        .def("type", &BoundType::type);
}
} // namespace huira
