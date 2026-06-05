#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include "curl/curl.h"
#include "huira_cli/cli.hpp"
#include "tclap/CmdLine.h"

namespace fs = std::filesystem;

namespace huira::cli {
namespace {

static size_t write_cb(void* p, size_t s, size_t n, void* u) noexcept
{
    auto* out = static_cast<std::ofstream*>(u);
    std::size_t bytes = s * n;
    out->write(static_cast<const char*>(p), static_cast<std::streamsize>(bytes));
    return out->good() ? bytes : 0;
}

static bool get(const std::string& url, const fs::path& out, bool force)
{
    if (!force && fs::exists(out)) {
        std::cout << "skip " << out.filename().string() << "\n";
        return true;
    }
    fs::create_directories(out.parent_path());
    std::cout << "get  " << out.filename().string() << "\n";
    std::ofstream f(out, std::ios::binary);
    CURL* c = curl_easy_init();
    if (!f || !c) {
        if (c) curl_easy_cleanup(c);
        return false;
    }
    curl_easy_setopt(c, CURLOPT_URL, url.c_str());
    curl_easy_setopt(c, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(c, CURLOPT_FAILONERROR, 1L);
    curl_easy_setopt(c, CURLOPT_WRITEFUNCTION, write_cb);
    curl_easy_setopt(c, CURLOPT_WRITEDATA, &f);
    CURLcode rc = curl_easy_perform(c);
    curl_easy_cleanup(c);
    f.close();
    if (rc != CURLE_OK) {
        std::cerr << "fail " << out.filename().string() << ": " << curl_easy_strerror(rc) << "\n";
        fs::remove(out);
        return false;
    }
    return true;
}

static int run_fetch_moon_data(const Context&, int argc, char** argv)
{
    TCLAP::CmdLine cmd("Download small Moon DEM/albedo set for QLD generation", ' ', HUIRA_VERSION);
    TCLAP::SwitchArg force("f", "force", "Overwrite existing files", cmd, false);
    TCLAP::UnlabeledValueArg<std::string> out_arg("output", "Output data directory", true, "", "dir", cmd);
    cmd.parse(argc, argv);

    fs::path root = out_arg.getValue();
    bool ok = true;
    curl_global_init(CURL_GLOBAL_DEFAULT);

    const std::string dem = "https://imbrium.mit.edu/DATA/LOLA_GDR/CYLINDRICAL/IMG/";
    for (const char* f : {"LDEM_16.IMG", "LDEM_16.LBL"}) {
        ok &= get(dem + f, root / "dems" / f, force.getValue());
    }

    const std::string alb = "https://pds.lroc.asu.edu/data/LRO-L-LROC-5-RDR-V1.0/LROLRC_2001/DATA/MDR/WAC_EMP/";
    for (const char* f : {
             "WAC_EMP_643NM_P900N0000_304P.IMG",
             "WAC_EMP_643NM_P900S0000_304P.IMG",
             "WAC_EMP_643NM_E300N2250_064P.IMG",
             "WAC_EMP_643NM_E300N3150_064P.IMG",
             "WAC_EMP_643NM_E300N0450_064P.IMG",
             "WAC_EMP_643NM_E300N1350_064P.IMG",
             "WAC_EMP_643NM_E300S2250_064P.IMG",
             "WAC_EMP_643NM_E300S3150_064P.IMG",
             "WAC_EMP_643NM_E300S0450_064P.IMG",
             "WAC_EMP_643NM_E300S1350_064P.IMG"}) {
        ok &= get(alb + f, root / "albedo" / f, force.getValue());
    }

    curl_global_cleanup();
    return ok ? 0 : 1;
}

struct RegisterFetchMoonData {
    RegisterFetchMoonData()
    {
        Registry::instance().add({"fetch-moon-data",
                                  "Download small Moon DEM/albedo dataset",
                                  run_fetch_moon_data});
    }
};

static RegisterFetchMoonData reg;

} // namespace
} // namespace huira::cli
