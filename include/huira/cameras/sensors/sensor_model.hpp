
#pragma once

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <mutex>
#include <numbers>
#include <string>

#include "huira/cameras/sensors/sensor_calibration.hpp"
#include "huira/concepts/spectral_concepts.hpp"
#include "huira/images/image.hpp"
#include "huira/render/frame_buffer.hpp"
#include "huira/units/units.hpp"
#include "huira/util/macros.hpp"
#include "huira/util/removed_api.hpp"

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
 * on threads, or on the order rows are read out in. Its integers are the same with every
 * standard library, unlike std::normal_distribution and the like; the normal and Poisson draws
 * made from them go through std::log, std::exp and the like, which can differ in the last bit
 * between platforms, so frames repeat exactly on one platform and toolchain.
 */
class SensorRandom {
  public:
    /// What a stream of random numbers is for. Each has its own, so that adding draws to one
    /// never changes another's: a sensor with row noise has the same pixel noise as without.
    enum class Stream : std::uint64_t {
        Rows = 1,      ///< One draw per row of a readout (row noise)
        Columns = 2,   ///< The draws for a readout's columns (column noise)
        Prnu = 3,      ///< Each pixel's gain, per row, the same in every readout (PRNU)
        HotPixels = 4, ///< Which pixels are hot, and how, per row, the same in every readout
    };

    /// The random numbers for the pixels of one row of one readout.
    SensorRandom(std::uint64_t seed, std::uint64_t readout, std::uint64_t row)
        : state_(mix_(seed ^ mix_(readout ^ mix_(row))))
    {
    }

    /// The random numbers of another stream, for one index (a row, say) of one readout. Keyed
    /// by the stream too, so independent of the pixels' and of each other's.
    SensorRandom(Stream stream, std::uint64_t seed, std::uint64_t readout, std::uint64_t index)
        : state_(mix_(mix_(STREAM_KEY_ ^ static_cast<std::uint64_t>(stream)) ^ seed ^
                      mix_(readout ^ mix_(index))))
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

    /// Mixed with a stream's number into its keys (an arbitrary constant: the first 64 bits of
    /// the fractional part of the square root of 2).
    static constexpr std::uint64_t STREAM_KEY_ = 0x6A09E667F3BCC908ULL;

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

    bool simulate_noise = true;         // See SensorModel::enable_noise()
    bool simulate_imperfections = true; // See SensorModel::enable_imperfections()
    float read_noise = 10.f;            // e- RMS
    float row_noise = 0.f;              // e- RMS, shared by a row (SensorModel::set_row_noise())
    float column_noise = 0.f;           // e- RMS, shared by a column
    float prnu = 0.f;                   // Std. dev. of each pixel's gain (SensorModel::set_prnu())
    float hot_pixel_fraction = 0.f;     // See SensorModel::set_hot_pixels()
    float hot_pixel_dark_current = 0.f; // e-/s, the hot pixels' median
    float hot_pixel_spread = 0.f;       // Std. dev. of ln(dark current) over the hot pixels
    float dark_current = 1.f;           // e-/s
    float bias_level_dn = 10.f;         // ADU

    int bit_depth = 12;

    // e-/ADU. Matched to the full well, slightly below full_well_capacity / (2^bit_depth - 1 -
    // bias_level_dn) = 4.896, so the ADC saturates just before the well is full (at 19935 e-):
    // nearly the whole well is used, and saturated pixels read exactly the largest DN.
    float gain = 4.88f;

    units::Radian rotation =
        units::Radian{0}; // Roll about the optical axis (see SensorModel::set_roll())

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

