#pragma once

#include <cmath>
#include <ratio>
#include <string>
#include <type_traits>

#include "huira/concepts/numeric_concepts.hpp"
#include "huira/units/dimensionality.hpp"

namespace huira {

    // Helper to get PI at compile time
    template<typename T>
    constexpr T PI() {
        return static_cast<T>(3.141592653589793238462643383279502884);
    }

    template<IsDimensionality Dim, IsRatio Scale>
    class Quantity {
    public:
        using dimension_type = Dim;
        using scale_type = Scale;

        constexpr explicit Quantity(double value)
            : value_(value)
        {
        }

        // Default constructor
        constexpr Quantity() : value_(0.0) {}

        // Copy constructor
        constexpr Quantity(const Quantity& other) : value_(other.value_) {}

        // Assignment operator
        constexpr Quantity& operator=(const Quantity& other) {
            value_ = other.value_;
            return *this;
        }

        double SIValue() const
        {
            return static_cast<double>(Scale::num) / static_cast<double>(Scale::den) * value_;
        }

        double rawValue() const {
            return value_;
        }

        std::string toString() const
        {
            std::string output = std::to_string(SIValue());
            output += " " + Dim::toSIString();
            return output;
        }

        // Addition (same dimension and scale)
        constexpr Quantity operator+(const Quantity& other) const {
            return Quantity(value_ + other.value_);
        }

        // Subtraction (same dimension and scale)
        constexpr Quantity operator-(const Quantity& other) const {
            return Quantity(value_ - other.value_);
        }

        // Addition assignment
        constexpr Quantity& operator+=(const Quantity& other) {
            value_ += other.value_;
            return *this;
        }

        // Subtraction assignment
        constexpr Quantity& operator-=(const Quantity& other) {
            value_ -= other.value_;
            return *this;
        }

        // Scalar multiplication
        constexpr Quantity operator*(double scalar) const {
            return Quantity(value_ * scalar);
        }

        // Scalar division
        constexpr Quantity operator/(double scalar) const {
            return Quantity(value_ / scalar);
        }

        // Scalar multiplication assignment
        constexpr Quantity& operator*=(double scalar) {
            value_ *= scalar;
            return *this;
        }

        // Scalar division assignment
        constexpr Quantity& operator/=(double scalar) {
            value_ /= scalar;
            return *this;
        }

        // Comparison operators
        constexpr bool operator==(const Quantity& other) const {
            return value_ == other.value_;
        }

        constexpr bool operator!=(const Quantity& other) const {
            return value_ != other.value_;
        }

        constexpr bool operator<(const Quantity& other) const {
            return value_ < other.value_;
        }

        constexpr bool operator>(const Quantity& other) const {
            return value_ > other.value_;
        }

        constexpr bool operator<=(const Quantity& other) const {
            return value_ <= other.value_;
        }

        constexpr bool operator>=(const Quantity& other) const {
            return value_ >= other.value_;
        }

    private:
        double value_;
    };

    // Helper to multiply two ratios
    template<typename R1, typename R2>
    using ratio_multiply = std::ratio_multiply<R1, R2>;

    // Helper to divide two ratios
    template<typename R1, typename R2>
    using ratio_divide = std::ratio_divide<R1, R2>;

    // Multiplication between two quantities
    template<IsDimensionality Dim1, IsRatio Scale1, IsDimensionality Dim2, IsRatio Scale2>
    constexpr auto operator*(const Quantity<Dim1, Scale1>& lhs, const Quantity<Dim2, Scale2>& rhs) {
        using ResultDim = decltype(Dim1{} *Dim2{});
        using ResultScale = ratio_multiply<Scale1, Scale2>;

        return Quantity<ResultDim, ResultScale>(lhs.rawValue() * rhs.rawValue());
    }

    // Division between two quantities
    template<IsDimensionality Dim1, IsRatio Scale1, IsDimensionality Dim2, IsRatio Scale2>
    constexpr auto operator/(const Quantity<Dim1, Scale1>& lhs, const Quantity<Dim2, Scale2>& rhs) {
        using ResultDim = decltype(Dim1{} / Dim2{});
        using ResultScale = ratio_divide<Scale1, Scale2>;

        return Quantity<ResultDim, ResultScale>(lhs.rawValue() / rhs.rawValue());
    }

    // Scalar multiplication (scalar * quantity)
    template<IsDimensionality Dim, IsRatio Scale>
    constexpr Quantity<Dim, Scale> operator*(double scalar, const Quantity<Dim, Scale>& quantity) {
        return quantity * scalar;
    }

