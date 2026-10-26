#include <cmath>
#include <cstddef>
#include <limits>
#include <string>
#include <thread>
#include <vector>

#include "catch2/catch_test_macros.hpp"
#include "catch2/matchers/catch_matchers_string.hpp"
#include "huira/cameras/camera_model.hpp"
#include "huira/cameras/sensors/sensor_calibration.hpp"
#include "huira/core/spectral_bins.hpp"
#include "huira/units/units.hpp"
#include "huira/util/logger.hpp"

using namespace huira;

TEST_CASE("Sensor readout respects the frame buffer's enabled outputs", "[cameras][sensor]")
{
    CameraModel<RGB> camera;
    camera.configure_sensor_from_pitch(Resolution{8, 6}, units::Micrometer(5.0));
    auto frame_buffer = camera.make_frame_buffer();

    // A few thousand photons per pixel: well above the noise floor and below full well.
    const RGB power{1e-15f};

    SECTION("Without a sensor response, readout leaves the frame buffer untouched")
    {
        frame_buffer.enable_received_power();
        frame_buffer.received_power().fill(power);

        camera.readout(frame_buffer, units::Second(1.0));

        REQUIRE_FALSE(frame_buffer.has_sensor_response());
        const Image<RGB>& received = frame_buffer.received_power();
        for (std::size_t i = 0; i < received.size(); ++i) {
            for (std::size_t c = 0; c < 3; ++c) {
                REQUIRE(received[i][c] == power[c]);
            }
        }
    }

    SECTION("With a sensor response, readout fills it")
    {
        frame_buffer.enable_sensor_response();
        frame_buffer.received_power().fill(power);

        camera.readout(frame_buffer, units::Second(1.0));

        const Image<RGB>& response = frame_buffer.sensor_response();
        REQUIRE(response.width() == 8);
        REQUIRE(response.height() == 6);
        for (std::size_t i = 0; i < response.size(); ++i) {
            for (std::size_t c = 0; c < 3; ++c) {
                REQUIRE(response[i][c] > 0.f);
            }
        }
    }
}

namespace {

/// A camera whose sensor the tests can configure directly.
class SensorProbe : public CameraModel<RGB> {
  public:
    using CameraModel<RGB>::sensor_;

    SensorProbe() = default;

    SensorProbe(const SensorProbe&) = delete;
    SensorProbe(SensorProbe&&) = delete;
    SensorProbe& operator=(const SensorProbe&) = delete;
    SensorProbe& operator=(SensorProbe&&) = delete;
};

/// Fill the received power so that each channel of each pixel expects the given number of signal
/// electrons over a 1 s exposure.
void expect_electrons(SensorProbe& camera, FrameBuffer<RGB>& frame_buffer, double electrons)
{
    const RGB energies = RGB::photon_energies();
    const RGB qe = camera.sensor_->quantum_efficiency();
    RGB power;
    for (std::size_t c = 0; c < 3; ++c) {
        power[c] = static_cast<float>(electrons * static_cast<double>(energies[c]) /
                                      static_cast<double>(qe[c]));
    }
    frame_buffer.received_power().fill(power);
}

/// A sensor that reports electrons directly: 1 e-/DN, no bias or dark current, 20 bits.
void electron_counter(SensorProbe& camera)
{
    camera.configure_sensor_from_pitch(Resolution{64, 64}, units::Micrometer(5.0));
    camera.sensor_->set_conversion_gain(1.f);
    camera.sensor_->set_bias_level_dn(0.f);
    camera.sensor_->set_dark_current(0.f);
    camera.sensor_->set_read_noise(0.f);
    camera.sensor_->set_full_well_capacity(1e9f);
    camera.sensor_->set_bit_depth(20);
}

/// The response, in DN, after an exposure of the given length.
std::vector<double>
read_dn(SensorProbe& camera, FrameBuffer<RGB>& frame_buffer, double seconds = 1.0)
{
    camera.readout(frame_buffer, units::Second(seconds));
    const double max_dn = std::pow(2.0, camera.sensor_->bit_depth()) - 1.0;
    std::vector<double> dn;
    const Image<RGB>& response = frame_buffer.sensor_response();
    for (std::size_t i = 0; i < response.size(); ++i) {
        for (std::size_t c = 0; c < 3; ++c) {
            dn.push_back(static_cast<double>(response[i][c]) * max_dn);
        }
    }
    return dn;
}

double mean_of(const std::vector<double>& values)
{
    double sum = 0.0;
    for (double v : values) {
        sum += v;
    }
    return sum / static_cast<double>(values.size());
}

double variance_of(const std::vector<double>& values)
{
    const double mean = mean_of(values);
    double sum = 0.0;
    for (double v : values) {
        sum += (v - mean) * (v - mean);
    }
    return sum / static_cast<double>(values.size() - 1);
}

} // namespace

