#include <algorithm>
#include <cmath>
#include <random>
#include <stdexcept>

#include "catch2/catch_test_macros.hpp"
#include "huira/core/spectral_bins.hpp"
#include "huira/images/fft_convolver.hpp"
#include "huira/images/image.hpp"

using namespace huira;

namespace {

// Reference implementation: true convolution with zero boundary and double accumulation.
Image<float> reference_convolve(const Image<float>& img, const Image<float>& kernel)
{
    Image<float> out(img.width(), img.height(), 0.0f);
    const int kcx = kernel.width() / 2;
    const int kcy = kernel.height() / 2;
    for (int y = 0; y < img.height(); ++y) {
        for (int x = 0; x < img.width(); ++x) {
            double sum = 0.0;
            for (int ky = 0; ky < kernel.height(); ++ky) {
                const int sy = y - (ky - kcy);
                if (sy < 0 || sy >= img.height()) {
                    continue;
                }
                for (int kx = 0; kx < kernel.width(); ++kx) {
                    const int sx = x - (kx - kcx);
                    if (sx < 0 || sx >= img.width()) {
                        continue;
                    }
                    sum += static_cast<double>(img(sx, sy)) * static_cast<double>(kernel(kx, ky));
                }
            }
            out(x, y) = static_cast<float>(sum);
        }
    }
    return out;
}

float max_abs_error(const Image<float>& a, const Image<float>& b)
{
    float max_err = 0.0f;
    for (int y = 0; y < a.height(); ++y) {
        for (int x = 0; x < a.width(); ++x) {
            max_err = std::max(max_err, std::fabs(a(x, y) - b(x, y)));
        }
    }
    return max_err;
}

Image<float> random_image(int w, int h, std::mt19937& rng)
{
    std::uniform_real_distribution<float> dist(0.0f, 1.0f);
    Image<float> img(w, h, 0.0f);
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            img(x, y) = dist(rng);
        }
    }
    return img;
}
} // namespace

TEST_CASE("FftConvolver matches reference convolution", "[images][convolution]")
{
    std::mt19937 rng(42);

    SECTION("Random asymmetric kernel")
    {
        Image<float> img = random_image(64, 48, rng);
        Image<float> kernel = random_image(9, 7, rng);

        Image<float> ref = reference_convolve(img, kernel);
        Image<float> fft = img;
        FftConvolver<float> convolver;
        convolver.set_kernel(kernel, fft.resolution());
        convolver.apply(fft);

        REQUIRE(max_abs_error(ref, fft) < 1e-4f);
    }

    SECTION("Kernel larger than the image (full-frame bloom support)")
    {
        Image<float> img(33, 21, 0.0f);
        img(5, 5) = 3.0f;

        Image<float> kernel(101, 77, 0.0f);
        double kernel_sum = 0.0;
        for (int y = 0; y < kernel.height(); ++y) {
            for (int x = 0; x < kernel.width(); ++x) {
                const float dx = static_cast<float>(x - 50);
                const float dy = static_cast<float>(y - 38);
                kernel(x, y) = 1.0f / (1.0f + dx * dx + dy * dy);
                kernel_sum += kernel(x, y);
            }
        }
        for (int y = 0; y < kernel.height(); ++y) {
            for (int x = 0; x < kernel.width(); ++x) {
                kernel(x, y) = static_cast<float>(kernel(x, y) / kernel_sum);
            }
        }

        Image<float> ref = reference_convolve(img, kernel);
        Image<float> fft = img;
        FftConvolver<float> convolver;
        convolver.set_kernel(kernel, fft.resolution());
        convolver.apply(fft);

        REQUIRE(max_abs_error(ref, fft) < 1e-5f);
    }

    SECTION("Spectral pixel type applies each channel independently")
    {
        using Spec8 = SpectralBins<8, 400, 800>;
        std::uniform_real_distribution<float> dist(0.0f, 1.0f);

        Image<Spec8> img(31, 29, Spec8{0.0f});
        img(10, 12) = Spec8{2.0f};
        Image<Spec8> kernel(11, 9, Spec8{0.0f});
        for (int y = 0; y < kernel.height(); ++y) {
            for (int x = 0; x < kernel.width(); ++x) {
                for (std::size_t c = 0; c < 8; ++c) {
                    kernel(x, y)[c] = dist(rng) * (1.0f + static_cast<float>(c));
                }
            }
        }

        Image<Spec8> fft = img;
        FftConvolver<Spec8> convolver;
        convolver.set_kernel(kernel, fft.resolution());
        convolver.apply(fft);

        for (std::size_t c = 0; c < 8; ++c) {
            Image<float> ref = reference_convolve(img.get_channel(c), kernel.get_channel(c));
            REQUIRE(max_abs_error(ref, fft.get_channel(c)) < 1e-4f);
        }
    }

    SECTION("Reusing a convolver across multiple frames")
    {
        Image<float> kernel = random_image(15, 15, rng);
        FftConvolver<float> convolver;
        convolver.set_kernel(kernel, Resolution{40, 40});

        for (int frame = 0; frame < 3; ++frame) {
            Image<float> img = random_image(40, 40, rng);
            Image<float> ref = reference_convolve(img, kernel);
            convolver.apply(img);
            REQUIRE(max_abs_error(ref, img) < 1e-4f);
        }
    }
}

