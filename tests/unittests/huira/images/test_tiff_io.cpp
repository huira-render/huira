#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <initializer_list>
#include <stdexcept>
#include <string>
#include <vector>

#include "catch2/catch_test_macros.hpp"
#include "catch2/matchers/catch_matchers_string.hpp"
#include "huira/core/spectral_bins.hpp"
#include "huira/images/image.hpp"
#include "huira/images/io/tiff_io.hpp"
#include "huira/util/logger.hpp"
#include "tiffio.h"

using namespace huira;
using Catch::Matchers::ContainsSubstring;
namespace fs = std::filesystem;

namespace {

using Bytes = std::vector<unsigned char>;

fs::path temp_path(const std::string& name)
{
    return fs::temp_directory_path() / ("huira_test_tiff_" + name);
}

/// A value as a file of max + 1 levels holds it, read back: round(v max) / max.
float quantized(float value, double max)
{
    return static_cast<float>(std::round(static_cast<double>(value) * max) / max);
}

/// What libtiff is given to write a TIFF, for the variants write_image_tiff() doesn't write.
struct TiffSpec {
    std::uint32_t width = 2;
    std::uint32_t height = 2;
    std::uint16_t samples_per_pixel = 1;
    std::uint16_t bits_per_sample = 8;
    std::uint16_t sample_format = SAMPLEFORMAT_UINT;
    std::uint16_t photometric = PHOTOMETRIC_MINISBLACK;
    std::uint16_t planar_config = PLANARCONFIG_CONTIG;
    std::vector<std::uint16_t> extra_samples;
    std::uint16_t max_sample_value = 0;   // not written if 0
    std::uint32_t tile_size = 0;          // strips if 0
    std::vector<std::uint16_t> color_map; // red, then green, then blue
};

/// A sample's bytes, of the spec's type.
void append_sample(Bytes& bytes, const TiffSpec& spec, double value)
{
    unsigned char buffer[8] = {};
    std::size_t size = spec.bits_per_sample / 8u;
    if (spec.sample_format == SAMPLEFORMAT_IEEEFP && spec.bits_per_sample == 32) {
        const auto v = static_cast<float>(value);
        std::memcpy(buffer, &v, size);
    } else if (spec.sample_format == SAMPLEFORMAT_IEEEFP) {
        std::memcpy(buffer, &value, size);
    } else if (spec.sample_format == SAMPLEFORMAT_INT) {
        const auto v = static_cast<std::int16_t>(value);
        std::memcpy(buffer, &v, size);
    } else if (spec.bits_per_sample == 32) {
        const auto v = static_cast<std::uint32_t>(value);
        std::memcpy(buffer, &v, size);
    } else if (spec.bits_per_sample == 16) {
        const auto v = static_cast<std::uint16_t>(value);
        std::memcpy(buffer, &v, size);
    } else if (spec.bits_per_sample == 8) {
        buffer[0] = static_cast<unsigned char>(value);
    } else {
        size = 1; // fewer bits: two 4-bit samples, say, of the same value
        const auto v = static_cast<unsigned>(value) & 0xFu;
        buffer[0] = static_cast<unsigned char>((v << 4) | v);
    }
    bytes.insert(bytes.end(), buffer, buffer + size);
}

/// Writes a TIFF of these samples (each pixel's in turn, row by row), with libtiff.
void write_tiff(const fs::path& path, const TiffSpec& spec, const std::vector<double>& samples)
{
    const std::size_t spp = spec.samples_per_pixel;
    REQUIRE(samples.size() == std::size_t{spec.width} * spec.height * spp);
    const auto sample = [&](std::uint32_t x, std::uint32_t y, std::size_t c) {
        return samples[(std::size_t{y} * spec.width + x) * spp + c];
    };

    TIFF* tif = TIFFOpen(path.string().c_str(), "w");
    REQUIRE(tif != nullptr);
    TIFFSetField(tif, TIFFTAG_IMAGEWIDTH, spec.width);
    TIFFSetField(tif, TIFFTAG_IMAGELENGTH, spec.height);
    TIFFSetField(tif, TIFFTAG_SAMPLESPERPIXEL, spec.samples_per_pixel);
    TIFFSetField(tif, TIFFTAG_BITSPERSAMPLE, spec.bits_per_sample);
    TIFFSetField(tif, TIFFTAG_SAMPLEFORMAT, spec.sample_format);
    TIFFSetField(tif, TIFFTAG_PHOTOMETRIC, spec.photometric);
    TIFFSetField(tif, TIFFTAG_PLANARCONFIG, spec.planar_config);
    if (!spec.extra_samples.empty()) {
        TIFFSetField(tif,
                     TIFFTAG_EXTRASAMPLES,
                     static_cast<std::uint16_t>(spec.extra_samples.size()),
                     spec.extra_samples.data());
    }
    if (spec.max_sample_value != 0) {
        TIFFSetField(tif, TIFFTAG_MAXSAMPLEVALUE, spec.max_sample_value);
    }
    if (!spec.color_map.empty()) {
        const std::size_t entries = spec.color_map.size() / 3;
        TIFFSetField(tif,
                     TIFFTAG_COLORMAP,
                     spec.color_map.data(),
                     spec.color_map.data() + entries,
                     spec.color_map.data() + 2 * entries);
    }

    const bool separate = spec.planar_config == PLANARCONFIG_SEPARATE;
    const std::size_t planes = separate ? spp : 1;
    const std::size_t per_pixel = separate ? 1 : spp;
    if (spec.tile_size == 0) {
        TIFFSetField(tif, TIFFTAG_ROWSPERSTRIP, std::uint32_t{1});
        for (std::size_t plane = 0; plane < planes; ++plane) {
            for (std::uint32_t y = 0; y < spec.height; ++y) {
                Bytes row;
                for (std::uint32_t x = 0; x < spec.width; ++x) {
                    for (std::size_t c = 0; c < per_pixel; ++c) {
                        append_sample(row, spec, sample(x, y, separate ? plane : c));
                    }
                }
                REQUIRE(TIFFWriteScanline(tif, row.data(), y, static_cast<std::uint16_t>(plane)) ==
                        1);
            }
        }
    } else {
        TIFFSetField(tif, TIFFTAG_TILEWIDTH, spec.tile_size);
        TIFFSetField(tif, TIFFTAG_TILELENGTH, spec.tile_size);
        for (std::size_t plane = 0; plane < planes; ++plane) {
            for (std::uint32_t ty = 0; ty < spec.height; ty += spec.tile_size) {
                for (std::uint32_t tx = 0; tx < spec.width; tx += spec.tile_size) {
                    Bytes tile;
                    for (std::uint32_t y = ty; y < ty + spec.tile_size; ++y) {
                        for (std::uint32_t x = tx; x < tx + spec.tile_size; ++x) {
                            for (std::size_t c = 0; c < per_pixel; ++c) {
                                const bool inside = x < spec.width && y < spec.height;
                                append_sample(
                                    tile, spec, inside ? sample(x, y, separate ? plane : c) : 0.0);
                            }
                        }
                    }
                    REQUIRE(TIFFWriteTile(
                                tif, tile.data(), tx, ty, 0, static_cast<std::uint16_t>(plane)) >=
                            0);
                }
            }
        }
    }
    TIFFClose(tif);
}

/// An RGB test pattern of width x height, as 8-bit samples: red x * 10, green y * 10, blue 7.
std::vector<double> pattern(std::uint32_t width, std::uint32_t height)
{
    std::vector<double> samples;
    for (std::uint32_t y = 0; y < height; ++y) {
        for (std::uint32_t x = 0; x < width; ++x) {
            samples.insert(samples.end(), {10.0 * x, 10.0 * y, 7.0});
        }
    }
    return samples;
}

void check_pattern(const ImageBundle<RGB>& bundle, double max, double scale = 1.0)
{
    const int width = bundle.image.width();
    const int height = bundle.image.height();
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const RGB expected{static_cast<float>(10.0 * x * scale / max),
                               static_cast<float>(10.0 * y * scale / max),
                               static_cast<float>(7.0 * scale / max)};
            REQUIRE(bundle.image(x, y) == expected);
        }
    }
}