    /**
     * @brief The default quantum efficiency: 0.5 in the bluest bin, falling in proportion to
     * photon energy towards the red.
     *
     * Proportional to photon energy, a pixel collects the same number of electrons for the same
     * power in every bin, so the default sensor is white balanced. Normalized to its peak, rather
     * than its sum, its sensitivity does not depend on how many bins the spectral type has.
     */
    constexpr TSpectral default_qe() const
    {
        TSpectral qe = TSpectral::photon_energies();
        return 0.5 * qe / qe.max();
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

    /// A copy has the same settings and maps (shared, as they are never changed), but a new
    /// default noise seed (see set_noise_seed()), so noise and fixed patterns of its own, and no
    /// readouts yet. Set the same seed on both for identical noise.
    SensorModel(const SensorModel& other)
        : config_(other.config_), dark_current_map_(other.dark_current_map_),
          bias_map_(other.bias_map_), bias_map_kind_(other.bias_map_kind_),
          filter_transmission_(other.filter_transmission_), flat_(other.flat_)
    {
    }
    SensorModel& operator=(const SensorModel& other)
    {
        if (this != &other) {
            config_ = other.config_;
            dark_current_map_ = other.dark_current_map_;
            bias_map_ = other.bias_map_;
            bias_map_kind_ = other.bias_map_kind_;
            filter_transmission_ = other.filter_transmission_;
            flat_ = other.flat_;
            noise_seed_ = detail::next_default_noise_seed();
            readout_count_.store(0, std::memory_order_relaxed);
        }
        return *this;
    }

    /// Moving keeps the noise: the same sensor, somewhere else.
    SensorModel(SensorModel&& other) noexcept
        : config_(std::move(other.config_)), dark_current_map_(std::move(other.dark_current_map_)),
          bias_map_(std::move(other.bias_map_)), bias_map_kind_(other.bias_map_kind_),
          filter_transmission_(other.filter_transmission_), flat_(std::move(other.flat_)),
          noise_seed_(other.noise_seed_),
          readout_count_(other.readout_count_.load(std::memory_order_relaxed))
    {
    }
    SensorModel& operator=(SensorModel&& other) noexcept
    {
        config_ = std::move(other.config_);
        dark_current_map_ = std::move(other.dark_current_map_);
        bias_map_ = std::move(other.bias_map_);
        bias_map_kind_ = other.bias_map_kind_;
        filter_transmission_ = other.filter_transmission_;
        flat_ = std::move(other.flat_);
        noise_seed_ = other.noise_seed_;
        readout_count_.store(other.readout_count_.load(std::memory_order_relaxed),
                             std::memory_order_relaxed);
        return *this;
    }

    virtual ~SensorModel() = default;

    void set_resolution(Resolution resolution);
    Resolution resolution() const { return {config_.resolution}; }

    void set_pixel_pitch(units::Micrometer pitch_x, units::Micrometer pitch_y);
    Vec2<float> pixel_pitch() const;

    void set_sensor_size(units::Millimeter width, units::Millimeter height);
    Vec2<float> sensor_size() const;

    void set_quantum_efficiency(const TSpectral& qe);
    TSpectral quantum_efficiency() const { return config_.quantum_efficiency; }

    /// The transmission of the filter in front of the sensor, per bin: what the camera's
    /// selected filter sets (CameraModel::select_filter()), or 1 everywhere without one.
    TSpectral filter_transmission() const { return filter_transmission_; }

    /// Whether the filter in front of the sensor has a flat. See flat().
    bool has_flat() const { return flat_ != nullptr; }

    Image<float> flat() const;

    void set_full_well_capacity(float fwc);
    float full_well_capacity() const { return config_.full_well_capacity; }

    /// Turn the sensor's noise on (the default) or off: shot noise, read noise, and row and
    /// column noise. Off, each pixel collects the expected number of electrons. Independent of
    /// enable_imperfections(); docs/design_overview/sensor_modeling.rst, "Noise and
    /// imperfections", has what the two give together.
    void enable_noise(bool noise = true) { config_.simulate_noise = noise; }

    /// Whether the sensor's noise is on (the default). See enable_noise().
    bool noise_enabled() const { return config_.simulate_noise; }

    /// Turn the sensor's imperfections on (the default) or off: what is the same in every frame
    /// and a calibration would remove. That is the dark current (single value, map and hot
    /// pixels), PRNU, the selected filter's flat, and the bias map. Off, each pixel's gain is 1
    /// and the bias is the bias level; the settings are kept. Independent of enable_noise(); see
    /// docs/design_overview/sensor_modeling.rst, "Noise and imperfections".
    void enable_imperfections(bool imperfections = true)
    {
        config_.simulate_imperfections = imperfections;
    }

    /// Whether the sensor's imperfections are on (the default). See enable_imperfections().
    bool imperfections_enabled() const { return config_.simulate_imperfections; }

    void set_read_noise(float read_noise);
    float read_noise() const { return config_.read_noise; }

    void set_row_noise(float row_noise);

    /// The row noise, in e- RMS. See set_row_noise().
    float row_noise() const { return config_.row_noise; }

    void set_column_noise(float column_noise);

    /// The column noise, in e- RMS. See set_column_noise().
    float column_noise() const { return config_.column_noise; }

    void set_prnu(float prnu);

    /// The standard deviation of each pixel's gain (0.01 is 1%). See set_prnu().
    float prnu() const { return config_.prnu; }

    Image<float> prnu_map() const;

    void set_hot_pixels(float fraction, float dark_current, float spread = 0.f);

    /// The fraction of the pixels that are hot. See set_hot_pixels().
    float hot_pixel_fraction() const { return config_.hot_pixel_fraction; }

    /// The hot pixels' median dark current, in e-/s, added to the sensor's. See set_hot_pixels().
    float hot_pixel_dark_current() const { return config_.hot_pixel_dark_current; }

    /// The spread of the hot pixels' dark currents: the standard deviation of their natural log.
    /// See set_hot_pixels().
    float hot_pixel_spread() const { return config_.hot_pixel_spread; }

    Image<float> hot_pixel_map() const;

    void set_dark_current(float dark_current);
    float dark_current() const { return config_.dark_current; }

    void set_dark_current_map(Image<float> electrons_per_second);
    void clear_dark_current_map();

    /// Whether a dark current map is set. See set_dark_current_map().
    bool has_dark_current_map() const { return dark_current_map_ != nullptr; }

    Image<float> dark_current_map() const;

    void set_bias_level_dn(float bias_level_dn);
    float bias_level_dn() const { return config_.bias_level_dn; }

    void set_bias_map(Image<float> bias_dn, BiasMapKind kind = BiasMapKind::Absolute);
    void clear_bias_map();

    /// Whether a bias map is set. See set_bias_map().
    bool has_bias_map() const { return bias_map_ != nullptr; }

    /// What the values of the bias map set are (BiasMapKind::Absolute if none is). See
    /// set_bias_map().
    BiasMapKind bias_map_kind() const { return bias_map_kind_; }

    Image<float> bias_map() const;

    void set_bit_depth(int bit_depth);
    int bit_depth() const { return config_.bit_depth; }

    void set_conversion_gain(float gain);
    float conversion_gain() const { return config_.gain; }

    void set_unity_db(float unity_db);
    float unity_db() const { return config_.unity_db; }

    void set_gain_db(float gain_db);
    float gain_db() const { return config_.gain_db(); }

    void set_roll(units::Radian angle);

    /// The sensor's roll about the optical axis. See set_roll().
    units::Radian roll() const { return config_.rotation; }

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

    // Removed: calling these fails to compile, with what to use instead.
    template <typename... Args>
    void set_simulate_noise(Args&&...)
    {
        static_assert(detail::removed_api<Args...>,
                      "SensorModel::set_simulate_noise() was removed in v0.9.10. Use "
                      "enable_noise() instead.");
    }

    template <typename... Args>
    bool simulate_noise(Args&&...) const
    {
        static_assert(detail::removed_api<Args...>,
                      "SensorModel::simulate_noise() was removed in v0.9.10. Use noise_enabled() "
                      "instead.");
        return {};
    }

    template <typename... Args>
    void set_rotation(Args&&...)
    {
        static_assert(detail::removed_api<Args...>,
                      "SensorModel::set_rotation() was removed in v0.9.10. Use set_roll() "
                      "instead.");
    }

    template <typename... Args>
    units::Radian rotation(Args&&...) const
    {
        static_assert(detail::removed_api<Args...>,
                      "SensorModel::rotation() was removed in v0.9.10. Use roll() instead.");
        return {};
    }

  protected:
    SensorConfig<TSpectral> config_;

    /// Each pixel's dark current, in e-/s, in place of config_.dark_current; null if none is set.
    /// Shared by copies of the sensor, so never changed: a new map replaces it.
    std::shared_ptr<const Image<float>> dark_current_map_;

    /// Each pixel's bias, in DN, or its offset from config_.bias_level_dn (bias_map_kind_); null
    /// if none is set. Shared by copies of the sensor, as dark_current_map_ is.
    std::shared_ptr<const Image<float>> bias_map_;
    BiasMapKind bias_map_kind_ = BiasMapKind::Absolute;

    /// The filter in front of the sensor, as the camera's filter wheel sets it
    /// (set_filter_response_()): its transmission per bin, 1 without one, and its flat, each
    /// pixel's response relative to the principal point's, null without one. The flat is shared
    /// by the wheel and copies of the sensor, so never changed.
    TSpectral filter_transmission_{1.f};
    std::shared_ptr<const Image<float>> flat_;

    /// Sets the filter in front of the sensor: what the camera's filter wheel does when a filter
    /// is selected (CameraModel::select_filter()), with a clear filter and no flat when the wheel
    /// is empty or cleared. The transmission is a ratio per bin and the flat, if any, is at the
    /// sensor's resolution, finite and non-negative, as CameraModel::add_filter() checks. Warns
    /// if the response, QE x transmission x flat, is above 1 anywhere: more electrons than
    /// photons.
    void set_filter_response_(const TSpectral& transmission,
                              std::shared_ptr<const Image<float>> flat);

    /// Warns if the response, QE x transmission x flat, is above 1 anywhere.
    void check_response_() const;

    static void check_bias_(float bias_level_dn, int bit_depth, const char* caller);

    /// Throws unless every pixel's bias, from the map, is in [0, 2^bit_depth - 1).
    static void check_bias_map_(const Image<float>& map,
                                BiasMapKind kind,
                                float bias_level_dn,
                                int bit_depth,
                                const char* caller);

    /// Throws unless the maps set, and the filter's flat, are of the given resolution: a
    /// readout's. For the sensor models' readout().
    void check_map_resolutions_(const Resolution& resolution, const char* caller) const;

    /// The noise seed, and how many readouts have been made since it was set. Together with
    /// the row they key each row's random numbers (see detail::SensorRandom).
    std::uint64_t noise_seed_ = detail::next_default_noise_seed();
    mutable std::atomic<std::uint64_t> readout_count_{0};

    /// The number of a new readout, for keying its random numbers.
    std::uint64_t next_readout_number_() const
    {
        return readout_count_.fetch_add(1, std::memory_order_relaxed);
    }

    /// A fixed pattern (each pixel's gain, or hot pixels' dark current) as last made, and what it
    /// was made from, so that it is made again only when that changes. Made from the noise seed,
    /// so a copy of the sensor, with a seed of its own, makes its own.
    struct FixedPatternCache {
        std::mutex mutex;
        std::shared_ptr<const Image<float>> map;
        std::array<double, 7> key{};
        bool made = false;
    };
    mutable FixedPatternCache prnu_cache_;
    mutable FixedPatternCache hot_pixel_cache_;

    /// Each pixel's gain (PRNU), or null without PRNU. Made from the noise seed when first
    /// needed after it, the PRNU or the resolution changed.
    std::shared_ptr<const Image<float>> prnu_gains_() const;

    /// Each pixel's dark current as a hot pixel, in e-/s, 0 for the others, or null without hot
    /// pixels. Made as prnu_gains_() is.
    std::shared_ptr<const Image<float>> hot_pixel_dark_currents_() const;

    /// Largest bit depth accepted. The sensor response holds DN / (2^bit_depth - 1) as a float,
    /// from which every DN is recovered exactly, by round(value * (2^bit_depth - 1)), up to 24
    /// bits (a float's significand). This limits how the response is stored, not the physics:
    /// the ADC clamps every DN to 2^bit_depth - 1. Real ADCs are narrower (scientific sensors
    /// reach 16 to 18 bits).
    static constexpr int MAX_BIT_DEPTH = 24;

    friend class CameraModel<TSpectral>;
};

template <typename T, typename TSpectral>
concept IsSensor = std::derived_from<T, SensorModel<TSpectral>>;
} // namespace huira

#include "huira_impl/cameras/sensors/sensor_model.ipp"
