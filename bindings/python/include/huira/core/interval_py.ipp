#pragma once

#include "huira/core/interval.hpp"
#include "pybind11/operators.h"
#include "pybind11/pybind11.h"

namespace py = pybind11;

namespace huira {

inline void bind_interval(py::module_& m)
{
    py::class_<Interval>(m, "Interval")
        // Constructor
        .def(py::init<Time, Time>(),
             py::arg("t0"),
             py::arg("t1"),
             "Create an interval from two time stamps")

        // Factory methods (no public constructor — use named factories)
        .def_static("from_centered",
                    &Interval::from_centered,
                    py::arg("center"),
                    py::arg("duration"),
                    "Create an exposure centered on a time with a given total duration")

        .def_static("from_start",
                    &Interval::from_start,
                    py::arg("start"),
                    py::arg("duration"),
                    "Create an exposure from a start time and integration duration")

        .def_static("from_bounds",
                    &Interval::from_bounds,
                    py::arg("start"),
                    py::arg("end"),
                    "Create an exposure from explicit start and end times")

        // Properties
        .def_readonly("start", &Interval::start, "Start time of the exposure interval")
        .def_readonly("end", &Interval::end, "End time of the exposure interval")

        // Computed accessors
        .def("center", &Interval::center, "Midpoint of the exposure interval")
        .def("duration", &Interval::duration, "Total duration of the exposure")

        // Repr
        .def("__repr__", [](const Interval& ei) {
            return "<Interval " + ei.start.to_iso_8601() + " to " + ei.end.to_iso_8601() + " (" +
                   std::to_string(ei.duration().value()) + "s)>";
        });
}

} // namespace huira
