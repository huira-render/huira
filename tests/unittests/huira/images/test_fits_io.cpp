#include <array>
#include <cmath>
#include <filesystem>
#include <string>
#include <utility>
#include <variant>
#include <vector>

#include <fitsio.h>

#include "catch2/catch_test_macros.hpp"
#include "catch2/matchers/catch_matchers_string.hpp"
#include "huira/images/image.hpp"
#include "huira/images/io/fits_io.hpp"
#include "huira/util/logger.hpp"

using namespace huira;
using Catch::Matchers::ContainsSubstring;
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

namespace {

/// The custom keyword with this name, or nullptr.
const FitsKeyword* find_keyword(const FitsMetadata& metadata, const std::string& key)
{
    for (const FitsKeyword& keyword : metadata.custom_keywords) {
        if (keyword.key == key) {
            return &keyword;
        }
    }
    return nullptr;
}

bool contains(const std::vector<std::string>& lines, const std::string& text)
{
    for (const std::string& line : lines) {
        if (line.find(text) != std::string::npos) {
            return true;
        }
    }
    return false;
}

/// Writes a FITS image with CFITSIO itself, as other software would: of a CFITSIO image type,
/// these dimensions and these physical values (BZERO and BSCALE are applied), with whatever
/// keywords add_keywords() writes.
template <typename AddKeywords>
void write_foreign_fits(const fs::path& path,
                        int image_type,
                        std::vector<long> axes,
                        std::vector<double> values,
                        AddKeywords&& add_keywords)
{
    fitsfile* file = nullptr;
    int status = 0;
    const std::string name = "!" + path.string();
    fits_create_file(&file, name.c_str(), &status);
    fits_create_img(file, image_type, static_cast<int>(axes.size()), axes.data(), &status);
    add_keywords(file, status);
    std::vector<long> first(axes.size(), 1);
    fits_write_pix(
        file, TDOUBLE, first.data(), static_cast<LONGLONG>(values.size()), values.data(), &status);
    fits_close_file(file, &status);
    REQUIRE(status == 0);
}

void write_foreign_fits(const fs::path& path,
                        int image_type,
                        std::vector<long> axes,
                        std::vector<double> values)
{
    write_foreign_fits(
        path, image_type, std::move(axes), std::move(values), [](fitsfile*, int&) {});
}

} // namespace

TEST_CASE("FITS metadata is written and read back", "[images][fits]")
{
    FitsMetadata metadata;
    metadata.object = "M31";
    metadata.telescop = "Huira";
    metadata.instrume = "Test camera";
    metadata.observer = "Catch2";
    metadata.date_obs = "2026-10-23T00:00:00.000";
    metadata.origin = "huira tests";
    metadata.exptime = 12.5f;
    metadata.filter = "Ha";
    metadata.bunit = "W/m2/sr/nm";
    metadata.saturate = 1.5; // kept for a float image that is not a sensor response
    metadata.crpix1 = 2.5;
    metadata.crpix2 = 1.0;
    metadata.crval1 = 10.68;
    metadata.crval2 = 41.27;
    metadata.cdelt1 = -0.001;
    metadata.cdelt2 = 0.001;
    metadata.ctype1 = "RA---TAN";
    metadata.ctype2 = "DEC--TAN";
    metadata.equinox = 2015.5;
    metadata.radesys = "FK5";
    metadata.comments = {"First comment", "Second comment"};
    metadata.history = {"Rendered by a test"};
    metadata.custom_keywords = {FitsKeyword{"STRKEY", std::string("text"), "A string"},
                                FitsKeyword{"DBLKEY", 0.125, "A double"},
                                FitsKeyword{"INTKEY", -42, ""},
                                FitsKeyword{"BOOLKEY", true, "A bool"},
                                FitsKeyword{"FALSEKEY", false, ""}};
    CHECK(metadata.has_wcs());

    const fs::path path = temp_fits("metadata");
    write_image_fits(path, row_image({0.25f, 0.5f, 0.75f, 1.f}), -32, metadata);
    const auto [image, read] = read_image_fits(path);
    fs::remove(path);

    CHECK(image(3, 0) == 1.f);
    CHECK(read.object == "M31");
    CHECK(read.telescop == "Huira");
    CHECK(read.instrume == "Test camera");
    CHECK(read.observer == "Catch2");
    CHECK(read.date_obs == "2026-10-23T00:00:00.000");
    CHECK(read.origin == "huira tests");
    CHECK(read.exptime == 12.5f);
    CHECK(read.filter == "Ha");
    CHECK(read.bunit == "W/m2/sr/nm");
    CHECK(read.saturate == 1.5);
    CHECK(read.datamin == 0.25); // from the image, not the metadata
    CHECK(read.datamax == 1.0);
    CHECK(read.crpix1 == 2.5);
    CHECK(read.crpix2 == 1.0);
    CHECK(read.crval1 == 10.68);
    CHECK(read.crval2 == 41.27);
    CHECK(read.cdelt1 == -0.001);
    CHECK(read.cdelt2 == 0.001);
    CHECK(read.ctype1 == "RA---TAN");
    CHECK(read.ctype2 == "DEC--TAN");
    CHECK(read.equinox == 2015.5);
    CHECK(read.radesys == "FK5");
    CHECK(read.has_wcs());
    CHECK(contains(read.comments, "First comment"));
    CHECK(contains(read.comments, "Second comment"));
    CHECK(contains(read.history, "Rendered by a test"));

    // Custom keywords keep their types and comments:
    const FitsKeyword* text = find_keyword(read, "STRKEY");
    REQUIRE(text != nullptr);
    REQUIRE(std::holds_alternative<std::string>(text->value));
    CHECK(std::get<std::string>(text->value) == "text");
    CHECK(text->comment == "A string");
    const FitsKeyword* real = find_keyword(read, "DBLKEY");
    REQUIRE(real != nullptr);
    REQUIRE(std::holds_alternative<double>(real->value));
    CHECK(std::get<double>(real->value) == 0.125);
    const FitsKeyword* integer = find_keyword(read, "INTKEY");
    REQUIRE(integer != nullptr);
    REQUIRE(std::holds_alternative<int>(integer->value));
    CHECK(std::get<int>(integer->value) == -42);
    const FitsKeyword* flag = find_keyword(read, "BOOLKEY");
    REQUIRE(flag != nullptr);
    REQUIRE(std::holds_alternative<bool>(flag->value));
    CHECK(std::get<bool>(flag->value));
    const FitsKeyword* unset = find_keyword(read, "FALSEKEY");
    REQUIRE(unset != nullptr);
    REQUIRE(std::holds_alternative<bool>(unset->value));
    CHECK_FALSE(std::get<bool>(unset->value));
}