/// A TIFF file's bytes, with the offset of every strip moved past the file's end.
Bytes with_missing_strips(const fs::path& path)
{
    Bytes bytes = read_file_to_buffer(path);
    fs::remove(path);

    REQUIRE(bytes[0] == 'I'); // libtiff writes in the machine's byte order: little-endian here
    const auto u16 = [&](std::size_t at) {
        return static_cast<std::uint32_t>(bytes[at] | (bytes[at + 1] << 8));
    };
    const auto u32 = [&](std::size_t at) { return u16(at) | (u16(at + 2) << 16); };
    const std::size_t ifd = u32(4);
    for (std::size_t i = 0; i < u16(ifd); ++i) {
        const std::size_t entry = ifd + 2 + 12 * i;
        if (u16(entry) == TIFFTAG_STRIPOFFSETS) {
            REQUIRE(u16(entry + 2) == TIFF_LONG);
            const std::size_t count = u32(entry + 4);
            const std::size_t offsets = count == 1 ? entry + 8 : u32(entry + 8);
            for (std::size_t strip = 0; strip < count; ++strip) {
                const std::size_t at = offsets + 4 * strip;
                bytes[at] = 0x00;
                bytes[at + 1] = 0xFF;
                bytes[at + 2] = 0xFF;
                bytes[at + 3] = 0x7F;
            }
            return bytes;
        }
    }
    FAIL("No StripOffsets tag");
    return bytes;
}

} // namespace

