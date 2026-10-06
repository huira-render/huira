#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <limits>
#include <optional>
#include <random>
#include <string>
#include <vector>

#include "catch2/catch_test_macros.hpp"
#include "huira/cameras/camera_model.hpp"
#include "huira/cameras/psfs/airy_band.hpp"
#include "huira/cameras/psfs/psf_tables.hpp"
#include "huira/core/spectral_bins.hpp"
#include "huira/images/image.hpp"
#include "huira/units/units.hpp"
#include "reference/airy.hpp"

using namespace huira;
using namespace huira::units::literals;

namespace {

using reference::PI;

/// Optics: f-number, and pixel width and height in meters.
struct Optics {
    double fnumber;
    double pitch_x;
    double pitch_y;
};

/// The Jupiter example's camera: f/3.3 with 8.5 um pixels.
constexpr Optics JUPITER{3.3, 8.5e-6, 8.5e-6};

/// A camera's optics as its PSF tables see them.
template <IsSpectral TSpectral>
Optics optics_of(const CameraModel<TSpectral>& camera)
{
    const auto [pitch_x, pitch_y] = camera.pixel_pitch();
    return {static_cast<double>(camera.fstop()), pitch_x.to_si(), pitch_y.to_si()};
}

/// The reference's band-averaged Airy pattern for a channel, in cycles per pixel width.
template <IsSpectral TSpectral>
reference::AiryBand reference_airy(const Optics& optics, std::size_t channel)
{
    const Bin bin = TSpectral::get_bin(channel);
    return {optics.pitch_x / (bin.min_wavelength * optics.fnumber),
            optics.pitch_x / (bin.max_wavelength * optics.fnumber)};
}

/// The reference's light in a channel, in the pixel whose center is (dx, dy) pixels from the
/// source.
template <IsSpectral TSpectral>
double reference_light(const Optics& optics, std::size_t channel, double dx, double dy)
{
    const reference::AiryBand airy = reference_airy<TSpectral>(optics, channel);
    const double aspect = optics.pitch_y / optics.pitch_x;
    const Bin bin = TSpectral::get_bin(channel);
    const double blue = optics.pitch_x / (bin.min_wavelength * optics.fnumber);
    return reference::rectangle_light(
        airy, dx - 0.5, dx + 0.5, aspect * (dy - 0.5), aspect * (dy + 0.5), 0.25 / blue);
}

/// The scale an error is judged against: the pixel's light, or where that dips in a dark ring,
/// the rings' mean (the far field, lambda N / (pi^3 rho^3) over the pixel's area).
template <IsSpectral TSpectral>
double scale(const Optics& optics, std::size_t channel, double dx, double dy, double light)
{
    const double aspect = optics.pitch_y / optics.pitch_x;
    const double r = std::hypot(dx, aspect * dy);
    if (r < 1.0) {
        return light;
    }
    const Bin bin = TSpectral::get_bin(channel);
    const double mean_period =
        0.5 * (bin.min_wavelength + bin.max_wavelength) * optics.fnumber / optics.pitch_x;
    return std::max(light, aspect * mean_period / (PI * PI * PI * r * r * r));
}

/// The worst error of a camera's psf_image() against the reference, as a fraction of scale(),
/// over all channels of the given pixels.
template <IsSpectral TSpectral>
double worst_error(CameraModel<TSpectral>& camera,
                   double x_offset,
                   double y_offset,
                   int radius,
                   const std::vector<std::array<int, 2>>& pixels)
{
    const Optics optics = optics_of(camera);
    const Image<TSpectral> image =
        camera.psf_image(radius, static_cast<float>(x_offset), static_cast<float>(y_offset));
    double worst = 0.0;
    for (const auto& pixel : pixels) {
        const double dx = pixel[0] - x_offset;
        const double dy = pixel[1] - y_offset;
        for (std::size_t channel = 0; channel < TSpectral::size(); ++channel) {
            const double exact = reference_light<TSpectral>(optics, channel, dx, dy);
            const double light =
                static_cast<double>(image(pixel[0] + radius, pixel[1] + radius)[channel]);
            worst = std::max(
                worst, std::abs(light - exact) / scale<TSpectral>(optics, channel, dx, dy, exact));
        }
    }
    return worst;
}

/// Every pixel within a radius, and a sample of pixels at random distances beyond it, out to an
/// outer radius.
std::vector<std::array<int, 2>> sample_pixels(int inner, int outer, int count, unsigned seed)
{
    std::vector<std::array<int, 2>> pixels;
    for (int y = -inner; y <= inner; ++y) {
        for (int x = -inner; x <= inner; ++x) {
            pixels.push_back({x, y});
        }
    }
    std::mt19937 rng(seed);
    std::uniform_real_distribution<double> unit(0.0, 1.0);
    while (static_cast<int>(pixels.size()) < (2 * inner + 1) * (2 * inner + 1) + count) {
        const double r = inner + (outer - inner) * unit(rng);
        const double angle = 2.0 * PI * unit(rng);
        const int x = static_cast<int>(std::lround(r * std::cos(angle)));
        const int y = static_cast<int>(std::lround(r * std::sin(angle)));
        if (std::max(std::abs(x), std::abs(y)) <= outer) {
            pixels.push_back({x, y});
        }
    }
    return pixels;
}

template <IsSpectral TSpectral>
void configure(CameraModel<TSpectral>& camera, const Optics& optics, Resolution resolution)
{
    camera.configure_sensor_from_pitch(resolution,
                                       units::Micrometer(optics.pitch_x * 1e6),
                                       units::Micrometer(optics.pitch_y * 1e6));
    camera.set_focal_length(50_mm);
    camera.set_fstop(static_cast<float>(optics.fnumber));
}

/// The Jupiter example's camera in Visible8, shared by the tests that only read its tables, so
/// that they are built once.
CameraModel<Visible8>& jupiter_camera()
{
    static CameraModel<Visible8> camera;
    static const bool configured = [] {
        configure(camera, JUPITER, Resolution{1920, 1080});
        return true;
    }();
    static_cast<void>(configured);
    return camera;
}

/// Harvey-Shack scattered light in pixels, written out independently of the library's: its
/// total found by integrating 2 pi r S(r) on panels growing by half each, out to a billion times
/// its outer radius, and in closed form beyond.
class ReferenceScatter {
  public:
    ReferenceScatter(double shoulder, double slope, double outer)
        : shoulder_{shoulder}, slope_{slope}, outer_{outer}
    {
        const auto ring = [&](double r) { return 2.0 * PI * r * shape(r); };
        double total = reference::gauss_legendre(ring, 0.0, shoulder, 64);
        double start = shoulder;
        const double end = 1e9 * std::max(shoulder, outer);
        while (start < end) {
            total += reference::gauss_legendre(ring, start, 1.5 * start, 4);
            start *= 1.5;
        }
        // Beyond, S falls as a^s r^-n, with n = s, or s + 2 times b^2 beyond an outer radius.
        const double falloff = outer > 0.0 ? slope + 2.0 : slope;
        const double scale = std::pow(shoulder, slope) * (outer > 0.0 ? outer * outer : 1.0);
        total += 2.0 * PI * scale * std::pow(start, 2.0 - falloff) / (falloff - 2.0);
        peak_ = 1.0 / total;
    }

