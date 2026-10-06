#include <cmath>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <limits>
#include <string>
#include <vector>

#include "catch2/catch_test_macros.hpp"
#include "huira/core/spectral_bins.hpp"
#include "huira/images/image.hpp"
#include "huira/images/io/color_space.hpp"
#include "huira/images/io/jpeg_io.hpp"
#include "huira/images/io/png_io.hpp"
#include "huira/images/io/tiff_io.hpp"
#include "tiffio.h"

using namespace huira;
namespace fs = std::filesystem;

namespace {

fs::path temp_path(const std::string& name)
{
    return fs::temp_directory_path() / ("huira_test_output_" + name);
}

/// A row of a sensor response of the given bit depth, holding these DN, as a readout makes it.
Image<float> sensor_row(const std::vector<std::uint32_t>& dn, int bits)
{
    const double max = std::ldexp(1.0, bits) - 1.0;
    Image<float> image(static_cast<int>(dn.size()), 1, 0.f);
    for (std::size_t i = 0; i < dn.size(); ++i) {
        image(static_cast<int>(i), 0) = static_cast<float>(static_cast<double>(dn[i]) / max);
    }
    image.set_sensor_bit_depth(bits);
    return image;
}

/// The samples of a gray PNG's first row, as stored.
std::vector<std::uint32_t> png_row(const fs::path& path, int& significant_bits, bool& gray)
{
    const auto file = read_file_to_buffer(path);
    const PNGData data = read_png_raw_(file.data(), file.size());
    significant_bits = data.significant_bits;
    gray = data.is_gray;
    std::vector<std::uint32_t> row;
    for (png_uint_32 x = 0; x < data.width; ++x) {
        if (data.final_bit_depth == 16) {
            std::uint16_t v = 0;
            std::memcpy(&v, data.raw_data.data() + x * data.channels * 2, 2);
            row.push_back(v);
        } else {
            row.push_back(data.raw_data[x * data.channels]);
        }
    }
    return row;
}

/// The samples of a 16-bit gray TIFF's first row, as stored, and its MaxSampleValue (0 if none).
std::vector<std::uint16_t> tiff_row16(const fs::path& path, int width, std::uint16_t& max_sample)
{
    TIFF* tif = TIFFOpen(path.string().c_str(), "r");
    REQUIRE(tif != nullptr);
    std::vector<std::uint16_t> row(static_cast<std::size_t>(width));
    REQUIRE(TIFFReadScanline(tif, row.data(), 0) >= 0);
    max_sample = 0;
    if (TIFFGetField(tif, TIFFTAG_MAXSAMPLEVALUE, &max_sample) != 1) {
        max_sample = 0;
    }
    TIFFClose(tif);
    return row;
}

} // namespace

TEST_CASE("A sensor response is written to PNG as its DN, which read back exactly", "[images][png]")
{
    // It was stretched to the file's range with nothing to say how many bits the data had.
    const std::vector<std::uint32_t> dn{0, 1, 2048, 4094, 4095};
    ImageBundle<float> bundle(sensor_row(dn, 12));
    bundle.bit_depth = 16;

    const fs::path path = temp_path("dn.png");
    write_image_png(path, bundle);

    int significant_bits = 0;
    bool gray = false;
    const std::vector<std::uint32_t> stored = png_row(path, significant_bits, gray);
    CHECK(gray); // a mono image is written gray, not as three equal channels
    CHECK(significant_bits == 12);
    for (std::size_t i = 0; i < dn.size(); ++i) {
        // Left-bit replication, so the DN are the top 12 bits:
        CHECK(stored[i] == ((dn[i] << 4) | (dn[i] >> 8)));
        CHECK((stored[i] >> 4) == dn[i]);
    }

    const ImageBundle<float> read = read_image_png_mono(path);
    CHECK(read.image.sensor_bit_depth() == 12);
    CHECK(read.color_space == ColorSpaceHint::Linear);
    for (std::size_t i = 0; i < dn.size(); ++i) {
        CHECK(read.image(static_cast<int>(i), 0) == bundle.image(static_cast<int>(i), 0));
    }
    fs::remove(path);

    // With fewer bits than the sensor, the lowest are dropped:
    bundle.bit_depth = 8;
    write_image_png(path, bundle);
    const std::vector<std::uint32_t> stored8 = png_row(path, significant_bits, gray);
    CHECK(stored8 == std::vector<std::uint32_t>{0, 0, 128, 255, 255});
    fs::remove(path);
}

