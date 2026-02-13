#include <iostream>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

#include "tclap/CmdLine.h"

#include "huira_cli/cli.hpp"
#include "huira_cli/progress_bar.hpp"
#include "huira_cli/compact_output.hpp"
#include "huira_cli/commands/tycho2.hpp"

#include "huira/stars/io/star_data.hpp"
#include "huira/stars/io/star_catalog.hpp"
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

        std::size_t file_count = tycho2_dat_files.size() + tycho2_suppl_files.size();

        std::unique_ptr<indicators::ProgressBar> bar;
        if (!ctx.verbose) {
            bar = make_progress_bar("Tycho-2 Process  ", file_count+1);
        }

        std::vector<StarData> all_stars;
        for (const auto& filename : tycho2_dat_files) {
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

        for (const auto& filename : tycho2_suppl_files) {
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

            std::vector<StarData> new_stars = huira::read_tycho2_suppl(path);
            all_stars.insert(all_stars.end(), new_stars.begin(), new_stars.end());
        }

        StarCatalog catalog(all_stars);
        catalog.set_catalog_type(CatalogType::Tycho2);

        if (ctx.verbose) {
            std::cout << "Reading files completed.\n";
        }
        else {
            update_bar(bar, "Saving");
        }
        
        catalog.write_star_data(output_path);

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
            for (const auto& filename : tycho2_dat_files) {
                auto path = input_dir / filename;
                std::filesystem::remove(path);
            }
            for (const auto& filename : tycho2_suppl_files) {
                auto path = input_dir / filename;
                std::filesystem::remove(path);
            }
        }

        return 0;
    }
    
    static int run_process(const Context& ctx, int argc, char** argv) {
        try {
            TCLAP::CmdLine cmd("Convert Tycho-2 catalog to Huira Star Catalog (.hrsc) format", ' ', "");
    
            TCLAP::UnlabeledValueArg<std::string> input_arg("input",
                "Input directory with tyc2.dat files", true, "", "input_directory", cmd);
    
            TCLAP::ValueArg<std::string> output_arg("o", "output",
                "Output directory for tycho2.hrsc", false, "", "output_directory", cmd);

            CompactOutput compact;
            cmd.setOutput(&compact);

            cmd.parse(argc, argv);
    
            fs::path input_dir = fs::path(input_arg.getValue());
            fs::path output_dir = fs::path(output_arg.getValue());
            if (output_dir.empty()) {
                output_dir = input_dir;
            }
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