    double operator()(double r) const { return peak_ * shape(r); }

  private:
    double shape(double r) const
    {
        double value = std::pow(1.0 + r * r / (shoulder_ * shoulder_), -0.5 * slope_);
        if (outer_ > 0.0) {
            value /= 1.0 + r * r / (outer_ * outer_);
        }
        return value;
    }

    double shoulder_;
    double slope_;
    double outer_;
    double peak_ = 1.0;
};

} // namespace

TEST_CASE("The PSF tables' Bessel functions and band average match the reference", "[cameras][psf]")
{
    std::mt19937 rng(3);
    std::uniform_real_distribution<double> unit(0.0, 1.0);
    double worst_bessel = 0.0;
    for (int k = 0; k < 3000; ++k) {
        const double x = k < 1500 ? 30.0 * unit(rng) : 4000.0 * unit(rng);
        const std::array<double, 3> j = detail::bessel_j012(x);
        for (int n = 0; n < 3; ++n) {
            worst_bessel = std::max(
                worst_bessel, std::abs(j[static_cast<std::size_t>(n)] - reference::bessel_j(n, x)));
        }
    }
    CHECK(worst_bessel < 2e-15);

    // T is used beyond its table at 2048, where the asymptotic form's error, 0.36 / x^5, is
    // 3e-10 of T.
    const detail::AiryTail& tail = detail::AiryTail::instance();
    double worst_tail = 0.0;
    for (double x = 0.003; x < 6000.0; x *= 1.013) {
        const double exact = reference::airy_tail()(x);
        worst_tail = std::max(worst_tail, std::abs(tail(x) - exact) / exact);
    }
    CHECK(worst_tail < 5e-10);

    for (const auto& [blue, red] : {std::array<double, 2>{6.78, 6.04}, {3.66, 3.43}, {4.0, 2.0}}) {
        const detail::AiryBandProfile profile(blue, red);
        const reference::AiryBand exact(blue, red);
        double worst = 0.0;
        for (double r = 0.0; r < 400.0; r = r * 1.01 + 0.001) {
            const double floor = exact.peak() * 1e-6 / (1.0 + r * r * r);
            worst = std::max(worst, std::abs(profile(r) - exact(r)) / std::max(exact(r), floor));
        }
        INFO("band " << blue << " to " << red << " cycles per pixel");
        CHECK(worst < 1e-8);
    }
}

