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

namespace {

/// The response of one channel, in DN, as an image.
Image<float> read_channel_dn(SensorProbe& camera, FrameBuffer<RGB>& frame_buffer, std::size_t c)
{
    camera.readout(frame_buffer, units::Second(1.0));
    const double max_dn = std::pow(2.0, camera.sensor_->bit_depth()) - 1.0;
    Image<float> dn = frame_buffer.sensor_response().get_channel(c);
    for (std::size_t i = 0; i < dn.size(); ++i) {
        dn[i] = static_cast<float>(std::round(static_cast<double>(dn[i]) * max_dn));
    }
    return dn;
}

/// A dark sensor whose only noise is its row and column noise: 1 e-/DN, a bias of 1000 DN.
void banded_sensor(SensorProbe& camera, int width, int height)
{
    camera.configure_sensor_from_pitch(Resolution{width, height}, units::Micrometer(5.0));
    camera.sensor_->set_conversion_gain(1.f);
    camera.sensor_->set_bias_level_dn(1000.f);
    camera.sensor_->set_dark_current(0.f);
    camera.sensor_->set_read_noise(0.f);
    camera.sensor_->set_full_well_capacity(1e9f);
    camera.sensor_->set_bit_depth(20);
    camera.sensor_->set_noise_seed(3);
}

} // namespace

TEST_CASE("Row noise shifts each row's pixels together, differently in each frame",
          "[cameras][sensor][banding]")
{
    SensorProbe camera;
    banded_sensor(camera, 64, 256);
    camera.sensor_->set_row_noise(20.f);
    CHECK(camera.sensor_->row_noise() == 20.f);
    auto frame_buffer = camera.make_frame_buffer();
    frame_buffer.enable_sensor_response();
    frame_buffer.received_power().fill(RGB{0.f});

    std::vector<double> row_offsets;
    std::vector<double> first_frame;
    for (int frame = 0; frame < 4; ++frame) {
        const Image<float> red = read_channel_dn(camera, frame_buffer, 0);
        const Image<float> blue = read_channel_dn(camera, frame_buffer, 2);
        (void)blue; // a second readout, a new frame
        for (int y = 0; y < red.height(); ++y) {
            // Every pixel of a row, in every channel, shifts together:
            for (int x = 0; x < red.width(); ++x) {
                REQUIRE(red(x, y) == red(0, y));
            }
            row_offsets.push_back(static_cast<double>(red(0, y)) - 1000.0);
        }
        if (frame == 0) {
            first_frame.assign(row_offsets.begin(), row_offsets.end());
        }
    }
    const Image<RGB>& response = frame_buffer.sensor_response();
    CHECK(response(5, 7)[0] == response(5, 7)[1]);
    CHECK(response(5, 7)[1] == response(5, 7)[2]);

    // 1024 rows: mean 0, variance 400 (plus the rounding's 1/12):
    CHECK(std::abs(mean_of(row_offsets)) < 2.0);
    CHECK(std::abs(variance_of(row_offsets) / (400.0 + 1.0 / 12.0) - 1.0) < 0.15);

    // Successive frames differ, and the seed repeats them:
    CHECK(std::vector<double>(row_offsets.begin() + 256, row_offsets.begin() + 512) != first_frame);
    camera.sensor_->set_noise_seed(3);
    const Image<float> again = read_channel_dn(camera, frame_buffer, 0);
    for (int y = 0; y < 256; ++y) {
        REQUIRE(static_cast<double>(again(0, y)) - 1000.0 ==
                first_frame[static_cast<std::size_t>(y)]);
    }
}

