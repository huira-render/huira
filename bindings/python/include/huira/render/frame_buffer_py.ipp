#pragma once

#include <sstream>

#include "pybind11/pybind11.h"

#include "huira/render/frame_buffer.hpp"

namespace py = pybind11;

namespace huira {

    template <IsSpectral TSpectral>
    void bind_frame_buffer(py::module_& m) {
        using FB = FrameBuffer<TSpectral>;
        using SensorT = typename FB::SensorT;

        std::string name = "FrameBuffer";

        py::class_<FB>(m, name.c_str())
            // No public constructor (FrameBuffer() = delete, private ctor)
            // Created via CameraModel, so no .def(py::init<>())

            // Resolution
            .def_property_readonly("width", &FB::width)
            .def_property_readonly("height", &FB::height)
            .def_property_readonly("resolution", [](const FB& fb) {
            auto r = fb.resolution();
            return py::make_tuple(r.width, r.height);
                })

            // -- Depth --------------------------------------------------------
            .def("enable_depth", &FB::enable_depth, py::arg("enable") = true)
            .def("has_depth", &FB::has_depth)
            .def_property_readonly("depth", [](FB& fb) -> Image<float>&{
            return fb.depth();
                }, py::return_value_policy::reference_internal)

            // -- Mesh IDs -----------------------------------------------------
            .def("enable_mesh_ids", &FB::enable_mesh_ids, py::arg("enable") = true)
            .def("has_mesh_ids", &FB::has_mesh_ids)
            .def_property_readonly("mesh_ids", [](FB& fb) -> Image<uint64_t>&{
            return fb.mesh_ids();
                }, py::return_value_policy::reference_internal)

            // -- Camera Normals -----------------------------------------------
            .def("enable_camera_normals", &FB::enable_camera_normals, py::arg("enable") = true)
            .def("has_camera_normals", &FB::has_camera_normals)
            .def_property_readonly("camera_normals", [](FB& fb) -> Image<Vec3<float>>&{
            return fb.camera_normals();
                }, py::return_value_policy::reference_internal)

            // -- World Normals ------------------------------------------------
            .def("enable_world_normals", &FB::enable_world_normals, py::arg("enable") = true)
            .def("has_world_normals", &FB::has_world_normals)
            .def_property_readonly("world_normals", [](FB& fb) -> Image<Vec3<float>>&{
            return fb.world_normals();
                }, py::return_value_policy::reference_internal)

            // -- Received Power -----------------------------------------------
            .def("enable_received_power", &FB::enable_received_power, py::arg("enable") = true)
            .def("has_received_power", &FB::has_received_power)
            .def_property_readonly("received_power", [](FB& fb) -> Image<TSpectral>&{
            return fb.received_power();
                }, py::return_value_policy::reference_internal)

            // -- Sensor Response ----------------------------------------------
            .def("enable_sensor_response", &FB::enable_sensor_response, py::arg("enable") = true)
            .def("has_sensor_response", &FB::has_sensor_response)
            .def_property_readonly("sensor_response", [](FB& fb) -> Image<SensorT>&{
            return fb.sensor_response();
                }, py::return_value_policy::reference_internal)

            // -- Clear --------------------------------------------------------
            .def("clear", &FB::clear)

            .def("__repr__", [](const FB& fb) {
            std::ostringstream os;
            os << "FrameBuffer(" << fb.width() << "x" << fb.height();
            if (fb.has_depth())           os << ", depth";
            if (fb.has_mesh_ids())        os << ", mesh_ids";
            if (fb.has_camera_normals())  os << ", camera_normals";
            if (fb.has_world_normals())   os << ", world_normals";
            if (fb.has_received_power())  os << ", received_power";
            if (fb.has_sensor_response()) os << ", sensor_response";
            os << ")";
            return os.str();
                })
            ;
    }

}
