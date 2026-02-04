#include <filesystem>
#include <format>
#include <fstream>
#include <stdexcept>
#include <memory>

#include "curl/curl.h"
#include "tclap/CmdLine.h"

#include "huira_cli/cli.hpp"
#include "huira_cli/progress_bar.hpp"
#include "huira_cli/commands/tycho2.hpp"
#include "huira/util/paths.hpp"

namespace fs = std::filesystem;

namespace huira::cli::tycho2 {

    // Callback for libcurl to write data to a file
    static size_t write_callback(void* ptr, size_t size, size_t nmemb, void* userdata) noexcept {
        try {
            auto* out = static_cast<std::ofstream*>(userdata);
            const size_t bytes = size * nmemb;
            out->write(static_cast<const char*>(ptr), static_cast<std::streamsize>(bytes));
            return out->good() ? bytes : 0;
        } catch (...) {
            // Prevent exceptions from propagating to C code
            return 0;
        }
    }

    // Download a single file. Returns true on success.
    static bool download_file(const std::string& url, const fs::path& dest, const Context& ctx) {
        if (ctx.verbose) {
            std::cout << "  " << url << "\n";
            std::cout << "  -> " << dest << "\n";
        }

        CURL* curl = curl_easy_init();
        if (!curl) {
            std::cerr << "Failed to initialize curl\n";
            return false;
        }

        std::ofstream out(dest, std::ios::binary);
        if (!out) {
            std::cerr << "Failed to open output file: " << dest << "\n";
            curl_easy_cleanup(curl);
            return false;
        }

        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &out);
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
        curl_easy_setopt(curl, CURLOPT_ACCEPT_ENCODING, "");  // Accept any encoding, auto-decompress
        curl_easy_setopt(curl, CURLOPT_FAILONERROR, 1L);      // Fail on HTTP errors (4xx, 5xx)

        CURLcode res = curl_easy_perform(curl);
        curl_easy_cleanup(curl);
        out.close();

        if (res != CURLE_OK) {
            std::cerr << "Download failed: " << curl_easy_strerror(res) << "\n";
            fs::remove(dest);  // Clean up partial file
            return false;
        }

        return true;
    }

    int fetch_tycho2(const fs::path& output_dir, const Context& ctx, bool force, bool process, bool clean) {
        huira::make_path(output_dir);

        constexpr std::string_view base_url = 
            "https://cdsarc.cds.unistra.fr/viz-bin/nph-Cat/txt?I/259";

        // Global curl init (once per process is fine)
        static bool curl_initialized = [] {
            return curl_global_init(CURL_GLOBAL_DEFAULT) == CURLE_OK;
        }();
        
        if (!curl_initialized) {
            std::cerr << "Failed to initialize libcurl\n";
            return 1;
        }

        // Create list of files to download:
        std::size_t file_count = tycho2_dat_files.size() + tycho2_suppl_files.size();

        // Create progress bar for non-verbose mode
        std::unique_ptr<indicators::ProgressBar> bar;
        if (!ctx.verbose) {
            bar = make_progress_bar("Tycho-2 Download ", file_count);
        }

        int failures = 0;
        for (const auto& filename : tycho2_dat_files) {
            std::string url = std::format("{}/{}.gz", base_url, filename);
            fs::path dest = output_dir / filename;

            if (ctx.verbose) {
                std::cout << "Downloading " << filename << "...\n";
            } else {
                update_bar(bar, "Downloading " + filename);
            }

            if (fs::exists(dest) && !force) {
                if (ctx.verbose) {
                    std::cout << "  Skipping (already exists)\n";
                } 
                continue;
            }

            if (!download_file(url, dest, ctx)) {
                ++failures;
            }
        }

        for (const auto& filename : tycho2_suppl_files) {
            std::string url = std::format("{}/{}.gz", base_url, filename);
            fs::path dest = output_dir / filename;

            if (ctx.verbose) {
                std::cout << "Downloading " << filename << "...\n";
            }
            else {
                update_bar(bar, "Downloading " + filename);
            }

            if (fs::exists(dest) && !force) {
                if (ctx.verbose) {
                    std::cout << "  Skipping (already exists)\n";
                }
                continue;
            }

            if (!download_file(url, dest, ctx)) {
                ++failures;
            }
        }

        // Restore cursor visibility and show completion message
        if (!ctx.verbose) {
            finish_bar(bar, "Complete: " + std::to_string(file_count) + " files");
        }

        if (failures > 0) {
            std::cerr << failures << " file(s) failed to download\n";
            return 1;
        }

        if (ctx.verbose) {
            std::cout << "Downloaded to: " << output_dir << "\n";
        }

        if (process) {
            if (ctx.verbose) {
                std::cout << "Processing downloaded files...\n";
            }
            return process_tycho2(output_dir, output_dir, ctx, clean);
        }

        return 0;
    }

    static int run_fetch(const Context& ctx, int argc, char** argv) {
        try {
            TCLAP::CmdLine cmd("Download Tycho-2 catalog from CDS Strasbourg", ' ', "", false);

            TCLAP::SwitchArg force_arg("f", "force", 
                "Overwrite existing files instead of skipping", cmd, false);

            TCLAP::SwitchArg process_arg("p", "process",
                "Automatically process the files once downloaded", cmd, false);

            TCLAP::SwitchArg clean_arg("c", "clean",
                "Remove downloaded .dat files after processing", cmd, false);

            TCLAP::UnlabeledValueArg<std::string> output_arg("output", 
                "Output directory for downloaded files", true, "", "output_directory", cmd);

            cmd.parse(argc, argv);

            fs::path output_dir = fs::path(output_arg.getValue());
            return fetch_tycho2(output_dir, ctx, force_arg.getValue(), process_arg.getValue(), clean_arg.getValue());

        } catch (TCLAP::ArgException& e) {
            std::cerr << "Error: " << e.error() << " for arg " << e.argId() << "\n";
            return 1;
        }
    }

    const bool registered = [] {
        Registry::instance().add({
            "fetch-tycho2",
            "Download Tycho-2 catalog from CDS Strasbourg",
            run_fetch
        });
        return true;
    }();

}
