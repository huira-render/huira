#include <array>
#include <cmath>
#include <filesystem>
#include <string>
#include <vector>

#include <fitsio.h>

#include "catch2/catch_test_macros.hpp"
#include "huira/images/image.hpp"
#include "huira/images/io/fits_io.hpp"

using namespace huira;
namespace fs = std::filesystem;

namespace {

fs::path temp_fits(const std::string& name)
{
    return fs::temp_directory_path() / ("huira_test_fits_" + name + ".fits");
}

/// The image's first row, from a list of values.
Image<float> row_image(const std::vector<float>& values)
{
    Image<float> image(static_cast<int>(values.size()), 1, 0.f);
    for (std::size_t i = 0; i < values.size(); ++i) {
        image(static_cast<int>(i), 0) = values[i];
    }
    return image;
}

/// The stored values (with BZERO applied) and the type they represent, read with CFITSIO.
struct Stored {
    int equivalent_type = 0;
    std::vector<double> values;
};

Stored read_stored(const fs::path& path, std::size_t count)
{
    Stored stored;
    fitsfile* file = nullptr;
    int status = 0;
    fits_open_file(&file, path.string().c_str(), READONLY, &status);
    fits_get_img_equivtype(file, &stored.equivalent_type, &status);
    stored.values.resize(count);
    long first[2] = {1, 1};
    fits_read_pix(file,
                  TDOUBLE,
                  first,
                  static_cast<long>(count),
                  nullptr,
                  stored.values.data(),
                  nullptr,
                  &status);
    fits_close_file(file, &status);
    REQUIRE(status == 0);
    return stored;
}

} // namespace

TEST_CASE("16-bit FITS images are unsigned and use their whole range", "[images][fits]")
{
    // They were written signed, with values clamped to 32767, so a 16-bit sensor saturated at
    // half its range.
    const std::vector<double> adu{0, 1, 4095, 32767, 32768, 65534, 65535};
    std::vector<float> values;
    for (double v : adu) {
        values.push_back(static_cast<float>(v / 65535.0));
    }
    Image<float> image = row_image(values);
    image.set_sensor_bit_depth(16);

    const fs::path path = temp_fits("u16");
    write_image_fits(path, image, 16);

    const Stored stored = read_stored(path, adu.size());
    CHECK(stored.equivalent_type == USHORT_IMG);
    CHECK(stored.values == adu);

    const auto [read, metadata] = read_image_fits(path);
    CHECK(read.sensor_bit_depth() == 16);
    for (std::size_t i = 0; i < adu.size(); ++i) {
        CHECK(std::round(static_cast<double>(read(static_cast<int>(i), 0)) * 65535.0) == adu[i]);
    }
    fs::remove(path);
}

TEST_CASE("32-bit FITS images are unsigned and written exactly", "[images][fits]")
{
    // Without a sensor bit depth the whole range is used: 1.0 is 2^32 - 1.
    const Image<float> image = row_image({0.f, 0.25f, 0.5f, 1.f});

    const fs::path path = temp_fits("u32");
    write_image_fits(path, image, 32);

    const Stored stored = read_stored(path, 4);
    CHECK(stored.equivalent_type == ULONG_IMG);
    CHECK(stored.values == std::vector<double>{0.0, 1073741824.0, 2147483648.0, 4294967295.0});

    const auto [read, metadata] = read_image_fits(path);
    for (int i = 0; i < 4; ++i) {
        CHECK(read(i, 0) == image(i, 0));
    }
    fs::remove(path);
}

TEST_CASE("8-bit FITS images keep pixels of 0 when there are no undefined pixels", "[images][fits]")
{
    // 0 was reserved for undefined pixels whether or not there were any, so dark pixels were
    // written as 1.
    Image<float> image = row_image({0.f, 1.f / 255.f, 1.f});
    image.set_sensor_bit_depth(8);

    const fs::path path = temp_fits("u8");
    write_image_fits(path, image, 8);

    const Stored stored = read_stored(path, 3);
    CHECK(stored.values == std::vector<double>{0.0, 1.0, 255.0});
    fs::remove(path);
}

TEST_CASE("Undefined pixels in integer FITS images read back as NaN", "[images][fits]")
{
    // They were read back as the BLANK value itself.
    Image<float> image = row_image({std::nanf(""), 0.f, 0.5f});
    image.set_sensor_bit_depth(16);

    const fs::path path = temp_fits("blank");
    write_image_fits(path, image, 16);

    const auto [read, metadata] = read_image_fits(path);
    CHECK(std::isnan(read(0, 0)));
    // With undefined pixels present, 0 ADU marks them, so valid pixels start at 1 ADU:
    CHECK(std::round(static_cast<double>(read(1, 0)) * 65535.0) == 1.0);
    CHECK(std::abs(read(2, 0) - 0.5f) < 1e-4f);
    fs::remove(path);
}
