#include <cmath>
#include <cstddef>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <vector>

#include <turbojpeg.h>

#include "catch2/catch_test_macros.hpp"
#include "catch2/matchers/catch_matchers_string.hpp"
#include "huira/core/spectral_bins.hpp"
#include "huira/images/image.hpp"
#include "huira/images/io/jpeg_io.hpp"

using namespace huira;
using Catch::Matchers::ContainsSubstring;
namespace fs = std::filesystem;

namespace {

using Bytes = std::vector<unsigned char>;

// What JPEG's color conversion and quantization may change a value by, at quality 100:
constexpr float TOLERANCE = 2.f / 255.f;

fs::path temp_path(const std::string& name)
{
    return fs::temp_directory_path() / ("huira_test_jpeg_" + name);
}

/// A 16 x 16 image of four flat 8 x 8 blocks, which JPEG keeps all but exactly.
Image<RGB> blocks()
{
    const RGB colors[4] = {
        RGB{1.f, 0.f, 0.f}, RGB{0.2f, 0.4f, 0.6f}, RGB{0.f, 0.f, 0.f}, RGB{0.9f, 0.9f, 0.5f}};
    Image<RGB> image(16, 16);
    for (int y = 0; y < 16; ++y) {
        for (int x = 0; x < 16; ++x) {
            image(x, y) = colors[(y / 8) * 2 + x / 8];
        }
    }
    return image;
}

bool close(const RGB& a, const RGB& b)
{
    for (std::size_t c = 0; c < 3; ++c) {
        if (std::abs(a[c] - b[c]) > TOLERANCE) {
            return false;
        }
    }
    return true;
}

/// A grayscale JPEG of these 8-bit samples, encoded with TurboJPEG.
Bytes gray_jpeg(const Bytes& samples, int width, int height)
{
    tjhandle compressor = tjInitCompress();
    REQUIRE(compressor != nullptr);
    unsigned char* buffer = nullptr;
    unsigned long size = 0;
    const int result = tjCompress2(compressor,
                                   samples.data(),
                                   width,
                                   0,
                                   height,
                                   TJPF_GRAY,
                                   &buffer,
                                   &size,
                                   TJSAMP_GRAY,
                                   100,
                                   0);
    tjDestroy(compressor);
    REQUIRE(result == 0);
    Bytes jpeg(buffer, buffer + size);
    tjFree(buffer);
    return jpeg;
}

} // namespace

TEST_CASE("JPEG files keep RGB images closely, and are labelled sRGB", "[images][jpeg]")
{
    const Image<RGB> image = blocks();
    const fs::path path = temp_path("blocks.jpg");
    write_image_jpeg(path, ImageBundle<RGB>(image), 100);

    const ImageBundle<RGB> read = read_image_jpeg(path);
    CHECK(read.color_space == ColorSpaceHint::sRGB);
    CHECK(read.alpha.empty());
    REQUIRE(read.image.resolution() == Resolution{16, 16});
    for (int y = 0; y < 16; ++y) {
        for (int x = 0; x < 16; ++x) {
            CHECK(close(read.image(x, y), image(x, y)));
        }
    }

    // Read as mono, a color is its luma (Y), 0.299 R + 0.587 G + 0.114 B:
    const ImageBundle<float> mono = read_image_jpeg_mono(path);
    CHECK(mono.color_space == ColorSpaceHint::sRGB);
    CHECK(std::abs(mono.image(0, 0) - 0.299f) < TOLERANCE);
    CHECK(std::abs(mono.image(12, 4) - (0.299f * 0.2f + 0.587f * 0.4f + 0.114f * 0.6f)) <
          TOLERANCE);
    CHECK(mono.image(3, 12) < TOLERANCE);
    fs::remove(path);
}

TEST_CASE("JPEG files keep mono images closely", "[images][jpeg]")
{
    Image<float> image(8, 8, 0.25f);
    for (int x = 0; x < 8; ++x) {
        image(x, 7) = 0.75f;
    }
    const fs::path path = temp_path("mono.jpg");
    write_image_jpeg(path, ImageBundle<float>(image)); // the default quality, 95

    const ImageBundle<float> mono = read_image_jpeg_mono(path);
    CHECK(std::abs(mono.image(4, 0) - 0.25f) < 3.f / 255.f);
    CHECK(std::abs(mono.image(4, 7) - 0.75f) < 3.f / 255.f);

    // A mono image is written as equal channels:
    const ImageBundle<RGB> rgb = read_image_jpeg(path);
    CHECK(std::abs(rgb.image(4, 0)[0] - rgb.image(4, 0)[2]) <= 1.f / 255.f);
    fs::remove(path);
}

