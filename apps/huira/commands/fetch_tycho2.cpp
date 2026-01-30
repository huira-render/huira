#include "../cli.hpp"

#include <curl/curl.h>

#include <filesystem>
#include <format>
#include <fstream>
#include <stdexcept>

namespace fs = std::filesystem;

namespace huira::cli::tycho2 {

namespace {

// Callback for libcurl to write data to a file
size_t write_callback(void* ptr, size_t size, size_t nmemb, void* userdata) {
    auto* out = static_cast<std::ofstream*>(userdata);
    const size_t bytes = size * nmemb;
    out->write(static_cast<const char*>(ptr), static_cast<std::streamsize>(bytes));
    return out->good() ? bytes : 0;
}

// Download a single file. Returns true on success.
bool download_file(const std::string& url, const fs::path& dest, const Context& ctx) {
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

}  // namespace

// Exposed so other commands can call this directly
int fetch(const fs::path& output_dir, const Context& ctx) {
    constexpr std::string_view base_url = 
        "https://cdsarc.cds.unistra.fr/viz-bin/nph-Cat/txt?I/259";
    constexpr int file_count = 20;

    if (ctx.verbose) {
        std::cout << "Fetching Tycho-2 catalog to: " << output_dir << "\n";
    }

    if (!ctx.dry_run) {
        std::error_code ec;
        fs::create_directories(output_dir, ec);
        if (ec) {
            std::cerr << "Failed to create directory: " << output_dir 
                      << " (" << ec.message() << ")\n";
            return 1;
        }
    }

    // Global curl init (once per process is fine)
    static bool curl_initialized = [] {
        return curl_global_init(CURL_GLOBAL_DEFAULT) == CURLE_OK;
    }();
    
    if (!curl_initialized) {
        std::cerr << "Failed to initialize libcurl\n";
        return 1;
    }

    int failures = 0;
    for (int i = 0; i < file_count; ++i) {
        std::string filename = std::format("tyc2.dat.{:02d}", i);
        std::string url = std::format("{}/{}.gz", base_url, filename);
        fs::path dest = output_dir / filename;

        std::cout << "Downloading " << filename << "...\n";

        if (ctx.dry_run) {
            continue;
        }

        if (fs::exists(dest)) {
            if (ctx.verbose) {
                std::cout << "  Skipping (already exists)\n";
            }
            continue;
        }

        if (!download_file(url, dest, ctx)) {
            ++failures;
            // Continue with other files rather than aborting
        }
    }

    if (failures > 0) {
        std::cerr << failures << " file(s) failed to download\n";
        return 1;
    }

    std::cout << "Downloaded to: " << output_dir << "\n";
    return 0;
}

namespace {

int run(const Context& ctx, int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "\nUsage: huira fetch-tycho2 <output_directory>\n\n";
        return 1;
    }

    fs::path output_dir = fs::path(argv[1]) / "tycho2";
    return fetch(output_dir, ctx);
}

const bool registered = [] {
    Registry::instance().add({
        "fetch-tycho2",
        "Download Tycho-2 catalog from CDS Strasbourg",
        run
    });
    return true;
}();

}  // namespace

}  // namespace huira::cli::tycho2
