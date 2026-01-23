#pragma once

#include <cstddef>
#include <string>
#include <variant>
#include <concepts>
#include <type_traits>

#include "huira/core/types.hpp"
#include "huira/detail/concepts/numeric_concepts.hpp"
#include "huira/detail/concepts/spectral_concepts.hpp"

namespace huira {
    struct DistortionCoefficients {
        DistortionCoefficients() = default;
        DistortionCoefficients(const DistortionCoefficients&) = default;
        DistortionCoefficients& operator=(const DistortionCoefficients&) = default;
        virtual ~DistortionCoefficients() = default;
    };
    
    template <IsSpectral TSpectral, IsFloatingPoint TFloat>
    class Distortion {
    public:
        using FloatType = TFloat;

        Distortion() = default;
        virtual ~Distortion() = default;
        
        [[nodiscard]] virtual Pixel distort(Pixel homogeneous_coords) const = 0;
        [[nodiscard]] virtual Pixel undistort(Pixel homogeneous_coords) const = 0;

        virtual DistortionCoefficients* get_coefficients() = 0;
        [[nodiscard]] virtual const DistortionCoefficients* get_coefficients() const = 0;

        [[nodiscard]] virtual std::string get_type_name() const = 0;

        void set_max_iterations(std::size_t max_iters) { max_iterations_ = max_iters; }
        [[nodiscard]] std::size_t get_max_iterations() const { return max_iterations_; }

        void set_tolerance(float tol) { tolerance_ = tol; tol_sq_ = tolerance_ * tolerance_; }
        [[nodiscard]] float get_tolerance() const { return tolerance_; }

    protected:
        std::size_t max_iterations_ = 20;
        float tol_sq_ = 1e-12f;
        float tolerance_ = 1e-6f;
    };

    template <typename T>
    struct is_distortion : std::false_type {};

    template <template <typename, typename> class Derived, typename TSpectral, typename TFloat>
        requires std::derived_from<Derived<TSpectral, TFloat>, Distortion<TSpectral, TFloat>>
    struct is_distortion<Derived<TSpectral, TFloat>> : std::true_type {};

    template <typename T>
    concept IsDistortion = is_distortion<T>::value;

}
