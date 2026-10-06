
#pragma once

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <numbers>
#include <string>

#include "huira/concepts/spectral_concepts.hpp"
#include "huira/render/frame_buffer.hpp"
#include "huira/units/units.hpp"
#include "huira/util/macros.hpp"

namespace huira {
// Forward Declare
template <IsSpectral TSpectral>
class CameraModel;

namespace detail {
/// A distinct default noise seed for each sensor, in the order they are made. See
/// SensorModel::set_noise_seed().
inline std::uint64_t next_default_noise_seed()
{
    HUIRA_PER_MODULE_STATE_BEGIN
    static std::atomic<std::uint64_t> next{1};
    HUIRA_PER_MODULE_STATE_END
    return next.fetch_add(1, std::memory_order_relaxed);
}

/**
 * @brief The random numbers for one row of one sensor readout.
 *
 * A SplitMix64 generator, started from a key made of the sensor's noise seed, the readout's
 * number and the row. A frame's noise therefore depends only on those: not on other sensors,
 * on threads, or on the order rows are read out in. It is self-contained, so it gives the same
 * numbers with every standard library, which std::normal_distribution and the like do not.
 */
class SensorRandom {
  public:
    SensorRandom(std::uint64_t seed, std::uint64_t readout, std::uint64_t row)
        : state_(mix_(seed ^ mix_(readout ^ mix_(row))))
    {
    }

    /// Uniform in [0, 1).
    double uniform() { return static_cast<double>(next_() >> 11) * 0x1.0p-53; }

    /// Standard normal, by the Box-Muller transform.
    double normal()
    {
        if (has_spare_) {
            has_spare_ = false;
            return spare_;
        }
        const double radius = std::sqrt(-2.0 * std::log(1.0 - uniform())); // log of (0, 1]
        const double angle = 2.0 * std::numbers::pi * uniform();
        spare_ = radius * std::sin(angle);
        has_spare_ = true;
        return radius * std::cos(angle);
    }

    /**
     * @brief A Poisson-distributed count with the given mean.
     *
     * Exact below a mean of POISSON_EXACT_BELOW, by searching the cumulative distribution (about
     * mean + 1 steps). Above it, a normal distribution of the same mean and variance, rounded to
     * a whole count, whose skew differs from the Poisson's by under 1 / sqrt(mean).
     */
    double poisson(double mean)
    {
        if (!(mean > 0.0)) {
            return 0.0;
        }
        if (mean >= POISSON_EXACT_BELOW) {
            return std::max(0.0, std::round(mean + std::sqrt(mean) * normal()));
        }
        const double u = uniform();
        const double limit = mean + 20.0 * std::sqrt(mean) + 30.0; // against rounding in the sum
        double probability = std::exp(-mean);
        double cumulative = probability;
        double count = 0.0;
        while (u > cumulative && count < limit) {
            count += 1.0;
            probability *= mean / count;
            cumulative += probability;
        }
        return count;
    }

    static constexpr double POISSON_EXACT_BELOW = 20.0;

  private:
    static constexpr std::uint64_t GAMMA_ = 0x9E3779B97F4A7C15ULL;

    static std::uint64_t finalize_(std::uint64_t z)
    {
        z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
        z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
        return z ^ (z >> 31);
    }

    static std::uint64_t mix_(std::uint64_t z) { return finalize_(z + GAMMA_); }

    std::uint64_t next_()
    {
        state_ += GAMMA_;
        return finalize_(state_);
    }

    std::uint64_t state_;
    double spare_ = 0.0;
    bool has_spare_ = false;
};
} // namespace detail

/**
 * @brief Configuration parameters for a sensor model.
 *
 * Holds all physical and electronic parameters needed to describe a sensor, including resolution,
 * pixel pitch, quantum efficiency, noise, gain, and rotation. Used to initialize and configure
 * SensorModel instances.
 *
 * @tparam TSpectral The spectral type (e.g., @ref RGB, @ref Visible8)
 */
template <IsSpectral TSpectral>
struct SensorConfig {
    Resolution resolution{1024, 1024};
    units::Micrometer pitch_x{8.5};
    units::Micrometer pitch_y{8.5};

    TSpectral quantum_efficiency = default_qe();

    float full_well_capacity = 20000.f; // e-

    bool simulate_noise = true; // Whether to simulate noise in the sensor readout
    float read_noise = 10.f;    // e- RMS
    float dark_current = 1.f;   // e-/s
    float bias_level_dn = 10.f; // ADU

    int bit_depth = 12;

    float gain = 1.22f; // e-/ADU

    units::Radian rotation = units::Radian{0}; // Sensor rotation angle

    float unity_db = 0.f; // Reference level for gain in dB