TEST_CASE("Column noise shifts each column's pixels together, differently in each frame",
          "[cameras][sensor][banding]")
{
    SensorProbe camera;
    banded_sensor(camera, 256, 64);
    camera.sensor_->set_column_noise(20.f);
    CHECK(camera.sensor_->column_noise() == 20.f);
    auto frame_buffer = camera.make_frame_buffer();
    frame_buffer.enable_sensor_response();
    frame_buffer.received_power().fill(RGB{0.f});

    std::vector<double> column_offsets;
    std::vector<double> first_frame;
    for (int frame = 0; frame < 4; ++frame) {
        const Image<float> green = read_channel_dn(camera, frame_buffer, 1);
        for (int x = 0; x < green.width(); ++x) {
            for (int y = 0; y < green.height(); ++y) {
                REQUIRE(green(x, y) == green(x, 0));
            }
            column_offsets.push_back(static_cast<double>(green(x, 0)) - 1000.0);
        }
        if (frame == 0) {
            first_frame.assign(column_offsets.begin(), column_offsets.end());
        }
    }
    CHECK(std::abs(mean_of(column_offsets)) < 2.0);
    CHECK(std::abs(variance_of(column_offsets) / (400.0 + 1.0 / 12.0) - 1.0) < 0.15);
    CHECK(std::vector<double>(column_offsets.begin() + 256, column_offsets.begin() + 512) !=
          first_frame);
}

TEST_CASE("Row and column noise add, independently of each other and of the pixels' noise",
          "[cameras][sensor][banding]")
{
    SensorProbe camera;
    banded_sensor(camera, 32, 32);
    auto frame_buffer = camera.make_frame_buffer();
    frame_buffer.enable_sensor_response();
    expect_electrons(camera, frame_buffer, 50.0); // shot noise, from the pixels' own numbers

    // The pixels' noise alone:
    camera.sensor_->set_noise_seed(9);
    const Image<float> plain = read_channel_dn(camera, frame_buffer, 0);

    // With row and column noise, from the same seed: each pixel's value differs from it by its
    // row's and its column's draws, rounded, whatever the pixel's own noise was. So the pixels'
    // noise is unchanged:
    camera.sensor_->set_row_noise(30.f);
    camera.sensor_->set_column_noise(10.f);
    camera.sensor_->set_noise_seed(9);
    const Image<float> banded = read_channel_dn(camera, frame_buffer, 0);
    auto shift = [&](int x, int y) { return static_cast<double>(banded(x, y) - plain(x, y)); };
    bool any_shift = false;
    for (int y = 0; y < 32; ++y) {
        for (int x = 0; x < 32; ++x) {
            // Additive: shift(x, y) = row(y) + column(x), to within the rounding of each.
            REQUIRE(std::abs(shift(x, y) - shift(x, 0) - shift(0, y) + shift(0, 0)) <= 2.0);
            any_shift = any_shift || shift(x, y) != 0.0;
        }
    }
    CHECK(any_shift);

    // Without the row noise, the columns' draws are the same as with it: the rows' numbers are
    // their own.
    camera.sensor_->set_row_noise(0.f);
    camera.sensor_->set_noise_seed(9);
    const Image<float> columns_only = read_channel_dn(camera, frame_buffer, 0);
    auto column = [&](int x) { return static_cast<double>(columns_only(x, 0) - plain(x, 0)); };
    for (int x = 0; x < 32; ++x) {
        for (int y = 0; y < 32; ++y) {
            REQUIRE(static_cast<double>(columns_only(x, y) - plain(x, y)) == column(x));
        }
        CHECK(std::abs((column(x) - column(0)) - (shift(x, 0) - shift(0, 0))) <= 2.0);
    }
}

TEST_CASE("Row and column noise are off with the other noise, and checked when set",
          "[cameras][sensor][banding]")
{
    SensorProbe camera;
    banded_sensor(camera, 16, 16);
    camera.sensor_->set_row_noise(50.f);
    camera.sensor_->set_column_noise(50.f);
    camera.sensor_->enable_noise(false);
    auto frame_buffer = camera.make_frame_buffer();
    frame_buffer.enable_sensor_response();
    frame_buffer.received_power().fill(RGB{0.f});
    for (double v : read_dn(camera, frame_buffer)) {
        REQUIRE(std::abs(v - 1000.0) < 1e-3);
    }

    // Default to 0, and are copied with the sensor's settings:
    SensorProbe fresh;
    CHECK(fresh.sensor_->row_noise() == 0.f);
    CHECK(fresh.sensor_->column_noise() == 0.f);
    SensorProbe other;
    other.set_sensor<SimpleSensor<RGB>>(
        *dynamic_cast<const SimpleSensor<RGB>*>(camera.sensor_.get()));
    CHECK(other.sensor_->row_noise() == 50.f);
    CHECK(other.sensor_->column_noise() == 50.f);

    for (const float bad :
         {-1.f, std::numeric_limits<float>::quiet_NaN(), std::numeric_limits<float>::infinity()}) {
        CHECK_THROWS(camera.sensor_->set_row_noise(bad));
        CHECK_THROWS(camera.sensor_->set_column_noise(bad));
    }
    CHECK(camera.sensor_->row_noise() == 50.f);

    CHECK(camera.describe().find("row noise 50 e-, column noise 50 e-") != std::string::npos);
}

