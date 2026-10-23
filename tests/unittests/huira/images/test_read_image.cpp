#include <cmath>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <vector>

#include "catch2/catch_test_macros.hpp"
#include "catch2/matchers/catch_matchers_string.hpp"
#include "huira/core/spectral_bins.hpp"
#include "huira/images/image.hpp"
#include "huira/images/io/fits_io.hpp"
#include "huira/images/io/io_util.hpp"
#include "huira/images/io/jpeg_io.hpp"
#include "huira/images/io/png_io.hpp"
#include "huira/images/io/read_image.hpp"
#include "huira/images/io/tiff_io.hpp"

using namespace huira;
using Catch::Matchers::ContainsSubstring;
namespace fs = std::filesystem;

namespace {

using Bytes = std::vector<unsigned char>;

fs::path temp_path(const std::string& name)
{
    return fs::temp_directory_path() / ("huira_test_read_image_" + name);
}

void write_bytes(const fs::path& path, const Bytes& bytes)
{
    std::ofstream file(path, std::ios::binary);
    file.write(reinterpret_cast<const char*>(bytes.data()),
               static_cast<std::streamsize>(bytes.size()));
}

// A red pixel, in each of the formats read_image() reads by itself:

Bytes red_bmp()
{
    Bytes bytes{'B', 'M', 58, 0, 0, 0, 0, 0, 0, 0, 54, 0, 0, 0}; // the file's size, pixel offset
    bytes.insert(bytes.end(), {40, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 24, 0}); // 1 x 1, 24-bit
    bytes.resize(54, 0);                       // BI_RGB, and the rest of the header
    bytes.insert(bytes.end(), {0, 0, 255, 0}); // BGR, padded to 4 bytes
    return bytes;
}

const Bytes RED_TGA{0, 0, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1, 0, 24, 0, 0, 0, 255};

Bytes red_hdr()
{
    const std::string header = "#?RADIANCE\nFORMAT=32-bit_rle_rgbe\n\n-Y 1 +X 1\n";
    Bytes bytes(header.begin(), header.end());
    bytes.insert(bytes.end(), {128, 0, 0, 129}); // 1, 0, 0
    return bytes;
}

} // namespace

TEST_CASE("Image formats are detected from the file extension", "[images][read_image]")
{
    CHECK(detect_image_format("a.bmp") == IMAGE_FORMAT_BMP);
    CHECK(detect_image_format("a.BMP") == IMAGE_FORMAT_BMP);
    CHECK(detect_image_format("a.fits") == IMAGE_FORMAT_FITS);
    CHECK(detect_image_format("a.FITS") == IMAGE_FORMAT_FITS);
    CHECK(detect_image_format("a.hdr") == IMAGE_FORMAT_HDR);
    CHECK(detect_image_format("a.HDR") == IMAGE_FORMAT_HDR);
    CHECK(detect_image_format("a.jpg") == IMAGE_FORMAT_JPEG);
    CHECK(detect_image_format("a.jpeg") == IMAGE_FORMAT_JPEG);
    CHECK(detect_image_format("a.JPG") == IMAGE_FORMAT_JPEG);
    CHECK(detect_image_format("a.JPEG") == IMAGE_FORMAT_JPEG);
    CHECK(detect_image_format("dir/a.png") == IMAGE_FORMAT_PNG);
    CHECK(detect_image_format("a.PNG") == IMAGE_FORMAT_PNG);
    CHECK(detect_image_format("a.tga") == IMAGE_FORMAT_TGA);
    CHECK(detect_image_format("a.TGA") == IMAGE_FORMAT_TGA);
    CHECK(detect_image_format("a.tif") == IMAGE_FORMAT_TIFF);
    CHECK(detect_image_format("a.tiff") == IMAGE_FORMAT_TIFF);
    CHECK(detect_image_format("a.TIF") == IMAGE_FORMAT_TIFF);
    CHECK(detect_image_format("a.TIFF") == IMAGE_FORMAT_TIFF);

    CHECK(detect_image_format("a.exr") == IMAGE_FORMAT_UNKNOWN);
    CHECK(detect_image_format("a.Png") == IMAGE_FORMAT_UNKNOWN);
    CHECK(detect_image_format("png") == IMAGE_FORMAT_UNKNOWN);
}

