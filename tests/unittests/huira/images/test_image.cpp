#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <type_traits>
#include <utility>

#include "catch2/catch_test_macros.hpp"
#include "huira/core/spectral_bins.hpp"
#include "huira/core/types.hpp"
#include "huira/images/image.hpp"

using namespace huira;

namespace {

/// A 3 x 2 image whose pixel (x, y) holds 10 y + x, so that bilinear samples, which are exact for
/// a linear function, can be checked against 10 py + px.
Image<float> ramp()
{
    Image<float> image(3, 2);
    for (int y = 0; y < 2; ++y) {
        for (int x = 0; x < 3; ++x) {
            image(x, y) = static_cast<float>(10 * y + x);
        }
    }
    return image;
}

} // namespace

static_assert(ImagePixelTraits<float>::channels == 1);
static_assert(std::is_same_v<ImagePixelTraits<float>::Scalar, float>);
static_assert(ImagePixelTraits<std::int8_t>::channels == 1);
static_assert(ImagePixelTraits<Vec3<float>>::channels == 3);
static_assert(std::is_same_v<ImagePixelTraits<Vec3<double>>::Scalar, double>);
static_assert(ImagePixelTraits<RGB>::channels == 3);
static_assert(ImagePixelTraits<Visible8>::channels == 8);
static_assert(std::is_same_v<ImagePixelTraits<Visible8>::Scalar, float>);

TEST_CASE("An image is constructed empty, sized, or sized and filled", "[images][image]")
{
    const Image<float> empty;
    CHECK(empty.empty());
    CHECK_FALSE(static_cast<bool>(empty));
    CHECK(empty.size() == 0);
    CHECK(empty.width() == 0);
    CHECK(empty.height() == 0);

    const Image<float> by_resolution(Resolution{3, 2});
    CHECK_FALSE(by_resolution.empty());
    CHECK(static_cast<bool>(by_resolution));
    CHECK(by_resolution.size() == 6);
    CHECK(by_resolution.resolution() == Resolution{3, 2});
    for (std::size_t i = 0; i < by_resolution.size(); ++i) {
        CHECK(by_resolution[i] == 0.f);
    }

    const Image<float> filled_by_resolution(Resolution{2, 4}, 7.f);
    CHECK(filled_by_resolution.width() == 2);
    CHECK(filled_by_resolution.height() == 4);
    for (std::size_t i = 0; i < filled_by_resolution.size(); ++i) {
        CHECK(filled_by_resolution[i] == 7.f);
    }

    const Image<float> by_size(4, 1);
    CHECK(by_size.resolution() == Resolution{4, 1});
    CHECK(by_size.size() == 4);

    const Image<RGB> filled_by_size(2, 2, RGB{0.f, 0.5f, 1.f});
    CHECK(filled_by_size.size() == 4);
    CHECK(filled_by_size(1, 1) == RGB{0.f, 0.5f, 1.f});

    // An image with no columns (or rows) holds no pixels, but keeps its other dimension:
    const Image<float> no_columns(0, 5);
    CHECK(no_columns.empty());
    CHECK(no_columns.height() == 5);
}

TEST_CASE("An image cannot have a negative width or height", "[images][image]")
{
    CHECK_THROWS_AS(Image<float>(-1, 2), std::runtime_error);
    CHECK_THROWS_AS(Image<float>(2, -1, 0.f), std::runtime_error);
    CHECK_THROWS_AS(Image<float>(Resolution{-3, 1}), std::runtime_error);
    CHECK_THROWS_AS(Image<float>(Resolution{1, -3}, 1.f), std::runtime_error);
}

TEST_CASE("Pixels are stored in row-major order from the top-left corner", "[images][image]")
{
    Image<float> image = ramp();
    const Image<float>& view = image;

    CHECK(image.data()[0] == 0.f);
    CHECK(view.data()[4] == 11.f);
    for (int y = 0; y < image.height(); ++y) {
        for (int x = 0; x < image.width(); ++x) {
            const auto index = static_cast<std::size_t>(y * image.width() + x);
            CHECK(view(x, y) == view[index]);
            CHECK(&image(x, y) == &image[index]);
        }
    }

    // Writes through every accessor reach the same pixel:
    image(2, 1) = 100.f;
    CHECK(view[5] == 100.f);
    image[5] = 101.f;
    CHECK(view(2, 1) == 101.f);
    image.data()[5] = 102.f;
    CHECK(view.at(5) == 102.f);
}

