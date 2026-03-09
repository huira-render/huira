#pragma once

#include "pybind11/pybind11.h"

#include "huira/render/renderer.hpp"

namespace py = pybind11;

namespace huira {

    template <IsSpectral TSpectral>
    void bind_ray(py::module_& m) {
        using RayT = Ray<TSpectral>;

        py::class_<RayT>(m, "Ray",
            "Ray with origin, direction, and precomputed reciprocal direction")
            .def(py::init<>())
            .def(py::init<const Vec3<float>&, const Vec3<float>&>(),
                py::arg("origin"), py::arg("direction"))

            .def_property_readonly("origin",    &RayT::origin)
            .def_property_readonly("direction", &RayT::direction)
            .def_property_readonly("reciprocal_direction", &RayT::reciprocal_direction)

            .def("at", &RayT::at, py::arg("t"),
                "Evaluate ray at parameter t: origin + t * direction")

            .def("__repr__", [](const RayT& r) {
                std::ostringstream ss;
                ss << "Ray(origin=" << vec_to_string<3, float>(r.origin())
                   << ", direction=" << vec_to_string<3, float>(r.direction()) << ")";
                return ss.str();
            })
            ;
    }

    inline void bind_hit_record(py::module_& m) {
        m.attr("INVALID_GEOMETRY_ID") = RTC_INVALID_GEOMETRY_ID;
        
        py::class_<HitRecord>(m, "HitRecord",
            "Result of a ray–scene intersection query")
            .def(py::init<>())

            .def_readwrite("t",       &HitRecord::t,       "Ray parameter at hit")
            .def_readwrite("u",       &HitRecord::u,       "Barycentric u")
            .def_readwrite("v",       &HitRecord::v,       "Barycentric v")
            .def_readwrite("inst_id", &HitRecord::inst_id, "Instance ID in TLAS")
            .def_readwrite("geom_id", &HitRecord::geom_id, "Geometry ID in BLAS")
            .def_readwrite("prim_id", &HitRecord::prim_id, "Triangle index")
            .def_readwrite("Ng",      &HitRecord::Ng,      "Geometric face normal (unnormalized)")

            .def("hit", &HitRecord::hit,
                "Returns true if the record represents a valid intersection")

            .def("__repr__", [](const HitRecord& h) {
                std::ostringstream ss;
                if (!h.hit()) {
                    ss << "HitRecord(miss)";
                } else {
                    ss << "HitRecord(t=" << h.t
                       << ", inst_id=" << h.inst_id
                       << ", geom_id=" << h.geom_id
                       << ", prim_id=" << h.prim_id << ")";
                }
                return ss.str();
            })
            ;
    }
}