#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <limits>
#include <string>
#include <vector>

#include "catch2/catch_test_macros.hpp"
#include "catch2/matchers/catch_matchers_floating_point.hpp"
#include "catch2/matchers/catch_matchers_string.hpp"
#include "huira/core/constants.hpp"
#include "huira/stars/io/load_tycho2.hpp"
#include "huira/stars/io/star_catalog.hpp"
#include "huira/stars/io/star_data.hpp"
#include "huira/stars/io/tycho2_id.hpp"

using namespace huira;
using Catch::Matchers::ContainsSubstring;
using Catch::Matchers::WithinRel;
namespace fs = std::filesystem;

namespace {

fs::path temp_file(const std::string& name)
{
    return fs::temp_directory_path() / ("huira_test_star_catalog_" + name);
}

StarData star(std::uint64_t id, float magnitude)
{
    StarData s;
    s.id = id;
    s.RA = 0.1 * static_cast<double>(id);
    s.DEC = -0.05 * static_cast<double>(id);
    s.pmRA = 1.5f;
    s.pmDEC = -2.5f;
    s.solid_angle = 1e-16 * static_cast<double>(id + 1);
    s.temperature = 5000.f + static_cast<float>(id);
    s.visual_magnitude = magnitude;
    return s;
}

std::vector<char> read_bytes(const fs::path& path)
{
    std::ifstream in(path, std::ios::binary);
    return std::vector<char>((std::istreambuf_iterator<char>(in)),
                             std::istreambuf_iterator<char>());
}

void write_bytes(const fs::path& path, const std::vector<char>& bytes)
{
    std::ofstream out(path, std::ios::binary);
    out.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
}

template <typename T>
T little_endian_at(const std::vector<char>& bytes, std::size_t offset)
{
    T value = 0;
    for (std::size_t i = 0; i < sizeof(T); ++i) {
        value = static_cast<T>(
            value | (static_cast<T>(static_cast<unsigned char>(bytes[offset + i])) << (8 * i)));
    }
    return value;
}

/// A catalog of three stars written to disk: magnitudes 3, 1, 2 (unsorted) plus one without a
/// magnitude, which the writer drops.
fs::path write_sample(const std::string& name)
{
    StarCatalog catalog;
    catalog.set_catalog_type(CatalogType::Tycho2);
    catalog.add_star(star(30, 3.f));
    catalog.add_star(star(10, 1.f));
    catalog.add_star(star(99, std::numeric_limits<float>::quiet_NaN()));
    catalog.add_star(star(20, 2.f));
    const fs::path path = temp_file(name + ".hrsc");
    catalog.write_star_data(path);
    return path;
}

} // namespace

TEST_CASE("HRSC header is written field by field, little-endian, at the documented offsets",
          "[stars][hrsc]")
{
    const fs::path path = write_sample("header");
    const std::vector<char> bytes = read_bytes(path);

    REQUIRE(bytes.size() == HRSC_HEADER_SIZE + 3 * sizeof(StarData));
    CHECK(std::memcmp(bytes.data(), "HRSC", 4) == 0);
    CHECK(little_endian_at<std::uint16_t>(bytes, 4) == HRSC_FORMAT_VERSION);
    CHECK(little_endian_at<std::uint16_t>(bytes, 6) == HRSC_DATA_VERSION);
    CHECK(static_cast<unsigned char>(bytes[8]) == static_cast<unsigned char>(CatalogType::Tycho2));
    CHECK((static_cast<unsigned char>(bytes[9]) & HRSC_FLAG_LITTLE_ENDIAN) != 0);
    CHECK(little_endian_at<std::uint16_t>(bytes, 10) == sizeof(StarData));
    CHECK(little_endian_at<std::uint32_t>(bytes, 12) == HRSC_HEADER_SIZE);
    CHECK(little_endian_at<std::uint64_t>(bytes, 16) == 3);
    CHECK(little_endian_at<std::uint64_t>(bytes, 24) == 0);

    // The first record is the brightest star, and its id is the first field:
    CHECK(little_endian_at<std::uint64_t>(bytes, HRSC_HEADER_SIZE) == 10);

    const HrscHeader header = StarCatalog::read_header(path);
    CHECK(header.format_version == HRSC_FORMAT_VERSION);
    CHECK(header.data_version == HRSC_DATA_VERSION);
    CHECK(header.catalog_type == CatalogType::Tycho2);
    CHECK(header.star_count == 3);

    fs::remove(path);
}

