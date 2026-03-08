#include <array>
#include <iostream>
#include <string>

#include "huira/core/physics.hpp"

namespace huira {
    /**
     * @brief Default constructor initializing all bins to zero.
     */
    template <std::size_t N, auto... Args>
    constexpr SpectralBins<N, Args...>::SpectralBins()
    {
        data_.fill(0.0f);
    }

    /**
     * @brief Constructs a SpectralBins with all bins set to the same value.
     * 
     * @param value The value to assign to all bins.
     */
    template <std::size_t N, auto... Args>
    constexpr SpectralBins<N, Args...>::SpectralBins(const float& value)
    {
        data_.fill(value);
    }

    /**
     * @brief Constructs from an initializer list.
     * 
     * If the list has one element, fills all bins with that value.
     * Otherwise, copies values from the list (up to N elements).
     * 
     * @param init Initializer list of float values.
     */
    template <std::size_t N, auto... Args>
    constexpr SpectralBins<N, Args...>::SpectralBins(std::initializer_list<float> init)
    {
        if (init.size() == 1) {
            data_.fill(*init.begin());
        }
        else {
            std::copy(init.begin(),
                std::next(init.begin(), static_cast<std::ptrdiff_t>(std::min(init.size(), N))),
                data_.begin());
        }
    }

    /**
     * @brief Constructs from exactly N values.
     * 
     * @tparam Args2 Parameter pack of convertible-to-float types.
     * @param args Values for each bin (must be exactly N values).
     */
    template <std::size_t N, auto... Args>
    template <typename... Args2>
        requires (sizeof...(Args2) == N && (std::convertible_to<Args2, float> && ...))
    constexpr SpectralBins<N, Args...>::SpectralBins(Args2&&... args)
        : data_{ static_cast<float>(args)... }
    {
        
    }



