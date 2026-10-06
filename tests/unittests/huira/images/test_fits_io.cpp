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

/// A numeric header keyword, read with CFITSIO; NaN if it is absent.
double read_key(const fs::path& path, const char* key)
{
    fitsfile* file = nullptr;
    int status = 0;
    fits_open_file(&file, path.string().c_str(), READONLY, &status);
    double value = std::nan("");
    int key_status = 0;
    fits_read_key_dbl(file, key, &value, nullptr, &key_status);
    if (key_status != 0) {
        value = std::nan("");
    }
    fits_close_file(file, &status);
    REQUIRE(status == 0);
    return value;
}

/// A sensor response of the given bit depth holding these DN.
Image<float> sensor_image(const std::vector<double>& dn, int bits)
{
    const double max = std::ldexp(1.0, bits) - 1.0;
    std::vector<float> values;
    for (double v : dn) {
        values.push_back(std::isfinite(v) ? static_cast<float>(v / max) : static_cast<float>(v));
    }
    Image<float> image = row_image(values);
    image.set_sensor_bit_depth(bits);
    return image;
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

TEST_CASE("A sensor with more bits than the file keeps its high bits, in DN", "[images][fits]")
{
    // The DN were clamped to the file's range: a 20-bit sensor in 16 bits saturated at 6% of
    // its range, while SATURATE claimed 2^20 - 1.
    const std::vector<double> dn{0, 15, 16, 65536, 524288, 1048575};
    const Image<float> image = sensor_image(dn, 20);

    const fs::path path = temp_fits("drop_bits");
    write_image_fits(path, image, 16);

    // Every FITS reader sees DN, with the lowest 4 bits dropped (BSCALE 16):
    const Stored stored = read_stored(path, dn.size());
    CHECK(stored.values == std::vector<double>{0, 0, 16, 65536, 524288, 1048560});
    CHECK(read_key(path, "BSCALE") == 16.0);
    CHECK(read_key(path, "SATURATE") == 1048575.0);
    CHECK(read_key(path, "ADCBITS") == 20.0);

    const auto [read, metadata] = read_image_fits(path);
    CHECK(read.sensor_bit_depth() == 20);
    for (std::size_t i = 0; i < dn.size(); ++i) {
        CHECK(std::round(static_cast<double>(read(static_cast<int>(i), 0)) * 1048575.0) ==
              stored.values[i]);
    }
    fs::remove(path);

    // The default 12-bit sensor in an 8-bit file:
    write_image_fits(path, sensor_image({0, 2048, 4095}, 12), 8);
    CHECK(read_stored(path, 3).values == std::vector<double>{0, 2048, 4080});
    fs::remove(path);
}

TEST_CASE("Undefined pixels use a value the sensor's DN leave spare", "[images][fits]")
{
    // A 12-bit sensor in 16 bits never reaches 65535, which marks the undefined pixels, so dark
    // pixels keep 0 DN instead of being raised to 1.
    const Image<float> image = sensor_image({std::nan(""), 0, 4095}, 12);

    const fs::path path = temp_fits("spare_blank");
    write_image_fits(path, image, 16);
    CHECK(read_key(path, "BLANK") == 32767.0); // 65535, stored offset by BZERO = 32768

    const auto [read, metadata] = read_image_fits(path);
    CHECK(std::isnan(read(0, 0)));
    CHECK(read(1, 0) == 0.f);
    CHECK(read(2, 0) == 1.f);
    fs::remove(path);
}

TEST_CASE("Keywords that describe the data are not carried over from another file",
          "[images][fits]")
{
    // BLANK came back as a custom keyword, and was written into the next file with the
    // metadata, where it matched 0 DN: every dark pixel read back as undefined. SATURATE,
    // DATAMIN and DATAMAX from the old file also won over the new image's.
    const fs::path first = temp_fits("carry_first");
    write_image_fits(first, sensor_image({std::nan(""), 0, 1}, 16), 16);
    const auto [old_image, metadata] = read_image_fits(first);
    for (const FitsKeyword& keyword : metadata.custom_keywords) {
        CHECK(keyword.key != "BLANK");
        CHECK(keyword.key != "ADCBITS");
    }

    FitsMetadata carried = metadata;
    carried.custom_keywords.push_back(FitsKeyword{"BLANK", -32768, ""});
    carried.custom_keywords.push_back(FitsKeyword{"BSCALE", 2.0, ""});

    const fs::path second = temp_fits("carry_second");
    write_image_fits(second, sensor_image({0, 7, 16383}, 14), 16, carried);
    CHECK(std::isnan(read_key(second, "BLANK")));
    CHECK(read_key(second, "SATURATE") == 16383.0);
    CHECK(read_key(second, "DATAMIN") == 0.0);
    CHECK(read_key(second, "DATAMAX") == 16383.0);

    const auto [read, read_metadata] = read_image_fits(second);
    CHECK(read.sensor_bit_depth() == 14);
    CHECK(read(0, 0) == 0.f);
    CHECK(std::round(static_cast<double>(read(1, 0)) * 16383.0) == 7.0);
    CHECK(read(2, 0) == 1.f);
    fs::remove(first);
    fs::remove(second);
}

TEST_CASE("A float FITS image of a sensor response holds its DN", "[images][fits]")
{
    // It held the fractions DN / (2^bits - 1), with nothing to say what they were.
    const Image<float> image = sensor_image({0, 1, 2048, 4095, std::nan("")}, 12);

    const fs::path path = temp_fits("float_dn");
    write_image_fits(path, image);
    const Stored stored = read_stored(path, 4);
    CHECK(stored.values == std::vector<double>{0, 1, 2048, 4095});
    CHECK(read_key(path, "ADCBITS") == 12.0);
    CHECK(read_key(path, "SATURATE") == 4095.0);

    const auto [read, metadata] = read_image_fits(path);
    CHECK(read.sensor_bit_depth() == 12);
    CHECK(metadata.bunit == "adu");
    for (int i = 0; i < 4; ++i) {
        CHECK(read(i, 0) == image(i, 0));
    }
    CHECK(std::isnan(read(4, 0)));
    fs::remove(path);

    // Stretched, if asked; and an image that is not a sensor response is written as it is:
    write_image_fits(path, sensor_image({0, 4095}, 12), 16, {}, PixelScaling::FullRange);
    CHECK(read_stored(path, 2).values == std::vector<double>{0, 65535});
    CHECK(std::isnan(read_key(path, "ADCBITS")));
    CHECK(read_key(path, "SATURATE") == 65535.0);
    fs::remove(path);

    write_image_fits(path, row_image({3.5f, -2.f}));
    CHECK(read_stored(path, 2).values == std::vector<double>{3.5, -2.0});
    fs::remove(path);
}

TEST_CASE("An image with no defined pixels writes a readable file", "[images][fits]")
{
    // DATAMIN and DATAMAX were +-1.8e308, which CFITSIO cannot read back.
    for (const int bitpix : {16, -32}) {
        const fs::path path = temp_fits("all_undefined");
        write_image_fits(path, sensor_image({std::nan(""), std::nan("")}, 12), bitpix);
        CHECK(std::isnan(read_key(path, "DATAMIN")));
        const auto [read, metadata] = read_image_fits(path);
        CHECK(std::isnan(read(0, 0)));
        CHECK(std::isnan(read(1, 0)));
        fs::remove(path);
    }
}