TEST_CASE("Pixel coordinates are truncated to the pixel they fall in", "[images][image]")
{
    Image<float> image = ramp();
    const Image<float>& view = image;

    CHECK(image(Pixel{1.9f, 1.2f}) == 11.f);
    CHECK(view(Pixel{0.f, 0.99f}) == 0.f);
    CHECK(image.at(Pixel{2.5f, 0.5f}) == 2.f);
    CHECK(view.at(Pixel{1.f, 1.f}) == 11.f);

    image(Pixel{0.5f, 1.5f}) = -1.f;
    CHECK(view(0, 1) == -1.f);
    image.at(Pixel{1.5f, 0.5f}) = -2.f;
    CHECK(view(1, 0) == -2.f);
}

TEST_CASE("at() checks its index or coordinates", "[images][image]")
{
    Image<float> image = ramp();
    const Image<float>& view = image;

    CHECK(image.at(5) == 12.f);
    CHECK(view.at(0) == 0.f);
    CHECK(image.at(2, 1) == 12.f);
    CHECK(view.at(0, 1) == 10.f);

    image.at(1, 1) = 50.f;
    CHECK(view(1, 1) == 50.f);
    image.at(4) = 60.f;
    CHECK(view(1, 1) == 60.f);

    CHECK_THROWS_AS(image.at(6), std::runtime_error);
    CHECK_THROWS_AS(view.at(6), std::runtime_error);
    CHECK_THROWS_AS(image.at(3, 0), std::runtime_error);
    CHECK_THROWS_AS(view.at(0, 2), std::runtime_error);
    CHECK_THROWS_AS(image.at(Pixel{3.f, 0.f}), std::runtime_error);
    CHECK_THROWS_AS(view.at(Pixel{0.f, 2.5f}), std::runtime_error);

    const Image<float> empty;
    CHECK_THROWS_AS(empty.at(0), std::runtime_error);
    CHECK_THROWS_AS(empty.at(0, 0), std::runtime_error);
}

TEST_CASE("Nearest-neighbour sampling maps [0, 1] onto the pixel centres", "[images][image]")
{
    const Image<float> image = ramp();

    // u = 0 and 1 are the centres of the first and last columns, v those of the first and last
    // rows:
    CHECK(image.sample_nearest_neighbor<WrapMode::Clamp>(0.f, 0.f) == 0.f);
    CHECK(image.sample_nearest_neighbor<WrapMode::Clamp>(1.f, 1.f) == 12.f);
    CHECK(image.sample_nearest_neighbor<WrapMode::Clamp>(0.5f, 0.f) == 1.f);
    CHECK(image.sample_nearest_neighbor<WrapMode::Clamp>(0.24f, 0.6f) == 10.f);

    // Clamp holds coordinates outside [0, 1] at the edges:
    CHECK(image.sample_nearest_neighbor<WrapMode::Clamp>(-0.5f, 2.f) == 10.f);
    CHECK(image.sample_nearest_neighbor<WrapMode::Clamp>(1.5f, -1.f) == 2.f);

    // Repeat (the default) wraps them around:
    CHECK(image.sample_nearest_neighbor(0.5f, 0.f) == 1.f);
    CHECK(image.sample_nearest_neighbor<WrapMode::Repeat>(1.3f, 0.f) == 1.f);
    CHECK(image.sample_nearest_neighbor<WrapMode::Repeat>(-0.1f, 0.f) == 2.f);
    CHECK(image.sample_nearest_neighbor<WrapMode::Repeat>(0.f, 1.9f) == 10.f);

    // Mirror reflects them at each edge:
    CHECK(image.sample_nearest_neighbor<WrapMode::Mirror>(1.1f, 0.f) == 2.f);
    CHECK(image.sample_nearest_neighbor<WrapMode::Mirror>(-0.1f, 0.f) == 0.f);
    CHECK(image.sample_nearest_neighbor<WrapMode::Mirror>(2.1f, 0.f) == 0.f);
    CHECK(image.sample_nearest_neighbor<WrapMode::Mirror>(0.f, 1.2f) == 10.f);

    // An empty image samples as a default pixel:
    CHECK(Image<float>{}.sample_nearest_neighbor(0.5f, 0.5f) == 0.f);
}

