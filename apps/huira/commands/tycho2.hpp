#pragma once

#include "../cli.hpp"

#include <filesystem>

namespace huira::cli::tycho2 {

    // Download Tycho-2 catalog files to the specified directory.
    // Returns 0 on success, non-zero on failure.
    int fetch(const std::filesystem::path& output_dir, const Context& ctx, bool force = false);

    // Process Tycho-2 catalog files from the specified directory.
    // Returns 0 on success, non-zero on failure.
    int process(const std::filesystem::path& input_dir, const Context& ctx);

}
