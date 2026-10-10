#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <vector>

#include "catch2/catch_test_macros.hpp"
#include "huira/huira.hpp"

using namespace huira;
using namespace huira::units::literals;

namespace {

/// The jupiter_long_range example's view without SPICE: the Sun 5.16 degrees from Jupiter, which
/// below a focal length of about 103 mm puts the Sun's disc in the frame at its left edge.
constexpr int SUN_WIDTH = 1920;
constexpr int SUN_HEIGHT = 1080;

Image<Visible8> render_sun_scene(double focal_mm, bool with_sun, bool with_jupiter, bool convolve)
{
    Scene<Visible8> scene;
    auto camera_model = scene.new_camera_model();
    camera_model.set_focal_length(units::Millimeter(focal_mm));
    camera_model.set_fstop(3.3f);
    camera_model.configure_sensor_from_pitch({SUN_WIDTH, SUN_HEIGHT}, 8.5_um, 8.5_um);
    camera_model.set_sensor_bit_depth(12);
    camera_model.enable_psf_convolution(convolve);
    auto camera = scene.root.new_instance(camera_model);
    camera.set_velocity(0_Kmps, 8_Kmps, 0_Kmps);
    camera.set_rotation(Rotation<double>::from_parent_to_local(
        Quaternion<double>(0.50865, -0.50865, 0.491198, 0.491198)));
    if (with_sun) {
        auto sun = scene.root.new_instance(scene.new_sun_light());
        sun.set_position(
            units::Meter(-1.500372e11), units::Meter(7.396523e9), units::Meter(3.207256e9));
    }
    if (with_jupiter) {
        auto jupiter = scene.root.new_instance(scene.new_unresolved_object_from_magnitude(-1.44));
        jupiter.set_position(
            units::Meter(-9.642371e11), units::Meter(-3.773395e10), units::Meter(3.673385e9));
    }
    auto frame_buffer = camera_model.make_frame_buffer();
    frame_buffer.enable_received_power();
    Renderer<Visible8> renderer;
    SceneView<Visible8> scene_view(scene,
                                   Interval::from_centered(Time::from_et(0.0), 1_s),
                                   camera,
                                   ObservationMode::ABERRATED_STATE);
    renderer.render(scene_view, frame_buffer);
    return frame_buffer.received_power();
}

/// A camera model with the Sun scene's optics, for the PSF's tables.
CameraModelHandle<Visible8> sun_scene_optics(Scene<Visible8>& scene, double focal_mm)
{
    auto camera_model = scene.new_camera_model();
    camera_model.set_focal_length(units::Millimeter(focal_mm));
    camera_model.set_fstop(3.3f);
    camera_model.configure_sensor_from_pitch({SUN_WIDTH, SUN_HEIGHT}, 8.5_um, 8.5_um);
    camera_model.set_sensor_bit_depth(12);
    return camera_model;
}

/// Electrons per unit of received power, per channel, for an exposure.
template <IsSpectral TSpectral>
std::array<double, TSpectral::size()> electrons_per_power(CameraModelHandle<TSpectral>& model,
                                                          double seconds)
{
    const TSpectral qe = model.sensor_quantum_efficiency();
    const TSpectral energies = TSpectral::photon_energies();
    std::array<double, TSpectral::size()> result{};
    for (std::size_t c = 0; c < TSpectral::size(); ++c) {
        result[c] = seconds * static_cast<double>(qe[c]) / static_cast<double>(energies[c]);
    }
    return result;
}

/// The light a pixel reads out, in electrons: the channels' sum.
template <IsSpectral TSpectral>
double electrons(const TSpectral& power, const std::array<double, TSpectral::size()>& per_power)
{
    double sum = 0.0;
    for (std::size_t c = 0; c < TSpectral::size(); ++c) {
        sum += static_cast<double>(power[c]) * per_power[c];
    }
    return sum;
}

} // namespace

