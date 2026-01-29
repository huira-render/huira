#pragma once

#include <filesystem>
#include <vector>

namespace fs = std::filesystem;

namespace huira {

    struct StarData {
        double RA;
        double DEC;

        double pmRA;
        double pmDE;

        double temperature;
        double solid_angle;
        double visual_magnitude;
    };

    std::vector<StarData> read_tycho2_dat(const fs::path& filepath);

}

#include "huira_impl/stars/io/load_tycho2.ipp"
