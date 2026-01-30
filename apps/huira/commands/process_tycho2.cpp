#include "../cli.hpp"

#include <iostream>

using namespace huira::cli;

namespace {

    int run(const Context& ctx, int argc, char** argv) {
        if (argc < 2) {
            std::cerr << "Usage: huira process-tycho2 <files...>\n";
            return 1;
        }

        if (ctx.verbose) {
            std::cout << "Processing Tycho-2 catalog\n";
        }

        (void)argv;
        return 0;
    }

    // self-register
    const bool registered = [] {
        Registry::instance().add({
            "process-tycho2",
            "Convert Tycho-2 catalog to Huira format",
            run
        });
        return true;
    }();

}
