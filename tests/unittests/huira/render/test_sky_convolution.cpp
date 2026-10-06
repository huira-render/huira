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
    Image<RGB> kernel; ///< The camera's convolution kernel, when convolving.
};

/// Renders a 64 x 64 frame of 10 um pixels behind a 50 mm f/8 lens (1/5000 rad per pixel),
/// against a uniform sky, with an Airy PSF and scattered light reaching 32 px. The sky is about
/// as bright as the sunlit ball, so that the convolution's rounding error, which scales with the
/// brightest pixel, is small next to it.
Rendered render(Content content, bool convolve, bool region_culling = true)
{
    Scene<RGB> scene;
    scene.set_background_radiance(200.0f);

    auto camera_model = scene.new_camera_model();
    camera_model.configure_sensor_from_pitch({64, 64}, 10_um);
    camera_model.set_focal_length(50_mm);
    camera_model.set_fstop(8.0f);
    camera_model.use_aperture_psf(8, 4);
    camera_model.set_harvey_shack_scatter(0.2f, 2.5f, 0.5f);
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

    return {total, convolve ? camera_model.get_psf_convolution_kernel() : Image<RGB>(0, 0)};
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
    // The reference is worked out here without the renderer's split of the sky: the frame
    // traced without convolution is set in a larger canvas whose border continues the sky (the
    // sky alone's edge pixels, carried outward), convolved with the camera's own kernel by
    // Image::convolve(), and cropped back. The ball's rim has pixels it covers only partly, and
    // it comes within 4 px of the frame's left edge, so its light also spreads beyond the frame.
    const Image<RGB> sky = render(Content::Nothing, false).power;
    const Image<RGB> traced = render(Content::BallNearEdge, false).power;
    const Rendered convolved = render(Content::BallNearEdge, true);

    constexpr int SIZE = 64;
    constexpr int BORDER = 40; // more than the kernel's 32 px reach
    Image<RGB> canvas(SIZE + 2 * BORDER, SIZE + 2 * BORDER, RGB{0.f});
    for (int y = 0; y < canvas.height(); ++y) {
        for (int x = 0; x < canvas.width(); ++x) {
            const int fx = x - BORDER;
            const int fy = y - BORDER;
            const bool in_frame = fx >= 0 && fx < SIZE && fy >= 0 && fy < SIZE;
            canvas(x, y) = in_frame ? traced(fx, fy)
                                    : sky(std::clamp(fx, 0, SIZE - 1), std::clamp(fy, 0, SIZE - 1));
        }
    }
    canvas.convolve(convolved.kernel);
    Image<RGB> reference(SIZE, SIZE, RGB{0.f});
    for (int y = 0; y < SIZE; ++y) {
        for (int x = 0; x < SIZE; ++x) {
            reference(x, y) = canvas(x + BORDER, y + BORDER);
        }
    }

    CHECK(largest_difference(convolved.power, reference) < 1e-4);

    // The frame convolved on its own, as it was before, is far from it toward the edges:
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