TEST_CASE("psf_image() matches the reference's pixel integrals", "[cameras][psf]")
{
    CameraModel<Visible8>& camera = jupiter_camera();

    // Each representation's range: the near table out to 11 pixels, the projected integrals to
    // between 29 and 164 pixels by channel, and the far field beyond. One source sits half-way
    // between the near table's points, where interpolation errs most.
    const std::vector<std::array<int, 2>> pixels = sample_pixels(6, 200, 150, 7);
    for (const auto& [x_offset, y_offset] :
         {std::array<double, 2>{0.37, -0.21}, {0.5 / 56.0 + 3.0 / 56.0, 0.5 - 0.5 / 56.0}}) {
        INFO("source at (" << x_offset << ", " << y_offset << ")");
        CHECK(worst_error(camera, x_offset, y_offset, 200, pixels) < 1e-3);
    }
}

TEST_CASE("psf_image() matches the reference for slow, fast and rectangular-pixel optics",
          "[cameras][psf]")
{
    const std::vector<std::array<int, 2>> pixels = sample_pixels(3, 120, 60, 11);

    SECTION("f/8 with 5 um pixels: a near table to 36 pixels, and rings out to the frame")
    {
        CameraModel<Visible8> camera;
        configure(camera, Optics{8.0, 5e-6, 5e-6}, Resolution{1024, 1024});
        CHECK(worst_error(camera, -0.23, 0.41, 120, pixels) < 1e-3);
    }
    SECTION("f/2 with 10 um pixels: the pattern much smaller than a pixel")
    {
        CameraModel<RGB> camera;
        configure(camera, Optics{2.0, 10e-6, 10e-6}, Resolution{1024, 1024});
        CHECK(worst_error(camera, 0.11, 0.47, 120, pixels) < 1e-3);
    }
    SECTION("f/4 with 10 x 7.5 um pixels, in RGB")
    {
        CameraModel<RGB> camera;
        configure(camera, Optics{4.0, 10e-6, 7.5e-6}, Resolution{1024, 1024});
        CHECK(worst_error(camera, 0.29, -0.33, 120, pixels) < 1e-3);
    }
}

