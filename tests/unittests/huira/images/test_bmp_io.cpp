#include <array>
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
#include "huira/images/io/bmp_io.hpp"

using namespace huira;
using Catch::Matchers::ContainsSubstring;
namespace fs = std::filesystem;

namespace {

using Bytes = std::vector<unsigned char>;
using Rgba = std::array<std::uint8_t, 4>;

constexpr std::uint32_t BI_RGB = 0;
constexpr std::uint32_t BI_BITFIELDS = 3;
constexpr std::uint32_t BI_ALPHABITFIELDS = 6;

void put_u16(Bytes& bytes, std::size_t at, std::uint32_t value)
{
    bytes[at] = static_cast<unsigned char>(value & 0xFFu);
    bytes[at + 1] = static_cast<unsigned char>((value >> 8) & 0xFFu);
}

void put_u32(Bytes& bytes, std::size_t at, std::uint32_t value)
{
    put_u16(bytes, at, value & 0xFFFFu);
    put_u16(bytes, at + 2, value >> 16);
}

/// The pixels of a BMP with these rows (given top first), as BGR or BGRA, each row padded to 4
/// bytes, in the file's row order.
Bytes bmp_pixels(const std::vector<std::vector<Rgba>>& rows, int bits, bool bottom_up)
{
    const std::size_t bytes_per_pixel = static_cast<std::size_t>(bits) / 8;
    Bytes pixels;
    for (std::size_t r = 0; r < rows.size(); ++r) {
        const auto& row = rows[bottom_up ? rows.size() - 1 - r : r];
        for (const Rgba& p : row) {
            pixels.push_back(p[2]);
            pixels.push_back(p[1]);
            pixels.push_back(p[0]);
            if (bytes_per_pixel == 4) {
                pixels.push_back(p[3]);
            }
        }
        for (std::size_t i = row.size() * bytes_per_pixel; i % 4 != 0; ++i) {
            pixels.push_back(0xEE); // padding, which isn't read
        }
    }
    return pixels;
}

/// A BMP file: the 14-byte file header, a DIB header of dib_size bytes (a BITMAPINFOHEADER for
/// its first 40), and the pixels.
Bytes bmp_file(std::int32_t width,
               std::int32_t height,
               std::uint32_t bits,
               std::uint32_t compression,
               const Bytes& pixels,
               std::uint32_t dib_size = 40,
               const std::vector<std::uint32_t>& masks = {})
{
    // Color masks are in a DIB header of 52 bytes or more, and otherwise follow it:
    const bool masks_in_header = dib_size >= 52;
    Bytes bytes(14 + dib_size + (masks_in_header ? 0 : 4 * masks.size()), 0);
    for (std::size_t i = 0; i < masks.size(); ++i) {
        put_u32(bytes, (masks_in_header ? 14 + 40 : 14 + dib_size) + 4 * i, masks[i]);
    }
    bytes[0] = 'B';
    bytes[1] = 'M';
    put_u32(bytes, 2, static_cast<std::uint32_t>(bytes.size() + pixels.size()));
    put_u32(bytes, 10, static_cast<std::uint32_t>(bytes.size())); // pixel offset
    put_u32(bytes, 14, dib_size);
    put_u32(bytes, 18, static_cast<std::uint32_t>(width));
    put_u32(bytes, 22, static_cast<std::uint32_t>(height));
    put_u16(bytes, 26, 1); // planes
    put_u16(bytes, 28, bits);
    put_u32(bytes, 30, compression);
    put_u32(bytes, 34, static_cast<std::uint32_t>(pixels.size()));
    bytes.insert(bytes.end(), pixels.begin(), pixels.end());
    return bytes;
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
    {Rgba{255, 0, 0, 255}, Rgba{0, 255, 0, 128}, Rgba{0, 0, 255, 0}},
    {Rgba{255, 255, 255, 64}, Rgba{0, 0, 0, 255}, Rgba{128, 64, 32, 1}},
};

void check_rows(const ImageBundle<RGB>& bundle)
{
    REQUIRE(bundle.image.width() == 3);
    REQUIRE(bundle.image.height() == 2);
    for (int y = 0; y < 2; ++y) {
        for (int x = 0; x < 3; ++x) {
            CHECK(bundle.image(x, y) ==
                  rgb(ROWS[static_cast<std::size_t>(y)][static_cast<std::size_t>(x)]));
        }
    }
}

fs::path write_temp(const std::string& name, const Bytes& bytes)
{
    const fs::path path = fs::temp_directory_path() / ("huira_test_bmp_" + name);
    std::ofstream file(path, std::ios::binary);
    file.write(reinterpret_cast<const char*>(bytes.data()),
               static_cast<std::streamsize>(bytes.size()));
    return path;
}

} // namespace

TEST_CASE("A 24-bit BMP is read top row first, from bottom-up rows", "[images][bmp]")
{
    const Bytes file = bmp_file(3, 2, 24, 0, bmp_pixels(ROWS, 24, true));
    const ImageBundle<RGB> bundle = read_image_bmp(file.data(), file.size());
    check_rows(bundle);
    CHECK(bundle.color_space == ColorSpaceHint::sRGB);
    CHECK(bundle.alpha.empty());
}

