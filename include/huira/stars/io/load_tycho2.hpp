#pragma once

#include <filesystem>
#include "huira/stars/io/star_data.hpp"

namespace fs = std::filesystem;

namespace huira {

    std::vector<StarData> read_tycho2_dat(const fs::path& filepath);

    std::vector<StarData> read_tycho2_suppl(const fs::path& filepath);
}

#include "huira_impl/stars/io/load_tycho2.ipp"