TEST_CASE("Each camera's noise is its own, and a seed repeats it", "[cameras][sensor]")
{
    // The noise used to come from one generator shared by every camera and thread: two
    // readouts at once raced on it, and a frame's noise depended on every readout before it.
    SensorProbe camera;
    camera.configure_sensor_from_pitch(Resolution{32, 32}, units::Micrometer(5.0));
    auto frame_buffer = camera.make_frame_buffer();
    frame_buffer.enable_sensor_response();
    expect_electrons(camera, frame_buffer, 500.0);

    camera.sensor_->set_noise_seed(7);
    const std::vector<double> first = read_dn(camera, frame_buffer);
    const std::vector<double> second = read_dn(camera, frame_buffer);
    CHECK(first != second); // successive frames differ

    camera.sensor_->set_noise_seed(7);
    CHECK(read_dn(camera, frame_buffer) == first); // the seed repeats them

    // The same seed in another camera, reading out at the same time as a third, gives the same
    // frame:
    SensorProbe again;
    SensorProbe other;
    for (SensorProbe* probe : {&again, &other}) {
        probe->configure_sensor_from_pitch(Resolution{32, 32}, units::Micrometer(5.0));
    }
    again.sensor_->set_noise_seed(7);
    other.sensor_->set_noise_seed(8);
    auto again_buffer = again.make_frame_buffer();
    auto other_buffer = other.make_frame_buffer();
    again_buffer.enable_sensor_response();
    other_buffer.enable_sensor_response();
    expect_electrons(again, again_buffer, 500.0);
    expect_electrons(other, other_buffer, 500.0);

    std::vector<double> again_dn;
    std::thread reader([&] { again_dn = read_dn(again, again_buffer); });
    (void)read_dn(other, other_buffer);
    reader.join();
    CHECK(again_dn == first);

    // Unless seeded, cameras have different seeds:
    SensorProbe a;
    SensorProbe b;
    CHECK(a.sensor_->noise_seed() != b.sensor_->noise_seed());
}

TEST_CASE("Shot noise is Poisson at low signal", "[cameras][sensor]")
{
    // A normal approximation used to give fractional, and negative, electron counts.
    SensorProbe camera;
    electron_counter(camera);
    auto frame_buffer = camera.make_frame_buffer();
    frame_buffer.enable_sensor_response();
    expect_electrons(camera, frame_buffer, 2.0);

    const std::vector<double> dn = read_dn(camera, frame_buffer);
    std::size_t zeros = 0;
    for (double v : dn) {
        REQUIRE(std::abs(v - std::round(v)) < 1e-3); // whole electrons
        zeros += (std::round(v) == 0.0) ? 1 : 0;
    }
    // 12288 draws of a mean of 2: P(0) = e^-2 = 0.135, and mean and variance both 2.
    CHECK(std::abs(static_cast<double>(zeros) / static_cast<double>(dn.size()) - std::exp(-2.0)) <
          0.015);
    CHECK(std::abs(mean_of(dn) - 2.0) < 0.06);
    CHECK(std::abs(variance_of(dn) - 2.0) < 0.15);

    // Above the exact range, the mean and variance still match:
    expect_electrons(camera, frame_buffer, 400.0);
    const std::vector<double> bright = read_dn(camera, frame_buffer);
    CHECK(std::abs(mean_of(bright) - 400.0) < 1.0);
    CHECK(std::abs(variance_of(bright) / 400.0 - 1.0) < 0.06);
}

TEST_CASE("A dark pixel with no dark current or read noise reads exactly the bias",
          "[cameras][sensor]")
{
    // Shot and read noise were normal distributions with a standard deviation of zero here,
    // which the standard library does not allow (it aborts in a checked build).
    SensorProbe camera;
    electron_counter(camera);
    camera.sensor_->set_bias_level_dn(10.f);
    auto frame_buffer = camera.make_frame_buffer();
    frame_buffer.enable_sensor_response();
    frame_buffer.received_power().fill(RGB{0.f});

    for (double v : read_dn(camera, frame_buffer)) {
        REQUIRE(std::abs(v - 10.0) < 1e-3);
    }
}

TEST_CASE("Read noise is added after the full well clamps the charge", "[cameras][sensor]")
{
    // The full well clamp used to come last, so saturated pixels had no read noise at all.
    SensorProbe camera;
    electron_counter(camera);
    camera.sensor_->set_full_well_capacity(1000.f);
    camera.sensor_->set_read_noise(10.f);
    camera.sensor_->set_bias_level_dn(100.f);
    auto frame_buffer = camera.make_frame_buffer();
    frame_buffer.enable_sensor_response();
    expect_electrons(camera, frame_buffer, 1e6);

    const std::vector<double> dn = read_dn(camera, frame_buffer);
    CHECK(std::abs(mean_of(dn) - 1100.0) < 1.0);
    CHECK(std::abs(std::sqrt(variance_of(dn)) - 10.0) < 0.5);
}

TEST_CASE("The default quantum efficiency is white balanced and peaks at 0.5", "[cameras][sensor]")
{
    // It used to sum to 0.5 over the bins, so each bin's share fell with the number of bins: a
    // peak of 0.20 for RGB and 0.08 for Visible8.
    auto check = [](auto spectral) {
        using TSpectral = decltype(spectral);
        const TSpectral qe = SensorConfig<TSpectral>{}.quantum_efficiency;
        const TSpectral energies = TSpectral::photon_energies();
        CHECK(std::abs(qe.max() - 0.5f) < 1e-6f);
        // Proportional to photon energy, so equal power in each bin gives equal electrons:
        for (std::size_t i = 0; i < TSpectral::size(); ++i) {
            CHECK(std::abs(qe[i] / energies[i] - qe[0] / energies[0]) <
                  1e-6f * qe[0] / energies[0]);
        }
    };
    check(RGB{});
    check(Visible8{});
}

