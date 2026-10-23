#include <array>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <vector>

#include "catch2/catch_test_macros.hpp"
#include "catch2/matchers/catch_matchers_string.hpp"
#include "huira/core/spectral_bins.hpp"
#include "huira/images/image.hpp"
#include "huira/images/io/hdr_io.hpp"
#include "huira/util/logger.hpp"

using namespace huira;
using Catch::Matchers::ContainsSubstring;
namespace fs = std::filesystem;

namespace {

using Bytes = std::vector<unsigned char>;
using Rgbe = std::array<std::uint8_t, 4>;

/// A Radiance HDR file: the header text, then the scanlines' bytes.
Bytes hdr_file(const std::string& header, const Bytes& scanlines = {})
{
    Bytes bytes(header.begin(), header.end());
    bytes.insert(bytes.end(), scanlines.begin(), scanlines.end());
    return bytes;
}

/// Flat (not run-length encoded) scanlines of these pixels, in order.
Bytes flat(const std::vector<Rgbe>& pixels)
{
    Bytes bytes;
    for (const Rgbe& p : pixels) {
        bytes.insert(bytes.end(), p.begin(), p.end());
    }
    return bytes;
}

/// An RGBE pixel's value: each mantissa times 2^(exponent - 136), or 0 if the exponent is 0.
RGB value(const Rgbe& p)
{
    if (p[3] == 0) {
        return RGB{0.f};
    }
    const float scale = std::ldexp(1.f, static_cast<int>(p[3]) - 136);
    return RGB{static_cast<float>(p[0]) * scale,
               static_cast<float>(p[1]) * scale,
               static_cast<float>(p[2]) * scale};
}

const std::string HEADER = "#?RADIANCE\nFORMAT=32-bit_rle_rgbe\n\n";

// Two rows of three, top first:
const std::vector<Rgbe> PIXELS{
    Rgbe{128, 64, 32, 129}, // 1, 0.5, 0.25
    Rgbe{0, 0, 0, 0},       // a zero exponent is black
    Rgbe{255, 0, 128, 136}, // a scale of 1
    Rgbe{1, 2, 3, 135},
    Rgbe{128, 128, 128, 128},
    Rgbe{10, 20, 30, 130},
};

void check_pixels(const ImageBundle<RGB>& bundle)
{
    REQUIRE(bundle.image.resolution() == Resolution{3, 2});
    for (int y = 0; y < 2; ++y) {
        for (int x = 0; x < 3; ++x) {
            CHECK(bundle.image(x, y) == value(PIXELS[static_cast<std::size_t>(y * 3 + x)]));
        }
    }
}

/// Collects the warnings logged while it exists.
struct WarningLog {
    std::vector<std::string> messages;

    WarningLog()
    {
        Logger::set_custom_sink([this](const LogEntry& entry) {
            if (entry.level == LogLevel::Warning) {
                messages.push_back(entry.message);
            }
        });
    }
    ~WarningLog() { Logger::clear_custom_sink(); }

    WarningLog(const WarningLog&) = delete;
    WarningLog& operator=(const WarningLog&) = delete;
};

fs::path write_temp(const std::string& name, const Bytes& bytes)
{
    const fs::path path = fs::temp_directory_path() / ("huira_test_hdr_" + name);
    std::ofstream file(path, std::ios::binary);
    file.write(reinterpret_cast<const char*>(bytes.data()),
               static_cast<std::streamsize>(bytes.size()));
    return path;
}

} // namespace

TEST_CASE("A flat Radiance HDR is read as linear RGB", "[images][hdr]")
{
    const Bytes file = hdr_file(HEADER + "-Y 2 +X 3\n", flat(PIXELS));
    const ImageBundle<RGB> bundle = read_image_hdr(file.data(), file.size());
    check_pixels(bundle);
    CHECK(bundle.image(0, 0) == RGB{1.f, 0.5f, 0.25f});
    CHECK(bundle.color_space == ColorSpaceHint::Linear);
    CHECK(bundle.alpha.empty());
}

