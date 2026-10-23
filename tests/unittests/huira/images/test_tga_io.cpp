#include <array>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <initializer_list>
#include <stdexcept>
#include <string>
#include <vector>

#include "catch2/catch_test_macros.hpp"
#include "catch2/matchers/catch_matchers_string.hpp"
#include "huira/core/spectral_bins.hpp"
#include "huira/images/image.hpp"
#include "huira/images/io/tga_io.hpp"

using namespace huira;
using Catch::Matchers::ContainsSubstring;
namespace fs = std::filesystem;

namespace {

using Bytes = std::vector<unsigned char>;
using Rgba = std::array<std::uint8_t, 4>;

constexpr std::uint8_t COLOR_MAPPED = 1;
constexpr std::uint8_t TRUE_COLOR = 2;
constexpr std::uint8_t GRAY = 3;
constexpr std::uint8_t RLE_COLOR_MAPPED = 9;
constexpr std::uint8_t RLE_TRUE_COLOR = 10;
constexpr std::uint8_t RLE_GRAY = 11;

constexpr std::uint8_t TOP_ORIGIN = 0x20;

struct TgaHeader {
    std::uint8_t image_type = TRUE_COLOR;
    std::uint8_t pixel_depth = 24;
    std::uint8_t descriptor = 0;
    std::uint16_t width = 2;
    std::uint16_t height = 2;
    std::uint8_t color_map_type = 0;
    std::uint16_t color_map_first = 0;
    std::uint16_t color_map_length = 0;
    std::uint8_t color_map_entry_bits = 0;
};

void append_u16(Bytes& bytes, std::uint16_t value)
{
    bytes.push_back(static_cast<unsigned char>(value & 0xFFu));
    bytes.push_back(static_cast<unsigned char>(value >> 8));
}

/// A TGA file: the 18-byte header, the image ID, the color map and the pixel data.
Bytes tga_file(const TgaHeader& header,
               const Bytes& pixels,
               const Bytes& color_map = {},
               const std::string& id = "")
{
    Bytes bytes;
    bytes.push_back(static_cast<unsigned char>(id.size()));
    bytes.push_back(header.color_map_type);
    bytes.push_back(header.image_type);
    append_u16(bytes, header.color_map_first);
    append_u16(bytes, header.color_map_length);
    bytes.push_back(header.color_map_entry_bits);
    append_u16(bytes, 0); // x origin
    append_u16(bytes, 0); // y origin
    append_u16(bytes, header.width);
    append_u16(bytes, header.height);
    bytes.push_back(header.pixel_depth);
    bytes.push_back(header.descriptor);
    bytes.insert(bytes.end(), id.begin(), id.end());
    bytes.insert(bytes.end(), color_map.begin(), color_map.end());
    bytes.insert(bytes.end(), pixels.begin(), pixels.end());
    return bytes;
}

/// A pixel as TGA stores it: BGR, or BGRA.
void append_pixel(Bytes& bytes, const Rgba& p, bool alpha)
{
    bytes.push_back(p[2]);
    bytes.push_back(p[1]);
    bytes.push_back(p[0]);
    if (alpha) {
        bytes.push_back(p[3]);
    }
}

/// The pixels of these rows (given top first), in the file's row order.
Bytes true_color_pixels(const std::vector<std::vector<Rgba>>& rows, bool alpha, bool top_origin)
{
    Bytes pixels;
    for (std::size_t r = 0; r < rows.size(); ++r) {
        for (const Rgba& p : rows[top_origin ? r : rows.size() - 1 - r]) {
            append_pixel(pixels, p, alpha);
        }
    }
    return pixels;
}

float unit(std::uint8_t value)
{
    return static_cast<float>(value) / 255.f;
}

RGB rgb(const Rgba& p)
{
    return RGB{unit(p[0]), unit(p[1]), unit(p[2])};
}

const std::vector<std::vector<Rgba>> ROWS{
    {Rgba{255, 0, 0, 255}, Rgba{0, 255, 0, 128}},
    {Rgba{0, 0, 255, 0}, Rgba{10, 20, 30, 40}},
};

const Rgba& pixel_at(int x, int y)
{
    return ROWS[static_cast<std::size_t>(y)][static_cast<std::size_t>(x)];
}

void check_rows(const ImageBundle<RGB>& bundle)
{
    REQUIRE(bundle.image.resolution() == Resolution{2, 2});
    for (int y = 0; y < 2; ++y) {
        for (int x = 0; x < 2; ++x) {
            CHECK(bundle.image(x, y) == rgb(pixel_at(x, y)));
        }
    }
}

void check_alpha(const ImageBundle<RGB>& bundle)
{
    REQUIRE(bundle.alpha.resolution() == Resolution{2, 2});
    for (int y = 0; y < 2; ++y) {
        for (int x = 0; x < 2; ++x) {
            CHECK(bundle.alpha(x, y) == unit(pixel_at(x, y)[3]));
        }
    }
}

/// A color map of the ROWS' colors (BGR, or BGRA), in reading order.
Bytes palette(bool alpha)
{
    Bytes map;
    for (int y = 0; y < 2; ++y) {
        for (int x = 0; x < 2; ++x) {
            append_pixel(map, pixel_at(x, y), alpha);
        }
    }
    return map;
}

fs::path write_temp(const std::string& name, const Bytes& bytes)
{
    const fs::path path = fs::temp_directory_path() / ("huira_test_tga_" + name);
    std::ofstream file(path, std::ios::binary);
    file.write(reinterpret_cast<const char*>(bytes.data()),
               static_cast<std::streamsize>(bytes.size()));
    return path;
}

} // namespace

