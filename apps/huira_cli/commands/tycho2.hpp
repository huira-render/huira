#pragma once

#include "huira_cli/cli.hpp"

#include <filesystem>
#include <string>

namespace fs = std::filesystem;

namespace huira::cli::tycho2 {

    // Download Tycho-2 catalog files to the specified directory.
    // Returns 0 on success, non-zero on failure.
    int fetch_tycho2(const fs::path& output_dir, const Context& ctx, bool force = false, bool process = false, bool clean = false);

    // Process Tycho-2 catalog files from the specified directory.
    // Returns 0 on success, non-zero on failure.
    int process_tycho2(const fs::path& input_dir, const fs::path& output_dir, const Context& ctx, bool clean = false);

}
