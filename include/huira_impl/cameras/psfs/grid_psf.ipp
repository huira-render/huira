#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <limits>
#include <memory>
#include <string>
#include <vector>

#include "huira/cameras/psfs/measured_psf.hpp"
#include "huira/cameras/psfs/psf_tables.hpp"
#include "huira/core/constants.hpp"
#include "huira/images/fft_convolver.hpp"
#include "huira/util/logger.hpp"
#include "tbb/parallel_for.h"

namespace huira {

namespace grid_psf_detail {

/// The cubic B-spline, which is nonzero on (-2, 2) and sums to 1 over shifts by whole numbers.
inline double spline(double t)
{
    t = std::abs(t);
    if (t >= 2.0) {
        return 0.0;
    }
    if (t >= 1.0) {
        const double u = 2.0 - t;
        return u * u * u / 6.0;
    }
    return (4.0 - 6.0 * t * t + 3.0 * t * t * t) / 6.0;
}

/// The cubic B-spline's integral from -2 to t: 0 below -2 and 1 above 2.
inline double spline_integral(double t)
{
    if (t <= -2.0) {
        return 0.0;
    }
    if (t >= 2.0) {
        return 1.0;
    }
    if (t > 0.0) {
        return 1.0 - spline_integral(-t);
    }
    if (t <= -1.0) {
        const double u = t + 2.0;
        return u * u * u * u / 24.0;
    }
    // From -1, where it is 1/24, to t in (-1, 0]:
    const double t2 = t * t;
    return 0.5 + (4.0 * t - 2.0 * t2 * t - 0.75 * t2 * t2) / 6.0;
}

} // namespace grid_psf_detail

/**
 * @brief Takes a PSF's samples: a measurement's own, within its radius of the center, or the
 * PSF's values every SAMPLE_STEP pixels out to its radius; then the coefficients of the cubic
 * B-spline through them, mirrored about the first and last, with MARGIN more on every side.
 *
 * @param psf The PSF, with its radius (PSF::radius()) set.
 * @param aspect The pixel's height over its width.
 * @throws std::runtime_error if the aspect is not positive and finite, the PSF has no radius, or
 *         its samples within the radius are fewer than three along an axis.
 */
template <IsSpectral TSpectral>
GridPsf<TSpectral>::GridPsf(PSF<TSpectral>& psf, double aspect)
    : aspect_{aspect}, pixel_integrated_{psf.is_pixel_integrated()}
{
    if (!(aspect > 0.0) || !std::isfinite(aspect)) {
        HUIRA_THROW_ERROR("GridPsf - The aspect must be positive and finite: " +
                          std::to_string(aspect));
    }
    const int radius = psf.radius();
    if (radius < 1) {
        HUIRA_THROW_ERROR("GridPsf - The PSF needs a radius of at least 1 pixel: " +
                          std::to_string(radius));
    }
    // Out to the edge of the pixels the PSF's radius takes in, from the center:
    const double half = static_cast<double>(radius) + 0.5;

    // The samples, channel by channel at each, count_x by count_y from (left_, top_):
    int count_x = 0;
    int count_y = 0;
    std::vector<double> samples;
    const auto sample_at = [&](int i, int j) -> double* {
        return &samples[(static_cast<std::size_t>(j) * static_cast<std::size_t>(count_x) +
                         static_cast<std::size_t>(i)) *
                        N];
    };
    if (const auto* measured = dynamic_cast<const MeasuredPSF<TSpectral>*>(&psf)) {
        const Image<TSpectral>& data = measured->data();
        const double per = static_cast<double>(measured->samples_per_pixel());
        step_ = 1.0 / per;
        const double cx = 0.5 * static_cast<double>(data.width() - 1);
        const double cy = 0.5 * static_cast<double>(data.height() - 1);
        const double reach = half * per + 1e-9;
        const int i_lo = std::max(0, static_cast<int>(std::ceil(cx - reach)));
        const int i_hi = std::min(data.width() - 1, static_cast<int>(std::floor(cx + reach)));
        const int j_lo = std::max(0, static_cast<int>(std::ceil(cy - reach)));
        const int j_hi = std::min(data.height() - 1, static_cast<int>(std::floor(cy + reach)));
        count_x = i_hi - i_lo + 1;
        count_y = j_hi - j_lo + 1;
        left_ = (static_cast<double>(i_lo) - cx) * step_;
        top_ = (static_cast<double>(j_lo) - cy) * step_;
        samples.assign(static_cast<std::size_t>(count_x) * static_cast<std::size_t>(count_y) * N,
                       0.0);
        for (int j = 0; j < count_y; ++j) {
            for (int i = 0; i < count_x; ++i) {
                const TSpectral& value = data(i_lo + i, j_lo + j);
                double* out = sample_at(i, j);
                for (std::size_t c = 0; c < N; ++c) {
                    out[c] = static_cast<double>(value[c]);
                }
            }
        }
    } else {
        // As finely as SAMPLE_STEP allows within the budget, each sample taking a value per
        // channel, and for point samples three running sums too:
        const double bytes = static_cast<double>(N * sizeof(double) * (pixel_integrated_ ? 2 : 5));
        const double most = std::floor(std::sqrt(static_cast<double>(MEMORY_BUDGET) / bytes));
        const double intervals = std::min(std::ceil(2.0 * half / SAMPLE_STEP - 1e-9),
                                          std::max(most - 1.0 - 2.0 * MARGIN, 4.0));
        step_ = 2.0 * half / intervals;
        if (step_ > SAMPLE_STEP * (1.0 + 1e-9)) {
            HUIRA_LOG_INFO("GridPsf - The PSF is sampled every " + std::to_string(step_) +
                           " pixels rather than " + std::to_string(SAMPLE_STEP) +
                           ", to keep it within " + std::to_string(MEMORY_BUDGET >> 20) + " MiB");
        }
        count_x = static_cast<int>(intervals) + 1;
        count_y = count_x;
        left_ = -half;
        top_ = -half;
        samples.assign(static_cast<std::size_t>(count_x) * static_cast<std::size_t>(count_y) * N,
                       0.0);
        tbb::parallel_for(0, count_y, [&](int j) {
            const double y = top_ + static_cast<double>(j) * step_;
            for (int i = 0; i < count_x; ++i) {
                const double x = left_ + static_cast<double>(i) * step_;
                const TSpectral value = psf.evaluate(static_cast<float>(x), static_cast<float>(y));
                double* out = sample_at(i, j);
                for (std::size_t c = 0; c < N; ++c) {
                    out[c] = static_cast<double>(value[c]);
                }
            }
        });
    }
    if (count_x < 3 || count_y < 3) {
        HUIRA_THROW_ERROR("GridPsf - The PSF needs at least three samples across within its "
                          "radius");
    }
    build_from_samples_(samples, count_x, count_y);
}

/**
 * @brief The PSF blurred by defocus, a uniform disc of radius blur pixel widths: the image of
 * the aperture out of focus, as PsfTables::airy() blurs diffraction.
 *
 * The blurred PSF is held as what a pixel centered at each sample collects (as pixel-integrated
 * samples are), which a pixel's width smooths, so that a large blur's sharp edge, where the disc
 * ends, is still resolved when the samples are spread out. Point samples are first integrated
 * over a pixel at each (see integrated_()). Those sum, over the pixels, to all of the light,
 * wherever the source is, so scale() is 1. The blurred spline is the focused one's coefficients
 * convolved with the disc's integral over each basis function: across each chord of the disc
 * exactly (spline_integral()), and along the disc by four-point Gauss-Legendre quadrature on each
 * sample's height, in the angle whose sine is the height over the radius, which takes away the
 * chord's square-root ends. The disc is round in pixel widths, so blur / aspect() pixels high. Its
 * values at the new samples, which reach a disc's radius and two samples beyond the old ones on
 * every side, are then fitted with a spline as before. The coefficients taken are the samples' own:
 * so within two samples of the edge of a PSF that does not fall to zero there, the PSF blurred
 * falls to zero over those two samples rather than at the edge.
 *
 * The new samples are every few of the old, if all of them would take more than MEMORY_BUDGET:
 * a large blur spreads the light over many samples. Only those are computed: the convolution is
 * split by where each coefficient falls between them, into one convolution per offset, each of
 * every few coefficients, whose spectra are added (a polyphase decomposition). So the work and
 * memory go with the samples kept, and the convolution is exact whatever the spacing.
 *
 * @param in_focus The PSF in focus.
 * @param blur The disc's radius, in pixel widths; positive.
 * @throws std::runtime_error if the blur is not positive and finite.
 */
template <IsSpectral TSpectral>
GridPsf<TSpectral>::GridPsf(const GridPsf& in_focus, double blur)
    : aspect_{in_focus.aspect_}, pixel_integrated_{true}, sums_to_one_{true}, step_{in_focus.step_}
{
    using grid_psf_detail::spline;
    using grid_psf_detail::spline_integral;
    if (!(blur > 0.0) || !std::isfinite(blur)) {
        HUIRA_THROW_ERROR("GridPsf - The blur must be positive and finite: " +
                          std::to_string(blur));
    }
    const GridPsf& focused = in_focus.collected_();
    const double h = focused.step_;
    const double tall = blur / aspect_; // the disc's half height, in pixels
    const double area = PI<double>() * blur * tall;
    // In samples, how far the disc reaches beyond a basis function, which itself reaches two:
    const int reach = static_cast<int>(std::ceil(std::max(blur, tall) / h)) + 2;
    if (reach > (1 << 20)) {
        HUIRA_THROW_ERROR("GridPsf - The blur is too large for the PSF's samples: " +
                          std::to_string(blur) + " px");
    }

    // The samples' own coefficients, and the blurred spline's extent in old samples:
    const int inner_x = focused.nx_ - 2 * MARGIN;
    const int inner_y = focused.ny_ - 2 * MARGIN;
    const int width = inner_x + 2 * reach;
    const int height = inner_y + 2 * reach;

    // Every few, if all would take more than the budget, out to the extent's far edge:
    const double bytes = static_cast<double>(N * sizeof(double) * 2);
    const double nodes =
        static_cast<double>(width + 2 * MARGIN) * static_cast<double>(height + 2 * MARGIN);
    const int every = std::max(
        1,
        static_cast<int>(std::ceil(std::sqrt(nodes * bytes / static_cast<double>(MEMORY_BUDGET)))));
    const int count_x = (width - 1 + every - 1) / every + 1;
    const int count_y = (height - 1 + every - 1) / every + 1;
    if (every > 1) {
        HUIRA_LOG_INFO("GridPsf - The PSF blurred by " + std::to_string(blur) +
                       " px is sampled every " + std::to_string(h * every) +
                       " pixels, to keep it within " + std::to_string(MEMORY_BUDGET >> 20) +
                       " MiB");
    }

    // The disc's integral over the basis function at an offset of (dx, dy) samples from its
    // center, over its area: the light a coefficient of 1 there gives the blurred spline at the
    // center. For each row, the quadrature's nodes along the disc, as the weight each takes and
    // the chord's half length, in samples:
    struct Node {
        double weight = 0.0;
        double chord = 0.0;
    };
    using Row = std::array<Node, 16>;
    constexpr std::array<double, 4> GL_NODES{
        -0.8611363115940526, -0.3399810435848563, 0.3399810435848563, 0.8611363115940526};
    constexpr std::array<double, 4> GL_WEIGHTS{
        0.3478548451374538, 0.6521451548625461, 0.6521451548625461, 0.3478548451374538};
    const auto row_nodes = [&](int dy, Row& row) {
        std::size_t count = 0;
        // The basis is nonzero for heights within two samples of dy h, a sample at a time:
        for (int k = -2; k < 2; ++k) {
            const double y0 = std::max(-tall, h * static_cast<double>(dy + k));
            const double y1 = std::min(tall, h * static_cast<double>(dy + k + 1));
            if (!(y1 > y0)) {
                continue;
            }
            const double t0 = std::asin(std::clamp(y0 / tall, -1.0, 1.0));
            const double t1 = std::asin(std::clamp(y1 / tall, -1.0, 1.0));
            const double mid = 0.5 * (t0 + t1);
            const double half = 0.5 * (t1 - t0);
            for (std::size_t n = 0; n < GL_NODES.size(); ++n) {
                const double theta = mid + half * GL_NODES[n];
                const double cosine = std::cos(theta);
                const double y = tall * std::sin(theta);
                row[count++] = Node{GL_WEIGHTS[n] * half * tall * cosine *
                                        spline(static_cast<double>(dy) - y / h) * h / area,
                                    blur * cosine / h};
            }
        }
        return count;
    };
    const auto disc_value = [&](const Row& row, std::size_t count, int dx) {
        double sum = 0.0;
        for (std::size_t n = 0; n < count; ++n) {
            sum += row[n].weight * (spline_integral(static_cast<double>(dx) + row[n].chord) -
                                    spline_integral(static_cast<double>(dx) - row[n].chord));
        }
        return sum;
    };

    // The new sample i, at old sample every i of the extent, takes coefficient m (at old sample
    // m + reach) through the disc at offset every i - reach - m. With m = every q + r, that is
    // every (i - q) - reach - r: for each r, a convolution of the coefficients every q + r with
    // the disc's values every p - reach - r, nonzero for p from 0 to most. Each is drawn
    // centered on p = middle, and its coefficients shifted by as much, so that the output's
    // sample i is the convolution's at i + half.
    const int most = (2 * reach + every - 1) / every;
    const int middle = most / 2;
    const int half_side = most - middle;
    const int side = 2 * half_side + 1;
    const int image_x = std::max(count_x, (inner_x + every - 1) / every + middle);
    const int image_y = std::max(count_y, (inner_y + every - 1) / every + middle);
    const int padded_x = FftConvolver<TSpectral>::next_fast_size(image_x + side - 1);
    const int padded_y = FftConvolver<TSpectral>::next_fast_size(image_y + side - 1);
    const auto plans = detail::FftwPlanCache::instance().get(padded_y, padded_x, false);
    const auto complex_columns = static_cast<std::size_t>(padded_x / 2 + 1);
    const std::size_t complex_size = complex_columns * static_cast<std::size_t>(padded_y);
    const std::size_t stride = 2 * complex_columns; // reals per row, transformed in place

    // Buffers aligned as the plans were made: the disc's offsets, and per channel the
    // coefficients' and the sum of the products' spectra.
    struct Buffer {
        fftw_complex* data = nullptr;
        explicit Buffer(std::size_t size) : data{fftw_alloc_complex(size)}
        {
            if (data == nullptr) {
                HUIRA_THROW_ERROR("GridPsf - Failed to allocate the blur's buffers");
            }
        }
        Buffer(const Buffer&) = delete;
        Buffer& operator=(const Buffer&) = delete;
        ~Buffer() { fftw_free(data); }
        double* real() const { return detail::in_place_real(data); }
    };
    Buffer disc(complex_size);
    std::vector<std::unique_ptr<Buffer>> spectra;
    std::vector<std::unique_ptr<Buffer>> scratch;
    for (std::size_t c = 0; c < N; ++c) {
        spectra.push_back(std::make_unique<Buffer>(complex_size));
        std::fill(spectra.back()->real(), spectra.back()->real() + 2 * complex_size, 0.0);
        scratch.push_back(std::make_unique<Buffer>(complex_size));
    }

    for (int ry = 0; ry < every; ++ry) {
        for (int rx = 0; rx < every; ++rx) {
            double* kernel = disc.real();
            std::fill(kernel, kernel + 2 * complex_size, 0.0);
            tbb::parallel_for(0, side, [&](int ky) {
                const int dy = every * (ky - half_side + middle) - reach - ry;
                if (std::abs(dy) > reach) {
                    return;
                }
                Row row;
                const std::size_t count = row_nodes(dy, row);
                for (int kx = 0; kx < side; ++kx) {
                    const int dx = every * (kx - half_side + middle) - reach - rx;
                    if (std::abs(dx) <= reach) {
                        kernel[static_cast<std::size_t>(ky) * stride +
                               static_cast<std::size_t>(kx)] = disc_value(row, count, dx);
                    }
                }
            });
            fftw_execute_dft_r2c(plans.forward, kernel, disc.data);

            tbb::parallel_for(std::size_t{0}, N, [&](std::size_t c) {
                double* values = scratch[c]->real();
                std::fill(values, values + 2 * complex_size, 0.0);
                for (int j = ry, q_y = 0; j < inner_y; j += every, ++q_y) {
                    double* out = values + static_cast<std::size_t>(q_y + middle) * stride +
                                  static_cast<std::size_t>(middle);
                    for (int i = rx, q_x = 0; i < inner_x; i += every, ++q_x) {
                        out[q_x] = focused.coefficients_[focused.at_(i + MARGIN, j + MARGIN) + c];
                    }
                }
                fftw_execute_dft_r2c(plans.forward, values, scratch[c]->data);
                fftw_complex* sum = spectra[c]->data;
                const fftw_complex* a = scratch[c]->data;
                const fftw_complex* b = disc.data;
                for (std::size_t k = 0; k < complex_size; ++k) {
                    sum[k][0] += a[k][0] * b[k][0] - a[k][1] * b[k][1];
                    sum[k][1] += a[k][0] * b[k][1] + a[k][1] * b[k][0];
                }
            });
        }
    }

    std::vector<double> samples(static_cast<std::size_t>(count_x) *
                                static_cast<std::size_t>(count_y) * N);
    const double normalization = 1.0 / (static_cast<double>(padded_x) * padded_y);
    tbb::parallel_for(std::size_t{0}, N, [&](std::size_t c) {
        fftw_execute_dft_c2r(plans.inverse, spectra[c]->data, spectra[c]->real());
        const double* values = spectra[c]->real();
        for (int j = 0; j < count_y; ++j) {
            for (int i = 0; i < count_x; ++i) {
                samples[(static_cast<std::size_t>(j) * static_cast<std::size_t>(count_x) +
                         static_cast<std::size_t>(i)) *
                            N +
                        c] = values[static_cast<std::size_t>(j + half_side) * stride +
                                    static_cast<std::size_t>(i + half_side)] *
                             normalization;
            }
        }
    });

    left_ = focused.left_ - static_cast<double>(reach) * h;
    top_ = focused.top_ - static_cast<double>(reach) * h;
    step_ = h * static_cast<double>(every);
    build_from_samples_(samples, count_x, count_y);
}

/**
 * @brief A point-sampled PSF integrated over a pixel centered at each of a grid of points, as
 * pixel-integrated samples: exactly (see rectangle_light_()), out to where a pixel no longer meets
 * the PSF's rectangle.
 *
 * Where the PSF is cut off at its rectangle, without having fallen to zero, the integral falls to
 * zero across a pixel's width, with a kink at each end, which a spline through samples rounds
 * with a dip below zero, of a few hundredths of the drop for samples a quarter of a pixel apart.
 * So the points are every SAMPLE_STEP pixels, or as finely as MEMORY_BUDGET allows if coarser,
 * whatever the PSF's own spacing, which takes the dip to a few thousandths of the drop.
 */
template <IsSpectral TSpectral>
GridPsf<TSpectral> GridPsf<TSpectral>::integrated_(const GridPsf& point, double finest)
{
    GridPsf result;
    result.aspect_ = point.aspect_;
    result.pixel_integrated_ = true;
    // The rectangle the integrals reach, half a pixel beyond the PSF's on every side:
    const double width = point.right_ - point.left_ + 1.0;
    const double height = point.bottom_ - point.top_ + 1.0;
    const double bytes = static_cast<double>(N * sizeof(double) * 2);
    const double most = std::sqrt(static_cast<double>(MEMORY_BUDGET) / bytes) - 2.0 * MARGIN - 1.0;
    const double step =
        std::min(point.step_, std::max(finest, std::max(width, height) / std::max(most, 4.0)));
    const int count_x = static_cast<int>(std::ceil(width / step - 1e-9)) + 1;
    const int count_y = static_cast<int>(std::ceil(height / step - 1e-9)) + 1;
    result.step_ = step;
    result.left_ = point.left_ - 0.5;
    result.top_ = point.top_ - 0.5;
    std::vector<double> samples(static_cast<std::size_t>(count_x) *
                                static_cast<std::size_t>(count_y) * N);
    tbb::parallel_for(0, count_y, [&](int j) {
        const double y = result.top_ + static_cast<double>(j) * step;
        for (int i = 0; i < count_x; ++i) {
            const double x = result.left_ + static_cast<double>(i) * step;
            const Light light = point.rectangle_light_(x - 0.5, x + 0.5, y - 0.5, y + 0.5);
            std::copy(light.begin(),
                      light.end(),
                      &samples[(static_cast<std::size_t>(j) * static_cast<std::size_t>(count_x) +
                                static_cast<std::size_t>(i)) *
                               N]);
        }
    });
    result.build_from_samples_(samples, count_x, count_y);
    return result;
}

/**
 * @brief The spline through samples count_x by count_y, every step_ from (left_, top_),
 * channel by channel at each: the coefficients, with the samples mirrored about the first and
 * last, MARGIN more of them on every side, normalized, and the bounds.
 */
template <IsSpectral TSpectral>
void GridPsf<TSpectral>::build_from_samples_(std::vector<double>& samples, int count_x, int count_y)
{
    const auto sample_at = [&](int i, int j) -> double* {
        return &samples[(static_cast<std::size_t>(j) * static_cast<std::size_t>(count_x) +
                         static_cast<std::size_t>(i)) *
                        N];
    };
    right_ = left_ + static_cast<double>(count_x - 1) * step_;
    bottom_ = top_ + static_cast<double>(count_y - 1) * step_;

    // The interpolating spline's coefficients, along the rows and then the columns, with the
    // samples mirrored about the first and last:
    const auto row_count = static_cast<std::size_t>(count_x);
    const auto column_count = static_cast<std::size_t>(count_y);
    tbb::parallel_for(0, count_y, [&](int j) {
        for (std::size_t c = 0; c < N; ++c) {
            psf_tables_detail::bspline_coefficients(sample_at(0, j) + c, row_count, N);
        }
    });
    tbb::parallel_for(0, count_x, [&](int i) {
        for (std::size_t c = 0; c < N; ++c) {
            psf_tables_detail::bspline_coefficients(
                sample_at(i, 0) + c, column_count, row_count * N);
        }
    });

    // With MARGIN more mirrored on every side, which the spline takes near the samples' edges:
    nx_ = count_x + 2 * MARGIN;
    ny_ = count_y + 2 * MARGIN;
    x0_ = left_ - MARGIN * step_;
    y0_ = top_ - MARGIN * step_;
    const auto mirror = [](int k, int count) {
        return k < 0 ? -k : (k >= count ? 2 * (count - 1) - k : k);
    };
    coefficients_.assign(static_cast<std::size_t>(nx_) * static_cast<std::size_t>(ny_) * N, 0.0);
    for (int j = 0; j < ny_; ++j) {
        const int from_j = mirror(j - MARGIN, count_y);
        for (int i = 0; i < nx_; ++i) {
            const double* from = sample_at(mirror(i - MARGIN, count_x), from_j);
            std::copy(from, from + N, &coefficients_[at_(i, j)]);
        }
    }

    // Normalized to the light within the rectangle:
    if (pixel_integrated_) {
        // ...which for samples of what pixels collect is their mean over the source's positions,
        // the samples' sum times a sample's area:
        Light total{};
        for (std::size_t k = 0; k < samples.size(); ++k) {
            total[k % N] += samples[k];
        }
        for (std::size_t k = 0; k < coefficients_.size(); ++k) {
            const double light = total[k % N] * step_ * step_;
            coefficients_[k] = light > 0.0 ? coefficients_[k] / light : 0.0;
        }
    } else {
        build_point_sampled_();
        const Light total = rectangle_light_(left_, right_, top_, bottom_);
        for (auto* values : {&coefficients_, &row_, &column_, &below_}) {
            for (std::size_t k = 0; k < values->size(); ++k) {
                const double light = total[k % N];
                (*values)[k] = light > 0.0 ? (*values)[k] / light : 0.0;
            }
        }
    }
    build_bounds_();
}

/// The running sums of the coefficients that corner_light_() takes.
template <IsSpectral TSpectral>
void GridPsf<TSpectral>::build_point_sampled_()
{
    row_ = coefficients_;
    column_ = coefficients_;
    for (int j = 0; j < ny_; ++j) {
        for (int i = 1; i < nx_; ++i) {
            for (std::size_t c = 0; c < N; ++c) {
                row_[at_(i, j) + c] += row_[at_(i - 1, j) + c];
            }
        }
    }
    for (int j = 1; j < ny_; ++j) {
        for (int i = 0; i < nx_; ++i) {
            for (std::size_t c = 0; c < N; ++c) {
                column_[at_(i, j) + c] += column_[at_(i, j - 1) + c];
            }
        }
    }
    below_ = row_;
    for (int j = 1; j < ny_; ++j) {
        for (int i = 0; i < nx_; ++i) {
            for (std::size_t c = 0; c < N; ++c) {
                below_[at_(i, j) + c] += below_[at_(i, j - 1) + c];
            }
        }
    }
}

/**
 * @brief The bounds the renderer draws within: the largest light a pixel gets at each distance
 * or beyond, the distance holding 90% of the light, and the farthest any pixel gets light.
 *
 * The spline is at most its largest coefficient within two samples, as its basis is positive
 * and sums to one. A point-sampled pixel holds at most the spline's largest value over the
 * pixel (whose area is one), within its half diagonal; a pixel-integrated one at most the
 * spline at its center, times the largest scale().
 */
template <IsSpectral TSpectral>
void GridPsf<TSpectral>::build_bounds_()
{
    const double half_diagonal = 0.5 * std::hypot(1.0, aspect_);
    const double farthest = std::hypot(std::max(std::abs(left_), std::abs(right_)),
                                       aspect_ * std::max(std::abs(top_), std::abs(bottom_)));
    reach_ = farthest + (pixel_integrated_ ? 0.0 : half_diagonal);

    // The largest scale, for pixel-integrated samples: the inverse of the smallest sum over the
    // pixels, over a grid of the source's positions within a pixel, with a margin.
    Light largest_scale;
    largest_scale.fill(1.0);
    if (pixel_integrated_ && !sums_to_one_) {
        constexpr int PHASES = 16;
        Light smallest;
        smallest.fill(std::numeric_limits<double>::infinity());
        for (int b = 0; b < PHASES; ++b) {
            for (int a = 0; a < PHASES; ++a) {
                const Light sum = sum_over_pixels_(static_cast<double>(a) / PHASES,
                                                   static_cast<double>(b) / PHASES);
                for (std::size_t c = 0; c < N; ++c) {
                    smallest[c] = std::min(smallest[c], 0.99 * sum[c]);
                }
            }
        }
        for (std::size_t c = 0; c < N; ++c) {
            largest_scale[c] = smallest[c] > 0.0 ? 1.0 / smallest[c] : 0.0;
        }
    }

    const auto bins = static_cast<std::size_t>(std::floor(farthest * ENVELOPE_BINS)) + 2;
    envelope_.assign(bins, {});
    struct Node {
        double r;
        std::size_t index;
    };
    std::vector<Node> nodes;
    nodes.reserve(static_cast<std::size_t>(nx_) * static_cast<std::size_t>(ny_));
    for (int j = 0; j < ny_; ++j) {
        const double y = aspect_ * (y0_ + static_cast<double>(j) * step_);
        for (int i = 0; i < nx_; ++i) {
            const double x = x0_ + static_cast<double>(i) * step_;
            const double r = std::hypot(x, y);
            const std::size_t index = at_(i, j);
            auto& bin = envelope_[std::min(static_cast<std::size_t>(r * ENVELOPE_BINS), bins - 1)];
            for (std::size_t c = 0; c < N; ++c) {
                bin[c] = std::max(bin[c], std::abs(coefficients_[index + c]) * largest_scale[c]);
            }
            // The coefficients of the samples themselves, for the light within each distance:
            if (i >= MARGIN && i < nx_ - MARGIN && j >= MARGIN && j < ny_ - MARGIN) {
                nodes.push_back({r, index});
            }
        }
    }
    for (std::size_t b = bins - 1; b-- > 0;) {
        for (std::size_t c = 0; c < N; ++c) {
            envelope_[b][c] = std::max(envelope_[b][c], envelope_[b + 1][c]);
        }
    }

    // The light, a coefficient times a sample's area, within each distance:
    std::sort(nodes.begin(), nodes.end(), [](const Node& a, const Node& b) { return a.r < b.r; });
    const double spread = 2.0 * step_ * std::hypot(1.0, aspect_);
    const double margin = pixel_integrated_ ? spread : half_diagonal + spread;
    for (std::size_t c = 0; c < N; ++c) {
        double light = 0.0;
        radius_90_[c] = reach_;
        for (const Node& node : nodes) {
            light += coefficients_[node.index + c] * step_ * step_;
            if (light >= 0.9) {
                radius_90_[c] = std::min(node.r + margin, reach_);
                break;
            }
        }
    }

    // A line's light per unit length in a pixel at least r from it, the envelope integrated along
    // it, from each bin's near edge: the envelope falls with distance, so the sums of its values
    // at the near ends of steps along the line hold it.
    const double along = 1.0 / ENVELOPE_BINS;
    const auto line_bins = static_cast<std::size_t>(std::ceil(reach_ * ENVELOPE_BINS)) + 1;
    line_envelope_.assign(line_bins, {});
    for (std::size_t b = 0; b < line_bins; ++b) {
        const double r = static_cast<double>(b) * along;
        for (std::size_t k = 0; k < line_bins; ++k) {
            const double distance = std::hypot(r, static_cast<double>(k) * along);
            if (distance > reach_) {
                break;
            }
            for (std::size_t c = 0; c < N; ++c) {
                line_envelope_[b][c] += 2.0 * along * envelope(c, distance);
            }
        }
    }
}

/**
 * @brief Where an offset (in pixels, relative to the source) falls among count coefficients
 * from origin along an axis, with the basis's integrals up to it of the four coefficients from
 * the second below it to the second above: those further below hold all of their light within
 * it, and those further above none.
 */
template <IsSpectral TSpectral>
typename GridPsf<TSpectral>::Axis
GridPsf<TSpectral>::integral_axis_(double offset, double origin, int count) const
{
    Axis axis;
    const double u = (offset - origin) / step_;
    if (!(u > -2.0)) {
        axis.before = true;
        return axis;
    }
    const long k0 = std::min(static_cast<long>(std::floor(u)), static_cast<long>(count) + 2);
    axis.first = k0 - 1;
    for (std::size_t a = 0; a < 4; ++a) {
        axis.weights[a] = grid_psf_detail::spline_integral(
            u - static_cast<double>(axis.first + static_cast<long>(a)));
    }
    return axis;
}

/// The same with the basis's values at the offset, for interpolation.
template <IsSpectral TSpectral>
typename GridPsf<TSpectral>::Axis GridPsf<TSpectral>::value_axis_(double offset,
                                                                  double origin) const
{
    Axis axis;
    const double u = (offset - origin) / step_;
    if (!(u > -2.0) || !(u < 1e9)) {
        axis.before = true;
        return axis;
    }
    axis.first = static_cast<long>(std::floor(u)) - 1;
    for (std::size_t a = 0; a < 4; ++a) {
        axis.weights[a] =
            grid_psf_detail::spline(u - static_cast<double>(axis.first + static_cast<long>(a)));
    }
    return axis;
}

/**
 * @brief The light in (-inf, x] x (-inf, y], relative to the source, in pixels.
 *
 * The spline is the sum of its coefficients times shifted basis functions, so its integral over
 * the corner is the sum of the coefficients times the basis's integrals along each axis. Those
 * are 1 for the coefficients two samples or more below (x, y), and 0 two or more above: so the
 * whole is the running sum below and left of a 4 x 4 window around (x, y), the running sums
 * along the window's rows and columns beyond it times one integral, and the window itself.
 */
template <IsSpectral TSpectral>
typename GridPsf<TSpectral>::Light GridPsf<TSpectral>::corner_light_(const Axis& x,
                                                                     const Axis& y) const
{
    Light light{};
    if (x.before || y.before) {
        return light;
    }
    const long nx = nx_;
    const long ny = ny_;
    const auto index = [&](long i, long j) {
        return at_(static_cast<int>(i), static_cast<int>(j));
    };
    // The coefficients below and left of the window, whose basis holds all its light within;
    // each term's weight and place are found once, and its channels, which lie together, added
    // up after:
    const long full_x = x.first - 1;
    const long full_y = y.first - 1;
    const auto add = [&](const std::vector<double>& sums, std::size_t at, double w) {
        const double* values = &sums[at];
        for (std::size_t c = 0; c < N; ++c) {
            light[c] += w * values[c];
        }
    };
    if (full_x >= 0 && full_y >= 0) {
        add(below_, index(std::min(full_x, nx - 1), std::min(full_y, ny - 1)), 1.0);
    }
    for (std::size_t b = 0; b < 4; ++b) {
        const long j = y.first + static_cast<long>(b);
        if (full_x >= 0 && j >= 0 && j < ny) {
            add(row_, index(std::min(full_x, nx - 1), j), y.weights[b]);
        }
    }
    for (std::size_t a = 0; a < 4; ++a) {
        const long i = x.first + static_cast<long>(a);
        if (full_y >= 0 && i >= 0 && i < nx) {
            add(column_, index(i, std::min(full_y, ny - 1)), x.weights[a]);
        }
    }
    const long i_begin = std::max(x.first, 0L);
    const long i_end = std::min(x.first + 4, nx);
    for (std::size_t b = 0; b < 4; ++b) {
        const long j = y.first + static_cast<long>(b);
        if (j < 0 || j >= ny) {
            continue;
        }
        for (long i = i_begin; i < i_end; ++i) {
            add(coefficients_,
                index(i, j),
                x.weights[static_cast<std::size_t>(i - x.first)] * y.weights[b]);
        }
    }
    const double h2 = step_ * step_;
    for (double& value : light) {
        value *= h2;
    }
    return light;
}

/// The spline's light in (-inf, x] x (-inf, y], relative to the source, in pixels.
template <IsSpectral TSpectral>
typename GridPsf<TSpectral>::Light GridPsf<TSpectral>::corner_light_(double x, double y) const
{
    return corner_light_(integral_axis_(x, x0_, nx_), integral_axis_(y, y0_, ny_));
}

/**
 * @brief The PSF's light in [x0, x1] x [y0, y1], relative to the source, in pixels: the spline's
 * within the part of it inside the samples' rectangle, from its four corners.
 */
template <IsSpectral TSpectral>
typename GridPsf<TSpectral>::Light
GridPsf<TSpectral>::rectangle_light_(double x0, double x1, double y0, double y1) const
{
    Light light{};
    x0 = std::max(x0, left_);
    x1 = std::min(x1, right_);
    y0 = std::max(y0, top_);
    y1 = std::min(y1, bottom_);
    if (!(x1 > x0) || !(y1 > y0)) {
        return light;
    }
    const Axis a0 = integral_axis_(x0, x0_, nx_);
    const Axis a1 = integral_axis_(x1, x0_, nx_);
    const Axis b0 = integral_axis_(y0, y0_, ny_);
    const Axis b1 = integral_axis_(y1, y0_, ny_);
    const Light upper = corner_light_(a1, b1);
    const Light left = corner_light_(a0, b1);
    const Light lower = corner_light_(a1, b0);
    const Light corner = corner_light_(a0, b0);
    for (std::size_t c = 0; c < N; ++c) {
        light[c] = upper[c] - left[c] - lower[c] + corner[c];
    }
    return light;
}

/// The spline at the positions the axes give.
template <IsSpectral TSpectral>
typename GridPsf<TSpectral>::Light GridPsf<TSpectral>::interpolate_(const Axis& x,
                                                                    const Axis& y) const
{
    Light value{};
    if (x.before || y.before) {
        return value;
    }
    for (std::size_t b = 0; b < 4; ++b) {
        const long j = y.first + static_cast<long>(b);
        if (j < 0 || j >= ny_ || y.weights[b] == 0.0) {
            continue;
        }
        for (std::size_t a = 0; a < 4; ++a) {
            const long i = x.first + static_cast<long>(a);
            if (i < 0 || i >= nx_) {
                continue;
            }
            const double w = x.weights[a] * y.weights[b];
            const std::size_t index = at_(static_cast<int>(i), static_cast<int>(j));
            for (std::size_t c = 0; c < N; ++c) {
                value[c] += w * coefficients_[index + c];
            }
        }
    }
    return value;
}

/// The PSF at (x, y), relative to the source, in pixels: the spline, within the samples'
/// rectangle, and nothing beyond it.
template <IsSpectral TSpectral>
typename GridPsf<TSpectral>::Light GridPsf<TSpectral>::interpolate_(double x, double y) const
{
    if (!(x >= left_ && x <= right_ && y >= top_ && y <= bottom_)) {
        return Light{};
    }
    return interpolate_(value_axis_(x, x0_), value_axis_(y, y0_));
}

/// The pixel-integrated PSF summed over every pixel whose center is within the samples'
/// rectangle, for a source at (source_x, source_y).
template <IsSpectral TSpectral>
typename GridPsf<TSpectral>::Light GridPsf<TSpectral>::sum_over_pixels_(double source_x,
                                                                        double source_y) const
{
    Light sum{};
    const auto first_x = static_cast<long>(std::ceil(source_x + left_ - 0.5));
    const auto last_x = static_cast<long>(std::floor(source_x + right_ - 0.5));
    const auto first_y = static_cast<long>(std::ceil(source_y + top_ - 0.5));
    const auto last_y = static_cast<long>(std::floor(source_y + bottom_ - 0.5));
    for (long y = first_y; y <= last_y; ++y) {
        for (long x = first_x; x <= last_x; ++x) {
            const Light value = interpolate_(static_cast<double>(x) + 0.5 - source_x,
                                             static_cast<double>(y) + 0.5 - source_y);
            for (std::size_t c = 0; c < N; ++c) {
                sum[c] += value[c];
            }
        }
    }
    return sum;
}

/**
 * @brief The factor a source's pixels take, per channel: 1 for point samples, whose pixels
 * hold all the light wherever the source is; for pixel-integrated samples, the inverse of the
 * PSF's sum over every pixel for the source's position, so that they hold all of it too.
 *
 * @param source_x, source_y The source's position on the sensor; pixel (x, y) covers
 *        [x, x + 1) by [y, y + 1).
 */
template <IsSpectral TSpectral>
typename GridPsf<TSpectral>::Light GridPsf<TSpectral>::scale(double source_x, double source_y) const
{
    Light result;
    result.fill(1.0);
    if (!pixel_integrated_ || sums_to_one_) {
        return result;
    }
    const Light sum = sum_over_pixels_(source_x, source_y);
    for (std::size_t c = 0; c < N; ++c) {
        result[c] = sum[c] > 0.0 ? 1.0 / sum[c] : 0.0;
    }
    return result;
}

/**
 * @brief The light a pixel whose center is (dx, dy) pixels from the source receives, per
 * channel, as a fraction of the source's: the spline integrated over the pixel, or for
 * pixel-integrated samples the spline there times scale (see scale()).
 */
template <IsSpectral TSpectral>
TSpectral GridPsf<TSpectral>::pixel(double dx, double dy, const Light& scale) const
{
    TSpectral result{0.f};
    if (pixel_integrated_) {
        const Light value = interpolate_(dx, dy);
        for (std::size_t c = 0; c < N; ++c) {
            result[c] = static_cast<float>(value[c] * scale[c]);
        }
        return result;
    }
    const Light light = rectangle_light_(dx - 0.5, dx + 0.5, dy - 0.5, dy + 0.5);
    for (std::size_t c = 0; c < N; ++c) {
        result[c] = static_cast<float>(light[c]);
    }
    return result;
}

/**
 * @brief Adds a source's light to the pixels of a block of an image, as pixel(), out to reach
 * pixel widths from the source, times weight(r) at a distance r.
 *
 * The weights along each axis are found once for each column and row of the block; for point
 * samples, the light within each corner once for each pixel corner, which four pixels share.
 *
 * @param target The image; pixel (x, y) covers [x, x + 1) by [y, y + 1).
 * @param x_begin, x_end, y_begin, y_end The block's pixels, which must be in the image.
 * @param source_x, source_y The source's position, in the same coordinates.
 * @param power The source's light, per channel.
 * @param scale The source's scale (see scale()).
 * @param reach How far out it is drawn.
 * @param weight A function of the distance, the share drawn there.
 * @return The light drawn per channel, as a fraction of power.
 */
template <IsSpectral TSpectral>
template <class Weight>
typename GridPsf<TSpectral>::Light GridPsf<TSpectral>::draw(Image<TSpectral>& target,
                                                            int x_begin,
                                                            int x_end,
                                                            int y_begin,
                                                            int y_end,
                                                            double source_x,
                                                            double source_y,
                                                            const TSpectral& power,
                                                            const Light& scale,
                                                            double reach,
                                                            const Weight& weight) const
{
    Light drawn{};
    if (x_end <= x_begin || y_end <= y_begin) {
        return drawn;
    }
    const double limit = std::min(reach, reach_);
    const double limit2 = limit * limit;
    const double aspect2 = aspect_ * aspect_;
    const auto width = static_cast<std::size_t>(x_end - x_begin);

    const auto add = [&](int x, int y, double share, const Light& value) {
        TSpectral& out = target(x, y);
        for (std::size_t c = 0; c < N; ++c) {
            const double light = share * value[c];
            drawn[c] += light;
            out[c] += static_cast<float>(light * static_cast<double>(power[c]));
        }
    };

    if (pixel_integrated_) {
        // The spline at each pixel's center:
        std::vector<Axis> columns(width);
        for (std::size_t i = 0; i < width; ++i) {
            columns[i] = value_axis_(
                static_cast<double>(x_begin) + static_cast<double>(i) + 0.5 - source_x, x0_);
        }
        for (int y = y_begin; y < y_end; ++y) {
            const double dy = static_cast<double>(y) + 0.5 - source_y;
            if (!(dy >= top_ && dy <= bottom_)) {
                continue;
            }
            const Axis row = value_axis_(dy, y0_);
            for (int x = x_begin; x < x_end; ++x) {
                const double dx = static_cast<double>(x) + 0.5 - source_x;
                const double r2 = dx * dx + aspect2 * dy * dy;
                if (r2 > limit2 || !(dx >= left_ && dx <= right_)) {
                    continue;
                }
                const double share = weight(std::sqrt(r2));
                if (!(share > 0.0)) {
                    continue;
                }
                Light value = interpolate_(columns[static_cast<std::size_t>(x - x_begin)], row);
                for (std::size_t c = 0; c < N; ++c) {
                    value[c] *= scale[c];
                }
                add(x, y, share, value);
            }
        }
        return drawn;
    }

    // The light within each pixel corner, a row of corners at a time, with the corners moved
    // into the samples' rectangle, which holds all the light:
    std::vector<Axis> columns(width + 1);
    for (std::size_t i = 0; i <= width; ++i) {
        const double x = static_cast<double>(x_begin) + static_cast<double>(i) - source_x;
        columns[i] = integral_axis_(std::clamp(x, left_, right_), x0_, nx_);
    }
    std::vector<Light> below(width + 1);
    std::vector<Light> above(width + 1);
    const auto corner_row = [&](int y, std::vector<Light>& corners) {
        const Axis row =
            integral_axis_(std::clamp(static_cast<double>(y) - source_y, top_, bottom_), y0_, ny_);
        for (std::size_t i = 0; i <= width; ++i) {
            corners[i] = corner_light_(columns[i], row);
        }
    };
    corner_row(y_begin, below);
    for (int y = y_begin; y < y_end; ++y) {
        corner_row(y + 1, above);
        const double dy = static_cast<double>(y) + 0.5 - source_y;
        for (int x = x_begin; x < x_end; ++x) {
            const double dx = static_cast<double>(x) + 0.5 - source_x;
            const double r2 = dx * dx + aspect2 * dy * dy;
            if (r2 > limit2) {
                continue;
            }
            const double share = weight(std::sqrt(r2));
            if (!(share > 0.0)) {
                continue;
            }
            const auto i = static_cast<std::size_t>(x - x_begin);
            Light value;
            for (std::size_t c = 0; c < N; ++c) {
                value[c] = above[i + 1][c] - above[i][c] - below[i + 1][c] + below[i][c];
            }
            add(x, y, share, value);
        }
        std::swap(below, above);
    }
    return drawn;
}

/**
 * @brief The light of a source at (source_x, source_y) that falls on a frame width by height
 * pixels, per channel, as a fraction of its own: exactly, from four corners, for point samples;
 * summed over the frame's pixels the PSF reaches for pixel-integrated ones, or all of it if it
 * reaches none beyond.
 */
template <IsSpectral TSpectral>
typename GridPsf<TSpectral>::Light GridPsf<TSpectral>::light_on_frame(
    double source_x, double source_y, int width, int height, const Light& scale) const
{
    if (!pixel_integrated_) {
        return rectangle_light_(-source_x,
                                static_cast<double>(width) - source_x,
                                -source_y,
                                static_cast<double>(height) - source_y);
    }
    Light light{};
    const long first_x = static_cast<long>(std::ceil(source_x + left_ - 0.5));
    const long last_x = static_cast<long>(std::floor(source_x + right_ - 0.5));
    const long first_y = static_cast<long>(std::ceil(source_y + top_ - 0.5));
    const long last_y = static_cast<long>(std::floor(source_y + bottom_ - 0.5));
    if (first_x >= 0 && first_y >= 0 && last_x < width && last_y < height) {
        for (std::size_t c = 0; c < N; ++c) {
            light[c] = scale[c] > 0.0 ? 1.0 : 0.0;
        }
        return light;
    }
    for (long y = std::max(first_y, 0L); y <= std::min(last_y, static_cast<long>(height) - 1);
         ++y) {
        for (long x = std::max(first_x, 0L); x <= std::min(last_x, static_cast<long>(width) - 1);
             ++x) {
            const Light value = interpolate_(static_cast<double>(x) + 0.5 - source_x,
                                             static_cast<double>(y) + 0.5 - source_y);
            for (std::size_t c = 0; c < N; ++c) {
                light[c] += value[c] * scale[c];
            }
        }
    }
    return light;
}

/**
 * @brief At least the light, as a fraction of the source's, that any pixel whose center is r
 * pixel widths or more from it receives in a channel; 0 beyond reach().
 */
template <IsSpectral TSpectral>
double GridPsf<TSpectral>::envelope(std::size_t channel, double r) const
{
    // The coefficients a value takes are within two samples of it, and a point-sampled pixel's
    // within its half diagonal more:
    const double spread = 2.0 * step_ * std::hypot(1.0, aspect_);
    const double margin = pixel_integrated_ ? spread : 0.5 * std::hypot(1.0, aspect_) + spread;
    const auto bin = static_cast<std::size_t>(std::max(0.0, r - margin) * ENVELOPE_BINS);
    if (bin >= envelope_.size()) {
        return 0.0;
    }
    return envelope_[bin][channel];
}

/// The memory the coefficients and sums take.
template <IsSpectral TSpectral>
std::size_t GridPsf<TSpectral>::memory_bytes() const
{
    return (coefficients_.size() + below_.size() + row_.size() + column_.size()) * sizeof(double) +
           (envelope_.size() + line_envelope_.size()) * sizeof(envelope_[0]);
}

} // namespace huira
