#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <string>
#include <type_traits>
#include <vector>

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
 * quantum efficiency, the selected filter's transmission and, if it has one, its flat (the
 * pixel's response relative to the principal point's), and the pixel's own gain (PRNU, if set),
 * signal electrons, to which the dark current's electrons are added (the pixel's own, from the
 * dark current map, if one is set, and its own as a hot pixel, if it is one). With
 * noise simulated, the electrons collected are drawn from a Poisson distribution of that mean (shot
 * noise); without, they are the mean. They are clamped to the full well, and the read noise is
 * added, with the row's and the column's noise (with noise simulated; see set_row_noise() and
 * set_column_noise()). The ADC divides by the gain, adds the bias (the pixel's own, from the bias
 * map, if one is set), rounds to a whole number of DN and clamps to its range, [0, 2^bit_depth -
 * 1]. The response holds DN / (2^bit_depth - 1), so its values are exact fractions of whole DN, and
 * the image records the bit depth (Image::sensor_bit_depth()) for the image writers. See
 * docs/design_overview/sensor_modeling.rst.
 *
 * Two switches leave parts out. Turning noise off (enable_noise()) removes what is random:
 * shot noise, read noise, and row and column noise. Turning imperfections off
 * (enable_imperfections()) removes what is the same in every frame: the dark current (the single
 * value, the map and hot pixels), PRNU, the selected filter's flat, and the bias map, leaving
 * the bias level. Each is applied whatever the other is.
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
 * @throws std::runtime_error if the frame buffer is not at the sensor's resolution, or a dark
 *         current or bias map, or the filter's flat, is set at another resolution and
 *         imperfections are on.
 */
