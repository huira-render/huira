#include <cstddef>
#include <random>

#include "huira/concepts/spectral_concepts.hpp"
#include "huira/images/image.hpp"
#include "huira/render/frame_buffer.hpp"
#include "huira/units/units.hpp"

namespace huira {

/**
 * @brief Simulates noise and ADC quantization for a sensor pixel.
 *
 * Adds shot noise, clamps to full well, applies read noise, and quantizes to digital number (DN).
 *
 * @param signal_e Signal electrons.
 * @param dark_e Dark current electrons.
 * @param config Sensor configuration.
 * @param max_dn Maximum digital number (saturation level).
 * @param rng Random number generator.
 * @param read_noise_dist Normal distribution for read noise.
 * @return Normalized intensity in [0, 1].
 */
template <IsSpectral TSpectral>
inline float noise_and_adc(float signal_e,
                           float dark_e,
                           const SensorConfig<TSpectral>& config,
                           float max_dn,
                           std::mt19937& rng,
                           std::normal_distribution<float>& read_noise_dist)
{

    // Shot Noise (Approximation of Poisson):
    float accumulated_e = signal_e + dark_e;

    float bias = 0.f;
    if (config.simulate_noise) {
        std::normal_distribution<float> shot_dist(0.0f, std::sqrt(accumulated_e));
        accumulated_e += shot_dist(rng);
        accumulated_e += read_noise_dist(rng);
        bias = config.bias_level_dn;
    }

    // Clamp to Full Well Capacity:
    accumulated_e = std::min(accumulated_e, config.full_well_capacity);

    // System Gain & Quantization (ADC)
    float dn_value = (accumulated_e / config.gain) + bias;
    // float intensity = std::max(0.f, std::floor(std::min(dn_value, max_dn))) / max_dn;
    float intensity = std::clamp(dn_value, 0.f, max_dn) / max_dn;

    return intensity;
}

/**
 * @brief Simulates the sensor readout process, including noise and quantization.
 *
 * Converts received power to electrons, applies quantum efficiency, adds noise, and quantizes the
 * result.
 *
 * @param fb The frame buffer containing received power and where the sensor response will be
 * written.
 * @param exposure_time The exposure time in seconds.
 */
template <IsSpectral TSpectral>
void SimpleSensor<TSpectral>::readout(FrameBuffer<TSpectral>& fb, units::Second exposure_time) const
{
    Image<TSpectral>& received_power = fb.received_power();
    auto& output = fb.sensor_response();
    output.set_sensor_bit_depth(this->config_.bit_depth);

    float dt = static_cast<float>(exposure_time.to_si());

    // TODO Move this somewhere else?
    static std::mt19937 rng(1);
    std::normal_distribution<float> read_noise_dist(0.0f, this->config_.read_noise);

    const TSpectral photon_energy = TSpectral::photon_energies();
    float max_dn = std::pow(2.f, static_cast<float>(this->config_.bit_depth)) - 1.f;

    float dark_e = 0.f;
    if (this->config_.simulate_noise) {
        dark_e = this->config_.dark_current * dt;
    }

    for (int y = 0; y < received_power.height(); ++y) {
        for (int x = 0; x < received_power.width(); ++x) {

            // Power to energy
            TSpectral received_energy = received_power(x, y) * dt;

            // Photon Conversion
            TSpectral photons = received_energy / photon_energy;

            // Quantum Efficiency
            TSpectral electrons = photons * this->config_.quantum_efficiency;

            // Compute Noise and ADC:
            if constexpr (std::is_same_v<TSpectral, RGB>) {
                RGB signal_e{electrons[0], electrons[1], electrons[2]};

                RGB pixel_value;
                for (std::size_t i = 0; i < 3; ++i) {
                    pixel_value[i] = noise_and_adc(
                        signal_e[i], dark_e, this->config_, max_dn, rng, read_noise_dist);
                }
                output(x, y) = pixel_value;
            } else {
                float signal_e = electrons.total();
                output(x, y) =
                    noise_and_adc(signal_e, dark_e, this->config_, max_dn, rng, read_noise_dist);
            }
        }
    }
}
} // namespace huira