TEST_CASE("Disabling the sensor response leaves the received power enabled", "[cameras][sensor]")
{
    // It used to disable the received power too, even when that had been enabled on its own.
    CameraModel<RGB> camera;
    camera.configure_sensor_from_pitch(Resolution{4, 4}, units::Micrometer(5.0));
    auto frame_buffer = camera.make_frame_buffer();
    frame_buffer.enable_received_power();
    frame_buffer.enable_sensor_response();
    frame_buffer.enable_sensor_response(false);
    CHECK_FALSE(frame_buffer.has_sensor_response());
    CHECK(frame_buffer.has_received_power());

    // Enabling it still enables the received power it reads out:
    auto other = camera.make_frame_buffer();
    other.enable_sensor_response();
    CHECK(other.has_received_power());
}

TEST_CASE("The readout gives whole digital numbers", "[cameras][sensor]")
{
    // The ADC's output was continuous: e.g. 0.82 DN for one electron at 1.22 e-/DN.
    SensorProbe camera;
    camera.configure_sensor_from_pitch(Resolution{32, 32}, units::Micrometer(5.0));
    auto frame_buffer = camera.make_frame_buffer();
    frame_buffer.enable_sensor_response();

    for (const bool noise : {true, false}) {
        camera.sensor_->enable_noise(noise);
        expect_electrons(camera, frame_buffer, 1.0);
        for (double v : read_dn(camera, frame_buffer)) {
            REQUIRE(std::abs(v - std::round(v)) < 1e-3);
        }
        expect_electrons(camera, frame_buffer, 1234.5);
        for (double v : read_dn(camera, frame_buffer)) {
            REQUIRE(std::abs(v - std::round(v)) < 1e-3);
        }
    }

    // Without noise, the expected electrons become DN by the gain, rounded:
    // (1234.5 + 1 dark) e- / 4.88 e-/DN + 10 DN = 263.18 -> 263.
    camera.sensor_->enable_noise(false);
    expect_electrons(camera, frame_buffer, 1234.5);
    for (double v : read_dn(camera, frame_buffer)) {
        REQUIRE(std::abs(v - 263.0) < 1e-3);
    }
}

TEST_CASE("The default gain matches the ADC's range to the full well", "[cameras][sensor]")
{
    // At 1.22 e-/DN, the ADC saturated at about 5000 e-, a quarter of the 20000 e- full well.
    // Now it saturates just before the well is full, within 1% of it.
    const SensorConfig<RGB> config;
    const double max_dn = std::pow(2.0, config.bit_depth) - 1.0;
    const double adc_saturation_e =
        (max_dn - static_cast<double>(config.bias_level_dn)) * static_cast<double>(config.gain);
    const double full_well = static_cast<double>(config.full_well_capacity);
    CHECK(adc_saturation_e <= full_well);
    CHECK(adc_saturation_e > 0.99 * full_well);
}

TEST_CASE("Without noise, the dark current and bias still apply", "[cameras][sensor]")
{
    // Turning noise off also removed the bias and the dark current, which are not noise, so a
    // noiseless frame was darker than the mean of a noisy one.
    SensorProbe camera;
    electron_counter(camera);
    camera.sensor_->set_dark_current(50.f);
    camera.sensor_->set_bias_level_dn(100.f);
    camera.sensor_->enable_noise(false);
    auto frame_buffer = camera.make_frame_buffer();
    frame_buffer.enable_sensor_response();
    frame_buffer.received_power().fill(RGB{0.f});

    for (double v : read_dn(camera, frame_buffer)) {
        REQUIRE(std::abs(v - 150.0) < 1e-3); // 50 e- of dark current over 1 s, at 1 e-/DN, + 100
    }
}

TEST_CASE("Received power that is not finite reads out as NaN", "[cameras][sensor]")
{
    // With noise on it read out as the bias, a plausible dark pixel that hid the problem.
    for (const bool noise : {true, false}) {
        SensorProbe camera;
        camera.configure_sensor_from_pitch(Resolution{4, 4}, units::Micrometer(5.0));
        camera.sensor_->enable_noise(noise);
        auto frame_buffer = camera.make_frame_buffer();
        frame_buffer.enable_sensor_response();
        frame_buffer.received_power().fill(RGB{1e-16f});
        frame_buffer.received_power()(1, 1) = RGB{std::nanf(""), 1e-16f, 1e-16f};
        frame_buffer.received_power()(2, 2) =
            RGB{std::numeric_limits<float>::infinity(), 1e-16f, 1e-16f};

        camera.readout(frame_buffer, units::Second(1.0));
        const Image<RGB>& response = frame_buffer.sensor_response();
        CHECK(std::isnan(response(1, 1)[0]));
        CHECK(std::isnan(response(2, 2)[0]));
        CHECK(std::isfinite(response(1, 1)[1]));
        CHECK(std::isfinite(response(0, 0)[0]));
    }
}

