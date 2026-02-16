#pragma once

#include <string>

#include "pybind11/pybind11.h"
#include "pybind11/stl.h"
#include "pybind11/operators.h"

#include "huira/core/concepts/numeric_concepts.hpp"
#include "huira/core/units/units.hpp"
#include "huira/core/units/quantity.hpp"
#include "huira/core/units/spectral_quantity.hpp"

namespace py = pybind11;

namespace huira {

    namespace detail {
        template <typename UnitType>
        UnitType unit_from_py(const py::object& obj) {
            double val = obj.attr("to_si")().cast<double>();
            using Dim = typename UnitType::dimension_type;
            using SIUnit = units::Quantity<Dim, std::ratio<1, 1>>;
            return UnitType(SIUnit(val));
        }
    }

    // ========================================= //
    // === Quantity (scalar) Python Bindings === //
    // ========================================= //

    template<typename SIUnit, typename... OtherUnits>
    void register_unit_conversions(py::module& m) {
        (void)m;
        (py::implicitly_convertible<OtherUnits, SIUnit>(), ...);
    }

    /**
     * @brief Bind a scalar Quantity type to Python
     *
     * This handles standard physical quantities like Meter, Second, Watt, etc.
     * These have a to_si() method that returns a double.
     */
    template<typename UnitType>
    void bind_quantity_type(py::module& m, const std::string& name) {
        using Dim = typename UnitType::dimension_type;

        std::string class_doc = name + " unit.\n\n"
            "Supports arithmetic with other units of the same dimension.\n"
            "Implicit conversion to SI base unit when passed to functions.\n\n"
            "Example:\n"
            "    >>> x = huira." + name + "(1.0)\n"
            "    >>> x.to_si()\n";

        py::class_<UnitType>(m, name.c_str(), class_doc.c_str())
            .def(py::init<>(), "Default constructor (value = 0).")
            .def(py::init<double>(), py::arg("value"),
                ("Create a " + name + " from a float.").c_str())
            .def(py::init<int>(), py::arg("value"),
                ("Create a " + name + " from an int.").c_str())
            .def(py::init<float>(), py::arg("value"),
                ("Create a " + name + " from a float.").c_str())
            .def("to_si", static_cast<double (UnitType::*)() const>(&UnitType::to_si),
                "Return the value converted to SI base units (returns float).")
            .def("raw_value", &UnitType::raw_value,
                "Return the raw value in this unit's scale.")
            .def("__str__", &UnitType::to_string)
            .def("__repr__", [name](const UnitType& self) {
            return name + "(" + std::to_string(self.raw_value()) + ")";
                })
            // Cross-unit addition (different scales, same dimension)
            .def("__add__", [](const UnitType& self, const py::object& other) -> UnitType {
            double other_si = other.attr("to_si")().cast<double>();
            double result_si = self.to_si() + other_si;
            using SIUnit = huira::units::Quantity<Dim, std::ratio<1, 1>>;
            SIUnit si_result(result_si);
            return UnitType(si_result);
                }, py::arg("other"),
                    "Add another unit of the same dimension. Result is in this unit's scale.")
            .def("__sub__", [](const UnitType& self, const py::object& other) -> UnitType {
            double other_si = other.attr("to_si")().cast<double>();
            double result_si = self.to_si() - other_si;
            using SIUnit = huira::units::Quantity<Dim, std::ratio<1, 1>>;
            SIUnit si_result(result_si);
            return UnitType(si_result);
                }, py::arg("other"),
                    "Subtract another unit of the same dimension. Result is in this unit's scale.")
            // Same-type operators
            .def(py::self += py::self)
            .def(py::self -= py::self)
            .def(py::self * double(), "Multiply by a scalar.")
            .def(py::self / double(), "Divide by a scalar.")
            .def(py::self *= double())
            .def(py::self /= double())
            .def(double() * py::self, "Multiply a scalar by this unit.")
            // Cross-unit comparison via SI values
            .def("__eq__", [](const UnitType& self, const py::object& other) {
            return self.to_si() == other.attr("to_si")().cast<double>();
                }, py::arg("other"), "Equal comparison via SI values.")
            .def("__lt__", [](const UnitType& self, const py::object& other) {
            return self.to_si() < other.attr("to_si")().cast<double>();
                }, py::arg("other"), "Less-than comparison via SI values.")
            .def("__le__", [](const UnitType& self, const py::object& other) {
            return self.to_si() <= other.attr("to_si")().cast<double>();
                }, py::arg("other"), "Less-or-equal comparison via SI values.")
            .def("__gt__", [](const UnitType& self, const py::object& other) {
            return self.to_si() > other.attr("to_si")().cast<double>();
                }, py::arg("other"), "Greater-than comparison via SI values.")
            .def("__ge__", [](const UnitType& self, const py::object& other) {
            return self.to_si() >= other.attr("to_si")().cast<double>();
                }, py::arg("other"), "Greater-or-equal comparison via SI values.");
    }

