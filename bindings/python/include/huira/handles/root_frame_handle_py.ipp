#pragma once

#include "pybind11/pybind11.h"
#include "pybind11/stl.h"

#include "huira/handles/root_frame_handle.hpp"

namespace py = pybind11;

namespace huira {
    template <typename TSpectral>
    inline void bind_root_frame_handle(py::module_& m) {
        using HandleType = RootFrameHandle<TSpectral>;

        py::class_<HandleType>(m, "RootFrameHandle")
            // Subframe management (inherited from FrameHandle, still valid)
            .def("new_subframe", &HandleType::new_subframe)
            .def("new_spice_subframe", &HandleType::new_spice_subframe,
                py::arg("spice_origin"), py::arg("spice_frame"))
            .def("delete_subframe", &HandleType::delete_subframe,
                py::arg("subframe"))

            // Instance management (inherited from FrameHandle, still valid)
            .def("new_instance", [](const HandleType& self, const CameraModelHandle<TSpectral>& asset) {
                return self.new_instance(asset);
            }, py::arg("asset_handle"))
            .def("new_instance", [](const HandleType& self, const LightHandle<TSpectral>& asset) {
                return self.new_instance(asset);
            }, py::arg("asset_handle"))
            .def("new_instance", [](const HandleType& self, const UnresolvedObjectHandle<TSpectral>& asset) {
                return self.new_instance(asset);
            }, py::arg("asset_handle"))
            .def("delete_instance", &HandleType::delete_instance,
                py::arg("instance"))

            // SPICE (read-only)
            .def("get_spice_origin", &HandleType::get_spice_origin)
            .def("get_spice_frame", &HandleType::get_spice_frame)

            // Parent access
            .def("get_parent", &HandleType::get_parent)

            .def("valid", &HandleType::valid)
            .def("__bool__", &HandleType::valid)
            .def("__repr__", [](const HandleType&) {
                return "<RootFrameHandle>";
            });

        // NOTE: Transform methods (set_position, set_rotation, set_scale,
        // set_velocity, set_angular_velocity) are intentionally omitted —
        // they are deleted in the C++ class to keep the root frame fixed.
    }
}