TEST_CASE("Reading out a sensor response without received power throws", "[cameras][sensor]")
{
    // It wrote nothing, silently: the sensor read out an empty image.
    CameraModel<RGB> camera;
    camera.configure_sensor_from_pitch(Resolution{4, 4}, units::Micrometer(5.0));
    auto frame_buffer = camera.make_frame_buffer();
    frame_buffer.enable_sensor_response();
    frame_buffer.enable_received_power(false);
    CHECK_THROWS(camera.readout(frame_buffer, units::Second(1.0)));
}

TEST_CASE("A copied sensor has the same settings and noise of its own", "[cameras][sensor]")
{
    // Sensors could not be copied, so set_sensor<T>(an existing sensor) did not compile.
    SensorProbe camera;
    camera.configure_sensor_from_pitch(Resolution{16, 16}, units::Micrometer(5.0));
    camera.sensor_->set_read_noise(3.f);
    camera.sensor_->set_bit_depth(14);

    SensorProbe other;
    other.set_sensor<SimpleSensor<RGB>>(
        *dynamic_cast<const SimpleSensor<RGB>*>(camera.sensor_.get()));
    CHECK(other.sensor_->read_noise() == 3.f);
    CHECK(other.sensor_->bit_depth() == 14);
    CHECK(other.resolution().width == 16);
    CHECK(other.sensor_->noise_seed() != camera.sensor_->noise_seed());

    auto read = [](SensorProbe& probe) {
        auto frame_buffer = probe.make_frame_buffer();
        frame_buffer.enable_sensor_response();
        expect_electrons(probe, frame_buffer, 100.0);
        return read_dn(probe, frame_buffer);
    };
    CHECK(read(camera) != read(other)); // noise of its own

    camera.sensor_->set_noise_seed(5);
    other.sensor_->set_noise_seed(5);
    CHECK(read(camera) == read(other)); // the same seed gives the same noise
}

namespace {

/// Collects the warnings logged while it lives.
struct WarningCapture {
    std::vector<std::string> warnings;

    WarningCapture()
    {
        set_log_sink([this](const LogEntry& entry) {
            if (entry.level == LogLevel::Warning) {
                warnings.push_back(entry.message);
            }
        });
    }
    ~WarningCapture() { Logger::instance().clear_custom_sink(); }

    WarningCapture(const WarningCapture&) = delete;
    WarningCapture(WarningCapture&&) = delete;
    WarningCapture& operator=(const WarningCapture&) = delete;
    WarningCapture& operator=(WarningCapture&&) = delete;

    /// Whether a warning was logged that contains the text.
    bool any(const std::string& text) const
    {
        for (const std::string& warning : warnings) {
            if (warning.find(text) != std::string::npos) {
                return true;
            }
        }
        return false;
    }
};

/// An image whose pixel (x, y) is value(x, y).
template <typename F>
Image<float> map_of(int width, int height, F value)
{
    Image<float> map(width, height);
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            map(x, y) = static_cast<float>(value(x, y));
        }
    }
    return map;
}

/// A Visible8 camera whose sensor the tests can configure directly.
class Visible8Probe : public CameraModel<Visible8> {
  public:
    using CameraModel<Visible8>::sensor_;

    Visible8Probe() = default;

    Visible8Probe(const Visible8Probe&) = delete;
    Visible8Probe(Visible8Probe&&) = delete;
    Visible8Probe& operator=(const Visible8Probe&) = delete;
    Visible8Probe& operator=(Visible8Probe&&) = delete;
};

} // namespace

TEST_CASE("A dark current map gives each pixel its own dark current", "[cameras][sensor][maps]")
{
    SensorProbe camera;
    electron_counter(camera); // 64x64, 1 e-/DN
    camera.sensor_->set_bias_level_dn(10.f);
    auto frame_buffer = camera.make_frame_buffer();
    frame_buffer.enable_sensor_response();

    // Without noise, each pixel reads its own dark current times the exposure, on top of the
    // signal and the bias, in every channel:
    const Image<float> rates = map_of(64, 64, [](int x, int y) { return 0.5 * (x + 100 * y); });
    camera.sensor_->set_dark_current_map(rates);
    REQUIRE(camera.sensor_->has_dark_current_map());
    camera.sensor_->enable_noise(false);
    expect_electrons(camera, frame_buffer, 20.0); // 20 e- per second
    const std::vector<double> dn = read_dn(camera, frame_buffer, 2.0);
    for (std::size_t i = 0; i < dn.size(); ++i) {
        const auto pixel = static_cast<int>(i / 3);
        const int x = pixel % 64;
        const int y = pixel / 64;
        REQUIRE(std::abs(dn[i] - (10.0 + 40.0 + x + 100.0 * y)) < 1e-3);
    }

    // With noise, the dark current's electrons are drawn with the signal's: Poisson, with the
    // map's mean.
    camera.sensor_->set_dark_current_map(Image<float>(64, 64, 50.f));
    camera.sensor_->enable_noise(true);
    frame_buffer.received_power().fill(RGB{0.f});
    const std::vector<double> noisy = read_dn(camera, frame_buffer);
    CHECK(std::abs(mean_of(noisy) - 60.0) < 0.25);
    CHECK(std::abs(variance_of(noisy) / 50.0 - 1.0) < 0.06);

    // Cleared, every pixel has the dark current again (here 0):
    camera.sensor_->clear_dark_current_map();
    CHECK_FALSE(camera.sensor_->has_dark_current_map());
    camera.sensor_->enable_noise(false);
    for (double v : read_dn(camera, frame_buffer)) {
        REQUIRE(std::abs(v - 10.0) < 1e-3);
    }
}

