#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <limits>

#include "catch2/catch_test_macros.hpp"
#include "huira/huira.hpp"

using namespace huira;
using namespace huira::units::literals;

namespace {

/// The Jupiter example's optics, 50 mm at f/3.3 with 8.5 um pixels, on a 64 x 64 sensor whose
/// pixel edges are at integers, so that sensor coordinates and pixel coordinates agree.
constexpr int SIZE = 64;
constexpr double FOCAL_PIXELS = 50e-3 / 8.5e-6;
constexpr double RANGE = 1e6;

CameraModelHandle<RGB> make_camera(Scene<RGB>& scene)
{
    auto camera_model = scene.new_camera_model();
    camera_model.set_pixel_convention(PixelConvention::colmap());
    camera_model.configure_sensor_from_pitch({SIZE, SIZE}, 8.5_um);
    camera_model.set_focal_length(50_mm);
    camera_model.set_fstop(3.3f);
    return camera_model;
}

/// A source this bright stays above the taper over the whole frame, so is drawn in full; one
/// this faint fades out a few pixels away. The faint one gives about 40 electrons.
constexpr double BRIGHT = 1e12;
constexpr double FAINT = 1.0;

/// Renders one still unresolved source at a position on the sensor, with stamps uncropped unless
/// asked.
Image<RGB> render_at(Scene<RGB>& scene,
                     CameraModelHandle<RGB>& camera_model,
                     const InstanceHandle<RGB>& camera,
                     const InstanceHandle<RGB>& source,
                     double x,
                     double y,
                     double taper = Renderer<RGB>::DEFAULT_UNRESOLVED_TAPER,
                     bool crop = false)
{
    const double axis = SIZE / 2.0;
    source.set_position(units::Meter((x - axis) / FOCAL_PIXELS * RANGE),
                        units::Meter((y - axis) / FOCAL_PIXELS * RANGE),
                        units::Meter(RANGE));
    auto frame_buffer = camera_model.make_frame_buffer();
    frame_buffer.enable_received_power();
    Renderer<RGB> renderer;
    renderer.set_stamp_cropping(crop);
    renderer.set_unresolved_taper(taper);
    Interval exposure{Time::from_et(0.0), Time::from_et(1.0)};
    SceneView<RGB> scene_view(scene, exposure, camera, ObservationMode::GEOMETRIC_STATE);
    renderer.render(scene_view, frame_buffer);
    return frame_buffer.received_power();
}

/// Each channel's sum.
std::array<double, 3> sums(const Image<RGB>& image)
{
    std::array<double, 3> sum{};
    for (std::size_t i = 0; i < image.size(); ++i) {
        for (std::size_t c = 0; c < 3; ++c) {
            sum[c] += static_cast<double>(image[i][c]);
        }
    }
    return sum;
}

} // namespace

TEST_CASE("A still source is drawn from the PSF's tables at its exact position",
          "[render][unresolved][psf]")
{
    Scene<RGB> scene;
    auto camera_model = make_camera(scene);
    auto camera = scene.root.new_instance(camera_model);
    auto source = scene.root.new_instance(scene.new_unresolved_emitter(units::Watt(BRIGHT)));
    const int radius = camera_model.get_psf_radius();

    // Positions across a pixel, including just either side of its edges, where a PSF core
    // smaller than a pixel moves its centroid fastest. Stamps at sixteenths of a pixel put these
    // up to 1/32 px off, and the centroid up to four times as far.
    for (const auto& [x, y] : {std::array<double, 2>{32.5, 32.5},
                               {32.03, 31.98},
                               {31.97, 32.27},
                               {32.31, 32.66},
                               {32.53, 32.02}}) {
        INFO("source at (" << x << ", " << y << ")");
        const Image<RGB> image = render_at(scene, camera_model, camera, source, x, y);

        // The rendered stamp is the PSF's tables around the source, as psf_image() gives them,
        // to a scale. Its center pixel is the one the source falls in.
        const int base_x = static_cast<int>(std::floor(x));
        const int base_y = static_cast<int>(std::floor(y));
        const Image<RGB> expected = camera_model.psf_image(
            radius, static_cast<float>(x - base_x - 0.5), static_cast<float>(y - base_y - 0.5));
        std::array<double, 3> rendered_sum{};
        for (int j = -radius; j <= radius; ++j) {
            for (int i = -radius; i <= radius; ++i) {
                for (std::size_t c = 0; c < 3; ++c) {
                    rendered_sum[c] += static_cast<double>(image(base_x + i, base_y + j)[c]);
                }
            }
        }
        const std::array<double, 3> expected_sum = sums(expected);
        for (std::size_t c = 0; c < 3; ++c) {
            double worst = 0.0;
            double peak = 0.0;
            std::array<double, 2> rendered_moment{};
            std::array<double, 2> expected_moment{};
            for (int j = -radius; j <= radius; ++j) {
                for (int i = -radius; i <= radius; ++i) {
                    const double a =
                        static_cast<double>(image(base_x + i, base_y + j)[c]) / rendered_sum[c];
                    const double b =
                        static_cast<double>(expected(i + radius, j + radius)[c]) / expected_sum[c];
                    worst = std::max(worst, std::abs(a - b));
                    peak = std::max(peak, b);
                    rendered_moment = {rendered_moment[0] + a * i, rendered_moment[1] + a * j};
                    expected_moment = {expected_moment[0] + b * i, expected_moment[1] + b * j};
                }
            }
            // The renderer projects in single precision, which moves the source by about 2e-6 px.
            INFO("channel " << c);
            CHECK(worst < 1e-4 * peak);
            CHECK(std::abs(rendered_moment[0] - expected_moment[0]) < 1e-5);
            CHECK(std::abs(rendered_moment[1] - expected_moment[1]) < 1e-5);
        }
    }
}

