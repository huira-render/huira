#pragma once

#include <string>

#include "huira/core/types.hpp"

namespace huira {

/**
 * @brief Which coordinate the center of the first pixel has.
 */
enum class PixelCenter {
    /// Pixel centers at integers, the first at 0. Pixel i covers [i - 0.5, i + 0.5).
    Integer,

    /// Pixel edges at integers, so the first pixel's center is at 0.5. Pixel i covers
    /// [i, i + 1).
    HalfInteger,

    /// Pixel centers at integers, the first at 1. Pixel n (counting from 1) covers
    /// [n - 0.5, n + 0.5).
    OneBased,
};

/**
 * @brief Which corner of the image pixel coordinates count from.
 *
 * x always runs left to right. This only changes how y is numbered; images are always stored
 * with their top row first.
 */
enum class PixelOrigin {
    /// y runs down from the top row.
    TopLeft,

    /// y runs up from the bottom row.
    BottomLeft,
};

/**
 * @brief The pixel coordinate convention a camera's users work in.
 *
 * Pixel coordinates are exchanged with a camera in three places: the principal point passed to
 * it (configure_sensor_from_pitch(), configure_sensor_from_size(), set_intrinsics() and
 * set_intrinsic_matrix()), the positions project_point() returns, and the positions cast_ray()
 * takes. A convention fixes what those numbers mean. Different tools number pixels
 * differently, and the differences are silent: a calibration made in one convention and used
 * in another is off by half a pixel or a whole one, or mirrored vertically.
 *
 * The convention never changes the rendered image when the principal point is left at its
 * default, the center of the sensor: it only changes the numbers used to talk about it.
 *
 * The default is OpenCV's, which most calibration tools share. Presets cover common tools:
 *
 * | Preset     | First pixel center | y runs | Used by                                         |
 * |------------|--------------------|--------|-------------------------------------------------|
 * | `opencv()` | (0, 0)             | down   | OpenCV, Kornia, most calibration tools          |
 * | `colmap()` | (0.5, 0.5)         | down   | COLMAP, Direct3D/Vulkan/Metal; Huira pre-0.9.10 |
 * | `matlab()` | (1, 1)             | down   | MATLAB                                          |
 * | `fits()`   | (1, 1)             | up     | FITS / WCS (CRPIX), DS9                         |
 *
 * Internally Huira works in sensor coordinates: x right and y down from the sensor's top-left
 * corner, in pixels, so that pixel i covers [i, i + 1). to_sensor() and from_sensor() convert.
 */
struct PixelConvention {
    PixelCenter center = PixelCenter::Integer;
    PixelOrigin origin = PixelOrigin::TopLeft;

    /// First pixel centered at (0, 0), y down. The default.
    static constexpr PixelConvention opencv()
    {
        return {PixelCenter::Integer, PixelOrigin::TopLeft};
    }

    /// First pixel centered at (0.5, 0.5), y down: pixel edges at integers.
    static constexpr PixelConvention colmap()
    {
        return {PixelCenter::HalfInteger, PixelOrigin::TopLeft};
    }

    /// First pixel centered at (1, 1), y down.
    static constexpr PixelConvention matlab()
    {
        return {PixelCenter::OneBased, PixelOrigin::TopLeft};
    }

    /// First pixel centered at (1, 1), y up from the bottom row: the convention of FITS WCS
    /// keywords such as CRPIX, for images written by Huira (which stores FITS rows bottom up).
    static constexpr PixelConvention fits()
    {
        return {PixelCenter::OneBased, PixelOrigin::BottomLeft};
    }

    /// Coordinate of the first pixel's center along either axis: 0, 0.5 or 1.
    [[nodiscard]] constexpr float first_pixel_center() const noexcept
    {
        switch (center) {
        case PixelCenter::Integer:
            return 0.f;
        case PixelCenter::HalfInteger:
            return 0.5f;
        case PixelCenter::OneBased:
            return 1.f;
        default:
            return 0.f; // unreachable: every value is handled above
        }
    }

    /// Converts a position in this convention to sensor coordinates.
    [[nodiscard]] constexpr Pixel to_sensor(const Pixel& p, const Resolution& resolution) const
    {
        const float shift = first_pixel_center() - 0.5f;
        const float y = p.y - shift;
        return {p.x - shift,
                origin == PixelOrigin::TopLeft ? y : static_cast<float>(resolution.height) - y};
    }

    /// Converts a position in sensor coordinates to this convention.
    [[nodiscard]] constexpr Pixel from_sensor(const Pixel& s, const Resolution& resolution) const
    {
        const float shift = first_pixel_center() - 0.5f;
        const float y =
            origin == PixelOrigin::TopLeft ? s.y : static_cast<float>(resolution.height) - s.y;
        return {s.x + shift, y + shift};
    }

    friend constexpr bool operator==(const PixelConvention&, const PixelConvention&) = default;

    [[nodiscard]] std::string to_string() const
    {
        const char* center_name = center == PixelCenter::Integer       ? "Integer"
                                  : center == PixelCenter::HalfInteger ? "HalfInteger"
                                                                       : "OneBased";
        const char* origin_name = origin == PixelOrigin::TopLeft ? "TopLeft" : "BottomLeft";
        return std::string("PixelConvention(center=") + center_name + ", origin=" + origin_name +
               ")";
    }
};

} // namespace huira
