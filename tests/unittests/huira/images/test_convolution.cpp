#include <cmath>
#include <random>

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
        using Spec8 = SpectralBins<8, 400.0, 800.0>;
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
