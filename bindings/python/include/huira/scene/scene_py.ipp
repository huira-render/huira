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

            // Root frame access
            .def_readonly("root", &SceneType::root)

            // =============================================================
            // Lights
            // =============================================================

            // new_point_light — spectral power
            .def("new_point_light",
                static_cast<LightHandle<TSpectral>(SceneType::*)(
                    const units::SpectralWatts<TSpectral>&, std::string)>(
                        &SceneType::new_point_light),
                py::arg("spectral_power"), py::arg("name") = "",
                "Create a point light from spectral power (SpectralWatts)")

            // new_point_light — scalar power
            .def("new_point_light",
                [](SceneType& self, const py::object& power, std::string name) {
                    return self.new_point_light(
                        detail::unit_from_py<units::Watt>(power), std::move(name));
                },
                py::arg("total_power"), py::arg("name") = "",
                "Create a point light from total power (any power unit)")

            .def("new_sun_light", &SceneType::new_sun_light,
                "Create a sun light source")

            .def("get_light", &SceneType::get_light,
                py::arg("name"),
                "Get a light handle by name")
            .def("delete_light", &SceneType::delete_light,
                py::arg("light_handle"))
            .def("set_light_name",
                static_cast<void (SceneType::*)(
                    const LightHandle<TSpectral>&, const std::string&)>(
                        &SceneType::set_name),
                py::arg("light_handle"), py::arg("name"),
                "Set the name of a light")

            // =============================================================
            // Unresolved Objects
            // =============================================================

            // new_unresolved_object — spectral irradiance
            .def("new_unresolved_object",
                static_cast<UnresolvedObjectHandle<TSpectral>(SceneType::*)(
                    const units::SpectralWattsPerMeterSquared<TSpectral>&, std::string)>(
                        &SceneType::new_unresolved_object),
                py::arg("spectral_irradiance"), py::arg("name") = "",
                "Create an unresolved object from spectral irradiance")

            // new_unresolved_object — scalar irradiance
            .def("new_unresolved_object",
                [](SceneType& self, const py::object& irradiance, std::string name) {
                    return self.new_unresolved_object(
                        detail::unit_from_py<units::WattsPerMeterSquared>(irradiance),
                        std::move(name));
                },
                py::arg("irradiance"), py::arg("name") = "",
                "Create an unresolved object from scalar irradiance (any irradiance unit)")

            // new_unresolved_object_from_magnitude — no albedo
            .def("new_unresolved_object_from_magnitude",
                static_cast<UnresolvedObjectHandle<TSpectral>(SceneType::*)(
                    double, std::string)>(
                        &SceneType::new_unresolved_object_from_magnitude),
                py::arg("visual_magnitude"), py::arg("name") = "",
                "Create an unresolved object from visual magnitude")

            // new_unresolved_object_from_magnitude — with albedo
            .def("new_unresolved_object_from_magnitude",
                static_cast<UnresolvedObjectHandle<TSpectral>(SceneType::*)(
                    double, TSpectral, std::string)>(
                        &SceneType::new_unresolved_object_from_magnitude),
                py::arg("visual_magnitude"), py::arg("albedo"), py::arg("name") = "",
                "Create an unresolved object from visual magnitude with spectral albedo")

            // new_unresolved_emitter — spectral power
            .def("new_unresolved_emitter",
                static_cast<UnresolvedObjectHandle<TSpectral>(SceneType::*)(
                    const units::SpectralWatts<TSpectral>&, std::string)>(
                        &SceneType::new_unresolved_emitter),
                py::arg("spectral_power"), py::arg("name") = "",
                "Create an unresolved emitter from spectral power")

            // new_unresolved_emitter — scalar power
            .def("new_unresolved_emitter",
                [](SceneType& self, const py::object& power, std::string name) {
                    return self.new_unresolved_emitter(
                        detail::unit_from_py<units::Watt>(power), std::move(name));
                },
                py::arg("power"), py::arg("name") = "",
                "Create an unresolved emitter from total power (any power unit)")

            // new_unresolved_sphere — no albedo
            .def("new_unresolved_sphere",
                [](SceneType& self, const py::object& radius,
                    InstanceHandle<TSpectral> sun, std::string name) {
                        return self.new_unresolved_sphere(
                            detail::unit_from_py<units::Meter>(radius),
                            std::move(sun), std::move(name));
                },
                py::arg("radius"), py::arg("sun"), py::arg("name") = "",
                "Create an unresolved sphere (accepts any distance unit for radius)")

            // new_unresolved_sphere — spectral albedo
            .def("new_unresolved_sphere",
                [](SceneType& self, const py::object& radius,
                    InstanceHandle<TSpectral> sun, TSpectral albedo, std::string name) {
                        return self.new_unresolved_sphere(
                            detail::unit_from_py<units::Meter>(radius),
                            std::move(sun), std::move(albedo), std::move(name));
                },
                py::arg("radius"), py::arg("sun"), py::arg("albedo"),
                py::arg("name") = "",
                "Create an unresolved sphere with spectral albedo")

            // new_unresolved_sphere — scalar albedo
            .def("new_unresolved_sphere",
                [](SceneType& self, const py::object& radius,
                    InstanceHandle<TSpectral> sun, float albedo, std::string name) {
                        return self.new_unresolved_sphere(
                            detail::unit_from_py<units::Meter>(radius),
                            std::move(sun), albedo, std::move(name));
                },
                py::arg("radius"), py::arg("sun"), py::arg("scalar_albedo"),
                py::arg("name") = "",
                "Create an unresolved sphere with scalar albedo")

            // new_unresolved_asteroid — no albedo
            .def("new_unresolved_asteroid",
                static_cast<UnresolvedObjectHandle<TSpectral>(SceneType::*)(
                    double, double, InstanceHandle<TSpectral>, std::string)>(
                        &SceneType::new_unresolved_asteroid),
                py::arg("H"), py::arg("G"), py::arg("sun"), py::arg("name") = "",
                "Create an unresolved asteroid from H, G magnitude parameters")

            // new_unresolved_asteroid — spectral albedo
            .def("new_unresolved_asteroid",
                static_cast<UnresolvedObjectHandle<TSpectral>(SceneType::*)(
                    double, double, InstanceHandle<TSpectral>, TSpectral, std::string)>(
                        &SceneType::new_unresolved_asteroid),
                py::arg("H"), py::arg("G"), py::arg("sun"), py::arg("albedo"),
                py::arg("name") = "",
                "Create an unresolved asteroid with spectral albedo")

            // new_unresolved_asteroid — scalar albedo
            .def("new_unresolved_asteroid",
                static_cast<UnresolvedObjectHandle<TSpectral>(SceneType::*)(
                    double, double, InstanceHandle<TSpectral>, float, std::string)>(
                        &SceneType::new_unresolved_asteroid),
                py::arg("H"), py::arg("G"), py::arg("sun"), py::arg("scalar_albedo"),
                py::arg("name") = "",
                "Create an unresolved asteroid with scalar albedo")

            .def("get_unresolved_object", &SceneType::get_unresolved_object,
                py::arg("name"),
                "Get an unresolved object handle by name")
            .def("delete_unresolved_object", &SceneType::delete_unresolved_object,
                py::arg("unresolved_object_handle"))
            .def("set_unresolved_object_name",
                static_cast<void (SceneType::*)(
                    const UnresolvedObjectHandle<TSpectral>&, const std::string&)>(
                        &SceneType::set_name),
                py::arg("unresolved_object_handle"), py::arg("name"),
                "Set the name of an unresolved object")

            // =============================================================
            // Camera models
            // =============================================================

            .def("new_camera_model", &SceneType::new_camera_model,
                py::arg("name") = "",
                "Create a new camera model and return its handle")
            .def("get_camera_model", &SceneType::get_camera_model,
                py::arg("name"),
                "Get a camera model handle by name")
            .def("delete_camera_model", &SceneType::delete_camera_model,
                py::arg("camera_model_handle"))
            .def("set_camera_model_name",
                static_cast<void (SceneType::*)(
                    const CameraModelHandle<TSpectral>&, const std::string&)>(
                        &SceneType::set_name),
                py::arg("camera_model_handle"), py::arg("name"),
                "Set the name of a camera model")

            // =============================================================
            // Star loading
            // =============================================================

            .def("load_stars", &SceneType::load_stars,
                py::arg("star_catalog_path"),
                py::arg("time"),
                py::arg("min_magnitude") = 100.f,
                "Load stars from a catalog file for the given observation time")

            // =============================================================
            // Debug printing
            // =============================================================

            .def("print_contents", &SceneType::print_contents)
            .def("print_graph", &SceneType::print_graph)

            .def("__repr__", [](const SceneType&) {
            return "<Scene>";
                });
    }

}
