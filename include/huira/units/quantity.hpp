#pragma once

#include <cmath>
#include <ratio>
#include <string>
#include <type_traits>
#include <ostream>

#include "huira/concepts/numeric_concepts.hpp"
#include "huira/math/constants.hpp"
#include "huira/units/dimensionality.hpp"

namespace huira {
    template<IsDimensionality Dim, IsRatio Scale>
    class Quantity {
    public:
        using dimension_type = Dim;
        using scale_type = Scale;

        template<IsNumeric T>
        constexpr Quantity(T value)
            requires std::is_arithmetic_v<T>
        : value_(static_cast<double>(value))
        {

        }

        template<IsRatio OtherScale>
        constexpr Quantity(const Quantity<Dim, OtherScale>& other)
        {
            value_ = this->fromSI(other.getSIValue());
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

        double getSIValue() const
        {
            return this->toSI(value_);
        }

        double rawValue() const {
            return value_;
        }

        std::string toString() const
        {
            std::string output = std::to_string(getSIValue());
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

        friend std::ostream& operator<<(std::ostream& os, const Quantity& quantity) {
            os << quantity.toString();
            return os;
		}

    private:
        double value_;

        constexpr double getRatio() const
        {
            return static_cast<double>(Scale::num) / static_cast<double>(Scale::den);
		}

        constexpr double fromSI(double si_value) const
        {
            return si_value / this->getRatio();
		}

        constexpr double toSI(double value) const
        {
            return value * this->getRatio();
        };
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
        double si_result = lhs.getSIValue() + rhs.getSIValue();
        return Quantity<Dim, std::ratio<1, 1>>(si_result);
    }

    // Subtraction between quantities of same dimension but different scales
    template<IsDimensionality Dim, IsRatio Scale1, IsRatio Scale2>
    constexpr auto operator-(const Quantity<Dim, Scale1>& lhs, const Quantity<Dim, Scale2>& rhs)
        -> Quantity<Dim, std::ratio<1, 1>> {
        // Convert both to SI units for subtraction
        double si_result = lhs.getSIValue() - rhs.getSIValue();
        return Quantity<Dim, std::ratio<1, 1>>(si_result);
    }

    // Comparison operators between quantities with different scales but same dimension
    template<IsDimensionality Dim, IsRatio Scale1, IsRatio Scale2>
    constexpr bool operator==(const Quantity<Dim, Scale1>& lhs, const Quantity<Dim, Scale2>& rhs) {
        return lhs.getSIValue() == rhs.getSIValue();
    }

    template<IsDimensionality Dim, IsRatio Scale1, IsRatio Scale2>
    constexpr bool operator!=(const Quantity<Dim, Scale1>& lhs, const Quantity<Dim, Scale2>& rhs) {
        return lhs.getSIValue() != rhs.getSIValue();
    }

    template<IsDimensionality Dim, IsRatio Scale1, IsRatio Scale2>
    constexpr bool operator<(const Quantity<Dim, Scale1>& lhs, const Quantity<Dim, Scale2>& rhs) {
        return lhs.getSIValue() < rhs.getSIValue();
    }

    template<IsDimensionality Dim, IsRatio Scale1, IsRatio Scale2>
    constexpr bool operator>(const Quantity<Dim, Scale1>& lhs, const Quantity<Dim, Scale2>& rhs) {
        return lhs.getSIValue() > rhs.getSIValue();
    }

    template<IsDimensionality Dim, IsRatio Scale1, IsRatio Scale2>
    constexpr bool operator<=(const Quantity<Dim, Scale1>& lhs, const Quantity<Dim, Scale2>& rhs) {
        return lhs.getSIValue() <= rhs.getSIValue();
    }

    template<IsDimensionality Dim, IsRatio Scale1, IsRatio Scale2>
    constexpr bool operator>=(const Quantity<Dim, Scale1>& lhs, const Quantity<Dim, Scale2>& rhs) {
        return lhs.getSIValue() >= rhs.getSIValue();
    }

    // Specialize SI Method for more complex conversions (or where `std::ratio` does not meet required precision)   
    using SiderealRatio = std::ratio<976395, 98436902>; // Magic number not used in computation
    template<>
    constexpr double Quantity<Time, SiderealRatio>::getRatio() const { return 86164.0905; }
    
    using DegreeRatio = std::ratio<17329, 192493>; // Magic number not used in computation
    template<>
    constexpr double Quantity<Angle, DegreeRatio>::getRatio() const { return PI<double>() / 180.0; }

    using ArcMinuteRatio = std::ratio<403921, 13463>; // Magic number not used in computation
    template<>
    constexpr double Quantity<Angle, ArcMinuteRatio>::getRatio() const { return PI<double>() / 10800.0; }

    using ArcSecondRatio = std::ratio<2354235, 918342>; // Magic number not used in computation
    template<>
    constexpr double Quantity<Angle, ArcSecondRatio>::getRatio() const { return PI<double>() / 648000.0; }

    using SquareDegreeRatio = std::ratio<934629, 743097>; // Magic number not used in computation
    template<>
    constexpr double Quantity<SolidAngle, SquareDegreeRatio>::getRatio() const { return (PI<double>() / 180.0) * (PI<double>() / 180.0); }

    using CelsiusRatio = std::ratio<189305, 10345>; // Magic number not used in computation
    template<>
    constexpr double Quantity<Temperature, CelsiusRatio>::toSI(double value) const {
        return value + 273.15;
    }

    using CelsiusRatio = std::ratio<189305, 10345>; // Magic number not used in computation
    template<>
    constexpr double Quantity<Temperature, CelsiusRatio>::fromSI(double si_value) const {
        return si_value - 273.15;
    }

    using FahrenheitRatio = std::ratio<9243, 1245>; // Magic number not used in computation
    template <>
    constexpr double Quantity<Temperature, FahrenheitRatio>::toSI(double value) const {
        return (value + 459.67) * (5. / 9.);
    }

    using FahrenheitRatio = std::ratio<9243, 1245>; // Magic number not used in computation
    template <>
    constexpr double Quantity<Temperature, FahrenheitRatio>::fromSI(double si_value) const {
		return si_value * (9. / 5.) - 459.67;
    }

    using EVRatio = std::ratio<2339243, 124345>; // Magic number not used in computation
    template <>
    constexpr double Quantity<Energy, EVRatio>::getRatio() const { return 1.602176634e-19; }
}