TEST_CASE("A frame with the Sun in it is convolved with the PSF's tables, with no rounding noise",
          "[render][psf][bodies]")
{
    // At 102 mm the edge of the Sun's disc is in the frame. Convolved through single-precision
    // FFTs, its light put rounding noise of about 1e-12 W, half of it negative, in every pixel:
    // about 1e6 electrons.
    const Image<Visible8> raw = render_sun_scene(102.0, true, false, false);
    const Image<Visible8> convolved = render_sun_scene(102.0, true, false, true);
    Scene<Visible8> scene;
    auto optics = sun_scene_optics(scene, 102.0);
    const std::array<double, 8> per_power = electrons_per_power(optics, 1.0);
    // The taper's threshold (see Renderer::set_unresolved_taper()) for the sensor's read noise:
    const double tau = Renderer<Visible8>::DEFAULT_UNRESOLVED_TAPER *
                       std::max(static_cast<double>(optics.sensor_read_noise()), 1.0);

    std::size_t negative = 0;
    for (std::size_t i = 0; i < convolved.size(); ++i) {
        for (std::size_t c = 0; c < 8; ++c) {
            if (convolved[i][c] < 0.f) {
                ++negative;
                break;
            }
        }
    }
    CHECK(negative == 0);

    // The Sun's pixels as traced, each a source the PSF spreads over the frame:
    struct Lit {
        int x;
        int y;
        Visible8 power;
    };
    std::vector<Lit> lit;
    for (int y = 0; y < SUN_HEIGHT; ++y) {
        for (int x = 0; x < SUN_WIDTH; ++x) {
            if (raw(x, y).max() > 0.f) {
                lit.push_back({x, y, raw(x, y)});
            }
        }
    }
    REQUIRE(!lit.empty());

    // Every pixel, near the Sun and across the frame, is the Sun's pixels' light spread by the
    // PSF's tables (psf_image()), to their accuracy, or within the taper's threshold.
    double worst = 0.0;
    for (int ty = 7; ty < SUN_HEIGHT; ty += 97) {
        for (int tx = 3; tx < SUN_WIDTH; tx += 131) {
            Visible8 expected{0.f};
            std::array<double, 8> sum{};
            for (const Lit& source : lit) {
                const Image<Visible8> spread = optics.psf_image(
                    0, static_cast<float>(source.x - tx), static_cast<float>(source.y - ty));
                for (std::size_t c = 0; c < 8; ++c) {
                    sum[c] +=
                        static_cast<double>(source.power[c]) * static_cast<double>(spread(0, 0)[c]);
                }
            }
            for (std::size_t c = 0; c < 8; ++c) {
                expected[c] = static_cast<float>(sum[c]);
            }
            const double want = electrons(expected, per_power);
            const double got = electrons(convolved(tx, ty), per_power);
            const double error = std::abs(got - want) / (tau + 1e-3 * want);
            worst = std::max(worst, error);
            INFO("pixel (" << tx << ", " << ty << "): " << got << " electrons, expected " << want);
            CHECK(error < 1.0);
        }
    }
    INFO("worst error over its allowance " << worst);
    CHECK(worst < 1.0);
}

TEST_CASE("The Sun just beyond the frame changes nothing in it", "[render][psf][bodies]")
{
    // At 104 mm the Sun is beyond the frame's edge, and is not traced: the frame is Jupiter's.
    const Image<Visible8> with_sun = render_sun_scene(104.0, true, true, true);
    const Image<Visible8> without_sun = render_sun_scene(104.0, false, true, true);
    Scene<Visible8> scene;
    auto optics = sun_scene_optics(scene, 104.0);
    const std::array<double, 8> per_power = electrons_per_power(optics, 1.0);
    const double tau = Renderer<Visible8>::DEFAULT_UNRESOLVED_TAPER *
                       std::max(static_cast<double>(optics.sensor_read_noise()), 1.0);
    double worst = 0.0;
    for (std::size_t i = 0; i < with_sun.size(); ++i) {
        Visible8 difference{0.f};
        for (std::size_t c = 0; c < 8; ++c) {
            difference[c] = with_sun[i][c] - without_sun[i][c];
        }
        worst = std::max(worst, std::abs(electrons(difference, per_power)));
    }
    CHECK(worst <= tau);
}