TEST_CASE("A true-color TGA is read top row first, from bottom-up rows", "[images][tga]")
{
    const Bytes file = tga_file(TgaHeader{}, true_color_pixels(ROWS, false, false));
    const ImageBundle<RGB> bundle = read_image_tga(file.data(), file.size());
    check_rows(bundle);
    CHECK(bundle.color_space == ColorSpaceHint::sRGB);
    CHECK(bundle.alpha.empty());
}

TEST_CASE("A TGA with its origin at the top is read from top-down rows", "[images][tga]")
{
    TgaHeader header;
    header.descriptor = TOP_ORIGIN;
    const Bytes file = tga_file(header, true_color_pixels(ROWS, false, true));
    check_rows(read_image_tga(file.data(), file.size()));
}

TEST_CASE("A 32-bit TGA has an alpha channel, unless it isn't asked for", "[images][tga]")
{
    TgaHeader header;
    header.pixel_depth = 32;
    header.descriptor = TOP_ORIGIN | 8; // 8 bits of alpha
    const Bytes file = tga_file(header, true_color_pixels(ROWS, true, true));

    const ImageBundle<RGB> bundle = read_image_tga(file.data(), file.size());
    check_rows(bundle);
    check_alpha(bundle);
    CHECK(read_image_tga(file.data(), file.size(), false).alpha.empty());
}

TEST_CASE("A grayscale TGA is read as equal channels", "[images][tga]")
{
    TgaHeader header;
    header.image_type = GRAY;
    header.pixel_depth = 8;
    const Bytes file = tga_file(header, Bytes{0, 64, 128, 255}); // bottom row first

    const ImageBundle<RGB> bundle = read_image_tga(file.data(), file.size());
    CHECK(bundle.image(0, 0) == RGB{unit(128)});
    CHECK(bundle.image(1, 0) == RGB{unit(255)});
    CHECK(bundle.image(0, 1) == RGB{unit(0)});
    CHECK(bundle.image(1, 1) == RGB{unit(64)});
    CHECK(bundle.alpha.empty());

    const ImageBundle<float> mono = read_image_tga_mono(file.data(), file.size());
    CHECK(mono.image(1, 1) == unit(64));
    CHECK(mono.image(1, 0) == unit(255));
    CHECK(mono.alpha.empty());
}