TEST_CASE("Moving sources are drawn from stamps made from the PSF's tables",
          "[render][unresolved][psf]")
{
    Scene<RGB> scene;
    auto camera_model = make_camera(scene);
    camera_model.use_aperture_psf(8, 4);

    // Bank (bx, by) holds a source bx / 4, by / 4 of a pixel from the center pixel's center,
    // normalized per channel:
    for (const auto& [bx, by] : {std::array<int, 2>{0, 0}, {1, 3}, {3, 2}}) {
        const float u = static_cast<float>(bx) / 4.f;
        const float v = static_cast<float>(by) / 4.f;
        const Image<RGB>& stamp = camera_model.get_psf_kernel(u + 0.125f, v + 0.125f);
        const Image<RGB> expected = camera_model.psf_image(8, u, v);
        const std::array<double, 3> expected_sum = sums(expected);
        REQUIRE(stamp.width() == expected.width());
        double worst = 0.0;
        for (int y = 0; y < stamp.height(); ++y) {
            for (int x = 0; x < stamp.width(); ++x) {
                for (std::size_t c = 0; c < 3; ++c) {
                    worst = std::max(
                        worst,
                        std::abs(static_cast<double>(stamp(x, y)[c]) -
                                 static_cast<double>(expected(x, y)[c]) / expected_sum[c]));
                }
            }
        }
        INFO("bank (" << bx << ", " << by << ")");
        CHECK(worst < 1e-6);
    }
}

namespace {

/// Renders one still source of a power, alone, in a scene of its own.
Image<RGB> render_alone(double power,
                        double x,
                        double y,
                        double taper = Renderer<RGB>::DEFAULT_UNRESOLVED_TAPER,
                        float read_noise = -1.0f,
                        bool crop = false)
{
    Scene<RGB> scene;
    auto camera_model = make_camera(scene);
    if (read_noise >= 0.0f) {
        camera_model.set_sensor_read_noise(read_noise);
    }
    auto camera = scene.root.new_instance(camera_model);
    auto source = scene.root.new_instance(scene.new_unresolved_emitter(units::Watt(power)));
    return render_at(scene, camera_model, camera, source, x, y, taper, crop);
}

/// The light a source puts on the frame, as a fraction of all of it, per channel: the PSF's
/// tables summed over the frame.
std::array<double, 3> fraction_on_frame(CameraModelHandle<RGB>& camera_model, double x, double y)
{
    const int base_x = static_cast<int>(std::floor(x));
    const int base_y = static_cast<int>(std::floor(y));
    const Image<RGB> table = camera_model.psf_image(
        SIZE, static_cast<float>(x - base_x - 0.5), static_cast<float>(y - base_y - 0.5));
    std::array<double, 3> sum{};
    for (int j = 0; j < SIZE; ++j) {
        for (int i = 0; i < SIZE; ++i) {
            const RGB value = table(i - base_x + SIZE, j - base_y + SIZE);
            for (std::size_t c = 0; c < 3; ++c) {
                sum[c] += static_cast<double>(value[c]);
            }
        }
    }
    return sum;
}

/// Electrons per unit of received power, per channel, in the one-second exposure.
std::array<double, 3> electrons_per_power(CameraModelHandle<RGB>& camera_model)
{
    const RGB qe = camera_model.sensor_quantum_efficiency();
    const RGB energies = RGB::photon_energies();
    std::array<double, 3> result{};
    for (std::size_t c = 0; c < 3; ++c) {
        result[c] = static_cast<double>(qe[c]) / static_cast<double>(energies[c]);
    }
    return result;
}

} // namespace