namespace {

/// A sunlit ball that covers about 30% of a 768 x 768 frame, far brighter than the sensor's
/// full well, and a faint star 290 px beyond its limb.
constexpr int BALL_SIZE = 768;
constexpr double BALL_FOCAL = 50e-3;
constexpr double BALL_PITCH = 10e-6;
constexpr std::array<double, 2> STAR_PIXEL{700.3, 120.6};

Image<RGB> render_ball_scene(bool with_ball, bool with_star, double seconds)
{
    Scene<RGB> scene;
    auto camera_model = scene.new_camera_model();
    camera_model.set_pixel_convention(PixelConvention::colmap());
    camera_model.configure_sensor_from_pitch({BALL_SIZE, BALL_SIZE}, units::Meter(BALL_PITCH));
    camera_model.set_focal_length(units::Meter(BALL_FOCAL));
    camera_model.set_fstop(2.8f);
    camera_model.set_sensor_full_well_capacity(20000.f);
    camera_model.set_sensor_read_noise(5.f);
    auto camera = scene.root.new_instance(camera_model);

    const double focal_pixels = BALL_FOCAL / BALL_PITCH;
    const double axis = BALL_SIZE / 2.0;
    if (with_ball) {
        // 237 px in radius, centered at pixel (240, 384):
        const double distance = 100.0;
        const double radius = distance * std::tan(237.0 / focal_pixels);
        auto geometry =
            scene.add_ellipsoid(units::Meter(radius), units::Meter(radius), units::Meter(radius));
        auto ball = scene.root.new_instance(scene.add_primitive(geometry));
        ball.set_position(units::Meter(distance * (240.0 - axis) / focal_pixels),
                          units::Meter(distance * (384.0 - axis) / focal_pixels),
                          units::Meter(distance));
        auto sun = scene.root.new_instance(scene.new_sun_light());
        sun.set_position(0_m, 0_m, -1_au);
    }
    if (with_star) {
        const Vec3<double> direction = glm::normalize(Vec3<double>{
            (STAR_PIXEL[0] - axis) / focal_pixels, (STAR_PIXEL[1] - axis) / focal_pixels, 1.0});
        scene.set_stars({Star<RGB>(direction, RGB{1e-10f})});
    }
    auto frame_buffer = camera_model.make_frame_buffer();
    frame_buffer.enable_received_power();
    Renderer<RGB> renderer;
    SceneView<RGB> scene_view(scene,
                              Interval{Time::from_et(0.0), Time::from_et(seconds)},
                              camera,
                              ObservationMode::GEOMETRIC_STATE);
    renderer.render(scene_view, frame_buffer);
    return frame_buffer.received_power();
}

/// A star's light and centroid, in electrons and pixels, measured as photometry does: the light
/// within 5 px of where it is, less a cubic surface fitted to the light 8 to 24 px from it.
struct Measurement {
    double light = 0.0;
    std::array<double, 2> centroid{};
    double background_rms = 0.0; ///< How far the light around it strays from the surface.
};

Measurement measure_star(const Image<RGB>& image, const std::array<double, 3>& per_power)
{
    const int cx = static_cast<int>(STAR_PIXEL[0]);
    const int cy = static_cast<int>(STAR_PIXEL[1]);
    // Least squares for the ten coefficients of a cubic in (u, v), by the normal equations:
    constexpr std::size_t TERMS = 10;
    std::array<std::array<double, TERMS + 1>, TERMS> normal{};
    const auto basis = [](double u, double v) {
        return std::array<double, TERMS>{
            1.0, u, v, u * u, u * v, v * v, u * u * u, u * u * v, u * v * v, v * v * v};
    };
    std::vector<std::array<double, 3>> ring;
    for (int y = cy - 24; y <= cy + 24; ++y) {
        for (int x = cx - 24; x <= cx + 24; ++x) {
            const double u = x + 0.5 - STAR_PIXEL[0];
            const double v = y + 0.5 - STAR_PIXEL[1];
            const double r = std::hypot(u, v);
            if (r < 8.0 || r > 24.0) {
                continue;
            }
            const double value = electrons(image(x, y), per_power);
            ring.push_back({u, v, value});
            const auto b = basis(u / 24.0, v / 24.0);
            for (std::size_t i = 0; i < TERMS; ++i) {
                for (std::size_t j = 0; j < TERMS; ++j) {
                    normal[i][j] += b[i] * b[j];
                }
                normal[i][TERMS] += b[i] * value;
            }
        }
    }
    // Gauss-Jordan elimination with partial pivoting:
    for (std::size_t i = 0; i < TERMS; ++i) {
        std::size_t pivot = i;
        for (std::size_t k = i + 1; k < TERMS; ++k) {
            if (std::abs(normal[k][i]) > std::abs(normal[pivot][i])) {
                pivot = k;
            }
        }
        std::swap(normal[i], normal[pivot]);
        for (std::size_t k = 0; k < TERMS; ++k) {
            if (k != i) {
                const double factor = normal[k][i] / normal[i][i];
                for (std::size_t j = i; j <= TERMS; ++j) {
                    normal[k][j] -= factor * normal[i][j];
                }
            }
        }
    }
    std::array<double, TERMS> coefficients{};
    for (std::size_t i = 0; i < TERMS; ++i) {
        coefficients[i] = normal[i][TERMS] / normal[i][i];
    }
    const auto surface = [&](double u, double v) {
        const auto b = basis(u / 24.0, v / 24.0);
        double value = 0.0;
        for (std::size_t i = 0; i < TERMS; ++i) {
            value += coefficients[i] * b[i];
        }
        return value;
    };

    Measurement result;
    double squares = 0.0;
    for (const auto& [u, v, value] : ring) {
        squares += (value - surface(u, v)) * (value - surface(u, v));
    }
    result.background_rms = std::sqrt(squares / static_cast<double>(ring.size()));
    for (int y = cy - 6; y <= cy + 6; ++y) {
        for (int x = cx - 6; x <= cx + 6; ++x) {
            const double u = x + 0.5 - STAR_PIXEL[0];
            const double v = y + 0.5 - STAR_PIXEL[1];
            if (std::hypot(u, v) > 5.0) {
                continue;
            }
            const double light = electrons(image(x, y), per_power) - surface(u, v);
            result.light += light;
            result.centroid[0] += light * (x + 0.5);
            result.centroid[1] += light * (y + 0.5);
        }
    }
    result.centroid[0] /= result.light;
    result.centroid[1] /= result.light;
    return result;
}

} // namespace