TEST_CASE("A color-mapped TGA looks its pixels up in its color map", "[images][tga]")
{
    TgaHeader header;
    header.image_type = COLOR_MAPPED;
    header.pixel_depth = 8;
    header.descriptor = TOP_ORIGIN;
    header.color_map_type = 1;
    header.color_map_length = 4;

    SECTION("of 24-bit colors")
    {
        header.color_map_entry_bits = 24;
        const Bytes file = tga_file(header, Bytes{0, 1, 2, 3}, palette(false));
        const ImageBundle<RGB> bundle = read_image_tga(file.data(), file.size());
        check_rows(bundle);
        CHECK(bundle.alpha.empty());
    }
    SECTION("of 32-bit colors, with alpha")
    {
        header.color_map_entry_bits = 32;
        const Bytes file = tga_file(header, Bytes{0, 1, 2, 3}, palette(true));
        const ImageBundle<RGB> bundle = read_image_tga(file.data(), file.size());
        check_rows(bundle);
        check_alpha(bundle);
    }
    SECTION("with 16-bit indices, from a first index")
    {
        header.color_map_entry_bits = 24;
        header.pixel_depth = 16;
        header.color_map_first = 300;
        Bytes pixels;
        for (const std::uint16_t index : std::initializer_list<std::uint16_t>{300, 301, 302, 303}) {
            append_u16(pixels, index);
        }
        const Bytes file = tga_file(header, pixels, palette(false));
        check_rows(read_image_tga(file.data(), file.size()));
    }
}

TEST_CASE("Run-length encoded TGAs are decoded", "[images][tga]")
{
    SECTION("true color, in run and raw packets")
    {
        TgaHeader header;
        header.image_type = RLE_TRUE_COLOR;
        header.width = 3;
        // Rows (bottom first): a run of 2 blue, a raw packet of red and green, across the end
        // of the row, and a run of 2 green:
        Bytes pixels{0x81};
        append_pixel(pixels, Rgba{0, 0, 255, 0}, false);
        pixels.push_back(0x01);
        append_pixel(pixels, Rgba{255, 0, 0, 0}, false);
        append_pixel(pixels, Rgba{0, 255, 0, 0}, false);
        pixels.push_back(0x81);
        append_pixel(pixels, Rgba{0, 255, 0, 0}, false);
        const Bytes file = tga_file(header, pixels);

        const ImageBundle<RGB> bundle = read_image_tga(file.data(), file.size());
        const RGB red{1.f, 0.f, 0.f};
        const RGB green{0.f, 1.f, 0.f};
        const RGB blue{0.f, 0.f, 1.f};
        CHECK(bundle.image(0, 1) == blue);
        CHECK(bundle.image(1, 1) == blue);
        CHECK(bundle.image(2, 1) == red);
        CHECK(bundle.image(0, 0) == green);
        CHECK(bundle.image(1, 0) == green);
        CHECK(bundle.image(2, 0) == green);
    }
    SECTION("grayscale, with a run longer than the pixels left")
    {
        TgaHeader header;
        header.image_type = RLE_GRAY;
        header.pixel_depth = 8;
        header.descriptor = TOP_ORIGIN;
        const Bytes file = tga_file(header, Bytes{0x00, 17, 0x85, 34}); // 1 raw, then 6 of 34
        const ImageBundle<float> bundle = read_image_tga_mono(file.data(), file.size());
        CHECK(bundle.image(0, 0) == unit(17));
        CHECK(bundle.image(1, 0) == unit(34));
        CHECK(bundle.image(0, 1) == unit(34));
        CHECK(bundle.image(1, 1) == unit(34));
    }
    SECTION("color-mapped")
    {
        TgaHeader header;
        header.image_type = RLE_COLOR_MAPPED;
        header.pixel_depth = 8;
        header.descriptor = TOP_ORIGIN;
        header.color_map_type = 1;
        header.color_map_length = 4;
        header.color_map_entry_bits = 32;
        const Bytes file = tga_file(header, Bytes{0x01, 0, 1, 0x00, 2, 0x80, 3}, palette(true));
        const ImageBundle<RGB> bundle = read_image_tga(file.data(), file.size());
        check_rows(bundle);
        check_alpha(bundle);
    }
}

TEST_CASE("A TGA's image ID is skipped", "[images][tga]")
{
    const Bytes file = tga_file(TgaHeader{}, true_color_pixels(ROWS, false, false), {}, "Huira");
    check_rows(read_image_tga(file.data(), file.size()));
}

