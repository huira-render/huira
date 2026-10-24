#include <array>
#include <cmath>
#include <csetjmp>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

#include <png.h>

#include "catch2/catch_test_macros.hpp"
#include "catch2/matchers/catch_matchers_string.hpp"
#include "huira/core/spectral_bins.hpp"
#include "huira/images/image.hpp"
#include "huira/images/io/png_io.hpp"
#include "huira/util/logger.hpp"

#ifdef _MSC_VER
#pragma warning(disable : 4611) // setjmp in encode_png(), which has no objects to destroy
#endif

using namespace huira;
using Catch::Matchers::ContainsSubstring;
namespace fs = std::filesystem;

namespace {

using Bytes = std::vector<unsigned char>;

fs::path temp_path(const std::string& name)
{
    return fs::temp_directory_path() / ("huira_test_png_" + name);
}

/// A value as a file of max + 1 levels holds it, read back: round(v max) / max.
float quantized(float value, double max)
{
    return static_cast<float>(std::round(static_cast<double>(value) * max) / max);
}

/// A 3 x 2 color image, and an alpha channel for it.
Image<RGB> colors()
{
    Image<RGB> image(3, 2);
    image(0, 0) = RGB{0.f, 0.25f, 0.5f};
    image(1, 0) = RGB{1.f, 0.1f, 0.9f};
    image(2, 0) = RGB{0.3f, 0.6f, 0.7f};
    image(0, 1) = RGB{0.05f, 0.95f, 0.45f};
    image(1, 1) = RGB{0.5f, 0.5f, 0.5f};
    image(2, 1) = RGB{0.8f, 0.2f, 0.f};
    return image;
}

Image<float> alpha()
{
    Image<float> image(3, 2, 1.f);
    image(0, 0) = 0.f;
    image(1, 0) = 0.5f;
    image(2, 1) = 0.25f;
    return image;
}

/// What libpng is given to write a PNG, for the variants write_image_png() doesn't write.
struct PngSpec {
    png_uint_32 width = 2;
    png_uint_32 height = 1;
    int bit_depth = 8;
    int color_type = PNG_COLOR_TYPE_RGB;
    std::vector<png_color> palette;
    std::vector<png_byte> palette_alpha;
    std::optional<png_color_16> transparent;
    std::optional<png_color_8> significant_bits;
    std::optional<double> gamma;
    Bytes icc_profile;
    std::vector<Bytes> rows; // as PNG stores them: packed, 16-bit samples big-endian
};

void append_png_data(png_structp png, png_bytep data, png_size_t length)
{
    auto* out = static_cast<Bytes*>(png_get_io_ptr(png));
    out->insert(out->end(), data, data + length);
}

void flush_png_data(png_structp /*png*/) {}

// libpng takes C callbacks (and setjmp's longjmp), which MSVC warns may throw (C5039):
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 5039)
#endif

/// Encodes the PNG spec describes into out, with libpng; false if libpng fails.
bool encode_png(const PngSpec& spec, Bytes& out)
{
    png_structp png = png_create_write_struct(PNG_LIBPNG_VER_STRING, nullptr, nullptr, nullptr);
    png_infop info = png_create_info_struct(png);
    if (setjmp(png_jmpbuf(png))) {
        png_destroy_write_struct(&png, &info);
        return false;
    }
    png_set_write_fn(png, &out, append_png_data, flush_png_data);
    png_set_IHDR(png,
                 info,
                 spec.width,
                 spec.height,
                 spec.bit_depth,
                 spec.color_type,
                 PNG_INTERLACE_NONE,
                 PNG_COMPRESSION_TYPE_DEFAULT,
                 PNG_FILTER_TYPE_DEFAULT);
    if (!spec.palette.empty()) {
        png_set_PLTE(png, info, spec.palette.data(), static_cast<int>(spec.palette.size()));
    }
    if (!spec.palette_alpha.empty() || spec.transparent) {
        png_set_tRNS(png,
                     info,
                     spec.palette_alpha.empty() ? nullptr : spec.palette_alpha.data(),
                     static_cast<int>(spec.palette_alpha.size()),
                     spec.transparent ? &*spec.transparent : nullptr);
    }
    if (spec.significant_bits) {
        png_set_sBIT(png, info, &*spec.significant_bits);
    }
    if (spec.gamma) {
        png_set_gAMA(png, info, *spec.gamma);
    }
    if (!spec.icc_profile.empty()) {
        png_set_iCCP(png,
                     info,
                     "Huira test",
                     PNG_COMPRESSION_TYPE_BASE,
                     spec.icc_profile.data(),
                     static_cast<png_uint_32>(spec.icc_profile.size()));
    }
    png_write_info(png, info);
    for (const Bytes& row : spec.rows) {
        png_write_row(png, row.data());
    }
    png_write_end(png, nullptr);
    png_destroy_write_struct(&png, &info);
    return true;
}

