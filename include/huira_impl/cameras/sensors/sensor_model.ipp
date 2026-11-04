
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <mutex>
#include <string>
#include <utility>

#include "huira/core/types.hpp"
#include "huira/util/logger.hpp"
#include "tbb/blocked_range.h"
#include "tbb/parallel_for.h"
#include "tbb/task_arena.h"

namespace huira {

namespace detail {
inline std::string resolution_text(const Resolution& resolution)
{
    return std::to_string(resolution.x) + "x" + std::to_string(resolution.y);
}
} // namespace detail

/**
 * @brief Constructs a SensorModel with the given configuration.
 *
 * @param config The sensor configuration parameters.
 * @throws std::runtime_error if any parameter is invalid; see the setters.
 */
template <IsSpectral TSpectral>
SensorModel<TSpectral>::SensorModel(SensorConfig<TSpectral> config)
{
    // Each setter checks its own value:
    set_resolution(config.resolution);
    set_pixel_pitch(config.pitch_x, config.pitch_y);
    set_quantum_efficiency(config.quantum_efficiency);
    set_full_well_capacity(config.full_well_capacity);
    enable_noise(config.simulate_noise);
    enable_imperfections(config.simulate_imperfections);
    set_read_noise(config.read_noise);
    set_row_noise(config.row_noise);
    set_column_noise(config.column_noise);
    set_prnu(config.prnu);
    set_hot_pixels(
        config.hot_pixel_fraction, config.hot_pixel_dark_current, config.hot_pixel_spread);
    set_dark_current(config.dark_current);
    // The bias is checked against the bit depth, so the bit depth first, from no bias:
    config_.bias_level_dn = 0.f;
    set_bit_depth(config.bit_depth);
    set_bias_level_dn(config.bias_level_dn);
    set_conversion_gain(config.gain);
    set_unity_db(config.unity_db);
    set_roll(config.rotation);
}

/**
 * @brief Checks that a resolution has at least one pixel in each direction, and no more pixels
 * than an image can hold (INT_MAX, since pixels are indexed in int).
 *
 * @param resolution The resolution to check.
 * @param caller Name for the error message.
 * @throws std::runtime_error if it does not.
 */
template <IsSpectral TSpectral>
void SensorModel<TSpectral>::check_resolution(const Resolution& resolution,
                                              const std::string& caller)
{
    if (resolution.x < 1 || resolution.y < 1) {
        HUIRA_THROW_ERROR(caller + " - Resolution must be at least 1x1: " +
                          std::to_string(resolution.x) + "x" + std::to_string(resolution.y));
    }
    if (static_cast<long long>(resolution.x) * static_cast<long long>(resolution.y) >
        std::numeric_limits<int>::max()) {
        HUIRA_THROW_ERROR(caller + " - Resolution must have at most " +
                          std::to_string(std::numeric_limits<int>::max()) + " pixels: " +
                          std::to_string(resolution.x) + "x" + std::to_string(resolution.y));
    }
}

/**
 * @brief Checks that pixel pitches are positive and finite.
 *
 * @param pitch_x The horizontal pixel pitch.
 * @param pitch_y The vertical pixel pitch.
 * @param caller Name for the error message.
 * @throws std::runtime_error if either is not.
 */
template <IsSpectral TSpectral>
void SensorModel<TSpectral>::check_pixel_pitch(units::Micrometer pitch_x,
                                               units::Micrometer pitch_y,
                                               const std::string& caller)
{
    // Checked as stored, in single precision, so that a value that rounds to 0 or infinity
    // there is caught too:
    const float x = pitch_x.to_si_f();
    const float y = pitch_y.to_si_f();
    if (!(x > 0.f) || !std::isfinite(x) || !(y > 0.f) || !std::isfinite(y)) {
        HUIRA_THROW_ERROR(caller + " - Pixel pitch must be positive and finite: " +
                          std::to_string(pitch_x.to_si()) + " m x " +
                          std::to_string(pitch_y.to_si()) + " m");
    }
}

/**
 * @brief Checks that sensor dimensions are positive and finite.
 *
 * @param width The sensor width.
 * @param height The sensor height.
 * @param caller Name for the error message.
 * @throws std::runtime_error if either is not.
 */
template <IsSpectral TSpectral>
void SensorModel<TSpectral>::check_sensor_size(units::Millimeter width,
                                               units::Millimeter height,
                                               const std::string& caller)
{
    const double w = width.to_si();
    const double h = height.to_si();
    if (!(w > 0.0) || !std::isfinite(w) || !(h > 0.0) || !std::isfinite(h)) {
        HUIRA_THROW_ERROR(caller + " - Sensor size must be positive and finite: " +
                          std::to_string(w) + " m x " + std::to_string(h) + " m");
    }
}

/**
 * @brief Sets the sensor resolution.
 *
 * A dark current map, bias map or filter flat of another size stays set, with a warning; a
 * readout with imperfections on throws until it is replaced or cleared (a flat, by
 * CameraModel::set_filter_flat()).
 *
 * @param resolution The new sensor resolution (width x height).
 * @throws std::runtime_error if the resolution is not at least 1x1.
 */
template <IsSpectral TSpectral>
void SensorModel<TSpectral>::set_resolution(Resolution resolution)
{
    check_resolution(resolution, "SensorModel::set_resolution");
    config_.resolution = resolution;

    auto warn = [&](const std::shared_ptr<const Image<float>>& map, const char* name) {
        if (map != nullptr && !(map->resolution() == resolution)) {
            HUIRA_LOG_WARNING("SensorModel::set_resolution - The sensor's " + std::string(name) +
                              " is " + detail::resolution_text(map->resolution()) +
                              ", and the sensor is now " + detail::resolution_text(resolution) +
                              ". Replace or clear it: a readout with imperfections on throws "
                              "until then.");
        }
    };
    warn(dark_current_map_, "dark current map");
    warn(bias_map_, "bias map");
    warn(flat_, "filter's flat");
}

/**
 * @brief Sets the pixel pitch of the sensor.
 *
 * @param pitch_x The horizontal pixel pitch in micrometers.
 * @param pitch_y The vertical pixel pitch in micrometers.
 * @throws std::runtime_error if either pitch is not positive and finite.
 */
template <IsSpectral TSpectral>
void SensorModel<TSpectral>::set_pixel_pitch(units::Micrometer pitch_x, units::Micrometer pitch_y)
{
    check_pixel_pitch(pitch_x, pitch_y, "SensorModel::set_pixel_pitch");
    config_.pitch_x = pitch_x;
    config_.pitch_y = pitch_y;
}

/**
 * @brief Returns the pixel pitch of the sensor.
 *
 * @return The pixel pitch as a 2D vector (x, y) in meters.
 */
template <IsSpectral TSpectral>
Vec2<float> SensorModel<TSpectral>::pixel_pitch() const
{
    return {config_.pitch_x.to_si(), config_.pitch_y.to_si()};
}

/**
 * @brief Sets the sensor size in millimeters.
 *
 * Computes and sets the pixel pitch based on the given sensor size and current resolution.
 *
 * @param width The sensor width in millimeters.
 * @param height The sensor height in millimeters.
 * @throws std::runtime_error if either dimension is invalid.
 */
template <IsSpectral TSpectral>
void SensorModel<TSpectral>::set_sensor_size(units::Millimeter width, units::Millimeter height)
{
    check_sensor_size(width, height, "SensorModel::set_sensor_size");
    units::Meter pitch_x(width.to_si() / static_cast<double>(config_.resolution.x));
    units::Meter pitch_y(height.to_si() / static_cast<double>(config_.resolution.y));
    set_pixel_pitch(pitch_x, pitch_y);
}

/**
 * @brief Returns the sensor size in meters.
 *
 * @return The sensor size as a 2D vector (width, height) in meters.
 */
template <IsSpectral TSpectral>
Vec2<float> SensorModel<TSpectral>::sensor_size() const
{
    Vec2<float> pitch = pixel_pitch();
    return {config_.resolution.x * pitch.x, config_.resolution.y * pitch.y};
}

/**
 * @brief Sets the quantum efficiency of the sensor.
 *
 * @param qe The quantum efficiency spectrum (values between 0 and 1).
 * @throws std::runtime_error if the values are invalid.
 */
template <IsSpectral TSpectral>
void SensorModel<TSpectral>::set_quantum_efficiency(const TSpectral& qe)
{
    if (!qe.valid_ratio()) {
        HUIRA_THROW_ERROR("SensorModel::set_quantum_efficiency - Quantum efficiency values must be "
                          "valid values between 0 and 1.");
    }
    config_.quantum_efficiency = qe;
    check_response_();
}

/**
 * @brief The flat of the filter in front of the sensor: each pixel's response relative to the
 * principal point's, by which its signal electrons are multiplied (see CameraModel::add_filter()
 * and flat_from_flat_field()); or, without one, 1 everywhere at the sensor's resolution.
 */
template <IsSpectral TSpectral>
Image<float> SensorModel<TSpectral>::flat() const
{
    if (flat_ != nullptr) {
        return *flat_;
    }
    return Image<float>(config_.resolution, 1.f);
}

template <IsSpectral TSpectral>
void SensorModel<TSpectral>::set_filter_response_(const TSpectral& transmission,
                                                  std::shared_ptr<const Image<float>> flat)
{
    filter_transmission_ = transmission;
    flat_ = std::move(flat);
    if (flat_ != nullptr && !(flat_->resolution() == config_.resolution)) {
        HUIRA_LOG_WARNING("SensorModel::set_filter_response_ - The filter's flat is " +
                          detail::resolution_text(flat_->resolution()) + ", and the sensor is " +
                          detail::resolution_text(config_.resolution) +
                          ". Replace or clear it: a readout with imperfections on throws until "
                          "then.");
    }
    check_response_();
}

template <IsSpectral TSpectral>
void SensorModel<TSpectral>::check_response_() const
{
    const TSpectral response = config_.quantum_efficiency * filter_transmission_;
    float largest_flat = 1.f;
    if (flat_ != nullptr) {
        largest_flat = 0.f;
        for (std::size_t i = 0; i < flat_->size(); ++i) {
            largest_flat = std::max(largest_flat, (*flat_)[i]);
        }
    }
    const float largest = response.max() * largest_flat;
    if (largest > 1.f) {
        HUIRA_LOG_WARNING("SensorModel - The response, quantum efficiency x filter transmission x "
                          "flat, is " +
                          std::to_string(largest) +
                          " at most: above 1, a pixel makes more electrons than it receives "
                          "photons. The quantum efficiency is the response at the principal "
                          "point, where the flat is normalized to 1, so lower it, or the flat.");
    }
}

/**
 * @brief Sets the full well capacity of the sensor.
 *
 * @param fwc The full well capacity in electrons.
 * @throws std::runtime_error if the value is invalid.
 */
template <IsSpectral TSpectral>
void SensorModel<TSpectral>::set_full_well_capacity(float fwc)
{
    if (fwc <= 0 || std::isinf(fwc) || std::isnan(fwc)) {
        HUIRA_THROW_ERROR(
            "SensorModel::set_full_well_capacity - Full well capacity must be a positive value: " +
            std::to_string(fwc) + " e-");
    }
    config_.full_well_capacity = fwc;
}

/**
 * @brief Sets the read noise of the sensor.
 *
 * @param read_noise The read noise in electrons RMS.
 * @throws std::runtime_error if the value is invalid.
 */
template <IsSpectral TSpectral>
void SensorModel<TSpectral>::set_read_noise(float read_noise)
{
    if (read_noise < 0 || std::isinf(read_noise) || std::isnan(read_noise)) {
        HUIRA_THROW_ERROR(
            "SensorModel::set_read_noise - Read noise must be a non-negative value: " +
            std::to_string(read_noise) + " e-");
    }
    config_.read_noise = read_noise;
}

/**
 * @brief Sets the row noise: a shift shared by all the pixels of a row, drawn anew for each row
 * in each readout. 0 by default.
 *
 * Added with the read noise, after the full well clamps the charge; for RGB, a row's channels
 * shift together. Rows run along the sensor's x axis; for a sensor read out along y, use
 * set_column_noise(). A fixed row pattern belongs in a bias map (set_bias_map()). Off with
 * enable_noise(false). Where it comes from: docs/design_overview/sensor_modeling.rst, "Read
 * Noise".
 *
 * @param row_noise The standard deviation, in e- RMS.
 * @throws std::runtime_error if the value is negative or not finite.
 */
template <IsSpectral TSpectral>
void SensorModel<TSpectral>::set_row_noise(float row_noise)
{
    if (!(row_noise >= 0.f) || !std::isfinite(row_noise)) {
        HUIRA_THROW_ERROR("SensorModel::set_row_noise - Row noise must be non-negative and "
                          "finite: " +
                          std::to_string(row_noise) + " e-");
    }
    config_.row_noise = row_noise;
}

/**
 * @brief Sets the column noise: a shift shared by all the pixels of a column, drawn anew for
 * each column in each readout. 0 by default.
 *
 * As set_row_noise(), along the sensor's y axis. A CMOS sensor has both, the row noise usually
 * the larger; a CCD has row noise only.
 *
 * @param column_noise The standard deviation, in e- RMS.
 * @throws std::runtime_error if the value is negative or not finite.
 */
template <IsSpectral TSpectral>
void SensorModel<TSpectral>::set_column_noise(float column_noise)
{
    if (!(column_noise >= 0.f) || !std::isfinite(column_noise)) {
        HUIRA_THROW_ERROR("SensorModel::set_column_noise - Column noise must be non-negative and "
                          "finite: " +
                          std::to_string(column_noise) + " e-");
    }
    config_.column_noise = column_noise;
}

/**
 * @brief Sets the PRNU (photo response non-uniformity): the standard deviation of each pixel's
 * gain, a fixed pattern made from the noise seed. 0 by default.
 *
 * Each pixel's signal electrons are multiplied by its gain, drawn once from a normal
 * distribution of mean 1 and this standard deviation (0.01 is 1%), never below 0. The light only:
 * not the dark current. An imperfection (enable_imperfections()). A flat from a real flat field
 * already holds the camera's PRNU, so leave this at 0 with one.
 *
 * @param prnu The standard deviation of each pixel's gain.
 * @throws std::runtime_error if the value is negative or not finite.
 */
template <IsSpectral TSpectral>
void SensorModel<TSpectral>::set_prnu(float prnu)
{
    if (!(prnu >= 0.f) || !std::isfinite(prnu)) {
        HUIRA_THROW_ERROR("SensorModel::set_prnu - PRNU must be non-negative and finite: " +
                          std::to_string(prnu));
    }
    config_.prnu = prnu;
}

/**
 * @brief Each pixel's gain from the PRNU (see set_prnu()), by which its signal electrons are
 * multiplied: or, without PRNU, 1 everywhere, at the sensor's resolution.
 */
template <IsSpectral TSpectral>
Image<float> SensorModel<TSpectral>::prnu_map() const
{
    const auto gains = prnu_gains_();
    return gains != nullptr ? *gains : Image<float>(config_.resolution, 1.f);
}

/**
 * @brief Gives the sensor hot pixels: a fraction of its pixels, a fixed pattern made from the
 * noise seed, that gather a dark current of their own on top of the sensor's. Off (a fraction of
 * 0) by default.
 *
 * Each pixel is hot with probability fraction. A hot pixel's dark current is dark_current times
 * exp(spread x N(0, 1)): log-normal, with that median. With a spread of 0 every hot pixel has
 * the median; 0.5 spreads them about 1.6x either way. A larger fraction keeps a smaller one's
 * hot pixels, with the same dark currents, and adds more. Dark current, so drawn with shot
 * noise; an imperfection (enable_imperfections()). A dark current map from a real master dark
 * already holds the camera's hot pixels, so leave these off with one. set_hot_pixels(0, 0) turns
 * them off.
 *
 * @param fraction The fraction of the pixels that are hot, from 0 to 1.
 * @param dark_current The hot pixels' median dark current, in e-/s; positive unless the fraction
 *        is 0.
 * @param spread The standard deviation of the natural log of their dark currents.
 * @throws std::runtime_error if the fraction is not in [0, 1], the dark current is not positive
 *         (with a fraction above 0), or the dark current or spread is negative or not finite.
 */
template <IsSpectral TSpectral>
void SensorModel<TSpectral>::set_hot_pixels(float fraction, float dark_current, float spread)
{
    if (!(fraction >= 0.f) || !(fraction <= 1.f)) {
        HUIRA_THROW_ERROR("SensorModel::set_hot_pixels - The fraction of hot pixels must be from "
                          "0 to 1: " +
                          std::to_string(fraction));
    }
    if (!(dark_current >= 0.f) || !std::isfinite(dark_current)) {
        HUIRA_THROW_ERROR("SensorModel::set_hot_pixels - The hot pixels' dark current must be "
                          "non-negative and finite: " +
                          std::to_string(dark_current) + " e-/s");
    }
    if (fraction > 0.f && dark_current == 0.f) {
        HUIRA_THROW_ERROR("SensorModel::set_hot_pixels - Hot pixels need a dark current above 0 "
                          "e-/s; a fraction of 0 turns them off.");
    }
    if (!(spread >= 0.f) || !std::isfinite(spread)) {
        HUIRA_THROW_ERROR("SensorModel::set_hot_pixels - The spread must be non-negative and "
                          "finite: " +
                          std::to_string(spread));
    }
    config_.hot_pixel_fraction = fraction;
    config_.hot_pixel_dark_current = dark_current;
    config_.hot_pixel_spread = spread;
}

/**
 * @brief Each pixel's dark current as a hot pixel, in e-/s, added to the sensor's: 0 for the
 * pixels that aren't hot, and everywhere without hot pixels. See set_hot_pixels().
 */
template <IsSpectral TSpectral>
Image<float> SensorModel<TSpectral>::hot_pixel_map() const
{
    const auto rates = hot_pixel_dark_currents_();
    return rates != nullptr ? *rates : Image<float>(config_.resolution, 0.f);
}

namespace detail {
/// A fixed pattern from its cache, or made (row by row, in parallel) and cached if what it is
/// made from changed. make_row(row, random, out) fills one row from that row's random numbers.
template <typename TCache, typename TMakeRow>
std::shared_ptr<const Image<float>> fixed_pattern(TCache& cache,
                                                  const std::array<double, 7>& key,
                                                  const Resolution& resolution,
                                                  SensorRandom::Stream stream,
                                                  std::uint64_t seed,
                                                  TMakeRow make_row)
{
    std::lock_guard<std::mutex> lock(cache.mutex);
    if (cache.made && cache.key == key) {
        return cache.map;
    }
    auto map = std::make_shared<Image<float>>(resolution);
    // Isolated, so that this thread can't pick up another readout of the sensor while it waits,
    // which would deadlock on the lock it holds.
    tbb::this_task_arena::isolate([&] {
        tbb::parallel_for(
            tbb::blocked_range<int>(0, resolution.y), [&](const tbb::blocked_range<int>& rows) {
                for (int y = rows.begin(); y < rows.end(); ++y) {
                    SensorRandom random(stream, seed, 0, static_cast<std::uint64_t>(y));
                    make_row(y, random, *map);
                }
            });
    });
    cache.map = std::move(map);
    cache.key = key;
    cache.made = true;
    return cache.map;
}
} // namespace detail

template <IsSpectral TSpectral>
std::shared_ptr<const Image<float>> SensorModel<TSpectral>::prnu_gains_() const
{
    const double prnu = static_cast<double>(config_.prnu);
    if (prnu == 0.0) {
        return nullptr;
    }
    const Resolution resolution = config_.resolution;
    const std::array<double, 7> key{static_cast<double>(noise_seed_ >> 32),
                                    static_cast<double>(noise_seed_ & 0xFFFFFFFFULL),
                                    static_cast<double>(resolution.x),
                                    static_cast<double>(resolution.y),
                                    prnu,
                                    0.0,
                                    0.0};
    return detail::fixed_pattern(prnu_cache_,
                                 key,
                                 resolution,
                                 detail::SensorRandom::Stream::Prnu,
                                 noise_seed_,
                                 [&](int y, detail::SensorRandom& random, Image<float>& gains) {
                                     for (int x = 0; x < resolution.x; ++x) {
                                         gains(x, y) = static_cast<float>(
                                             std::max(0.0, 1.0 + prnu * random.normal()));
                                     }
                                 });
}

template <IsSpectral TSpectral>
std::shared_ptr<const Image<float>> SensorModel<TSpectral>::hot_pixel_dark_currents_() const
{
    const double fraction = static_cast<double>(config_.hot_pixel_fraction);
    const double dark_current = static_cast<double>(config_.hot_pixel_dark_current);
    const double spread = static_cast<double>(config_.hot_pixel_spread);
    if (fraction == 0.0) {
        return nullptr;
    }
    const Resolution resolution = config_.resolution;
    const std::array<double, 7> key{static_cast<double>(noise_seed_ >> 32),
                                    static_cast<double>(noise_seed_ & 0xFFFFFFFFULL),
                                    static_cast<double>(resolution.x),
                                    static_cast<double>(resolution.y),
                                    fraction,
                                    dark_current,
                                    spread};
    return detail::fixed_pattern(
        hot_pixel_cache_,
        key,
        resolution,
        detail::SensorRandom::Stream::HotPixels,
        noise_seed_,
        [&](int y, detail::SensorRandom& random, Image<float>& rates) {
            for (int x = 0; x < resolution.x; ++x) {
                // One uniform and one normal for every pixel, hot or not, so that which pixels
                // are hot, and how, doesn't depend on the fraction: a larger one only adds more.
                const double u = random.uniform();
                const double n = random.normal();
                // (kept finite in float, whatever the spread)
                rates(x, y) = u < fraction
                                  ? static_cast<float>(std::min(
                                        dark_current * std::exp(spread * n),
                                        static_cast<double>(std::numeric_limits<float>::max())))
                                  : 0.f;
            }
        });
}

/**
 * @brief Sets the dark current of the sensor: the same for every pixel.
 *
 * A dark current map, while one is set, takes its place; a warning says so. An imperfection
 * (enable_imperfections()).
 *
 * @param dark_current The dark current in electrons per second.
 * @throws std::runtime_error if the value is invalid.
 */
template <IsSpectral TSpectral>
void SensorModel<TSpectral>::set_dark_current(float dark_current)
{
    if (dark_current < 0 || std::isinf(dark_current) || std::isnan(dark_current)) {
        HUIRA_THROW_ERROR(
            "SensorModel::set_dark_current - Dark current must be a non-negative value: " +
            std::to_string(dark_current) + " e-/s");
    }
    config_.dark_current = dark_current;
    if (dark_current_map_ != nullptr) {
        HUIRA_LOG_WARNING("SensorModel::set_dark_current - A dark current map is set, and is used "
                          "in place of the dark current until it is cleared "
                          "(clear_dark_current_map()).");
    }
}

/**
 * @brief Sets the dark current of each pixel, in place of the one dark current for all
 * (set_dark_current()).
 *
 * Each pixel's dark current, times the exposure time, is the mean number of electrons it
 * gathers in the dark, added to its signal before shot noise; for RGB, each channel gathers that
 * many. dark_current_from_master_dark() makes the map from a master dark and a master bias. In
 * the sensor response's layout (top row first). An imperfection (enable_imperfections()). Kept
 * until clear_dark_current_map() or replaced; copies of the sensor share it.
 *
 * @param electrons_per_second The dark current of each pixel, in e-/s, at the sensor's
 *        resolution.
 * @throws std::runtime_error if the map is not at the sensor's resolution, a value is negative
 *         or not finite, or the image has a sensor bit depth (a frame in normalized DN, not
 *         e-/s: see dark_current_from_master_dark()).
 */
template <IsSpectral TSpectral>
void SensorModel<TSpectral>::set_dark_current_map(Image<float> electrons_per_second)
{
    const char* caller = "SensorModel::set_dark_current_map";
    if (electrons_per_second.sensor_bit_depth() > 0) {
        HUIRA_THROW_ERROR(std::string(caller) +
                          " - The map has a sensor bit depth, so holds normalized DN (a sensor "
                          "response, or a frame read from a file), not electrons per second. "
                          "Make a map from a master dark with dark_current_from_master_dark().");
    }
    if (!(electrons_per_second.resolution() == config_.resolution)) {
        HUIRA_THROW_ERROR(std::string(caller) + " - The map must be at the sensor's resolution, " +
                          detail::resolution_text(config_.resolution) + ": it is " +
                          detail::resolution_text(electrons_per_second.resolution()));
    }
    for (std::size_t i = 0; i < electrons_per_second.size(); ++i) {
        const float rate = electrons_per_second[i];
        if (!(rate >= 0.f) || !std::isfinite(rate)) {
            HUIRA_THROW_ERROR(
                std::string(caller) +
                " - Each pixel's dark current must be "
                "non-negative and finite: " +
                std::to_string(rate) + " e-/s at pixel (" +
                std::to_string(i % static_cast<std::size_t>(config_.resolution.x)) + ", " +
                std::to_string(i / static_cast<std::size_t>(config_.resolution.x)) + ")");
        }
    }
    dark_current_map_ = std::make_shared<const Image<float>>(std::move(electrons_per_second));
}

/**
 * @brief Clears the dark current map, so that every pixel has the dark current again
 * (set_dark_current()).
 */
template <IsSpectral TSpectral>
void SensorModel<TSpectral>::clear_dark_current_map()
{
    dark_current_map_.reset();
}

/**
 * @brief The dark current of each pixel, in e-/s: the dark current map, or, if none is set, the
 * dark current everywhere at the sensor's resolution. See set_dark_current_map().
 */
template <IsSpectral TSpectral>
Image<float> SensorModel<TSpectral>::dark_current_map() const
{
    if (dark_current_map_ != nullptr) {
        return *dark_current_map_;
    }
    return Image<float>(config_.resolution, config_.dark_current);
}

/**
 * @brief Sets the bias level of the sensor in ADU: the same for every pixel.
 *
 * With a relative bias map, it is what the map's offsets are added to (see set_bias_map()), and
 * every pixel's bias must stay in range. With an absolute bias map, the map takes its place; a
 * warning says so.
 *
 * @param bias_level_dn The bias level in ADU.
 * @throws std::runtime_error if the value is negative or not finite, or at or above the largest
 *         value the bit depth holds, where every pixel would read out saturated; or if a pixel's
 *         bias, with a relative bias map, would be outside [0, 2^bit_depth - 1).
 */
template <IsSpectral TSpectral>
void SensorModel<TSpectral>::set_bias_level_dn(float bias_level_dn)
{
    if (bias_level_dn < 0 || std::isinf(bias_level_dn) || std::isnan(bias_level_dn)) {
        HUIRA_THROW_ERROR(
            "SensorModel::set_bias_level_dn - Bias level must be a non-negative value: " +
            std::to_string(bias_level_dn) + " ADU");
    }
    check_bias_(bias_level_dn, config_.bit_depth, "set_bias_level_dn");
    if (bias_map_ != nullptr && bias_map_kind_ == BiasMapKind::Relative) {
        check_bias_map_(*bias_map_,
                        bias_map_kind_,
                        bias_level_dn,
                        config_.bit_depth,
                        "SensorModel::set_bias_level_dn");
    }
    config_.bias_level_dn = bias_level_dn;
    if (bias_map_ != nullptr && bias_map_kind_ == BiasMapKind::Absolute) {
        HUIRA_LOG_WARNING("SensorModel::set_bias_level_dn - An absolute bias map is set, and is "
                          "used in place of the bias level until it is cleared "
                          "(clear_bias_map()).");
    }
}

/**
 * @brief Sets the bias of each pixel, in place of the one bias level for all, or as offsets from
 * it.
 *
 * - BiasMapKind::Absolute (the default): each pixel's bias, as a master bias frame gives it. The
 *   bias level is not used while the map is set.
 * - BiasMapKind::Relative: each pixel's offset from the bias level, averaging about 0 (a warning
 *   points out a mean beyond half a DN). The bias level still applies, and moves the whole map.
 *
 * Values are DN. An image with a sensor bit depth (a sensor response, or a frame read from a
 * file that records it) holds DN / (2^bits - 1), and an absolute map is converted from it; an
 * absolute map that looks normalized (no bit depth, every value at most 1 DN), and a relative
 * map with a bit depth, are refused: see docs/design_overview/sensor_modeling.rst, "Calibration
 * maps". In the sensor response's layout (top row first). An imperfection
 * (enable_imperfections()). Kept until clear_bias_map() or replaced; copies of the sensor share
 * it.
 *
 * @param bias_dn The bias, or the offsets from the bias level, of each pixel, in DN, at the
 *        sensor's resolution.
 * @param kind What the values are: see above.
 * @throws std::runtime_error if the map is not at the sensor's resolution, a value is not
 *         finite, a pixel's bias would be outside [0, 2^bit_depth - 1), the map is absolute and
 *         normalized, or the map is relative and has a sensor bit depth.
 */
template <IsSpectral TSpectral>
void SensorModel<TSpectral>::set_bias_map(Image<float> bias_dn, BiasMapKind kind)
{
    const char* caller = "SensorModel::set_bias_map";
    if (!(bias_dn.resolution() == config_.resolution)) {
        HUIRA_THROW_ERROR(std::string(caller) + " - The map must be at the sensor's resolution, " +
                          detail::resolution_text(config_.resolution) + ": it is " +
                          detail::resolution_text(bias_dn.resolution()));
    }
    if (kind == BiasMapKind::Relative && bias_dn.sensor_bit_depth() > 0) {
        HUIRA_THROW_ERROR(std::string(caller) +
                          " - A relative bias map holds offsets in DN, but this image has a "
                          "sensor bit depth, so holds normalized DN, as a sensor response or a "
                          "frame read from a file does. Give a master bias as "
                          "BiasMapKind::Absolute, or subtract its mean in DN first.");
    }
    if (kind == BiasMapKind::Absolute) {
        detail::check_not_normalized(bias_dn, "bias map", caller);
        const double scale = detail::dn_per_value(bias_dn);
        if (scale != 1.0) {
            for (std::size_t i = 0; i < bias_dn.size(); ++i) {
                bias_dn[i] = static_cast<float>(static_cast<double>(bias_dn[i]) * scale);
            }
        }
    }
    bias_dn.set_sensor_bit_depth(0); // DN, as converted
    check_bias_map_(bias_dn, kind, config_.bias_level_dn, config_.bit_depth, caller);

    if (kind == BiasMapKind::Relative) {
        double sum = 0.0;
        for (std::size_t i = 0; i < bias_dn.size(); ++i) {
            sum += static_cast<double>(bias_dn[i]);
        }
        const double mean = sum / static_cast<double>(bias_dn.size());
        if (std::abs(mean) > 0.5) {
            HUIRA_LOG_WARNING(std::string(caller) + " - The relative bias map's offsets average " +
                              std::to_string(mean) +
                              " DN. They are added to the bias level, so are meant to average "
                              "about 0; for a map of the bias itself, use BiasMapKind::Absolute.");
        }
    }
    bias_map_ = std::make_shared<const Image<float>>(std::move(bias_dn));
    bias_map_kind_ = kind;
}

/**
 * @brief Clears the bias map, so that every pixel has the bias level again
 * (set_bias_level_dn()).
 */
template <IsSpectral TSpectral>
void SensorModel<TSpectral>::clear_bias_map()
{
    bias_map_.reset();
    bias_map_kind_ = BiasMapKind::Absolute;
}

/**
 * @brief The bias of each pixel, in DN: an absolute bias map as it is, a relative one added to the
 * bias level, or, if none is set, the bias level everywhere at the sensor's resolution. See
 * set_bias_map().
 */
template <IsSpectral TSpectral>
Image<float> SensorModel<TSpectral>::bias_map() const
{
    if (bias_map_ == nullptr) {
        return Image<float>(config_.resolution, config_.bias_level_dn);
    }
    Image<float> bias = *bias_map_;
    if (bias_map_kind_ == BiasMapKind::Relative) {
        const double level = static_cast<double>(config_.bias_level_dn);
        for (std::size_t i = 0; i < bias.size(); ++i) {
            bias[i] = static_cast<float>(level + static_cast<double>(bias[i]));
        }
    }
    return bias;
}

/// Throws unless every pixel's bias, from the map (as it is, or added to the bias level), is
/// finite and in [0, 2^bit_depth - 1): below 0 the ADC would clamp it, and at the top every pixel
/// would read out saturated.
template <IsSpectral TSpectral>
void SensorModel<TSpectral>::check_bias_map_(const Image<float>& map,
                                             BiasMapKind kind,
                                             float bias_level_dn,
                                             int bit_depth,
                                             const char* caller)
{
    const double max_dn = std::ldexp(1.0, bit_depth) - 1.0;
    const double level = kind == BiasMapKind::Relative ? static_cast<double>(bias_level_dn) : 0.0;
    for (std::size_t i = 0; i < map.size(); ++i) {
        const double bias = level + static_cast<double>(map[i]);
        if (!std::isfinite(bias) || !(bias >= 0.0) || !(bias < max_dn)) {
            const auto width = static_cast<std::size_t>(map.width());
            HUIRA_THROW_ERROR(
                std::string(caller) + " - Each pixel's bias must be finite and in [0, " +
                std::to_string(static_cast<long long>(max_dn)) + ") DN, below the largest " +
                std::to_string(bit_depth) + "-bit value, or the pixel reads out saturated: it is " +
                std::to_string(bias) + " DN at pixel (" + std::to_string(i % width) + ", " +
                std::to_string(i / width) + ")" +
                (kind == BiasMapKind::Relative
                     ? ", from a bias level of " + std::to_string(bias_level_dn) + " DN"
                     : std::string()) +
                ".");
        }
    }
}

template <IsSpectral TSpectral>
void SensorModel<TSpectral>::check_map_resolutions_(const Resolution& resolution,
                                                    const char* caller) const
{
    auto check = [&](const std::shared_ptr<const Image<float>>& map, const char* name) {
        if (map != nullptr && !(map->resolution() == resolution)) {
            HUIRA_THROW_ERROR(std::string(caller) + " - The sensor's " + name + " is " +
                              detail::resolution_text(map->resolution()) +
                              ", but the frame read out is " + detail::resolution_text(resolution) +
                              ": set a map of the sensor's resolution, or clear it.");
        }
    };
    check(dark_current_map_, "dark current map");
    check(bias_map_, "bias map");
    check(flat_, "filter's flat");
}

/// Throws unless the bias is below the largest value the bit depth holds, where every pixel
/// would read out saturated.
template <IsSpectral TSpectral>
void SensorModel<TSpectral>::check_bias_(float bias_level_dn, int bit_depth, const char* caller)
{
    const double max_dn = std::ldexp(1.0, bit_depth) - 1.0;
    if (!(static_cast<double>(bias_level_dn) < max_dn)) {
        HUIRA_THROW_ERROR(std::string("SensorModel::") + caller + " - The bias level, " +
                          std::to_string(bias_level_dn) + " ADU, must be below the largest " +
                          std::to_string(bit_depth) + "-bit value, " + std::to_string(max_dn) +
                          ", or every pixel reads out saturated. Set the bias level first when "
                          "lowering both.");
    }
}

/**
 * @brief Sets the bit depth of the sensor.
 *
 * @param bit_depth The bit depth (number of bits per pixel).
 * @throws std::runtime_error if the value is not between 1 and MAX_BIT_DEPTH (24): the sensor
 *         response is stored as a float, from which every DN is recovered exactly only up to 24
 *         bits; or if the bias level, or a pixel's bias from the bias map, is at or above the
 *         largest value it holds.
 */
template <IsSpectral TSpectral>
void SensorModel<TSpectral>::set_bit_depth(int bit_depth)
{
    if (bit_depth < 1 || bit_depth > MAX_BIT_DEPTH) {
        HUIRA_THROW_ERROR("SensorModel::set_bit_depth - Bit depth must be between 1 and " +
                          std::to_string(MAX_BIT_DEPTH) + ": " + std::to_string(bit_depth) +
                          " bits. (The sensor response is stored as a float, from which every "
                          "digital number is recovered exactly only up to " +
                          std::to_string(MAX_BIT_DEPTH) + " bits.)");
    }
    check_bias_(config_.bias_level_dn, bit_depth, "set_bit_depth");
    if (bias_map_ != nullptr) {
        check_bias_map_(*bias_map_,
                        bias_map_kind_,
                        config_.bias_level_dn,
                        bit_depth,
                        "SensorModel::set_bit_depth");
    }
    config_.bit_depth = bit_depth;
}

/**
 * @brief Sets the conversion gain of the sensor in e-/ADU.
 *
 * @param gain The gain in electrons per ADU.
 * @throws std::runtime_error if the value is invalid.
 */
template <IsSpectral TSpectral>
void SensorModel<TSpectral>::set_conversion_gain(float gain)
{
    if (gain <= 0 || std::isinf(gain) || std::isnan(gain)) {
        HUIRA_THROW_ERROR("SensorModel::set_conversion_gain - Gain must be a positive value: " +
                          std::to_string(gain) + " e-/ADU");
    }
    config_.gain = gain;
}

/**
 * @brief Sets the unity dB reference level for gain.
 *
 * @param unity_db The unity dB reference level.
 * @throws std::runtime_error if the value is invalid.
 */
template <IsSpectral TSpectral>
void SensorModel<TSpectral>::set_unity_db(float unity_db)
{
    if (std::isinf(unity_db) || std::isnan(unity_db)) {
        HUIRA_THROW_ERROR(
            "SensorModel::set_unity_db - Unity dB reference level cannot be infinite or NaN: " +
            std::to_string(unity_db) + " dB");
    }
    config_.unity_db = unity_db;
}

/**
 * @brief Sets the gain in dB for the sensor.
 *
 * @param gain_db The gain in decibels.
 * @throws std::runtime_error if the value is not finite, or gives a conversion gain that is not a
 *         positive finite value.
 */
template <IsSpectral TSpectral>
void SensorModel<TSpectral>::set_gain_db(float gain_db)
{
    config_.set_gain_db(gain_db);
}

/**
 * @brief Sets the sensor's roll: its rotation about the optical axis, relative to the camera.
 *
 * The sensor's x axis is turned by the angle toward its y axis, so the scene turns the other
 * way in the image. See CameraModel::set_sensor_roll().
 *
 * @param angle The roll angle (any angle unit).
 * @throws std::runtime_error if the angle is not finite.
 */
template <IsSpectral TSpectral>
void SensorModel<TSpectral>::set_roll(units::Radian angle)
{
    if (!std::isfinite(angle.to_si())) {
        HUIRA_THROW_ERROR("SensorModel::set_roll - Roll must be finite: " +
                          std::to_string(angle.to_si()) + " rad");
    }
    config_.rotation = angle;
}
/**
 * @brief Seed the sensor's noise.
 *
 * Each readout draws its noise from random numbers determined by the seed, by how many readouts
 * the sensor has made since the seed was set, and by the row. So a frame's noise does not depend
 * on other cameras, threads or render order, successive frames differ, and setting the same seed
 * again repeats the same sequence of frames.
 *
 * The seed also makes the sensor's fixed patterns, PRNU (set_prnu()) and hot pixels
 * (set_hot_pixels()), the same in every readout: another seed gives another sensor's.
 *
 * Sensors get distinct seeds by default, in the order they are made, so that two cameras never
 * share their noise unless given the same seed. A sensor set with CameraModel::set_sensor() is
 * a new sensor, with a new seed, and so is a copy of a sensor (it has the same settings).
 *
 * @param seed Any value.
 */
template <IsSpectral TSpectral>
void SensorModel<TSpectral>::set_noise_seed(std::uint64_t seed)
{
    noise_seed_ = seed;
    readout_count_.store(0, std::memory_order_relaxed);
}

} // namespace huira