TEST_CASE("A dark current map applies once to a spectral pixel", "[cameras][sensor][maps]")
{
    // A pixel of a spectral type adds up its bins' electrons, then gathers its dark current.
    Visible8Probe camera;
    camera.configure_sensor_from_pitch(Resolution{8, 4}, units::Micrometer(5.0));
    camera.sensor_->set_conversion_gain(1.f);
    camera.sensor_->set_bias_level_dn(0.f);
    camera.sensor_->set_bit_depth(16);
    camera.sensor_->enable_noise(false);
    camera.sensor_->set_dark_current_map(map_of(8, 4, [](int x, int y) { return x + 10 * y; }));
    auto frame_buffer = camera.make_frame_buffer();
    frame_buffer.enable_sensor_response();
    frame_buffer.received_power().fill(Visible8{0.f});

    camera.readout(frame_buffer, units::Second(1.0));
    const Image<float>& response = frame_buffer.sensor_response();
    const double max_dn = std::pow(2.0, 16) - 1.0;
    for (int y = 0; y < 4; ++y) {
        for (int x = 0; x < 8; ++x) {
            REQUIRE(std::abs(static_cast<double>(response(x, y)) * max_dn - (x + 10.0 * y)) < 1e-3);
        }
    }
}

TEST_CASE("A bias map gives each pixel its own bias, as it is or added to the bias level",
          "[cameras][sensor][maps]")
{
    SensorProbe camera;
    electron_counter(camera);
    camera.sensor_->enable_noise(false);
    auto frame_buffer = camera.make_frame_buffer();
    frame_buffer.enable_sensor_response();
    frame_buffer.received_power().fill(RGB{0.f});

    auto check_bias = [&](auto expected) {
        const std::vector<double> dn = read_dn(camera, frame_buffer);
        for (std::size_t i = 0; i < dn.size(); ++i) {
            const auto pixel = static_cast<int>(i / 3);
            REQUIRE(std::abs(dn[i] - expected(pixel % 64, pixel / 64)) < 1e-3);
        }
    };

    // Absolute: each pixel's bias, in place of the bias level.
    const Image<float> master_bias = map_of(64, 64, [](int x, int y) { return 100 + x + 64 * y; });
    camera.sensor_->set_bias_map(master_bias);
    CHECK(camera.sensor_->bias_map_kind() == BiasMapKind::Absolute);
    check_bias([](int x, int y) { return 100.0 + x + 64.0 * y; });
    {
        WarningCapture capture;
        camera.sensor_->set_bias_level_dn(500.f); // not used while the map is set
        CHECK(capture.any("absolute bias map is set"));
    }
    check_bias([](int x, int y) { return 100.0 + x + 64.0 * y; });

    // Relative: offsets from the bias level, which still applies.
    const Image<float> offsets = map_of(64, 64, [](int x, int) { return x % 5 - 2; });
    {
        WarningCapture capture;
        camera.sensor_->set_bias_map(offsets, BiasMapKind::Relative);
        CHECK(capture.warnings.empty()); // the offsets average about 0
    }
    CHECK(camera.sensor_->bias_map_kind() == BiasMapKind::Relative);
    check_bias([](int x, int) { return 500.0 + x % 5 - 2.0; });
    camera.sensor_->set_bias_level_dn(1000.f); // moves the whole map
    check_bias([](int x, int) { return 1000.0 + x % 5 - 2.0; });
    const Image<float> effective = camera.sensor_->bias_map();
    CHECK(effective(3, 7) == 1001.f);
    CHECK(effective(0, 0) == 998.f);

    // Offsets that average far from 0 are probably a map of the bias itself:
    {
        WarningCapture capture;
        camera.sensor_->set_bias_map(master_bias, BiasMapKind::Relative);
        CHECK(capture.any("offsets average"));
    }

    // Cleared, every pixel has the bias level again:
    camera.sensor_->clear_bias_map();
    CHECK_FALSE(camera.sensor_->has_bias_map());
    CHECK(camera.sensor_->bias_map_kind() == BiasMapKind::Absolute);
    check_bias([](int, int) { return 1000.0; });
}