    /**
     * @brief Integrates externally-sampled spectral data into each bin.
     *
     * Given a set of (wavelength, value) pairs representing a continuous spectral function
     * (e.g. a sensor quantum-efficiency curve loaded from a CSV), this method integrates
     * the data over each bin's wavelength range using the trapezoidal rule with linear
     * interpolation at bin boundaries, then divides by the bin width to produce a mean
     * value per bin.
     *
     *   bin[i] = (1 / bin_width_nm) * integral(value(λ) dλ,  bin_min_nm, bin_max_nm)
     *
     * Bins with no overlapping data (entirely outside the sampled range) are set to 0.
     *
     * @param wavelengths Sample wavelengths in **nanometers**, strictly ascending.
     * @param values      Spectral values at each corresponding wavelength.
     * @return SpectralBins with the mean spectral value over each bin.
     * @throws std::invalid_argument if lengths differ, vectors are empty, or wavelengths
     *         are not strictly ascending.
     */
    template <std::size_t N, auto... Args>
    SpectralBins<N, Args...> SpectralBins<N, Args...>::integrate_over_data(
        const std::vector<double>& wavelengths,
        const std::vector<float>& values)
    {
        // --- Precondition checks ---
        if (wavelengths.size() != values.size()) {
            throw std::invalid_argument(
                "integrate_over_data: wavelengths and values must have the same length");
        }
        if (wavelengths.empty()) {
            throw std::invalid_argument(
                "integrate_over_data: wavelengths and values must not be empty");
        }
        for (std::size_t k = 1; k < wavelengths.size(); ++k) {
            if (wavelengths[k] <= wavelengths[k - 1]) {
                throw std::invalid_argument(
                    "integrate_over_data: wavelengths must be strictly ascending");
            }
        }

        const std::size_t M = wavelengths.size();

        // Linearly interpolate a value at wavelength `t` between adjacent sample indices.
        auto lerp_value = [&](std::size_t lo, std::size_t hi, double t) -> double {
            double alpha = (t - wavelengths[lo]) / (wavelengths[hi] - wavelengths[lo]);
            return static_cast<double>(values[lo]) * (1.0 - alpha)
                + static_cast<double>(values[hi]) * alpha;
            };

        // Manual lower-bound: returns first index k where wavelengths[k] >= target.
        auto lower_bound_idx = [&](double target) -> std::size_t {
            std::size_t first = 0, count = M;
            while (count > 0) {
                std::size_t step = count / 2, mid = first + step;
                if (wavelengths[mid] < target) { first = mid + 1; count -= step + 1; }
                else { count = step; }
            }
            return first;
            };

        SpectralBins result;

        for (std::size_t i = 0; i < N; ++i) {
            // Bin edges in nm (bins_ stores SI metres, so multiply by 1e9).
            const double bin_min_nm = get_bin(i).min_wavelength * 1e9;
            const double bin_max_nm = get_bin(i).max_wavelength * 1e9;
            const double bin_width = bin_max_nm - bin_min_nm;

            // Index of the first sample at or above bin_min_nm.
            const std::size_t lo_idx = lower_bound_idx(bin_min_nm);

            // Bin has no coverage if all samples are below it, or the first sample is already
            // at or beyond bin_max_nm (i.e. no samples fall inside the bin and no sample pair
            // straddles it).
            if (lo_idx == M) {
                result[i] = 0.0f;   // entire bin is beyond the data range
                continue;
            }
            if (wavelengths[lo_idx] >= bin_max_nm && lo_idx == 0) {
                result[i] = 0.0f;   // entire bin is before the data range
                continue;
            }
            // Also handle the case where the bin sits entirely in a gap beyond the first sample
            // but the lower-bound index points past bin_max_nm.
            if (lo_idx > 0 && wavelengths[lo_idx] >= bin_max_nm) {
                // No sample inside the bin, but the bin straddles a gap between lo_idx-1 and
                // lo_idx.  Integrate the linear segment across the full bin width.
                double left_val = lerp_value(lo_idx - 1, lo_idx, bin_min_nm);
                double right_val = lerp_value(lo_idx - 1, lo_idx, bin_max_nm);
                result[i] = static_cast<float>(0.5 * (left_val + right_val));
                continue;
            }

            // --- Accumulate the trapezoid integral across the bin ---
            // We walk left-to-right, tracking (prev_lam, prev_val) as the last knot processed.
            double integral = 0.0;
            double prev_lam = 0.0;
            double prev_val = 0.0;
            bool   have_prev = false;

            // Left edge: if bin_min_nm falls strictly between two samples, interpolate.
            if (lo_idx > 0 && wavelengths[lo_idx] > bin_min_nm) {
                prev_lam = bin_min_nm;
                prev_val = lerp_value(lo_idx - 1, lo_idx, bin_min_nm);
                have_prev = true;
            }

            // Interior samples: all k where bin_min_nm <= wavelengths[k] < bin_max_nm.
            for (std::size_t k = lo_idx; k < M && wavelengths[k] < bin_max_nm; ++k) {
                double cur_lam = wavelengths[k];
                double cur_val = static_cast<double>(values[k]);
                if (have_prev) {
                    integral += 0.5 * (prev_val + cur_val) * (cur_lam - prev_lam);
                }
                prev_lam = cur_lam;
                prev_val = cur_val;
                have_prev = true;
            }

            // Right edge: interpolate at bin_max_nm.
            // hi_idx is the first index >= bin_max_nm.
            const std::size_t hi_idx = lower_bound_idx(bin_max_nm);
            double right_val = 0.0;
            if (hi_idx < M) {
                // bin_max_nm falls between wavelengths[hi_idx-1] and wavelengths[hi_idx]
                // (or exactly on wavelengths[hi_idx], but lower_bound guarantees hi_idx > lo_idx).
                right_val = lerp_value(hi_idx - 1, hi_idx, bin_max_nm);
            }
            else {
                // bin_max_nm is beyond the last sample: extend flat from the last sample.
                right_val = static_cast<double>(values[M - 1]);
            }

            if (have_prev) {
                integral += 0.5 * (prev_val + right_val) * (bin_max_nm - prev_lam);
            }

            result[i] = static_cast<float>(integral / bin_width);
        }

        return result;
    }