TEST_CASE("psf_image() puts each source's centroid where the reference does", "[cameras][psf]")
{
    CameraModel<Visible8>& camera = jupiter_camera();
    constexpr int RADIUS = 6;

    std::mt19937 rng(13);
    std::uniform_real_distribution<double> unit(-0.5, 0.5);
    double worst = 0.0;
    for (int k = 0; k < 8; ++k) {
        // Half of the sources sit half-way between the near table's points.
        double x_offset = unit(rng);
        double y_offset = unit(rng);
        if (k % 2 == 0) {
            x_offset = (std::floor(x_offset * 56.0) + 0.5) / 56.0;
            y_offset = (std::floor(y_offset * 56.0) + 0.5) / 56.0;
        }
        const Image<Visible8> image =
            camera.psf_image(RADIUS, static_cast<float>(x_offset), static_cast<float>(y_offset));
        for (std::size_t channel : {std::size_t{0}, std::size_t{7}}) {
            std::array<double, 3> moments{};
            std::array<double, 3> exact{};
            for (int y = -RADIUS; y <= RADIUS; ++y) {
                for (int x = -RADIUS; x <= RADIUS; ++x) {
                    const double light =
                        static_cast<double>(image(x + RADIUS, y + RADIUS)[channel]);
                    const double reference_value =
                        reference_light<Visible8>(JUPITER, channel, x - x_offset, y - y_offset);
                    moments = {moments[0] + light, moments[1] + light * x, moments[2] + light * y};
                    exact = {exact[0] + reference_value,
                             exact[1] + reference_value * x,
                             exact[2] + reference_value * y};
                }
            }
            worst = std::max({worst,
                              std::abs(moments[1] / moments[0] - exact[1] / exact[0]),
                              std::abs(moments[2] / moments[0] - exact[2] / exact[0])});
        }
    }
    CHECK(worst < 1e-4);
}

TEST_CASE("psf_image() holds the light the reference puts in the square", "[cameras][psf]")
{
    CameraModel<Visible8>& camera = jupiter_camera();
    constexpr int RADIUS = 100;
    const double x_offset = 0.31;
    const double y_offset = -0.17;
    const Image<Visible8> image =
        camera.psf_image(RADIUS, static_cast<float>(x_offset), static_cast<float>(y_offset));

    for (std::size_t channel = 0; channel < Visible8::size(); ++channel) {
        double sum = 0.0;
        for (int y = 0; y < image.height(); ++y) {
            for (int x = 0; x < image.width(); ++x) {
                sum += static_cast<double>(image(x, y)[channel]);
            }
        }
        const reference::AiryBand airy = reference_airy<Visible8>(JUPITER, channel);
        const double edge = RADIUS + 0.5;
        const double exact = reference::rectangle_light(
            airy, -edge - x_offset, edge - x_offset, -edge - y_offset, edge - y_offset, 0.02);
        INFO("channel " << channel);
        CHECK(std::abs(sum - exact) < 1e-6);
    }
}

TEST_CASE("psf_image() keeps the pattern's symmetry", "[cameras][psf]")
{
    CameraModel<RGB> camera;
    configure(camera, JUPITER, Resolution{256, 256});
    constexpr int RADIUS = 40;
    const Image<RGB> image = camera.psf_image(RADIUS, 0.3f, -0.2f);
    const Image<RGB> mirrored = camera.psf_image(RADIUS, -0.3f, -0.2f);
    bool same = true;
    for (int y = 0; y < image.height(); ++y) {
        for (int x = 0; x < image.width(); ++x) {
            for (std::size_t channel = 0; channel < RGB::size(); ++channel) {
                same = same && image(x, y)[channel] == mirrored(2 * RADIUS - x, y)[channel];
            }
        }
    }
    CHECK(same);
}

TEST_CASE("psf_image() follows the optics", "[cameras][psf]")
{
    CameraModel<RGB> camera;
    configure(camera, JUPITER, Resolution{256, 256});
    const std::vector<std::array<int, 2>> pixels = sample_pixels(2, 30, 20, 17);
    CHECK(worst_error(camera, 0.2, 0.1, 30, pixels) < 1e-3);

    // A new f-number rebuilds the tables.
    camera.set_fstop(5.6f);
    CHECK(worst_error(camera, 0.2, 0.1, 30, pixels) < 1e-3);
}

TEST_CASE("psf_image() without the aperture's PSF", "[cameras][psf]")
{
    CameraModel<RGB> camera;
    configure(camera, JUPITER, Resolution{64, 64});

    SECTION("With no PSF, the pixel the source falls in holds all its light")
    {
        camera.delete_psf();
        const Image<RGB> image = camera.psf_image(2, 0.7f, -0.2f);
        for (int y = 0; y < image.height(); ++y) {
            for (int x = 0; x < image.width(); ++x) {
                const float expected = (x == 3 && y == 2) ? 1.f : 0.f;
                CHECK(image(x, y)[0] == expected);
            }
        }
    }
    SECTION("A PSF that was set is not supported")
    {
        camera.set_measured_psf(Image<RGB>(9, 9, RGB{1.f}), 1.f, PSFSampling::PointSampled);
        CHECK_THROWS(camera.psf_image(2));
    }
    SECTION("Out-of-range arguments throw")
    {
        CHECK_THROWS(camera.psf_image(-1));
        CHECK_THROWS(camera.psf_image(CameraModel<RGB>::MAX_PSF_IMAGE_RADIUS + 1));
        CHECK_THROWS(camera.psf_image(2, std::numeric_limits<float>::quiet_NaN()));
    }
}

