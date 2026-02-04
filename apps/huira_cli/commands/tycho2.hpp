#pragma once

#include "huira_cli/cli.hpp"

#include <filesystem>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace huira::cli::tycho2 {

    const static std::vector<std::string> tycho2_dat_files{
        "tyc2.dat.00", "tyc2.dat.01", "tyc2.dat.02", "tyc2.dat.03", "tyc2.dat.04",
        "tyc2.dat.05", "tyc2.dat.06", "tyc2.dat.07", "tyc2.dat.08", "tyc2.dat.09",
        "tyc2.dat.10", "tyc2.dat.11", "tyc2.dat.12", "tyc2.dat.13", "tyc2.dat.14",
        "tyc2.dat.15", "tyc2.dat.16", "tyc2.dat.17", "tyc2.dat.18", "tyc2.dat.19",
    };
    const static std::vector<std::string> tycho2_suppl_files{ "suppl_1.dat", "suppl_2.dat" };

    // Download Tycho-2 catalog files to the specified directory.
    // Returns 0 on success, non-zero on failure.
    int fetch_tycho2(const fs::path& output_dir, const Context& ctx, bool force = false, bool process = false, bool clean = false);

    // Process Tycho-2 catalog files from the specified directory.
    // Returns 0 on success, non-zero on failure.
    int process_tycho2(const fs::path& input_dir, const fs::path& output_dir, const Context& ctx, bool clean = false);

}
