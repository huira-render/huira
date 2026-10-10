#pragma once

#include <array>
#include <cstddef>
#include <memory>
#include <vector>

#include "huira/cameras/psfs/psf_tables.hpp"
#include "huira/core/types.hpp"
#include "huira/images/fft_convolver.hpp"
#include "huira/images/image.hpp"
#include "huira/util/logger.hpp"

namespace huira {

/**
 * @brief Convolves a frame with the PSF its tables give, across the whole frame: each pixel's
 * light is spread as a still source's at its center would be (PsfTables::pixel()), out to the
 * frame's far edges.
 *
 * The PSF is split in two. The smooth part is its far field (PsfTables::far_pixel()), with a
 * window that rises smoothly from 0 to 1 between half of near_radius() and near_radius(); the
 * near part is the rest, which is nothing beyond near_radius(), where pixel() is the far field.
 * The near part is convolved through FFTs, in double precision (see FftConvolver), so that the
 * rounding of a bright body's light stays below the light the PSF puts anywhere in the frame.
 *
 * The smooth part changes little over many pixels, and is convolved on a coarse grid, a node
 * every step() pixels: each pixel's light is shared among the six nodes on either axis around it
 * by Lagrange interpolation, the nodes' light is convolved with the smooth part at their offsets
 * (also through FFTs), and the result is interpolated back to the pixels the same way. That
 * leaves the interpolation error, of order (step / r)^6 at a distance r, and larger where the
 * window rises: with twenty steps to where it begins, it measures under 1e-4 of the light there,
 * and less beyond. The tables themselves are good to about 1e-3.
 *
 * @tparam TSpectral The spectral type (e.g., @ref RGB, @ref Visible8)
 */
template <IsSpectral TSpectral>
class PsfConvolver {
  public:
    void set_tables(std::shared_ptr<const PsfTables<TSpectral>> tables, Resolution resolution);
    void apply(Image<TSpectral>& image) const;

    [[nodiscard]] bool is_ready() const { return tables_ != nullptr; }

    /// The tables the PSF comes from.
    [[nodiscard]] const std::shared_ptr<const PsfTables<TSpectral>>& tables() const
    {
        return tables_;
    }

    /// The resolution of the frames it convolves.
    [[nodiscard]] Resolution resolution() const { return Resolution{width_, height_}; }

    /// How far the near part reaches, in pixel widths.
    [[nodiscard]] double near_radius() const { return near_radius_; }

    /// The coarse grid's spacing, in pixels, along x and y.
    [[nodiscard]] std::array<int, 2> step() const { return step_; }

  private:
    /// Nodes in a pixel's stencil along each axis, and how many of them come before its cell.
    static constexpr int ORDER = 6;
    static constexpr int BEFORE = 2;
    /// The fewest coarse steps across where the window begins.
    static constexpr double STEPS_TO_WINDOW = 20.0;
    /// The least reach of the near part, in pixel widths. A narrower window would need a grid
    /// finer than the pixels to stay smooth on it.
    static constexpr double MIN_NEAR_RADIUS = 128.0;

    std::shared_ptr<const PsfTables<TSpectral>> tables_;
    int width_ = 0;
    int height_ = 0;
    double near_radius_ = 0.0;
    FftConvolver<TSpectral> near_;

    /// Whether the smooth part reaches into the frame, and so needs the coarse grid.
    bool has_far_ = false;
    std::array<int, 2> step_{1, 1};
    std::array<int, 2> nodes_{0, 0};
    /// Each pixel's weights for the nodes of its stencil, by its offset within its cell.
    std::array<std::vector<std::array<double, ORDER>>, 2> weights_;
    FftConvolver<TSpectral> far_;
};

} // namespace huira

#include "huira_impl/cameras/psfs/psf_convolver.ipp"