    /**
     * @brief Creates a spectral distribution from a total value.
     * 
     * Distributes the total value proportionally across bins based on their
     * wavelength widths. Wider bins receive proportionally more of the total.
     * 
     * @param total The total value to distribute.
     * @return A SpectralBins instance with distributed values.
     */
    template <std::size_t N, auto... Args>
    constexpr SpectralBins<N, Args...> SpectralBins<N, Args...>::from_total(float total) {
        SpectralBins result;

        // Sum total wavelength coverage
        double total_width = 0.0;
        for (std::size_t i = 0; i < N; ++i) {
            total_width += get_bin(i).max_wavelength - get_bin(i).min_wavelength;
        }

        // Distribute proportionally to each bin's width
        for (std::size_t i = 0; i < N; ++i) {
            double bin_width = get_bin(i).max_wavelength - get_bin(i).min_wavelength;
            result[i] = static_cast<float>(static_cast<double>(total) * (bin_width / total_width));
        }

        return result;
    }

    
    /**
     * @brief Computes the sum of all spectral values.
     * 
     * @return The total sum across all bins.
     */
    template <std::size_t N, auto... Args>
    float SpectralBins<N, Args...>::total() const {
        float sum = 0.0f;
        for (std::size_t i = 0; i < N; ++i) {
            sum += data_[i];
        }
        return sum;
    }

    /**
     * @brief Computes the Euclidean magnitude (L2 norm).
     * 
     * @return The square root of the sum of squared values.
     */
    template <std::size_t N, auto... Args>
    float SpectralBins<N, Args...>::magnitude() const {
        float sum_of_squares = 0.0f;
        for (std::size_t i = 0; i < N; ++i) {
            sum_of_squares += data_[i] * data_[i];
        }
        return std::sqrt(sum_of_squares);
    }

    /**
     * @brief Finds the maximum value across all bins.
     * 
     * @return The maximum spectral value, or 0.0 if N is 0.
     */
    template <std::size_t N, auto... Args>
    float SpectralBins<N, Args...>::max() const {
        if constexpr (N == 0) {
            return 0.0f;
        }
        float max_val = data_[0];
        for (std::size_t i = 1; i < N; ++i) {
            if (data_[i] > max_val) {
                max_val = data_[i];
            }
        }
        return max_val;
    }

    /**
     * @brief Finds the minimum value across all bins.
     * 
     * @return The minimum spectral value, or 0.0 if N is 0.
     */
    template <std::size_t N, auto... Args>
    float SpectralBins<N, Args...>::min() const {
        if constexpr (N == 0) {
            return 0.0f;
        }
        float min_val = data_[0];
        for (std::size_t i = 1; i < N; ++i) {
            if (data_[i] < min_val) {
                min_val = data_[i];
            }
        }
        return min_val;
    }

    /**
     * @brief Computes the wavelength-weighted integral.
     * 
     * Integrates the spectral distribution by summing each bin's value
     * multiplied by its wavelength width.
     * 
     * @return The integrated value over wavelength.
     */
    template <std::size_t N, auto... Args>
    float SpectralBins<N, Args...>::integrate() const {
        float integral = 0.0f;
        for (std::size_t i = 0; i < N; ++i) {
            float bin_width = static_cast<float>(get_bin(i).max_wavelength - get_bin(i).min_wavelength);
            integral += data_[i] * bin_width;
        }
        return integral;
    }

    template <std::size_t N, auto... Args>
    float SpectralBins<N, Args...>::integrate_over_band(double min_wavelength, double max_wavelength) const
    {
        float integral = 0.0f;
        for (std::size_t i = 0; i < N; ++i) {
            double bin_min = get_bin(i).min_wavelength;
            double bin_max = get_bin(i).max_wavelength;

            if (bin_max <= min_wavelength || bin_min >= max_wavelength) {
                continue;
            }

            double overlap_min = std::max(bin_min, min_wavelength);
            double overlap_max = std::min(bin_max, max_wavelength);
            double overlap_width = overlap_max - overlap_min;

            integral += static_cast<float>(data_[i] * overlap_width);
        }
        return integral;
    }