TEST_CASE("FITS metadata without a WCS or exposure writes none", "[images][fits]")
{
    const fs::path path = temp_fits("no_wcs");
    write_image_fits(path, row_image({0.f, 1.f}));
    CHECK(std::isnan(read_key(path, "CRPIX1")));
    CHECK(std::isnan(read_key(path, "EQUINOX")));
    CHECK(std::isnan(read_key(path, "EXPTIME")));

    const auto [image, metadata] = read_image_fits(path);
    CHECK_FALSE(metadata.has_wcs());
    CHECK(metadata.exptime == 0.f);
    CHECK(metadata.object.empty());
    CHECK(metadata.equinox == 2000.0); // the defaults
    CHECK(metadata.radesys == "ICRS");
    CHECK(metadata.bunit.empty()); // a float image that is not a sensor response has no unit
    CHECK_FALSE(metadata.saturate.has_value());
    CHECK(metadata.history.empty());
    fs::remove(path);
}

TEST_CASE("Unusual FITS header cards are read as custom keywords, or skipped", "[images][fits]")
{
    const fs::path path = temp_fits("cards");
    write_foreign_fits(path, FLOAT_IMG, {2, 1}, {1.0, 2.0}, [](fitsfile* file, int& status) {
        fits_write_record(file, "CPLXKEY =         (1.0, 2.0) / a complex value", &status);
        fits_write_record(file, "NOVALUE =                      / an undefined value", &status);
        fits_write_record(file, "HISTORY", &status); // with no text
    });
    const auto [image, metadata] = read_image_fits(path);
    fs::remove(path);

    CHECK(image(1, 0) == 2.f);
    const FitsKeyword* complex = find_keyword(metadata, "CPLXKEY");
    REQUIRE(complex != nullptr);
    REQUIRE(std::holds_alternative<std::string>(complex->value));
    CHECK(find_keyword(metadata, "NOVALUE") == nullptr);
    CHECK(metadata.history.empty());
}

TEST_CASE("FITS images from other software are normalised by their type's range", "[images][fits]")
{
    const fs::path path = temp_fits("foreign");
    const auto check_read = [&](int image_type, double value, double expected) {
        write_foreign_fits(path, image_type, {2, 1}, {0.0, value});
        const auto [image, metadata] = read_image_fits(path);
        CHECK(image(0, 0) == 0.f);
        CHECK(image(1, 0) == static_cast<float>(expected));
        CHECK(image.sensor_bit_depth() == 0);
    };

    check_read(BYTE_IMG, 51.0, 51.0 / 255.0);
    check_read(SBYTE_IMG, -127.0, -1.0);
    check_read(SHORT_IMG, 32767.0, 1.0);
    check_read(SHORT_IMG, -16384.0, -16384.0 / 32767.0);
    check_read(USHORT_IMG, 65535.0, 1.0);
    check_read(LONG_IMG, 2147483647.0, 1.0);
    check_read(ULONG_IMG, 1073741824.0, 1073741824.0 / 4294967295.0);
    check_read(LONGLONG_IMG, 4611686018427387904.0, 0.5);
    check_read(FLOAT_IMG, 3.5, 3.5); // as it is
    check_read(DOUBLE_IMG, -0.125, -0.125);
    fs::remove(path);
}

