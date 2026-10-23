#include <cmath>
#include <cstdint>
#include <limits>
#include <string>
#include <vector>

#include "catch2/catch_test_macros.hpp"
#include "huira/core/spectral_bins.hpp"
#include "huira/core/types.hpp"
#include "huira/images/image.hpp"
#include "huira/images/io/color_space.hpp"
#include "huira/images/io/convert_pixel.hpp"
#include "huira/images/io/pixel_encoding.hpp"
#include "huira/util/logger.hpp"

using namespace huira;

namespace {

constexpr float NAN_F = std::numeric_limits<float>::quiet_NaN();
constexpr float INF_F = std::numeric_limits<float>::infinity();

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

bool close(float a, float b, float tolerance = 1e-6f)
{
    return std::abs(a - b) <= tolerance;
}

} // namespace

TEST_CASE("Integers map to and from a float range", "[images][convert_pixel]")
{
    CHECK(integer_to_float<std::uint8_t>(0) == 0.f);
    CHECK(integer_to_float<std::uint8_t>(255) == 1.f);
    CHECK(integer_to_float<std::uint8_t>(51) == 0.2f);
    CHECK(integer_to_float<std::uint16_t>(65535) == 1.f);
    CHECK(integer_to_float<std::uint8_t>(0, -1.f, 1.f) == -1.f);
    CHECK(integer_to_float<std::uint8_t>(255, -1.f, 1.f) == 1.f);

    // Signed integers map their whole range, from the most negative value:
    CHECK(integer_to_float<std::int8_t>(-128) == 0.f);
    CHECK(integer_to_float<std::int8_t>(127) == 1.f);
    CHECK(integer_to_float<std::int16_t>(-32768, 0.f, 10.f) == 0.f);
    CHECK(integer_to_float<std::int16_t>(32767, 0.f, 10.f) == 10.f);

    CHECK(float_to_integer<std::uint8_t>(0.f) == 0);
    CHECK(float_to_integer<std::uint8_t>(1.f) == 255);
    CHECK(float_to_integer<std::uint8_t>(0.5f) == 128); // rounded
    CHECK(float_to_integer<std::uint8_t>(2.f) == 255);  // clamped
    CHECK(float_to_integer<std::uint8_t>(-1.f) == 0);
    CHECK(float_to_integer<std::uint8_t>(5.f, 0.f, 10.f) == 128);
    CHECK(float_to_integer<std::uint16_t>(1.f) == 65535);

    CHECK(float_to_integer<std::int8_t>(1.f) == 127);
    CHECK(float_to_integer<std::int8_t>(0.5f) == 0);
    CHECK(float_to_integer<std::int16_t>(1.f) == 32767);
    CHECK(float_to_integer<std::int16_t>(15.f, -5.f, 15.f) == 32767);

    // Every value round-trips through 8 and 16 bits:
    for (int v = 0; v <= 255; ++v) {
        const auto byte = static_cast<std::uint8_t>(v);
        REQUIRE(float_to_integer<std::uint8_t>(integer_to_float<std::uint8_t>(byte)) == byte);
    }
    for (int v = 0; v <= 65535; v += 257) {
        const auto word = static_cast<std::uint16_t>(v);
        REQUIRE(float_to_integer<std::uint16_t>(integer_to_float<std::uint16_t>(word)) == word);
    }
}

TEST_CASE("Values that are not finite become the lowest integer", "[images][convert_pixel]")
{
    // Converting NaN or an infinity to an integer is undefined behaviour.
    CHECK(float_to_integer<std::uint8_t>(NAN_F) == 0);
    CHECK(float_to_integer<std::uint16_t>(INF_F) == 0);
    CHECK(float_to_integer<std::uint16_t>(-INF_F) == 0);
    CHECK(float_to_integer<std::int8_t>(NAN_F) == -128);
    CHECK(float_to_integer<std::int16_t>(INF_F) == -32768);
}

TEST_CASE("A float becomes a pixel of any type", "[images][convert_pixel]")
{
    CHECK(convert_float_to_pixel<float>(0.25f) == 0.25f);
    CHECK(convert_float_to_pixel<double>(0.25f) == 0.25);
    CHECK(convert_float_to_pixel<std::uint8_t>(1.f) == 255);
    CHECK(convert_float_to_pixel<std::uint16_t>(0.f) == 0);
    CHECK(convert_float_to_pixel<std::int16_t>(1.f) == 32767);
    CHECK(convert_float_to_pixel<Vec3<float>>(0.5f) == Vec3<float>{0.5f, 0.5f, 0.5f});
    CHECK(convert_float_to_pixel<Vec3<double>>(0.5f) == Vec3<double>{0.5, 0.5, 0.5});
}