TEST_CASE("Scattered light's profile holds all of its light", "[cameras][psf][scatter]")
{
    struct Case {
        double shoulder;
        double slope;
        double outer;
    };
    for (const Case& c : {Case{0.5, 3.0, 0.0},
                          Case{1.0, 2.2, 0.0},
                          Case{2.0, 2.0, 300.0},
                          Case{1.0, 1.2, 50.0},
                          Case{5.0, 0.5, 40.0}}) {
        const detail::ScatterProfile profile(
            c.shoulder, c.slope, c.outer > 0.0 ? std::optional<double>(c.outer) : std::nullopt);
        const ReferenceScatter exact(c.shoulder, c.slope, c.outer);
        INFO("shoulder " << c.shoulder << ", slope " << c.slope << ", outer " << c.outer);
        double worst = 0.0;
        for (double r = 0.0; r < 1e4; r = 1.2 * r + 0.01) {
            worst = std::max(worst, std::abs(profile(r) / exact(r) - 1.0));
        }
        CHECK(worst < 1e-12);

        // The derivatives against central differences, whose own error is about 1e-5.
        for (double r : {0.3, 2.0, 17.0, 150.0}) {
            const double step = 1e-4 * std::max(r, c.shoulder);
            const std::array<double, 3> d = profile.derivatives(r);
            const double first = (profile(r + step) - profile(r - step)) / (2.0 * step);
            const double second =
                (profile(r + step) - 2.0 * profile(r) + profile(r - step)) / (step * step);
            const double scale = d[0] / std::max(r, c.shoulder);
            CHECK(std::abs(d[1] - first) < 1e-5 * scale);
            CHECK(std::abs(d[2] - second) < 1e-4 * scale / std::max(r, c.shoulder));
        }
    }
}

TEST_CASE("psf_image() includes scattered light set in angles", "[cameras][psf][scatter]")
{
    CameraModel<RGB> camera;
    configure(camera, JUPITER, Resolution{512, 512});
    const units::Arcsecond shoulder(20.0);
    const units::Degree outer(1.0);
    camera.set_scatter(0.05f, 1.8f, shoulder, outer);

    // An angle lands f theta from the source: 50 mm over 8.5 um pixels.
    const auto check = [&](double focal_length) {
        const double pixels_per_radian = focal_length / JUPITER.pitch_x;
        const ReferenceScatter scatter(
            shoulder.to_si() * pixels_per_radian, 1.8, outer.to_si() * pixels_per_radian);
        const double x_offset = 0.21;
        const double y_offset = -0.36;
        const Image<RGB> image =
            camera.psf_image(150, static_cast<float>(x_offset), static_cast<float>(y_offset));
        double worst = 0.0;
        for (const auto& pixel : sample_pixels(4, 150, 80, 23)) {
            const double dx = pixel[0] - x_offset;
            const double dy = pixel[1] - y_offset;
            for (std::size_t channel = 0; channel < RGB::size(); ++channel) {
                const reference::AiryBand airy = reference_airy<RGB>(JUPITER, channel);
                const auto density = [&](double r) { return 0.95 * airy(r) + 0.05 * scatter(r); };
                const double exact = reference::pixel_light(density, dx, dy, 0.02);
                const double light =
                    static_cast<double>(image(pixel[0] + 150, pixel[1] + 150)[channel]);
                worst = std::max(
                    worst, std::abs(light - exact) / scale<RGB>(JUPITER, channel, dx, dy, exact));
            }
        }
        return worst;
    };
    CHECK(check(0.05) < 1e-3);

    // The scattered light follows the focal length.
    camera.set_focal_length(100_mm);
    camera.set_fstop(static_cast<float>(JUPITER.fnumber));
    CHECK(check(0.1) < 1e-3);
    CHECK(camera.describe().find("arcsec") != std::string::npos);
}