    // ================================================= //
    // === SpectralQuantity (vector) Python Bindings === //
    // ================================================= //

    /**
     * @brief Bind a SpectralQuantity type to Python
     *
     * This handles spectral physical quantities like SpectralWatts<RGB>.
     * These have a to_si() method that returns a spectral type (not a double).
     */
    template<typename SpectralUnitType>
    void bind_spectral_quantity_type(py::module& m, const std::string& name) {
        //using Dim = typename SpectralUnitType::dimension_type;
        using TSpectral = typename SpectralUnitType::spectral_type;
        //constexpr std::size_t N = TSpectral::size();

        std::string class_doc = name + " spectral unit.\n\n"
            "Wraps a spectral type with physical dimensionality.\n"
            "Supports conversion between different scales.\n\n"
            "Example:\n"
            "    >>> spectral_data = huira.SpectralBins([1.0, 2.0, 3.0, ...])\n"
            "    >>> power = huira." + name + "(spectral_data)\n"
            "    >>> si_values = power.to_si()  # Returns SpectralBins\n";

        py::class_<SpectralUnitType>(m, name.c_str(), class_doc.c_str())
            .def(py::init<>(), "Default constructor (all spectral values = 0).")
            .def(py::init<const TSpectral&>(), py::arg("spectral_value"),
                ("Create from a spectral type. Values are in " + name + " units.").c_str())
            .def("to_si", static_cast<TSpectral (SpectralUnitType::*)() const>(&SpectralUnitType::to_si),
                "Convert the spectral data to SI base units (returns SpectralBins).")
            .def("value", &SpectralUnitType::value,
                "Get the underlying spectral data in the current unit's scale.",
                py::return_value_policy::reference_internal)
            .def("__str__", &SpectralUnitType::to_string)
            .def("__repr__", [name](const SpectralUnitType& self) {
            return name + "(" + self.to_string() + ")";
                })
            // Comparison operators (same type)
            .def(py::self == py::self)
            .def(py::self != py::self);
    }