    /**
     * @brief Checks if all spectral values are valid.
     * 
     * Validates that all values are non-negative, not NaN, and not infinite.
     * 
     * @return True if all values are valid.
     */
    template <std::size_t N, auto... Args>
    bool SpectralBins<N, Args...>::valid() const
    {
        for (std::size_t i = 0; i < N; ++i) {
            if (data_[i] < 0.f) {
                return false;
            }

            if (std::isnan(data_[i])) {
                return false;
            }

            if (std::isinf(data_[i])) {
                return false;
            }
        }
        return true;
    }

    /**
     * @brief Checks if all spectral values are valid albedo values.
     *
     * Validates that all values are between 0 and 1, not NaN, and not infinite.
     *
     * @return True if all values are valid abledo values.
     */
    template <std::size_t N, auto... Args>
    bool SpectralBins<N, Args...>::valid_ratio() const
    {
        for (std::size_t i = 0; i < N; ++i) {
            if (data_[i] < 0.f || data_[i] > 1.f) {
                return false;
            }
            if (std::isnan(data_[i])) {
                return false;
            }
            if (std::isinf(data_[i])) {
                return false;
            }
        }
        return true;
    }


    // ========================================= //
    // === Array-Array Arithmetic Operations === //
    // ========================================= //
    template <std::size_t N, auto... Args>
    constexpr SpectralBins<N, Args...>& SpectralBins<N, Args...>::operator+=(const SpectralBins& other) {
        for (std::size_t i = 0; i < N; ++i) {
            data_[i] += other.data_[i];
        }
        return *this;
    }

    template <std::size_t N, auto... Args>
    constexpr SpectralBins<N, Args...>& SpectralBins<N, Args...>::operator-=(const SpectralBins& other) {
        for (std::size_t i = 0; i < N; ++i) {
            data_[i] -= other.data_[i];
        }
        return *this;
    }

    template <std::size_t N, auto... Args>
    constexpr SpectralBins<N, Args...>& SpectralBins<N, Args...>::operator*=(const SpectralBins& other) {
        for (std::size_t i = 0; i < N; ++i) {
            data_[i] *= other.data_[i];
        }
        return *this;
    }

    template <std::size_t N, auto... Args>
    constexpr SpectralBins<N, Args...>& SpectralBins<N, Args...>::operator/=(const SpectralBins& other) {
        for (std::size_t i = 0; i < N; ++i) {
            data_[i] /= other.data_[i];
        }
        return *this;
    }

    // ========================================== //
    // === Array-Scalar Arithmetic Operations === //
    // ========================================== //
    template <std::size_t N, auto... Args>
    template <typename U>
    constexpr SpectralBins<N, Args...>& SpectralBins<N, Args...>::operator+=(const U& scalar) {
        for (std::size_t i = 0; i < N; ++i) {
            data_[i] += static_cast<float>(scalar);
        }
        return *this;
    }

    template <std::size_t N, auto... Args>
    template <typename U>
    constexpr SpectralBins<N, Args...>& SpectralBins<N, Args...>::operator-=(const U& scalar) {
        for (std::size_t i = 0; i < N; ++i) {
            data_[i] -= static_cast<float>(scalar);
        }
        return *this;
    }

    template <std::size_t N, auto... Args>
    template <typename U>
    constexpr SpectralBins<N, Args...>& SpectralBins<N, Args...>::operator*=(const U& scalar) {
        for (std::size_t i = 0; i < N; ++i) {
            data_[i] *= static_cast<float>(scalar);
        }
        return *this;
    }

    template <std::size_t N, auto... Args>
    template <typename U>
    constexpr SpectralBins<N, Args...>& SpectralBins<N, Args...>::operator/=(const U& scalar) {
        for (std::size_t i = 0; i < N; ++i) {
            data_[i] /= static_cast<float>(scalar);
        }
        return *this;
    }

