#include "tycho2.hpp"

#include <iostream>

namespace huira::cli::tycho2 {

int process(const std::filesystem::path& input_dir, const Context& ctx) {
    if (ctx.verbose) {
        std::cout << "Processing Tycho-2 catalog from: " << input_dir << "\n";
    }

    if (!std::filesystem::exists(input_dir)) {
        std::cerr << "Input directory does not exist: " << input_dir << "\n";
        return 1;
    }

    // TODO: actual processing logic
    // For now, just verify the files exist
    for (int i = 0; i < 20; ++i) {
        auto filename = std::format("tyc2.dat.{:02d}", i);
        auto path = input_dir / filename;
        
        if (!std::filesystem::exists(path)) {
            std::cerr << "Missing file: " << path << "\n";
            return 1;
        }

        if (ctx.verbose) {
            std::cout << "  Found: " << filename << "\n";
        }
    }

    if (ctx.dry_run) {
        std::cout << "Dry run: would process " << input_dir << "\n";
        return 0;
    }

    // TODO: actual conversion to Huira format
    std::cout << "Processing complete.\n";
    return 0;
}

namespace {

int run(const Context& ctx, int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "Usage: huira process-tycho2 <input_directory>\n";
        std::cerr << "Example: huira process-tycho2 /home/user/huira_data/tycho2\n";
        return 1;
    }

    std::filesystem::path input_dir = argv[1];
    return process(input_dir, ctx);
}

const bool registered = [] {
    Registry::instance().add({
        "process-tycho2",
        "Convert Tycho-2 catalog to Huira format",
        run
    });
    return true;
}();

}  // namespace

}  // namespace huira::cli::tycho2