TEST_CASE("A TGA is read as mono by averaging its channels", "[images][tga]")
{
    TgaHeader header;
    header.pixel_depth = 32;
    const Bytes file = tga_file(header, true_color_pixels(ROWS, true, false));

    const ImageBundle<float> bundle = read_image_tga_mono(file.data(), file.size());
    CHECK(bundle.color_space == ColorSpaceHint::sRGB);
    for (int y = 0; y < 2; ++y) {
        for (int x = 0; x < 2; ++x) {
            const Rgba& p = pixel_at(x, y);
            CHECK(bundle.image(x, y) == (unit(p[0]) + unit(p[1]) + unit(p[2])) / 3.f);
            CHECK(bundle.alpha(x, y) == unit(p[3]));
        }
    }
    CHECK(read_image_tga_mono(file.data(), file.size(), false).alpha.empty());
}

TEST_CASE("TGA files are read from a path", "[images][tga]")
{
    TgaHeader header;
    header.pixel_depth = 32;
    const fs::path path =
        write_temp("file.tga", tga_file(header, true_color_pixels(ROWS, true, false)));
    const ImageBundle<RGB> bundle = read_image_tga(path);
    check_rows(bundle);
    check_alpha(bundle);
    CHECK(read_image_tga(path, false).alpha.empty());
    CHECK(read_image_tga_mono(path).alpha.width() == 2);
    CHECK(read_image_tga_mono(path, false).alpha.empty());
    fs::remove(path);

    CHECK_THROWS_AS(read_image_tga_mono(fs::temp_directory_path() / "huira_test_tga_missing.tga"),
                    std::runtime_error);
}

TEST_CASE("TGA data that is invalid or unsupported is rejected", "[images][tga]")
{
    const Bytes valid = tga_file(TgaHeader{}, true_color_pixels(ROWS, false, false));

    SECTION("too small for the header")
    {
        const Bytes file(valid.begin(), valid.begin() + 17);
        CHECK_THROWS_WITH(read_image_tga(file.data(), file.size()), ContainsSubstring("too small"));
    }
    SECTION("an unsupported image type")
    {
        for (const std::uint8_t type : std::initializer_list<std::uint8_t>{
                 0, 32, 33}) { // no image data, and Huffman compressed
            TgaHeader header;
            header.image_type = type;
            const Bytes file = tga_file(header, true_color_pixels(ROWS, false, false));
            CHECK_THROWS_WITH(read_image_tga(file.data(), file.size()),
                              ContainsSubstring("Unsupported TGA image type"));
        }
    }
    SECTION("an image ID longer than the data")
    {
        Bytes file(valid.begin(), valid.begin() + 18);
        file[0] = 200;
        CHECK_THROWS_WITH(read_image_tga(file.data(), file.size()),
                          ContainsSubstring("truncated in image ID"));
    }
    SECTION("a color map longer than the data")
    {
        TgaHeader header;
        header.image_type = COLOR_MAPPED;
        header.pixel_depth = 8;
        header.color_map_type = 1;
        header.color_map_length = 256;
        header.color_map_entry_bits = 24;
        const Bytes file = tga_file(header, Bytes{0, 1, 2, 3}, palette(false));
        CHECK_THROWS_WITH(read_image_tga(file.data(), file.size()),
                          ContainsSubstring("truncated in color map"));
    }
    SECTION("missing pixels")
    {
        const Bytes file(valid.begin(), valid.end() - 1);
        CHECK_THROWS_WITH(read_image_tga_mono(file.data(), file.size()),
                          ContainsSubstring("truncated in pixel data"));
    }
    SECTION("run-length data that ends early")
    {
        TgaHeader header;
        header.image_type = RLE_TRUE_COLOR;
        for (const Bytes& packets : {Bytes{}, Bytes{0x83, 1, 2}, Bytes{0x03, 1, 2, 3, 4, 5, 6}}) {
            const Bytes file = tga_file(header, packets); // no packet, a short run, a short raw
            CHECK_THROWS_WITH(read_image_tga(file.data(), file.size()),
                              ContainsSubstring("Unexpected end of RLE data"));
        }
    }
}
