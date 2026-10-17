#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstddef>
#include <cstdlib>
#include <memory>
#include <new>

#include "catch2/catch_test_macros.hpp"
#include "huira/cameras/psfs/psf_convolver.hpp"
#include "huira/cameras/psfs/psf_tables.hpp"
#include "huira/core/spectral_bins.hpp"
#include "huira/images/image.hpp"

using namespace huira;

namespace {
// While armed, the first allocation of at least fail_size bytes throws std::bad_alloc, as when
// memory runs out part of the way through a build.
std::atomic<bool> armed{false};
std::atomic<std::size_t> fail_size{0};
} // namespace

#if defined(__clang__)
#pragma clang diagnostic push
#if __has_warning("-Wallocator-wrappers")
#pragma clang diagnostic ignored "-Wallocator-wrappers"
#endif
#endif

void* operator new(std::size_t size)
{
    bool expected = true;
    if (size >= fail_size.load() && armed.compare_exchange_strong(expected, false)) {
        throw std::bad_alloc();
    }
    if (void* memory = std::malloc(size > 0 ? size : 1)) {
        return memory;
    }
    throw std::bad_alloc();
}
#if defined(__clang__)
#pragma clang diagnostic pop
#endif

void operator delete(void* memory) noexcept
{
    std::free(memory);
}
void operator delete(void* memory, std::size_t) noexcept
{
    std::free(memory);
}

TEST_CASE("A convolver whose build throws is not taken as built", "[cameras][psf][convolution]")
{
    // The camera rebuilds the convolution of resolved bodies when the convolver is not ready, or
    // was built for other parts. The convolver recorded the parts and called itself ready before
    // building, so a build that threw (out of memory, say) left it ready for the new parts with
    // the old kernels, and the camera used them until some other setting changed.
    const auto tables =
        std::make_shared<const PsfTables<RGB>>(PsfTables<RGB>::airy(3.3, 8.5e-6, 8.5e-6, 400.0));
    const typename PsfConvolver<RGB>::Parts parts{tables, 1.0, nullptr, 0.0};
    const Resolution resolution{64, 48};

    PsfConvolver<RGB> convolver;
    // The near part's kernel, the build's first large allocation, fails:
    fail_size = 100000;
    armed = true;
    CHECK_THROWS_AS(convolver.set_parts(parts, resolution), std::bad_alloc);
    armed = false;
    CHECK_FALSE(convolver.is_ready());

    // Built again, it convolves as one built at once does:
    convolver.set_parts(parts, resolution);
    REQUIRE(convolver.is_ready());
    PsfConvolver<RGB> fresh;
    fresh.set_parts(parts, resolution);
    Image<RGB> image(resolution.width, resolution.height, RGB{0.f});
    image(20, 17) = RGB{1.f};
    Image<RGB> expected = image;
    convolver.apply(image);
    fresh.apply(expected);
    double worst = 0.0;
    for (std::size_t i = 0; i < image.size(); ++i) {
        for (std::size_t c = 0; c < 3; ++c) {
            worst = std::max(worst, std::abs(static_cast<double>(image[i][c] - expected[i][c])));
        }
    }
    CHECK(worst == 0.0);
}
