#include <algorithm>
#include <atomic>
#include <cstddef>
#include <cstdlib>
#include <new>

#include "catch2/catch_test_macros.hpp"
#include "huira/huira.hpp"

using namespace huira;
using namespace huira::units::literals;

#if defined(__clang__) && __has_warning("-Wallocator-wrappers")
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wallocator-wrappers"
#endif

// This test executable counts the memory allocated with new, so that a test can find the most a
// piece of code had allocated at once. Each block carries its size in a header in front of it.
namespace {

std::atomic<std::size_t> live_bytes{0};
std::atomic<std::size_t> peak_bytes{0};

constexpr std::size_t HEADER = alignof(std::max_align_t);

void* counted_allocate(std::size_t size) noexcept
{
    void* block = std::malloc(size + HEADER);
    if (block == nullptr) {
        return nullptr;
    }
    *static_cast<std::size_t*>(block) = size;
    const std::size_t live = live_bytes.fetch_add(size) + size;
    std::size_t peak = peak_bytes.load();
    while (live > peak && !peak_bytes.compare_exchange_weak(peak, live)) {
    }
    return static_cast<std::byte*>(block) + HEADER;
}

void counted_free(void* pointer) noexcept
{
    if (pointer == nullptr) {
        return;
    }
    void* block = static_cast<std::byte*>(pointer) - HEADER;
    live_bytes.fetch_sub(*static_cast<std::size_t*>(block));
    std::free(block);
}

/// The most allocated at once while f runs, beyond what was allocated before it.
template <typename F>
std::size_t peak_allocated_during(F&& f)
{
    const std::size_t before = live_bytes.load();
    peak_bytes.store(before);
    f();
    return peak_bytes.load() - before;
}

} // namespace

void* operator new(std::size_t size)
{
    void* pointer = counted_allocate(size);
    if (pointer == nullptr) {
        throw std::bad_alloc();
    }
    return pointer;
}
void* operator new[](std::size_t size)
{
    return operator new(size);
}
void* operator new(std::size_t size, const std::nothrow_t&) noexcept
{
    return counted_allocate(size);
}
void* operator new[](std::size_t size, const std::nothrow_t&) noexcept
{
    return counted_allocate(size);
}
void operator delete(void* pointer) noexcept
{
    counted_free(pointer);
}
void operator delete[](void* pointer) noexcept
{
    counted_free(pointer);
}
void operator delete(void* pointer, std::size_t) noexcept
{
    counted_free(pointer);
}
void operator delete[](void* pointer, std::size_t) noexcept
{
    counted_free(pointer);
}
void operator delete(void* pointer, const std::nothrow_t&) noexcept
{
    counted_free(pointer);
}
void operator delete[](void* pointer, const std::nothrow_t&) noexcept
{
    counted_free(pointer);
}

TEST_CASE("Rendering unresolved sources takes memory for the frame, whatever their stamps' size",
          "[render][unresolved]")
{
    // Unresolved sources are rendered in 64 px tiles. Before, each tile with a source in it had
    // its own buffer, enlarged by the stamp's radius on every side, so that memory grew as
    // tiles x (64 + 2 radius)^2 pixels: rendering this frame took 8.6 frames' worth, where it
    // now takes 3, and a 4096 x 4096 RGB frame with 64 px stamps took 2 GiB more.
    constexpr int SIZE = 512;
    Scene<RGB> scene;
    auto camera_model = scene.new_camera_model();
    camera_model.configure_sensor_from_pitch({SIZE, SIZE}, 10_um);
    camera_model.set_focal_length(50_mm);
    camera_model.set_fstop(4.0f);
    camera_model.set_focus_sensor_offset(units::Micrometer(4800.0)); // a 55 px blur radius
    auto camera = scene.root.new_instance(camera_model);

    // A source in the middle of each tile, 5000 px from the optical axis per radian:
    auto emitter = scene.new_unresolved_emitter(units::Watt(1.0));
    constexpr double RANGE = 1000.0;
    for (int ty = 0; ty < SIZE / 64; ++ty) {
        for (int tx = 0; tx < SIZE / 64; ++tx) {
            auto source = scene.root.new_instance(emitter);
            source.set_position(units::Meter((tx * 64 + 32.5 - SIZE / 2) / 5000.0 * RANGE),
                                units::Meter((ty * 64 + 32.5 - SIZE / 2) / 5000.0 * RANGE),
                                units::Meter(RANGE));
        }
    }

    auto frame_buffer = camera_model.make_frame_buffer();
    frame_buffer.enable_received_power();
    Renderer<RGB> renderer;
    Interval exposure{Time::from_et(0.0), Time::from_et(0.001)};
    auto render = [&] {
        SceneView<RGB> scene_view(scene, exposure, camera, ObservationMode::GEOMETRIC_STATE);
        renderer.render(scene_view, frame_buffer);
    };

    // The first render builds the stamps, which the camera keeps:
    render();
    REQUIRE(camera_model.defocus_blur_radius() > 50.f);

    const std::size_t frame_bytes = std::size_t{SIZE} * SIZE * sizeof(RGB);
    const std::size_t peak = peak_allocated_during(render);
    INFO("peak allocation while rendering: "
         << static_cast<double>(peak) / static_cast<double>(frame_bytes) << " frames");
    CHECK(peak < 4 * frame_bytes);
}

#if defined(__clang__) && __has_warning("-Wallocator-wrappers")
#pragma clang diagnostic pop
#endif