TEST_CASE("Image::convolve paths agree with each other", "[images][convolution]")
{
    std::mt19937 rng(7);

    SECTION("Direct path (small kernel) matches reference")
    {
        Image<float> img = random_image(40, 40, rng);
        Image<float> kernel = random_image(5, 5, rng); // 25 <= 25 -> direct path

        Image<float> ref = reference_convolve(img, kernel);
        Image<float> got = img;
        got.convolve(kernel);
        REQUIRE(max_abs_error(ref, got) < 1e-4f);
    }

    SECTION("Direct and FFT paths shift an asymmetric kernel identically")
    {
        // Regression test: convolve_direct_ previously computed correlation (no kernel
        // flip) while the FFT path computed true convolution, so the same asymmetric
        // kernel shifted the image in opposite directions depending on kernel size.
        Image<float> direct_img(32, 32, 0.0f);
        direct_img(16, 16) = 1.0f;
        Image<float> fft_img = direct_img;

        Image<float> small_kernel(5, 5, 0.0f);
        small_kernel(1, 2) = 1.0f; // one pixel left of center

        Image<float> padded_kernel(7, 7, 0.0f);
        padded_kernel(2, 3) = 1.0f; // same offset; 49 > 25 forces the FFT path

        direct_img.convolve(small_kernel);
        fft_img.convolve(padded_kernel);

        REQUIRE(direct_img(15, 16) > 0.5f);
        REQUIRE(fft_img(15, 16) > 0.5f);
    }
}

TEST_CASE("FftConvolver::next_fast_size returns 7-smooth sizes", "[images][convolution]")
{
    REQUIRE(FftConvolver<float>::next_fast_size(1) == 1);
    REQUIRE(FftConvolver<float>::next_fast_size(2048) == 2048);
    REQUIRE(FftConvolver<float>::next_fast_size(2559) == 2560); // 2^9 * 5
    REQUIRE(FftConvolver<float>::next_fast_size(3072) == 3072); // 2^10 * 3
    REQUIRE(FftConvolver<float>::next_fast_size(3079) == 3087); // 3^2 * 7^3
    REQUIRE(FftConvolver<float>::next_fast_size(8191) == 8192); // avoids a prime size
}