TEST_CASE("A filter wheel's filters are selected by index or name, and the first by default",
          "[cameras][sensor][filters]")
{
    SensorProbe camera;
    electron_counter(camera); // 64x64, 1 e-/DN, no dark current, bias or read noise
    camera.sensor_->enable_noise(false);
    CHECK(camera.filter_count() == 0);
    CHECK_FALSE(camera.selected_filter().has_value());
    CHECK_FALSE(camera.selected_filter_name().has_value());
    CHECK_THROWS(camera.select_filter(0));
    CHECK_THROWS(camera.select_filter("R"));
    CHECK_THROWS(camera.filter(0));
    CHECK_THROWS(camera.filter_index("R"));

    CHECK(camera.add_filter("R", RGB{0.9f, 0.1f, 0.f}) == 0);
    CHECK(camera.selected_filter() == 0); // the first is selected
    CHECK(camera.add_filter("G", RGB{0.1f, 0.8f, 0.1f}) == 1);
    CHECK(camera.add_filter("B", RGB{0.f, 0.1f, 0.7f}) == 2);
    CHECK(camera.filter_count() == 3);
    CHECK(camera.selected_filter() == 0); // still
    CHECK(camera.selected_filter_name() == "R");

    camera.select_filter("B");
    CHECK(camera.selected_filter() == 2);
    camera.select_filter(1);
    CHECK(camera.selected_filter_name() == "G");
    CHECK(camera.filter_index("B") == 2);
    CHECK(camera.filter(2).name == "B");
    CHECK(camera.filter("G").transmission[1] == 0.8f);
    CHECK(camera.filter("G").flat == nullptr);

    // Names are unique, non-empty; transmissions are ratios:
    CHECK_THROWS(camera.add_filter("G", RGB{0.5f}));
    CHECK_THROWS(camera.add_filter("", RGB{0.5f}));
    CHECK_THROWS(camera.add_filter("bad", RGB{1.5f, 0.5f, 0.5f}));
    CHECK_THROWS(camera.add_filter("bad", RGB{-0.1f, 0.5f, 0.5f}));
    CHECK(camera.filter_count() == 3);
    CHECK_THROWS(camera.select_filter(3));
    CHECK_THROWS(camera.select_filter("U"));
    CHECK(camera.selected_filter() == 1); // unchanged by what failed

    // Each channel's signal is scaled by the selected filter's transmission in its bin:
    auto frame_buffer = camera.make_frame_buffer();
    frame_buffer.enable_sensor_response();
    expect_electrons(camera, frame_buffer, 1000.0);
    std::vector<double> dn = read_dn(camera, frame_buffer);
    CHECK(std::abs(dn[0] - 100.0) < 1e-3); // R through G: 0.1
    CHECK(std::abs(dn[1] - 800.0) < 1e-3);
    CHECK(std::abs(dn[2] - 100.0) < 1e-3);
    camera.select_filter("R");
    dn = read_dn(camera, frame_buffer);
    CHECK(std::abs(dn[0] - 900.0) < 1e-3);
    CHECK(std::abs(dn[2] - 0.0) < 1e-3);

    // Emptied, the camera has a clear filter again:
    camera.clear_filters();
    CHECK(camera.filter_count() == 0);
    CHECK_FALSE(camera.selected_filter().has_value());
    dn = read_dn(camera, frame_buffer);
    CHECK(std::abs(dn[0] - 1000.0) < 1e-3);
    CHECK(std::abs(dn[2] - 1000.0) < 1e-3);
    CHECK(camera.add_filter("clear", RGB{1.f}) == 0); // indices start over
}