TEST_CASE("A color becomes a pixel of any type, as its luminance if it has one channel",
          "[images][convert_pixel]")
{
    const Vec3<float> color{0.2f, 0.4f, 0.8f};
    const float luminance = 0.2126f * 0.2f + 0.7152f * 0.4f + 0.0722f * 0.8f;

    CHECK(close(convert_vec3_to_pixel<float>(color), luminance));
    CHECK(close(static_cast<float>(convert_vec3_to_pixel<double>(color)), luminance));
    CHECK(convert_vec3_to_pixel<std::uint8_t>(Vec3<float>{1.f, 1.f, 1.f}) == 255);
    CHECK(convert_vec3_to_pixel<std::uint8_t>(Vec3<float>{1.f, 0.f, 0.f}) == 54); // 0.2126 * 255
    CHECK(convert_vec3_to_pixel<Vec3<float>>(color) == color);
    CHECK(convert_vec3_to_pixel<Vec3<double>>(Vec3<float>{0.5f, 0.25f, 1.f}) ==
          Vec3<double>{0.5, 0.25, 1.0});
}

TEST_CASE("A pixel of any type becomes a float, or a color", "[images][convert_pixel]")
{
    CHECK(convert_pixel_to_float<float>(0.75f) == 0.75f);
    CHECK(convert_pixel_to_float<double>(0.75) == 0.75f);
    CHECK(convert_pixel_to_float<std::uint8_t>(255) == 1.f);
    CHECK(convert_pixel_to_float<std::uint16_t>(0) == 0.f);
    CHECK(convert_pixel_to_float<std::int8_t>(-128) == 0.f);
    CHECK(close(convert_pixel_to_float<Vec3<float>>(Vec3<float>{1.f, 1.f, 1.f}), 1.f));
    CHECK(close(convert_pixel_to_float<Vec3<float>>(Vec3<float>{0.f, 1.f, 0.f}), 0.7152f));
    CHECK(close(convert_pixel_to_float<Vec3<double>>(Vec3<double>{0.f, 0.f, 1.f}), 0.0722f));

    CHECK(convert_pixel_to_vec3<float>(0.5f) == Vec3<float>{0.5f, 0.5f, 0.5f});
    CHECK(convert_pixel_to_vec3<double>(0.5) == Vec3<float>{0.5f, 0.5f, 0.5f});
    CHECK(convert_pixel_to_vec3<std::uint8_t>(255) == Vec3<float>{1.f, 1.f, 1.f});
    CHECK(convert_pixel_to_vec3<std::uint16_t>(0) == Vec3<float>{0.f, 0.f, 0.f});
    CHECK(convert_pixel_to_vec3<Vec3<float>>(Vec3<float>{0.1f, 0.2f, 0.3f}) ==
          Vec3<float>{0.1f, 0.2f, 0.3f});
    CHECK(convert_pixel_to_vec3<Vec3<double>>(Vec3<double>{0.5, 0.25, 1.0}) ==
          Vec3<float>{0.5f, 0.25f, 1.f});
}

TEST_CASE("Integer encodings stretch values, or write a sensor's DN", "[images][pixel_encoding]")
{
    // An image without a sensor bit depth is stretched over the file's range:
    const detail::IntegerEncoding picture(8, 0, PixelScaling::Counts);
    CHECK_FALSE(picture.counts());
    CHECK(picture.file_max() == 255);
    CHECK(picture.full_range(0.5f) == 128);
    CHECK(picture.full_range(2.f) == 255);
    CHECK(picture.full_range(-1.f) == 0);
    CHECK(picture.raw(1.f) == 255);

    // A sensor response is written as its DN, with no more bits than the file:
    const detail::IntegerEncoding counts(16, 12, PixelScaling::Counts);
    CHECK(counts.counts());
    CHECK(counts.shift == 0);
    CHECK(counts.sensor_max() == 4095);
    CHECK(counts.dn(1.f) == 4095);
    CHECK(counts.dn(0.5f) == 2048);
    CHECK(counts.dn(2.f) == 4095);
    CHECK(counts.dn(-1.f) == 0);
    CHECK(counts.raw(1.f) == 4095);

    // or with its lowest bits dropped, with more:
    const detail::IntegerEncoding dropped(8, 12, PixelScaling::Counts);
    CHECK(dropped.shift == 4);
    CHECK(dropped.raw(1.f) == 255);
    CHECK(dropped.raw(16.f / 4095.f) == 1);

    // FullRange, or a bit depth that isn't positive, stretches it as any other image:
    const detail::IntegerEncoding full_range(16, 12, PixelScaling::FullRange);
    CHECK_FALSE(full_range.counts());
    CHECK(full_range.raw(1.f) == 65535);
    CHECK_FALSE(detail::IntegerEncoding(16, -3, PixelScaling::Counts).counts());
}