    static void bind_units(py::module& m) {
        // ============================================== //
        // === Bind Scalar Quantity Types (Quantity)  === //
        // ============================================== //

        // Distance Units
        bind_quantity_type<huira::units::Kilometer>(m, "Kilometer");
        bind_quantity_type<huira::units::Meter>(m, "Meter");
        bind_quantity_type<huira::units::Centimeter>(m, "Centimeter");
        bind_quantity_type<huira::units::Millimeter>(m, "Millimeter");
        bind_quantity_type<huira::units::Micrometer>(m, "Micrometer");
        bind_quantity_type<huira::units::Nanometer>(m, "Nanometer");

        bind_quantity_type<huira::units::AstronomicalUnit>(m, "AstronomicalUnit");
        m.attr("AU") = m.attr("AstronomicalUnit");

        bind_quantity_type<huira::units::Foot>(m, "Foot");
        bind_quantity_type<huira::units::Yard>(m, "Yard");
        bind_quantity_type<huira::units::Mile>(m, "Mile");

        // Mass Units
        bind_quantity_type<huira::units::Kilogram>(m, "Kilogram");
        bind_quantity_type<huira::units::Gram>(m, "Gram");
        bind_quantity_type<huira::units::Milligram>(m, "Milligram");

        // Time Units
        bind_quantity_type<huira::units::SiderealDay>(m, "SiderealDay");
        bind_quantity_type<huira::units::Day>(m, "Day");
        bind_quantity_type<huira::units::Hour>(m, "Hour");
        bind_quantity_type<huira::units::Minute>(m, "Minute");
        bind_quantity_type<huira::units::Second>(m, "Second");
        bind_quantity_type<huira::units::Millisecond>(m, "Millisecond");
        bind_quantity_type<huira::units::Microsecond>(m, "Microsecond");
        bind_quantity_type<huira::units::Nanosecond>(m, "Nanosecond");
        bind_quantity_type<huira::units::Femtosecond>(m, "Femtosecond");

        // Current Units
        bind_quantity_type<huira::units::Ampere>(m, "Ampere");

        // Temperature Units
        bind_quantity_type<huira::units::Kelvin>(m, "Kelvin");
        bind_quantity_type<huira::units::Celsius>(m, "Celsius");
        bind_quantity_type<huira::units::Fahrenheit>(m, "Fahrenheit");

        // Amount of Substance Units
        bind_quantity_type<huira::units::Mole>(m, "Mole");

        // Luminosity Units
        bind_quantity_type<huira::units::Candela>(m, "Candela");

        // Angular Units
        bind_quantity_type<huira::units::Radian>(m, "Radian");
        bind_quantity_type<huira::units::Degree>(m, "Degree");
        bind_quantity_type<huira::units::Arcminute>(m, "Arcminute");
        bind_quantity_type<huira::units::Arcsecond>(m, "Arcsecond");

        // Solid Angle Units
        bind_quantity_type<huira::units::Steradian>(m, "Steradian");
        bind_quantity_type<huira::units::SquareDegree>(m, "SquareDegree");

        // Speed Units
        bind_quantity_type<huira::units::MetersPerSecond>(m, "MetersPerSecond");
        bind_quantity_type<huira::units::KilometersPerSecond>(m, "KilometersPerSecond");
        bind_quantity_type<huira::units::MilesPerHour>(m, "MilesPerHour");
        bind_quantity_type<huira::units::KilometersPerHour>(m, "KilometersPerHour");

        // Angular Rate Units
        bind_quantity_type<huira::units::RadiansPerSecond>(m, "RadiansPerSecond");
        bind_quantity_type<huira::units::DegreesPerSecond>(m, "DegreesPerSecond");

        // Frequency Units
        bind_quantity_type<huira::units::Hertz>(m, "Hertz");
        bind_quantity_type<huira::units::Kilohertz>(m, "Kilohertz");
        bind_quantity_type<huira::units::Megahertz>(m, "Megahertz");
        bind_quantity_type<huira::units::Gigahertz>(m, "Gigahertz");
        bind_quantity_type<huira::units::Terahertz>(m, "Terahertz");

        // Force Units
        bind_quantity_type<huira::units::Newton>(m, "Newton");
        bind_quantity_type<huira::units::Kilonewton>(m, "Kilonewton");

        // Pressure Units
        bind_quantity_type<huira::units::Pascal>(m, "Pascal");
        bind_quantity_type<huira::units::Kilopascal>(m, "Kilopascal");

        // Energy Units
        bind_quantity_type<huira::units::Joule>(m, "Joule");
        bind_quantity_type<huira::units::Kilojoule>(m, "Kilojoule");
        bind_quantity_type<huira::units::Megajoule>(m, "Megajoule");
        bind_quantity_type<huira::units::ElectronVolt>(m, "ElectronVolt");

        // Area Units
        bind_quantity_type<huira::units::SquareMeter>(m, "SquareMeter");
        bind_quantity_type<huira::units::SquareCentimeter>(m, "SquareCentimeter");
        bind_quantity_type<huira::units::SquareMillimeter>(m, "SquareMillimeter");

        // Power Units (scalar)
        bind_quantity_type<huira::units::Milliwatt>(m, "Milliwatt");
        bind_quantity_type<huira::units::Watt>(m, "Watt");
        bind_quantity_type<huira::units::Kilowatt>(m, "Kilowatt");
        bind_quantity_type<huira::units::Megawatt>(m, "Megawatt");
        bind_quantity_type<huira::units::Gigawatt>(m, "Gigawatt");

        // Electric Charge Units
        bind_quantity_type<huira::units::Coulomb>(m, "Coulomb");

        // Radiometric Units (scalar)
        bind_quantity_type<huira::units::WattsPerMeterSquaredSteradian>(m, "WattsPerMeterSquaredSteradian");
        bind_quantity_type<huira::units::WattsPerMeterSquared>(m, "WattsPerMeterSquared");
        bind_quantity_type<huira::units::WattsPerSteradian>(m, "WattsPerSteradian");

        // Photometric Units
        bind_quantity_type<huira::units::Lumen>(m, "Lumen");

        // ===================================== //
        // === Register Implicit Conversions === //
        // ===================================== //
        // Allows users to pass e.g. Degree where Radian is expected in function signatures.
        // Each call registers conversions from all listed types -> the SI base unit (first arg).

        // Distance -> Meter
        register_unit_conversions<huira::units::Meter,
            huira::units::Kilometer, huira::units::Centimeter, huira::units::Millimeter,
            huira::units::Micrometer, huira::units::Nanometer, huira::units::AstronomicalUnit,
            huira::units::Foot, huira::units::Yard, huira::units::Mile>(m);

        // Mass -> Kilogram
        register_unit_conversions<huira::units::Kilogram,
            huira::units::Gram, huira::units::Milligram>(m);

        // Time -> Second
        register_unit_conversions<huira::units::Second,
            huira::units::Femtosecond, huira::units::Nanosecond, huira::units::Microsecond,
            huira::units::Millisecond, huira::units::Minute, huira::units::Hour,
            huira::units::Day, huira::units::SiderealDay>(m);

        // Temperature -> Kelvin
        register_unit_conversions<huira::units::Kelvin,
            huira::units::Celsius, huira::units::Fahrenheit>(m);

        // Angle -> Radian
        register_unit_conversions<huira::units::Radian,
            huira::units::Degree, huira::units::Arcminute, huira::units::Arcsecond>(m);

        // Solid Angle -> Steradian
        register_unit_conversions<huira::units::Steradian,
            huira::units::SquareDegree>(m);

        // Speed -> MetersPerSecond
        register_unit_conversions<huira::units::MetersPerSecond,
            huira::units::KilometersPerSecond, huira::units::MilesPerHour,
            huira::units::KilometersPerHour>(m);

        // Frequency -> Hertz
        register_unit_conversions<huira::units::Hertz,
            huira::units::Kilohertz, huira::units::Megahertz,
            huira::units::Gigahertz, huira::units::Terahertz>(m);

        // Force -> Newton
        register_unit_conversions<huira::units::Newton,
            huira::units::Kilonewton>(m);

        // Pressure -> Pascal
        register_unit_conversions<huira::units::Pascal,
            huira::units::Kilopascal>(m);

        // Energy -> Joule
        register_unit_conversions<huira::units::Joule,
            huira::units::Kilojoule, huira::units::Megajoule,
            huira::units::ElectronVolt>(m);

        // Power -> Watt
        register_unit_conversions<huira::units::Watt,
            huira::units::Milliwatt, huira::units::Kilowatt, huira::units::Megawatt,
            huira::units::Gigawatt>(m);
    }

