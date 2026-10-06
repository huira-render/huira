#include <cmath>
#include <cstddef>
#include <limits>
#include <thread>
#include <vector>

#include "catch2/catch_test_macros.hpp"
#include "huira/cameras/camera_model.hpp"
#include "huira/core/spectral_bins.hpp"
#include "huira/units/units.hpp"

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

/// The response, in DN.
std::vector<double> read_dn(SensorProbe& camera, FrameBuffer<RGB>& frame_buffer)
{
    camera.readout(frame_buffer, units::Second(1.0));
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
        camera.sensor_->set_simulate_noise(noise);
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
    camera.sensor_->set_simulate_noise(false);
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
    camera.sensor_->set_simulate_noise(false);
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
        camera.sensor_->set_simulate_noise(noise);
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
