#include <algorithm>
#include <cmath>
#include <cstddef>

#include "catch2/catch_test_macros.hpp"
#include "huira/huira.hpp"

using namespace huira;
using namespace huira::units::literals;

namespace {

constexpr int SIZE = 48;

struct Setup {
    bool psf = false;     ///< The aperture's Airy pattern, in 13 px stamps.
    bool scatter = false; ///< Harvey-Shack scatter, 10% of the light, out to 16 px.
    bool convolve =
        true; ///< Left at its default when true; enable_psf_convolution(false) when not.
    bool defocus = false; ///< About a 3 px blur.
    bool ball = false;    ///< A sunlit ball 10 px in radius in the middle, instead of a star.
};

/// Renders a 48 x 48 frame of 10 um pixels behind a 50 mm f/8 lens (1/5000 rad per pixel), of
/// an unresolved source near the middle, or a ball there, and returns the received power.
Image<RGB> render(const Setup& setup)
{
    Scene<RGB> scene;
    auto camera_model = scene.new_camera_model();
    camera_model.configure_sensor_from_pitch({SIZE, SIZE}, 10_um);
    camera_model.set_focal_length(50_mm);
    camera_model.set_fstop(8.0f);
    if (setup.psf) {
        camera_model.use_aperture_psf(6, 4);
    }
    if (setup.scatter) {
        camera_model.set_harvey_shack_scatter(0.1f, 2.5f, 0.5f);
        camera_model.set_psf_convolution_radius(16);
    }
    if (!setup.convolve) {
        camera_model.enable_psf_convolution(false);
    }
    if (setup.defocus) {
        camera_model.set_focus_sensor_offset(units::Micrometer(480.0));
    }
    auto camera = scene.root.new_instance(camera_model);

    if (setup.ball) {
        auto geometry = scene.add_ellipsoid(2_cm, 2_cm, 2_cm);
        auto ball = scene.root.new_instance(scene.add_primitive(geometry));
        ball.set_position(0_m, 0_m, 10_m);
        auto sun = scene.root.new_instance(scene.new_sun_light());
        sun.set_position(0_m, 0_m, -1_au);
    } else {
        auto source = scene.root.new_instance(scene.new_unresolved_emitter(units::Watt(1.0)));
        source.set_position(units::Meter(0.3 / 5000.0 * 1000.0),
                            units::Meter(0.2 / 5000.0 * 1000.0),
                            units::Meter(1000.0));
    }

    auto frame_buffer = camera_model.make_frame_buffer();
    frame_buffer.enable_received_power();
    Renderer<RGB> renderer;
    renderer.set_samples_per_pixel(2);
    Interval exposure{Time::from_et(0.0), Time::from_et(0.001)};
    SceneView<RGB> scene_view(scene, exposure, camera, ObservationMode::GEOMETRIC_STATE);
    renderer.render(scene_view, frame_buffer);
    return frame_buffer.received_power();
}

bool identical(const Image<RGB>& a, const Image<RGB>& b)
{
    for (std::size_t i = 0; i < a.size(); ++i) {
        for (std::size_t c = 0; c < 3; ++c) {
            if (a[i][c] != b[i][c]) {
                return false;
            }
        }
    }
    return a.size() == b.size();
}

/// Power in the green channel over the pixels whose centers are between the given distances
/// from the middle of the frame.
double power_between(const Image<RGB>& image, double inner, double outer)
{
    double total = 0.0;
    for (int y = 0; y < image.height(); ++y) {
        for (int x = 0; x < image.width(); ++x) {
            const double r = std::hypot(x + 0.5 - SIZE / 2.0, y + 0.5 - SIZE / 2.0);
            if (r >= inner && r < outer) {
                total += static_cast<double>(image(x, y)[1]);
            }
        }
    }
    return total;
}

} // namespace

TEST_CASE("Scattered light reaches stars whether or not bodies are convolved", "[render][psf]")
{
    // The wings used to need enable_psf_convolution() too. The Airy stamps reach 6 px; beyond
    // 8 px there is only scattered light.
    const Image<RGB> on = render({true, true, true});
    const Image<RGB> off = render({true, true, false});
    CHECK(identical(on, off));
    CHECK(power_between(off, 8.0, SIZE) > 0.01 * power_between(off, 0.0, SIZE));
}

TEST_CASE("Out of focus, stars are blurred by the PSF whether or not bodies are convolved",
          "[render][psf]")
{
    // The defocus disk is blurred by the PSF and scattered light through the convolution kernel,
    // which used to need enable_psf_convolution() too.
    const Image<RGB> with_psf = render({true, false, false, true});
    CHECK(identical(with_psf, render({true, false, true, true})));
    CHECK_FALSE(identical(with_psf, render({false, false, false, true})));
}

TEST_CASE("A PSF blurs resolved bodies by default", "[render][psf]")
{
    // Against a black sky the sharp ball, 10 px in radius, lights nothing beyond 11 px. Its
    // light reaches further only through the PSF, which used to need enable_psf_convolution().
    const Image<RGB> sharp = render({true, false, false, false, true});
    const Image<RGB> by_default = render({true, false, true, false, true});
    REQUIRE(power_between(sharp, 0.0, 10.0) > 0.0);
    CHECK(power_between(sharp, 11.0, SIZE) == 0.0);
    CHECK(power_between(by_default, 11.0, SIZE) > 0.0);

    // With neither a PSF nor scattered light, there is nothing to convolve with:
    CHECK(identical(render({false, false, true, false, true}),
                    render({false, false, false, false, true})));
}
