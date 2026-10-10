#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <memory>
#include <utility>
#include <vector>

#include "tbb/blocked_range.h"
#include "tbb/parallel_for.h"

namespace huira {

namespace psf_convolver_detail {

/// Rises from 0 at t <= 0 to 1 at t >= 1 with every derivative continuous, so that the smooth
/// part it windows stays smooth for the coarse grid's interpolation.
inline double window(double t)
{
    if (t <= 0.0) {
        return 0.0;
    }
    if (t >= 1.0) {
        return 1.0;
    }
    const double rise = std::exp(-1.0 / t);
    const double fall = std::exp(-1.0 / (1.0 - t));
    return rise / (rise + fall);
}

} // namespace psf_convolver_detail

/**
 * @brief Prepares to convolve frames of a resolution with the PSF the tables give: the near
 * part's kernel and the smooth part's at the coarse grid's offsets, transformed.
 *
 * The near part reaches one pixel beyond PsfTables::far_from(), and at least MIN_NEAR_RADIUS,
 * or across the frame if that is less; the window rises over its outer half; and the coarse grid
 * has STEPS_TO_WINDOW steps to where the window begins. Where the window begins beyond the
 * frame, there is no coarse grid.
 */
template <IsSpectral TSpectral>
void PsfConvolver<TSpectral>::set_tables(std::shared_ptr<const PsfTables<TSpectral>> tables,
                                         Resolution resolution)
{
    constexpr std::size_t N = TSpectral::size();
    tables_ = std::move(tables);
    width_ = resolution.width;
    height_ = resolution.height;
    const PsfTables<TSpectral>& psf = *tables_;
    const double aspect = psf.aspect();

    near_radius_ = std::max(std::ceil(psf.far_from()) + 1.0, MIN_NEAR_RADIUS);
    const double window_to = near_radius_ - 1.0;
    const double window_from = 0.5 * window_to;
    // The smooth part at an offset in pixels:
    const auto smooth = [&](double dx, double dy) {
        const double r = std::sqrt(dx * dx + aspect * aspect * dy * dy);
        std::array<double, N> values{};
        const double share =
            psf_convolver_detail::window((r - window_from) / (window_to - window_from));
        if (share > 0.0) {
            values = psf.far_pixel(dx, dy);
            for (double& value : values) {
                value *= share;
            }
        }
        return values;
    };

    // The near part: the tables less the smooth part, out to near_radius_, or across the frame
    // if that is less.
    const int near_x = std::min(static_cast<int>(near_radius_), width_ - 1);
    const int near_y = std::min(static_cast<int>(std::ceil(near_radius_ / aspect)), height_ - 1);
    Image<TSpectral> near_kernel(2 * near_x + 1, 2 * near_y + 1, TSpectral{0.f});
    tbb::parallel_for(0, 2 * near_y + 1, [&](int y) {
        const double dy = static_cast<double>(y - near_y);
        for (int x = 0; x < 2 * near_x + 1; ++x) {
            const double dx = static_cast<double>(x - near_x);
            const TSpectral exact = psf.pixel(dx, dy);
            const std::array<double, N> part = smooth(dx, dy);
            TSpectral value{0.f};
            for (std::size_t c = 0; c < N; ++c) {
                value[c] = static_cast<float>(static_cast<double>(exact[c]) - part[c]);
            }
            near_kernel(x, y) = value;
        }
    });
    near_.set_kernel(near_kernel, resolution);

    // Where the window begins beyond the frame's diagonal, the near part is all of it:
    const double diagonal =
        std::hypot(static_cast<double>(width_ - 1), aspect * static_cast<double>(height_ - 1));
    has_far_ = window_from < diagonal;
    if (!has_far_) {
        far_ = FftConvolver<TSpectral>{};
        return;
    }

    // The coarse grid, with each pixel's weights for the ORDER nodes around it: node k is at
    // pixel k step, and a pixel at i = k0 step + rho takes nodes k0 - BEFORE to
    // k0 - BEFORE + ORDER - 1, held from BEFORE nodes before pixel 0.
    step_[0] = std::max(1, static_cast<int>(std::floor(window_from / STEPS_TO_WINDOW)));
    step_[1] = std::max(1, static_cast<int>(std::floor(window_from / (STEPS_TO_WINDOW * aspect))));
    const std::array<int, 2> pixels{width_, height_};
    for (std::size_t axis = 0; axis < 2; ++axis) {
        const int step = step_[axis];
        nodes_[axis] = (pixels[axis] - 1) / step + ORDER;
        weights_[axis].assign(static_cast<std::size_t>(step), {});
        for (int rho = 0; rho < step; ++rho) {
            const double t = static_cast<double>(rho) / static_cast<double>(step);
            for (int m = 0; m < ORDER; ++m) {
                double weight = 1.0;
                for (int n = 0; n < ORDER; ++n) {
                    if (n != m) {
                        weight *=
                            (t - static_cast<double>(n - BEFORE)) / static_cast<double>(m - n);
                    }
                }
                weights_[axis][static_cast<std::size_t>(rho)][static_cast<std::size_t>(m)] = weight;
            }
        }
    }

    // The smooth part at every offset between nodes:
    const int nx = nodes_[0];
    const int ny = nodes_[1];
    Image<TSpectral> far_kernel(2 * nx - 1, 2 * ny - 1, TSpectral{0.f});
    tbb::parallel_for(0, 2 * ny - 1, [&](int y) {
        const double dy = static_cast<double>((y - (ny - 1)) * step_[1]);
        for (int x = 0; x < 2 * nx - 1; ++x) {
            const double dx = static_cast<double>((x - (nx - 1)) * step_[0]);
            const std::array<double, N> part = smooth(dx, dy);
            TSpectral value{0.f};
            for (std::size_t c = 0; c < N; ++c) {
                value[c] = static_cast<float>(part[c]);
            }
            far_kernel(x, y) = value;
        }
    });
    far_.set_kernel(far_kernel, Resolution{nx, ny});
}

/**
 * @brief Convolves a frame in place: the near part through its FFTs, and the smooth part on the
 * coarse grid, added. The interpolation runs in double precision, one axis at a time.
 */
template <IsSpectral TSpectral>
void PsfConvolver<TSpectral>::apply(Image<TSpectral>& image) const
{
    constexpr std::size_t N = TSpectral::size();
    using Channels = std::array<double, N>;
    if (!is_ready()) {
        HUIRA_THROW_ERROR("PsfConvolver::apply - set_tables() has not been called");
    }
    if (image.width() != width_ || image.height() != height_) {
        HUIRA_THROW_ERROR("PsfConvolver::apply - Image resolution does not match the "
                          "resolution given to set_tables()");
    }
    if (!has_far_) {
        near_.apply(image);
        return;
    }
    const int nx = nodes_[0];
    const int ny = nodes_[1];
    const auto node_of = [](int i, int step) { return i / step; };
    const auto residue_of = [](int i, int step) { return static_cast<std::size_t>(i % step); };

    // Each row's light shared among the nodes along x:
    std::vector<Channels> rows(static_cast<std::size_t>(height_) * static_cast<std::size_t>(nx));
    std::vector<char> lit(static_cast<std::size_t>(height_), 0);
    tbb::parallel_for(0, height_, [&](int y) {
        Channels* row = &rows[static_cast<std::size_t>(y) * static_cast<std::size_t>(nx)];
        for (int x = 0; x < width_; ++x) {
            const TSpectral& value = image(x, y);
            if (value.max() == 0.f && value.min() == 0.f) {
                continue;
            }
            lit[static_cast<std::size_t>(y)] = 1;
            const auto& weights = weights_[0][residue_of(x, step_[0])];
            Channels* first = row + node_of(x, step_[0]);
            for (int m = 0; m < ORDER; ++m) {
                for (std::size_t c = 0; c < N; ++c) {
                    first[m][c] +=
                        weights[static_cast<std::size_t>(m)] * static_cast<double>(value[c]);
                }
            }
        }
    });

    // ...then among the nodes along y, a column of nodes at a time:
    Image<TSpectral> nodes(nx, ny, TSpectral{0.f});
    bool any = false;
    for (const char row_lit : lit) {
        any = any || row_lit != 0;
    }
    if (any) {
        tbb::parallel_for(0, nx, [&](int kx) {
            std::vector<Channels> column(static_cast<std::size_t>(ny));
            for (int y = 0; y < height_; ++y) {
                if (lit[static_cast<std::size_t>(y)] == 0) {
                    continue;
                }
                const Channels& value =
                    rows[static_cast<std::size_t>(y) * static_cast<std::size_t>(nx) +
                         static_cast<std::size_t>(kx)];
                const auto& weights = weights_[1][residue_of(y, step_[1])];
                const int first = node_of(y, step_[1]);
                for (int m = 0; m < ORDER; ++m) {
                    for (std::size_t c = 0; c < N; ++c) {
                        column[static_cast<std::size_t>(first + m)][c] +=
                            weights[static_cast<std::size_t>(m)] * value[c];
                    }
                }
            }
            for (int ky = 0; ky < ny; ++ky) {
                TSpectral value{0.f};
                for (std::size_t c = 0; c < N; ++c) {
                    value[c] = static_cast<float>(column[static_cast<std::size_t>(ky)][c]);
                }
                nodes(kx, ky) = value;
            }
        });
        far_.apply(nodes);
    }

    near_.apply(image);
    if (!any) {
        return;
    }

    // The smooth part's convolution, interpolated from the nodes back to the pixels: along y
    // into each row's nodes, then along x.
    std::fill(rows.begin(), rows.end(), Channels{});
    tbb::parallel_for(0, height_, [&](int y) {
        Channels* row = &rows[static_cast<std::size_t>(y) * static_cast<std::size_t>(nx)];
        const auto& weights = weights_[1][residue_of(y, step_[1])];
        const int first = node_of(y, step_[1]);
        for (int m = 0; m < ORDER; ++m) {
            const double weight = weights[static_cast<std::size_t>(m)];
            for (int kx = 0; kx < nx; ++kx) {
                const TSpectral& node = nodes(kx, first + m);
                for (std::size_t c = 0; c < N; ++c) {
                    row[kx][c] += weight * static_cast<double>(node[c]);
                }
            }
        }
        for (int x = 0; x < width_; ++x) {
            const auto& x_weights = weights_[0][residue_of(x, step_[0])];
            const Channels* node = row + node_of(x, step_[0]);
            Channels sum{};
            for (int m = 0; m < ORDER; ++m) {
                for (std::size_t c = 0; c < N; ++c) {
                    sum[c] += x_weights[static_cast<std::size_t>(m)] * node[m][c];
                }
            }
            TSpectral& pixel = image(x, y);
            for (std::size_t c = 0; c < N; ++c) {
                pixel[c] = static_cast<float>(static_cast<double>(pixel[c]) + sum[c]);
            }
        }
    });
}

} // namespace huira