    // Addition between quantities of same dimension but different scales
    template<IsDimensionality Dim, IsRatio Scale1, IsRatio Scale2>
    constexpr auto operator+(const Quantity<Dim, Scale1>& lhs, const Quantity<Dim, Scale2>& rhs)
        -> Quantity<Dim, std::ratio<1, 1>> {
        // Convert both to SI units for addition
        double si_result = lhs.SIValue() + rhs.SIValue();
        return Quantity<Dim, std::ratio<1, 1>>(si_result);
    }

    // Subtraction between quantities of same dimension but different scales
    template<IsDimensionality Dim, IsRatio Scale1, IsRatio Scale2>
    constexpr auto operator-(const Quantity<Dim, Scale1>& lhs, const Quantity<Dim, Scale2>& rhs)
        -> Quantity<Dim, std::ratio<1, 1>> {
        // Convert both to SI units for subtraction
        double si_result = lhs.SIValue() - rhs.SIValue();
        return Quantity<Dim, std::ratio<1, 1>>(si_result);
    }

    // Comparison operators between quantities with different scales but same dimension
    template<IsDimensionality Dim, IsRatio Scale1, IsRatio Scale2>
    constexpr bool operator==(const Quantity<Dim, Scale1>& lhs, const Quantity<Dim, Scale2>& rhs) {
        return lhs.SIValue() == rhs.SIValue();
    }

    template<IsDimensionality Dim, IsRatio Scale1, IsRatio Scale2>
    constexpr bool operator!=(const Quantity<Dim, Scale1>& lhs, const Quantity<Dim, Scale2>& rhs) {
        return lhs.SIValue() != rhs.SIValue();
    }

    template<IsDimensionality Dim, IsRatio Scale1, IsRatio Scale2>
    constexpr bool operator<(const Quantity<Dim, Scale1>& lhs, const Quantity<Dim, Scale2>& rhs) {
        return lhs.SIValue() < rhs.SIValue();
    }

    template<IsDimensionality Dim, IsRatio Scale1, IsRatio Scale2>
    constexpr bool operator>(const Quantity<Dim, Scale1>& lhs, const Quantity<Dim, Scale2>& rhs) {
        return lhs.SIValue() > rhs.SIValue();
    }

    template<IsDimensionality Dim, IsRatio Scale1, IsRatio Scale2>
    constexpr bool operator<=(const Quantity<Dim, Scale1>& lhs, const Quantity<Dim, Scale2>& rhs) {
        return lhs.SIValue() <= rhs.SIValue();
    }

    template<IsDimensionality Dim, IsRatio Scale1, IsRatio Scale2>
    constexpr bool operator>=(const Quantity<Dim, Scale1>& lhs, const Quantity<Dim, Scale2>& rhs) {
        return lhs.SIValue() >= rhs.SIValue();
    }



    // Specialize SI Method for more complex conversions (or where `std::ratio` does not meet required precision)   
    using SiderealRatio = std::ratio<976395, 98436902>; // Magic number not used in computation
    template<>
    double Quantity<Time, SiderealRatio>::SIValue() const {
        constexpr double sidreal_to_seconds = 86164.0905;
        return value_ * sidreal_to_seconds;
    }
    
    using DegreeRatio = std::ratio<17329, 192493>; // Magic number not used in computation
    template<>
    double Quantity<Angle, DegreeRatio>::SIValue() const {
        constexpr double degrees_to_radians = PI<double>() / 180.0;
        return value_ * degrees_to_radians;
    }

    using ArcMinuteRatio = std::ratio<403921, 13463>; // Magic number not used in computation
    template<>
    double Quantity<Angle, ArcMinuteRatio>::SIValue() const {
        constexpr double arcminute_to_radian = PI<double>() / 10800;
        return value_ * arcminute_to_radian;
    }

    using ArcSecondRatio = std::ratio<2354235, 918342>; // Magic number not used in computation
    template<>
    double Quantity<Angle, ArcSecondRatio>::SIValue() const {
        constexpr double arcsecond_to_radian = PI<double>() / 648000;
        return value_ * arcsecond_to_radian;
    }

    using CelsiusRatio = std::ratio<189305, 10345>; // Magic number not used in computation
    template<>
    double Quantity<Temperature, CelsiusRatio>::SIValue() const {
        return value_ + 273.15;
    }

    using FahrenheitRatio = std::ratio<9243, 1245>; // Magic number not used in computation
    template <>
    double Quantity<Temperature, FahrenheitRatio>::SIValue() const {
        return (value_ + 459.67) * (5. / 9.);
    }

    using EVRatio = std::ratio<2339243, 124345>; // Magic number not used in computation
    template <>
    double Quantity<Energy, EVRatio>::SIValue() const {
        return value_ * 1.602176634e-19;
    }
}