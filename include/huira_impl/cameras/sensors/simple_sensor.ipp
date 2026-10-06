#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <type_traits>

#include "huira/concepts/spectral_concepts.hpp"
#include "huira/images/image.hpp"
#include "huira/render/frame_buffer.hpp"
#include "huira/units/units.hpp"
#include "tbb/blocked_range.h"
#include "tbb/parallel_for.h"

namespace huira {

/**
 * @brief Simulates the sensor readout, including noise and the ADC.
 *
 * For each pixel (each channel, for RGB): the received energy becomes photons and, through the
 * quantum efficiency, signal electrons, to which the dark current's electrons are added. With
 * noise simulated, the electrons collected are drawn from a Poisson distribution of that mean
 * (shot noise), clamped to the full well, and then the read noise is added; the bias is added
 * after the gain. The result is clamped to the ADC's range and normalized to [0, 1]. See
 * docs/design_overview/sensor_modeling.rst.
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
    const double dark_e =
        noise ? static_cast<double>(config.dark_current) * static_cast<double>(dt) : 0.0;
    const double bias = noise ? static_cast<double>(config.bias_level_dn) : 0.0;
    const double read_noise = noise ? static_cast<double>(config.read_noise) : 0.0;
    const double full_well = static_cast<double>(config.full_well_capacity);
    const double gain = static_cast<double>(config.gain);

    // Expected electrons to the normalized response:
    auto respond = [&](double expected_e, detail::SensorRandom& random) {
        double electrons = noise ? random.poisson(expected_e) : expected_e;
        electrons = std::min(electrons, full_well);
        if (read_noise > 0.0) {
            electrons += read_noise * random.normal();
        }
        const double dn = electrons / gain + bias;
        return static_cast<float>(std::clamp(dn, 0.0, max_dn) / max_dn);
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
}
} // namespace huira
