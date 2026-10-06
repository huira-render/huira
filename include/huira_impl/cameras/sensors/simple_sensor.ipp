#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <string>
#include <type_traits>

#include "huira/concepts/spectral_concepts.hpp"
#include "huira/images/image.hpp"
#include "huira/render/frame_buffer.hpp"
#include "huira/units/units.hpp"
#include "huira/util/logger.hpp"
#include "tbb/blocked_range.h"
#include "tbb/parallel_for.h"

namespace huira {

/**
 * @brief Simulates the sensor readout, from the received power to digital numbers (DN).
 *
 * For each pixel (each channel, for RGB): the received energy becomes photons and, through the
 * quantum efficiency, signal electrons, to which the dark current's electrons are added. With
 * noise simulated, the electrons collected are drawn from a Poisson distribution of that mean
 * (shot noise); without, they are the mean. They are clamped to the full well, and the read
 * noise is added (with noise simulated). The ADC divides by the gain, adds the bias, rounds to a
 * whole number of DN and clamps to its range, [0, 2^bit_depth - 1]. The response holds
 * DN / (2^bit_depth - 1), so its values are exact fractions of whole DN, and the image records
 * the bit depth (Image::sensor_bit_depth()) for the image writers. See
 * docs/design_overview/sensor_modeling.rst.
 *
 * Turning noise off removes only what is random: shot noise and read noise. The dark current's
 * electrons and the bias are not noise, and are applied either way.
 *
 * A pixel whose received power is not finite (NaN or infinite) has no meaningful response: it
 * reads out as NaN, which the image writers record as missing data, and a warning gives the
 * number of such pixels.
 *
 * Rows are read out in parallel. The noise is reproducible: see set_noise_seed().
 *
 * @param fb The frame buffer containing received power and where the sensor response will be
 * written.
 * @param exposure_time The exposure time in seconds.
 */
template <IsSpectral TSpectral>
void SimpleSensor<TSpectral>::readout(FrameBuffer<TSpectral>& fb, units::Second exposure_time) const
{
    const Image<TSpectral>& received_power = fb.received_power();
    auto& output = fb.sensor_response();
    output.set_sensor_bit_depth(this->config_.bit_depth);

    const SensorConfig<TSpectral>& config = this->config_;
    const float dt = static_cast<float>(exposure_time.to_si());
    const TSpectral photon_energy = TSpectral::photon_energies();
    const double max_dn = std::pow(2.0, static_cast<double>(config.bit_depth)) - 1.0;

    const bool noise = config.simulate_noise;
    const double dark_e = static_cast<double>(config.dark_current) * static_cast<double>(dt);
    const double bias = static_cast<double>(config.bias_level_dn);
    const double read_noise = noise ? static_cast<double>(config.read_noise) : 0.0;
    const double full_well = static_cast<double>(config.full_well_capacity);
    const double gain = static_cast<double>(config.gain);

    std::atomic<std::size_t> non_finite{0};

    // Expected electrons to the normalized response:
    auto respond = [&](double expected_e, detail::SensorRandom& random) {
        if (!std::isfinite(expected_e)) {
            non_finite.fetch_add(1, std::memory_order_relaxed);
            return std::numeric_limits<float>::quiet_NaN();
        }
        double electrons = noise ? random.poisson(expected_e) : expected_e;
        electrons = std::min(electrons, full_well);
        if (read_noise > 0.0) {
            electrons += read_noise * random.normal();
        }
        // The ADC: to whole DN, within its range.
        const double dn = std::clamp(std::round(electrons / gain + bias), 0.0, max_dn);
        return static_cast<float>(dn / max_dn);
    };

    const std::uint64_t readout_number = this->next_readout_number_();
    const int width = received_power.width();
    tbb::parallel_for(
        tbb::blocked_range<int>(0, received_power.height()),
        [&](const tbb::blocked_range<int>& rows) {
            for (int y = rows.begin(); y < rows.end(); ++y) {
                detail::SensorRandom random(
                    this->noise_seed_, readout_number, static_cast<std::uint64_t>(y));
                for (int x = 0; x < width; ++x) {
                    // Power to energy, to photons, to signal electrons:
                    const TSpectral photons = received_power(x, y) * dt / photon_energy;
                    const TSpectral electrons = photons * config.quantum_efficiency;

                    if constexpr (std::is_same_v<TSpectral, RGB>) {
                        RGB value;
                        for (std::size_t c = 0; c < 3; ++c) {
                            value[c] = respond(static_cast<double>(electrons[c]) + dark_e, random);
                        }
                        output(x, y) = value;
                    } else {
                        output(x, y) =
                            respond(static_cast<double>(electrons.total()) + dark_e, random);
                    }
                }
            }
        });

    const std::size_t missing = non_finite.load(std::memory_order_relaxed);
    if (missing > 0) {
        HUIRA_LOG_WARNING("SimpleSensor::readout - " + std::to_string(missing) +
                          " pixel value(s) received a power that is not finite (NaN or "
                          "infinite), and read out as NaN. This points to a problem upstream, "
                          "such as a material or light returning a non-finite radiance.");
    }
}
} // namespace huira
