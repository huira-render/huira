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
/// The sources' depth, which sets their defocus: the references take the PSF's tables there.
constexpr double RANGE = 1e6;

CameraModelHandle<RGB> make_camera(Scene<RGB>& scene, double focal_pixels = FOCAL_PIXELS)
{
    auto camera_model = scene.new_camera_model();
    camera_model.set_pixel_convention(PixelConvention::colmap());
    camera_model.configure_sensor_from_pitch({SIZE, SIZE}, 8.5_um);
    camera_model.set_focal_length(units::Meter(focal_pixels * 8.5e-6));
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
        const Image<RGB> expected = camera_model.psf_image(radius,
                                                           static_cast<float>(x - base_x - 0.5),
                                                           static_cast<float>(y - base_y - 0.5),
                                                           units::Meter(RANGE));
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

TEST_CASE("The PSF's stamps are made from its tables", "[render][unresolved][psf]")
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
    const Image<RGB> table = camera_model.psf_image(SIZE,
                                                    static_cast<float>(x - base_x - 0.5),
                                                    static_cast<float>(y - base_y - 0.5),
                                                    units::Meter(RANGE));
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
        const Image<RGB> center = camera_model.psf_image(0,
                                                         static_cast<float>(x - base_x - 0.5),
                                                         static_cast<float>(y - base_y - 0.5),
                                                         units::Meter(RANGE));
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

TEST_CASE("A still source is drawn across the frame as the PSF's tables give it, out of focus "
          "and with scattered light",
          "[render][unresolved][psf][defocus][scatter]")
{
    // A 2 px blur: R f |vergence| / pitch, with R = f / (2 N).
    constexpr double DIOPTERS = 2.0 * 8.5e-6 * 2.0 * 3.3 / (50e-3 * 50e-3);
    for (const int optics : {0, 1, 2}) {
        Scene<RGB> scene;
        auto camera_model = make_camera(scene);
        if (optics != 1) {
            camera_model.set_focus_diopters(units::Diopter(DIOPTERS));
        }
        if (optics != 0) {
            camera_model.set_scatter(0.05f, 2.5f, units::Arcsecond(20.0));
        }
        INFO((optics == 0 ? "defocused" : optics == 1 ? "scattered light" : "both"));
        auto camera = scene.root.new_instance(camera_model);
        auto source = scene.root.new_instance(scene.new_unresolved_emitter(units::Watt(BRIGHT)));
        const double x = 30.27;
        const double y = 33.61;
        const Image<RGB> image = render_at(scene, camera_model, camera, source, x, y);

        // The tables over the whole frame, and the source's power from its center pixel:
        const int base_x = static_cast<int>(std::floor(x));
        const int base_y = static_cast<int>(std::floor(y));
        const Image<RGB> table = camera_model.psf_image(SIZE,
                                                        static_cast<float>(x - base_x - 0.5),
                                                        static_cast<float>(y - base_y - 0.5),
                                                        units::Meter(RANGE));
        for (std::size_t c = 0; c < 3; ++c) {
            INFO("channel " << c);
            const double expected_center = static_cast<double>(table(SIZE, SIZE)[c]);
            const double power = static_cast<double>(image(base_x, base_y)[c]) / expected_center;
            double worst = 0.0;
            double sum = 0.0;
            double on_frame = 0.0;
            for (int j = 0; j < SIZE; ++j) {
                for (int i = 0; i < SIZE; ++i) {
                    const double expected =
                        static_cast<double>(table(i - base_x + SIZE, j - base_y + SIZE)[c]);
                    const double rendered = static_cast<double>(image(i, j)[c]) / power;
                    worst = std::max(worst, std::abs(rendered - expected));
                    sum += rendered;
                    on_frame += expected;
                }
            }
            // To the renderer's single-precision projection; and no light lost or added.
            CHECK(worst < 1e-4 * expected_center);
            CHECK(std::abs(sum - on_frame) < 2e-5);
        }
    }
}

namespace {

/// The moving sources' optics are the same at ten times the focal length, so that the light
/// they deliver hardly changes across the frame: a source at a pixel from the axis delivers
/// (1 + (r / focal)^2)^-1.5 of its light on it, the aperture seen at a slant from further away.
constexpr double LONG_FOCAL_PIXELS = 10.0 * FOCAL_PIXELS;

/// Renders one source moving along x at a constant speed, from (x, y) to (x + length, y) over
/// the one-second exposure: exactly so on the sensor, which images the source's plane evenly.
Image<RGB> render_moving(CameraModelHandle<RGB>& camera_model,
                         Scene<RGB>& scene,
                         double power,
                         double x,
                         double y,
                         double length,
                         double taper = Renderer<RGB>::DEFAULT_UNRESOLVED_TAPER)
{
    auto camera = scene.root.new_instance(camera_model);
    auto source = scene.root.new_instance(scene.new_unresolved_emitter(units::Watt(power)));
    const double axis = SIZE / 2.0;
    source.set_position(units::Meter((x - axis) / LONG_FOCAL_PIXELS * RANGE),
                        units::Meter((y - axis) / LONG_FOCAL_PIXELS * RANGE),
                        units::Meter(RANGE));
    source.set_velocity(units::MetersPerSecond(length / LONG_FOCAL_PIXELS * RANGE),
                        units::MetersPerSecond(0.0),
                        units::MetersPerSecond(0.0));
    auto frame_buffer = camera_model.make_frame_buffer();
    frame_buffer.enable_received_power();
    Renderer<RGB> renderer;
    renderer.set_unresolved_taper(taper);
    Interval exposure{Time::from_et(0.0), Time::from_et(1.0)};
    // Where the source is, not where its light left it.
    SceneView<RGB> scene_view(scene, exposure, camera, ObservationMode::TRUE_STATE, 3);
    renderer.render(scene_view, frame_buffer);
    return frame_buffer.received_power();
}

/// The path from (x, y) to (x + length, y) integrated over the PSF's tables (psf_image()), out
/// to radius pixels from it, per unit of light: Gauss-Legendre quadrature, three points per
/// sixty-fourth of a pixel. Pixel (i, j) of the frame is at (i, j) in the image.
Image<RGB> streak_reference(
    CameraModelHandle<RGB>& camera_model, double x, double y, double length, int radius)
{
    constexpr std::array<double, 3> NODES{-0.7745966692414834, 0.0, 0.7745966692414834};
    constexpr std::array<double, 3> WEIGHTS{5.0 / 9.0, 8.0 / 9.0, 5.0 / 9.0};
    const int panels = static_cast<int>(std::ceil(64.0 * length));
    std::vector<std::array<double, 3>> sum(static_cast<std::size_t>(SIZE * SIZE));
    for (int k = 0; k < panels; ++k) {
        for (std::size_t n = 0; n < NODES.size(); ++n) {
            const double t = (k + 0.5 + 0.5 * NODES[n]) / panels;
            const double weight = 0.5 * WEIGHTS[n] / panels;
            const double px = x + t * length;
            const int base_x = static_cast<int>(std::floor(px));
            const int base_y = static_cast<int>(std::floor(y));
            const Image<RGB> stamp = camera_model.psf_image(radius,
                                                            static_cast<float>(px - base_x - 0.5),
                                                            static_cast<float>(y - base_y - 0.5),
                                                            units::Meter(RANGE));
            for (int j = -radius; j <= radius; ++j) {
                for (int i = -radius; i <= radius; ++i) {
                    const int fx = base_x + i;
                    const int fy = base_y + j;
                    if (fx < 0 || fx >= SIZE || fy < 0 || fy >= SIZE) {
                        continue;
                    }
                    for (std::size_t c = 0; c < 3; ++c) {
                        sum[static_cast<std::size_t>(fy * SIZE + fx)][c] +=
                            weight * static_cast<double>(stamp(i + radius, j + radius)[c]);
                    }
                }
            }
        }
    }
    Image<RGB> result(SIZE, SIZE, RGB{0.f});
    for (int j = 0; j < SIZE; ++j) {
        for (int i = 0; i < SIZE; ++i) {
            const auto& value = sum[static_cast<std::size_t>(j * SIZE + i)];
            result(i, j) = RGB{static_cast<float>(value[0]),
                               static_cast<float>(value[1]),
                               static_cast<float>(value[2])};
        }
    }
    return result;
}

/// The fraction of a moving source's light that falls on the frame, per channel: that of a
/// still source along its path, by four-point Gauss-Legendre quadrature over the time, on panels
/// a pixel long, and finer towards where the path crosses an edge, where it changes sharply.
std::array<double, 3>
streak_on_frame(CameraModelHandle<RGB>& camera_model, double x, double y, double length)
{
    constexpr std::array<double, 4> NODES{
        -0.8611363115940526, -0.3399810435848563, 0.3399810435848563, 0.8611363115940526};
    constexpr std::array<double, 4> WEIGHTS{
        0.3478548451374538, 0.6521451548625461, 0.6521451548625461, 0.3478548451374538};
    std::vector<double> cuts;
    for (double u = 0.0; u < length; u += 1.0) {
        cuts.push_back(u);
    }
    cuts.push_back(length);
    for (const double edge : {0.0, static_cast<double>(SIZE)}) {
        for (double step = 1.0 / 64.0; step < 2.0; step *= 2.0) {
            for (const double cut : {edge - x - step, edge - x, edge - x + step}) {
                if (cut > 0.0 && cut < length) {
                    cuts.push_back(cut);
                }
            }
        }
    }
    std::sort(cuts.begin(), cuts.end());
    std::array<double, 3> result{};
    for (std::size_t k = 0; k + 1 < cuts.size(); ++k) {
        const double half = 0.5 * (cuts[k + 1] - cuts[k]);
        for (std::size_t n = 0; n < NODES.size(); ++n) {
            // The PSF's tables summed over the frame, reaching it from beyond:
            const double px = x + 0.5 * (cuts[k] + cuts[k + 1]) + half * NODES[n];
            const int base_x = static_cast<int>(std::floor(px));
            const int base_y = static_cast<int>(std::floor(y));
            constexpr int REACH = 2 * SIZE;
            const Image<RGB> table = camera_model.psf_image(REACH,
                                                            static_cast<float>(px - base_x - 0.5),
                                                            static_cast<float>(y - base_y - 0.5),
                                                            units::Meter(RANGE));
            for (int j = 0; j < SIZE; ++j) {
                for (int i = 0; i < SIZE; ++i) {
                    const RGB value = table(i - base_x + REACH, j - base_y + REACH);
                    for (std::size_t c = 0; c < 3; ++c) {
                        result[c] += half * WEIGHTS[n] * static_cast<double>(value[c]) / length;
                    }
                }
            }
        }
    }
    return result;
}

} // namespace

TEST_CASE("A moving source is drawn as its path integrated over the PSF's tables",
          "[render][unresolved][psf][streak]")
{
    // A 2 px blur: R f |vergence| / pitch, with R = f / (2 N).
    constexpr double FOCAL = LONG_FOCAL_PIXELS * 8.5e-6;
    constexpr double DIOPTERS = 2.0 * 8.5e-6 * 2.0 * 3.3 / (FOCAL * FOCAL);
    for (const int optics : {0, 1, 2}) {
        Scene<RGB> scene;
        auto camera_model = make_camera(scene, LONG_FOCAL_PIXELS);
        if (optics != 0) {
            camera_model.set_focus_diopters(units::Diopter(DIOPTERS));
        }
        if (optics == 2) {
            camera_model.set_scatter(0.05f, 2.5f, units::Arcsecond(20.0));
        }
        INFO((optics == 0 ? "in focus" : optics == 1 ? "defocused" : "defocused, scattered light"));
        // Bright enough to be drawn in full, across a pixel's edges at odd places:
        const double x = 25.37;
        const double y = 31.81;
        const double length = 11.43;
        const Image<RGB> image = render_moving(camera_model, scene, BRIGHT, x, y, length);
        // Pixels within NEAR of the path, which the reference follows out to REACH from every
        // point of it:
        constexpr double NEAR = 10.0;
        constexpr int REACH = 24;
        const Image<RGB> expected = streak_reference(camera_model, x, y, length, REACH);

        // The source's power from the brightest pixel; every pixel within the reference's reach
        // is then its path integrated over the tables. Stamps used to bead along the path, and
        // be cut off at their edge.
        for (std::size_t c = 0; c < 3; ++c) {
            INFO("channel " << c);
            int bx = 0;
            int by = 0;
            for (int j = 0; j < SIZE; ++j) {
                for (int i = 0; i < SIZE; ++i) {
                    if (expected(i, j)[c] > expected(bx, by)[c]) {
                        bx = i;
                        by = j;
                    }
                }
            }
            const double peak = static_cast<double>(expected(bx, by)[c]);
            const double power = static_cast<double>(image(bx, by)[c]) / peak;
            double worst = 0.0;
            for (int j = 0; j < SIZE; ++j) {
                for (int i = 0; i < SIZE; ++i) {
                    const double nearest_x = std::clamp(i + 0.5, x, x + length);
                    if (std::hypot(i + 0.5 - nearest_x, j + 0.5 - y) > NEAR) {
                        continue;
                    }
                    const double rendered = static_cast<double>(image(i, j)[c]) / power;
                    worst = std::max(worst,
                                     std::abs(rendered - static_cast<double>(expected(i, j)[c])));
                }
            }
            CHECK(worst < 1e-4 * peak);
        }
    }
}

TEST_CASE("A moving source puts all its light that falls on the frame on it",
          "[render][unresolved][psf][streak]")
{
    // Along the middle, across the right edge, and past the frame near a corner:
    struct Path {
        double x;
        double y;
        double length;
    };
    for (const Path& path :
         {Path{20.31, 41.77, 9.6}, Path{57.2, 30.6, 13.1}, Path{60.3, 66.2, 7.9}}) {
        Scene<RGB> scene;
        auto camera_model = make_camera(scene, LONG_FOCAL_PIXELS);
        INFO("path from (" << path.x << ", " << path.y << "), " << path.length << " px long");
        const std::array<double, 3> on_frame =
            streak_on_frame(camera_model, path.x, path.y, path.length);
        const Image<RGB> bright =
            render_moving(camera_model, scene, BRIGHT, path.x, path.y, path.length);
        Scene<RGB> faint_scene;
        auto faint_model = make_camera(faint_scene, LONG_FOCAL_PIXELS);
        const Image<RGB> faint =
            render_moving(faint_model, faint_scene, FAINT, path.x, path.y, path.length);

        // The power, from a still source's center pixel, drawn in full:
        Scene<RGB> still_scene;
        auto still_model = make_camera(still_scene, LONG_FOCAL_PIXELS);
        const Image<RGB> still = render_moving(still_model, still_scene, BRIGHT, 32.5, 32.5, 0.0);
        const Image<RGB> center = still_model.psf_image(0, 0.f, 0.f, units::Meter(RANGE));
        const std::array<double, 3> bright_sum = sums(bright);
        const std::array<double, 3> faint_sum = sums(faint);
        for (std::size_t c = 0; c < 3; ++c) {
            INFO("channel " << c);
            const double power =
                static_cast<double>(still(32, 32)[c]) / static_cast<double>(center(0, 0)[c]);
            CHECK(std::abs(bright_sum[c] / power - on_frame[c]) < 2e-5);
            CHECK(std::abs(faint_sum[c] / (power * FAINT / BRIGHT) - on_frame[c]) < 2e-5);
        }
    }
}

TEST_CASE("A faint moving source fades out smoothly below the taper",
          "[render][unresolved][psf][streak]")
{
    constexpr float READ_NOISE = 5.0f;
    const double x = 21.27;
    const double y = 33.61;
    const double length = 17.3;
    Scene<RGB> exact_scene;
    auto exact_model = make_camera(exact_scene, LONG_FOCAL_PIXELS);
    exact_model.set_sensor_read_noise(READ_NOISE);
    const Image<RGB> exact = render_moving(exact_model, exact_scene, BRIGHT, x, y, length);
    Scene<RGB> faint_scene;
    auto faint_model = make_camera(faint_scene, LONG_FOCAL_PIXELS);
    faint_model.set_sensor_read_noise(READ_NOISE);
    const Image<RGB> tapered = render_moving(faint_model, faint_scene, FAINT, x, y, length);
    const std::array<double, 3> electrons = electrons_per_power(faint_model);

    // Every pixel is within the taper's threshold of the source drawn in full, with what the
    // taper leaves out spread over the frame: no edge anywhere.
    const double tau = Renderer<RGB>::DEFAULT_UNRESOLVED_TAPER * READ_NOISE;
    const double scale = FAINT / BRIGHT;
    double worst = 0.0;
    double peak = 0.0;
    for (std::size_t i = 0; i < exact.size(); ++i) {
        for (std::size_t c = 0; c < 3; ++c) {
            const double full = static_cast<double>(exact[i][c]) * scale * electrons[c];
            worst =
                std::max(worst, std::abs(static_cast<double>(tapered[i][c]) * electrons[c] - full));
            peak = std::max(peak, full);
        }
    }
    INFO("peak " << peak << " electrons, threshold " << tau);
    REQUIRE(peak > 20.0 * tau);
    CHECK(worst < 2.0 * tau);
}