TEST_CASE("Integer FITS images are normalised by SATURATE, which may give a bit depth",
          "[images][fits]")
{
    const fs::path path = temp_fits("saturate");

    // An ADC's ceiling, 2^12 - 1:
    write_foreign_fits(path, SHORT_IMG, {2, 1}, {0.0, 2048.0}, [](fitsfile* file, int& status) {
        double saturate = 4095.0;
        fits_update_key(file, TDOUBLE, "SATURATE", &saturate, nullptr, &status);
    });
    const auto [adc, adc_metadata] = read_image_fits(path);
    CHECK(adc.sensor_bit_depth() == 12);
    CHECK(adc(1, 0) == static_cast<float>(2048.0 / 4095.0));
    CHECK(adc_metadata.saturate == 4095.0);

    // Any other level:
    write_foreign_fits(path, SHORT_IMG, {2, 1}, {0.0, 500.0}, [](fitsfile* file, int& status) {
        double saturate = 1000.0;
        fits_update_key(file, TDOUBLE, "SATURATE", &saturate, nullptr, &status);
    });
    const auto [level, level_metadata] = read_image_fits(path);
    CHECK(level.sensor_bit_depth() == 0);
    CHECK(level(1, 0) == 0.5f);

    // Scaled integers hold values that aren't integers, and are returned as they are:
    write_foreign_fits(path, SHORT_IMG, {2, 1}, {0.0, 2.5}, [](fitsfile* file, int& status) {
        double bscale = 0.5;
        fits_update_key(file, TDOUBLE, "BSCALE", &bscale, nullptr, &status);
        fits_set_bscale(file, 0.5, 0.0, &status);
    });
    const auto [scaled, scaled_metadata] = read_image_fits(path);
    CHECK(scaled(1, 0) == 2.5f);

    // An ADC bit depth more than a double holds is ignored:
    write_foreign_fits(path, FLOAT_IMG, {2, 1}, {0.0, 7.0}, [](fitsfile* file, int& status) {
        int bits = 60;
        fits_update_key(file, TINT, "ADCBITS", &bits, nullptr, &status);
    });
    const auto [wide, wide_metadata] = read_image_fits(path);
    CHECK(wide.sensor_bit_depth() == 0);
    CHECK(wide(1, 0) == 7.f);
    fs::remove(path);
}

TEST_CASE("64-bit float FITS images are written exactly", "[images][fits]")
{
    const fs::path path = temp_fits("f64");
    write_image_fits(path, row_image({0.1f, -2.5f, 1e30f}), -64);
    const Stored stored = read_stored(path, 3);
    CHECK(stored.equivalent_type == DOUBLE_IMG);
    CHECK(stored.values ==
          std::vector<double>{static_cast<double>(0.1f), -2.5, static_cast<double>(1e30f)});
    const auto [image, metadata] = read_image_fits(path);
    CHECK(image(0, 0) == 0.1f);
    CHECK(image(2, 0) == 1e30f);
    fs::remove(path);
}

TEST_CASE("FITS files that are not single images are rejected", "[images][fits]")
{
    const fs::path path = temp_fits("shape");

    write_foreign_fits(path, FLOAT_IMG, {3}, {1.0, 2.0, 3.0});
    CHECK_THROWS_WITH(read_image_fits(path), ContainsSubstring("NAXIS=1"));

    write_foreign_fits(path, FLOAT_IMG, {2, 1, 2}, {1.0, 2.0, 3.0, 4.0});
    CHECK_THROWS_WITH(read_image_fits(path), ContainsSubstring("data cube"));

    fs::remove(path);

    CHECK_THROWS_WITH(read_image_fits(temp_fits("missing")), ContainsSubstring("FITS I/O error"));
}