TEST_CASE("A Radiance HDR is read as mono by averaging its channels", "[images][hdr]")
{
    const Bytes file = hdr_file(HEADER + "-Y 2 +X 3\n", flat(PIXELS));
    const ImageBundle<float> bundle = read_image_hdr_mono(file.data(), file.size());
    REQUIRE(bundle.image.resolution() == Resolution{3, 2});
    CHECK(bundle.color_space == ColorSpaceHint::Linear);
    for (int y = 0; y < 2; ++y) {
        for (int x = 0; x < 3; ++x) {
            const RGB rgb = value(PIXELS[static_cast<std::size_t>(y * 3 + x)]);
            CHECK(bundle.image(x, y) == (rgb[0] + rgb[1] + rgb[2]) / 3.f);
        }
    }
}

TEST_CASE("Run-length encoded Radiance HDR scanlines are decoded", "[images][hdr]")
{
    // One scanline of 8: each component is encoded separately, in runs and literals.
    Bytes scanline{2, 2, 0, 8};
    scanline.insert(scanline.end(), {128 + 8, 200});                     // red: a run of 8
    scanline.insert(scanline.end(), {8, 0, 10, 20, 30, 40, 50, 60, 70}); // green: 8 literals
    scanline.insert(scanline.end(), {128 + 3, 7, 5, 1, 2, 3, 4, 5});     // blue: both
    scanline.insert(scanline.end(), {128 + 100, 136});                   // a run past the end
    const Bytes file = hdr_file(HEADER + "-Y 1 +X 8\n", scanline);

    const ImageBundle<RGB> bundle = read_image_hdr(file.data(), file.size());
    REQUIRE(bundle.image.resolution() == Resolution{8, 1});
    const std::array<float, 8> blue{7.f, 7.f, 7.f, 1.f, 2.f, 3.f, 4.f, 5.f};
    for (int x = 0; x < 8; ++x) {
        CHECK(bundle.image(x, 0) ==
              RGB{200.f, static_cast<float>(10 * x), blue[static_cast<std::size_t>(x)]});
    }
}

TEST_CASE("Radiance HDR headers are read in their common variants", "[images][hdr]")
{
    SECTION("the RGBE magic number, other header lines and CRLF line endings")
    {
        const Bytes file = hdr_file("#?RGBE\r\n# made by Huira's tests\r\nEXPOSURE=1.0\r\n"
                                    "FORMAT=32-bit_rle_rgbe\r\n\r\n-Y 2 +X 3\r\n",
                                    flat(PIXELS));
        check_pixels(read_image_hdr(file.data(), file.size()));
    }
    SECTION("the XYZE format")
    {
        const Bytes file =
            hdr_file("#?RADIANCE\nFORMAT=32-bit_rle_xyze\n\n-Y 2 +X 3\n", flat(PIXELS));
        check_pixels(read_image_hdr(file.data(), file.size()));
    }
    SECTION("no format, which is taken to be RGBE with a warning")
    {
        const Bytes file = hdr_file("#?RADIANCE\n\n-Y 2 +X 3\n", flat(PIXELS));
        WarningLog log;
        check_pixels(read_image_hdr(file.data(), file.size()));
        REQUIRE(log.messages.size() == 1);
        CHECK_THAT(log.messages[0], ContainsSubstring("No FORMAT line"));
    }
    SECTION("a single column")
    {
        const Bytes file =
            hdr_file(HEADER + "-Y 2 +X 1\n", flat({Rgbe{1, 1, 1, 136}, Rgbe{2, 2, 2, 136}}));
        const ImageBundle<float> bundle = read_image_hdr_mono(file.data(), file.size());
        CHECK(bundle.image(0, 0) == 1.f);
        CHECK(bundle.image(0, 1) == 2.f);
    }
}

