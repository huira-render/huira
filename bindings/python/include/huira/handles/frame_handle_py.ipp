#pragma once

#include "pybind11/pybind11.h"
#include "pybind11/stl.h"

#include "huira/handles/frame_handle.hpp"
#include "huira/handles/node_handle_py.ipp"

namespace py = pybind11;

namespace huira {
    template <typename TSpectral>
    inline void bind_frame_handle(py::module_& m) {
        using HandleType = FrameHandle<TSpectral>;

        auto cls = py::class_<HandleType>(m, "FrameHandle")
            // Subframe management
            .def("new_subframe", &HandleType::new_subframe)
            .def("new_spice_subframe", &HandleType::new_spice_subframe,
                py::arg("spice_origin"), py::arg("spice_frame"))
            .def("delete_subframe", &HandleType::delete_subframe,
                py::arg("subframe"))

            // Instance management
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

            .def("valid", &HandleType::valid)
            .def("__bool__", &HandleType::valid)
            .def("__repr__", [](const HandleType&) {
                return "<FrameHandle>";
            });

        bind_node_handle_methods<TSpectral, FrameNode<TSpectral>>(cls);
    }
}