TEST_CASE("TIFF files keep RGB and mono images, at 8 and 16 bits", "[images][tiff]")
{
    Image<RGB> image(3, 2);
    image(0, 0) = RGB{0.f, 0.25f, 0.5f};
    image(1, 0) = RGB{1.f, 0.1f, 0.9f};
    image(2, 0) = RGB{0.3f, 0.6f, 0.7f};
    image(0, 1) = RGB{0.05f, 0.95f, 0.45f};
    image(1, 1) = RGB{0.5f, 0.5f, 0.5f};
    image(2, 1) = RGB{0.8f, 0.2f, 0.f};

    for (const int bits : {8, 16}) {
        const double max = bits == 8 ? 255.0 : 65535.0;
        ImageBundle<RGB> bundle(image);
        bundle.bit_depth = bits;
        const fs::path path = temp_path("rgb.tif");
        write_image_tiff(path, bundle);

        const ImageBundle<RGB> read = read_image_tiff_rgb(path);
        CHECK(read.bit_depth == bits);
        CHECK(read.color_space == ColorSpaceHint::sRGB);
        CHECK(read.alpha.empty());
        REQUIRE(read.image.resolution() == Resolution{3, 2});
        for (int y = 0; y < 2; ++y) {
            for (int x = 0; x < 3; ++x) {
                for (std::size_t c = 0; c < 3; ++c) {
                    CHECK(read.image(x, y)[c] == quantized(image(x, y)[c], max));
                }
            }
        }

        // Read as mono, the channels are averaged:
        const ImageBundle<float> mono = read_image_tiff_mono(path);
        const RGB& p = read.image(1, 0);
        CHECK(mono.image(1, 0) == (p[0] + p[1] + p[2]) / 3.f);
        fs::remove(path);

        // A mono image, read as RGB, has equal channels:
        ImageBundle<float> gray(image.get_channel(1));
        gray.bit_depth = bits;
        write_image_tiff(path, gray);
        const ImageBundle<RGB> promoted = read_image_tiff_rgb(path);
        CHECK(promoted.image(2, 1) == RGB{quantized(0.2f, max)});
        CHECK(read_image_tiff_mono(path).image(1, 0) == quantized(0.1f, max));
        fs::remove(path);
    }
}

