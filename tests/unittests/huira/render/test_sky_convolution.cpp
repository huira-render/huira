#include <algorithm>
#include <cmath>
#include <cstddef>

#include "catch2/catch_test_macros.hpp"
#include "huira/huira.hpp"

using namespace huira;
using namespace huira::units::literals;

namespace {

enum class Content {
    Nothing,      ///< No geometry at all.
    BallBehind,   ///< A ball behind the camera: geometry, but none in view.
    BallNearEdge, ///< A sunlit ball 10 px in radius, reaching to 4 px from the left edge.
};

struct Rendered {
    Image<RGB> power;  ///< Received power.
    Image<RGB> kernel; ///< The camera's convolution kernel, reaching 32 px, when convolving.
    Image<RGB> psf;    ///< The PSF of a source at a pixel's center, to 63 px, when convolving.
};

constexpr int SIZE = 64;

/// Renders a 64 x 64 frame of 10 um pixels behind a 50 mm f/8 lens (1/5000 rad per pixel),
/// against a uniform sky, with an Airy PSF and scattered light. The sky is about as bright as the
/// sunlit ball.
Rendered render(Content content, bool convolve, bool region_culling = true)
{
    Scene<RGB> scene;
    scene.set_background_radiance(200.0f);

    auto camera_model = scene.new_camera_model();
    camera_model.configure_sensor_from_pitch({SIZE, SIZE}, 10_um);
    camera_model.set_focal_length(50_mm);
    camera_model.set_fstop(8.0f);
    camera_model.use_aperture_psf(8, 4);
    camera_model.set_scatter(0.2f, 2.5f, units::Radian(0.5 / 5000.0));
    camera_model.set_psf_convolution_radius(32);
    camera_model.enable_psf_convolution(convolve);
    auto camera = scene.root.new_instance(camera_model);

    if (content != Content::Nothing) {
        // 2 cm at 10 m is 10 px. Near the edge, its center is 14 px from the left edge.
        auto geometry = scene.add_ellipsoid(2_cm, 2_cm, 2_cm);
        auto ball = scene.root.new_instance(scene.add_primitive(geometry));
        if (content == Content::BallBehind) {
            ball.set_position(0_m, 0_m, -10_m);
        } else {
            ball.set_position(units::Meter((14.0 - 32.0) / 5000.0 * 10.0), 0_m, 10_m);
        }

        // A white ball would reflect the uniform sky's own radiance, and vanish against it, so
        // the Sun, behind the camera, lights it:
        auto sun = scene.root.new_instance(scene.new_sun_light());
        sun.set_position(0_m, 0_m, -1_au);
    }

    auto frame_buffer = camera_model.make_frame_buffer();
    frame_buffer.enable_received_power();
    frame_buffer.enable_received_direct_power();
    frame_buffer.enable_received_indirect_power();
    Renderer<RGB> renderer;
    renderer.set_samples_per_pixel(2);
    renderer.set_region_culling(region_culling);
    Interval exposure{Time::from_et(0.0), Time::from_et(0.001)};
    SceneView<RGB> scene_view(scene, exposure, camera, ObservationMode::GEOMETRIC_STATE);
    renderer.render(scene_view, frame_buffer);

    // The total is the sum of its parts, with or without the sky split off:
    const Image<RGB>& total = frame_buffer.received_power();
    const Image<RGB>& direct = frame_buffer.received_direct_power();
    const Image<RGB>& indirect = frame_buffer.received_indirect_power();
    double brightest = 0.0;
    double worst = 0.0;
    for (std::size_t i = 0; i < total.size(); ++i) {
        brightest = std::max(brightest, static_cast<double>(total[i][1]));
        worst = std::max(
            worst, std::abs(static_cast<double>(total[i][1] - direct[i][1] - indirect[i][1])));
    }
    CHECK(worst <= 1e-5 * brightest);

    if (!convolve) {
        return {total, Image<RGB>(0, 0), Image<RGB>(0, 0)};
    }
    return {total, camera_model.get_psf_convolution_kernel(), camera_model.psf_image(SIZE - 1)};
}

/// Largest relative difference between two images, in the green channel.
double largest_difference(const Image<RGB>& image, const Image<RGB>& reference)
{
    double worst = 0.0;
    for (int y = 0; y < image.height(); ++y) {
        for (int x = 0; x < image.width(); ++x) {
            const double r = static_cast<double>(reference(x, y)[1]);
            worst = std::max(worst, std::abs(static_cast<double>(image(x, y)[1]) - r) / r);
        }
    }
    return worst;
}

} // namespace