TEST_CASE("FITS writes that can't be done are rejected", "[images][fits]")
{
    const fs::path path = temp_fits("rejected");
    CHECK_THROWS_WITH(write_image_fits(path, Image<float>{}), ContainsSubstring("empty image"));
    CHECK_THROWS_WITH(write_image_fits(path, row_image({0.f}), 12),
                      ContainsSubstring("Invalid FITS bit_depth"));

    Image<float> wide = row_image({0.f, 1.f});
    wide.set_sensor_bit_depth(40);
    CHECK_THROWS_WITH(write_image_fits(path, wide, 32), ContainsSubstring("more than the 32 bits"));
    // which only matters for writing DN:
    write_image_fits(path, wide, 32, {}, PixelScaling::FullRange);
    CHECK(read_stored(path, 2).values == std::vector<double>{0.0, 4294967295.0});
    fs::remove(path);
}

TEST_CASE("FITS images with further axes of length 1 are read", "[images][fits]")
{
    // The first pixel was given to CFITSIO as two coordinates, though it reads one for every
    // axis: with NAXIS3 = 1, it read past them, and the read failed ("bad first element number").
    const fs::path path = temp_fits("extra_axes");
    write_foreign_fits(path, FLOAT_IMG, {2, 1, 1}, {1.0, 2.0});
    CHECK(read_image_fits(path).first(1, 0) == 2.f);

    write_foreign_fits(path, FLOAT_IMG, {2, 1, 1, 1}, {3.0, 4.0});
    CHECK(read_image_fits(path).first(0, 0) == 3.f);

    // More than one plane along any of them is a cube:
    write_foreign_fits(path, FLOAT_IMG, {2, 1, 1, 2}, {1.0, 2.0, 3.0, 4.0});
    CHECK_THROWS_WITH(read_image_fits(path), ContainsSubstring("NAXIS4=2"));
    fs::remove(path);
}

TEST_CASE("A FITS file whose recognised keywords aren't of their types is still read",
          "[images][fits]")
{
    // A recognised keyword whose value isn't a number (or that had no value) failed the whole
    // read ("bad formatted string to double conversion").
    const fs::path path = temp_fits("unreadable_keys");
    write_foreign_fits(path, SHORT_IMG, {2, 1}, {0.0, 16383.0}, [](fitsfile* file, int& status) {
        fits_write_record(file, "EXPTIME = 'unknown'          / not a number", &status);
        fits_write_record(file, "DATAMIN = 'n/a'", &status);
        fits_write_record(file, "SATURATE= 'none'", &status);
        fits_write_record(file, "ADCBITS = 'twelve'", &status);
        fits_write_record(file, "CRPIX1  = '2026-10-23T00:00:00'", &status);
        fits_write_record(file, "EQUINOX =                      / no value", &status);
        fits_write_record(file, "OBJECT  =                      / no value", &status);
    });

    std::vector<std::string> warnings;
    Logger::set_custom_sink([&](const LogEntry& entry) {
        if (entry.level == LogLevel::Warning) {
            warnings.push_back(entry.message);
        }
    });
    const auto [image, metadata] = read_image_fits(path);
    Logger::clear_custom_sink();
    fs::remove(path);

    // They are left unset, so SATURATE and ADCBITS don't normalise the image:
    CHECK(image(1, 0) == static_cast<float>(16383.0 / 32767.0));
    CHECK(image.sensor_bit_depth() == 0);
    CHECK(metadata.exptime == 0.f);
    CHECK_FALSE(metadata.datamin.has_value());
    CHECK_FALSE(metadata.saturate.has_value());
    CHECK_FALSE(metadata.crpix1.has_value());
    CHECK(metadata.equinox == 2000.0);
    CHECK(metadata.object.empty());
    CHECK(warnings.size() == 6);

    // and returned as custom keywords, as the file has them, when they have a value:
    for (const auto& [key, text] :
         std::vector<std::pair<std::string, std::string>>{{"EXPTIME", "unknown"},
                                                          {"DATAMIN", "n/a"},
                                                          {"SATURATE", "none"},
                                                          {"ADCBITS", "twelve"},
                                                          {"CRPIX1", "2026-10-23T00:00:00"}}) {
        INFO(key);
        const FitsKeyword* keyword = find_keyword(metadata, key);
        REQUIRE(keyword != nullptr);
        REQUIRE(std::holds_alternative<std::string>(keyword->value));
        CHECK(std::get<std::string>(keyword->value) == text);
    }
    CHECK(find_keyword(metadata, "EQUINOX") == nullptr);

    // A number written as a string is read as the number:
    write_foreign_fits(path, FLOAT_IMG, {2, 1}, {0.0, 1.0}, [](fitsfile* file, int& status) {
        fits_write_record(file, "EXPTIME = '30.5'", &status);
    });
    const auto [quoted, quoted_metadata] = read_image_fits(path);
    fs::remove(path);
    CHECK(quoted_metadata.exptime == 30.5f);
    CHECK(find_keyword(quoted_metadata, "EXPTIME") == nullptr);
}