TEST_CASE("A filter's flat scales each pixel's signal before shot noise",
          "[cameras][sensor][filters]")
{
    SensorProbe camera;
    electron_counter(camera);
    const Image<float> flat =
        map_of(64, 64, [](int x, int y) { return 0.5 + 0.5 * x / 63.0 - 0.2 * y / 63.0; });
    camera.add_filter("V", RGB{1.f}, flat);
    REQUIRE(camera.filter("V").flat != nullptr);
    CHECK(camera.sensor_->has_flat());
    CHECK(camera.sensor_->flat()(63, 0) == 1.f);

    // Without noise, exactly the flat times the signal, per channel:
    camera.sensor_->enable_noise(false);
    auto frame_buffer = camera.make_frame_buffer();
    frame_buffer.enable_sensor_response();
    expect_electrons(camera, frame_buffer, 1000.0);
    const std::vector<double> dn = read_dn(camera, frame_buffer);
    for (std::size_t i = 0; i < dn.size(); ++i) {
        const auto pixel = static_cast<int>(i / 3);
        const double expected =
            std::round(1000.0 * static_cast<double>(flat(pixel % 64, pixel / 64)));
        REQUIRE(std::abs(dn[i] - expected) < 1e-3);
    }

    // With noise, it is the mean the shot noise is drawn around: Poisson with the scaled mean.
    camera.sensor_->enable_noise(true);
    camera.add_filter("dim", RGB{1.f}, Image<float>(64, 64, 0.1f));
    camera.select_filter("dim");
    expect_electrons(camera, frame_buffer, 400.0);
    const std::vector<double> noisy = read_dn(camera, frame_buffer);
    CHECK(std::abs(mean_of(noisy) - 40.0) < 0.3);
    CHECK(std::abs(variance_of(noisy) / 40.0 - 1.0) < 0.1);

    // The dark current is the pixel's own, not through the flat:
    camera.sensor_->enable_noise(false);
    camera.sensor_->set_dark_current(50.f);
    frame_buffer.received_power().fill(RGB{0.f});
    for (double v : read_dn(camera, frame_buffer)) {
        REQUIRE(std::abs(v - 50.0) < 1e-3);
    }

    // A flat is checked: the sensor's resolution, finite, non-negative:
    CHECK_THROWS(camera.add_filter("small", RGB{1.f}, Image<float>(32, 32, 1.f)));
    Image<float> bad(64, 64, 1.f);
    bad(3, 3) = -0.1f;
    CHECK_THROWS(camera.add_filter("negative", RGB{1.f}, bad));
    bad(3, 3) = std::numeric_limits<float>::quiet_NaN();
    CHECK_THROWS(camera.add_filter("nan", RGB{1.f}, bad));
    CHECK(camera.filter_count() == 2);
    bad(3, 3) = 0.f; // a dead pixel is fine
    CHECK_NOTHROW(camera.add_filter("dead", RGB{1.f}, bad));
}

TEST_CASE("The response is warned of above 1 electron per photon", "[cameras][sensor][filters]")
{
    SensorProbe camera;
    camera.configure_sensor_from_pitch(Resolution{8, 8}, units::Micrometer(5.0));
    camera.sensor_->set_quantum_efficiency(RGB{0.9f});
    {
        WarningCapture capture;
        camera.add_filter("fine", RGB{1.f}, Image<float>(8, 8, 1.1f)); // 0.99
        CHECK_FALSE(capture.any("more electrons"));
        camera.add_filter("too much", RGB{1.f}, Image<float>(8, 8, 1.2f)); // 1.08, not selected
        CHECK_FALSE(capture.any("more electrons"));
        camera.select_filter("too much");
        CHECK(capture.any("more electrons"));
    }
    {
        WarningCapture capture;
        camera.select_filter("fine");
        camera.sensor_->set_quantum_efficiency(RGB{0.95f}); // 1.045 through 'fine'
        CHECK(capture.any("more electrons"));
    }
}