    // ======================= //
    // === Unary Operators === //
    // ======================= //
    template <std::size_t N, auto... Args>
    constexpr SpectralBins<N, Args...> SpectralBins<N, Args...>::operator+() const {
        return *this;
    }

    template <std::size_t N, auto... Args>
    constexpr SpectralBins<N, Args...> SpectralBins<N, Args...>::operator-() const {
        SpectralBins<N, Args...> result;
        for (std::size_t i = 0; i < N; ++i) {
            result.data_[i] = -data_[i];
        }
        return result;
    }

    // ============================ //
    // === Comparison Operators === //
    // ============================ //
    template <std::size_t N, auto... Args>
    constexpr bool SpectralBins<N, Args...>::operator==(const SpectralBins<N, Args...>& other) const {
        return data_ == other.data_;
    }

    template <std::size_t N, auto... Args>
    constexpr bool SpectralBins<N, Args...>::operator!=(const SpectralBins<N, Args...>& other) const {
        return data_ != other.data_;
    }

    // ======================== //
    // === String Functions === //
    // ======================== //
    template <std::size_t N, auto... Args>
    std::string SpectralBins<N, Args...>::to_string() const {
        std::string result = "[";
        for (std::size_t i = 0; i < N; ++i) {
            result += std::to_string(data_[i]);
            if (i < (N - 1)) {
                result += ", ";
            }
        }
        result += "]";
        return result;
    }

    /**
     * @brief Computes photon energies for each bin's center wavelength.
     * 
     * Calculates the energy of a photon at each bin's center wavelength
     * using the relation E = hc/lambda.
     * 
     * @return A SpectralBins instance containing photon energies in Joules.
     */
    template <std::size_t N, auto... Args>
    constexpr SpectralBins<N, Args...> SpectralBins<N, Args...>::photon_energies()
    {
        SpectralBins<N, Args...> result;
        for (std::size_t i = 0; i < N; ++i) {
            double wavelength = static_cast<double>(get_bin(i).center_wavelength);
            result[i] = static_cast<float>(photon_energy(wavelength));
        }
        return result;
    }


    // ================================== //
    // === Array Arithmetic Operators === //
    // ================================== //
    template <std::size_t N, auto... Args>
    constexpr SpectralBins<N, Args...> operator+(const SpectralBins<N, Args...>& lhs, const SpectralBins<N, Args...>& rhs) {
        SpectralBins<N, Args...> result = lhs;
        result += rhs;
        return result;
    }

    template <std::size_t N, auto... Args>
    constexpr SpectralBins<N, Args...> operator-(const SpectralBins<N, Args...>& lhs, const SpectralBins<N, Args...>& rhs) {
        SpectralBins<N, Args...> result = lhs;
        result -= rhs;
        return result;
    }

    template <std::size_t N, auto... Args>
    constexpr SpectralBins<N, Args...> operator*(const SpectralBins<N, Args...>& lhs, const SpectralBins<N, Args...>& rhs) {
        SpectralBins<N, Args...> result = lhs;
        result *= rhs;
        return result;
    }

    template <std::size_t N, auto... Args>
    constexpr SpectralBins<N, Args...> operator/(const SpectralBins<N, Args...>& lhs, const SpectralBins<N, Args...>& rhs) {
        SpectralBins<N, Args...> result = lhs;
        result /= rhs;
        return result;
    }

    // ========================================= //
    // === Array-Scalar Arithmetic Operators === //
    // ========================================= //
    template <std::size_t N, auto... Args, typename U>
    constexpr SpectralBins<N, Args...> operator+(const SpectralBins<N, Args...>& lhs, const U& rhs) {
        SpectralBins<N, Args...> result = lhs;
        result += rhs;
        return result;
    }

    template <std::size_t N, auto... Args, typename U>
    constexpr SpectralBins<N, Args...> operator-(const SpectralBins<N, Args...>& lhs, const U& rhs) {
        SpectralBins<N, Args...> result = lhs;
        result -= rhs;
        return result;
    }

