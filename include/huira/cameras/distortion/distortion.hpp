
#pragma once

#include <cstddef>
#include <concepts>
#include <string>
#include <type_traits>
#include <variant>

#include "huira/core/concepts/spectral_concepts.hpp"
#include "huira/core/types.hpp"

namespace huira {
    /**
     * @brief Base class for distortion coefficient sets.
     *
     * Provides a polymorphic interface for all distortion coefficient types.
     */
    struct DistortionCoefficients {
        DistortionCoefficients() = default;
        DistortionCoefficients(const DistortionCoefficients&) = default;
        DistortionCoefficients& operator=(const DistortionCoefficients&) = default;
        virtual ~DistortionCoefficients() = default;
    };
    

    /**
     * @brief Abstract base class for lens distortion models.
     *
     * Defines the interface for all distortion models, including distortion/undistortion and coefficient access.
     *
     * @tparam TSpectral The spectral representation type.
     */
    template <IsSpectral TSpectral>
    class Distortion {
    public:
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
        double tol_sq_ = 1e-12;
        double tolerance_ = 1e-6;
    };

    template <typename T>
    struct is_distortion : std::false_type {};

    template <template <typename> class Derived, typename TSpectral>
        requires std::derived_from<Derived<TSpectral>, Distortion<TSpectral>>
    struct is_distortion<Derived<TSpectral>> : std::true_type {};

    template <typename T>
    concept IsDistortion = is_distortion<T>::value;

}
