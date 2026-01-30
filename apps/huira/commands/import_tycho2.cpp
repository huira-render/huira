#include "tycho2.hpp"

#include <iostream>

namespace huira::cli::tycho2 {

namespace {

int run(const Context& ctx, int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "Usage: huira import-tycho2 <data_directory>\n";
        std::cerr << "Example: huira import-tycho2 /home/user/huira_data\n";
        std::cerr << "\n";
        std::cerr << "This command downloads the Tycho-2 catalog and converts it to Huira format.\n";
        std::cerr << "Equivalent to running:\n";
        std::cerr << "  huira fetch-tycho2 <data_directory>\n";
        std::cerr << "  huira process-tycho2 <data_directory>/tycho2\n";
        return 1;
    }

    std::filesystem::path base_dir = argv[1];
    std::filesystem::path tycho2_dir = base_dir / "tycho2";

    // Step 1: Fetch
    std::cout << "=== Fetching Tycho-2 catalog ===\n";
    int result = fetch(tycho2_dir, ctx);
    if (result != 0) {
        std::cerr << "Fetch failed, aborting import.\n";
        return result;
    }

    // Step 2: Process
    std::cout << "\n=== Processing Tycho-2 catalog ===\n";
    result = process(tycho2_dir, ctx);
    if (result != 0) {
        std::cerr << "Processing failed.\n";
        return result;
    }

    std::cout << "\nImport complete.\n";
    return 0;
}

const bool registered = [] {
    Registry::instance().add({
        "import-tycho2",
        "Download and convert Tycho-2 catalog (fetch + process)",
        run
    });
    return true;
}();

}  // namespace

}  // namespace huira::cli::tycho2