    /**
     * @brief Sets the gain in dB for the sensor.
     * @param gain_db The gain in decibels.
     * @throws std::runtime_error if the value is not finite, or gives a conversion gain that is
     *         not a positive finite value.
     */
    void set_gain_db(float gain_db)
    {
        const float new_gain = std::pow(10.f, (unity_db - gain_db) / 20.f);
        if (!std::isfinite(gain_db) || !(new_gain > 0.f) || !std::isfinite(new_gain)) {
            HUIRA_THROW_ERROR("SensorModel::set_gain_db - Gain in dB must be finite, and give a "
                              "positive finite conversion gain: " +
                              std::to_string(gain_db) + " dB");
        }
        gain = new_gain;
    }
    /**
     * @brief Returns the gain in dB for the sensor.
     * @return The gain in decibels.
     */
    float gain_db() const { return unity_db - 20.f * std::log10(gain); }

    constexpr TSpectral default_qe() const
    {
        TSpectral qe = TSpectral::photon_energies();
        return 0.5 * qe / qe.total();
    }
};

/**
 * @brief Abstract base class for sensor models.
 *
 * Defines the interface and configuration for all sensor models, including pixel pitch, quantum
 * efficiency, and noise parameters.
 *
 * @tparam TSpectral The spectral type (e.g., @ref RGB, @ref Visible8)
 */
template <IsSpectral TSpectral>
class SensorModel {
  public:
    SensorModel() = default;
    SensorModel(SensorConfig<TSpectral> config);

    virtual ~SensorModel() = default;

    void set_resolution(Resolution resolution);
    Resolution resolution() const { return {config_.resolution}; }

    void set_pixel_pitch(units::Micrometer pitch_x, units::Micrometer pitch_y);
    Vec2<float> pixel_pitch() const;

    void set_sensor_size(units::Millimeter width, units::Millimeter height);
    Vec2<float> sensor_size() const;

    void set_quantum_efficiency(const TSpectral& qe);
    TSpectral quantum_efficiency() const { return config_.quantum_efficiency; }

    void set_full_well_capacity(float fwc);
    float full_well_capacity() const { return config_.full_well_capacity; }

    void set_simulate_noise(bool simulate_noise) { config_.simulate_noise = simulate_noise; }
    bool simulate_noise() const { return config_.simulate_noise; }

    void set_read_noise(float read_noise);
    float read_noise() const { return config_.read_noise; }

    void set_dark_current(float dark_current);
    float dark_current() const { return config_.dark_current; }

    void set_bias_level_dn(float bias_level_dn);
    float bias_level_dn() const { return config_.bias_level_dn; }

    void set_bit_depth(int bit_depth);
    int bit_depth() const { return config_.bit_depth; }

    void set_conversion_gain(float gain);
    float conversion_gain() const { return config_.gain; }

    void set_unity_db(float unity_db);
    float unity_db() const { return config_.unity_db; }

    void set_gain_db(float gain_db);
    float gain_db() const { return config_.gain_db(); }

    void set_rotation(units::Radian angle);
    units::Radian rotation() const { return config_.rotation; }

    void set_noise_seed(std::uint64_t seed);

    /// The seed of the sensor's noise. See set_noise_seed().
    std::uint64_t noise_seed() const { return noise_seed_; }

    static void check_resolution(const Resolution& resolution, const std::string& caller);
    static void check_pixel_pitch(units::Micrometer pitch_x,
                                  units::Micrometer pitch_y,
                                  const std::string& caller);
    static void
    check_sensor_size(units::Millimeter width, units::Millimeter height, const std::string& caller);

    /// Convert the frame buffer's received power into its sensor response. Only called (via
    /// CameraModel::readout()) when the frame buffer has a sensor response to write into.
    virtual void readout(FrameBuffer<TSpectral>& fb, units::Second exposure_time) const = 0;

  protected:
    SensorConfig<TSpectral> config_;

    /// The noise seed, and how many readouts have been made since it was set. Together with
    /// the row they key each row's random numbers (see detail::SensorRandom).
    std::uint64_t noise_seed_ = detail::next_default_noise_seed();
    mutable std::atomic<std::uint64_t> readout_count_{0};

    /// The number of a new readout, for keying its random numbers.
    std::uint64_t next_readout_number_() const
    {
        return readout_count_.fetch_add(1, std::memory_order_relaxed);
    }

    /// Largest bit depth accepted: that of the widest integer type. Real ADCs are far narrower
    /// (scientific sensors reach 16 to 18 bits); this only rules out nonsense.
    static constexpr int MAX_BIT_DEPTH = 64;

    friend class CameraModel<TSpectral>;
};

template <typename T, typename TSpectral>
concept IsSensor = std::derived_from<T, SensorModel<TSpectral>>;
} // namespace huira

#include "huira_impl/cameras/sensors/sensor_model.ipp"
