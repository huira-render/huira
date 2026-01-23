#pragma once

#include <ostream>
#include <ratio>
#include <string>
#include <type_traits>

#include "huira/detail/constants.hpp"
#include "huira/detail/units/dimensionality.hpp"

#include "huira/detail/concepts/unit_concepts.hpp"
#include "huira/detail/validate.hpp"

namespace huira::units {
    template<IsDimensionality Dim, IsRatioOrTag Scale>
    class Quantity {
    public:
        using dimension_type = Dim;
        using scale_type = Scale;

        template<IsNumeric T>
        explicit constexpr Quantity(T value)
            requires std::is_arithmetic_v<T>
        : value_(static_cast<double>(value))
        {
            detail::validate_real(value_, "Unit[" + Dim::to_si_string() + "]");
        }

        template<IsRatioOrTag OtherScale>
        constexpr Quantity(const Quantity<Dim, OtherScale>& other)
        {
            value_ = this->from_si(other.get_si_value());
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

        constexpr operator double() const
            requires std::is_same_v<Dim, Dimensionless>
        {
            return get_si_value();
        }

        double get_si_value() const
        {
            return this->to_si(value_);
        }

        double value() const {
            return value_;
        }

        double raw_value() const {
            return value_;
        }

        template<IsRatioOrTag NewScale>
        Quantity<Dim, NewScale> as() const {
            return Quantity<Dim, NewScale>(*this);
        }

        template<typename QuantityType>
            requires requires { typename QuantityType::dimension_type; typename QuantityType::scale_type; }
        auto as() const {
            using NewScale = typename QuantityType::scale_type;
            return Quantity<Dim, NewScale>(*this);
        }

        std::string to_string() const
        {
            std::string output = std::to_string(get_si_value());
            output += " " + Dim::to_si_string();
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
            os << quantity.to_string();
            return os;
        }

    private:
        double value_;

        constexpr double get_ratio() const
        {
            return static_cast<double>(Scale::num) / static_cast<double>(Scale::den);
        }

        constexpr double from_si(double value_si) const
        {
            return value_si / this->get_ratio();
        }

        constexpr double to_si(double value) const
        {
            return value * this->get_ratio();
        }
    };

    template<typename R1, typename R2>
    struct ratio_multiply_impl {
        using type = std::ratio_multiply<R1, R2>;
    };

    template<typename R1, typename R2>
    using ratio_multiply = typename ratio_multiply_impl<R1, R2>::type;

    template<typename R1, typename R2>
    struct ratio_divide_impl {
        using type = std::ratio_divide<R1, R2>;
    };

    template<typename R1, typename R2>
    using ratio_divide = typename ratio_divide_impl<R1, R2>::type;

    template<IsUnitTag Tag>
    struct ratio_divide_impl<Tag, Tag> {
        using type = std::ratio<1, 1>;
    };

    template<IsUnitTag Tag, IsRatio Ratio>
    struct ratio_multiply_impl<Tag, Ratio> {
        using type = std::ratio<1, 1>;
    };

    template<IsRatio Ratio, IsUnitTag Tag>
    struct ratio_multiply_impl<Ratio, Tag> {
        using type = std::ratio<1, 1>;
    };

    template<IsUnitTag Tag, IsRatio Ratio>
    struct ratio_divide_impl<Tag, Ratio> {
        using type = std::ratio<1, 1>;
    };

    template<IsRatio Ratio, IsUnitTag Tag>
    struct ratio_divide_impl<Ratio, Tag> {
        using type = std::ratio<1, 1>;
    };

    template<IsUnitTag Tag1, IsUnitTag Tag2>
    struct ratio_multiply_impl<Tag1, Tag2> {
        using type = std::ratio<1, 1>;
    };

    template<IsUnitTag Tag1, IsUnitTag Tag2>
    struct ratio_divide_impl<Tag1, Tag2> {
        using type = std::ratio<1, 1>;
    };

    template<typename S1, typename S2>
    constexpr bool involves_tag_v = is_unit_tag<S1>::value || is_unit_tag<S2>::value;

    template<IsDimensionality Dim1, IsRatioOrTag Scale1, IsDimensionality Dim2, IsRatioOrTag Scale2>
    constexpr auto operator*(const Quantity<Dim1, Scale1>& lhs, const Quantity<Dim2, Scale2>& rhs) {
        using ResultDim = decltype(Dim1{}* Dim2{});
        if constexpr (involves_tag_v<Scale1, Scale2>) {
            return Quantity<ResultDim, std::ratio<1, 1>>(lhs.get_si_value() * rhs.get_si_value());
        }
        else {
            using ResultScale = ratio_multiply<Scale1, Scale2>;
            return Quantity<ResultDim, ResultScale>(lhs.raw_value() * rhs.raw_value());
        }
    }