#ifdef _MSC_VER
#pragma warning(pop)
#endif

Bytes encode_png(const PngSpec& spec)
{
    Bytes out;
    REQUIRE(encode_png(spec, out));
    return out;
}

/// An ICC profile that libpng accepts: a v2 RGB display profile's header, no tags, and data
/// that doesn't compress, since libpng ignores an iCCP chunk of fewer than about 90 bytes.
Bytes rgb_icc_profile()
{
    constexpr std::uint32_t SIZE = 132 + 256;
    Bytes profile(SIZE, 0);
    const auto put = [&](std::size_t at, std::uint32_t value) {
        for (std::size_t i = 0; i < 4; ++i) {
            profile[at + i] = static_cast<unsigned char>(value >> (24 - 8 * i));
        }
    };
    const auto put_tag = [&](std::size_t at, const char* tag) {
        for (std::size_t i = 0; i < 4; ++i) {
            profile[at + i] = static_cast<unsigned char>(tag[i]);
        }
    };
    put(0, SIZE);
    put(8, 0x02100000);  // version 2.1
    put_tag(12, "mntr"); // a display
    put_tag(16, "RGB ");
    put_tag(20, "XYZ ");
    put_tag(36, "acsp");
    put(68, 0x0000F6D6); // the D50 illuminant
    put(72, 0x00010000);
    put(76, 0x0000D32D);
    std::uint32_t state = 1;
    for (std::size_t i = 132; i < profile.size(); ++i) {
        state = state * 1664525u + 1013904223u;
        profile[i] = static_cast<unsigned char>(state >> 24);
    }
    return profile;
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

} // namespace

TEST_CASE("PNG files keep RGB images and their alpha, at 8 and 16 bits", "[images][png]")
{
    for (const int bits : {8, 16}) {
        const double max = bits == 8 ? 255.0 : 65535.0;
        ImageBundle<RGB> bundle(colors(), alpha());
        bundle.bit_depth = bits;

        const fs::path path = temp_path("rgba.png");
        write_image_png(path, bundle);

        const ImageBundle<RGB> read = read_image_png(path);
        CHECK(read.bit_depth == bits);
        CHECK(read.image.sensor_bit_depth() == 0);
        REQUIRE(read.image.resolution() == Resolution{3, 2});
        REQUIRE(read.alpha.resolution() == Resolution{3, 2});
        for (int y = 0; y < 2; ++y) {
            for (int x = 0; x < 3; ++x) {
                for (std::size_t c = 0; c < 3; ++c) {
                    CHECK(read.image(x, y)[c] == quantized(bundle.image(x, y)[c], max));
                }
                CHECK(read.alpha(x, y) == quantized(bundle.alpha(x, y), max));
            }
        }
        CHECK(read_image_png(path, false).alpha.empty());

        // Read as mono, the channels are averaged:
        const ImageBundle<float> mono = read_image_png_mono(path);
        for (int y = 0; y < 2; ++y) {
            for (int x = 0; x < 3; ++x) {
                const RGB& p = read.image(x, y);
                CHECK(mono.image(x, y) == (p[0] + p[1] + p[2]) / 3.f);
                CHECK(mono.alpha(x, y) == read.alpha(x, y));
            }
        }
        CHECK(read_image_png_mono(path, false).alpha.empty());
        fs::remove(path);
    }
}

TEST_CASE("PNG files keep gray images and their alpha, at 8 and 16 bits", "[images][png]")
{
    for (const int bits : {8, 16}) {
        const double max = bits == 8 ? 255.0 : 65535.0;
        ImageBundle<float> bundle(colors().get_channel(1), alpha());
        bundle.bit_depth = bits;

        const fs::path path = temp_path("gray_alpha.png");
        write_image_png(path, bundle);

        const ImageBundle<float> read = read_image_png_mono(path);
        CHECK(read.bit_depth == bits);
        for (int y = 0; y < 2; ++y) {
            for (int x = 0; x < 3; ++x) {
                CHECK(read.image(x, y) == quantized(bundle.image(x, y), max));
                CHECK(read.alpha(x, y) == quantized(bundle.alpha(x, y), max));
            }
        }

        // Read as RGB, the channels are equal:
        const ImageBundle<RGB> rgb = read_image_png(path);
        CHECK(rgb.image(1, 0) == RGB{read.image(1, 0)});
        CHECK(rgb.alpha(1, 0) == read.alpha(1, 0));
        fs::remove(path);

        // Without alpha:
        write_image_png(path, ImageBundle<float>(bundle.image));
        CHECK(read_image_png_mono(path).alpha.empty());
        CHECK(read_image_png(path).image(2, 1) == RGB{read.image(2, 1)});
        fs::remove(path);
    }
}