TEST_CASE("A sensor response is written to TIFF as raw DN with MaxSampleValue", "[images][tiff]")
{
    // It was stretched to the file's range, and truncated rather than rounded.
    const std::vector<std::uint32_t> dn{0, 1, 2048, 4095};
    ImageBundle<float> bundle(sensor_row(dn, 12));
    bundle.bit_depth = 16;

    const fs::path path = temp_path("dn.tif");
    write_image_tiff(path, bundle);

    std::uint16_t max_sample = 0;
    const std::vector<std::uint16_t> stored = tiff_row16(path, 4, max_sample);
    CHECK(max_sample == 4095);
    CHECK(stored == std::vector<std::uint16_t>{0, 1, 2048, 4095});

    const ImageBundle<float> read = read_image_tiff_mono(path);
    CHECK(read.image.sensor_bit_depth() == 12);
    CHECK(read.color_space == ColorSpaceHint::Linear);
    for (int i = 0; i < 4; ++i) {
        CHECK(read.image(i, 0) == bundle.image(i, 0));
    }
    fs::remove(path);

    // Stretched over the range only if asked, and rounded:
    bundle.scaling = PixelScaling::FullRange;
    write_image_tiff(path, bundle);
    CHECK(tiff_row16(path, 4, max_sample) == std::vector<std::uint16_t>{0, 16, 32776, 65535});
    CHECK(max_sample == 0);
    fs::remove(path);

    Image<float> picture(1, 1, 0.999f);
    write_image_tiff(path, ImageBundle<float>(picture));  // 8 bits
    CHECK(read_image_tiff_mono(path).image(0, 0) == 1.f); // 254.7 rounds to 255
    fs::remove(path);
}

TEST_CASE("Values that are not finite are written as 0", "[images][png][tiff][jpeg]")
{
    // Converting NaN or an infinity to an integer is undefined behaviour.
    Image<float> image(4, 1, 0.5f);
    image(0, 0) = std::numeric_limits<float>::quiet_NaN();
    image(1, 0) = std::numeric_limits<float>::infinity();
    image(2, 0) = -std::numeric_limits<float>::infinity();

    const fs::path png = temp_path("nan.png");
    write_image_png(png, ImageBundle<float>(image));
    const ImageBundle<float> from_png = read_image_png_mono(png);
    const fs::path tif = temp_path("nan.tif");
    write_image_tiff(tif, ImageBundle<float>(image));
    const ImageBundle<float> from_tiff = read_image_tiff_mono(tif);
    for (int x = 0; x < 3; ++x) {
        CHECK(from_png.image(x, 0) == 0.f);
        CHECK(from_tiff.image(x, 0) == 0.f);
    }
    CHECK(from_png.image(3, 0) == 128.f / 255.f);

    // JPEG blurs neighbouring values, so a whole 8x8 block:
    Image<float> block(8, 8, std::numeric_limits<float>::quiet_NaN());
    const fs::path jpeg = temp_path("nan.jpg");
    write_image_jpeg(jpeg, ImageBundle<float>(block), 100);
    CHECK(read_image_jpeg_mono(jpeg).image(3, 3) == 0.f);

    fs::remove(png);
    fs::remove(tif);
    fs::remove(jpeg);
}

TEST_CASE("Files are labelled with the encoding their values have", "[images][png]")
{
    // Every image was labelled sRGB unless set otherwise, though Huira renders linear values.
    ImageBundle<RGB> linear(Image<RGB>(2, 1, RGB{0.25f}));
    CHECK(linear.color_space == ColorSpaceHint::Linear);
    linear.bit_depth = 16;
    linear.image.set_sensor_bit_depth(12);

    const fs::path path = temp_path("label.png");
    write_image_png(path, linear);
    CHECK(read_image_png(path).color_space == ColorSpaceHint::Linear);

    // linear_to_srgb() marks its result sRGB, keeps the file settings, and drops the sensor bit
    // depth, since sRGB values are not DN:
    // A channel of a sensor response is DN too:
    CHECK(linear.image.get_channel(1).sensor_bit_depth() == 12);

    const ImageBundle<RGB> srgb = linear_to_srgb(linear);
    CHECK(srgb.color_space == ColorSpaceHint::sRGB);
    CHECK(srgb.bit_depth == 16);
    CHECK(srgb.image.sensor_bit_depth() == 0);
    write_image_png(path, srgb);
    const ImageBundle<RGB> read = read_image_png(path);
    CHECK(read.color_space == ColorSpaceHint::sRGB);
    CHECK(read.image.sensor_bit_depth() == 0);
    CHECK(std::abs(read.image(0, 0)[0] - linear_to_srgb(0.25f)) < 1e-4f);
    fs::remove(path);
}

TEST_CASE("A spectral image can be written to TIFF", "[images][tiff]")
{
    // The overload called one that does not exist, so it did not compile when used.
    ImageBundle<Visible8> bundle(Image<Visible8>(2, 2, Visible8{0.5f}));
    const fs::path path = temp_path("spectral.tif");
    write_image_tiff(path, bundle);
    CHECK(std::abs(read_image_tiff_mono(path).image(1, 1) - 128.f / 255.f) < 1e-6f);
    fs::remove(path);
}