TEST_CASE("TIFF files are written with a description, artist and software", "[images][tiff]")
{
    const fs::path path = temp_path("tags.tif");
    write_image_tiff(path, ImageBundle<RGB>(Image<RGB>(1, 1, RGB{0.5f})), "A test", "Huira");

    TIFF* tif = TIFFOpen(path.string().c_str(), "r");
    REQUIRE(tif != nullptr);
    char* description = nullptr;
    char* artist = nullptr;
    char* software = nullptr;
    REQUIRE(TIFFGetField(tif, TIFFTAG_IMAGEDESCRIPTION, &description) == 1);
    REQUIRE(TIFFGetField(tif, TIFFTAG_ARTIST, &artist) == 1);
    REQUIRE(TIFFGetField(tif, TIFFTAG_SOFTWARE, &software) == 1);
    CHECK(std::string(description) == "A test");
    CHECK(std::string(artist) == "Huira");
    CHECK(std::string(software) == "Huira Image Library");
    TIFFClose(tif);
    fs::remove(path);
}

TEST_CASE("TIFF writers reject what they cannot write", "[images][tiff]")
{
    ImageBundle<RGB> bundle(Image<RGB>(2, 2, RGB{0.5f}));
    bundle.bit_depth = 32;
    CHECK_THROWS_WITH(write_image_tiff(temp_path("32.tif"), bundle),
                      ContainsSubstring("Bit depth must be 8 or 16"));
    CHECK_THROWS_WITH(write_image_tiff(temp_path("empty.tif"), ImageBundle<RGB>{}),
                      ContainsSubstring("Cannot write empty image"));
    CHECK_THROWS_WITH(write_image_tiff(temp_path("empty.tif"), ImageBundle<float>{}),
                      ContainsSubstring("Cannot write empty image"));

    // A directory can't be opened as a file:
    const fs::path not_a_file = temp_path("directory.tif");
    fs::create_directories(not_a_file);
    CHECK_THROWS_WITH(write_image_tiff(not_a_file, ImageBundle<float>(Image<float>(1, 1))),
                      ContainsSubstring("Failed to open file for writing"));
    fs::remove_all(not_a_file);
}

TEST_CASE("TIFF files of floats or 32-bit integers are read", "[images][tiff]")
{
    const fs::path path = temp_path("types.tif");

    TiffSpec floats;
    floats.sample_format = SAMPLEFORMAT_IEEEFP;
    floats.bits_per_sample = 32;
    write_tiff(path, floats, {0.5, -1.0, 3.25, 0.0});
    const ImageBundle<float> read_floats = read_image_tiff_mono(path);
    CHECK(read_floats.color_space == ColorSpaceHint::Linear); // float TIFFs hold linear values
    CHECK(read_floats.image(0, 0) == 0.5f);
    CHECK(read_floats.image(1, 0) == -1.f);
    CHECK(read_floats.image(0, 1) == 3.25f);

    TiffSpec words;
    words.bits_per_sample = 32;
    write_tiff(path, words, {4294967295.0, 0.0, 2147483648.0, 1.0});
    const ImageBundle<float> read_words = read_image_tiff_mono(path);
    CHECK(read_words.image(0, 0) == 1.f);
    CHECK(read_words.image(1, 0) == 0.f);
    CHECK(read_words.image(0, 1) == static_cast<float>(2147483648.0 / 4294967295.0));
    CHECK(read_words.color_space == ColorSpaceHint::sRGB);
    fs::remove(path);
}