    template <std::size_t N, auto... Args, typename U>
    constexpr SpectralBins<N, Args...> operator*(const SpectralBins<N, Args...>& lhs, const U& rhs) {
        SpectralBins<N, Args...> result = lhs;
        result *= rhs;
        return result;
    }

    template <std::size_t N, auto... Args, typename U>
    constexpr SpectralBins<N, Args...> operator/(const SpectralBins<N, Args...>& lhs, const U& rhs) {
        SpectralBins<N, Args...> result = lhs;
        result /= rhs;
        return result;
    }

    // ========================================== //
    // === Scalar-Array Arithmetic Operators ==== //
    // ========================================== //
    template <std::size_t N, auto... Args, typename U>
    constexpr SpectralBins<N, Args...> operator+(const U& lhs, const SpectralBins<N, Args...>& rhs) {
        return rhs + lhs;
    }

    template <std::size_t N, auto... Args, typename U>
    constexpr SpectralBins<N, Args...> operator*(const U& lhs, const SpectralBins<N, Args...>& rhs) {
        return rhs * lhs;
    }

    template <std::size_t N, auto... Args, typename U>
    constexpr SpectralBins<N, Args...> operator-(const U& lhs, const SpectralBins<N, Args...>& rhs) {
        SpectralBins<N, Args...> result;
        for (std::size_t i = 0; i < N; ++i) {
            result[i] = static_cast<float>(lhs) - rhs[i];
        }
        return result;
    }

    template <std::size_t N, auto... Args, typename U>
    constexpr SpectralBins<N, Args...> operator/(const U& lhs, const SpectralBins<N, Args...>& rhs) {
        SpectralBins<N, Args...> result;
        for (std::size_t i = 0; i < N; ++i) {
            result[i] = static_cast<float>(lhs) / rhs[i];
        }
        return result;
    }

    // ======================== //
    // === Stream Operator === //
    // ======================== //
    template <std::size_t N, auto... Args>
    std::ostream& operator<<(std::ostream& os, const SpectralBins<N, Args...>& v) {
        os << v.to_string();
        return os;
    }


    // ======================== //
    // === Static Functions === //
    // ======================== //
    
    /**
     * @brief Initializes the bin array based on template parameters.
     * 
     * Dispatches to the appropriate initialization method based on the number
     * of template arguments, and validates that bins are non-overlapping.
     * 
     * @return Array of initialized bins.
     * @note Produces a compile-time error if bins are invalid or overlap.
     */
    template <std::size_t N, auto... Args>
    constexpr std::array<Bin, N> SpectralBins<N, Args...>::initialize_bins_static_() {
        constexpr size_t num_args = sizeof...(Args);
        std::array<Bin, N> bins{};
        if constexpr (num_args == 2) {
            static_assert(N > 0, "Must have at least 1 bin");
            bins = initialize_uniform_static_();
        }
        else if constexpr (num_args == 2 * N) {
            bins = initialize_pairs_static_();
        }
        else if constexpr (num_args == N + 1) {
            bins = initialize_edges_static_();
        }
        else {
            static_assert(num_args == 2 || num_args == 2 * N || num_args == N + 1,
                "Must provide either 2 args (uniform), 2*N args (pairs), or N+1 args (edges)");
        }


        // Check each bin has min < max
        for (std::size_t i = 0; i < N; ++i) {
            if (bins[i].min_wavelength >= bins[i].max_wavelength) {
                throw "Bin has min_wavelength >= max_wavelength (inverted or zero-width bin)";
            }
        }

        // Check no pair of bins overlaps (order-independent)
        for (std::size_t i = 0; i < N; ++i) {
            for (std::size_t j = i + 1; j < N; ++j) {
                // Two intervals overlap if one starts before the other ends and vice versa
                if (bins[i].min_wavelength < bins[j].max_wavelength &&
                    bins[j].min_wavelength < bins[i].max_wavelength) {
                    throw "Spectral bins overlap";
                }
            }
        }

        return bins;
    }

