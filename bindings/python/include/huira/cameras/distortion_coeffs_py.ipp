#pragma once

#include "pybind11/pybind11.h"

#include "huira/cameras/distortion/brown_distortion.hpp"
#include "huira/cameras/distortion/opencv_distortion.hpp"
#include "huira/cameras/distortion/owen_distortion.hpp"

namespace py = pybind11;

namespace huira {

    inline void bind_distortion_coefficients(py::module_& m) {
        // Base class for distortion coefficients
        py::class_<DistortionCoefficients>(m, "DistortionCoefficients")
            .def(py::init<>());

        // Brown distortion coefficients
        py::class_<BrownCoefficients, DistortionCoefficients>(m, "BrownCoefficients")
            .def(py::init<>())
            .def(py::init<double, double, double, double, double>(),
                py::arg("k1"), py::arg("k2"), py::arg("k3"),
                py::arg("p1"), py::arg("p2"),
                "Construct Brown distortion coefficients with radial (k1, k2, k3) and tangential (p1, p2) parameters")
            .def_readwrite("k1", &BrownCoefficients::k1, "First radial distortion coefficient")
            .def_readwrite("k2", &BrownCoefficients::k2, "Second radial distortion coefficient")
            .def_readwrite("k3", &BrownCoefficients::k3, "Third radial distortion coefficient")
            .def_readwrite("p1", &BrownCoefficients::p1, "First tangential distortion coefficient")
            .def_readwrite("p2", &BrownCoefficients::p2, "Second tangential distortion coefficient")
            .def("__repr__", [](const BrownCoefficients& c) {
            return "BrownCoefficients(k1=" + std::to_string(c.k1) +
                ", k2=" + std::to_string(c.k2) +
                ", k3=" + std::to_string(c.k3) +
                ", p1=" + std::to_string(c.p1) +
                ", p2=" + std::to_string(c.p2) + ")";
                });

        // OpenCV distortion coefficients
        py::class_<OpenCVCoefficients, DistortionCoefficients>(m, "OpenCVCoefficients")
            .def(py::init<>())
            .def(py::init<double, double, double, double, double, double,
                double, double, double, double, double, double>(),
                py::arg("k1"), py::arg("k2"), py::arg("k3"),
                py::arg("k4"), py::arg("k5"), py::arg("k6"),
                py::arg("p1"), py::arg("p2"),
                py::arg("s1"), py::arg("s2"), py::arg("s3"), py::arg("s4"),
                "Construct OpenCV distortion coefficients with radial (k1-k6), tangential (p1, p2), and thin prism (s1-s4) parameters")
            .def_readwrite("k1", &OpenCVCoefficients::k1, "First radial distortion coefficient")
            .def_readwrite("k2", &OpenCVCoefficients::k2, "Second radial distortion coefficient")
            .def_readwrite("k3", &OpenCVCoefficients::k3, "Third radial distortion coefficient")
            .def_readwrite("k4", &OpenCVCoefficients::k4, "Fourth radial distortion coefficient")
            .def_readwrite("k5", &OpenCVCoefficients::k5, "Fifth radial distortion coefficient")
            .def_readwrite("k6", &OpenCVCoefficients::k6, "Sixth radial distortion coefficient")
            .def_readwrite("p1", &OpenCVCoefficients::p1, "First tangential distortion coefficient")
            .def_readwrite("p2", &OpenCVCoefficients::p2, "Second tangential distortion coefficient")
            .def_readwrite("s1", &OpenCVCoefficients::s1, "First thin prism distortion coefficient")
            .def_readwrite("s2", &OpenCVCoefficients::s2, "Second thin prism distortion coefficient")
            .def_readwrite("s3", &OpenCVCoefficients::s3, "Third thin prism distortion coefficient")
            .def_readwrite("s4", &OpenCVCoefficients::s4, "Fourth thin prism distortion coefficient")
            .def("__repr__", [](const OpenCVCoefficients& c) {
            return "OpenCVCoefficients(k1=" + std::to_string(c.k1) +
                ", k2=" + std::to_string(c.k2) +
                ", k3=" + std::to_string(c.k3) +
                ", k4=" + std::to_string(c.k4) +
                ", k5=" + std::to_string(c.k5) +
                ", k6=" + std::to_string(c.k6) +
                ", p1=" + std::to_string(c.p1) +
                ", p2=" + std::to_string(c.p2) +
                ", s1=" + std::to_string(c.s1) +
                ", s2=" + std::to_string(c.s2) +
                ", s3=" + std::to_string(c.s3) +
                ", s4=" + std::to_string(c.s4) + ")";
                });

        // Owen distortion coefficients
        py::class_<OwenCoefficients, DistortionCoefficients>(m, "OwenCoefficients")
            .def(py::init<>())
            .def(py::init<double, double, double, double, double, double>(),
                py::arg("e1"), py::arg("e2"), py::arg("e3"),
                py::arg("e4"), py::arg("e5"), py::arg("e6"),
                "Construct Owen distortion coefficients with coordinate-aligned and rotated terms")
            .def_readwrite("e1", &OwenCoefficients::e1, "First Owen distortion coefficient")
            .def_readwrite("e2", &OwenCoefficients::e2, "Second Owen distortion coefficient")
            .def_readwrite("e3", &OwenCoefficients::e3, "Third Owen distortion coefficient")
            .def_readwrite("e4", &OwenCoefficients::e4, "Fourth Owen distortion coefficient")
            .def_readwrite("e5", &OwenCoefficients::e5, "Fifth Owen distortion coefficient")
            .def_readwrite("e6", &OwenCoefficients::e6, "Sixth Owen distortion coefficient")
            .def("__repr__", [](const OwenCoefficients& c) {
            return "OwenCoefficients(e1=" + std::to_string(c.e1) +
                ", e2=" + std::to_string(c.e2) +
                ", e3=" + std::to_string(c.e3) +
                ", e4=" + std::to_string(c.e4) +
                ", e5=" + std::to_string(c.e5) +
                ", e6=" + std::to_string(c.e6) + ")";
                });
    }

}