TEST_CASE("MinIsWhite TIFFs are inverted, but not their alpha", "[images][tiff]")
{
    const fs::path path = temp_path("miniswhite.tif");
    TiffSpec spec;
    spec.photometric = PHOTOMETRIC_MINISWHITE;
    write_tiff(path, spec, {0.0, 255.0, 51.0, 204.0});
    const ImageBundle<float> gray = read_image_tiff_mono(path);
    CHECK(gray.image(0, 0) == 1.f);
    CHECK(gray.image(1, 0) == 0.f);
    CHECK(gray.image(0, 1) == 1.f - static_cast<float>(51.0 / 255.0));

    spec.samples_per_pixel = 2;
    spec.extra_samples = {EXTRASAMPLE_UNASSALPHA};
    write_tiff(path, spec, {0.0, 255.0, 255.0, 0.0, 51.0, 51.0, 0.0, 255.0});
    const ImageBundle<float> with_alpha = read_image_tiff_mono(path);
    CHECK(with_alpha.image(0, 0) == 1.f);
    CHECK(with_alpha.alpha(0, 0) == 1.f);
    CHECK(with_alpha.image(1, 0) == 0.f);
    CHECK(with_alpha.alpha(1, 0) == 0.f);
    fs::remove(path);
}

TEST_CASE("A TIFF's alpha channel is read, unless it isn't asked for", "[images][tiff]")
{
    const fs::path path = temp_path("alpha.tif");

    TiffSpec rgba;
    rgba.photometric = PHOTOMETRIC_RGB;
    rgba.samples_per_pixel = 4;
    for (const std::uint16_t kind :
         std::initializer_list<std::uint16_t>{EXTRASAMPLE_ASSOCALPHA, EXTRASAMPLE_UNASSALPHA}) {
        rgba.extra_samples = {kind};
        write_tiff(path, rgba, {255, 0, 0, 255, 0, 255, 0, 51, 0, 0, 255, 0, 255, 255, 255, 102});
        const ImageBundle<RGB> read = read_image_tiff_rgb(path);
        CHECK(read.image(0, 0) == RGB{1.f, 0.f, 0.f});
        CHECK(read.image(1, 1) == RGB{1.f});
        REQUIRE(read.alpha.resolution() == Resolution{2, 2});
        CHECK(read.alpha(0, 0) == 1.f);
        CHECK(read.alpha(1, 0) == static_cast<float>(51.0 / 255.0));
        CHECK(read.alpha(0, 1) == 0.f);
        CHECK(read_image_tiff_rgb(path, false).alpha.empty());

        const ImageBundle<float> mono = read_image_tiff_mono(path);
        CHECK(mono.image(1, 1) == 1.f);
        CHECK(mono.alpha(1, 1) == static_cast<float>(102.0 / 255.0));
        CHECK(read_image_tiff_mono(path, false).alpha.empty());
    }

    // Gray and alpha, read as RGB:
    TiffSpec gray_alpha;
    gray_alpha.samples_per_pixel = 2;
    gray_alpha.extra_samples = {EXTRASAMPLE_ASSOCALPHA};
    write_tiff(path, gray_alpha, {255, 255, 0, 51, 51, 0, 102, 255});
    const ImageBundle<RGB> read = read_image_tiff_rgb(path);
    CHECK(read.image(0, 1) == RGB{static_cast<float>(51.0 / 255.0)});
    CHECK(read.alpha(1, 0) == static_cast<float>(51.0 / 255.0));
    fs::remove(path);
}

TEST_CASE("Palette TIFFs are expanded to RGB", "[images][tiff]")
{
    const fs::path path = temp_path("palette.tif");
    TiffSpec spec;
    spec.photometric = PHOTOMETRIC_PALETTE;
    spec.color_map.assign(3 * 256, 0);
    // Entry 1 is red, 2 green and 3 a gray of 128, in 16 bits:
    spec.color_map[1] = 0xFFFF;
    spec.color_map[256 + 2] = 0xFFFF;
    for (std::size_t c = 0; c < 3; ++c) {
        spec.color_map[c * 256 + 3] = 0x8080;
    }
    write_tiff(path, spec, {1, 2, 3, 0});

    const ImageBundle<RGB> read = read_image_tiff_rgb(path);
    CHECK(read.image(0, 0) == RGB{1.f, 0.f, 0.f});
    CHECK(read.image(1, 0) == RGB{0.f, 1.f, 0.f});
    CHECK(read.image(0, 1) == RGB{128.f / 255.f});
    CHECK(read.image(1, 1) == RGB{0.f});
    CHECK(read.alpha(1, 1) == 1.f);
    CHECK(read.color_space == ColorSpaceHint::sRGB);

    const ImageBundle<float> mono = read_image_tiff_mono(path, false);
    CHECK(mono.image(0, 0) == 1.f / 3.f);
    CHECK(mono.alpha.empty());
    fs::remove(path);
}