TEST_CASE("FftConvolver in single precision rounds no more than rounding_error() says",
          "[images][convolution]")
{
    // A normalized kernel with power-law wings, and frames whose brightest light is a point, a
    // disc or half the frame. Where the frame convolved in double precision is dark, single
    // precision may be off only by its rounding, which rounding_error() bounds. Before, there
    // was no single precision.
    constexpr int WIDTH = 256;
    constexpr int HEIGHT = 192;
    constexpr int RADIUS = 40;
    Image<double> kernel(2 * RADIUS + 1, 2 * RADIUS + 1, 0.0);
    double sum = 0.0;
    for (int y = 0; y < kernel.height(); ++y) {
        for (int x = 0; x < kernel.width(); ++x) {
            const double r2 = (x - RADIUS) * (x - RADIUS) + (y - RADIUS) * (y - RADIUS);
            kernel(x, y) = std::pow(1.0 + r2 / 0.64, -1.5);
            sum += kernel(x, y);
        }
    }
    for (std::size_t i = 0; i < kernel.size(); ++i) {
        kernel[i] /= sum;
    }
    FftConvolver<double> convolver;
    convolver.set_kernel(kernel, Resolution{WIDTH, HEIGHT});

    for (int shape = 0; shape < 3; ++shape) {
        Image<double> frame(WIDTH, HEIGHT, 0.0);
        for (int y = 0; y < HEIGHT; ++y) {
            for (int x = 0; x < WIDTH; ++x) {
                if ((shape == 0 && x == 70 && y == 90) ||
                    (shape == 1 && std::hypot(x - 128.0, y - 96.0) < 48.0) ||
                    (shape == 2 && x < WIDTH / 2)) {
                    frame(x, y) = shape == 0 ? 1e6 : 1e3 * (1.0 + 0.5 * std::sin(0.1 * x + y));
                }
            }
        }
        Image<double> single = frame;
        Image<double> twice = frame;
        convolver.apply(single, FftPrecision::Single);
        convolver.apply(twice, FftPrecision::Double);
        double peak = 0.0;
        for (std::size_t i = 0; i < twice.size(); ++i) {
            peak = std::max(peak, std::abs(twice[i]));
        }
        double worst = 0.0;
        for (std::size_t i = 0; i < twice.size(); ++i) {
            if (std::abs(twice[i]) < 1e-5 * peak) {
                worst = std::max(worst, std::abs(single[i] - twice[i]));
            }
        }
        const double bound = convolver.rounding_error(frame, FftPrecision::Single)[0];
        INFO("shape " << shape << ": off by " << worst << " where dark, against " << bound);
        CHECK(worst > 0.0);
        CHECK(worst <= bound);
        CHECK(convolver.rounding_error(frame, FftPrecision::Double)[0] < 1e-8 * bound);
    }

    // Single precision's spectra and buffers take half the memory:
    FftConvolver<double> single_only;
    single_only.set_kernel(kernel, Resolution{WIDTH, HEIGHT});
    Image<double> frame(WIDTH, HEIGHT, 1.0);
    single_only.apply(frame, FftPrecision::Single);
    const std::size_t single_bytes = single_only.memory_bytes();
    single_only.apply(frame, FftPrecision::Double);
    CHECK(single_only.memory_bytes() == 3 * single_bytes);
}

TEST_CASE("FftConvolver rejects a kernel or image it can't use", "[images][convolution]")
{
    FftConvolver<float> convolver;
    CHECK_FALSE(convolver.is_ready());
    CHECK(convolver.memory_bytes() == 0);

    Image<float> image(8, 8, 1.f);
    CHECK_THROWS_AS(convolver.apply(image), std::runtime_error);
    CHECK_THROWS_AS(convolver.set_kernel(Image<float>{}, Resolution{8, 8}), std::runtime_error);
    CHECK_THROWS_AS(convolver.set_kernel(Image<float>(3, 3, 1.f), Resolution{0, 8}),
                    std::runtime_error);
    CHECK_FALSE(convolver.is_ready());

    convolver.set_kernel(Image<float>(7, 7, 1.f / 49.f), Resolution{8, 8});
    CHECK(convolver.is_ready());
    CHECK(convolver.image_resolution() == Resolution{8, 8});
    Image<float> other(8, 4, 1.f);
    CHECK_THROWS_AS(convolver.apply(other), std::runtime_error);

    // A failed set_kernel() leaves it unready, rather than with the old kernel:
    CHECK_THROWS_AS(convolver.set_kernel(Image<float>(0, 3), Resolution{8, 8}), std::runtime_error);
    CHECK_FALSE(convolver.is_ready());
}