TEST_CASE("An absolute bias map with a sensor bit depth is turned into DN",
          "[cameras][sensor][maps]")
{
    // A sensor response, or a frame read from a file that records its bit depth, holds
    // DN / (2^bits - 1).
    SensorProbe camera;
    electron_counter(camera);
    Image<float> frame = map_of(64, 64, [](int x, int) { return (100.0 + x) / 4095.0; });
    frame.set_sensor_bit_depth(12);
    camera.sensor_->set_bias_map(frame);
    const Image<float> bias = camera.sensor_->bias_map();
    CHECK(bias.sensor_bit_depth() == 0);
    CHECK(std::abs(bias(0, 5) - 100.f) < 1e-3f);
    CHECK(std::abs(bias(63, 5) - 163.f) < 1e-3f);

    // Offsets aren't stored normalized:
    CHECK_THROWS(camera.sensor_->set_bias_map(frame, BiasMapKind::Relative));

    // A map without a bit depth whose values are all at most 1 DN is normalized, as no real
    // bias in DN is, and its DN can't be recovered: it is refused. It was taken as a bias of
    // about 0, with a warning.
    frame.set_sensor_bit_depth(0);
    camera.sensor_->clear_bias_map();
    CHECK_THROWS_WITH(camera.sensor_->set_bias_map(frame),
                      Catch::Matchers::ContainsSubstring("at most 1 DN"));
    CHECK_FALSE(camera.sensor_->has_bias_map());

    // Multiplied back into DN, it is taken; and a map of zeros, which a bias of 0 gives, is too:
    for (std::size_t i = 0; i < frame.size(); ++i) {
        frame[i] *= 4095.f;
    }
    CHECK_NOTHROW(camera.sensor_->set_bias_map(frame));
    CHECK_NOTHROW(camera.sensor_->set_bias_map(Image<float>(64, 64, 0.f)));

    // A relative map's offsets may well be small:
    CHECK_NOTHROW(camera.sensor_->set_bias_level_dn(10.f));
    CHECK_NOTHROW(camera.sensor_->set_bias_map(Image<float>(64, 64, 0.25f), BiasMapKind::Relative));
}

TEST_CASE("Maps are checked when set, and when the bias or bit depth changes",
          "[cameras][sensor][maps]")
{
    SensorProbe camera;
    electron_counter(camera); // 20 bits: DN up to 1048575
    camera.sensor_->set_bias_level_dn(10.f);
    const float nan = std::numeric_limits<float>::quiet_NaN();
    const float inf = std::numeric_limits<float>::infinity();

    auto with = [](Image<float> map, int x, int y, float value) {
        map(x, y) = value;
        return map;
    };
    const Image<float> ones(64, 64, 1.f);

    // The sensor's resolution:
    CHECK_THROWS(camera.sensor_->set_dark_current_map(Image<float>(32, 64, 1.f)));
    CHECK_THROWS(camera.sensor_->set_bias_map(Image<float>(64, 32, 100.f)));

    // Dark currents are non-negative and finite:
    CHECK_THROWS(camera.sensor_->set_dark_current_map(with(ones, 3, 4, -1.f)));
    CHECK_THROWS(camera.sensor_->set_dark_current_map(with(ones, 3, 4, nan)));
    CHECK_THROWS(camera.sensor_->set_dark_current_map(with(ones, 3, 4, inf)));
    CHECK_FALSE(camera.sensor_->has_dark_current_map()); // a map that fails is not set
    CHECK_NOTHROW(camera.sensor_->set_dark_current_map(with(ones, 3, 4, 0.f)));

    // Each pixel's bias is finite and in [0, 2^bits - 1):
    const Image<float> hundreds(64, 64, 100.f);
    CHECK_THROWS(camera.sensor_->set_bias_map(with(hundreds, 1, 2, -1.f)));
    CHECK_THROWS(camera.sensor_->set_bias_map(with(hundreds, 1, 2, nan)));
    CHECK_THROWS(camera.sensor_->set_bias_map(with(hundreds, 1, 2, 1048575.f)));
    CHECK_FALSE(camera.sensor_->has_bias_map());
    CHECK_NOTHROW(camera.sensor_->set_bias_map(with(hundreds, 1, 2, 1048574.f)));

    // A relative map, from the bias level:
    const Image<float> zeros(64, 64, 0.f);
    CHECK_THROWS(camera.sensor_->set_bias_map(with(zeros, 1, 2, -11.f), BiasMapKind::Relative));
    CHECK_NOTHROW(camera.sensor_->set_bias_map(with(zeros, 1, 2, -10.f), BiasMapKind::Relative));
    CHECK_THROWS(camera.sensor_->set_bias_level_dn(9.f)); // that pixel would be at -1 DN
    CHECK(camera.sensor_->bias_level_dn() == 10.f);
    CHECK_NOTHROW(camera.sensor_->set_bias_level_dn(20.f));

    // A lower bit depth must still hold every pixel's bias:
    camera.sensor_->set_bias_map(with(hundreds, 1, 2, 300.f));
    CHECK_THROWS(camera.sensor_->set_bit_depth(8));
    CHECK(camera.sensor_->bit_depth() == 20);
    CHECK_NOTHROW(camera.sensor_->set_bit_depth(9));
}

TEST_CASE("A map of another size than the frame makes the readout throw", "[cameras][sensor][maps]")
{
    SensorProbe camera;
    electron_counter(camera);
    camera.sensor_->set_dark_current_map(Image<float>(64, 64, 1.f));
    camera.sensor_->set_bias_map(Image<float>(64, 64, 100.f));

    {
        WarningCapture capture;
        camera.configure_sensor_from_pitch(Resolution{32, 32}, units::Micrometer(5.0));
        CHECK(capture.any("dark current map is 64x64, and the sensor is now 32x32"));
        CHECK(capture.any("bias map is 64x64, and the sensor is now 32x32"));
    }
    auto frame_buffer = camera.make_frame_buffer();
    frame_buffer.enable_sensor_response();
    CHECK_THROWS(camera.readout(frame_buffer, units::Second(1.0)));

    camera.sensor_->clear_dark_current_map();
    CHECK_THROWS(camera.readout(frame_buffer, units::Second(1.0)));
    camera.sensor_->clear_bias_map();
    CHECK_NOTHROW(camera.readout(frame_buffer, units::Second(1.0)));
}

