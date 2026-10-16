// build_star_catalog: converts the Tycho-2 catalog files into a Huira star catalog (.hrsc).
//
// This is the tool that builds the catalog published on media.huira.space. Users do not run
// it; they download the result. Build it with -DHUIRA_BUILD_TOOLS=ON.
//
//   build_star_catalog <tycho2_dir> [output.hrsc]
//
// <tycho2_dir> holds the files as distributed by CDS (VizieR catalog I/259) and mirrored at
// media.huira.space/data/stars/tycho2/archive/: tyc2.dat.00 .. tyc2.dat.19, suppl_1.dat and
// suppl_2.dat, uncompressed. The output defaults to <tycho2_dir>/tycho2.hrsc.
//
// Publishing a new build: if the values changed (see HRSC_DATA_VERSION in star_catalog.hpp),
// bump that constant first, so the file says which build it is; then upload the file as
// archive/tycho2-d<N>.hrsc, copy it to tycho2.hrsc, and add both to SHA256SUMS.

#include <cstdint>
#include <exception>
#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

#include "huira/stars/io/load_tycho2.hpp"
#include "huira/stars/io/star_catalog.hpp"
#include "huira/stars/io/star_data.hpp"

namespace fs = std::filesystem;

namespace {

const std::vector<std::string> tycho2_dat_files{
    "tyc2.dat.00", "tyc2.dat.01", "tyc2.dat.02", "tyc2.dat.03", "tyc2.dat.04",
    "tyc2.dat.05", "tyc2.dat.06", "tyc2.dat.07", "tyc2.dat.08", "tyc2.dat.09",
    "tyc2.dat.10", "tyc2.dat.11", "tyc2.dat.12", "tyc2.dat.13", "tyc2.dat.14",
    "tyc2.dat.15", "tyc2.dat.16", "tyc2.dat.17", "tyc2.dat.18", "tyc2.dat.19",
};
const std::vector<std::string> tycho2_suppl_files{"suppl_1.dat", "suppl_2.dat"};

int usage(std::ostream& os)
{
    os << "Usage: build_star_catalog <tycho2_dir> [output.hrsc]\n"
          "\n"
          "Converts the Tycho-2 catalog (tyc2.dat.00 .. tyc2.dat.19, suppl_1.dat, suppl_2.dat\n"
          "in <tycho2_dir>) into a Huira star catalog. The output defaults to\n"
          "<tycho2_dir>/tycho2.hrsc.\n";
    return 2;
}

} // namespace

int main(int argc, char** argv)
{
    if (argc < 2 || argc > 3) {
        return usage(std::cerr);
    }
    const std::string first = argv[1];
    if (first == "-h" || first == "--help") {
        usage(std::cout);
        return 0;
    }

    const fs::path input_dir = first;
    const fs::path output_path = argc == 3 ? fs::path(argv[2]) : input_dir / "tycho2.hrsc";

    if (!fs::is_directory(input_dir)) {
        std::cerr << "Not a directory: " << input_dir << "\n";
        return 1;
    }

    // Every input must be there before the (slow) read starts:
    bool missing = false;
    for (const auto& files : {tycho2_dat_files, tycho2_suppl_files}) {
        for (const auto& filename : files) {
            if (!fs::exists(input_dir / filename)) {
                std::cerr << "Missing: " << (input_dir / filename) << "\n";
                missing = true;
            }
        }
    }
    if (missing) {
        return 1;
    }

    try {
        huira::StarCatalog catalog;
        catalog.set_catalog_type(huira::CatalogType::Tycho2);

        for (const auto& filename : tycho2_dat_files) {
            std::cout << "Reading " << filename << "\n";
            for (const auto& star : huira::read_tycho2_dat(input_dir / filename)) {
                catalog.add_star(star);
            }
        }
        for (const auto& filename : tycho2_suppl_files) {
            std::cout << "Reading " << filename << "\n";
            for (const auto& star : huira::read_tycho2_suppl(input_dir / filename)) {
                catalog.add_star(star);
            }
        }

        catalog.write_star_data(output_path);

        const huira::HrscHeader header = huira::StarCatalog::read_header(output_path);
        std::cout << "\nWrote " << output_path << "\n"
                  << "  stars:          " << header.star_count << " (of " << catalog.size()
                  << " read)\n"
                  << "  format version: " << header.format_version << "\n"
                  << "  data version:   " << header.data_version << "\n"
                  << "\nIf the computed values changed since the last published build, make "
                     "sure HRSC_DATA_VERSION\n"
                     "in star_catalog.hpp was bumped before this build. To publish: upload as "
                     "archive/tycho2-d"
                  << header.data_version
                  << ".hrsc, copy to tycho2.hrsc, and add both to SHA256SUMS.\n";
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }

    return 0;
}
