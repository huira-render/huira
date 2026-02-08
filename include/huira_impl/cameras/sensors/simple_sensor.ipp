#include <random>

#include "huira/core/concepts/spectral_concepts.hpp"
#include "huira/render/frame_buffer.hpp"
#include "huira/images/image.hpp"

namespace huira {
    template <IsSpectral TSpectral>
    void SimpleSensor<TSpectral>::readout(FrameBuffer<TSpectral>& fb, float exposure_time) const
    {
        Image<TSpectral>& received_power = fb.received_power();
        Image<float>& output = fb.sensor_response();
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
                float signal_e = electrons.total();

                // Dark Current
                float dark_e = this->config_.dark_current * exposure_time;

                // Shot Noise (Poisson)  TODO THIS IS JUST A GAUSSIAN APPROXIMATION
                float accumulated_e = signal_e + dark_e;
                float shot_noise_std = std::sqrt(accumulated_e);
                std::normal_distribution<float> shot_dist(0.0f, shot_noise_std);
                float noisy_e = accumulated_e + shot_dist(rng);

                // Well Saturation (basic clamping)
                if (noisy_e > this->config_.full_well_capacity) {
                    noisy_e = this->config_.full_well_capacity;
                }

                // Read Noise (Gaussian)
                noisy_e += read_noise_dist(rng);

                // System Gain & Quantization (ADC)
                float dn_value = (noisy_e / this->config_.gain) + this->config_.bias_level_dn;
                float dn = std::floor(std::min(dn_value, max_dn));

                output(x, y) = std::max(0.f, dn) / max_dn;
            }
        }
    }
}
