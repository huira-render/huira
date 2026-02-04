#include "huira/core/concepts/spectral_concepts.hpp"
#include "huira/render/frame_buffer.hpp"
#include "huira/images/image.hpp"

namespace huira {
    template <IsSpectral TSpectral>
    void SimpleSensor<TSpectral>::readout(FrameBuffer<TSpectral>& fb, float exposure_time) const {

        const TSpectral photon_energy = TSpectral::photon_energies();

        Image<TSpectral>& received_power = fb.received_power();
        Image<float>& output = fb.sensor_response();

        // 1. Prepare Random Number Generators (Poisson & Normal)
        // (Implementation details below...)

        // 2. Iterate Pixels (Parallelize this!)
        for (int y = 0; y < received_power.height(); ++y) {
            for (int x = 0; x < received_power.width(); ++x) {

                // Power to energy
                TSpectral received_energy = received_power(x, y) * exposure_time;

                // Photon Conversion
                TSpectral photons = received_energy / photon_energy;

                // Quantum Efficiency
                TSpectral electrons = photons * quantum_efficiency;
                float signal_e = electrons.total();

                // Dark Current
                float dark_e = dark_current * exposure_time;

                // Shot Noise (Poisson)
                //float noisy_e = sample_poisson(signal_e + dark_e);
                float noisy_e = signal_e + dark_e; // Placeholder for Poisson sampling

                // Read Noise (Gaussian)
                //noisy_e += sample_normal(0.0, read_noise);

                // Well Saturation (basic clamping)
                if (noisy_e > full_well_capacity) {
                    noisy_e = full_well_capacity;
                }

                // Quantization (ADC)
                float max_dn = std::pow(2.f, static_cast<float>(bit_depth)) - 1.f;
                float dn = std::floor(std::min(noisy_e / gain, max_dn));

                // Map back to floating point [0,1] for output image:
                output(x, y) = dn / max_dn;
            }
        }
    }
}