    template<IsDimensionality Dim1, IsRatioOrTag Scale1, IsDimensionality Dim2, IsRatioOrTag Scale2>
    constexpr auto operator/(const Quantity<Dim1, Scale1>& lhs, const Quantity<Dim2, Scale2>& rhs) {
        using ResultDim = decltype(Dim1{} / Dim2{});
        if constexpr (involves_tag_v<Scale1, Scale2>) {
            return Quantity<ResultDim, std::ratio<1, 1>>(lhs.get_si_value() / rhs.get_si_value());
        }
        else {
            using ResultScale = ratio_divide<Scale1, Scale2>;
            return Quantity<ResultDim, ResultScale>(lhs.raw_value() / rhs.raw_value());
        }
    }

    template<IsDimensionality Dim, IsRatioOrTag Scale>
    constexpr Quantity<Dim, Scale> operator*(double scalar, const Quantity<Dim, Scale>& quantity) {
        return quantity * scalar;
    }

    template<IsDimensionality Dim, IsRatioOrTag Scale>
    constexpr auto operator/(double scalar, const Quantity<Dim, Scale>& quantity) {
        return Quantity<Dimensionless, std::ratio<1, 1>>(scalar) / quantity;
    }

    template<IsDimensionality Dim, IsRatioOrTag Scale1, IsRatioOrTag Scale2>
    constexpr auto operator+(const Quantity<Dim, Scale1>& lhs, const Quantity<Dim, Scale2>& rhs)
        -> Quantity<Dim, Scale1> {
        Quantity<Dim, Scale1> rhs_converted(rhs);
        return Quantity<Dim, Scale1>(lhs.raw_value() + rhs_converted.raw_value());
    }

    template<IsDimensionality Dim, IsRatioOrTag Scale1, IsRatioOrTag Scale2>
    constexpr auto operator-(const Quantity<Dim, Scale1>& lhs, const Quantity<Dim, Scale2>& rhs)
        -> Quantity<Dim, Scale1> {
        Quantity<Dim, Scale1> rhs_converted(rhs);
        return Quantity<Dim, Scale1>(lhs.raw_value() - rhs_converted.raw_value());
    }

    template<IsDimensionality Dim, IsRatioOrTag Scale1, IsRatioOrTag Scale2>
    constexpr bool operator==(const Quantity<Dim, Scale1>& lhs, const Quantity<Dim, Scale2>& rhs) {
        return lhs.get_si_value() == rhs.get_si_value();
    }

    template<IsDimensionality Dim, IsRatioOrTag Scale1, IsRatioOrTag Scale2>
    constexpr bool operator!=(const Quantity<Dim, Scale1>& lhs, const Quantity<Dim, Scale2>& rhs) {
        return lhs.get_si_value() != rhs.get_si_value();
    }

    template<IsDimensionality Dim, IsRatioOrTag Scale1, IsRatioOrTag Scale2>
    constexpr bool operator<(const Quantity<Dim, Scale1>& lhs, const Quantity<Dim, Scale2>& rhs) {
        return lhs.get_si_value() < rhs.get_si_value();
    }

    template<IsDimensionality Dim, IsRatioOrTag Scale1, IsRatioOrTag Scale2>
    constexpr bool operator>(const Quantity<Dim, Scale1>& lhs, const Quantity<Dim, Scale2>& rhs) {
        return lhs.get_si_value() > rhs.get_si_value();
    }

    template<IsDimensionality Dim, IsRatioOrTag Scale1, IsRatioOrTag Scale2>
    constexpr bool operator<=(const Quantity<Dim, Scale1>& lhs, const Quantity<Dim, Scale2>& rhs) {
        return lhs.get_si_value() <= rhs.get_si_value();
    }

    template<IsDimensionality Dim, IsRatioOrTag Scale1, IsRatioOrTag Scale2>
    constexpr bool operator>=(const Quantity<Dim, Scale1>& lhs, const Quantity<Dim, Scale2>& rhs) {
        return lhs.get_si_value() >= rhs.get_si_value();
    }

    struct SiderealDayTag {};
    template<> struct is_unit_tag<SiderealDayTag> : std::true_type {};
    template<>
    constexpr double Quantity<Time, SiderealDayTag>::get_ratio() const { return 86164.0905; }

    struct DegreeTag {};
    template<> struct is_unit_tag<DegreeTag> : std::true_type {};
    template<>
    constexpr double Quantity<Angle, DegreeTag>::get_ratio() const { return PI<double>() / 180.0; }