TEST_CASE("read_image() reads each format it detects", "[images][read_image]")
{
    const RGB red{1.f, 0.f, 0.f};
    const ImageBundle<RGB> red_image(Image<RGB>(8, 8, red));

    std::vector<fs::path> files{temp_path("red.bmp"),
                                temp_path("red.tga"),
                                temp_path("red.hdr"),
                                temp_path("red.png"),
                                temp_path("red.tif"),
                                temp_path("red.jpg")};
    write_bytes(files[0], red_bmp());
    write_bytes(files[1], RED_TGA);
    write_bytes(files[2], red_hdr());
    write_image_png(files[3], red_image);
    write_image_tiff(files[4], red_image);
    write_image_jpeg(files[5], red_image, 100);

    for (const fs::path& path : files) {
        INFO(path.filename().string());
        const ImageBundle<RGB> rgb = read_image(path);
        CHECK(std::abs(rgb.image(0, 0)[0] - 1.f) < 2.f / 255.f);
        CHECK(rgb.image(0, 0)[1] < 2.f / 255.f);
        CHECK(rgb.image(0, 0)[2] < 2.f / 255.f);
        CHECK(std::abs(read_image_mono(path).image(0, 0) - 1.f / 3.f) < 0.1f);

        const Bytes bytes = read_file_to_buffer(path);
        const ImageFormat format = detect_image_format(path);
        CHECK(read_image(format, bytes.data(), bytes.size()).image.resolution() ==
              rgb.image.resolution());
        CHECK(read_image_mono(format, bytes.data(), bytes.size()).image.resolution() ==
              rgb.image.resolution());
        fs::remove(path);
    }
}

TEST_CASE("read_image() passes on whether to read alpha", "[images][read_image]")
{
    const fs::path path = temp_path("alpha.png");
    write_image_png(path, ImageBundle<RGB>(Image<RGB>(2, 2, RGB{0.5f}), Image<float>(2, 2, 0.5f)));
    CHECK(read_image(path).alpha.width() == 2);
    CHECK(read_image(path, false).alpha.empty());
    CHECK(read_image_mono(path).alpha.width() == 2);
    CHECK(read_image_mono(path, false).alpha.empty());
    fs::remove(path);
}

TEST_CASE("read_image() rejects FITS files and formats it doesn't know", "[images][read_image]")
{
    const fs::path fits = temp_path("image.fits");
    write_image_fits(fits, Image<float>(2, 2, 0.5f));
    CHECK_THROWS_WITH(read_image(fits),
                      ContainsSubstring("FITS format is not currently supported"));
    CHECK_THROWS_WITH(read_image_mono(fits),
                      ContainsSubstring("FITS format is not currently supported"));
    fs::remove(fits);

    const fs::path unknown = temp_path("image.exr");
    write_bytes(unknown, red_bmp());
    CHECK_THROWS_WITH(read_image(unknown),
                      ContainsSubstring("Unsupported or unknown image format"));
    CHECK_THROWS_WITH(read_image_mono(unknown),
                      ContainsSubstring("Unsupported or unknown image format"));
    fs::remove(unknown);

    const Bytes bmp = red_bmp();
    CHECK_THROWS_WITH(read_image(IMAGE_FORMAT_UNKNOWN, bmp.data(), bmp.size()),
                      ContainsSubstring("Unsupported or unknown image format"));
}

TEST_CASE("read_file_to_buffer() reads a whole file, and rejects missing or empty files",
          "[images][read_image]")
{
    const fs::path path = temp_path("bytes.bin");
    write_bytes(path, RED_TGA);
    CHECK(read_file_to_buffer(path) == RED_TGA);

    write_bytes(path, Bytes{});
    CHECK_THROWS_WITH(read_file_to_buffer(path), ContainsSubstring("file is empty"));
    fs::remove(path);

    CHECK_THROWS_WITH(read_file_to_buffer(temp_path("missing.bin")),
                      ContainsSubstring("Failed to open file"));
    CHECK_THROWS_WITH(read_image(temp_path("missing.png")),
                      ContainsSubstring("Failed to open file"));
}