TEST_CASE("Tiled and planar TIFFs are read", "[images][tiff]")
{
    const fs::path path = temp_path("layout.tif");
    TiffSpec spec;
    spec.photometric = PHOTOMETRIC_RGB;
    spec.samples_per_pixel = 3;
    spec.width = 20; // partial tiles at the right and the bottom
    spec.height = 18;

    SECTION("in strips, one plane per channel")
    {
        spec.planar_config = PLANARCONFIG_SEPARATE;
        write_tiff(path, spec, pattern(spec.width, spec.height));
        check_pattern(read_image_tiff_rgb(path), 255.0);
    }
    SECTION("in tiles, the channels together")
    {
        spec.tile_size = 16;
        write_tiff(path, spec, pattern(spec.width, spec.height));
        check_pattern(read_image_tiff_rgb(path), 255.0);
    }
    SECTION("in tiles, one plane per channel, of 16 bits")
    {
        spec.tile_size = 16;
        spec.planar_config = PLANARCONFIG_SEPARATE;
        spec.bits_per_sample = 16;
        std::vector<double> samples = pattern(spec.width, spec.height);
        for (double& v : samples) {
            v *= 257.0;
        }
        write_tiff(path, spec, samples);
        check_pattern(read_image_tiff_rgb(path), 65535.0, 257.0);
    }
    fs::remove(path);
}

TEST_CASE("TIFF samples are normalized by MaxSampleValue", "[images][tiff]")
{
    const fs::path path = temp_path("max_sample.tif");
    TiffSpec spec;
    spec.bits_per_sample = 16;

    // A maximum of 2^bits - 1 is a sensor's bit depth, and its DN are linear:
    spec.max_sample_value = 1023;
    write_tiff(path, spec, {0, 1023, 512, 2000});
    const ImageBundle<float> sensor = read_image_tiff_mono(path);
    CHECK(sensor.image.sensor_bit_depth() == 10);
    CHECK(sensor.color_space == ColorSpaceHint::Linear);
    CHECK(sensor.image(1, 0) == 1.f);
    CHECK(sensor.image(0, 1) == static_cast<float>(512.0 / 1023.0));

    // Any other maximum only normalizes:
    spec.max_sample_value = 1000;
    write_tiff(path, spec, {0, 1000, 500, 250});
    const ImageBundle<float> other = read_image_tiff_mono(path);
    CHECK(other.image.sensor_bit_depth() == 0);
    CHECK(other.color_space == ColorSpaceHint::sRGB);
    CHECK(other.image(0, 1) == 0.5f);
    CHECK(other.image(1, 1) == 0.25f);
    fs::remove(path);
}