TEST_CASE("HRSC round trip keeps every field, sorted by magnitude, without NaN magnitudes",
          "[stars][hrsc]")
{
    const fs::path path = write_sample("roundtrip");
    const StarCatalog catalog = StarCatalog::read_star_data(path);

    REQUIRE(catalog.size() == 3);
    CHECK(catalog.get_catalog_type() == CatalogType::Tycho2);
    CHECK(catalog.get_format_version() == HRSC_FORMAT_VERSION);
    CHECK(catalog.get_data_version() == HRSC_DATA_VERSION);
    CHECK(catalog.get_star_id(0) == format_tycho2_id(10));

    const std::array<std::uint64_t, 3> ids{10, 20, 30};
    for (std::size_t i = 0; i < 3; ++i) {
        const StarData expected = star(ids[i], static_cast<float>(i + 1));
        const StarData& actual = catalog[i];
        CHECK(actual.id == expected.id);
        CHECK(actual.RA == expected.RA);
        CHECK(actual.DEC == expected.DEC);
        CHECK(actual.pmRA == expected.pmRA);
        CHECK(actual.pmDEC == expected.pmDEC);
        CHECK(actual.solid_angle == expected.solid_angle);
        CHECK(actual.temperature == expected.temperature);
        CHECK(actual.visual_magnitude == expected.visual_magnitude);
    }

    fs::remove(path);
}

TEST_CASE("HRSC magnitude cutoff reads a prefix of the sorted file", "[stars][hrsc]")
{
    const fs::path path = write_sample("cutoff");

    CHECK(StarCatalog::read_star_data(path, 2.f).size() == 2);
    CHECK(StarCatalog::read_star_data(path, 1.5f).size() == 1);
    CHECK(StarCatalog::read_star_data(path, 0.f).size() == 0);
    CHECK(StarCatalog::read_star_data(path, -std::numeric_limits<float>::infinity()).size() == 0);
    CHECK_THROWS_WITH(StarCatalog::read_star_data(path, std::numeric_limits<float>::quiet_NaN()),
                      ContainsSubstring("NaN"));

    fs::remove(path);
}

TEST_CASE("HRSC reader rejects what it cannot read, saying which side is behind", "[stars][hrsc]")
{
    const fs::path path = write_sample("reject");
    const std::vector<char> good = read_bytes(path);

    SECTION("not an .hrsc file")
    {
        std::vector<char> bytes = good;
        bytes[0] = 'X';
        write_bytes(path, bytes);
        CHECK_THROWS_WITH(StarCatalog::read_star_data(path), ContainsSubstring("Not a Huira"));
    }

    SECTION("a file from before the format was versioned")
    {
        // The pre-release header was the memory image of a struct: "HRSC", uint8 version 1
        // and 3 bytes of padding, uint64 reserved, uint8 catalog type and 7 bytes of padding,
        // uint64 star count. Only its magic and first version byte line up with the versioned
        // header, so it must be told apart before anything else is checked.
        std::vector<char> bytes = good;
        std::fill(bytes.begin() + 4, bytes.begin() + 32, '\0');
        bytes[4] = 1;
        bytes[16] = static_cast<char>(CatalogType::Tycho2);
        bytes[24] = 3;
        write_bytes(path, bytes);
        CHECK_THROWS_WITH(StarCatalog::read_star_data(path),
                          ContainsSubstring("before the star catalog format was versioned"));
    }

    SECTION("a newer format asks for a newer huira")
    {
        std::vector<char> bytes = good;
        bytes[4] = static_cast<char>(HRSC_FORMAT_VERSION + 1);
        write_bytes(path, bytes);
        CHECK_THROWS_WITH(StarCatalog::read_star_data(path), ContainsSubstring("update huira"));
    }

    SECTION("an older format asks for a newer catalog")
    {
        std::vector<char> bytes = good;
        bytes[4] = 1; // retired: the pre-release reader's version byte, never written since
        write_bytes(path, bytes);
        CHECK_THROWS_WITH(StarCatalog::read_star_data(path),
                          ContainsSubstring("download or build a current catalog"));
    }

    SECTION("a newer data version is accepted and reported")
    {
        std::vector<char> bytes = good;
        bytes[6] = static_cast<char>(HRSC_DATA_VERSION + 7);
        write_bytes(path, bytes);
        const StarCatalog catalog = StarCatalog::read_star_data(path);
        CHECK(catalog.size() == 3);
        CHECK(catalog.get_data_version() == HRSC_DATA_VERSION + 7);
    }

    SECTION("a big-endian catalog")
    {
        std::vector<char> bytes = good;
        bytes[9] = 0;
        write_bytes(path, bytes);
        CHECK_THROWS_WITH(StarCatalog::read_star_data(path), ContainsSubstring("endian"));
    }

    SECTION("a different record size")
    {
        std::vector<char> bytes = good;
        bytes[10] = 56;
        write_bytes(path, bytes);
        CHECK_THROWS_WITH(StarCatalog::read_star_data(path),
                          ContainsSubstring("byte star records"));
    }

    SECTION("a truncated file")
    {
        std::vector<char> bytes = good;
        bytes.resize(bytes.size() - 1);
        write_bytes(path, bytes);
        CHECK_THROWS_WITH(StarCatalog::read_star_data(path), ContainsSubstring("truncated"));
    }

    SECTION("a header padded beyond 32 bytes is skipped to the records")
    {
        std::vector<char> bytes = good;
        bytes[12] = 40; // header_size
        bytes.insert(bytes.begin() + static_cast<std::ptrdiff_t>(HRSC_HEADER_SIZE), 8, '\0');
        write_bytes(path, bytes);
        const StarCatalog catalog = StarCatalog::read_star_data(path, 1.5f);
        REQUIRE(catalog.size() == 1);
        CHECK(catalog[0].id == 10);
    }

    fs::remove(path);
}

