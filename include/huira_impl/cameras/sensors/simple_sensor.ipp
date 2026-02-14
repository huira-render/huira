#include <random>
#include <cstddef>

#include "huira/core/concepts/spectral_concepts.hpp"
#include "huira/render/frame_buffer.hpp"
#include "huira/images/image.hpp"

namespace huira {
    template <IsSpectral TSpectral>
    inline float noise_and_adc(float signal_e, float dark_e,
        const SensorConfig<TSpectral>& config,
        float max_dn, std::mt19937& rng,
        std::normal_distribution<float>& read_noise_dist) {

        // Shot Noise (Approximation of Poisson):
        float accumulated_e = signal_e + dark_e;
        std::normal_distribution<float> shot_dist(0.0f, std::sqrt(accumulated_e));
        float noisy_e = accumulated_e + shot_dist(rng);

        // Clamp to Full Well Capacity:
        noisy_e = std::min(noisy_e, config.full_well_capacity);
        noisy_e += read_noise_dist(rng);

        // System Gain & Quantization (ADC)
        float dn_value = (noisy_e / config.gain) + config.bias_level_dn;
        float intensity = std::max(0.f, std::floor(std::min(dn_value, max_dn))) / max_dn;

        return intensity;
    }

    template <IsSpectral TSpectral>
    void SimpleSensor<TSpectral>::readout(FrameBuffer<TSpectral>& fb, float exposure_time) const
    {
        Image<TSpectral>& received_power = fb.received_power();
        auto& output = fb.sensor_response();
        output.set_sensor_bit_depth(this->config_.bit_depth);

        // TODO Move this somewhere else?
        static std::mt19937 rng(1);
        std::normal_distribution<float> read_noise_dist(0.0f, this->config_.read_noise);

        const TSpectral photon_energy = TSpectral::photon_energies();
        float max_dn = std::pow(2.f, static_cast<float>(this->config_.bit_depth)) - 1.f;

        for (int y = 0; y < received_power.height(); ++y) {
            for (int x = 0; x < received_power.width(); ++x) {

                // Power to energy
                TSpectral received_energy = received_power(x, y) * exposure_time;

                // Photon Conversion
                TSpectral photons = received_energy / photon_energy;

                // Quantum Efficiency
                TSpectral electrons = photons * this->config_.quantum_efficiency;

                // Compute Noise and ADC:
                if constexpr (std::is_same_v<TSpectral, RGB>) {
                    Vec3<float> signal_e{ electrons[0], electrons[1], electrons[2] };
                    float dark_e = this->config_.dark_current * exposure_time;

                    Vec3<float> pixel_value;
                    for (int i = 0; i < 3; ++i) {
                        pixel_value[i] = noise_and_adc(signal_e[i], dark_e, this->config_, max_dn, rng, read_noise_dist);
                    }

                    output(x, y) = pixel_value;
                }
                else {
                    float signal_e = electrons.total();
                    float dark_e = this->config_.dark_current * exposure_time;
                    output(x, y) = noise_and_adc(signal_e, dark_e, this->config_, max_dn, rng, read_noise_dist);
                }
            }
        }
    }
}