TEST_CASE("TIFF data that is invalid or unsupported is rejected", "[images][tiff]")
{
    const fs::path path = temp_path("unsupported.tif");
    const auto check_rejected = [&](const TiffSpec& spec, const std::string& reason) {
        write_tiff(path,
                   spec,
                   std::vector<double>(
                       std::size_t{spec.width} * spec.height * spec.samples_per_pixel, 1.0));
        CHECK_THROWS_WITH(read_image_tiff_rgb(path), ContainsSubstring(reason));
        CHECK_THROWS_WITH(read_image_tiff_mono(path), ContainsSubstring(reason));
    };

    TiffSpec signed_integers;
    signed_integers.sample_format = SAMPLEFORMAT_INT;
    signed_integers.bits_per_sample = 16;
    check_rejected(signed_integers, "Unsupported signed integer");

    TiffSpec untyped;
    untyped.sample_format = SAMPLEFORMAT_VOID;
    check_rejected(untyped, "Unsupported sample format");

    TiffSpec four_bits;
    four_bits.bits_per_sample = 4;
    check_rejected(four_bits, "Unsupported bits per sample");

    TiffSpec doubles;
    doubles.sample_format = SAMPLEFORMAT_IEEEFP;
    doubles.bits_per_sample = 64;
    check_rejected(doubles, "Unsupported float bit depth");

    TiffSpec cmyk;
    cmyk.photometric = PHOTOMETRIC_SEPARATED;
    cmyk.samples_per_pixel = 4;
    check_rejected(cmyk, "Cannot interpret 4-channel TIFF");

    TiffSpec unspecified_extra;
    unspecified_extra.samples_per_pixel = 2;
    unspecified_extra.extra_samples = {EXTRASAMPLE_UNSPECIFIED};
    check_rejected(unspecified_extra, "Cannot interpret 2-channel TIFF");
    fs::remove(path);

    const Bytes not_tiff{'n', 'o', 't', ' ', 'a', ' ', 'T', 'I', 'F', 'F'};
    CHECK_THROWS_WITH(read_image_tiff_rgb(not_tiff.data(), not_tiff.size()),
                      ContainsSubstring("Failed to open TIFF"));

    // Strips that aren't in the file, as in a truncated file:
    const fs::path damaged = temp_path("damaged.tif");
    write_image_tiff(damaged, ImageBundle<float>(Image<float>(4, 1, 0.5f)));
    const Bytes missing_strip = with_missing_strips(damaged);
    CHECK_THROWS_WITH(read_image_tiff_mono(missing_strip.data(), missing_strip.size()),
                      ContainsSubstring("Failed to read scanline 0"));

    TiffSpec planar;
    planar.photometric = PHOTOMETRIC_RGB;
    planar.samples_per_pixel = 3;
    planar.planar_config = PLANARCONFIG_SEPARATE;
    write_tiff(damaged, planar, std::vector<double>(12, 1.0));
    const Bytes missing_plane = with_missing_strips(damaged);
    CHECK_THROWS_WITH(read_image_tiff_rgb(missing_plane.data(), missing_plane.size()),
                      ContainsSubstring("Failed to read scanline 0 channel 0"));

    TiffSpec palette;
    palette.photometric = PHOTOMETRIC_PALETTE;
    palette.color_map.assign(3 * 256, 0);
    write_tiff(damaged, palette, {1, 2, 3, 0});
    const Bytes missing_palette = with_missing_strips(damaged);
    CHECK_THROWS_WITH(read_image_tiff_rgb(missing_palette.data(), missing_palette.size()),
                      ContainsSubstring("Failed to read palette TIFF"));

    CHECK_THROWS_AS(read_image_tiff_rgb(temp_path("missing.tif")), std::runtime_error);
}

TEST_CASE("libtiff's error messages are logged", "[images][tiff]")
{
    // Its handler formatted each message into a buffer of one char, so only the module was
    // logged ("LIBTIFF FATAL: memory - "), not what went wrong.
    std::vector<std::string> messages;
    const LogLevel level = Logger::get_level();
    Logger::set_level(LogLevel::Debug);
    Logger::set_custom_sink([&](const LogEntry& entry) { messages.push_back(entry.message); });
    const Bytes not_tiff{'n', 'o', 't', ' ', 'a', ' ', 'T', 'I', 'F', 'F'};
    CHECK_THROWS(read_image_tiff_mono(not_tiff.data(), not_tiff.size()));
    Logger::clear_custom_sink();
    Logger::set_level(level);

    bool logged = false;
    for (const std::string& message : messages) {
        logged = logged || (message.find("LIBTIFF FATAL") != std::string::npos &&
                            message.find("Not a TIFF") != std::string::npos);
    }
    CHECK(logged);
}