TEST_CASE("The selected filter follows the camera to a new sensor, and its flat must fit",
          "[cameras][sensor][filters]")
{
    SensorProbe camera;
    electron_counter(camera);
    camera.add_filter("R", RGB{0.5f, 0.f, 0.f}, Image<float>(64, 64, 0.5f));

    // A new sensor is behind the same wheel:
    camera.set_sensor<SimpleSensor<RGB>>(
        *dynamic_cast<const SimpleSensor<RGB>*>(camera.sensor_.get()));
    CHECK(camera.sensor_->filter_transmission()[0] == 0.5f);
    CHECK(camera.sensor_->has_flat());
    CHECK(camera.selected_filter_name() == "R");

    // A copy of the sensor carries the filter it was behind; given to another camera, it is
    // behind that camera's wheel instead, here an empty one:
    const SimpleSensor<RGB> copy(*dynamic_cast<const SimpleSensor<RGB>*>(camera.sensor_.get()));
    CHECK(copy.has_flat());
    CHECK(copy.filter_transmission()[0] == 0.5f);
    SensorProbe other;
    other.set_sensor<SimpleSensor<RGB>>(copy);
    CHECK(other.filter_count() == 0);
    CHECK_FALSE(other.sensor_->has_flat());
    CHECK(other.sensor_->filter_transmission()[0] == 1.f);

    // The sensor reconfigured to another size: the flat no longer fits, and a readout throws
    // until the filter is replaced or the wheel cleared.
    {
        WarningCapture capture;
        camera.configure_sensor_from_pitch(Resolution{32, 32}, units::Micrometer(5.0));
        CHECK(capture.any("filter's flat is 64x64, and the sensor is now 32x32"));
    }
    auto frame_buffer = camera.make_frame_buffer();
    frame_buffer.enable_sensor_response();
    CHECK_THROWS(camera.readout(frame_buffer, units::Second(1.0)));
    camera.clear_filters();
    CHECK_NOTHROW(camera.readout(frame_buffer, units::Second(1.0)));
}

TEST_CASE("flat_from_measured divides out the rendered falloff and normalizes to the center",
          "[cameras][sensor][filters]")
{
    // A wide lens, where the natural falloff is large: 50 x 40 px of 100 um at 2 mm.
    SensorProbe camera;
    camera.configure_sensor_from_pitch(Resolution{50, 40}, units::Micrometer(100.0));
    camera.set_focal_length(units::Millimeter(2.0));

    // A flat field with only the falloff the renderer gives: the flat is 1 everywhere (to the
    // precision of the geometry), whatever its scale.
    Image<float> natural(50, 40);
    for (int y = 0; y < 40; ++y) {
        for (int x = 0; x < 50; ++x) {
            natural(x, y) = 12345.f * camera.pixel_radiance_to_power(x, y);
        }
    }
    const Image<float> unity = camera.flat_from_measured(natural);
    CHECK(camera.pixel_radiance_to_power(0, 0) < 0.5f * camera.pixel_radiance_to_power(25, 20));
    for (std::size_t i = 0; i < unity.size(); ++i) {
        REQUIRE(std::abs(unity[i] - 1.f) < 1e-4f);
    }

    // More vignetting than that, and PRNU, is kept, relative to the center:
    Image<float> measured = natural;
    for (int y = 0; y < 40; ++y) {
        for (int x = 0; x < 50; ++x) {
            const double dx = (x + 0.5 - 25.0) / 25.0;
            const double dy = (y + 0.5 - 20.0) / 20.0;
            measured(x, y) *= static_cast<float>((1.0 - 0.3 * (dx * dx + dy * dy)) *
                                                 (1.0 + 0.01 * ((x * 7 + y * 3) % 5 - 2)));
        }
    }
    const Image<float> flat = camera.flat_from_measured(measured);
    CHECK(std::abs(flat(25, 20) - 1.f) < 0.03f); // the center, to within its PRNU
    CHECK(std::abs(flat(0, 0) - 0.4f) < 0.03f);  // 1 - 0.3 * 2
    CHECK(flat(49, 39) < flat(25, 20));
    CHECK(flat.sensor_bit_depth() == 0);

    // The central patch's median sets the scale: a dust mote there does not, beyond moving the
    // median a rank or two through the PRNU (here steps of 1%).
    Image<float> dusty = measured;
    dusty(25, 20) *= 0.1f;
    dusty(24, 20) *= 0.1f;
    const Image<float> flat_dusty = camera.flat_from_measured(dusty);
    CHECK(std::abs(flat_dusty(10, 10) - flat(10, 10)) < 0.01f);
    CHECK(flat_dusty(25, 20) < 0.15f);

    // A larger patch:
    const Image<float> wide = camera.flat_from_measured(measured, 0.5f);
    CHECK(wide(25, 20) > flat(25, 20)); // normalized to a dimmer, wider median

    // What can't be used:
    CHECK_THROWS(camera.flat_from_measured(Image<float>(40, 50, 1.f)));
    CHECK_THROWS(camera.flat_from_measured(measured, 0.f));
    CHECK_THROWS(camera.flat_from_measured(measured, 1.5f));
    CHECK_THROWS(camera.flat_from_measured(Image<float>(50, 40, 0.f))); // no signal at the center
    Image<float> missing = measured;
    missing(5, 5) = std::numeric_limits<float>::quiet_NaN();
    CHECK_THROWS(camera.flat_from_measured(missing));

    // The flat is for the wheel, and gives back the measured response in a render of a uniform
    // scene: the rendered falloff times the flat is the measured falloff.
    camera.add_filter("V", RGB{1.f}, flat);
    for (int x : {0, 12, 25, 40, 49}) {
        const double rendered = camera.pixel_radiance_to_power(x, 20) * flat(x, 20);
        const double reference = camera.pixel_radiance_to_power(25, 20) * flat(25, 20);
        CHECK(std::abs(rendered / reference - measured(x, 20) / measured(25, 20)) < 1e-3);
    }
}

