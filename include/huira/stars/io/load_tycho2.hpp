#pragma once

#include <filesystem>
#include <vector>

#include "huira/stars/io/star_data.hpp"

namespace fs = std::filesystem;

namespace huira {

/// Reads one of the main Tycho-2 files (tyc2.dat.00 .. tyc2.dat.19).
inline std::vector<StarData> read_tycho2_dat(const fs::path& filepath);

/// Reads one of the Tycho-2 supplement files (suppl_1.dat, suppl_2.dat).
inline std::vector<StarData> read_tycho2_suppl(const fs::path& filepath);
} // namespace huira

#include "huira_impl/stars/io/load_tycho2.ipp"