TEST_CASE("Tycho-2 records are parsed in both the raw CDS and VizieR export shapes",
          "[stars][tycho2]")
{
    // Two records from tyc2.dat.00 as VizieR's text export writes them: one with a mean
    // position, one (pflag 'X') with only an observed position and no proper motion.
    const std::string normal =
        "0001|00008|1| |  2.31750494   2.23184345|  -16.3|   -9.0| 68| 73| 1.7| 1.8|1958.89|"
        "1951.94| 4|1.0|1.0|0.9|1.0|12.146|0.158|12.146|0.223|999| |      |   |  2.31754222   "
        "2.23186444|1.67|1.54| 88.0|100.8| |-0.2";
    const std::string observed_only =
        "0001|00041|1|X|                         |       |       |   |   |    |    |       |"
        "       |  |   |   |   |   |13.035|0.320|12.235|0.220|999| |      |   |  1.80874194   "
        "2.31573611|1.75|1.77|112.2|148.2| | 0.0";
    // The first record with both magnitudes blanked, which the reader skips:
    std::string no_magnitudes = normal;
    no_magnitudes.replace(110, 6, "      ");
    no_magnitudes.replace(123, 6, "      ");

    const fs::path path = temp_file("tyc2.dat");
    {
        std::ofstream out(path);
        out << "#Table: I/259/tyc2.dat.00.gz\n"
            << "----\n"
            << normal << "\n"
            << no_magnitudes << "\n"
            << observed_only << "\n";
    }

    const std::vector<StarData> stars = read_tycho2_dat(path);
    REQUIRE(stars.size() == 2);

    constexpr double deg = PI<double>() / 180.0;

    CHECK(stars[0].id == pack_tycho2_id(1, 8, 1));
    CHECK_THAT(stars[0].RA, WithinRel(2.31750494 * deg, 1e-12));
    CHECK_THAT(stars[0].DEC, WithinRel(2.23184345 * deg, 1e-12));
    CHECK(stars[0].pmRA == -16.3f);
    CHECK(stars[0].pmDEC == -9.0f);
    CHECK(stars[0].visual_magnitude == 12.146f);

    // Observed position, brought from its epoch (1991.75 / 1991.77) to J2000 with zero proper
    // motion, is unchanged. Its sign and leading digit survive the column read:
    CHECK(stars[1].id == pack_tycho2_id(1, 41, 1));
    CHECK_THAT(stars[1].RA, WithinRel(1.80874194 * deg, 1e-12));
    CHECK_THAT(stars[1].DEC, WithinRel(2.31573611 * deg, 1e-12));
    CHECK_THAT(stars[1].visual_magnitude, WithinRel(12.235 - 0.090 * (13.035 - 12.235), 1e-6));

    // The skipped record must not leak its proper motion into the next star's slot:
    CHECK(stars[1].pmRA == 0.f);
    CHECK(stars[1].pmDEC == 0.f);

    // The raw CDS file has no separator between HIP (bytes 143-148) and CCDM (149-151), so
    // from there on it sits one byte to the left of the export. The same record, raw:
    std::string observed_only_raw = observed_only;
    REQUIRE(observed_only_raw[148] == '|');
    observed_only_raw.erase(148, 1);
    REQUIRE(observed_only_raw[151] == '|');
    REQUIRE(observed_only_raw[177] == '|');
    {
        std::ofstream out(path);
        out << normal << "\n" << observed_only_raw << "\n";
    }
    const std::vector<StarData> raw = read_tycho2_dat(path);
    REQUIRE(raw.size() == 2);
    CHECK(raw[1].id == stars[1].id);
    CHECK(raw[1].RA == stars[1].RA);
    CHECK(raw[1].DEC == stars[1].DEC);
    CHECK(raw[1].visual_magnitude == stars[1].visual_magnitude);

    // A record with a proper motion and an epoch, in both shapes, lands in the same place:
    // the RA (100.13 deg) keeps its hundreds digit and the epoch its integer part.
    std::string moving = observed_only;
    moving.replace(41, 7, "  -30.0"); // pmRA
    moving.replace(49, 7, "   20.0"); // pmDE
    moving.replace(153, 12, "100.13082417");
    moving.replace(166, 12, " -10.5979541");
    moving.replace(179, 4, "1.95");
    moving.replace(184, 4, "1.89");
    std::string moving_raw = moving;
    moving_raw.erase(148, 1);
    {
        std::ofstream out(path);
        out << moving << "\n" << moving_raw << "\n";
    }
    const std::vector<StarData> both = read_tycho2_dat(path);
    REQUIRE(both.size() == 2);
    CHECK(both[0].RA == both[1].RA);
    CHECK(both[0].DEC == both[1].DEC);
    CHECK(both[0].RA > 100.0 * deg);
    CHECK(both[0].DEC < -10.0 * deg);
    CHECK(both[0].pmRA == -30.f);

    // Anything else is refused rather than read at the wrong columns:
    std::string broken = observed_only;
    broken.erase(148, 2);
    {
        std::ofstream out(path);
        out << broken << "\n";
    }
    CHECK_THROWS_WITH(read_tycho2_dat(path), ContainsSubstring("neither a raw CDS"));

    fs::remove(path);
}