TEST_CASE("The camera's description lists the filter wheel", "[cameras][sensor][filters]")
{
    SensorProbe camera;
    camera.configure_sensor_from_pitch(Resolution{4, 4}, units::Micrometer(5.0));
    CHECK(camera.describe().find("Filters:     none (clear)") != std::string::npos);
    camera.add_filter("R", RGB{0.6f, 0.3f, 0.f});
    camera.add_filter("G", RGB{0.3f}, Image<float>(4, 4, 0.9f));
    std::string text = camera.describe();
    INFO(text);
    CHECK(text.find("2 in the wheel, selected 'R' (0): transmission mean 0.3, flat none") !=
          std::string::npos);
    camera.select_filter("G");
    text = camera.describe();
    CHECK(text.find(
              "selected 'G' (1): transmission mean 0.3, flat map (mean 0.9, from 0.9 to 0.9)") !=
          std::string::npos);
}

TEST_CASE("PRNU gives each pixel a gain of its own, the same in every frame",
          "[cameras][sensor][fixed]")
{
    SensorProbe camera;
    electron_counter(camera); // 64x64, 1 e-/DN
    camera.sensor_->set_noise_seed(21);
    camera.sensor_->enable_noise(false);
    auto frame_buffer = camera.make_frame_buffer();
    frame_buffer.enable_sensor_response();
    expect_electrons(camera, frame_buffer, 10000.0);

    // Without it, every pixel is the same:
    for (double v : read_dn(camera, frame_buffer)) {
        REQUIRE(std::abs(v - 10000.0) < 1e-3);
    }
    CHECK(camera.sensor_->prnu_map()(5, 5) == 1.f);

    camera.sensor_->set_prnu(0.02f);
    CHECK(camera.sensor_->prnu() == 0.02f);
    const Image<float> gains = camera.sensor_->prnu_map();
    const std::vector<double> first = read_dn(camera, frame_buffer);
    std::vector<double> ratios;
    for (std::size_t i = 0; i < first.size(); ++i) {
        const auto pixel = static_cast<int>(i / 3);
        const double gain = static_cast<double>(gains(pixel % 64, pixel / 64));
        // Each pixel's signal, by its gain (to the rounding of a float's electrons to DN); its
        // three channels alike:
        REQUIRE(std::abs(first[i] - 10000.0 * gain) <= 0.5 + 1e-2);
        REQUIRE(first[i] == first[i - i % 3]);
        if (i % 3 == 0) {
            ratios.push_back(first[i] / 10000.0);
        }
    }
    // 4096 gains: mean 1, standard deviation 0.02.
    CHECK(std::abs(mean_of(ratios) - 1.0) < 0.002);
    CHECK(std::abs(std::sqrt(variance_of(ratios)) / 0.02 - 1.0) < 0.06);

    // The same in the next frame, and with the seed again; another with another seed:
    CHECK(read_dn(camera, frame_buffer) == first);
    camera.sensor_->set_noise_seed(21);
    CHECK(read_dn(camera, frame_buffer) == first);
    camera.sensor_->set_noise_seed(22);
    CHECK(read_dn(camera, frame_buffer) != first);

    // With noise, the gain sets the mean the shot noise is drawn around, and the noise itself
    // is as without PRNU: its random numbers are their own.
    camera.sensor_->set_noise_seed(23);
    camera.sensor_->enable_noise(true);
    camera.sensor_->set_prnu(0.f);
    const std::vector<double> plain = read_dn(camera, frame_buffer);
    camera.sensor_->set_noise_seed(23);
    camera.sensor_->set_prnu(0.02f);
    const Image<float> seeded = camera.sensor_->prnu_map();
    const std::vector<double> with_prnu = read_dn(camera, frame_buffer);
    double sum = 0.0;
    for (std::size_t i = 0; i < plain.size(); i += 3) {
        const auto pixel = static_cast<int>(i / 3);
        sum += with_prnu[i] - plain[i] -
               10000.0 * (static_cast<double>(seeded(pixel % 64, pixel / 64)) - 1.0);
    }
    CHECK(std::abs(sum / 4096.0) < 3.0); // the differences are the gains', on average

    // It applies to the light, not the dark current:
    camera.sensor_->enable_noise(false);
    camera.sensor_->set_dark_current(500.f);
    frame_buffer.received_power().fill(RGB{0.f});
    for (double v : read_dn(camera, frame_buffer)) {
        REQUIRE(std::abs(v - 500.0) < 1e-3);
    }

    // Made again for a new resolution:
    camera.configure_sensor_from_pitch(Resolution{16, 8}, units::Micrometer(5.0));
    CHECK(camera.sensor_->prnu_map().width() == 16);

    CHECK_THROWS(camera.sensor_->set_prnu(-0.01f));
    CHECK_THROWS(camera.sensor_->set_prnu(std::numeric_limits<float>::infinity()));
    CHECK(camera.sensor_->prnu() == 0.02f);
}