TEST_CASE("set_scatter() checks its arguments", "[cameras][psf][scatter]")
{
    CameraModel<RGB> camera;
    const units::Arcsecond shoulder(10.0);
    const units::Degree outer(2.0);
    const float nan = std::numeric_limits<float>::quiet_NaN();
    CHECK_THROWS(camera.set_scatter(-0.1f, 2.5f, shoulder));
    CHECK_THROWS(camera.set_scatter(1.f, 2.5f, shoulder));
    CHECK_THROWS(camera.set_scatter(nan, 2.5f, shoulder));
    CHECK_THROWS(camera.set_scatter(0.01f, 0.f, shoulder, outer));
    CHECK_THROWS(camera.set_scatter(0.01f, nan, shoulder, outer));
    CHECK_THROWS(camera.set_scatter(0.01f, 2.5f, units::Arcsecond(0.0)));
    CHECK_THROWS(camera.set_scatter(0.01f, 2.5f, units::Degree(90.0)));
    CHECK_THROWS(camera.set_scatter(0.01f, 2.5f, shoulder, units::Arcsecond(5.0)));
    CHECK_THROWS(camera.set_scatter(0.01f, 2.5f, shoulder, units::Degree(90.0)));

    // A slope of 2 or less has no finite total without an outer angle.
    CHECK_THROWS(camera.set_scatter(0.01f, 2.f, shoulder));
    CHECK_THROWS(camera.set_scatter(0.01f, 1.5f, shoulder));
    CHECK_NOTHROW(camera.set_scatter(0.01f, 1.5f, shoulder, outer));
    CHECK_NOTHROW(camera.set_scatter(0.01f, 2.5f, shoulder));
    CHECK_NOTHROW(camera.set_scatter(0.f, 2.5f, shoulder));
}

TEST_CASE("psf_image() does not take scattered light set in pixels", "[cameras][psf][scatter]")
{
    CameraModel<RGB> camera;
    configure(camera, JUPITER, Resolution{64, 64});
    camera.set_harvey_shack_scatter(0.05f, 2.5f);
    CHECK_THROWS(camera.psf_image(2));

    // Set in angles, it replaces the earlier setting.
    camera.set_scatter(0.05f, 2.5f, units::Arcsecond(20.0));
    CHECK_NOTHROW(camera.psf_image(2));
}

TEST_CASE("Renders use scattered light set in angles at its size in pixels",
          "[cameras][psf][scatter]")
{
    // 50 mm over 8.5 um pixels: 20 arcsec is 0.5704 px and 1 degree 102.66 px.
    CameraModel<RGB> in_angles;
    configure(in_angles, JUPITER, Resolution{128, 128});
    in_angles.set_psf_convolution_radius(40);
    in_angles.set_scatter(0.05f, 1.8f, units::Arcsecond(20.0), units::Degree(1.0));

    CameraModel<RGB> in_pixels;
    configure(in_pixels, JUPITER, Resolution{128, 128});
    in_pixels.set_psf_convolution_radius(40);
    const double pixels_per_radian = 0.05 / JUPITER.pitch_x;
    in_pixels.set_harvey_shack_scatter(
        0.05f,
        1.8f,
        static_cast<float>(units::Arcsecond(20.0).to_si() * pixels_per_radian),
        static_cast<float>(units::Degree(1.0).to_si() * pixels_per_radian));

    const Image<RGB>& angles = in_angles.get_psf_wings_kernel();
    const Image<RGB>& pixels = in_pixels.get_psf_wings_kernel();
    REQUIRE(angles.width() == pixels.width());
    double worst = 0.0;
    for (int y = 0; y < angles.height(); ++y) {
        for (int x = 0; x < angles.width(); ++x) {
            worst = std::max(worst,
                             std::abs(static_cast<double>(angles(x, y)[0] - pixels(x, y)[0])) /
                                 static_cast<double>(pixels(x, y)[0]));
        }
    }
    CHECK(worst < 1e-5);
}
