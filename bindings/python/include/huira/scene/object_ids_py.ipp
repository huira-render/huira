#pragma once

#include <cstdint>
#include <sstream>
#include <string>

#include "huira/scene/object_ids.hpp"
#include "pybind11/operators.h"
#include "pybind11/pybind11.h"

namespace py = pybind11;

namespace huira {

inline void bind_object_ids(py::module_& m)
{
    m.attr("NO_OBJECT_ID") = NO_OBJECT_ID;

    py::class_<ObjectIds>(m,
                          "ObjectIds",
                          "The IDs of the scene objects a ray hit, as SceneView.object_ids() "
                          "gives them and a FrameBuffer's geometry_ids, model_ids, instance_ids "
                          "and part_ids record them. Each is an object's id(), or "
                          "huira.NO_OBJECT_ID where there is none.")
        .def(py::init<>())
        .def_readonly("geometry",
                      &ObjectIds::geometry,
                      "The geometry hit: the mesh or ellipsoid, shared by every primitive made "
                      "from it")
        .def_readonly("model",
                      &ObjectIds::model,
                      "The model placed by the instance, or NO_OBJECT_ID for a primitive placed "
                      "directly")
        .def_readonly("instance",
                      &ObjectIds::instance,
                      "The instance in the scene graph that placed the primitive or its model")
        .def_readonly("part",
                      &ObjectIds::part,
                      "The innermost instance that placed the primitive: the model's own node "
                      "for a primitive in a model, or the instance itself for a primitive "
                      "placed directly")
        .def(py::self == py::self)
        .def(py::self != py::self)
        .def("__repr__", [](const ObjectIds& ids) {
            const auto id = [](std::uint64_t value) {
                return value == NO_OBJECT_ID ? std::string{"None"} : std::to_string(value);
            };
            std::ostringstream os;
            os << "ObjectIds(geometry=" << id(ids.geometry) << ", model=" << id(ids.model)
               << ", instance=" << id(ids.instance) << ", part=" << id(ids.part) << ")";
            return os.str();
        });
}

} // namespace huira