TEST_CASE("A BMP with a negative height is read from top-down rows", "[images][bmp]")
{
    const Bytes file = bmp_file(3, -2, 24, 0, bmp_pixels(ROWS, 24, false));
    check_rows(read_image_bmp(file.data(), file.size()));
}

TEST_CASE("A 32-bit BMP has an alpha channel, unless it isn't asked for", "[images][bmp]")
{
    // BGRA, in the order a little-endian 0xAARRGGBB stores it:
    const std::vector<std::uint32_t> bgra{0x00FF0000u, 0x0000FF00u, 0x000000FFu, 0xFF000000u};
    const Bytes pixels = bmp_pixels(ROWS, 32, true);
    const std::vector<Bytes> files{
        bmp_file(3, 2, 32, BI_RGB, pixels),                      // no masks
        bmp_file(3, 2, 32, BI_BITFIELDS, pixels, 108, bgra),     // a BITMAPV4HEADER's masks
        bmp_file(3, 2, 32, BI_ALPHABITFIELDS, pixels, 40, bgra), // masks after the header
    };
    for (const Bytes& file : files) {

        const ImageBundle<RGB> bundle = read_image_bmp(file.data(), file.size());
        check_rows(bundle);
        REQUIRE(bundle.alpha.resolution() == Resolution{3, 2});
        for (int y = 0; y < 2; ++y) {
            for (int x = 0; x < 3; ++x) {
                CHECK(bundle.alpha(x, y) ==
                      unit(ROWS[static_cast<std::size_t>(y)][static_cast<std::size_t>(x)][3]));
            }
        }

        const ImageBundle<RGB> no_alpha = read_image_bmp(file.data(), file.size(), false);
        check_rows(no_alpha);
        CHECK(no_alpha.alpha.empty());
    }
}

TEST_CASE("A 32-bit BMP without masks has alpha only if some pixel sets it", "[images][bmp]")
{
    // BI_RGB leaves the fourth byte unused, and many programs leave it 0 though the image is
    // opaque. It was read as alpha anyway, so such a BMP was entirely transparent.
    std::vector<std::vector<Rgba>> opaque = ROWS;
    for (auto& row : opaque) {
        for (Rgba& p : row) {
            p[3] = 0;
        }
    }
    const Bytes file = bmp_file(3, 2, 32, BI_RGB, bmp_pixels(opaque, 32, true));
    const ImageBundle<RGB> bundle = read_image_bmp(file.data(), file.size());
    check_rows(bundle);
    CHECK(bundle.alpha.empty());
    CHECK(read_image_bmp_mono(file.data(), file.size()).alpha.empty());
}

TEST_CASE("A 32-bit BMP's color masks say where each channel is", "[images][bmp]")
{
    // The masks were ignored, and every pixel read as B, G, R and alpha bytes.
    SECTION("RGBA bytes")
    {
        Bytes pixels;
        for (auto row = ROWS.rbegin(); row != ROWS.rend(); ++row) {
            for (const Rgba& p : *row) {
                pixels.insert(pixels.end(), p.begin(), p.end());
            }
        }
        const std::vector<std::uint32_t> rgba{0x000000FFu, 0x0000FF00u, 0x00FF0000u, 0xFF000000u};
        const Bytes file = bmp_file(3, 2, 32, BI_BITFIELDS, pixels, 124, rgba);
        const ImageBundle<RGB> bundle = read_image_bmp(file.data(), file.size());
        check_rows(bundle);
        CHECK(bundle.alpha(1, 0) == unit(128));
    }
    SECTION("10 bits of each color, and 2 of alpha")
    {
        const std::vector<std::uint32_t> masks{0x3FF00000u, 0x000FFC00u, 0x000003FFu, 0xC0000000u};
        Bytes pixels(4, 0);
        put_u32(pixels, 0, (2u << 30) | (1023u << 20) | (0u << 10) | 512u);
        const Bytes file = bmp_file(1, 1, 32, BI_BITFIELDS, pixels, 56, masks);
        const ImageBundle<RGB> bundle = read_image_bmp(file.data(), file.size());
        CHECK(bundle.image(0, 0) == RGB{1.f, 0.f, unit(128)}); // 512 / 1023, to 8 bits
        CHECK(bundle.alpha(0, 0) == unit(170));                // 2 / 3
    }
    SECTION("no alpha mask")
    {
        const std::vector<std::uint32_t> rgb{0x00FF0000u, 0x0000FF00u, 0x000000FFu};
        const Bytes file = bmp_file(3, 2, 32, BI_BITFIELDS, bmp_pixels(ROWS, 32, true), 40, rgb);
        const ImageBundle<RGB> bundle = read_image_bmp(file.data(), file.size());
        check_rows(bundle);
        CHECK(bundle.alpha.empty());
    }
    SECTION("masks the data ends before")
    {
        Bytes file = bmp_file(1, 1, 32, BI_BITFIELDS, Bytes{});
        CHECK_THROWS_WITH(read_image_bmp(file.data(), file.size()),
                          ContainsSubstring("truncated in color masks"));
    }
}

