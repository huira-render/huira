#include <string>
#include <iostream>
#include <array>

namespace huira {
    // ======================== //
    // === Static Definition === //
    // ======================== //
    template <size_t N, auto... Args>
    std::array<Bin, N> SpectralBins<N, Args...>::bins_ = SpectralBins<N, Args...>::initialize_bins_static();


    // ======================= //
    // === Static Functions === //
    // ======================= //
    template <size_t N, auto... Args>
    constexpr std::array<Bin, N> SpectralBins<N, Args...>::initialize_bins_static() {
        constexpr size_t num_args = sizeof...(Args);
        if constexpr (num_args == 2) {
            static_assert(N > 0, "Must have at least 1 bin");
            return initialize_uniform_static();
        }
        else if constexpr (num_args == 2 * N) {
            return initialize_pairs_static();
        }
        else if constexpr (num_args == N + 1) {
            return initialize_edges_static();
        }
        else {
            static_assert(num_args == 2 || num_args == 2 * N || num_args == N + 1,
                "Must provide either 2 args (uniform), 2*N args (pairs), or N+1 args (edges)");
            return {};
        }
    }

    template <size_t N, auto... Args>
    constexpr std::array<Bin, N> SpectralBins<N, Args...>::initialize_uniform_static() {
        auto args_array = std::array<float, 2>{ static_cast<float>(Args)... };
        float min_val = args_array[0];
        float max_val = args_array[1];
        float step = (max_val - min_val) / N;

        std::array<Bin, N> result{};
        for (size_t i = 0; i < N; ++i) {
            float bin_min = min_val + static_cast<float>(i) * step;
            float bin_max = min_val + static_cast<float>(i + 1) * step;
            result[i] = Bin(bin_min, bin_max);
        }
        return result;
    }

    template <size_t N, auto... Args>
    constexpr std::array<Bin, N> SpectralBins<N, Args...>::initialize_pairs_static() {
        auto args_array = std::array<float, 2 * N>{ static_cast<float>(Args)... };
        std::array<Bin, N> result{};
        for (size_t i = 0; i < N; ++i) {
            result[i] = Bin(args_array[2 * i], args_array[2 * i + 1]);
        }
        return result;
    }

    template <size_t N, auto... Args>
    constexpr std::array<Bin, N> SpectralBins<N, Args...>::initialize_edges_static() {
        auto args_array = std::array<float, N + 1>{ static_cast<float>(Args)... };
        std::array<Bin, N> result{};
        for (size_t i = 0; i < N; ++i) {
            result[i] = Bin(args_array[i], args_array[i + 1]);
        }
        return result;
    }
}