    // ====================================================== //
    // === Bind Spectral Quantity Types (Template Helper) === //
    // ====================================================== //

    /**
     * @brief Helper to bind all spectral quantity types for a given spectral type
     *
     * This should be called after the spectral type itself has been bound.
     * For example, after binding RGB or Visible8 as "SpectralBins".
     *
     * @tparam TSpectral The spectral type (e.g., RGB, Visible8)
     */
    template<typename TSpectral>
    void bind_spectral_units_for_type(py::module& m) {
        // Power Units (spectral)
        bind_spectral_quantity_type<huira::units::SpectralMilliwatts<TSpectral>>(
            m, "SpectralMillwatts");
        bind_spectral_quantity_type<huira::units::SpectralWatts<TSpectral>>(
            m, "SpectralWatts");
        bind_spectral_quantity_type<huira::units::SpectralKilowatts<TSpectral>>(
            m, "SpectralKilowatts");
        bind_spectral_quantity_type<huira::units::SpectralMegawatts<TSpectral>>(
            m, "SpectralMegawatts");
        bind_spectral_quantity_type<huira::units::SpectralGigawatts<TSpectral>>(
            m, "SpectralGigawatts");

        // Irradiance Units (spectral)
        bind_spectral_quantity_type<huira::units::SpectralWattsPerMeterSquared<TSpectral>>(
            m, "SpectralWattsPerMeterSquared");
    }
}