TEST_CASE("Tycho-2 colors outside the range of real stars are clamped, not rejected",
          "[stars][tycho2]")
{
    // TYC 10-415-1: BT = 12.380, VT = 13.209, so B-V = -0.705, past the pole of the
    // temperature relation at -0.674. Unclamped, its temperature was -158297 K and its solid
    // angle negative, and the renderer dropped it with a warning.
    StarData noisy_blue;
    noisy_blue.process_magnitude(12.380, 13.209);

    StarData bluest;
    bluest.process_magnitude(13.0, 13.0 - BV_COLOR_INDEX_MIN / 0.850);

    CHECK(noisy_blue.temperature == bluest.temperature);
    CHECK(noisy_blue.solid_angle > 0.0);
    CHECK(std::isfinite(noisy_blue.solid_angle));
    CHECK_THAT(noisy_blue.visual_magnitude, WithinRel(13.209 - 0.090 * (12.380 - 13.209), 1e-6));

    // The reddest real stars, and beyond:
    StarData noisy_red;
    noisy_red.process_magnitude(16.0, 12.0);
    StarData reddest;
    reddest.process_magnitude(13.0, 13.0 - BV_COLOR_INDEX_MAX / 0.850);
    CHECK(noisy_red.temperature == reddest.temperature);
    CHECK(noisy_red.solid_angle > 0.0);

    // The limits of the clamp are the limits of real stars, roughly O-type to M giants:
    CHECK(bluest.temperature > 20000.f);
    CHECK(bluest.temperature < 25000.f);
    CHECK(reddest.temperature > 2500.f);
    CHECK(reddest.temperature < 3000.f);

    // An ordinary color is untouched:
    StarData sun_like;
    sun_like.process_magnitude(13.0, 13.0 - 0.65 / 0.850);
    CHECK_THAT(sun_like.temperature, WithinRel(5800.f, 0.03f));

    // Whatever the photometry, what the builder writes passes the renderer's checks:
    for (double bt_minus_vt : {-5.0, -1.8, -0.8, -0.3, 0.0, 0.76, 2.0, 3.5, 10.0}) {
        StarData star;
        star.process_magnitude(13.0 + bt_minus_vt, 13.0);
        CHECK(star.temperature > 0.f);
        CHECK(std::isfinite(star.temperature));
        CHECK(star.solid_angle > 0.0);
        CHECK(std::isfinite(star.solid_angle));
    }
}