TEST_CASE("Hot pixels gather a dark current of their own, the same pixels in every frame",
          "[cameras][sensor][fixed]")
{
    SensorProbe camera;
    camera.configure_sensor_from_pitch(Resolution{256, 256}, units::Micrometer(5.0));
    camera.sensor_->set_conversion_gain(1.f);
    camera.sensor_->set_bias_level_dn(0.f);
    camera.sensor_->set_dark_current(0.f);
    camera.sensor_->set_read_noise(0.f);
    camera.sensor_->set_full_well_capacity(1e9f);
    camera.sensor_->set_bit_depth(20);
    camera.sensor_->set_noise_seed(5);
    camera.sensor_->enable_noise(false);
    auto frame_buffer = camera.make_frame_buffer();
    frame_buffer.enable_sensor_response();
    frame_buffer.received_power().fill(RGB{0.f});

    // 1% of the pixels, each at 100 e-/s, over 2 s:
    camera.sensor_->set_hot_pixels(0.01f, 100.f);
    const Image<float> map = camera.sensor_->hot_pixel_map();
    std::size_t hot = 0;
    for (std::size_t i = 0; i < map.size(); ++i) {
        REQUIRE((map[i] == 0.f || map[i] == 100.f));
        hot += map[i] > 0.f ? std::size_t{1} : std::size_t{0};
    }
    CHECK(std::abs(static_cast<double>(hot) - 655.36) < 4.0 * std::sqrt(655.36)); // binomial
    const std::vector<double> dn = read_dn(camera, frame_buffer, 2.0);
    for (std::size_t i = 0; i < dn.size(); ++i) {
        const auto pixel = static_cast<int>(i / 3);
        REQUIRE(std::abs(dn[i] - 2.0 * static_cast<double>(map(pixel % 256, pixel / 256))) < 1e-3);
    }
    CHECK(read_dn(camera, frame_buffer, 2.0) == dn); // the same pixels in the next frame

    // A larger fraction keeps them, with the same dark currents, and adds more:
    camera.sensor_->set_hot_pixels(0.05f, 100.f);
    const Image<float> more = camera.sensor_->hot_pixel_map();
    std::size_t more_hot = 0;
    for (std::size_t i = 0; i < map.size(); ++i) {
        if (map[i] > 0.f) {
            REQUIRE(more[i] == map[i]);
        }
        more_hot += more[i] > 0.f ? std::size_t{1} : std::size_t{0};
    }
    CHECK(more_hot > 4 * hot);

    // A spread: log-normal about the median.
    camera.sensor_->set_hot_pixels(0.05f, 100.f, 0.5f);
    const Image<float> spread = camera.sensor_->hot_pixel_map();
    std::vector<double> logs;
    for (std::size_t i = 0; i < spread.size(); ++i) {
        if (spread[i] > 0.f) {
            REQUIRE(more[i] > 0.f); // the same pixels
            logs.push_back(std::log(static_cast<double>(spread[i]) / 100.0));
        }
    }
    CHECK(std::abs(mean_of(logs)) < 0.05);
    CHECK(std::abs(std::sqrt(variance_of(logs)) / 0.5 - 1.0) < 0.08);
    CHECK(camera.sensor_->hot_pixel_spread() == 0.5f);

    // They add to the dark current in effect, a map's too:
    camera.sensor_->set_hot_pixels(0.01f, 100.f);
    camera.sensor_->set_dark_current_map(Image<float>(256, 256, 7.f));
    for (double v : read_dn(camera, frame_buffer)) {
        REQUIRE((std::abs(v - 7.0) < 1e-3 || std::abs(v - 107.0) < 1e-3));
    }

    // Another seed, other pixels:
    camera.sensor_->set_noise_seed(6);
    const Image<float> other = camera.sensor_->hot_pixel_map();
    bool differ = false;
    for (std::size_t i = 0; i < map.size(); ++i) {
        differ = differ || (other[i] > 0.f) != (map[i] > 0.f);
    }
    CHECK(differ);

    // Off with a fraction or dark current of 0:
    camera.sensor_->set_hot_pixels(0.f, 100.f);
    CHECK(camera.sensor_->hot_pixel_map()(3, 3) == 0.f);

    CHECK_THROWS(camera.sensor_->set_hot_pixels(1.5f, 100.f));
    CHECK_THROWS(camera.sensor_->set_hot_pixels(-0.1f, 100.f));
    CHECK_THROWS(camera.sensor_->set_hot_pixels(0.01f, -1.f));
    CHECK_THROWS(camera.sensor_->set_hot_pixels(0.01f, 100.f, -0.5f));
    CHECK_THROWS(camera.sensor_->set_hot_pixels(0.01f, std::numeric_limits<float>::quiet_NaN()));
}

