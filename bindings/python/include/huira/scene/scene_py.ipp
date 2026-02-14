#pragma once

#include "pybind11/pybind11.h"
#include <string>

#include "pybind11/stl.h"
#include "pybind11/stl/filesystem.h"

#include "huira/scene/scene.hpp"
#include "huira/handles/camera_handle.hpp"

namespace py = pybind11;

namespace huira {

    template <typename TSpectral>
    inline void bind_scene(py::module_& m) {
        using SceneType = Scene<TSpectral>;

        py::class_<SceneType>(m, "Scene")
            .def(py::init<>())

            // Camera models
            .def("new_camera_model", &SceneType::new_camera_model,
                py::arg("name") = "",
                "Create a new camera model and return its handle")
            .def("get_camera_model", &SceneType::get_camera_model,
                py::arg("name"),
                "Get a camera model handle by name")
            .def("delete_camera_model", &SceneType::delete_camera_model,
                py::arg("camera_model_handle"))
            .def("set_camera_model_name",
                static_cast<void (SceneType::*)(const CameraModelHandle<TSpectral>&, const std::string&)>(&SceneType::set_name),
                py::arg("camera_model_handle"), py::arg("name"),
                "Set the name of a camera model")

            // Star loading
            .def("load_stars", &SceneType::load_stars,
                py::arg("star_catalog_path"),
                py::arg("time"),
                py::arg("min_magnitude") = 100.f,
                "Load stars from a catalog file for the given observation time")

            // Debug printing
            .def("print_contents", &SceneType::print_contents)
            .def("print_graph", &SceneType::print_graph)

            .def("__repr__", [](const SceneType&) {
            return "<Scene>";
                });
    }

}