TEST_CASE("Radiance HDR scanlines are placed in the order the resolution string gives",
          "[images][hdr]")
{
    // Every order was read as -Y +X, top to bottom and left to right: +Y (bottom to top) came
    // out upside down, and -X (right to left) mirrored.
    for (const bool bottom_up : {false, true}) {
        for (const bool right_to_left : {false, true}) {
            const std::string resolution = std::string(bottom_up ? "+Y" : "-Y") + " 2 " +
                                           (right_to_left ? "-X" : "+X") + " 3\n";
            INFO(resolution);
            const Bytes file = hdr_file(HEADER + resolution, flat(PIXELS));
            const ImageBundle<RGB> bundle = read_image_hdr(file.data(), file.size());
            const ImageBundle<float> mono = read_image_hdr_mono(file.data(), file.size());
            REQUIRE(bundle.image.resolution() == Resolution{3, 2});
            for (int y = 0; y < 2; ++y) {
                for (int x = 0; x < 3; ++x) {
                    // The pixel's place in the file: its scanline, and its place in that:
                    const int scanline = bottom_up ? 1 - y : y;
                    const int place = right_to_left ? 2 - x : x;
                    const RGB expected =
                        value(PIXELS[static_cast<std::size_t>(scanline * 3 + place)]);
                    CHECK(bundle.image(x, y) == expected);
                    CHECK(mono.image(x, y) == (expected[0] + expected[1] + expected[2]) / 3.f);
                }
            }
        }
    }
}

TEST_CASE("Radiance HDR files are read from a path", "[images][hdr]")
{
    const fs::path path = write_temp("file.hdr", hdr_file(HEADER + "-Y 2 +X 3\n", flat(PIXELS)));
    check_pixels(read_image_hdr(path));
    CHECK(read_image_hdr_mono(path).image.resolution() == Resolution{3, 2});
    fs::remove(path);

    CHECK_THROWS_AS(read_image_hdr(fs::temp_directory_path() / "huira_test_hdr_missing.hdr"),
                    std::runtime_error);
}

TEST_CASE("Radiance HDR data that is invalid or unsupported is rejected", "[images][hdr]")
{
    const auto check_rejected = [](const Bytes& file, const std::string& reason) {
        CHECK_THROWS_WITH(read_image_hdr(file.data(), file.size()), ContainsSubstring(reason));
        CHECK_THROWS_WITH(read_image_hdr_mono(file.data(), file.size()), ContainsSubstring(reason));
    };

    check_rejected(Bytes{}, "Failed to read HDR header");
    check_rejected(hdr_file("P6\n3 2\n255\n"), "bad magic");
    check_rejected(hdr_file(HEADER), "Failed to read resolution string");
    check_rejected(hdr_file(HEADER + "+X 3 +Y 2\n", flat(PIXELS)), "Unsupported resolution");
    check_rejected(hdr_file(HEADER + "-Y two +X 3\n", flat(PIXELS)), "Unsupported resolution");
    check_rejected(hdr_file(HEADER + "-Y 0 +X 3\n"), "Invalid dimensions");
    check_rejected(hdr_file(HEADER + "-Y 2 +X -3\n"), "Invalid dimensions");

    // Scanlines that end early:
    check_rejected(hdr_file(HEADER + "-Y 2 +X 3\n"), "Unexpected end of data at scanline 0");
    check_rejected(hdr_file(HEADER + "-Y 2 +X 3\n", flat({PIXELS[0], PIXELS[1], PIXELS[2]})),
                   "Unexpected end of data at scanline 1");
    check_rejected(hdr_file(HEADER + "-Y 1 +X 3\n", flat({PIXELS[0], PIXELS[1]})),
                   "Unexpected end of data");
    const std::string rle_header = HEADER + "-Y 1 +X 8\n";
    for (const Bytes& scanline :
         {Bytes{2, 2, 0, 8}, Bytes{2, 2, 0, 8, 128 + 8}, Bytes{2, 2, 0, 8, 3, 1}}) {
        check_rejected(hdr_file(rle_header, scanline), "Unexpected end of RLE data");
    }
}