TEST_CASE("Without maps, the map getters give the single values everywhere",
          "[cameras][sensor][maps]")
{
    SensorProbe camera;
    camera.configure_sensor_from_pitch(Resolution{6, 4}, units::Micrometer(5.0));
    camera.sensor_->set_dark_current(3.f);
    camera.sensor_->set_bias_level_dn(42.f);

    const Image<float> dark = camera.sensor_->dark_current_map();
    const Image<float> bias = camera.sensor_->bias_map();
    REQUIRE(dark.width() == 6);
    REQUIRE(dark.height() == 4);
    REQUIRE(bias.width() == 6);
    REQUIRE(bias.height() == 4);
    for (std::size_t i = 0; i < dark.size(); ++i) {
        REQUIRE(dark[i] == 3.f);
        REQUIRE(bias[i] == 42.f);
    }

    // With maps, they give them:
    const Image<float> rates = map_of(6, 4, [](int x, int y) { return x * y; });
    camera.sensor_->set_dark_current_map(rates);
    {
        WarningCapture capture;
        camera.sensor_->set_dark_current(5.f); // not used while the map is set
        CHECK(capture.any("dark current map is set"));
    }
    CHECK(camera.sensor_->dark_current_map()(5, 3) == 15.f);
    CHECK(camera.sensor_->dark_current() == 5.f);
}

TEST_CASE("A copied sensor shares the maps", "[cameras][sensor][maps]")
{
    SensorProbe camera;
    electron_counter(camera);
    camera.sensor_->set_bias_level_dn(10.f);
    camera.sensor_->set_dark_current_map(Image<float>(64, 64, 7.f));
    camera.sensor_->set_bias_map(map_of(64, 64, [](int x, int) { return x % 2 == 0 ? 2 : -2; }),
                                 BiasMapKind::Relative);

    SensorProbe other;
    other.set_sensor<SimpleSensor<RGB>>(
        *dynamic_cast<const SimpleSensor<RGB>*>(camera.sensor_.get()));
    REQUIRE(other.sensor_->has_dark_current_map());
    REQUIRE(other.sensor_->has_bias_map());
    CHECK(other.sensor_->bias_map_kind() == BiasMapKind::Relative);
    CHECK(other.sensor_->dark_current_map()(10, 10) == 7.f);

    // Each sensor's maps are its own to replace or clear:
    camera.sensor_->clear_dark_current_map();
    camera.sensor_->clear_bias_map();
    CHECK(other.sensor_->has_dark_current_map());
    CHECK(other.sensor_->has_bias_map());
}

TEST_CASE("dark_current_from_master_dark gives (dark - bias) * gain / exposure",
          "[cameras][sensor][maps]")
{
    const Image<float> dark = map_of(4, 3, [](int x, int y) { return 110 + 10 * x + y; });
    const Image<float> bias = map_of(4, 3, [](int x, int) { return 100 + x; });

    // e- / s = (dark - bias) DN * 2 e-/DN / 4 s:
    const Image<float> rates = dark_current_from_master_dark(dark, bias, 2.f, units::Second(4.0));
    REQUIRE(rates.width() == 4);
    REQUIRE(rates.height() == 3);
    for (int y = 0; y < 3; ++y) {
        for (int x = 0; x < 4; ++x) {
            REQUIRE(std::abs(rates(x, y) - (10.0 + 9.0 * x + y) * 0.5) < 1e-5);
        }
    }

    // One bias for every pixel, and the exposure in any unit:
    const Image<float> flat_bias =
        dark_current_from_master_dark(dark, 100.f, 2.f, units::Millisecond(4000.0));
    CHECK(std::abs(flat_bias(3, 2) - (142.0 - 100.0) * 0.5) < 1e-5);

    // Images with a sensor bit depth are turned into DN:
    Image<float> normalized_dark = dark;
    for (std::size_t i = 0; i < normalized_dark.size(); ++i) {
        normalized_dark[i] = static_cast<float>(normalized_dark[i] / 4095.0);
    }
    normalized_dark.set_sensor_bit_depth(12);
    const Image<float> from_normalized =
        dark_current_from_master_dark(normalized_dark, bias, 2.f, units::Second(4.0));
    CHECK(std::abs(from_normalized(2, 1) - rates(2, 1)) < 1e-3);

    // A pixel below the bias, as noise near zero dark current gives, has no dark current:
    {
        WarningCapture capture;
        Image<float> low = dark;
        low(1, 1) = 50.f;
        low(2, 2) = 80.f;
        const Image<float> clamped =
            dark_current_from_master_dark(low, bias, 2.f, units::Second(4.0));
        CHECK(clamped(1, 1) == 0.f);
        CHECK(clamped(2, 2) == 0.f);
        CHECK(capture.any("2 pixel(s) of the master dark are below the master bias"));
    }

    // A master dark or bias that is normalized, without a bit depth to give back its DN, is
    // refused, as no real one in DN is all at most 1 DN. It gave a dark current of about 0, with
    // a warning.
    CHECK_THROWS_WITH(
        dark_current_from_master_dark(Image<float>(4, 3, 0.5f), 0.f, 2.f, units::Second(4.0)),
        Catch::Matchers::ContainsSubstring("master dark is at most 1 DN"));
    CHECK_THROWS_WITH(
        dark_current_from_master_dark(dark, Image<float>(4, 3, 0.01f), 2.f, units::Second(4.0)),
        Catch::Matchers::ContainsSubstring("master bias is at most 1 DN"));
    // Unless it has a bit depth (normalized_dark, above), or is all zeros:
    CHECK_NOTHROW(
        dark_current_from_master_dark(dark, Image<float>(4, 3, 0.f), 2.f, units::Second(4.0)));

    // What can't be used:
    Image<float> missing = dark;
    missing(0, 0) = std::numeric_limits<float>::quiet_NaN();
    CHECK_THROWS(dark_current_from_master_dark(missing, bias, 2.f, units::Second(4.0)));
    CHECK_THROWS(
        dark_current_from_master_dark(dark, Image<float>(3, 4, 100.f), 2.f, units::Second(4.0)));
    CHECK_THROWS(dark_current_from_master_dark(dark, bias, 0.f, units::Second(4.0)));
    CHECK_THROWS(dark_current_from_master_dark(dark, bias, 2.f, units::Second(0.0)));
    CHECK_THROWS(dark_current_from_master_dark(dark, bias, 2.f, units::Second(-1.0)));
    CHECK_THROWS(dark_current_from_master_dark(
        dark, bias, 2.f, units::Second(std::numeric_limits<double>::infinity())));
    CHECK_THROWS(dark_current_from_master_dark(Image<float>(), 0.f, 2.f, units::Second(4.0)));
}