template <IsSpectral TSpectral>
void SimpleSensor<TSpectral>::readout(FrameBuffer<TSpectral>& fb, units::Second exposure_time) const
{
    const Image<TSpectral>& received_power = fb.received_power();
    if (!(received_power.resolution() == this->config_.resolution)) {
        HUIRA_THROW_ERROR("SimpleSensor::readout - The frame buffer is " +
                          detail::resolution_text(received_power.resolution()) +
                          ", and the sensor " + detail::resolution_text(this->config_.resolution) +
                          ": a frame buffer is read out by the sensor it was made for.");
    }
    // The imperfections: the dark current, the maps, the flat and the fixed patterns. Without
    // them, none is used, so none need fit.
    const bool imperfections = this->config_.simulate_imperfections;
    if (imperfections) {
        this->check_map_resolutions_(received_power.resolution(), "SimpleSensor::readout");
    }
    auto& output = fb.sensor_response();
    output.set_sensor_bit_depth(this->config_.bit_depth);

    const SensorConfig<TSpectral>& config = this->config_;
    const float dt = static_cast<float>(exposure_time.to_si());
    const TSpectral photon_energy = TSpectral::photon_energies();
    // Photons to signal electrons, per bin: the quantum efficiency through the filter.
    const TSpectral response = config.quantum_efficiency * this->filter_transmission_;
    const std::shared_ptr<const Image<float>> flat = imperfections ? this->flat_ : nullptr;
    // The sensor's fixed patterns, made from its noise seed (the same in every readout):
    const std::shared_ptr<const Image<float>> prnu_gains =
        imperfections ? this->prnu_gains_() : nullptr;
    const std::shared_ptr<const Image<float>> hot_pixels =
        imperfections ? this->hot_pixel_dark_currents_() : nullptr;
    const double max_dn = std::pow(2.0, static_cast<double>(config.bit_depth)) - 1.0;

    const bool noise = config.simulate_noise;
    const double dark_e =
        imperfections ? static_cast<double>(config.dark_current) * static_cast<double>(dt) : 0.0;
    const double bias = static_cast<double>(config.bias_level_dn);
    const double read_noise = noise ? static_cast<double>(config.read_noise) : 0.0;
    const double row_noise = noise ? static_cast<double>(config.row_noise) : 0.0;
    const double column_noise = noise ? static_cast<double>(config.column_noise) : 0.0;
    const double full_well = static_cast<double>(config.full_well_capacity);
    const double gain = static_cast<double>(config.gain);

    // Each pixel's dark current and bias, where maps give them:
    const std::shared_ptr<const Image<float>> dark_map =
        imperfections ? this->dark_current_map_ : nullptr;
    const std::shared_ptr<const Image<float>> bias_map = imperfections ? this->bias_map_ : nullptr;
    const double bias_map_base = this->bias_map_kind_ == BiasMapKind::Relative ? bias : 0.0;

    std::atomic<std::size_t> non_finite{0};

    // Expected electrons to the normalized response, with the pixel's bias, and the noise its row
    // and column share:
    auto respond = [&](double expected_e,
                       double pixel_bias,
                       double shared_noise_e,
                       detail::SensorRandom& random) {
        if (!std::isfinite(expected_e)) {
            non_finite.fetch_add(1, std::memory_order_relaxed);
            return std::numeric_limits<float>::quiet_NaN();
        }
        double electrons = noise ? random.poisson(expected_e) : expected_e;
        electrons = std::min(electrons, full_well);
        if (read_noise > 0.0) {
            electrons += read_noise * random.normal();
        }
        if (shared_noise_e != 0.0) {
            electrons += shared_noise_e;
        }
        // The ADC: to whole DN, within its range.
        const double dn = std::clamp(std::round(electrons / gain + pixel_bias), 0.0, max_dn);
        return static_cast<float>(dn / max_dn);
    };

    const std::uint64_t readout_number = this->next_readout_number_();
    const int width = received_power.width();

    // Each column's noise for this readout, from the columns' own random numbers:
    std::vector<double> column_noise_e(static_cast<std::size_t>(width), 0.0);
    if (column_noise > 0.0) {
        detail::SensorRandom columns(
            detail::SensorRandom::Stream::Columns, this->noise_seed_, readout_number, 0);
        for (double& column_e : column_noise_e) {
            column_e = column_noise * columns.normal();
        }
    }

    tbb::parallel_for(
        tbb::blocked_range<int>(0, received_power.height()),
        [&](const tbb::blocked_range<int>& rows) {
            for (int y = rows.begin(); y < rows.end(); ++y) {
                detail::SensorRandom random(
                    this->noise_seed_, readout_number, static_cast<std::uint64_t>(y));

                // The row's noise for this readout, from the rows' own random numbers:
                double row_noise_e = 0.0;
                if (row_noise > 0.0) {
                    detail::SensorRandom row_random(detail::SensorRandom::Stream::Rows,
                                                    this->noise_seed_,
                                                    readout_number,
                                                    static_cast<std::uint64_t>(y));
                    row_noise_e = row_noise * row_random.normal();
                }

                for (int x = 0; x < width; ++x) {
                    // Power to energy, to photons, to signal electrons:
                    const TSpectral photons = received_power(x, y) * dt / photon_energy;
                    TSpectral electrons = photons * response;
                    if (flat != nullptr) {
                        electrons = electrons * (*flat)(x, y);
                    }
                    if (prnu_gains != nullptr) {
                        electrons = electrons * (*prnu_gains)(x, y);
                    }

                    double pixel_dark_e =
                        dark_map != nullptr
                            ? static_cast<double>((*dark_map)(x, y)) * static_cast<double>(dt)
                            : dark_e;
                    if (hot_pixels != nullptr) {
                        pixel_dark_e +=
                            static_cast<double>((*hot_pixels)(x, y)) * static_cast<double>(dt);
                    }
                    const double pixel_bias =
                        bias_map != nullptr ? bias_map_base + static_cast<double>((*bias_map)(x, y))
                                            : bias;

                    const double shared_noise_e =
                        row_noise_e + column_noise_e[static_cast<std::size_t>(x)];

                    if constexpr (std::is_same_v<TSpectral, RGB>) {
                        RGB value;
                        for (std::size_t c = 0; c < 3; ++c) {
                            value[c] = respond(static_cast<double>(electrons[c]) + pixel_dark_e,
                                               pixel_bias,
                                               shared_noise_e,
                                               random);
                        }
                        output(x, y) = value;
                    } else {
                        output(x, y) =
                            respond(static_cast<double>(electrons.total()) + pixel_dark_e,
                                    pixel_bias,
                                    shared_noise_e,
                                    random);
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
