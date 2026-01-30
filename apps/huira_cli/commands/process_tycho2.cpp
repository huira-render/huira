#include <iostream>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

#include "tclap/CmdLine.h"

#include "huira_cli/cli.hpp"
#include "huira_cli/progress_bar.hpp"
#include "huira_cli/commands/tycho2.hpp"

#include "huira/stars/io/star_data.hpp"
#include "huira/stars/io/load_tycho2.hpp"

namespace fs = std::filesystem;

namespace huira::cli::tycho2 {

    int process_tycho2(const fs::path& input_dir, const fs::path& output_dir, const Context& ctx, bool clean) {
        if (ctx.verbose) {
            std::cout << "Reading Tycho-2 catalog from: " << input_dir << "\n";
        }
    
        if (!std::filesystem::exists(input_dir)) {
            std::cerr << "Input directory does not exist: " << input_dir << "\n";
            return 1;
        }

        fs::path output_path = output_dir / "tycho2.hrsc";

        std::unique_ptr<indicators::ProgressBar> bar;
        if (!ctx.verbose) {
            bar = make_progress_bar("Tycho-2 Process ", 20+1);
        }

        std::vector<StarData> all_stars;
        for (int i = 0; i < 20; ++i) {
            auto filename = std::format("tyc2.dat.{:02d}", i);
            auto path = input_dir / filename;
            
            if (!std::filesystem::exists(path)) {
                std::cerr << "Could not find file: " << path << "\n";
                return 1;
            }
    
            if (ctx.verbose) {
                std::cout << "Reading " << filename << "\n";
            }
            else {
                update_bar(bar, "Reading " + filename);
            }

            std::vector<StarData> new_stars = huira::read_tycho2_dat(path);
            all_stars.insert(all_stars.end(), new_stars.begin(), new_stars.end());
        }

        if (ctx.verbose) {
            std::cout << "Reading files completed.\n";
        }
        else {
            update_bar(bar, "Saving to " + output_path.string());
        }
        
        write_star_data(output_path, all_stars);

        if (ctx.verbose) {
            std::cout << std::to_string(all_stars.size()) << " stars written to " << output_path << "\n";
        }
        else {
            finish_bar(bar, std::to_string(all_stars.size()) + " stars written to " + output_path.string());
        }

        if (clean) {
            if (ctx.verbose) {
                std::cout << "Cleaning up .dat files...\n";
            }
            for (int i = 0; i < 20; ++i) {
                auto filename = std::format("tyc2.dat.{:02d}", i);
                auto path = input_dir / filename;
                std::filesystem::remove(path);
            }
        }

        return 0;
    }
    
    int run_process(const Context& ctx, int argc, char** argv) {
        try {
            TCLAP::CmdLine cmd("Convert Tycho-2 catalog to Huira Star Catalog (.hrsc) format", ' ', "", false);
    
            TCLAP::UnlabeledValueArg<std::string> input_arg("input",
                "Input directory with tyc2.dat files", true, "", "input_directory", cmd);
    
            TCLAP::UnlabeledValueArg<std::string> output_arg("output",
                "Output directory for tycho2.hrsc", true, "", "output_directory", cmd);
    
            cmd.parse(argc, argv);
    
            fs::path input_dir = fs::path(input_arg.getValue());
            fs::path output_dir = fs::path(output_arg.getValue());
            return process_tycho2(input_dir, output_dir, ctx);
    
        }
        catch (TCLAP::ArgException& e) {
            std::cerr << "Error: " << e.error() << " for arg " << e.argId() << "\n";
            return 1;
        }
    }
    
    const bool registered = [] {
        Registry::instance().add({
            "process-tycho2",
            "Convert Tycho-2 catalog to Huira Star Catalog (.hrsc) format",
            run_process
        });
        return true;
    }();
}