TEST_CASE("A copied sensor has the same fixed pattern settings, and patterns of its own",
          "[cameras][sensor][fixed]")
{
    SensorProbe camera;
    electron_counter(camera);
    camera.sensor_->set_prnu(0.03f);
    camera.sensor_->set_hot_pixels(0.02f, 50.f, 0.3f);

    SensorProbe other;
    other.set_sensor<SimpleSensor<RGB>>(
        *dynamic_cast<const SimpleSensor<RGB>*>(camera.sensor_.get()));
    CHECK(other.sensor_->prnu() == 0.03f);
    CHECK(other.sensor_->hot_pixel_fraction() == 0.02f);
    CHECK(other.sensor_->hot_pixel_dark_current() == 50.f);
    CHECK(other.sensor_->hot_pixel_spread() == 0.3f);

    // A new seed is a new detector; the same seed, the same one:
    CHECK(other.sensor_->prnu_map()(10, 10) != camera.sensor_->prnu_map()(10, 10));
    other.sensor_->set_noise_seed(camera.sensor_->noise_seed());
    CHECK(other.sensor_->prnu_map()(10, 10) == camera.sensor_->prnu_map()(10, 10));
    const Image<float> a = camera.sensor_->hot_pixel_map();
    const Image<float> b = other.sensor_->hot_pixel_map();
    for (std::size_t i = 0; i < a.size(); ++i) {
        REQUIRE(a[i] == b[i]);
    }

    const std::string text = camera.describe();
    INFO(text);
    CHECK(text.find("Fixed:       PRNU 3%, hot pixels 2% at 50 e-/s (spread 0.3), from the seed") !=
          std::string::npos);
    SensorProbe plain;
    plain.configure_sensor_from_pitch(Resolution{4, 4}, units::Micrometer(5.0));
    CHECK(plain.describe().find("Fixed:       PRNU 0%, hot pixels none") != std::string::npos);
}
