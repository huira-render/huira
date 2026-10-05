#pragma once

#include "huira/cameras/pixel_convention.hpp"
#include "pybind11/operators.h"
#include "pybind11/pybind11.h"

namespace py = pybind11;

namespace huira {

inline void bind_pixel_convention(py::module_& m)
{
    py::enum_<PixelCenter>(m, "PixelCenter", "Which coordinate the center of the first pixel has.")
        .value("Integer",
               PixelCenter::Integer,
               "Pixel centers at integers, the first at 0: pixel i covers [i - 0.5, i + 0.5).")
        .value("HalfInteger",
               PixelCenter::HalfInteger,
               "Pixel edges at integers, so the first pixel's center is at 0.5: pixel i covers "
               "[i, i + 1).")
        .value("OneBased", PixelCenter::OneBased, "Pixel centers at integers, the first at 1.");

    py::enum_<PixelOrigin>(m,
                           "PixelOrigin",
                           "Which corner of the image pixel coordinates count from. x always runs "
                           "left to right; images are always stored top row first.")
        .value("TopLeft", PixelOrigin::TopLeft, "y runs down from the top row.")
        .value("BottomLeft", PixelOrigin::BottomLeft, "y runs up from the bottom row.");

    py::class_<PixelConvention>(
        m,
        "PixelConvention",
        "The pixel coordinate convention a camera's users work in: what the principal point "
        "(cx, cy) passed to a camera, and the positions it projects to, mean.\n\n"
        "Presets: opencv() (the default; first pixel centered at (0, 0), y down), colmap() "
        "(first pixel centered at (0.5, 0.5), y down; Huira before v0.9.10), matlab() (first "
        "pixel centered at (1, 1), y down) and fits() (first pixel centered at (1, 1), y up from "
        "the bottom row, as for FITS WCS keywords such as CRPIX).\n\n"
        "The convention never changes the rendered image when the principal point is left at "
        "its default, the center of the sensor.")
        .def(py::init<>(), "The default convention, PixelConvention.opencv().")
        .def(py::init([](PixelCenter center, PixelOrigin origin) {
                 return PixelConvention{center, origin};
             }),
             py::arg("center"),
             py::arg("origin") = PixelOrigin::TopLeft)
        .def_readwrite("center", &PixelConvention::center)
        .def_readwrite("origin", &PixelConvention::origin)
        .def_static("opencv",
                    &PixelConvention::opencv,
                    "First pixel centered at (0, 0), y down. The default.")
        .def_static("colmap",
                    &PixelConvention::colmap,
                    "First pixel centered at (0.5, 0.5), y down: pixel edges at integers.")
        .def_static("matlab", &PixelConvention::matlab, "First pixel centered at (1, 1), y down.")
        .def_static("fits",
                    &PixelConvention::fits,
                    "First pixel centered at (1, 1), y up from the bottom row.")
        .def(py::self == py::self)
        .def(py::self != py::self)
        .def("__repr__", &PixelConvention::to_string);
}

} // namespace huira
