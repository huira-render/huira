#include <iostream>
#include <filesystem>

#include "tclap/CmdLine.h"

#include "huira_cli/cli.hpp"
#include "huira_cli/progress_bar.hpp"
#include "huira_cli/compact_output.hpp"

namespace fs = std::filesystem;

namespace huira::cli::dems {
    int process_dems(const fs::path& input_dir, const fs::path& output_dir, const Context& ctx) {
        if (ctx.verbose) {
            std::cout << "Processing DEMs from: " << input_dir << "\n";
        }
    
        if (!std::filesystem::exists(input_dir)) {
            std::cerr << "Input directory does not exist: " << input_dir << "\n";
            return 1;
        }
        fs::path output_path = output_dir / "dems_processed.dat";

        // TODO Process DEMs

        if (ctx.verbose) {
            std::cout << "Processing completed. Output written to: " << output_path << "\n";
        }
        return 0;
    }

    static int run_process(const Context& ctx, int argc, char** argv) {
        try {
            TCLAP::CmdLine cmd("Process Digital Elevation Models (DEMs)", ' ', "");
    
            TCLAP::UnlabeledValueArg<std::string> input_arg("input",
                "Input directory with DEM files", true, "", "input_directory", cmd);

            TCLAP::UnlabeledValueArg<std::string> output_arg("output",
                "Output directory for processed DEMs", true, "", "output_directory", cmd);
    
            cmd.parse(argc, argv);
    
            fs::path input_dir(input_arg.getValue());
            fs::path output_dir(output_arg.getValue());
    
            return process_dems(input_dir, output_dir, ctx);
        }
        catch (const TCLAP::ArgException& e) {
            std::cerr << "Error parsing arguments: " << e.what() << "\n";
            return 1;
        }
    }

    const bool registered = []() {
        Registry::instance().add({
            "process_dems",
            "Process Digital Elevation Models (DEMs)",
            run_process
        });
        return true;
    }();
}