TEST_CASE("A sensor response is written to an RGB PNG as its DN", "[images][png]")
{
    // A 10-bit sensor in 16 bits, with alpha:
    Image<RGB> image(2, 1);
    image(0, 0) = RGB{0.f, 1.f / 1023.f, 512.f / 1023.f};
    image(1, 0) = RGB{1.f, 1000.f / 1023.f, 3.f / 1023.f};
    image.set_sensor_bit_depth(10);
    ImageBundle<RGB> bundle(image, Image<float>(2, 1, 0.5f));
    bundle.bit_depth = 16;

    const fs::path path = temp_path("rgb_dn.png");
    write_image_png(path, bundle);
    const ImageBundle<RGB> read = read_image_png(path);
    CHECK(read.image.sensor_bit_depth() == 10);
    CHECK(read.color_space == ColorSpaceHint::Linear);
    for (int x = 0; x < 2; ++x) {
        CHECK(read.image(x, 0) == image(x, 0));
    }
    CHECK(read.alpha(0, 0) == quantized(0.5f, 65535.0)); // alpha isn't DN
    fs::remove(path);
}

TEST_CASE("sBIT is read as a sensor bit depth only when it is the same for every color",
          "[images][png]")
{
    PngSpec spec;
    spec.rows = {Bytes{0xFF, 0x80, 0x04, 0x00, 0x08, 0xFF}};

    // RGB 565, as some displays have, is not a sensor's bit depth:
    spec.significant_bits = png_color_8{5, 6, 5, 0, 0};
    const Bytes rgb565 = encode_png(spec);
    const ImageBundle<RGB> read = read_image_png(rgb565.data(), rgb565.size());
    CHECK(read.image.sensor_bit_depth() == 0);
    CHECK(read.image(0, 0) ==
          RGB{1.f, static_cast<float>(128.0 / 255.0), static_cast<float>(4.0 / 255.0)});

    // Six bits of gray, in eight:
    PngSpec gray;
    gray.color_type = PNG_COLOR_TYPE_GRAY;
    gray.significant_bits = png_color_8{0, 0, 0, 6, 0};
    gray.rows = {Bytes{0xFF, 0x84}}; // 63 and 33, scaled up
    const Bytes six_bits = encode_png(gray);
    const ImageBundle<float> mono = read_image_png_mono(six_bits.data(), six_bits.size());
    CHECK(mono.image.sensor_bit_depth() == 6);
    CHECK(mono.image(0, 0) == 1.f);
    CHECK(mono.image(1, 0) == static_cast<float>(33.0 / 63.0));
}

TEST_CASE("PNG files are labelled with the encoding of their values", "[images][png]")
{
    const fs::path path = temp_path("encoding.png");
    ImageBundle<float> bundle(Image<float>(2, 2, 0.5f));

    bundle.color_space = ColorSpaceHint::sRGB;
    write_image_png(path, bundle);
    CHECK(read_image_png_mono(path).color_space == ColorSpaceHint::sRGB);

    bundle.color_space = ColorSpaceHint::Linear;
    write_image_png(path, bundle);
    CHECK(read_image_png_mono(path).color_space == ColorSpaceHint::Linear);

    bundle.color_space = ColorSpaceHint::Gamma;
    bundle.gamma_value = 1.8f;
    write_image_png(path, bundle);
    const ImageBundle<float> gamma = read_image_png_mono(path);
    CHECK(gamma.color_space == ColorSpaceHint::Gamma);
    CHECK(std::abs(gamma.gamma_value - 1.8f) < 1e-4f);

    // A gamma of 2.2 is taken for sRGB, as is a file with no label:
    bundle.gamma_value = 2.2f;
    write_image_png(path, bundle);
    CHECK(read_image_png_mono(path).color_space == ColorSpaceHint::sRGB);

    bundle.color_space = ColorSpaceHint::Unknown;
    write_image_png(path, bundle);
    CHECK(read_image_png_mono(path).color_space == ColorSpaceHint::sRGB);
    fs::remove(path);

    // An ICC profile can't be described by a hint:
    PngSpec spec;
    spec.icc_profile = rgb_icc_profile();
    spec.rows = {Bytes(6, 0x80)};
    const Bytes icc = encode_png(spec);
    CHECK(read_image_png(icc.data(), icc.size()).color_space == ColorSpaceHint::Unknown);

    ImageBundle<float> unknown;
    read_color_space(PngColorInfo{PngColorSpace::UNKNOWN, 1.0}, unknown);
    CHECK(unknown.color_space == ColorSpaceHint::Unknown);
}

