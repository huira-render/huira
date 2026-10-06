#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <string>

#include "huira/images/image.hpp"
#include "huira/util/logger.hpp"

namespace huira::detail {

/**
 * @brief How an image writer turns an image's values into a file's unsigned integers.
 *
 * With a sensor bit depth and PixelScaling::Counts, a value v holds DN / (2^bits - 1), and the
 * file gets the DN: as they are when the file has at least the sensor's bits, or with the lowest
 * bits dropped (shifted out) when it has fewer. Otherwise, v in [0, 1] is stretched over the
 * file's range.
 */
struct IntegerEncoding {
    int file_bits = 8;   ///< Bits per sample in the file.
    int sensor_bits = 0; ///< The sensor's bit depth when writing counts; 0 otherwise.
    int shift = 0;       ///< Low bits dropped: sensor_bits - file_bits, when positive.

    IntegerEncoding(int file_bits_in, int image_sensor_bits, PixelScaling scaling)
        : file_bits(file_bits_in),
          sensor_bits(scaling == PixelScaling::Counts ? std::max(image_sensor_bits, 0) : 0),
          shift(std::max(sensor_bits - file_bits_in, 0))
    {
    }

    [[nodiscard]] bool counts() const { return sensor_bits > 0; }

    /// Largest value of the file's type: 2^file_bits - 1.
    [[nodiscard]] std::uint64_t file_max() const { return (std::uint64_t{1} << file_bits) - 1; }

    /// The sensor's largest DN: 2^sensor_bits - 1.
    [[nodiscard]] std::uint64_t sensor_max() const { return (std::uint64_t{1} << sensor_bits) - 1; }

    /// The DN a sensor response value holds (counts only), within the sensor's range.
    [[nodiscard]] std::uint64_t dn(float value) const
    {
        const double max = static_cast<double>(sensor_max());
        return static_cast<std::uint64_t>(
            std::clamp(std::round(static_cast<double>(value) * max), 0.0, max));
    }

    /// A finite value stretched over the file's range: round(clamp(v, 0, 1) * file_max).
    [[nodiscard]] std::uint64_t full_range(float value) const
    {
        const double max = static_cast<double>(file_max());
        return static_cast<std::uint64_t>(
            std::round(std::clamp(static_cast<double>(value), 0.0, 1.0) * max));
    }

    /// The file's value for a finite image value, writing raw DN when counting: what TIFF and
    /// FITS store.
    [[nodiscard]] std::uint64_t raw(float value) const
    {
        return counts() ? (dn(value) >> shift) : full_range(value);
    }
};

/**
 * @brief Left-bit replication: a value of from_bits bits scaled to to_bits bits by repeating its
 * bits, which the PNG specification recommends for samples with fewer bits than the file. The
 * top from_bits bits of the result are the value, so value = result >> (to_bits - from_bits).
 */
inline std::uint64_t replicate_bits(std::uint64_t value, int from_bits, int to_bits)
{
    if (from_bits >= to_bits || from_bits <= 0) {
        return value;
    }
    std::uint64_t result = 0;
    int filled = 0;
    while (filled < to_bits) {
        result = (result << from_bits) | value;
        filled += from_bits;
    }
    return result >> (filled - to_bits);
}

/**
 * @brief Counts the values an image writer cannot represent (NaN and infinities) in a format
 * without missing data, which it writes as 0, and reports them once the image is written.
 */
class NonFiniteCounter {
  public:
    /// True, and counted, if the value is not finite.
    bool not_finite(float value)
    {
        if (std::isnan(value)) {
            ++nan_;
            return true;
        }
        if (std::isinf(value)) {
            ++(value > 0.f ? positive_inf_ : negative_inf_);
            return true;
        }
        return false;
    }

    /// Log a warning, if any value was not finite.
    void warn(const std::string& writer, const std::string& path) const
    {
        if (nan_ + positive_inf_ + negative_inf_ == 0) {
            return;
        }
        HUIRA_LOG_WARNING(writer + " - " + path + " has no way to mark missing data, so values " +
                          "that are not finite were written as 0: " + std::to_string(nan_) +
                          " NaN, " + std::to_string(positive_inf_) + " +inf and " +
                          std::to_string(negative_inf_) + " -inf.");
    }

  private:
    std::size_t nan_ = 0;
    std::size_t positive_inf_ = 0;
    std::size_t negative_inf_ = 0;
};

} // namespace huira::detail