    /**
     * @brief Creates uniformly spaced bins from min to max wavelength.
     * 
     * @return Array of N uniformly spaced bins.
     */
    template <std::size_t N, auto... Args>
    constexpr std::array<Bin, N> SpectralBins<N, Args...>::initialize_uniform_static_() {
        auto args_array = std::array<double, 2>{ static_cast<double>(Args)... };
        double min_val = args_array[0] * 1e-9;
        double max_val = args_array[1] * 1e-9;
        double step = (max_val - min_val) / static_cast<double>(N);

        std::array<Bin, N> result{};
        for (std::size_t i = 0; i < N; ++i) {
            double bin_min = min_val + static_cast<double>(i) * step;
            double bin_max = min_val + static_cast<double>(i + 1) * step;
            result[i] = Bin(bin_min, bin_max);
        }
        return result;
    }

    /**
     * @brief Creates bins from explicit min/max pairs.
     * 
     * Expects 2N arguments: min1, max1, min2, max2, ..., minN, maxN (in nm).
     * 
     * @return Array of bins defined by the pairs.
     */
    template <std::size_t N, auto... Args>
    constexpr std::array<Bin, N> SpectralBins<N, Args...>::initialize_pairs_static_() {
        auto args_array = std::array<double, 2 * N>{ static_cast<double>(Args)... };
        std::array<Bin, N> result{};
        for (std::size_t i = 0; i < N; ++i) {
            result[i] = Bin(args_array[2 * i] * 1e-9, args_array[2 * i + 1] * 1e-9);
        }
        return result;
    }

    /**
     * @brief Creates bins from N+1 bin edges.
     * 
     * Expects N+1 arguments defining bin boundaries (in nm).
     * Bin i spans from edge[i] to edge[i+1].
     * 
     * @return Array of bins defined by the edges.
     */
    template <std::size_t N, auto... Args>
    constexpr std::array<Bin, N> SpectralBins<N, Args...>::initialize_edges_static_() {
        auto args_array = std::array<double, N + 1>{ static_cast<double>(Args)... };
        std::array<Bin, N> result{};
        for (std::size_t i = 0; i < N; ++i) {
            result[i] = Bin(args_array[i] * 1e-9, args_array[i + 1] * 1e-9);
        }
        return result;
    }



    /// @brief Conversion function from RGB to a spectral representation (e.g., Visible8).
    template <typename TSpectral>
    TSpectral convert_rgb_to_spectral(const RGB& rgb)
    {
        if constexpr (std::same_as<TSpectral, RGB>) {
            return rgb;
        }
        else {
            // Determine overlap between TSpectral's full range and RGB's full range
            constexpr double spectral_min = TSpectral::get_bin(0).min_wavelength;
            constexpr double spectral_max = TSpectral::get_bin(TSpectral::size() - 1).max_wavelength;
            constexpr double rgb_min = RGB::get_bin(0).min_wavelength;  // 380nm (blue bin min)
            constexpr double rgb_max = RGB::get_bin(0).max_wavelength;  // 750nm (red bin max)

            constexpr double overlap_min = std::max(spectral_min, rgb_min);
            constexpr double overlap_max = std::min(spectral_max, rgb_max);
            constexpr double overlap = overlap_max - overlap_min;
            constexpr double rgb_range = rgb_max - rgb_min;

            constexpr bool has_significant_overlap = (overlap / rgb_range) > 0.1;

            if constexpr (has_significant_overlap) {
                TSpectral spectral;
                for (std::size_t i = 0; i < TSpectral::size(); ++i) {
                    const Bin& bin = TSpectral::get_bin(i);
                    float denom = static_cast<float>(bin.max_wavelength - bin.min_wavelength);
                    spectral[i] = rgb.integrate_over_band(bin.min_wavelength, bin.max_wavelength) / denom;
                }
                return spectral;
            }
            else {
                // No significant overlap (e.g. SWIR): fill all bins with grayscale average
                float gray = (rgb[0] + rgb[1] + rgb[2]) / 3.0f;
                return TSpectral(gray);
            }
        }
    }
}
