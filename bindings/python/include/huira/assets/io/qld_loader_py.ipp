#pragma once

#include <filesystem>
#include <string>

#include "huira/assets/io/qld_loader.hpp"
#include "huira/core/spectral_bins.hpp"
#include "huira/handles/materials/material_handle.hpp"
#include "huira/handles/scene/frame_handle.hpp"
#include "huira/scene/scene.hpp"
#include "pybind11/pybind11.h"
#include "pybind11/stl/filesystem.h"

namespace py = pybind11;

namespace huira {

inline void bind_qld_loader(py::module_& m)
{
    m.def("add_qld_tiles",
          [](Scene<Visible8>& scene,
             FrameHandle<Visible8>& frame,
             const std::filesystem::path& dir,
             MaterialHandle<Visible8>& material,
             int lod,
             double unit_scale,
             const std::string& name,
             double gsd) {
              qld::add_tiles(scene, frame, dir, material, lod, unit_scale, name, gsd);
          },
          py::arg("scene"),
          py::arg("frame"),
          py::arg("dir"),
          py::arg("material"),
          py::arg("lod"),
          py::arg("unit_scale") = 1.0,
          py::arg("name") = "qld",
          py::arg("gsd") = -1.0,
          "Add QLD terrain/albedo tiles to a Visible8 scene.");
}

} // namespace huira