TEST_CASE("Palette PNGs are expanded to RGB, with their transparency as alpha", "[images][png]")
{
    PngSpec spec;
    spec.color_type = PNG_COLOR_TYPE_PALETTE;
    spec.width = 3;
    spec.palette = {png_color{255, 0, 0}, png_color{0, 255, 0}, png_color{0, 0, 255}};

    SECTION("8-bit indices, without transparency")
    {
        spec.rows = {Bytes{2, 0, 1}};
        const Bytes file = encode_png(spec);
        const ImageBundle<RGB> read = read_image_png(file.data(), file.size());
        CHECK(read.image(0, 0) == RGB{0.f, 0.f, 1.f});
        CHECK(read.image(1, 0) == RGB{1.f, 0.f, 0.f});
        CHECK(read.image(2, 0) == RGB{0.f, 1.f, 0.f});
        CHECK(read.alpha.empty());
        CHECK(read.bit_depth == 8);
    }
    SECTION("2-bit indices, with transparency")
    {
        spec.bit_depth = 2;
        spec.palette_alpha = {255, 0, 51};
        spec.rows = {Bytes{0b10'00'01'00}}; // 2, 0, 1, and padding
        const Bytes file = encode_png(spec);
        const ImageBundle<RGB> read = read_image_png(file.data(), file.size());
        CHECK(read.image(0, 0) == RGB{0.f, 0.f, 1.f});
        CHECK(read.image(1, 0) == RGB{1.f, 0.f, 0.f});
        CHECK(read.alpha(0, 0) == 0.2f);
        CHECK(read.alpha(1, 0) == 1.f);
        CHECK(read.alpha(2, 0) == 0.f);

        const ImageBundle<float> mono = read_image_png_mono(file.data(), file.size());
        CHECK(mono.image(0, 0) == 1.f / 3.f);
        CHECK(mono.alpha(2, 0) == 0.f);
    }
}

TEST_CASE("Gray PNGs of fewer than 8 bits are expanded", "[images][png]")
{
    PngSpec spec;
    spec.color_type = PNG_COLOR_TYPE_GRAY;
    spec.width = 2;

    spec.bit_depth = 1;
    spec.rows = {Bytes{0b01'000000}};
    Bytes file = encode_png(spec);
    ImageBundle<float> read = read_image_png_mono(file.data(), file.size());
    CHECK(read.image(0, 0) == 0.f);
    CHECK(read.image(1, 0) == 1.f);

    spec.bit_depth = 4;
    spec.rows = {Bytes{0xF8}};
    file = encode_png(spec);
    read = read_image_png_mono(file.data(), file.size());
    CHECK(read.image(0, 0) == 1.f);
    CHECK(read.image(1, 0) == static_cast<float>(0x88 / 255.0));
}

TEST_CASE("A PNG's transparent color is read as alpha", "[images][png]")
{
    PngSpec gray;
    gray.color_type = PNG_COLOR_TYPE_GRAY;
    gray.transparent = png_color_16{0, 0, 0, 0, 7};
    gray.rows = {Bytes{7, 200}};
    Bytes file = encode_png(gray);
    const ImageBundle<float> mono = read_image_png_mono(file.data(), file.size());
    CHECK(mono.alpha(0, 0) == 0.f);
    CHECK(mono.alpha(1, 0) == 1.f);
    CHECK(mono.image(1, 0) == static_cast<float>(200.0 / 255.0));

    PngSpec rgb;
    rgb.bit_depth = 16;
    rgb.transparent = png_color_16{0, 1, 2, 3, 0};
    rgb.rows = {Bytes{0, 1, 0, 2, 0, 3, 0xFF, 0xFF, 0, 0, 0, 0}};
    file = encode_png(rgb);
    const ImageBundle<RGB> color = read_image_png(file.data(), file.size());
    CHECK(color.alpha(0, 0) == 0.f);
    CHECK(color.alpha(1, 0) == 1.f);
    CHECK(color.image(1, 0) == RGB{1.f, 0.f, 0.f});
}

TEST_CASE("PNG writers create the file's directory, and reject what they cannot write",
          "[images][png]")
{
    const fs::path directory = temp_path("new_directory");
    fs::remove_all(directory);
    write_image_png(directory / "image.png", ImageBundle<float>(Image<float>(1, 1, 1.f)));
    CHECK(read_image_png_mono(directory / "image.png").image(0, 0) == 1.f);
    fs::remove_all(directory);

    CHECK_THROWS_WITH(write_image_png(temp_path("empty.png"), ImageBundle<float>{}),
                      ContainsSubstring("Cannot write empty image"));
    CHECK_THROWS_WITH(write_image_png(temp_path("empty.png"), ImageBundle<RGB>{}),
                      ContainsSubstring("Cannot write empty image"));

    ImageBundle<RGB> twelve_bits(colors());
    twelve_bits.bit_depth = 12;
    CHECK_THROWS_WITH(write_image_png(temp_path("twelve.png"), twelve_bits),
                      ContainsSubstring("bit_depth must be 8 or 16"));

    // A directory can't be opened as a file:
    const fs::path not_a_file = temp_path("directory.png");
    fs::create_directories(not_a_file);
    CHECK_THROWS_WITH(write_image_png(not_a_file, ImageBundle<RGB>(colors())),
                      ContainsSubstring("Failed to open PNG file"));
    fs::remove_all(not_a_file);
}

TEST_CASE("An alpha channel that doesn't match the image is not written", "[images][png]")
{
    const fs::path path = temp_path("mismatched_alpha.png");
    WarningLog log;
    write_image_png(path, ImageBundle<RGB>(colors(), Image<float>(2, 2, 0.5f)));
    REQUIRE(log.messages.size() == 1);
    CHECK_THAT(log.messages[0], ContainsSubstring("The alpha mask will be ignored"));
    CHECK(read_image_png(path).alpha.empty());
    fs::remove(path);
}

TEST_CASE("A damaged ancillary chunk is skipped, with libpng's warning logged", "[images][png]")
{
    const fs::path path = temp_path("damaged_chunk.png");
    write_image_png(path, ImageBundle<RGB>(colors()));
    Bytes file = read_file_to_buffer(path);
    fs::remove(path);

    // A tEXt chunk with the wrong CRC, after the signature (8 bytes) and the IHDR chunk (25):
    const std::string text("Comment\0damaged", 15);
    Bytes chunk{0, 0, 0, static_cast<unsigned char>(text.size()), 't', 'E', 'X', 't'};
    chunk.insert(chunk.end(), text.begin(), text.end());
    chunk.insert(chunk.end(), {0, 0, 0, 0});
    file.insert(file.begin() + 33, chunk.begin(), chunk.end());

    std::vector<std::string> messages;
    const LogLevel level = Logger::get_level();
    Logger::set_level(LogLevel::Debug);
    Logger::set_custom_sink([&](const LogEntry& entry) { messages.push_back(entry.message); });
    const ImageBundle<RGB> read = read_image_png(file.data(), file.size());
    Logger::clear_custom_sink();
    Logger::set_level(level);

    CHECK(read.image(1, 0)[2] == quantized(0.9f, 255.0));
    bool warned = false;
    for (const std::string& message : messages) {
        warned = warned || message.find("libpng warning") != std::string::npos;
    }
    CHECK(warned);
}

TEST_CASE("PNG data that is invalid is rejected", "[images][png]")
{
    ImageBundle<RGB> bundle(colors());
    const fs::path path = temp_path("valid.png");
    write_image_png(path, bundle);
    const Bytes valid = read_file_to_buffer(path);
    fs::remove(path);

    const Bytes too_small(valid.begin(), valid.begin() + 7);
    CHECK_THROWS_WITH(read_image_png(too_small.data(), too_small.size()),
                      ContainsSubstring("too small"));

    Bytes bad_signature = valid;
    bad_signature[1] = 'Q';
    CHECK_THROWS_WITH(read_image_png_mono(bad_signature.data(), bad_signature.size()),
                      ContainsSubstring("bad signature"));

    // libpng's errors (here, data that ends early) are thrown:
    for (const std::size_t size : {std::size_t{20}, valid.size() - 20}) {
        const Bytes truncated(valid.begin(), valid.begin() + static_cast<std::ptrdiff_t>(size));
        CHECK_THROWS_WITH(read_image_png(truncated.data(), truncated.size()),
                          ContainsSubstring("Error during PNG read"));
    }

    CHECK_THROWS_AS(read_image_png(temp_path("missing.png")), std::runtime_error);
}
