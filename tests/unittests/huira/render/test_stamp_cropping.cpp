#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdlib>
#include <utility>

#include "catch2/catch_test_macros.hpp"
#include "huira/huira.hpp"

using namespace huira;
using namespace huira::units::literals;

namespace {

struct Exposure {
    double power_w = 1e-2; ///< The source's power; at 1 km.
    double seconds = 1.0;
    float read_noise = 10.f; ///< Electrons.
    bool cropping = true;
};

/// Renders one unresolved source near the middle of a 48 x 48 frame of 10 um pixels behind a
/// 50 mm f/32 lens, with the aperture's PSF in 8 px stamps, and returns the received power.
Image<RGB> render(const Exposure& exposure)
{
    Scene<RGB> scene;
    auto camera_model = scene.new_camera_model();
    camera_model.configure_sensor_from_pitch({48, 48}, 10_um);
    camera_model.set_focal_length(50_mm);
    camera_model.set_fstop(32.f);
    camera_model.use_aperture_psf(8, 2); // small, so quick to build
    camera_model.set_sensor_read_noise(exposure.read_noise);
    auto camera = scene.root.new_instance(camera_model);

    auto source =
        scene.root.new_instance(scene.new_unresolved_emitter(units::Watt(exposure.power_w)));
    source.set_position(units::Meter(0.3 / 5000.0 * 1000.0),
                        units::Meter(0.2 / 5000.0 * 1000.0),
                        units::Meter(1000.0));

    auto frame_buffer = camera_model.make_frame_buffer();
    frame_buffer.enable_received_power();
    Renderer<RGB> renderer;
    renderer.set_stamp_cropping(exposure.cropping);
    Interval interval{Time::from_et(0.0), Time::from_et(exposure.seconds)};
    SceneView<RGB> scene_view(scene, interval, camera, ObservationMode::GEOMETRIC_STATE);
    renderer.render(scene_view, frame_buffer);
    return frame_buffer.received_power();
}

double total(const Image<RGB>& image)
{
    double sum = 0.0;
    for (std::size_t i = 0; i < image.size(); ++i) {
        sum += static_cast<double>(image[i][1]);
    }
    return sum;
}

/// The centroid of the light, in pixels.
std::pair<double, double> centroid(const Image<RGB>& image)
{
    double sum = 0.0;
    double sum_x = 0.0;
    double sum_y = 0.0;
    for (int y = 0; y < image.height(); ++y) {
        for (int x = 0; x < image.width(); ++x) {
            const auto value = static_cast<double>(image(x, y)[1]);
            sum += value;
            sum_x += value * (x + 0.5);
            sum_y += value * (y + 0.5);
        }
    }
    return {sum_x / sum, sum_y / sum};
}

/// How far the stamp reaches: the largest Chebyshev distance of a lit pixel from the brightest.
int lit_radius(const Image<RGB>& image)
{
    int bx = 0;
    int by = 0;
    for (int y = 0; y < image.height(); ++y) {
        for (int x = 0; x < image.width(); ++x) {
            if (image(x, y)[1] > image(bx, by)[1]) {
                bx = x;
                by = y;
            }
        }
    }
    int radius = 0;
    for (int y = 0; y < image.height(); ++y) {
        for (int x = 0; x < image.width(); ++x) {
            if (image(x, y)[1] > 0.f) {
                radius = std::max(radius, std::max(std::abs(x - bx), std::abs(y - by)));
            }
        }
    }
    return radius;
}

} // namespace

TEST_CASE("A cropped stamp keeps all of the source's light", "[render][unresolved][cropping]")
{
    // A faint source's stamp is cropped, and a bright one's is not, but the light received is in
    // proportion to the power. Cropped stamps used to lose the light beyond the crop. (The lit
    // radius is measured from the brightest pixel, which can be one from the stamp's middle.)
    const Image<RGB> faint = render({1e-3});
    const Image<RGB> bright = render({1e2});
    REQUIRE(lit_radius(faint) < 8);
    REQUIRE(lit_radius(bright) >= 8);
    const double ratio = (total(faint) / 1e-3) / (total(bright) / 1e2);
    CHECK(std::abs(ratio - 1.0) <= 1e-5);
}

TEST_CASE("Stamps are cropped by the light received over the exposure, against the read noise",
          "[render][unresolved][cropping]")
{
    // The crop used to be by the photons per second, whatever the exposure or the sensor.
    const int brief = lit_radius(render({1e-2, 0.01}));
    const int long_exposure = lit_radius(render({1e-2, 10.0}));
    INFO("lit radius " << brief << " in 0.01 s, " << long_exposure << " in 10 s");
    CHECK(brief < long_exposure);

    const int quiet = lit_radius(render({1e-2, 1.0, 1.f}));
    const int noisy = lit_radius(render({1e-2, 1.0, 100.f}));
    INFO("lit radius " << quiet << " with 1 e- of read noise, " << noisy << " with 100 e-");
    CHECK(noisy < quiet);
}

TEST_CASE("Cropping keeps the source's centroid, and can be turned off",
          "[render][unresolved][cropping]")
{
    // Without cropping, every source gets the whole stamp. Cropping cuts light that is not
    // symmetric about the source, so it is never so tight that the centroid moves by more than
    // 0.01 px.
    const Image<RGB> whole = render({1e-2, 1.0, 10.f, false});
    const Image<RGB> cropped = render({1e-2});
    REQUIRE(lit_radius(cropped) < 8);
    CHECK(lit_radius(whole) >= 8);
    CHECK(std::abs(total(cropped) / total(whole) - 1.0) <= 1e-5);

    const auto [whole_x, whole_y] = centroid(whole);
    const auto [cropped_x, cropped_y] = centroid(cropped);
    INFO("centroid moved by (" << cropped_x - whole_x << ", " << cropped_y - whole_y << ") px");
    CHECK(std::abs(cropped_x - whole_x) <= 0.0101);
    CHECK(std::abs(cropped_y - whole_y) <= 0.0101);
}