TEST_CASE("Bilinear sampling interpolates between the four nearest pixels", "[images][image]")
{
    const Image<float> image = ramp();

    // The ramp is linear, so a bilinear sample at (px, py) = (2 u, v) is exactly 10 py + px:
    CHECK(image.sample_bilinear<WrapMode::Clamp>(0.f, 0.f) == 0.f);
    CHECK(image.sample_bilinear<WrapMode::Clamp>(0.25f, 0.5f) == 5.5f);
    CHECK(image.sample_bilinear<WrapMode::Clamp>(0.75f, 0.25f) == 4.f);
    CHECK(image.sample_bilinear<WrapMode::Clamp>(1.f, 1.f) == 12.f);
    CHECK(image.sample_bilinear<WrapMode::Clamp>(2.f, -1.f) == 2.f);
    CHECK(image.sample_bilinear(0.25f, 0.5f) == 5.5f);
    CHECK(image.sample_bilinear<WrapMode::Repeat>(1.25f, 0.5f) == 5.5f);
    CHECK(image.sample_bilinear<WrapMode::Mirror>(1.25f, -0.5f) == 6.5f);
    CHECK(Image<float>{}.sample_bilinear(0.5f, 0.5f) == 0.f);

    // Integer pixels are interpolated as floats, and truncated back:
    Image<std::int8_t> bytes(2, 1);
    bytes(0, 0) = 0;
    bytes(1, 0) = 100;
    CHECK(bytes.sample_bilinear<WrapMode::Clamp>(0.5f, 0.f) == 50);
    CHECK(bytes.sample_bilinear<WrapMode::Clamp>(0.255f, 0.f) == 25);

    // Vector and spectral pixels are interpolated in every channel:
    Image<Vec3<float>> vectors(2, 1);
    vectors(0, 0) = Vec3<float>{0.f, 0.f, 0.f};
    vectors(1, 0) = Vec3<float>{2.f, 4.f, 6.f};
    CHECK(vectors.sample_bilinear<WrapMode::Clamp>(0.5f, 0.f) == Vec3<float>{1.f, 2.f, 3.f});

    Image<RGB> colors(2, 2, RGB{0.f});
    colors(1, 0) = RGB{4.f, 0.f, 0.f};
    colors(0, 1) = RGB{0.f, 4.f, 0.f};
    colors(1, 1) = RGB{0.f, 0.f, 4.f};
    CHECK(colors.sample_bilinear<WrapMode::Clamp>(0.5f, 0.5f) == RGB{1.f, 1.f, 1.f});
    CHECK(Image<RGB>{}.sample_bilinear(0.5f, 0.5f) == RGB{0.f});
}

TEST_CASE("Bilinear sampling falls back to the nearest pixel for other pixel types",
          "[images][image]")
{
    Image<Vec4<float>> image(2, 2);
    image(0, 0) = Vec4<float>{1.f, 0.f, 0.f, 0.f};
    image(1, 0) = Vec4<float>{0.f, 1.f, 0.f, 0.f};
    image(0, 1) = Vec4<float>{0.f, 0.f, 1.f, 0.f};
    image(1, 1) = Vec4<float>{0.f, 0.f, 0.f, 1.f};

    CHECK(image.sample_bilinear<WrapMode::Clamp>(0.1f, 0.1f) == image(0, 0));
    CHECK(image.sample_bilinear<WrapMode::Clamp>(0.6f, 0.4f) == image(1, 0));
    CHECK(image.sample_bilinear<WrapMode::Clamp>(0.4f, 0.6f) == image(0, 1));
    CHECK(image.sample_bilinear<WrapMode::Clamp>(0.6f, 0.6f) == image(1, 1));
}

TEST_CASE("Copies of an image are independent and keep its sensor bit depth", "[images][image]")
{
    Image<float> image = ramp();
    CHECK(image.sensor_bit_depth() == 0);
    image.set_sensor_bit_depth(12);

    Image<float> copy = image;
    CHECK(copy.sensor_bit_depth() == 12);
    image(0, 0) = 99.f;
    CHECK(copy(0, 0) == 0.f);

    Image<float> assigned;
    assigned = copy;
    CHECK(assigned.resolution() == Resolution{3, 2});
    CHECK(assigned.sensor_bit_depth() == 12);

    Image<float> moved = std::move(copy);
    CHECK(moved.size() == 6);
    CHECK(moved(2, 1) == 12.f);
    Image<float> move_assigned;
    move_assigned = std::move(moved);
    CHECK(move_assigned(1, 1) == 11.f);
    CHECK(move_assigned.sensor_bit_depth() == 12);
}