    struct ArcMinuteTag {};
    template<> struct is_unit_tag<ArcMinuteTag> : std::true_type {};
    template<>
    constexpr double Quantity<Angle, ArcMinuteTag>::get_ratio() const { return PI<double>() / 10800.0; }

    struct ArcSecondTag {};
    template<> struct is_unit_tag<ArcSecondTag> : std::true_type {};
    template<>
    constexpr double Quantity<Angle, ArcSecondTag>::get_ratio() const { return PI<double>() / 648000.0; }

    struct SquareDegreeTag {};
    template<> struct is_unit_tag<SquareDegreeTag> : std::true_type {};
    template<>
    constexpr double Quantity<SolidAngle, SquareDegreeTag>::get_ratio() const {
        return (PI<double>() / 180.0) * (PI<double>() / 180.0);
    }

    struct CelsiusTag {};
    template<> struct is_unit_tag<CelsiusTag> : std::true_type {};
    template<>
    constexpr double Quantity<Temperature, CelsiusTag>::to_si(double value) const {
        return value + 273.15;
    }

    template<>
    constexpr double Quantity<Temperature, CelsiusTag>::from_si(double value_si) const {
        return value_si - 273.15;
    }

    struct FahrenheitTag {};
    template<> struct is_unit_tag<FahrenheitTag> : std::true_type {};
    template <>
    constexpr double Quantity<Temperature, FahrenheitTag>::to_si(double value) const {
        return (value + 459.67) * (5. / 9.);
    }

    template <>
    constexpr double Quantity<Temperature, FahrenheitTag>::from_si(double value_si) const {
        return value_si * (9. / 5.) - 459.67;
    }

    struct ElectronVoltTag {};
    template<> struct is_unit_tag<ElectronVoltTag> : std::true_type {};
    template <>
    constexpr double Quantity<Energy, ElectronVoltTag>::get_ratio() const { return 1.602176634e-19; }

    template<IsRatioOrTag Scale>
    class Quantity<Dimensionless, Scale> {
    public:
        using dimension_type = Dimensionless;
        using scale_type = Scale;

        template<IsNumeric T>
        explicit constexpr Quantity(T value)
            requires std::is_arithmetic_v<T>
        : value_(static_cast<double>(value))
        {
            detail::validate_real(value_, "Dimensionless");
        }

        template<IsRatioOrTag OtherScale>
        constexpr Quantity(const Quantity<Dimensionless, OtherScale>& other)
        {
            value_ = this->from_si(other.get_si_value());
        }

        constexpr Quantity() : value_(0.0) {}
        constexpr Quantity(const Quantity& other) : value_(other.value_) {}

        constexpr Quantity& operator=(const Quantity& other) {
            value_ = other.value_;
            return *this;
        }

        constexpr operator double() const {
            return get_si_value();
        }

        double get_si_value() const {
            return this->to_si(value_);
        }

        double value() const {
            return value_;
        }

        double raw_value() const {
            return value_;
        }

        template<IsRatioOrTag NewScale>
        Quantity<Dimensionless, NewScale> as() const {
            return Quantity<Dimensionless, NewScale>(*this);
        }

        template<typename QuantityType>
            requires requires { typename QuantityType::dimension_type; typename QuantityType::scale_type; }
        auto as() const {
            using NewScale = typename QuantityType::scale_type;
            return Quantity<Dimensionless, NewScale>(*this);
        }

        std::string to_string() const {
            return std::to_string(get_si_value());
        }

        constexpr Quantity operator+(const Quantity& other) const {
            return Quantity(value_ + other.value_);
        }

        constexpr Quantity operator-(const Quantity& other) const {
            return Quantity(value_ - other.value_);
        }

        constexpr Quantity& operator+=(const Quantity& other) {
            value_ += other.value_;
            return *this;
        }

        constexpr Quantity& operator-=(const Quantity& other) {
            value_ -= other.value_;
            return *this;
        }

        constexpr Quantity operator*(double scalar) const {
            return Quantity(value_ * scalar);
        }

        constexpr Quantity operator/(double scalar) const {
            return Quantity(value_ / scalar);
        }

        constexpr Quantity& operator*=(double scalar) {
            value_ *= scalar;
            return *this;
        }

        constexpr Quantity& operator/=(double scalar) {
            value_ /= scalar;
            return *this;
        }

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
            os << quantity.to_string();
            return os;
        }

    private:
        double value_;

        constexpr double get_ratio() const {
            return static_cast<double>(Scale::num) / static_cast<double>(Scale::den);
        }

        constexpr double from_si(double value_si) const {
            return value_si / this->get_ratio();
        }

        constexpr double to_si(double value) const {
            return value * this->get_ratio();
        }
    };
}
