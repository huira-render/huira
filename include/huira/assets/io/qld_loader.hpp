#pragma once

#include <filesystem>
#include <string>

#include "huira/core/spectral_bins.hpp"
#include "huira/handles/materials/material_handle.hpp"
#include "huira/scene/scene.hpp"

namespace fs = std::filesystem;

namespace huira::qld {

/// Add QLD terrain/albedo tiles to a Visible8 scene frame.
///
/// The loader selects the requested LoD, or the finest LoD whose ground sample
/// distance is no greater than `gsd` when `gsd` is positive. `unit_scale`
/// converts tile coordinates to scene units; Moon examples use the mean lunar
/// radius so the instance can be scaled back to meters in the scene graph.
template <typename FrameHandle>
void add_tiles(huira::Scene<huira::Visible8>& scene,
               FrameHandle& frame,
               const fs::path& dir,
               huira::MaterialHandle<huira::Visible8>& material,
               int lod,
               double unit_scale = 1.0,
               const std::string& name = "qld",
               double gsd = -1.0);

} // namespace huira::qld

#include "huira_impl/assets/io/qld_loader.ipp"