TEST_CASE("clear(), fill() and reset() change an image's pixels", "[images][image]")
{
    Image<float> image = ramp();
    image.fill(3.f);
    for (std::size_t i = 0; i < image.size(); ++i) {
        CHECK(image[i] == 3.f);
    }
    image.reset();
    for (std::size_t i = 0; i < image.size(); ++i) {
        CHECK(image[i] == 0.f);
    }
    image.reset(-1.f);
    CHECK(image(2, 1) == -1.f);

    image.clear();
    CHECK(image.empty());
    CHECK(image.width() == 0);
    CHECK(image.height() == 0);
    CHECK(image.size() == 0);
}

TEST_CASE("get_channel() extracts one channel of any pixel type", "[images][image]")
{
    const Image<float> mono = ramp();
    const Image<float> channel = mono.get_channel(0);
    CHECK(channel.resolution() == mono.resolution());
    CHECK(channel(2, 1) == 12.f);
    CHECK_THROWS_AS(mono.get_channel(1), std::runtime_error);

    Image<std::int8_t> bytes(1, 1, std::int8_t{-5});
    CHECK(bytes.get_channel(0)(0, 0) == -5.f);

    const Image<Vec3<float>> vectors(2, 1, Vec3<float>{1.f, 2.f, 3.f});
    CHECK(vectors.get_channel(0)(1, 0) == 1.f);
    CHECK(vectors.get_channel(2)(1, 0) == 3.f);
    CHECK_THROWS_AS(vectors.get_channel(3), std::runtime_error);

    Image<RGB> colors(1, 2, RGB{0.25f, 0.5f, 0.75f});
    colors.set_sensor_bit_depth(10);
    CHECK(colors.get_channel(1)(0, 1) == 0.5f);
    CHECK(colors.get_channel(2).sensor_bit_depth() == 10);
    CHECK_THROWS_AS(colors.get_channel(3), std::runtime_error);
}

TEST_CASE("Images of the same resolution add pixel by pixel", "[images][image]")
{
    const Image<float> a = ramp();
    const Image<float> b(3, 2, 0.5f);
    const Image<float> sum = a + b;
    CHECK(sum.resolution() == a.resolution());
    for (std::size_t i = 0; i < sum.size(); ++i) {
        CHECK(sum[i] == a[i] + 0.5f);
    }

    const Image<RGB> colors = Image<RGB>(1, 1, RGB{1.f, 2.f, 3.f}) + Image<RGB>(1, 1, RGB{1.f});
    CHECK(colors(0, 0) == RGB{2.f, 3.f, 4.f});

    CHECK_THROWS_AS(a + Image<float>(2, 3), std::runtime_error);
}

TEST_CASE("Convolving with a single-pixel kernel scales the image", "[images][image]")
{
    Image<float> image = ramp();
    image.convolve(Image<float>(1, 1, 0.5f));
    CHECK(image(0, 0) == 0.f);
    CHECK(image(2, 1) == 6.f);

    Image<RGB> colors(2, 1, RGB{1.f, 2.f, 4.f});
    colors.convolve(Image<RGB>(1, 1, RGB{2.f, 1.f, 0.5f}));
    CHECK(colors(1, 0) == RGB{2.f, 2.f, 2.f});
}

TEST_CASE("An image bundle holds an image with how it is stored in a file", "[images][image]")
{
    const ImageBundle<float> defaults;
    CHECK(defaults.image.empty());
    CHECK(defaults.alpha.empty());
    CHECK(defaults.color_space == ColorSpaceHint::Linear);
    CHECK(defaults.gamma_value == 1.f);
    CHECK(defaults.bit_depth == 8);
    CHECK(defaults.scaling == PixelScaling::Counts);

    ImageBundle<RGB> colors(Image<RGB>(2, 1, RGB{1.f}), Image<float>(2, 1, 0.25f));
    CHECK(colors.image.width() == 2);
    CHECK(colors.alpha(1, 0) == 0.25f);
    colors.color_space = ColorSpaceHint::Gamma;
    colors.gamma_value = 1.8f;
    colors.bit_depth = 16;
    colors.scaling = PixelScaling::FullRange;

    // A bundle of another pixel type takes everything but the image from the original:
    const ImageBundle<float> mono(colors, Image<float>(2, 1, 0.5f));
    CHECK(mono.image(0, 0) == 0.5f);
    CHECK(mono.alpha(1, 0) == 0.25f);
    CHECK(mono.color_space == ColorSpaceHint::Gamma);
    CHECK(mono.gamma_value == 1.8f);
    CHECK(mono.bit_depth == 16);
    CHECK(mono.scaling == PixelScaling::FullRange);
}