TEST_CASE("Left-bit replication scales a value to more bits", "[images][pixel_encoding]")
{
    CHECK(detail::replicate_bits(0xABC, 12, 16) == 0xABCA);
    CHECK(detail::replicate_bits(0x5, 3, 8) == 0xB6); // 101 101 10
    CHECK(detail::replicate_bits(1, 1, 8) == 0xFF);
    CHECK(detail::replicate_bits(0x3F, 6, 16) == 0xFFFF);
    CHECK(detail::replicate_bits(0, 12, 16) == 0);

    // A value with as many bits as the result, or more, or no bits, is returned as it is:
    CHECK(detail::replicate_bits(0x1234, 16, 16) == 0x1234);
    CHECK(detail::replicate_bits(0x1234, 16, 8) == 0x1234);
    CHECK(detail::replicate_bits(7, 0, 8) == 7);
}

TEST_CASE("Values that are not finite are counted, and reported once", "[images][pixel_encoding]")
{
    detail::NonFiniteCounter counter;
    CHECK_FALSE(counter.not_finite(0.f));
    CHECK_FALSE(counter.not_finite(-std::numeric_limits<float>::max()));
    {
        WarningLog log;
        counter.warn("writer", "nothing.png");
        CHECK(log.messages.empty());
    }

    CHECK(counter.not_finite(NAN_F));
    CHECK(counter.not_finite(NAN_F));
    CHECK(counter.not_finite(INF_F));
    CHECK(counter.not_finite(-INF_F));

    WarningLog log;
    counter.warn("writer", "image.png");
    REQUIRE(log.messages.size() == 1);
    CHECK(log.messages[0].find("writer - image.png") != std::string::npos);
    CHECK(log.messages[0].find("2 NaN, 1 +inf and 1 -inf") != std::string::npos);
}

TEST_CASE("Values convert between linear and the sRGB curve", "[images][color_space]")
{
    CHECK(srgb_to_linear(0.f) == 0.f);
    CHECK(srgb_to_linear(0.04045f) == 0.04045f / 12.92f); // the linear segment
    CHECK(close(srgb_to_linear(0.5f), 0.214041f));
    CHECK(close(srgb_to_linear(1.f), 1.f));

    CHECK(linear_to_srgb(0.f) == 0.f);
    CHECK(linear_to_srgb(0.002f) == 0.002f * 12.92f); // the linear segment
    CHECK(close(linear_to_srgb(0.214041f), 0.5f));
    CHECK(close(linear_to_srgb(1.f), 1.f));

    for (const float v : {0.f, 0.001f, 0.01f, 0.1f, 0.25f, 0.5f, 0.75f, 1.f}) {
        CHECK(close(srgb_to_linear(linear_to_srgb(v)), v));
        CHECK(close(linear_to_srgb(srgb_to_linear(v)), v));
    }

    CHECK(gamma_to_linear(1.f, 2.2f) == 1.f);
    CHECK(close(gamma_to_linear(0.5f, 2.2f), 0.217638f));
    CHECK(close(gamma_to_linear(0.25f, 0.5f), 0.5f));
}

TEST_CASE("Image bundles convert between linear and sRGB, keeping their file settings",
          "[images][color_space]")
{
    Image<float> mono(2, 1, 0.25f);
    mono(1, 0) = 1.f;
    mono.set_sensor_bit_depth(12);
    ImageBundle<float> linear(mono, Image<float>(2, 1, 0.5f));
    linear.bit_depth = 16;
    linear.scaling = PixelScaling::FullRange;

    const ImageBundle<float> srgb = linear_to_srgb(linear);
    CHECK(srgb.color_space == ColorSpaceHint::sRGB);
    CHECK(srgb.image(0, 0) == linear_to_srgb(0.25f));
    CHECK(srgb.image(1, 0) == linear_to_srgb(1.f));
    CHECK(srgb.alpha(1, 0) == 0.5f);
    CHECK(srgb.bit_depth == 16);
    CHECK(srgb.scaling == PixelScaling::FullRange);
    CHECK(srgb.image.sensor_bit_depth() == 0); // sRGB values are not a sensor's DN

    const ImageBundle<float> back = srgb_to_linear(srgb);
    CHECK(back.color_space == ColorSpaceHint::Linear);
    CHECK(close(back.image(0, 0), 0.25f));
    CHECK(back.alpha(0, 0) == 0.5f);
    CHECK(back.bit_depth == 16);

    ImageBundle<RGB> color(Image<RGB>(1, 2, RGB{0.f, 0.5f, 1.f}));
    color.color_space = ColorSpaceHint::sRGB;
    const ImageBundle<RGB> color_linear = srgb_to_linear(color);
    CHECK(color_linear.color_space == ColorSpaceHint::Linear);
    CHECK(color_linear.image(0, 1)[0] == 0.f);
    CHECK(color_linear.image(0, 1)[1] == srgb_to_linear(0.5f));
    CHECK(color_linear.image(0, 1)[2] == srgb_to_linear(1.f));

    const ImageBundle<RGB> color_srgb = linear_to_srgb(color_linear);
    CHECK(color_srgb.color_space == ColorSpaceHint::sRGB);
    for (std::size_t c = 0; c < 3; ++c) {
        CHECK(close(color_srgb.image(0, 0)[c], color.image(0, 0)[c]));
    }
}
