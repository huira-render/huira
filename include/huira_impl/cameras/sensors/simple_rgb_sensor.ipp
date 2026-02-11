#include "huira/core/concepts/spectral_concepts.hpp"
#include "huira/util/logger.hpp"
#include "huira/render/frame_buffer.hpp"
#include "huira/images/image.hpp"

namespace huira {
    void SimpleRGBSensor::readout(FrameBuffer<RGB>& fb, float exposure_time) const {
        Image<RGB>& received_power = fb.received_power();
        Image<Vec3<float>>& output = fb.sensor_response_rgb();
        output.set_sensor_bit_depth(this->config_.bit_depth);

        // TODO Move this somewhere else?
        static std::mt19937 rng(1);
        std::normal_distribution<float> read_noise_dist(0.0f, this->config_.read_noise);

        const RGB photon_energy = RGB::photon_energies();
        float max_dn = std::pow(2.f, static_cast<float>(this->config_.bit_depth)) - 1.f;

        for (int y = 0; y < received_power.height(); ++y) {
            for (int x = 0; x < received_power.width(); ++x) {

                // Power to energy
                RGB received_energy = received_power(x, y) * exposure_time;

                // Photon Conversion
                RGB photons = received_energy / photon_energy;

                // Quantum Efficiency
                RGB electrons = photons * this->config_.quantum_efficiency;
                Vec3<float> signal_e{ electrons[0], electrons[1], electrons[2] };

                // Dark Current
                float dark_e = this->config_.dark_current * exposure_time;

                // Shot Noise (Poisson)  TODO THIS IS JUST A GAUSSIAN APPROXIMATION
                Vec3<float> pixel_value;
                for (int i = 0; i < 3; ++i) {
                    float accumulated_e = signal_e[i] + dark_e;
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

                    pixel_value[i] = std::max(0.f, dn) / max_dn;
                }

                output(x, y) = pixel_value;
            }
        }
    }
}