TEST_CASE("A still source puts all its light that falls on the frame on it",
          "[render][unresolved][psf]")
{
    Scene<RGB> scene;
    auto camera_model = make_camera(scene);
    for (const auto& [x, y] : {std::array<double, 2>{32.5, 32.5}, {20.31, 41.77}}) {
        INFO("source at (" << x << ", " << y << ")");
        const std::array<double, 3> on_frame = fraction_on_frame(camera_model, x, y);
        const Image<RGB> bright = render_alone(BRIGHT, x, y);
        const Image<RGB> faint = render_alone(FAINT, x, y);

        // The bright source's power, from its center pixel, which it draws in full:
        const int base_x = static_cast<int>(std::floor(x));
        const int base_y = static_cast<int>(std::floor(y));
        const Image<RGB> center = camera_model.psf_image(
            0, static_cast<float>(x - base_x - 0.5), static_cast<float>(y - base_y - 0.5));
        const std::array<double, 3> bright_sum = sums(bright);
        const std::array<double, 3> faint_sum = sums(faint);
        for (std::size_t c = 0; c < 3; ++c) {
            INFO("channel " << c);
            const double power = static_cast<double>(bright(base_x, base_y)[c]) /
                                 static_cast<double>(center(0, 0)[c]);
            CHECK(std::abs(bright_sum[c] / power - on_frame[c]) < 2e-5);
            CHECK(std::abs(faint_sum[c] / (power * FAINT / BRIGHT) - on_frame[c]) < 2e-5);
        }
    }
}

TEST_CASE("A faint still source fades out smoothly below the taper", "[render][unresolved][psf]")
{
    Scene<RGB> scene;
    auto camera_model = make_camera(scene);
    const std::array<double, 3> electrons = electrons_per_power(camera_model);
    constexpr float READ_NOISE = 5.0f;
    const double x = 30.27;
    const double y = 33.61;
    const Image<RGB> exact = render_alone(BRIGHT, x, y);
    const Image<RGB> tapered =
        render_alone(FAINT, x, y, Renderer<RGB>::DEFAULT_UNRESOLVED_TAPER, READ_NOISE, true);
    const Image<RGB> untapered = render_alone(FAINT, x, y, 0.0, READ_NOISE, true);

    // Every pixel is within the taper's threshold of the source drawn in full, with what the
    // taper leaves out spread over the frame: no edge anywhere, whether or not stamps are
    // cropped. Without a taper, it is drawn in full.
    const double tau = Renderer<RGB>::DEFAULT_UNRESOLVED_TAPER * READ_NOISE;
    const double scale = FAINT / BRIGHT;
    double worst_tapered = 0.0;
    double worst_untapered = 0.0;
    double peak = 0.0;
    for (std::size_t i = 0; i < exact.size(); ++i) {
        for (std::size_t c = 0; c < 3; ++c) {
            const double full = static_cast<double>(exact[i][c]) * scale * electrons[c];
            worst_tapered = std::max(
                worst_tapered, std::abs(static_cast<double>(tapered[i][c]) * electrons[c] - full));
            worst_untapered =
                std::max(worst_untapered,
                         std::abs(static_cast<double>(untapered[i][c]) * electrons[c] - full));
            peak = std::max(peak, full);
        }
    }
    INFO("peak " << peak << " electrons, threshold " << tau);
    CHECK(worst_tapered < 2.0 * tau);
    CHECK(worst_untapered < 1e-5 * peak);
}

TEST_CASE("The taper must be a non-negative fraction", "[render][unresolved][psf]")
{
    Renderer<RGB> renderer;
    CHECK(renderer.unresolved_taper() == Renderer<RGB>::DEFAULT_UNRESOLVED_TAPER);
    CHECK_THROWS(renderer.set_unresolved_taper(-1e-3));
    CHECK_THROWS(renderer.set_unresolved_taper(std::nan("")));
    CHECK_THROWS(renderer.set_unresolved_taper(std::numeric_limits<double>::infinity()));
    renderer.set_unresolved_taper(0.01);
    CHECK(renderer.unresolved_taper() == 0.01);
}