TEST_CASE("Values outside [0, 1] are clamped in a JPEG", "[images][jpeg]")
{
    const fs::path path = temp_path("clamped.jpg");
    write_image_jpeg(path, ImageBundle<RGB>(Image<RGB>(8, 8, RGB{2.f, -1.f, 0.5f})), 100);
    CHECK(close(read_image_jpeg(path).image(4, 4), RGB{1.f, 0.f, 0.5f}));
    fs::remove(path);
}

TEST_CASE("A grayscale JPEG is read as RGB with equal channels", "[images][jpeg]")
{
    Bytes samples(8 * 8, 64);
    samples[0] = 200;
    const Bytes jpeg = gray_jpeg(samples, 8, 8);

    const ImageBundle<RGB> rgb = read_image_jpeg(jpeg.data(), jpeg.size());
    REQUIRE(rgb.image.resolution() == Resolution{8, 8});
    CHECK(close(rgb.image(7, 7), RGB{64.f / 255.f}));
    CHECK(rgb.image(7, 7)[0] == rgb.image(7, 7)[1]);
    CHECK(rgb.image(7, 7)[0] == rgb.image(7, 7)[2]);

    const ImageBundle<float> mono = read_image_jpeg_mono(jpeg.data(), jpeg.size());
    CHECK(mono.image(7, 7) == rgb.image(7, 7)[0]);
}

TEST_CASE("JPEG writers create the file's directory, and reject what they cannot write",
          "[images][jpeg]")
{
    const fs::path directory = temp_path("new_directory");
    fs::remove_all(directory);
    write_image_jpeg(directory / "image.jpg", ImageBundle<float>(Image<float>(8, 8, 1.f)), 1);
    CHECK(fs::exists(directory / "image.jpg"));
    fs::remove_all(directory);

    const ImageBundle<RGB> image(blocks());
    CHECK_THROWS_WITH(write_image_jpeg(temp_path("empty.jpg"), ImageBundle<RGB>{}),
                      ContainsSubstring("Cannot write empty image"));
    CHECK_THROWS_WITH(write_image_jpeg(temp_path("empty.jpg"), ImageBundle<float>{}),
                      ContainsSubstring("Cannot write empty image"));
    CHECK_THROWS_WITH(write_image_jpeg(temp_path("quality.jpg"), image, 0),
                      ContainsSubstring("quality must be 1-100"));
    CHECK_THROWS_WITH(write_image_jpeg(temp_path("quality.jpg"), image, 101),
                      ContainsSubstring("quality must be 1-100"));

    // A directory can't be opened as a file:
    const fs::path not_a_file = temp_path("directory.jpg");
    fs::create_directories(not_a_file);
    CHECK_THROWS_WITH(write_image_jpeg(not_a_file, image),
                      ContainsSubstring("Failed to open file for writing"));
    fs::remove_all(not_a_file);
}

TEST_CASE("JPEG data that is invalid is rejected", "[images][jpeg]")
{
    const Bytes not_jpeg{'n', 'o', 't', ' ', 'a', ' ', 'J', 'P', 'E', 'G'};
    CHECK_THROWS_WITH(read_image_jpeg(not_jpeg.data(), not_jpeg.size()),
                      ContainsSubstring("Failed to read JPEG header"));
    CHECK_THROWS_WITH(read_image_jpeg_mono(not_jpeg.data(), not_jpeg.size()),
                      ContainsSubstring("Failed to read JPEG header"));

    // Its header complete, but not its data, which starts after the start-of-scan segment:
    const fs::path path = temp_path("truncated.jpg");
    write_image_jpeg(path, ImageBundle<RGB>(blocks()), 100);
    const Bytes valid = read_file_to_buffer(path);
    fs::remove(path);
    std::size_t scan = 0;
    while (scan + 3 < valid.size() && !(valid[scan] == 0xFF && valid[scan + 1] == 0xDA)) {
        ++scan;
    }
    REQUIRE(scan + 3 < valid.size());
    const std::size_t data_start = scan + 2 + (std::size_t{valid[scan + 2]} << 8) + valid[scan + 3];
    const Bytes truncated(valid.begin(),
                          valid.begin() + static_cast<std::ptrdiff_t>(data_start + 4));
    CHECK_THROWS_WITH(read_image_jpeg(truncated.data(), truncated.size()),
                      ContainsSubstring("Failed to decompress JPEG"));

    CHECK_THROWS_AS(read_image_jpeg(temp_path("missing.jpg")), std::runtime_error);
    CHECK_THROWS_AS(read_image_jpeg_mono(temp_path("missing.jpg")), std::runtime_error);
}