TEST_CASE("A BMP is read as mono by averaging its channels", "[images][bmp]")
{
    const Bytes file = bmp_file(3, 2, 32, 0, bmp_pixels(ROWS, 32, true));
    const ImageBundle<float> bundle = read_image_bmp_mono(file.data(), file.size());
    REQUIRE(bundle.image.resolution() == Resolution{3, 2});
    CHECK(bundle.color_space == ColorSpaceHint::sRGB);
    for (int y = 0; y < 2; ++y) {
        for (int x = 0; x < 3; ++x) {
            const Rgba& p = ROWS[static_cast<std::size_t>(y)][static_cast<std::size_t>(x)];
            CHECK(bundle.image(x, y) == (unit(p[0]) + unit(p[1]) + unit(p[2])) / 3.f);
            CHECK(bundle.alpha(x, y) == unit(p[3]));
        }
    }
    CHECK(read_image_bmp_mono(file.data(), file.size(), false).alpha.empty());

    const Bytes opaque = bmp_file(3, 2, 24, 0, bmp_pixels(ROWS, 24, true));
    CHECK(read_image_bmp_mono(opaque.data(), opaque.size()).alpha.empty());
}

TEST_CASE("A BMP with a larger DIB header is read from its pixel offset", "[images][bmp]")
{
    // A BITMAPV5HEADER (124 bytes), as most programs write today:
    const Bytes file = bmp_file(3, 2, 24, 0, bmp_pixels(ROWS, 24, true), 124);
    check_rows(read_image_bmp(file.data(), file.size()));
}

TEST_CASE("BMP files are read from a path", "[images][bmp]")
{
    const fs::path path = write_temp("file.bmp", bmp_file(3, 2, 32, 0, bmp_pixels(ROWS, 32, true)));
    check_rows(read_image_bmp(path));
    CHECK(read_image_bmp(path, false).alpha.empty());
    CHECK(read_image_bmp_mono(path).alpha.width() == 3);
    CHECK(read_image_bmp_mono(path, false).alpha.empty());
    fs::remove(path);

    CHECK_THROWS_AS(read_image_bmp(fs::temp_directory_path() / "huira_test_bmp_missing.bmp"),
                    std::runtime_error);
}

TEST_CASE("BMP data that is invalid or unsupported is rejected", "[images][bmp]")
{
    const Bytes valid = bmp_file(3, 2, 24, 0, bmp_pixels(ROWS, 24, true));

    SECTION("too small for the headers")
    {
        const Bytes file(valid.begin(), valid.begin() + 53);
        CHECK_THROWS_WITH(read_image_bmp(file.data(), file.size()), ContainsSubstring("too small"));
    }
    SECTION("without the BM signature")
    {
        Bytes file = valid;
        file[1] = 'A';
        CHECK_THROWS_WITH(read_image_bmp(file.data(), file.size()),
                          ContainsSubstring("bad signature"));
    }
    SECTION("an OS/2 header")
    {
        Bytes file = valid;
        put_u32(file, 14, 12);
        CHECK_THROWS_WITH(read_image_bmp(file.data(), file.size()),
                          ContainsSubstring("Unsupported BMP DIB header"));
    }
    SECTION("a DIB header longer than the data")
    {
        Bytes file = valid;
        put_u32(file, 14, 124);
        CHECK_THROWS_WITH(read_image_bmp(file.data(), file.size()),
                          ContainsSubstring("truncated in DIB header"));
    }
    SECTION("run-length compression")
    {
        Bytes file = valid;
        put_u32(file, 30, 1); // BI_RLE8
        CHECK_THROWS_WITH(read_image_bmp(file.data(), file.size()),
                          ContainsSubstring("Unsupported BMP compression"));
    }
    SECTION("a palette")
    {
        Bytes file = valid;
        put_u16(file, 28, 8);
        CHECK_THROWS_WITH(read_image_bmp_mono(file.data(), file.size()),
                          ContainsSubstring("Unsupported BMP bit depth"));
    }
    SECTION("missing pixels")
    {
        const Bytes file(valid.begin(), valid.end() - 1);
        CHECK_THROWS_WITH(read_image_bmp(file.data(), file.size()),
                          ContainsSubstring("truncated in pixel data"));
    }
    SECTION("dimensions far larger than the data")
    {
        Bytes file = valid;
        put_u32(file, 18, 1u << 20);
        put_u32(file, 22, 1u << 20);
        CHECK_THROWS_WITH(read_image_bmp(file.data(), file.size()),
                          ContainsSubstring("truncated in pixel data"));
    }
    SECTION("a negative width")
    {
        Bytes file = valid;
        put_u32(file, 18, static_cast<std::uint32_t>(-3));
        CHECK_THROWS(read_image_bmp(file.data(), file.size()));
    }
}