TEST_CASE("A large bright body leaves a faint star's measurement as it is without it",
          "[render][psf][bodies]")
{
    // The ball reads out about 1e4 times the full well in each pixel. Its light, spread by the
    // PSF, lies smoothly under the star; through single-precision FFTs it came with rounding
    // noise that a background fit cannot take out, which biased the star's light and centroid.
    constexpr double SECONDS = 0.1;
    const Image<RGB> both = render_ball_scene(true, true, SECONDS);
    const Image<RGB> star = render_ball_scene(false, true, SECONDS);
    Scene<RGB> scene;
    auto optics = scene.new_camera_model();
    optics.set_sensor_full_well_capacity(20000.f);
    const std::array<double, 3> per_power = electrons_per_power(optics, SECONDS);

    // How bright the ball is, against the full well:
    double brightest = 0.0;
    for (std::size_t i = 0; i < both.size(); ++i) {
        brightest = std::max(brightest, electrons(both[i], per_power));
    }
    INFO("the ball's brightest pixel reads " << brightest / 20000.0 << " full wells");
    REQUIRE(brightest > 3e3 * 20000.0);

    const Measurement alone = measure_star(star, per_power);
    const Measurement beside = measure_star(both, per_power);
    INFO("star alone: " << alone.light << " electrons at (" << alone.centroid[0] << ", "
                        << alone.centroid[1] << ")");
    INFO("beside the ball: " << beside.light << " electrons at (" << beside.centroid[0] << ", "
                             << beside.centroid[1] << "), background strays by "
                             << beside.background_rms << " electrons");
    REQUIRE(alone.light > 5e3);
    CHECK(std::abs(beside.light / alone.light - 1.0) < 1e-3);
    CHECK(std::abs(beside.centroid[0] - alone.centroid[0]) < 1e-3);
    CHECK(std::abs(beside.centroid[1] - alone.centroid[1]) < 1e-3);
    CHECK(beside.background_rms < 0.5);
}