TEST_CASE("The PSF convolution leaves a uniform sky as it is", "[render][psf]")
{
    // With nothing at all in the scene, the frame is the camera's response to the sky alone.
    const Image<RGB> sky = render(Content::Nothing, true).power;

    // Geometry out of view used to send the frame through the convolution, which cannot see the
    // sky beyond the frame, and darkened the sky toward the edges: by 18% in the corners.
    for (bool region_culling : {true, false}) {
        INFO("region culling " << region_culling);
        const Image<RGB> image = render(Content::BallBehind, true, region_culling).power;
        CHECK(largest_difference(image, sky) == 0.0);
    }
}

TEST_CASE("The frame is convolved as if a uniform sky continued beyond its edges", "[render][psf]")
{
    // A sky that goes on beyond the frame is its own image convolved, since the PSF holds all of
    // a source's light: so the reference is the sky alone, and the ball's light less the sky it
    // hides, spread by the PSF over the whole frame. The ball's rim has pixels it covers only
    // partly, and it comes within 4 px of the frame's left edge, so its light also spreads
    // beyond the frame.
    const Image<RGB> sky = render(Content::Nothing, false).power;
    const Image<RGB> traced = render(Content::BallNearEdge, false).power;
    const Rendered convolved = render(Content::BallNearEdge, true);

    Image<RGB> reference = sky;
    const int center = SIZE - 1;
    for (int sy = 0; sy < SIZE; ++sy) {
        for (int sx = 0; sx < SIZE; ++sx) {
            const RGB ball = traced(sx, sy) - sky(sx, sy);
            if (ball.max() == 0.f && ball.min() == 0.f) {
                continue;
            }
            for (int y = 0; y < SIZE; ++y) {
                for (int x = 0; x < SIZE; ++x) {
                    reference(x, y) += ball * convolved.psf(center + x - sx, center + y - sy);
                }
            }
        }
    }

    CHECK(largest_difference(convolved.power, reference) < 1e-4);

    // The frame convolved on its own, as it was before the sky was split off, is far from it
    // toward the edges:
    Image<RGB> alone = traced;
    alone.convolve(convolved.kernel);
    CHECK(largest_difference(alone, reference) > 0.1);
}

TEST_CASE("A background image is convolved even with nothing else in view", "[render][psf]")
{
    // A 5 x 5 texel bright patch, 2.5 degrees across, straight ahead in a 0.5 degree
    // equirectangular map, seen by a wide camera (0.57 degrees per pixel) at f/64, whose Airy
    // pattern's first dark ring is 4.3 px out. Only a uniform sky is split off.
    auto render_patch = [](bool convolve) {
        Scene<RGB> scene;
        Image<RGB> background(720, 360, RGB{0.f});
        for (int y = 177; y <= 182; ++y) {
            for (int x = 537; x <= 542; ++x) {
                background(x, y) = RGB{1.f};
            }
        }
        scene.set_background_radiance(std::move(background));

        auto camera_model = scene.new_camera_model();
        camera_model.configure_sensor_from_pitch({64, 64}, 10_um);
        camera_model.set_focal_length(1_mm);
        camera_model.set_fstop(64.0f);
        camera_model.use_aperture_psf(12, 4);
        camera_model.enable_psf_convolution(convolve);
        auto camera = scene.root.new_instance(camera_model);

        auto frame_buffer = camera_model.make_frame_buffer();
        frame_buffer.enable_received_power();
        Renderer<RGB> renderer;
        renderer.set_samples_per_pixel(4);
        Interval exposure{Time::from_et(0.0), Time::from_et(0.001)};
        SceneView<RGB> scene_view(scene, exposure, camera, ObservationMode::GEOMETRIC_STATE);
        renderer.render(scene_view, frame_buffer);
        return frame_buffer.received_power();
    };

    auto peak = [](const Image<RGB>& image) {
        double brightest = 0.0;
        for (std::size_t i = 0; i < image.size(); ++i) {
            brightest = std::max(brightest, static_cast<double>(image[i][1]));
        }
        return brightest;
    };

    const double sharp = peak(render_patch(false));
    const double blurred = peak(render_patch(true));
    INFO("peak " << sharp << " without the convolution, " << blurred << " with it");
    REQUIRE(sharp > 0.0);
    CHECK(blurred < 0.9 * sharp);
}