TEST_CASE("A dark current map is recovered from the sensor's own calibration frames",
          "[cameras][sensor][maps]")
{
    // Master darks and biases averaged from the simulated sensor's frames, as from a real
    // camera's, give back the dark current it was given.
    SensorProbe camera;
    electron_counter(camera);
    camera.sensor_->set_conversion_gain(2.f);
    camera.sensor_->set_read_noise(5.f);
    camera.sensor_->set_bias_map(map_of(64, 64, [](int x, int) { return 200 + 3 * (x % 8); }));
    Image<float> rates = map_of(64, 64, [](int x, int) { return x < 32 ? 20.0 : 80.0; });
    rates(10, 10) = 5000.f; // a hot pixel
    camera.sensor_->set_dark_current_map(rates);
    camera.sensor_->set_noise_seed(11);

    auto frame_buffer = camera.make_frame_buffer();
    frame_buffer.enable_sensor_response();
    frame_buffer.received_power().fill(RGB{0.f});

    // The mean of 16 frames, and of each pixel's three channels:
    auto master = [&](double seconds) {
        Image<float> sum(64, 64, 0.f);
        constexpr int FRAMES = 16;
        for (int frame = 0; frame < FRAMES; ++frame) {
            camera.readout(frame_buffer, units::Second(seconds));
            const Image<RGB>& response = frame_buffer.sensor_response();
            for (std::size_t i = 0; i < sum.size(); ++i) {
                sum[i] += (response[i][0] + response[i][1] + response[i][2]) / (3.f * FRAMES);
            }
        }
        sum.set_sensor_bit_depth(20); // the sensor response's
        return sum;
    };
    const Image<float> master_bias = master(0.0);
    const Image<float> master_dark = master(10.0);

    const Image<float> recovered =
        dark_current_from_master_dark(master_dark, master_bias, 2.f, units::Second(10.0));
    double left = 0.0;
    double right = 0.0;
    for (int y = 0; y < 64; ++y) {
        for (int x = 0; x < 64; ++x) {
            if (x == 10 && y == 10) {
                continue;
            }
            (x < 32 ? left : right) += static_cast<double>(recovered(x, y));
        }
    }
    CHECK(std::abs(left / (32.0 * 64.0 - 1.0) - 20.0) < 0.2);
    CHECK(std::abs(right / (32.0 * 64.0) - 80.0) < 0.4);
    CHECK(std::abs(recovered(10, 10) - 5000.f) < 25.f);
}

TEST_CASE("The camera's description includes the maps", "[cameras][sensor][maps]")
{
    SensorProbe camera;
    camera.configure_sensor_from_pitch(Resolution{4, 4}, units::Micrometer(5.0));
    CHECK(camera.describe().find("bias 10 DN, mean QE") != std::string::npos);
    CHECK(camera.describe().find("dark current 1 e-/s") != std::string::npos);

    camera.sensor_->set_bias_map(map_of(4, 4, [](int x, int) { return 20 + x; }));
    camera.sensor_->set_dark_current_map(Image<float>(4, 4, 2.f));
    std::string text = camera.describe();
    INFO(text);
    CHECK(text.find("bias map (mean 21.5, from 20 to 23 DN)") != std::string::npos);
    CHECK(text.find("dark current map (mean 2, from 2 to 2 e-/s)") != std::string::npos);

    camera.sensor_->set_bias_map(map_of(4, 4, [](int x, int) { return x - 1.5; }),
                                 BiasMapKind::Relative);
    text = camera.describe();
    CHECK(text.find("bias 10 DN + map (offsets mean 0, from -1.5 to 1.5 DN)") != std::string::npos);
}
